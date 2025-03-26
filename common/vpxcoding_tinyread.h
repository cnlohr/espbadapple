/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *  Amalgam is (c) 2024,2025 cnlohr
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the libvpx
 *  source tree. An additional intellectual property rights grant can
 *  be found in the file PATENTS in the libvpx source tree.  All
 *  contributing project authors may be found in the AUTHORS file in
 *  the root of the libvpx source tree.
 *
 * This is designed for very tight embedded use, edited by cnlohr.
 */

#ifndef _VPX_TINYREAD_SFH_H
#define _VPX_TINYREAD_SFH_H

#include <stdint.h>
#include <limits.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VPXCODING_DECORATOR
#define VPXCODING_DECORATOR static
#endif

#ifdef VPXCODING_AUTOGEN_NORM
uint8_t vpx_norm[256];
#else
VPXCODING_DECORATOR const uint8_t vpx_norm[256] = {
	0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
#endif


// This is meant to be a large, positive constant that can still be
// efficiently loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.  Also
// on RISC-V this turns into a LUI
#define LOTS_OF_BITS 0x40000000


typedef uint32_t BD_VALUE;
typedef uint8_t vpx_prob;

#define BD_VALUE_SIZE ((int)sizeof(BD_VALUE) * CHAR_BIT)

typedef void (*vpx_ingest_cb)(void *decrypt_state, const unsigned char *input,
                               unsigned char *output, int count);

typedef struct {
	// Be careful when reordering this struct, it may impact the caching.
	BD_VALUE value;
	unsigned int range;
	int count;
	const uint8_t *buffer_end;
	const uint8_t *buffer;
} vpx_reader;


VPXCODING_DECORATOR int vpx_reader_init(vpx_reader *r, const uint8_t *buffer, size_t size);
VPXCODING_DECORATOR int vpx_read(vpx_reader *r, int prob);
VPXCODING_DECORATOR int vpx_tree_read( vpx_reader * reader, const uint8_t * probabilities, int num_probabilities, int bits_for_max_element );

// These 3 should always be static inline.
static inline int vpx_read_bit(vpx_reader *r) {
	return vpx_read(r, 128);  // vpx_prob_half
}

VPXCODING_DECORATOR int vpx_reader_init(vpx_reader *r, const uint8_t *buffer, size_t size )
{
#ifdef VPXCODING_AUTOGEN_NORM
	if( vpx_norm[1] == 0 )
	{
		//	0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,...
		int n;
		for( n = 1; n < 256; n++ )
		{
			int b;
			for( b = 0; ((255-n)<<b)&0x80; b++ );
			vpx_norm[n] = b;
			//printf( "%d=%d\n", n, b );
		}
	}
#endif
	r->buffer_end = buffer + size;
	r->buffer = buffer;
	r->value = 0;
	r->count = -8;
	r->range = 255;
	return vpx_read_bit(r) != 0;  // marker bit
}

VPXCODING_DECORATOR int vpx_read(vpx_reader *r, int prob)
{
	BD_VALUE value;
	int count;

	value = r->value;
	count = r->count;

	if (r->count < 0)
	{
		//vpx_reader_fill
		const uint8_t *const buffer_end = r->buffer_end;
		const uint8_t *buffer = r->buffer;
		const uint8_t *buffer_start = buffer;
		const size_t bytes_left = buffer_end - buffer;
		const size_t bits_left = bytes_left * CHAR_BIT;
		int shift = BD_VALUE_SIZE - CHAR_BIT - (count + CHAR_BIT);

		if (bits_left > BD_VALUE_SIZE) {
			const int bits = (shift & 0xfffffff8) + CHAR_BIT;
			BD_VALUE nv;
			BD_VALUE big_endian_values = 0;
			int n;
			for( n = 0; n < 4; n++ ) big_endian_values = (big_endian_values<<8) | buffer[n];
			nv = big_endian_values >> (BD_VALUE_SIZE - bits);
			count += bits;
			buffer += (bits >> 3);
			value = r->value | (nv << (shift & 0x7));
#ifdef CHECKPOINT
			CHECKPOINT(vpxcheck=1,vpxcpv=nv);
#endif
		} else {
			const int bits_over = (int)(shift + CHAR_BIT - (int)bits_left);
			int loop_end = 0;
			if (bits_over >= 0) {
				count += LOTS_OF_BITS;
				loop_end = bits_over;
			}

			if (bits_over < 0 || bits_left) {
				while (shift >= loop_end) {
					count += CHAR_BIT;
					value |= (BD_VALUE)*buffer++ << shift;
					shift -= CHAR_BIT;
				}
			}
		}

		// NOTE: Variable 'buffer' may not relate to 'r->buffer' after decryption,
		// so we increase 'r->buffer' by the amount that 'buffer' moved, rather
		// than assign 'buffer' to 'r->buffer'.
		r->buffer += buffer - buffer_start;
	}

	BD_VALUE bigsplit;
	unsigned int range;
	unsigned int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;
	unsigned int bit = 0;

	bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);

	range = split;

	if (value >= bigsplit) {
		range = r->range - split;
		value = value - bigsplit;
		bit = 1;
	}

	const unsigned char shift = vpx_norm[(unsigned char)range];
	r->range = range << shift;
	r->value = value << shift;
	r->count = count - shift;
	return bit;
}

VPXCODING_DECORATOR int vpx_tree_read( vpx_reader * reader, const uint8_t * probabilities, int num_probabilities, int bits_for_max_element )
{
	int probplace = 0;
	int ret = 0;
	int level;
	for( level = 0; level < bits_for_max_element; level++ )
	{
		if( probplace >= num_probabilities ) return -1;
		uint8_t probability = probabilities[probplace];
		int bit = vpx_read( reader, probability );
		ret |= bit<<(bits_for_max_element-level-1);
		if( bit )
			probplace += 1<<(bits_for_max_element-level-1);
		else
			probplace++;
	}
	return ret;
}

#ifdef __cplusplus
};
#endif

#endif
