/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *  Amalgam is (c) 2024 cnlohr
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the libvpx
 *  source tree. An additional intellectual property rights grant can
 *  be found in the file PATENTS in the libvpx source tree.  All
 *  contributing project authors may be found in the AUTHORS file in
 *  the root of the libvpx source tree.
 *
 *  This is the bitreader / bitwriter from libvpx from the VP7/8/9
 *  video codec.
 *  
 *  This amalgam has some notable changes:
 *    1. Changed decrypt_state / decrypt_cb to ingest for reader.
 *    2. Removed all libvpx dependencies.
 *
 *  To Use:

#define VPXCODING_READER
#define VPXCODING_WRITER
#include "vpxcoding.h"

 * For embedded use, you can define VPX_64BIT or VPX_32BIT.

 */

#ifndef _VPX_SFH_H
#define _VPX_SFH_H

#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <endian.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default behavior is VPXCODING_IMPLEMENTATION
#ifndef VPXCODING_DECORATOR
#define VPXCODING_DECORATOR static
#define VPXCODING_IMPLEMENTATION
#endif

#ifndef VPXCODING_CUSTOM_VPXNORM
static const uint8_t vpx_norm[256] = {
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

#ifdef VPXCODING_READER

// This is meant to be a large, positive constant that can still be
// efficiently loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.
#define LOTS_OF_BITS 0x40000000

#if !defined( VPX_64BIT ) && !defined( VPX_32BIT )
#if SIZE_MAX == 0xffffffffffffffffULL
#define VPX_64BIT
#else
#define VPX_32BIT
#endif
#endif

#ifdef VPX_64BIT
typedef uint64_t BD_VALUE;
#elif defined( VPX_32BIT )
typedef uint32_t BD_VALUE;
#else
typedef size_t BD_VALUE;
#endif

typedef int8_t vpx_tree_index;
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
	vpx_ingest_cb ingest_cb;
	void *ingest_state;
	uint8_t clear_buffer[sizeof(BD_VALUE) + 1];
} vpx_reader;


VPXCODING_DECORATOR int vpx_reader_init(vpx_reader *r, const uint8_t *buffer,
	size_t size, vpx_ingest_cb ingest_cb, void * ingest_state );
VPXCODING_DECORATOR void vpx_reader_fill(vpx_reader *r);
VPXCODING_DECORATOR const uint8_t * vpx_reader_find_end(vpx_reader *r);
VPXCODING_DECORATOR int vpx_reader_has_error(vpx_reader *r);
VPXCODING_DECORATOR int vpx_read(vpx_reader *r, int prob);

// These 3 should always be static inline.
static inline int vpx_read_bit(vpx_reader *r) {
	return vpx_read(r, 128);  // vpx_prob_half
}

static inline int vpx_read_literal(vpx_reader *r, int bits) {
	int literal = 0, bit;
	for (bit = bits - 1; bit >= 0; bit--) literal |= vpx_read_bit(r) << bit;
	return literal;
}

static inline int vpx_read_tree(vpx_reader *r,
	const vpx_tree_index *tree, const vpx_prob *probs) {
	vpx_tree_index i = 0;
	while ((i = tree[i + vpx_read(r, probs[i >> 1])]) > 0) continue;
	return -i;
}

#ifdef VPXCODING_IMPLEMENTATION

#define VPXMIN(x, y) (((x) < (y)) ? (x) : (y))
#define VPXMAX(x, y) (((x) > (y)) ? (x) : (y))

VPXCODING_DECORATOR int vpx_reader_init(vpx_reader *r, const uint8_t *buffer,
	size_t size, vpx_ingest_cb ingest_cb, void * ingest_state ) {
	if (size && !buffer) {
		return 1;
	} else {
		r->buffer_end = buffer + size;
		r->buffer = buffer;
		r->value = 0;
		r->count = -8;
		r->range = 255;
		r->ingest_cb = ingest_cb;
		r->ingest_state = ingest_state;
		vpx_reader_fill(r);
		return vpx_read_bit(r) != 0;  // marker bit
	}
}

