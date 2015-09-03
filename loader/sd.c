#if 0// Copyright 2008  Haxx Enterprises  <bushing@gmail.com>
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt


#include "loader.h"


static int fd;
static u32 rca; // 16 bottom bits are stuff bits


static int sd_hc_write8(u8 reg, u8 data)
{
	u32 param[6];
	int err;

	memset(param, 0, sizeof param);
	param[0] = reg;
	param[3] = 1;	// reg size
	param[4] = data;

	err = ios_ioctl(fd, 1, param, sizeof param, 0, 0);

	return err;
}

static int sd_hc_read8(u8 reg, u8 *x)
{
	u32 param[6];
	u32 data;
	int err;

	memset(param, 0, sizeof param);
	param[0] = reg;
	param[3] = 1;	// reg size
	param[4] = 0;

	err = ios_ioctl(fd, 2, param, sizeof param, &data, sizeof data);
	if (err)
		return err;

	*x = data;

	return err;
}

static int sd_reset_card(void)
{
	u32 reply;
	int err;

	err = ios_ioctl(fd, 4, 0, 0, &reply, sizeof reply);
	if (err)
		return err;

	rca = reply & 0xffff0000;

//	printf("sd_reset_card(): got reply = %08x\n", reply);

	return 0;
}

static int sd_set_clock(void)
{
	u32 clock;
	int err;

	clock = 1;	// half of the sdclk divisor: a power of two or zero,
			// should look at capabilities reg to compute this

	err = ios_ioctl(fd, 6, &clock, sizeof clock, 0, 0);

	return err;
}

static int sd_command(u32 cmd, u32 cmd_type, u32 resp_type, u32 arg,
                      u32 block_count, u32 block_size, void *addr,
                      u32 *outreply, u32 reply_size)
{
	u32 param[9];
	u32 reply[4];
	int err;

	param[0] = cmd;
	param[1] = cmd_type;
	param[2] = resp_type;
	param[3] = arg;
	param[4] = block_count;
	param[5] = block_size;
	param[6] = (u32)addr;
	param[7] = 0; // ???
	param[8] = 0; // ???

	err = ios_ioctl(fd, 7, param, sizeof param, reply, sizeof reply);

	if (reply_size) // ???
		memcpy(outreply, reply, reply_size);

	return err;
}


#define TYPE_BC 1
#define TYPE_BCR 2
#define TYPE_AC 3
#define TYPE_ADTC 4

#define RESPONSE_NONE 0
#define RESPONSE_R1 1
#define RESPONSE_R1B 2
#define RESPONSE_R2 3
#define RESPONSE_R3 4
#define RESPONSE_R4 5
#define RESPONSE_R5 6
#define RESPONSE_R6 7


static int sd_app_command(u32 cmd, u32 cmd_type, u32 resp_type, u32 arg,
                          u32 block_count, u32 block_size, void *addr,
                          u32 *outreply, u32 reply_size)
{
	int err;

	err = sd_command(55, TYPE_AC, RESPONSE_R1, rca, 0, 0, 0, 0, 0);
	if (err)
		return err;

	err = sd_command(cmd, cmd_type, resp_type, arg,
                         block_count, block_size, addr,
                         outreply, reply_size);

	return err;
}

static int sd_data_command(u32 cmd, u32 cmd_type, u32 resp_type, u32 arg,
                           u32 block_count, u32 block_size, void *data,
                           u32 unk1, u32 unk2, u32 *outreply, u32 reply_size)
{
	u32 param[9];
	u32 reply[4];
	struct ioctlv vec[3];
	int err;

	param[0] = cmd;
	param[1] = cmd_type;
	param[2] = resp_type;
	param[3] = arg;
	param[4] = block_count;
	param[5] = block_size;
	param[6] = (u32)data;
	param[7] = unk1; // ???
	param[8] = unk2; // ???

	vec[0].data = param;
	vec[0].len = sizeof param;
	vec[1].data = data;
	vec[1].len = block_count * block_size;
	vec[2].data = reply;
	vec[2].len = sizeof reply;

	err = ios_ioctlv(fd, 7, 2, 1, vec);

	if (reply_size) // ???
		memcpy(outreply, reply, reply_size);

	return err;
}

static int sd_select(void)
{
	int err;

	//printf("Selecting card:\n");
	err = sd_command(7, TYPE_AC, RESPONSE_R1B, rca, 0, 0, 0, 0, 0);

	return err;
}

static int sd_set_blocklength(u32 len)
{
	int err;

	//printf("sd_set_blocklength(%u)\n", len);
	err = sd_command(16, TYPE_AC, RESPONSE_R1, len, 0, 0, 0, 0, 0);

	return err;
}

