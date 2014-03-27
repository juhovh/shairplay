/**
 *  Copyright (C) 2014  Joakim Plate
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

#ifndef BIO_H
#define BIO_H

#include <inttypes.h>


static __inline uint8_t  bio_get_be_u08(unsigned char ptr[1])
{
	return ptr[0];
}

static __inline uint16_t bio_get_be_u16(unsigned char ptr[2])
{
	return ((uint16_t)bio_get_be_u08(&ptr[0]) <<  8)
	     | ((uint16_t)bio_get_be_u08(&ptr[1]) <<  0);
}

static __inline uint32_t bio_get_be_u24(unsigned char ptr[3])
{
	return ((uint16_t)bio_get_be_u08(&ptr[0]) << 16)
	     | ((uint16_t)bio_get_be_u08(&ptr[1]) <<  8)
	     | ((uint16_t)bio_get_be_u08(&ptr[2]) <<  0);
}

static __inline uint32_t bio_get_be_u32(unsigned char ptr[4])
{
	return ((uint32_t)bio_get_be_u16(&ptr[0]) << 16)
	     | ((uint32_t)bio_get_be_u16(&ptr[2]) <<  0);
}

static __inline uint64_t bio_get_be_u64(unsigned char ptr[8])
{
	return ((uint64_t)bio_get_be_u32(&ptr[0]) << 32)
	     | ((uint64_t)bio_get_be_u32(&ptr[4]) <<  0);
}

static __inline void bio_set_be_u16(unsigned char ptr[2], uint16_t value)
{
	ptr[0] = value >> 8;
	ptr[1] = value >> 0;
}

static __inline void bio_set_be_u24(unsigned char ptr[2], uint32_t value)
{
	ptr[0] = value >> 16;
	ptr[1] = value >>  8;
	ptr[2] = value >>  0;
}

static __inline void bio_set_be_u32(unsigned char ptr[4], uint32_t value)
{
	bio_set_be_u16(&ptr[0], value >> 16);
	bio_set_be_u16(&ptr[2], value >>  0);
}

static __inline void bio_set_be_u64(unsigned char ptr[8], uint64_t value)
{
	bio_set_be_u32(&ptr[0], value >> 32);
	bio_set_be_u32(&ptr[4], value >>  0);
}

#endif
