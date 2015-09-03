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
#include <stdio.h>
#include <string.h>

#include "buffer.h"
#include "tools.h"

#ifdef DEBUG_BUFFER
#define dbg( ... )	printf( __VA_ARGS__ )
#else
#define dbg( ... )
#endif

Buffer::Buffer( const void* stuff, u32 size ) : ptr( NULL ), len( 0 )
{
	if( stuff )
		SetData( stuff, size );
}

Buffer::Buffer() : ptr( NULL ), len( 0 )
{
}

Buffer::Buffer( u32 size )
{
	ptr = malloc( size );
	if( !ptr )
	{
		dbg("Buffer::Buffer() failed to allocate memory (0x%08x bytes)\n", size );
		len = 0;
		return;
	}
	len = size;
}

Buffer::Buffer( u32 size, char fillChar )
{
	ptr = malloc( size );
	if( !ptr )
	{
		dbg("Buffer::Buffer() failed to allocate memory (0x%08x bytes)\n", size );
		len = 0;
		return;
	}
	len = size;
	memset( ptr, fillChar, len );
}

Buffer::Buffer( const Buffer &other )
{
	ptr = NULL;
	len = 0;
	SetData( other.ptr, other.len );
}

Buffer::~Buffer()
{
	if( ptr )
		free( ptr );
}

void Buffer::Free()
{
	if( ptr )
	{
		free( ptr );
		ptr = NULL;
	}
	len = 0;
}

void Buffer::Dump( u32 start, u32 size )
{
	if( !size )
		size = len - start;

	if( ( start + size ) > len || !ptr )
		return;

	hexdump( (u8*)ptr + start, size );
}

Buffer &Buffer::SetData( const void* stuff, u32 size )
{
	// TODO - this probably wont work out too well if "stuff" is already contained within this buffer
	if( ptr )
	{
		free( ptr );
		ptr = NULL;
		len = 0;
	}
	if( !stuff && !size )
		return *this;

	if( !size )
	{
		len = strlen( (const char*)stuff );
	}
	else
	{
		len = size;
	}
	if( !len )//wtf.  this probably will never happen
		return *this;

	ptr = malloc( len );
	if( !ptr )
	{
		dbg("Buffer::Buffer() failed to allocate memory (0x%08x bytes)\n", len );
		len = 0;
		return *this;
	}
	memset( ptr, 0, len );
	if( stuff )
	{
		memcpy( ptr, stuff, len );
	}
	return *this;
}

Buffer &Buffer::Append( const void* stuff, u32 size )
{
	if( !stuff )
		return *this;
	if( !size )
	{
		size = strlen( (const char*)stuff );
	}
	ptr = realloc( ptr, len + size );
	if( !ptr )
	{
		dbg( "Buffer::Append(): failed to allocate memory\n" );
		len = 0;
		return *this;
	}
	memcpy( (u8*)ptr + len, stuff, size );
	len += size;
	return *this;
}

Buffer &Buffer::Append( const Buffer &other )
{
	return Append( other.ptr, other.len );
}

Buffer &Buffer::Append( char c )
{
	ptr = realloc( ptr, len + 1 );
	if( !ptr )
	{
		dbg( "Buffer::Append(): failed to allocate memory\n" );
		len = 0;
		return *this;
	}
	char *ch = (char*)ptr;
	ch[ len ] = c;
	len++;
	return *this;
}

Buffer &Buffer::Resize( u32 size )
{
	if( !size )
	{
		Free();
		return *this;
	}
	ptr = realloc( ptr, size );
	if( !ptr )
	{
		dbg( "Buffer::Resize(): failed to allocate memory\n" );
		len = 0;
		return *this;
	}
	len = size;
	return *this;
}

Buffer &Buffer::Insert( u32 pos, char c )
{
	ptr = realloc( ptr, len + 1 );
	if( !ptr )
	{
		dbg( "Buffer::Insert(): failed to allocate memory\n" );
		len = 0;
		return *this;
	}

	//copy the chunk after the insertion point
	char *ch = (char*)ptr;
	u32 p1 = len - 1;
	u32 p2 = p1 + 1;
	while( p1 >= pos + 1 )
	{
		ch[ p2-- ] = ch[ p1-- ];
	}
	//copy the new data
	ch[ pos ] = c;
	len++;
	return *this;
}

