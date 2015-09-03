// Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// This code is licensed to you under the terms of the GNU GPL, version 2;
// see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#ifndef _LOADER_H
#define _LOADER_H

#include <stddef.h>


// String functions.

//size_t strlen(const char *);
//size_t strnlen(const char *, size_t);
void *memset(void *, int, size_t) __attribute__ ((externally_visible));
void *memcpy(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);


// Basic types.

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned int u32;
typedef unsigned long long int u64;
/*
static inline u16 le16(const u8 *p)
{
	return p[0] | (p[1] << 8);
}

static inline u32 le32(const u8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}*/


// Basic I/O.

#ifdef DEBUG
static inline u32 read32(u32 addr)
{
	u32 x;

	asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write32(u32 addr, u32 x)
{
	asm("stw %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}

static inline u16 read16(u32 addr)
{
	u16 x;

	asm volatile("lhz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));

	return x;
}

static inline void write16(u32 addr, u16 x)
{
	asm("sth %0,0(%1) ; eieio" : : "r"(x), "b"(0xc0000000 | addr));
}


// Address mapping.
/*
static inline u32 virt_to_phys(const void *p)
{
	return (u32)p & 0x7fffffff;
}

static inline void *phys_to_virt(u32 x)
{
	return (void *)(x | 0x80000000);
}*/





// USB Gecko.


void usbgecko_init(void);
int usbgecko_checkgecko(void);
void usbgecko_console_putc(u8 c);


// Version string.

//extern const char version[];


// Video.

//void video_init(void);
//void fb_putc(char);


// Console.

//void console_init(void);
int printf(const char *fmt, ...);
#else
#define printf( ... )
#define usbgecko_init()
#endif


#endif