VPXCODING_DECORATOR void vpx_reader_fill(vpx_reader *r)
{
	const uint8_t *const buffer_end = r->buffer_end;
	const uint8_t *buffer = r->buffer;
	const uint8_t *buffer_start = buffer;
	BD_VALUE value = r->value;
	int count = r->count;
	const size_t bytes_left = buffer_end - buffer;
	const size_t bits_left = bytes_left * CHAR_BIT;
	int shift = BD_VALUE_SIZE - CHAR_BIT - (count + CHAR_BIT);

	if (r->ingest_cb) {
		size_t n = VPXMIN(sizeof(r->clear_buffer), bytes_left);
		r->ingest_cb(r->ingest_state, buffer, r->clear_buffer, (int)n);
		buffer = r->clear_buffer;
		buffer_start = r->clear_buffer;
	}
	if (bits_left > BD_VALUE_SIZE) {
		const int bits = (shift & 0xfffffff8) + CHAR_BIT;
		BD_VALUE nv;
		BD_VALUE big_endian_values;
		memcpy(&big_endian_values, buffer, sizeof(BD_VALUE));
#ifdef VPX_64BIT
		big_endian_values = htobe64(big_endian_values);
#else
		big_endian_values = htobe32(big_endian_values);
#endif
		nv = big_endian_values >> (BD_VALUE_SIZE - bits);
		count += bits;
		buffer += (bits >> 3);
		value = r->value | (nv << (shift & 0x7));
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
	r->value = value;
	r->count = count;
}

VPXCODING_DECORATOR const uint8_t * vpx_reader_find_end(vpx_reader *r) {
	// Find the end of the coded buffer
	while (r->count > CHAR_BIT && r->count < BD_VALUE_SIZE) {
		r->count -= CHAR_BIT;
		r->buffer--;
	}
	return r->buffer;
}

VPXCODING_DECORATOR int vpx_reader_has_error(vpx_reader *r) {
  // Check if we have reached the end of the buffer.
  //
  // Variable 'count' stores the number of bits in the 'value' buffer, minus
  // 8. The top byte is part of the algorithm, and the remainder is buffered
  // to be shifted into it. So if count == 8, the top 16 bits of 'value' are
  // occupied, 8 for the algorithm and 8 in the buffer.
  //
  // When reading a byte from the user's buffer, count is filled with 8 and
  // one byte is filled into the value buffer. When we reach the end of the
  // data, count is additionally filled with LOTS_OF_BITS. So when
  // count == LOTS_OF_BITS - 1, the user's data has been exhausted.
  //
  // 1 if we have tried to decode bits after the end of stream was encountered.
  // 0 No error.
  return r->count > BD_VALUE_SIZE && r->count < LOTS_OF_BITS;
}

VPXCODING_DECORATOR int vpx_read(vpx_reader *r, int prob) {
	unsigned int bit = 0;
	BD_VALUE value;
	BD_VALUE bigsplit;
	int count;
	unsigned int range;
	unsigned int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;

	if (r->count < 0) vpx_reader_fill(r);

	value = r->value;
	count = r->count;

	bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);

	range = split;

	if (value >= bigsplit) {
		range = r->range - split;
		value = value - bigsplit;
		bit = 1;
	}

	{
		const unsigned char shift = vpx_norm[(unsigned char)range];
		range <<= shift;
		value <<= shift;
		count -= shift;
	}
	r->value = value;
	r->count = count;
	r->range = range;

#if CONFIG_BITSTREAM_DEBUG
	{
		const int queue_r = bitstream_queue_get_read();
		const int frame_idx = bitstream_queue_get_frame_read();
		int ref_result, ref_prob;
		bitstream_queue_pop(&ref_result, &ref_prob);
		if ((int)bit != ref_result) {
			fprintf( stderr,
				"\n *** [bit] result error, frame_idx_r %d bit %d ref_result %d "
				"queue_r %d\n",
				frame_idx, bit, ref_result, queue_r);
			assert(0);
		}
		if (prob != ref_prob) {
			fprintf( stderr,
				"\n *** [bit] prob error, frame_idx_r %d prob %d ref_prob %d "
				"queue_r %d\n",
				frame_idx, prob, ref_prob, queue_r);
			assert(0);
		}
	}
#endif

	return bit;
}

#endif  // VPXCODING_IMPLEMENTATION

#endif  // VPXCODING_READER

#ifdef VPXCODING_WRITER

#define VPX_NO_UNSIGNED_SHIFT_CHECK

typedef struct vpx_writer {
	unsigned int lowvalue;
	unsigned int range;
	int count;
	// Whether there has been an error.
	int error;
	// We maintain the invariant that pos <= size, i.e., we never write beyond
	// the end of the buffer. If pos would be incremented to be greater than
	// size, leave pos unchanged and set error to 1.
	unsigned int pos;
	unsigned int size;
	uint8_t *buffer;
} vpx_writer;


