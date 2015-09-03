/* Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
   This code is licensed to you under the terms of the GNU GPL, version 2;
   see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt  */

OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

ENTRY(_start)

SECTIONS {
	. = 0x92ffffd4;
	.sbss : {
		*(.bss.a)
		*(.bss.b)
		*(.bss.c)
		*(.bss.d)
		*(.bss.e)
		*(.bss.f)
		*(.bss.g)
		*(.bss.h)
		*(.bss.i)
		*(.bss.j)
		*(.bss.k)
		}

	.start : { crt0.o(*) }
	.text.main : { *(.text.main) }
	.text : { *(.text) }
	.rodata : { *(.rodata .rodata.*)}
	.data : { *(.data) }

	.bss : {
		*(.bss)
		}
}
