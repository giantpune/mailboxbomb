# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

	.globl _start
_start:
	# as long as it doesn't crash, this is a good enough stack
	# we cant use the existing stack as we might be loading an elf in that location
	lis 1, 0x9310;

	# initialize bss?  meh
	# Clear BSS.
#	lis 3,__bss_start@ha ; addi 3,3,__bss_start@l
#	li 4,0
#	lis 5,__bss_end@ha ; addi 5,5,__bss_end@l ; sub 5,5,3
#	bl memset

	# there is no branch to main() or anything here because the ld script
	# and compiler options insert main() directly after this file. so no branching needed
