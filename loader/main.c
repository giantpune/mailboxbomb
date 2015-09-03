// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "loader.h"


static u8 *const code_buffer = (u8 *)0x90100000;
static u8 *const trampoline_buffer = (u8 *)0x80001800;

static void dsp_reset(void)
{
	write16(0x0c00500a, read16(0x0c00500a) & ~0x01f8);
	write16(0x0c00500a, read16(0x0c00500a) | 0x0010);
	write16(0x0c005036, 0);
}

/*static u32 reboot_trampoline[] = {
	0x3c209000, // lis 1,0x9000
	0x60210020, // ori 1,1,0x0020
	0x7c2903a6, // mtctr 1
	0x4e800420  // bctr
};*/

int try_sd_load(void)
{
	int err;

	err = sd_init();
	if (err) {
		printf("SD card not found (%d)\n", err);
		return err;
	}

	err = fat_init();
	if (err == 0)
		printf("SD card detected\n");
	else {
		printf("SD card not detected (%d)\n", err);
		return err;
	}

//	if (usbgecko_checkgecko())
//		printf("USBGecko serial interface detected\n");
//	else
//		printf("USBGecko serial interface not detected\n");

	printf("Opening boot.elf:\n");
	err = fat_open("boot.elf");

	if (err) {
		printf("boot.elf not found (%d)\n", err);
		return err;
	}

extern u32 fat_file_size;

	printf("reading %d bytes...\n", fat_file_size);
	err = fat_read(code_buffer, fat_file_size);
	if (err) {
		printf("Error %d reading file\n", err);
		return err;
	}

	printf("Done.\n");
	return 0;
}

int try_usbgecko_load(void)
{
	if (!usbgecko_checkgecko()) {
		printf("USBGecko not found\n");
		return -1;
	}

#define FLASH_OFFSET 0x30000
	int i, size = usbgecko_flash_read32(FLASH_OFFSET);
	if (size < 0) {
		printf("Invalid code size in usbgecko flash (%d)\n", size);
		return -1;
	}
	printf("Loading %d bytes from USBGecko flash (offset=%x)\n",
		size, FLASH_OFFSET+4);

	for (i=0; i < size; i++)
		code_buffer[i] = usbgecko_flash_read8(FLASH_OFFSET + 4 + i);

	return 0;
}

int main(void)
{
	// slot LED
	write32(0xcd8000c0, 0x20);

	dsp_reset();

	exception_init();

	// Install trampoline at 80001800; some payloads like to jump
	// there to restart.  Sometimes this can even work.
	//memcpy(trampoline_buffer, reboot_trampoline, sizeof(reboot_trampoline));

	// Clear interrupt mask.
	write32(0x0c003004, 0);

	// Unlock EXI.
	write32(0x0d00643c, 0);

	video_init();
	usbgecko_init();

	printf("savezelda\n%s\n", version);

	printf("\n");
	printf("Copyright 2008,2009  Segher Boessenkool\n");
	printf("Copyright 2008  Haxx Enterprises\n");
	printf("Copyright 2008  Hector Martin (\"marcan\")\n");
	printf("Copyright 2003,2004  Felix Domke\n");
	printf("\n");
	printf("This code is licensed to you under the terms of the\n");
	printf("GNU GPL, version 2; see the file COPYING\n");
	printf("\n");
	printf("Font and graphics by Freddy Leitner\n");
	printf("\n");
	printf("\n");

	printf("Cleaning up environment... ");

	reset_ios();

	printf("OK.\n");


	int err;

 restart:
	err = try_sd_load();

	if (err) {
		err = try_usbgecko_load();

		if (err) {
			printf("No code found to load, hanging.\n");
			for (;;)
				;
		}
	}

	if (valid_elf_image(code_buffer)) {
		printf("Valid ELF image detected.\n");
		void (*entry)() = load_elf_image(code_buffer);
		entry();
		printf("Program returned to loader, reloading.\n");
	} else
		printf("No valid ELF image detected, retrying.\n");

	goto restart;
}
