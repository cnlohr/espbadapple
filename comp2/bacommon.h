#ifndef _BACOMMON_H
#define _BACOMMON_H

#include <stdint.h>
#include <stdbool.h>


#include "gifenc.h"

// Doing MSE flattens out the glyph usage
// BUT by not MSE'ing it looks the same to me
// but it "should" be better 
//#define MSE

// Target glyphs, and how quickly to try approaching it.
#define TARGET_GLYPH_COUNT 256
#define GLYPH_COUNT_REDUCE_PER_FRAME 10
// How many glpyhs to start at?
#define KMEANS 2048
// How long to train?
#define KMEANSITER 220

// Completely comment out to disable tile inversion
// Tile inversion allows glyphs to be either positive or negative, and the huffman tree can choose which way to go.
// so theoretically you would need half the total tiles.
#define ALLOW_GLYPH_INVERSION
// Flip only implemented for COMPRESSION_UNIFIED_BY_BLOCK, overall found to be net loss.
// In almost all situations.
//#define ALLOW_GLYPH_FLIP

// DO NOT change this without code changes!
#ifndef BLOCKSIZE
#warning Your toolchain should set blocksize (in the Makefile).
#define BLOCKSIZE 8
#endif

#define HALFTONE_EN  1

// THINGS TO NOTE:

// try different stream compression schemes
//#define COMPRESSION_BLOCK_RASTER
//#define COMPRESSION_TWO_HUFF
//#define COMPRESSION_UNIFIED_BY_BLOCK_AND_TWO_HUFF // Mix of two huff and unified, by having both a unified, and block-only huff. >> Actually slightly larger because of overhead.
#define COMPRESSION_UNIFIED_BY_BLOCK // Best (and most fleshed out)

// Try with blur on/off
// NOTE: To disable, comment out completely.
//#define BLUR_BASE 1.0

// To target learning halftones or target gradients.  Depends on HALFTONE_EN.
#define ENCHALFTONE

// If you want to try reducing FPS.
//#define FPS_REDUCTION 2

// If you want to try allowing encode history (only currently implemented on UNIFIED_BY_BLOCK emissions...
// Turns out it's worse.
//#define ENCODE_HISTORY 14

// Specifically generate a code for inverting color in huffman tree.
#define HUFFMAN_ALLOW_CODE_FOR_INVERT

// Speed up videocomp on appropriate systems.
#define ENABLE_SSE





#ifdef ENCODE_HISTORY
#define ENCODE_HISTORY_FLAG 0x8000
#define ENCODE_HISTORY_MIN_FOR_SEL 200
#endif


#if defined( HUFFMAN_ALLOW_CODE_FOR_INVERT ) && ! defined( ALLOW_GLYPH_INVERSION )
#error Can't disable huffman invert and glyph inversion.
#endif

#if defined( ALLOW_GLYPH_INVERSION ) || defined( ALLOW_GLYPH_FLIP )
#define ALLOW_GLYPH_ATTRIBUTES
#endif


#if defined( ALLOW_GLYPH_ATTRIBUTES )
#define GLYPH_NOATTRIB_MASK 0x7ff
#else
#define GLYPH_NOATTRIB_MASK 0x3fff
#define GLYPH_NOATTRIB_MASK ((uint32_t)-1)
#endif

// Just FYI the top two bits are stored for huffman tree properties and "is it a glyph or RLE?"
#define GLYPH_INVERSION_MASK 0x2000
#define GLYPH_FLIP_X_MASK    0x1000
#define GLYPH_FLIP_Y_MASK    0x0800


#if BLOCKSIZE==8
typedef uint64_t blocktype;
#define BBASSIGN( to, from ) to = from
#elif BLOCKSIZE==16
typedef uint64_t blocktype[4];
#define BBASSIGN( to, from ) memcpy( to, from, sizeof(blocktype) )
#else
#error UNSUPPORTED BLOCKSIZE
#endif

struct block
{
	float intensity[BLOCKSIZE*BLOCKSIZE]; // For when we start culling blocks.
	blocktype blockdata;
	uint32_t count;
	uint32_t scratch;
#if BLOCKSIZE==8
	uint64_t extra1;
	uint64_t extra2;
#elif BLOCKSIZE==16
	uint64_t extra1;
	uint64_t extra2;
	uint64_t extra3;
#endif
};





#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

void UpdateBlockDataFromIntensity( struct block * k );
void DrawBlockBasic( int xofs, int yofs, blocktype bb, int original_glyph_id  );
void DrawBlock( int xofs, int yofs, struct block * bb, int boolean, int original_glyph_id  );
void * alignedcalloc( size_t size, uint32_t align_bits, void ** freeptr );


void DrawBlock( int xofs, int yofs, struct block * bb, int boolean, int original_glyph_id )
{
	uint32_t boo[BLOCKSIZE*BLOCKSIZE] = { 0 };
	int i;
	if( boolean )
	{
		blocktype b;
		BBASSIGN( b, bb->blockdata );

#if BLOCKSIZE==8
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			if( b & (1ULL<<i) )
				boo[i] = 0xffffffff;
			else
				boo[i] = 0xff000000;
		}
#else
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			if( b[i/64] & (1ULL<<(i&63)) )
				boo[i] = 0xffffffff;
			else
				boo[i] = 0xff000000;
		}
