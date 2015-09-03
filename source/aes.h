#ifndef __AES_H_
#define __AES_H_


#include "types.h"


void aes_encrypt( u8 *iv, const u8 *inbuf, u8 *outbuf, unsigned long long len );
void aes_decrypt( u8 *iv, const u8 *inbuf, u8 *outbuf, unsigned long long len );
void aes_set_key( const u8 *key );

#endif //__AES_H_

