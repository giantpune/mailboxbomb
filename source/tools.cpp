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

#include "tools.h"

#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#ifdef WIN32
#define MKDIR( x, y )					\
	mkdir( x )
#else
#define MKDIR( x, y )					\
	mkdir( x, y )
#endif

void PutU16Str( u8* buf, const string &str )
{
	u32 len = str.size() + 1;
	for( u32 i = 0; i < len; i++ )
	{
		wbe16( buf + ( i << 1 ), str[ i ] );
	}
}

static inline char ascii( char s )
{
	return ( ( s < 0x20 || s > 0x7e ) ? '.' : s );
}

#define HDUMP_FILE	stdout
void hexdump( const void *d, int len )
{
	unsigned char *data = (unsigned char*)d;
	const char *space = "   ";
	const char *nl = "\n";
	int foo;
	foo = fwrite( nl, 1, 1, HDUMP_FILE );
	for( int off = 0; off < len; off += 16 )
	{
		fprintf( HDUMP_FILE, "%08x  ", off );
		for( int i = 0; i < 16; i++ )
		{
			if( ( i + 1 ) % 4 )
			{
				if ( ( i + off ) >= len )
					foo = fwrite( space, 1, 2, HDUMP_FILE );
				else
					fprintf( HDUMP_FILE, "%02x", data[ off + i ] );
			}
			else
			{
				if ( ( i + off ) >= len )
					foo = fwrite( space, 1, 3, HDUMP_FILE );
				else
					fprintf( HDUMP_FILE, "%02x ", data[ off + i ] );
			}
		}

		foo = fwrite( space, 1, 1, HDUMP_FILE );
		for( int i = 0; i < 16; i++ )
		{
			if ( ( i + off ) >= len )
				foo = fwrite( space, 1, 1, HDUMP_FILE );
			else
				fprintf( HDUMP_FILE, "%c", ascii( data[ off + i ] ) );
		}
		foo = fwrite( nl, 1, 1, HDUMP_FILE );
	}
	fflush( HDUMP_FILE );
	(void)foo;
}

typedef struct stat Stat;

static int do_mkdir( const char *path, mode_t mode )
{
	Stat            st;
	int             status = 0;

	if( stat( path, &st ) )
	{
		// Directory does not exist
		if( MKDIR( path, mode ) )
			status = -1;
	}
	else if( !S_ISDIR( st.st_mode ) )
	{
		errno = ENOTDIR;
		status = -1;
	}

	return( status );
}

int MkPath( const char *path, mode_t mode )
{
	if( !path )
	{
		return -1;
	}

	char           *pp;
	char           *sp;
	int            status = 0;
	char           *copypath = strdup( path );

	if( !copypath )
	{
		return -1;
	}

#ifdef WIN32
	// skip base drive
	pp = strchr( copypath, SEPC );
	if( !pp )
	{
		return -1;
	}
#else
	pp = copypath;
#endif
	while( !status && ( sp = strchr( pp, SEPC ) ) != 0 )
	{
		if( sp != pp )
		{
			*sp = '\0';
			status = do_mkdir( copypath, mode );
			*sp = SEPC;
		}
		pp = sp + 1;
	}
	if( !status && *pp )
	{
		status = do_mkdir( path, mode );
	}
	free( copypath );
	return status;
}

#if 0
Buffer ReadFile( const string &path )
{
	Stat			filestat;
	Buffer			ret;

	FILE *file = fopen( path.c_str(), "rb" );
	if( !file )
	{
		cout << "ReadFile -> error opening file " << path << endl;
		return Buffer();
	}

	if( fstat( fileno( file ), &filestat ) )
	{
		cout << "ReadFile -> can't stat " << path << endl;
		fclose( file );
		return Buffer();
	}

	ret.Resize( filestat.st_size );
	if( (s64)ret.Size() != filestat.st_size )
	{
		cout << "ReadFile -> not enough memory"<< endl;
		fclose( file );
		return Buffer();
	}

	if( fread( ret.Data(), 1, filestat.st_size, file ) != (u32)filestat.st_size )
	{
		cout << "ReadFile -> error reading file " << path << endl;
		fclose( file );
		return Buffer();
	}

	fclose( file );
	return ret;
}
#endif

bool WriteFile( const string &path, const Buffer &ba )
{
	FILE *fp = fopen( path.c_str(), "wb" );
	if( !fp )
	{
		cout << "WriteFile -> can't open " << path << endl;
		return false;
	}
	if( fwrite( ba.ConstData(), 1, ba.Size(), fp ) != ba.Size() )
	{
		cout << "WriteFile -> error writing to " << path << endl;
		fclose( fp );
		return false;
	}
	fclose( fp );
	return true;
}

bool DirExists( const string &path )
{
	struct stat filestat;
	if( !stat( path.c_str(), &filestat ) && ( ( filestat.st_mode ) & S_IFMT ) == S_IFDIR )
		return true;
	return false;
}

bool DirExists( const char *path )
{
	struct stat filestat;
	if( path && !stat( path, &filestat ) && ( ( filestat.st_mode ) & S_IFMT ) == S_IFDIR )
		return true;
	return false;
}

string ResolvePath( const string &path )
{
#ifdef WIN32	// fucking bill gates
	char full[ _MAX_PATH ];
	if( _fullpath( full, path.c_str(), _MAX_PATH ) )
	{
		return string( full );
	}
	return string();
#else
	char actualpath[ PATH_MAX + 1 ];
	if( realpath( path.c_str(), actualpath ) )
	{
		return string( actualpath );
	}
	return string();
#endif
}

