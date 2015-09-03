
#include "loader.h"


#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )
#define TV( x )\
	printf( #x": %08x\n", (x) );

// this is to make all these function pointers point to the right spot
//! they are sort of statically-dynamically linked (tm) functions
//! we rely on the ld script and the code that loads this binary to take care of the linking

#define IMPORT( x ) __attribute__((section(".bss." #x )))

// which version of their filestruct is used
u32 IMPORT( a ) fileStructVersion;

// reboot the system menu
void __attribute__(( noreturn, section(".bss.b") ))(*OSReturnToMenu)();

// kill audio
void IMPORT( c ) (*__OSStopAudioSystem)();

// abort gx stuff
void IMPORT( d ) (*__GXAbort)();

// open a file from the currently mounted device
//! returns pointer to file object
u32* IMPORT( e ) (*VFSysOpenFile_current)( const char* path, const char* mode );

// read an open file
//! returns 0 on success?
int IMPORT( f ) (*VFSysReadFile)( u32 *outLength, void* buffer, u32 size, u32 *fp );

// release stm
void IMPORT( g ) (*__OSUnRegisterStateEvent)();

// blackout the screen
void IMPORT( h ) (*VISetBlack)( int one );

// flush video
void IMPORT( i ) (*VIFlush)();

// convert a letter to a mounted FAT device
//! returns a 'pointer' to the device mounted at that letter or 0 on error
u32 IMPORT( j ) (*VFiPFVOL_GetVolumeFromDrvChar)( int letter );

// set a FAT device as current
//! no clue what the return value is.  it wasnt being checked in the code i looked at
u32 IMPORT( k ) (*VFiPFVOL_SetCurrentVolume)( u32 pointer );

void *sync_before_exec( void *p, u32 len )
{
	u32 a, b;

	a = (u32)p & ~0x1f;
	b = ((u32)p + len + 0x1f) & ~0x1f;

	for ( ; a < b; a += 32)
	{
		asm("dcbst 0,%0 ; sync ; icbi 0,%0 ; isync" : : "b"(a));// this is probably superfluous cache syncing, but it saves 4 bytes in the final binary
	}
	return p;
}

#ifdef DEBUG
char ascii( char s ) {
	if ( s < 0x20 ) return '.';
	if ( s > 0x7E ) return '.';
	return s;
}

void hexdump( const void *d, int len ) {
	unsigned char *data;
	int i, off;
	data = (unsigned char*)d;
	printf( "\n");
	for ( off = 0; off < len; off += 16 ) {
		printf( "%08x  ", off );
		for ( i=0; i<16; i++ )
		{
			if( ( i + 1 ) % 4 )
			{
				if ( ( i + off ) >= len ) printf("  ");
				else printf("%02x",data[ off + i ]);
			}
			else
			{
				if ( ( i + off ) >= len ) printf("   ");
				else printf("%02x ",data[ off + i ]);
			}
		}

		printf( " " );
		for ( i = 0; i < 16; i++ )
			if ( ( i + off) >= len ) printf(" ");
		else printf("%c", ascii( data[ off + i ]));
		printf("\n");
	}
}
#else
#define hexdump( x, y )
#endif

typedef struct _dolheader {
	u32 data_pos[ 18 ];
	u32 data_start[ 18 ];
	u32 data_size[ 18 ];
	u32 bss_start;
	u32 bss_size;
	void *entry_point;
} dolheader;

// this dol loader makes a lot of assumptions.  like that the dol is valid
//! it will try to load any pointer passed to except NULL
static void LoadDol( const void *dolstart )
{
	u32 i;
	dolheader *dolfile;
	dolfile = (dolheader *) dolstart;
	if( !dolstart )
	{
		return;
	}

	for( i = 0; i < 18; i++ )
	{
		sync_before_exec( memcpy( (void *)dolfile->data_start[ i ], dolstart+dolfile->data_pos[ i ], dolfile->data_size[ i ] ), dolfile->data_size[ i ] );
	}

	memset( (void *)dolfile->bss_start, 0, dolfile->bss_size );
	void __attribute__(( noreturn )) (*Entry)() = dolfile->entry_point;
	Entry();
}

static inline int valid_elf_image(void *addr)
{
	u32 *header = addr;

	return addr && header[0] == 0x7f454c46		// ELF
		&& header[1] == 0x01020100	// 32-bit, BE, ELF v1, SVR
		&& header[4] == 0x00020014	// executable, PowerPC
		&& header[5] == 1		// object file v1
		&& (header[10] & 0xffff) == 32;	// PHDR size
}

// either loads the elf and executes it, or returns the same pointer it was passed
static inline void *LoadElf( void *addr )
{
	if( !valid_elf_image( addr ) )
	{
		return addr;
	}

	u32 *header = addr;
	u32 *phdr = addr + header[7];
	u32 n = header[11] >> 16;
	u32 i;

	for (i = 0; i < n; i++, phdr += 8) {
		if (phdr[0] != 1)	// PT_LOAD
			continue;

		u32 off = phdr[1];
		void *dest = (void *)phdr[3];
		u32 filesz = phdr[4];
		u32 memsz = phdr[5];

		//memcpy(dest, addr + off, filesz);
		//memset(dest + filesz, 0, memsz - filesz);

		//sync_before_exec( dest, memsz );

		// passing the return walue of memcpy to the sync_before_exec saves 4 bytes
		sync_before_exec( memcpy( dest, addr + off, filesz ), memsz );
		memset(dest + filesz, 0, memsz - filesz);
	}
	void __attribute__(( noreturn )) (*Entry)() = (void *)header[6];
	Entry();
}

// cleanup a little bit
void ShutDown()
{
	__GXAbort();
	__OSStopAudioSystem();
	__OSUnRegisterStateEvent();
}

// making this global shaves off 4 bytes
u32 justRead = 0;

u8* OpenAndReadFile()
{
	u8 *buf = (u8*)0x92100000;

	printf( "OpenAndReadFile(): " );
	u32 size;

	// open file
	u32* fp = VFSysOpenFile_current( "/boot.elf", "r+" );
	if( !fp )
	{
		fp = VFSysOpenFile_current( "/boot.dol", "r+" );
		if( !fp )
		{
			printf( "no elf or dol found\n" );
			return NULL;
		}
	}
	//printf( "fd: %08x\n", fp );

	// get size
	size = *(u32*)( fp[ 2 ] + fileStructVersion );

	// read
	printf( "reading 0x%0x bytes...", size );
	VFSysReadFile( &justRead, buf, size, fp );
	//printf( "VFSysReadFile( %08x, %08x ): %d\n", justRead, (u32)buf, r );
	if( justRead != size )
	{
		printf( "short read\n" );
		OSReturnToMenu();
	}
	printf( "ok\n" );
	return buf;
}

u8 MountDrive( int drive )
{
	printf( "MountDrive( \'%c\' ): ", drive );
	u32 volume = VFiPFVOL_GetVolumeFromDrvChar( drive );
	if( !volume )
	{
		printf( "error getting volume \'%c\'\n", drive );
		return 0;
	}
	VFiPFVOL_SetCurrentVolume( volume );
	printf( "ok\n" );
	return 1;
}

void __attribute__(( noreturn )) main()
{
	usbgecko_init();
	VISetBlack( 1 );
	VIFlush();
	ShutDown();

	// check all mounted FAT devices and look for boot.elf and boot.dol
	int ch;
	for( ch = 0x41; ch < 0x5b; ch++ )
	{
		// try to set current device
		if( MountDrive( ch ) )
		{
			// try to load file from whatever device is set as current
			LoadDol( LoadElf( OpenAndReadFile() ) );
		}
	}
	printf( "couldn\'t load file from any device.  exiting...\n" );
	OSReturnToMenu();
}