VPXCODING_DECORATOR void vpx_start_encode(vpx_writer *br, uint8_t *source, size_t size);
// Returns 0 on success and returns -1 in case of error.
VPXCODING_DECORATOR int vpx_stop_encode(vpx_writer *br);

static inline VPX_NO_UNSIGNED_SHIFT_CHECK void vpx_write(vpx_writer *br,
	int bit, int probability);

static inline void vpx_write_bit(vpx_writer *w, int bit) {
	vpx_write(w, bit, 128);  // vpx_prob_half
}

static inline void vpx_write_literal(vpx_writer *w, int data, int bits) {
	int bit;
	for (bit = bits - 1; bit >= 0; bit--) vpx_write_bit(w, 1 & (data >> bit));
}


static inline VPX_NO_UNSIGNED_SHIFT_CHECK void vpx_write(vpx_writer *br,
	int bit, int probability) {
	unsigned int split;
	int count = br->count;
	unsigned int range = br->range;
	unsigned int lowvalue = br->lowvalue;
	int shift;

#if CONFIG_BITSTREAM_DEBUG
	int queue_r = 0;
	int frame_idx_r = 0;
	int queue_w = bitstream_queue_get_write();
	int frame_idx_w = bitstream_queue_get_frame_write();
	if (frame_idx_w == frame_idx_r && queue_w == queue_r) {
		fprintf(stderr, "\n *** bitstream queue at frame_idx_w %d queue_w %d\n",
		frame_idx_w, queue_w);
		assert(0);
	}
	bitstream_queue_push(bit, probability);
#endif

	split = 1 + (((range - 1) * probability) >> 8);

	range = split;

	if (bit) {
		lowvalue += split;
		range = br->range - split;
	}

	shift = vpx_norm[range];

	range <<= shift;
	count += shift;

	if (count >= 0) {
		int offset = shift - count;

		if (!br->error) {
			if ((lowvalue << (offset - 1)) & 0x80000000) {
				int x = (int)br->pos - 1;

				while (x >= 0 && br->buffer[x] == 0xff) {
					br->buffer[x] = 0;
					x--;
				}

				// TODO(wtc): How to prove x >= 0?
				br->buffer[x] += 1;
			}

			if (br->pos < br->size) {
				br->buffer[br->pos++] = (lowvalue >> (24 - offset)) & 0xff;
			} else {
				br->error = 1;
			}
		}
		lowvalue <<= offset;
		shift = count;
		lowvalue &= 0xffffff;
		count -= 8;
	}

	lowvalue <<= shift;
	br->count = count;
	br->lowvalue = lowvalue;
	br->range = range;
}

#define vpx_write_prob(w, v) vpx_write_literal((w), (v), 8)

#ifdef VPXCODING_IMPLEMENTATION

VPXCODING_DECORATOR void vpx_start_encode(vpx_writer *br, uint8_t *source, size_t size)
{
	br->lowvalue = 0;
	br->range = 255;
	br->count = -24;
	br->error = 0;
	br->pos = 0;
	// Make sure it is safe to cast br->pos to int in vpx_write().
	if (size > INT_MAX) size = INT_MAX;
	br->size = (unsigned int)size;
	br->buffer = source;
	vpx_write_bit(br, 0);
}

// Returns 0 on success and returns -1 in case of error.
VPXCODING_DECORATOR int vpx_stop_encode(vpx_writer *br) {
	int i;

#if CONFIG_BITSTREAM_DEBUG
	bitstream_queue_set_skip_write(1);
#endif
	for (i = 0; i < 32; i++) vpx_write_bit(br, 0);

	// Ensure there's no ambigous collision with any index marker bytes
	if (!br->error && (br->buffer[br->pos - 1] & 0xe0) == 0xc0) {
	if (br->pos < br->size) {
			br->buffer[br->pos++] = 0;
		} else {
			br->error = 1;
		}
	}

#if CONFIG_BITSTREAM_DEBUG
	bitstream_queue_set_skip_write(0);
#endif

	return br->error ? -1 : 0;
}

#endif  // VPXCODING_IMPLEMENTATION

#endif // VPXCODING_WRITER

#ifdef __cplusplus
};
#endif

#endif