static int sd_set_bus_width(int width)
{
	u32 arg;
	u8 reg;
	int err;

	// First notify the card.
	arg = (width == 4) ? 2 : 0;
	//printf("sd_set_bus_width()\n");
	err = sd_app_command(6, TYPE_AC, RESPONSE_R1, arg, 0, 0, 0, 0, 0);
	if (err)
		return err;

	// Now change the Host Control Register.
	err = sd_hc_read8(0x28, &reg);
	if (err)
		return err;

	reg = (reg & ~2) | arg;

	err = sd_hc_write8(0x28, reg);

	return err;
}

int sd_read_sector(u8 *data, u32 offset)
{
	u32 reply[4];
	int err;

	if (offset >= 0x800000)
		return -1;

	err = sd_data_command(18, TYPE_AC, RESPONSE_R1, 0x200 * offset,
	                      1, 0x200, data, 1, 0, reply, sizeof reply);

	sync_before_read(data, 0x200);

	//printf("READ block %d\r",offset);
	if (err)
		printf("SD READ %d: err=%08x, reply=%08x %08x %08x %08x\n", 
		       offset, err, reply[0], reply[1], reply[2], reply[3]);

	return err;
}

int sd_close(void)
{
	return ios_close(fd);
}

int sd_init(void)
{
	int err;

	fd = ios_open("/dev/sdio/slot0", 0);
	if (fd < 0)
		return fd;

	err = sd_reset_card();
	if (err) {
		printf("SD Card not present? (%d)\n", err);
		goto out;
	}

	// now in standby state

	err = sd_select();
	if (err)
		goto out;

	// now in transfer state

	// Some broken cards require this:
	err = sd_set_blocklength(0x200);
	if (err)
		goto out;

	err = sd_set_bus_width(4);	// XXX: Should check in SCR first.
	if (err)
		goto out;

	err = sd_set_clock();	// XXX: Should check.
	if (err)
		goto out;

	return 0;

 out:
	sd_close();

	return err;
}
#endif
// Copyright 2008  Haxx Enterprises  <bushing@gmail.com>
// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt


#include "loader.h"


static int fd;
static u32 rca; // 16 bottom bits are stuff bits
static u32 blk_mul;

static int sd_hc_write8(u8 reg, u8 data)
{
	u32 param[6];
	int err;

	memset(param, 0, sizeof param);
	param[0] = reg;
	param[3] = 1;	// reg size
	param[4] = data;

	err = ios_ioctl(fd, 1, param, sizeof param, 0, 0);

	return err;
}

static int sd_hc_read8(u8 reg, u8 *x)
{
	u32 param[6];
	u32 data;
	int err;

	memset(param, 0, sizeof param);
	param[0] = reg;
	param[3] = 1;	// reg size
	param[4] = 0;

	err = ios_ioctl(fd, 2, param, sizeof param, &data, sizeof data);
	if (err)
		return err;

	*x = data;

	return err;
}

static int sd_reset_card(void)
{
	u32 reply;
	int err;

	err = ios_ioctl(fd, 4, 0, 0, &reply, sizeof reply);
	if (err)
		return err;

	rca = reply & 0xffff0000;

//	printf("sd_reset_card(): got reply = %08x\n", reply);

	return 0;
}

static int sd_check_sdhc(void)
{
	u32 status;
	int err;

	err = ios_ioctl(fd, 11, 0, 0, &status, sizeof status);
	if (err)
		return err;

	return !!(status & 0x100000);
}

static int sd_set_clock(void)
{
	u32 clock;
	int err;

	clock = 1;	// half of the sdclk divisor: a power of two or zero,
			// should look at capabilities reg to compute this

	err = ios_ioctl(fd, 6, &clock, sizeof clock, 0, 0);

	return err;
}

static int sd_command(u32 cmd, u32 cmd_type, u32 resp_type, u32 arg,
                      u32 block_count, u32 block_size, void *addr,
                      u32 *outreply, u32 reply_size)
{
	u32 param[9];
	u32 reply[4];
	int err;

	param[0] = cmd;
	param[1] = cmd_type;
	param[2] = resp_type;
	param[3] = arg;
	param[4] = block_count;
	param[5] = block_size;
	param[6] = (u32)addr;
	param[7] = 0; // ???
	param[8] = 0; // ???

	err = ios_ioctl(fd, 7, param, sizeof param, reply, sizeof reply);

	if (reply_size) // ???
		memcpy(outreply, reply, reply_size);

	return err;
}


