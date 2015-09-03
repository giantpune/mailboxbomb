// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

// Based on code:
//	Copyright (c) 2008 - Nuke - <wiinuke@gmail.com>

#include "loader.h"


static void exi_write(u32 addr, u32 x)
{
	write32(0x0d006800 + addr, x);
}

static u32 exi_read(u32 addr)
{
	return read32(0x0d006800 + addr);
}

#define EXI_CH1_STATUS	0x14
#define EXI_CH1_CONTROL	0x20
#define EXI_CH1_DATA	0x24


static void usbgecko_deselect_device(void)
{
	exi_write(EXI_CH1_STATUS, 0);
}

static void usbgecko_select_device(void)
{
	// device 0, 16MHz
	exi_write(EXI_CH1_STATUS, 0xc0);
}

static void usbgecko_wait_for_transfer_complete(void)
{
	while (exi_read(EXI_CH1_CONTROL) & 1)
		;
}


u8 usbgecko_flash_read8(u32 offset)
{
	u8 x;

	usbgecko_deselect_device();

	usbgecko_select_device();
	exi_write(EXI_CH1_DATA, 0xf0000000 | (offset << 9));
	exi_write(EXI_CH1_CONTROL, 0x35); // 4 bytes immediate write
	usbgecko_wait_for_transfer_complete();

	usbgecko_select_device();
	exi_write(EXI_CH1_CONTROL, 0x39); // 4 bytes immediate read/write
	usbgecko_wait_for_transfer_complete();

	x = exi_read(EXI_CH1_DATA) >> 23;

	usbgecko_deselect_device();

	return x;
}

u32 usbgecko_flash_read32(u32 offset)
{
	u32 x, i;

	x = 0;
	for (i = 0; i < 4; i++)
		x = (x << 8) | usbgecko_flash_read8(offset++);

	return x;
}



static int usbgecko_console_enabled = 0;

static u32 usbgecko_command(u32 command)
{
	u32 x;

	usbgecko_select_device();
	exi_write(EXI_CH1_DATA, command);
	exi_write(EXI_CH1_CONTROL, 0x19); // 2 bytes immediate read/write
	usbgecko_wait_for_transfer_complete();

	x = exi_read(EXI_CH1_DATA);

	usbgecko_deselect_device();

	return x;
}

int usbgecko_checkgecko(void)
{
	return usbgecko_command(0x90000000) == 0x04700000;
}

void usbgecko_console_putc(u8 c)
{
	u32 x;

	if (!usbgecko_console_enabled)
		return;

	if (c == '\n')
		usbgecko_console_putc('\r');

	x = usbgecko_command(0xb0000000 | (c << 20));
}

static void usbgecko_flush(void)
{
	u32 x;

	do {
		x = usbgecko_command(0xa0000000);
	} while (x & 0x08000000);
}

void usbgecko_init(void)
{
	if (!usbgecko_checkgecko())
		return;

	usbgecko_console_enabled = 1;
	usbgecko_flush();
}
