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
#define KMEANS 1024
// How long to train?
#define KMEANSITER 100

// DO NOT change this without code changes!
#ifndef BLOCKSIZE
#warning Your toolchain should set blocksize.
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

void DrawBlockBasic( int xofs, int yofs, blocktype bb );
void DrawBlock( int xofs, int yofs, struct block * bb, int boolean );
void * alignedcalloc( size_t size, uint32_t align_bits, void ** freeptr );


void DrawBlock( int xofs, int yofs, struct block * bb, int boolean )
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
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		uint32_t v = boo[x+y*BLOCKSIZE];
		bobig[(2*x+0) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+0) + (2*y+1)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+1)*BLOCKSIZE*2] = v;
	}
	CNFGBlitImage( bobig, xofs, yofs, BLOCKSIZE*2, BLOCKSIZE*2 );
}

void DrawBlockGif( ge_GIF * gif, int xofs, int yofs, int vw, struct block * bb )
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
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		char b = boo[x+y*BLOCKSIZE];
		uint8_t * v = gif->frame;
		v[(2*x+0+xofs) + (2*y+0 + yofs)*vw] = b;
		v[(2*x+1+xofs) + (2*y+0 + yofs)*vw] = b;
		v[(2*x+0+xofs) + (2*y+1 + yofs)*vw] = b;
		v[(2*x+1+xofs) + (2*y+1 + yofs)*vw] = b;
	}
}



void DrawBlockBasic( int xofs, int yofs, blocktype bb )
{
	struct block b;
	BBASSIGN( b.blockdata, bb );
	DrawBlock( xofs, yofs, &b, true );
}



void BlockUpdateGif( ge_GIF * gif, int xofs, int yofs, int vw, blocktype bb )
{
	struct block b;
	BBASSIGN( b.blockdata, bb );
	DrawBlockGif( gif, xofs, yofs, vw, &b );
}


void * alignedcalloc( size_t size, uint32_t align_bits, void ** freeptr )
{
	uintptr_t alignment_mask = (1ULL<<align_bits)-1;
	uintptr_t ret = (uintptr_t)calloc( size + alignment_mask, 1 );
	*freeptr = (void*)ret;
	ret = (ret + alignment_mask) & ~alignment_mask;
	return (void*)ret;
}

#endif