#define TYPE_BC 1
#define TYPE_BCR 2
#define TYPE_AC 3
#define TYPE_ADTC 4

#define RESPONSE_NONE 0
#define RESPONSE_R1 1
#define RESPONSE_R1B 2
#define RESPONSE_R2 3
#define RESPONSE_R3 4
#define RESPONSE_R4 5
#define RESPONSE_R5 6
#define RESPONSE_R6 7


static int sd_app_command(u32 cmd, u32 cmd_type, u32 resp_type, u32 arg,
                          u32 block_count, u32 block_size, void *addr,
                          u32 *outreply, u32 reply_size)
{
	int err;

	err = sd_command(55, TYPE_AC, RESPONSE_R1, rca, 0, 0, 0, 0, 0);
	if (err)
		return err;

	err = sd_command(cmd, cmd_type, resp_type, arg,
                         block_count, block_size, addr,
                         outreply, reply_size);

	return err;
}

static int sd_data_command(u32 cmd, u32 cmd_type, u32 resp_type, u32 arg,
                           u32 block_count, u32 block_size, void *data,
                           u32 unk1, u32 unk2, u32 *outreply, u32 reply_size)
{
	u32 param[9];
	u32 reply[4];
	struct ioctlv vec[3];
	int err;

	param[0] = cmd;
	param[1] = cmd_type;
	param[2] = resp_type;
	param[3] = arg;
	param[4] = block_count;
	param[5] = block_size;
	param[6] = (u32)data;
	param[7] = unk1; // ???
	param[8] = unk2; // ???

	vec[0].data = param;
	vec[0].len = sizeof param;
	vec[1].data = data;
	vec[1].len = block_count * block_size;
	vec[2].data = reply;
	vec[2].len = sizeof reply;

	err = ios_ioctlv(fd, 7, 2, 1, vec);

	if (reply_size) // ???
		memcpy(outreply, reply, reply_size);

	return err;
}

static int sd_select(void)
{
	int err;

	//printf("Selecting card:\n");
	err = sd_command(7, TYPE_AC, RESPONSE_R1B, rca, 0, 0, 0, 0, 0);

	return err;
}

static int sd_set_blocklength(u32 len)
{
	int err;

	//printf("sd_set_blocklength(%u)\n", len);
	err = sd_command(16, TYPE_AC, RESPONSE_R1, len, 0, 0, 0, 0, 0);

	return err;
}

static int sd_set_bus_width(int width)
{
	u32 arg;
	u8 reg;
	int err;

	// First notify the card.
	arg = (width == 4) ? 2 : 0;
	//printf("sd_set_bus_width()\n");
	err = sd_app_command(6, TYPE_AC, RESPONSE_R1, arg, 0, 0, 0, 0, 0);
	if (err)
		return err;

	// Now change the Host Control Register.
	err = sd_hc_read8(0x28, &reg);
	if (err)
		return err;

	reg = (reg & ~2) | arg;

	err = sd_hc_write8(0x28, reg);

	return err;
}

int sd_read_sector(u8 *data, u32 offset)
{
	u32 reply[4];
	int err;

	//if (offset >= 0x800000)
	//	return -1;

	err = sd_data_command(18, TYPE_AC, RESPONSE_R1, blk_mul * offset,
	                      1, 0x200, data, 1, 0, reply, sizeof reply);

	sync_before_read(data, 0x200);

	//printf("READ block %d\r",offset);
	if (err)
		printf("SD READ %d: err=%08x, reply=%08x %08x %08x %08x\n", 
		       offset, err, reply[0], reply[1], reply[2], reply[3]);

	return err;
}

int sd_close(void)
{
	return ios_close(fd);
}

int sd_init(void)
{
	int err;

	fd = ios_open("/dev/sdio/slot0", 0);
	if (fd < 0)
		return fd;

	err = sd_reset_card();
	if (err) {
		printf("SD Card not present? (%d)\n", err);
		goto out;
	}

	err = sd_check_sdhc();
	if (err < 0) {
		printf("SD Card bad status (%d)\n", err);
		goto out;
	}
	blk_mul = err ? 1 : 512;

	// now in standby state

	err = sd_select();
	if (err)
		goto out;

	// now in transfer state

	// Some broken cards require this:
	err = sd_set_blocklength(0x200);
	if (err)
		goto out;

	err = sd_set_bus_width(4);	// XXX: Should check in SCR first.
	if (err)
		goto out;

	err = sd_set_clock();	// XXX: Should check.
	if (err)
		goto out;

	return 0;

 out:
	sd_close();

	return err;
}