#endif
	}
	else
	{
		float * fi = bb->intensity;
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			float f = fi[i];
			int c = f * 255;
			if( c > 255 ) c = 255;
			if( c < 0 ) c = 0;
			boo[i] = (c) | (c<<8) | (c<<16) | (0xff000000);
		}
	}

	uint32_t bobig[BLOCKSIZE*BLOCKSIZE*4];
	int x, y;

#ifdef ALLOW_GLYPH_INVERSION
	uint32_t invertmask = (original_glyph_id&GLYPH_INVERSION_MASK)?0xffffff:0x000000;
#else
	uint32_t invertmask = 0;
#endif
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		int iy = ( original_glyph_id & GLYPH_FLIP_Y_MASK ) ? (BLOCKSIZE - 1 - y) : y;
		int ix = ( original_glyph_id & GLYPH_FLIP_X_MASK ) ? (BLOCKSIZE - 1 - x) : x;
		uint32_t v = boo[ix+iy*BLOCKSIZE] ^ invertmask;
		bobig[(2*x+0) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+0) + (2*y+1)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+1)*BLOCKSIZE*2] = v;
	}
	CNFGBlitImage( bobig, xofs, yofs, BLOCKSIZE*2, BLOCKSIZE*2 );
}

void DrawBlockGif( ge_GIF * gif, int xofs, int yofs, int vw, struct block * bb, int original_glyph_id )
{

	char boo[BLOCKSIZE*BLOCKSIZE] = { 0 };
	int i;

	{
		blocktype b;
		BBASSIGN( b, bb->blockdata );
#if BLOCKSIZE==8
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			if( b & (1ULL<<i) )
				boo[i] = 1;
			else
				boo[i] = 0;
		}
#else
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			if( b[i/64] & (1ULL<<(i&63)) )
				boo[i] = 1;
			else
				boo[i] = 0;
		}
#endif
	}

//	uint32_t bobig[BLOCKSIZE*BLOCKSIZE*4];
	int x, y;
#ifdef ALLOW_GLYPH_INVERSION
	uint32_t invertmask = (original_glyph_id&GLYPH_INVERSION_MASK)?0x1:0x0;
#else
	uint32_t invertmask = 0;
#endif

	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		int iy = ( original_glyph_id & GLYPH_FLIP_Y_MASK ) ? (BLOCKSIZE - 1 - y) : y;
		int ix = ( original_glyph_id & GLYPH_FLIP_X_MASK ) ? (BLOCKSIZE - 1 - x) : x;
		char b = boo[ix+iy*BLOCKSIZE] ^ invertmask;
		uint8_t * v = gif->frame;
		v[(2*x+0+xofs) + (2*y+0 + yofs)*vw] = b;
		v[(2*x+1+xofs) + (2*y+0 + yofs)*vw] = b;
		v[(2*x+0+xofs) + (2*y+1 + yofs)*vw] = b;
		v[(2*x+1+xofs) + (2*y+1 + yofs)*vw] = b;
	}
}



void DrawBlockBasic( int xofs, int yofs, blocktype bb, int original_glyph_id )
{
	struct block b;
	BBASSIGN( b.blockdata, bb );
	DrawBlock( xofs, yofs, &b, true, original_glyph_id );
}



void BlockUpdateGif( ge_GIF * gif, int xofs, int yofs, int vw, blocktype bb, int original_glyph_id )
{
	struct block b;
	BBASSIGN( b.blockdata, bb );
	DrawBlockGif( gif, xofs, yofs, vw, &b, original_glyph_id );
}


void * alignedcalloc( size_t size, uint32_t align_bits, void ** freeptr )
{
	uintptr_t alignment_mask = (1ULL<<align_bits)-1;
	uintptr_t ret = (uintptr_t)calloc( size + alignment_mask, 1 );
	*freeptr = (void*)ret;
	ret = (ret + alignment_mask) & ~alignment_mask;
	return (void*)ret;
}


void UpdateBlockDataFromIntensity( struct block * k )
{
	int i;


#if BLOCKSIZE==8
	blocktype ret = 0;
#else
	blocktype ret = { 0 };
#endif
	for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
	{
		int ix = i % BLOCKSIZE;
		int iy = i / BLOCKSIZE;
		float ft = k->intensity[i];
#ifndef HALFTONE_EN
	#if BLOCKSIZE==8
			if( ft > .6 ) ret |= 1ULL<<i;
	#else
			if( ft > .6 ) ret[bpl/64] |= 1ULL<<(i&63);
	#endif

#else
			int evenodd = (ix+iy)&1;
	#if BLOCKSIZE==8
			if( ft > .3+evenodd*.4 ) ret |= 1ULL<<i;
	#else
			if( ft > .3+evenodd*.4 ) ret[i/64] |= 1ULL<<(i&63);
	#endif
#endif
	}
	BBASSIGN( k->blockdata, ret );
}


#endif


