#ifndef CRYPTOSTUFF_H
#define CRYPTOSTUFF_H

#include "aes.h"
#include "buffer.h"
#include "sha1.h"
#include "tools.h"

typedef struct
{
	SHA1Context hash_ctx;
	unsigned char key[ 0x40 ];
} hmac_ctx;

u32 ComputeCRC32( u8 *Buffer, u16 Size ) __attribute__ ( ( const ) );

void hmac_init(hmac_ctx *ctx, const char *key, int key_size);
void hmac_update( hmac_ctx *ctx, const u8 *data, int size );
void hmac_final( hmac_ctx *ctx, unsigned char *hmac );

Buffer GetSha1( const Buffer &stuff );

#endif // CRYPTOSTUFF_H
