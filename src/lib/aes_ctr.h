/**
 *  Copyright (C) 2018  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef AES_CTR_H
#define AES_CTR_H

#include <stdint.h>
#include "crypto/crypto.h"

typedef struct aes_ctr_key_st {
	AES_CTX aes_ctx;
	uint8_t counter[AES_BLOCKSIZE];
	uint8_t state[AES_BLOCKSIZE];
	uint8_t available;
} AES_CTR_CTX;

void AES_ctr_set_key(AES_CTR_CTX *ctx, const uint8_t *key, const uint8_t *nonce, AES_MODE mode);
void AES_ctr_encrypt(AES_CTR_CTX *ctx, const uint8_t *msg, uint8_t *out, int length);

#endif