Buffer &Buffer::Insert( u32 pos, const void* stuff, u32 size )
{
	if( !stuff )
	{
		dbg( "Buffer::Insert(): stuff = NULL\n" );
		return *this;
	}
	if( pos > len )
	{
		dbg( "Buffer::Insert(): pos > len [ %08x > %08x ]\n", pos, len );
		return *this;
	}
	ptr = realloc( ptr, len + size );
	if( !ptr )
	{
		dbg( "Buffer::Insert(): failed to allocate memory\n" );
		len = 0;
		return *this;
	}

	//copy the chunk after the insertion point
	u32 p1 = len - 1;
	u32 p2 = ( len + size ) -1;
	char *ch = (char*)ptr;
	while( p1 >= pos + 1 )
	{
		ch[ p2-- ] = ch[ p1-- ];
	}
	//copy the new data
	memcpy( ch + pos, stuff, size );
	len += size;
	return *this;
}

Buffer &Buffer::Insert( u32 pos, const Buffer &other )
{
	return Insert( pos, other.ptr, other.len );
}

Buffer &Buffer::operator=( const Buffer &other )
{
    if( this == &other )
    {
        return *this;
    }
	Free();
	if( other.len )
	{
		ptr = malloc( other.len );
		if( !ptr )
		{
			dbg( "Buffer::operator=: failed to allocate memory\n" );
			len = 0;
			return *this;
		}
		len = other.len;
		memcpy( ptr, other.ptr, len );
	}
	return *this;
}

Buffer &Buffer::operator=( const char* stuff )
{
    if( (const char*)ptr == stuff )
    {
        return *this;
    }
	Free();
	if( !stuff )
		return *this;

	len = strlen( stuff );
	ptr = malloc( len );
	if( !ptr )
	{
		dbg( "Buffer::operator=: failed to allocate memory\n" );
		len = 0;
		return *this;
	}
	memcpy( ptr, stuff, len );
	return *this;
}

Buffer Buffer::FromHex( const std::string &hexEncoded )
{
	Buffer res( ( hexEncoded.size() + 1 ) / 2 );

	u8 *result = (u8 *)res.Data() + res.Size();
	bool odd_digit = true;
	for( int i = hexEncoded.size() - 1; i >= 0; --i )
	{
		int ch = hexEncoded.at( i );
		int tmp;
		if( ch >= '0' && ch <= '9' )
			tmp = ch - '0';
		else if( ch >= 'a' && ch <= 'f' )
			tmp = ch - 'a' + 10;
		else if( ch >= 'A' && ch <= 'F' )
			tmp = ch - 'A' + 10;
		else
			continue;
		if( odd_digit )
		{
			--result;
			*result = tmp;
			odd_digit = false;
		}
		else
		{
			*result |= tmp << 4;
			odd_digit = true;
		}
	}
	if( result == (const u8 *)res.ConstData() )
	{
		return res;
	}
	Buffer ret( result - (const u8 *)res.ConstData() );
	if( (s64)ret.Size() != result - (const u8 *)res.ConstData() )
	{
		dbg( "Buffer::FromHex() out of memory\n" );
		return res;
	}
	memcpy( ret.Data(), result, res.Size() - ( result - (const u8 *)res.ConstData() ) );
	return ret;
}

Buffer Buffer::ToHex() const
{
	Buffer hex( len * 2 );
	u8 *hexData = (u8*)hex.Data();
	const u8 *data = (u8*)ptr;
	for( u32 i = 0; i < len; ++i )
	{
		int j = ( data[ i ] >> 4 ) & 0xf;
		if( j <= 9 )
			hexData[ i << 1 ] = ( j + '0' );
		else
			hexData[ i << 1 ] = ( j + 'a' - 10 );
		j = data[ i ] & 0xf;
		if( j <= 9 )
			hexData[ ( i << 1 ) + 1 ] = ( j + '0' );
		else
			hexData[ ( i << 1 ) + 1 ] = ( j + 'a' - 10 );
	}
	return hex;
}
