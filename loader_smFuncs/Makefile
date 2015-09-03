# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#CROSS ?= $(WIIDEV)/bin/powerpc-elf-
CROSS ?= $(DEVKITPRO)/devkitPPC_186/bin/powerpc-gekko-


ifeq ($(origin CC), default)
	CC = $(CROSS)gcc -m32
endif

OBJCOPY ?= $(CROSS)objcopy



DEPDIR = .deps
MACHDEP = -mcpu=750 -meabi -mhard-float
CFLAGS = $(MACHDEP) -Os -Wall -pipe -ffunction-sections -finline-functions-called-once --combine -fwhole-program -ffreestanding
LDFLAGS = $(MACHDEP) -n -nostartfiles -nostdlib
ASFLAGS = -D_LANGUAGE_ASSEMBLY

targets := loader.bin

CFILES = main.c string.c
OBJS := crt0.o _all.o


ifeq ($(debug),1)
	CFLAGS += -DDEBUG -std=gnu99
	CFILES += console.c usbgecko.c
endif


all: $(targets)

$(targets): %.bin: %.elf
	@echo "  OBJCOPY   $@"
	@$(OBJCOPY) -O binary $< $@

elfs := $(targets:.bin=.elf)
$(elfs): %.elf: %.lds $(OBJS)
	@echo "  LINK      $@"
	@$(CC) $(LDFLAGS) -n -T $^ -o $@

%.o: %.s
	@echo "  ASSEMBLE  $<"
	@$(CC) $(CFLAGS) $(DEFINES) $(ASFLAGS) -c $< -o $@

_all.o: $(CFILES)
	@echo "  COMPILE ALL"
	@mkdir -p $(DEPDIR)
	@$(CC) $(CFLAGS) $(DEFINES) -Wp,-MMD,$(DEPDIR)/$(*F).d,-MQ,"$@",-MP -c $(CFILES) -o $@


FORCE:

clean:
	@echo "clean..."
	@rm -rf $(OBJS) $(targets) $(elfs) $(DEPDIR)
