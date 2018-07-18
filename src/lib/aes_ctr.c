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

#include <string.h>
#include <assert.h>

#include "aes_ctr.h"

static void
ctr128_inc(uint8_t *counter)
{
	uint32_t n = 16, c = 1;

	do {
		--n;
		c += counter[n];
		counter[n] = (uint8_t) c;
		c >>= 8;
	} while (n);
}

void
AES_ctr_set_key(AES_CTR_CTX *ctx, const uint8_t *key, const uint8_t *nonce, AES_MODE mode)
{
	assert(ctx);

	/* Setting IV as nonce, but it will be overridden in encrypt */
	AES_set_key(&ctx->aes_ctx, key, nonce, mode);
	memcpy(ctx->counter, nonce, AES_BLOCKSIZE);
	memset(ctx->state, 0, AES_BLOCKSIZE);
	ctx->available = 0;
}

void
AES_ctr_encrypt(AES_CTR_CTX *ctx, const uint8_t *msg, uint8_t *out, int length)
{
	unsigned char block[16];
	int msgidx, i;

	assert(ctx);
	assert(msg);
	assert(out);

	msgidx = 0;
	while (msgidx < length) {
		if (ctx->available == 0) {
			/* Generate a new block into state if we have no bytes */
			memset(ctx->aes_ctx.iv, 0, AES_IV_SIZE);
			AES_cbc_encrypt(&ctx->aes_ctx, ctx->counter, ctx->state, AES_BLOCKSIZE);
			ctx->available = AES_BLOCKSIZE;
			ctr128_inc(ctx->counter);
		}
		for (i=0; i<ctx->available && msgidx<length; i++, msgidx++) {
			out[msgidx] = msg[msgidx] ^ ctx->state[AES_BLOCKSIZE-ctx->available+i];
		}
		ctx->available -= i;
	}
}
