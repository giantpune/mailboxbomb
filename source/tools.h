#ifndef TOOLS_H
#define TOOLS_H


#include <iostream>
#include <sstream>
#include <string>

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "buffer.h"



#define INC_FILE( x )			\
		extern const u8 x [];	\
		extern const u32 x##_size;

// macros for printing pretty debug crap
#define NAME_WIDTH	30

#define TOSTR( x )	#x
#define STR( tok ) TOSTR( tok )

#define COUT_U32( x )																	\
	printf( "        %-" STR( NAME_WIDTH ) "s: %08x\n", #x, x )

#define COUT_STR( x )																	\
	cout << left << "        " << setw( NAME_WIDTH ) << #x << ": \"" << x << "\"" << endl

#define COUT_BUF( x, siz, chunk )														\
	do																					\
	{																					\
		Buffer x##h = x.ToHex();														\
		string x##s( (const char*)x##h.ConstData(), ( siz ) << 1 );						\
		u8 c = ( sizeof( chunk ) << 1 );												\
		int l = x##s.size();															\
		l += ( l / c ) - 1;																\
		for( int i = c; i < l; i += c + 1 )												\
		{																				\
			x##s.insert( i, 1, '-' );													\
		}																				\
		cout << left << "        " << setw( NAME_WIDTH ) << #x << ": " << x##s << endl;	\
	}while( 0 )

#define WRN								\
	cout << __PRETTY_FUNCTION__ << ": "

using namespace std;

inline u64 htonll( u64 v )
{
	union
	{
		u32 lv[ 2 ];
		u64 llv;
	} u;
	u.lv[ 0 ] = htonl( v >> 32 );
	u.lv[ 1 ] = htonl( v & 0xFFFFFFFFULL );
	return u.llv;
}

inline void wbe64( void *ptr, u64 val ) { *(u64*)ptr = htonll( val ); }
inline void wbe32( void *ptr, u32 val ) { *(u32*)ptr = htonl( val ); }
inline void wbe16( void *ptr, u16 val ) { *(u16*)ptr = htons( val ); }

void PutU16Str( u8* buf, const std::string &str );
void hexdump( const void *d, int len );

bool WriteFile( const string &path, const Buffer &ba );

bool DirExists( const string &path );
bool DirExists( const char *path );

#if 0
Buffer ReadFile( const string &path );
#endif

int MkPath( const char *path, mode_t mode );

string ResolvePath( const string &path );

#endif // TOOLS_H
