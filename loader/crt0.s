# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt


	.globl _start
_start:

	# Disable interrupts, enable FP.
	mfmsr 3 ; rlwinm 3,3,0,17,15 ; ori 3,3,0x2000 ; mtmsr 3 ; isync

	# Setup stack.
	lis 1,_stack_top@ha ; addi 1,1,_stack_top@l ; li 0,0 ; stwu 0,-64(1)

	# Clear BSS.
	lis 3,__bss_start@ha ; addi 3,3,__bss_start@l
	li 4,0
	lis 5,__bss_end@ha ; addi 5,5,__bss_end@l ; sub 5,5,3
	bl memset


	# set [DI]BAT0 for 256MB@80000000,
	# real 00000000, WIMG=0000, R/W
	li 3,2 ; lis 4,0x8000 ; ori 4,4,0x1fff
	mtspr 529,3 ; mtspr 528,4 ; mtspr 537,3 ; mtspr 536,4 ; isync

	# set [DI]BAT4 for 256MB@90000000,
	# real 10000000, WIMG=0000, R/W
	addis 3,3,0x1000 ; addis 4,4,0x1000
	mtspr 561,3 ; mtspr 560,4 ; mtspr 569,3 ; mtspr 568,4 ; isync

	# set DBAT1 for 256MB@c0000000,
	# real 00000000, WIMG=0101, R/W
	li 3,0x2a ; lis 4,0xc000 ; ori 4,4,0x1fff
	mtspr 539,3 ; mtspr 538,4 ; isync

	# set DBAT5 for 256MB@d0000000,
	# real 10000000, WIMG=0101, R/W
	addis 3,3,0x1000 ; addis 4,4,0x1000
	mtspr 571,3 ; mtspr 570,4 ; isync

	# enable [DI]BAT4-7 in HID4
	lis 3, 0x8200
	mtspr 1011,3

	# Go!
	bl main

	# If it returns, hang.  Shouldn't happen.
	b .


