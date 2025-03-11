// Be sure to check make.inc for more, basic settings.

#ifndef _BACOMMON_H
#define _BACOMMON_H

#include <stdint.h>
#include <stdbool.h>

#ifndef BA_CONFIG_ONLY
#include "gifenc.h"
#endif

// Doing MSE flattens out the glyph usage
// BUT by not MSE'ing it looks the same to me
// but it "should" be better. Depending on the context it may be RMSE, but it's always at least "mean squared error"
#define MSE

// Target glyphs, and how quickly to try approaching it.
//#define TARGET_GLYPH_COUNT 256 // defined in Makefile

// The following 3 are for tuning the K-Means front-end.
#define GLYPH_COUNT_REDUCE_PER_FRAME 8
// How many glpyhs to start at?
#define KMEANS 2048
// How long to train?
#define KMEANSITER 250

// Debugging (VPX only)
//#define MONITOR_FRAME 1065



// Completely comment out to disable tile inversion
// Tile inversion allows glyphs to be either positive or negative, and the huffman tree can choose which way to go.
// so theoretically you would need half the total tiles.
// XXX This DOES NOT WORK with later steps of the code, after finding glyph inversion was not the long pole in the tent.
//#define ALLOW_GLYPH_INVERSION

// Flip only implemented for COMPRESSION_UNIFIED_BY_BLOCK, overall found to be net loss.
// In almost all situations.
//#define ALLOW_GLYPH_FLIP

// DO NOT change this without code changes!
#ifndef BLOCKSIZE
#warning Your toolchain should set blocksize (in the Makefile).
#define BLOCKSIZE 8
#endif

// When doing output use VPX instead of huffman for length.
#define USE_VPX_LEN  1
#define UNIFIED_VPX  1


// vpxtestonly:  How do we encode the run lengths?
//  Use two-level run codes.  Provides about 0.5% better compression.
//#define RUNCODES_TWOLEVEL 1  // 84264 -> 84242
//Instead use the codes themself, along each step, predict likliehood of being a 1 or a 0.
// This is mutually exclusive with RUNCODES_TWOLEVEL
// TUNE THIS!
// WARNING: DO NOT SET THIS TO ABOVE 254 Otherwise it will overflow on the embedded decoder.
#define RUNCODES_CONTINUOUS 128  // 84264 -> 83795  [RECOMMEND ON]

#if defined( RUNCODES_TWOLEVEL ) && defined( RUNCODES_CONTINUOUS )
#error mutually exclusive settings
#endif

// For streamcomp + VPX Test, skip first frame after transition. (Some features only in vpxtest)
// How do we decide if we want to skip frames?
// This performs a massive space savings. 
// Enable all 3 for maximum savings.
//#define SMART_TRANSITION_SKIP 1 // Only available in vpx compressor. // Better savings, Better Quality But still drops some frames and looks chonkier.
//#define SKIP_ONLY_DOUBLE_UPDATES 1 // Middling savings, not good quality.
//#define SKIP_FIRST_AFTER_TRANSITION 1  // This removes the ability to represent a one cell.  Must be used in conjunction with SKIP_ONLY_DOUBLE_UPDATES - not as important if RUNCODES_CONTINUOUS is used.
// FYI for VPX, if you want, you can enable all 3.
// NOTE NOTE: If the stream comes pre dentropied, then you should NOT USE THESE (ok maybe SKIP_FIRST_AFTER_TRANSITION)
// NOTE NOTE NOTE: A number of people prefer (SKIP_ONLY_DOUBLE_UPDATES+SKIP_FIRST_AFTER_TRANSITION) instead of SMART_TRANSITION_SKIP

// Test to allow backtracking on VPX coding.  DEFAULT OFF
//#define VPX_CODING_ALLOW_BACKTRACK 1

// A more sophisticated algorithm for figuring out probabilities of glyph transitions.
// WARNING: This cannot be more than 16 because of the classificaiton output.
// DEEP QUESTION: Why is 15 sometimes a magic number here, regardless of number of tiles?
#define USE_TILE_CLASSES 13 // TUNE ME!!!
#define DEDICATE_TILE_CLASS01 1 // Only used for K-Means
#define EXPECTED_VALUE_GREEDY_OPTIMIZATION 1 // Disable k-means VPX, and instead use a greedy algorithm

#define VPX_GREY3 1
//#define VPX_GREY4 1
//#define VPX_GREY16 1 

// Force huffman in VPX for tiles instead for size comparison.
//#define VPX_USE_HUFFMAN_TILES 1

// apply a filter to the image.
//#define VPX_GORP_KERNEL_MOVE 1
//#define VPX_TAA 1


#if RUNCODES_CONTINUOUS && USE_TILE_CLASSES
// Optional (You canturn this off if you want) But make runtimes use classes.
#define RUNCODES_CONTINUOUS_BY_CLASS 1
// Optional. Make run lenght classes based on new glpyh not previous. NOTE: This should universally produce smaller outputs.
#define TILE_CLASSES_RUNCODE_FORWARD 1
#endif

// TUNE ME !!! 8 to 20
#define MAXPIXELRUNTOSTORE 9

// Don't do this unless you know you want it.
// This is mostly using to A/B test the encoding of non-1/0 symbols.
//#define PROB_ENDIAN_FLIP 1

// TUNE ME!!  This controls how likely less-than-most significant bits in pixels are to be set/unset.
#if defined( VPX_GREY3 )
//XXX TODO Tune for Grey3
#define GSC1 255
#define GSC0 212
#else
#define GSC1 6
#define GSC0 248
#endif


// Relies on RUNCODES_CONTINUOUS_BY_CLASS
#define INVERT_RUNCODE_COMPRESSION // Shouldn't really have an effect but sometimes does?

#if defined( INVERT_RUNCODE_COMPRESSION ) && !defined( RUNCODES_CONTINUOUS_BY_CLASS )
#error configuration invalid
#endif

#if defined( VPX_CODING_ALLOW_BACKTRACK ) && defined( INVERT_RUNCODE_COMPRESSION )
#error configuraiton invalid, VPX_CODING_ALLOW_BACKTRACK was invented before INVERT_RUNCODE_COMPRESSION.
#endif 

//////////////// HUFFMAN + K-MEANS ONLY BELOW THIS LINE /////////////////////

#define K_MEANS_HERO_FRAME  1688
#define K_MEANS_HERO_CELLX   3
#define K_MEANS_HERO_CELLY   2
#define K_MEANS_HERO_FRAME_START 1686
#define K_MEANS_HERO_FRAME_END   1702


// THINGS TO NOTE:

// try different stream compression schemes
//#define COMPRESSION_BLOCK_RASTER
//#define COMPRESSION_TWO_HUFF
//#define COMPRESSION_UNIFIED_BY_BLOCK_AND_TWO_HUFF // Mix of two huff and unified, by having both a unified, and block-only huff. >> Actually slightly larger because of overhead.
//THESE NEXT TWO Are interesting:
#define COMPRESSION_TWO_HUFF_TRY_2 // Two separate huffman trees, one for glyph, one for runlength.  DING DING DING<<<>>>
//#define COMPRESSION_UNIFIED_BY_BLOCK // Best (and most fleshed out)

// NOTE: ONLY AVAILABLE ON COMPRESSION_TWO_HUFF_TRY_2, Don't emit a second huffman table. Use Exponential-Golomb coding.
//#ifdef COMPRESSION_TWO_HUFF_TRY_2
//#define USE_UEGB // Exponential-Golomb coding  Not worth it
//#endif

// Try with blur on/off
// NOTE: To disable, comment out completely.
//#define BLUR_BASE 1.0

// To target learning halftones or target gradients.  Depends on HALFTONE_EN.
//#define ENCHALFTONE
//#define HALFTONE_EN  1


// If you want to try reducing FPS.
//#define FPS_REDUCTION 2

// If you want to try allowing encode history (only currently implemented on UNIFIED_BY_BLOCK emissions...
// Turns out it's worse.
//#define ENCODE_HISTORY 14

// Specifically generate a code for inverting color in huffman tree.
#ifdef ALLOW_GLYPH_INVERSION
#ifdef COMPRESSION_UNIFIED_BY_BLOCK //Only valid for this.
#define HUFFMAN_ALLOW_CODE_FOR_INVERT
#endif
#endif

// Speed up videocomp on appropriate systems.
#define ENABLE_SSE


//I stopped developing this route - cnl.
// I discovered this was acutally pretty bad. It's a huffman on the RLE of the glpyh data. - roughly 1.5x bigger
//#define GLYPH_COMPRESS_HUFFMAN_RUNS
// Instead try doing it by data. OK This is bad, too. This does it on each 8-bit line of the glyph. - roughly 1.25x bigger.
//#define GLYPH_COMPRESS_HUFFMAN_DATA

// Suppress tile updates in output image, unless they change nontrivially. (sets the score)
//#define REDUCE_MOTION_IN_OUTPUT_FOR_SIMILAR_IMAGES 	2

#ifdef ENCODE_HISTORY
#define ENCODE_HISTORY_FLAG 0x8000
#define ENCODE_HISTORY_MIN_FOR_SEL 200
#endif


#if defined( HUFFMAN_ALLOW_CODE_FOR_INVERT ) && ! defined( ALLOW_GLYPH_INVERSION )
#error Cant disable huffman invert and glyph inversion.
#endif

#if defined( ALLOW_GLYPH_INVERSION ) || defined( ALLOW_GLYPH_FLIP )
#define ALLOW_GLYPH_ATTRIBUTES
#endif


// Consider tweaking this and glyph_inversion_mask.
#define GLYPH_NOATTRIB_MASK 0x7ff

// Just FYI the top two bits are stored for huffman tree properties and "is it a glyph or RLE?"
#if TARGET_GLYPH_COUNT <= 2048
#define GLYPH_INVERSION_MASK 0x800
#else
#error cant do inversion mask
#endif
#define GLYPH_FLIP_X_MASK    0x2000
#define GLYPH_FLIP_Y_MASK    0x1000

#ifndef BA_CONFIG_ONLY



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
	uint64_t extra1_countbw;
	uint64_t extra2_countbw;
#elif BLOCKSIZE==16
	uint64_t extra1_countbw;
	uint64_t extra2_countbw;
	uint64_t extra3;
#endif
};





#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
int HandleDestroy() { return 0; }

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
#if HALFTONE_EN
			int evenodd = (ix+iy)&1;
	#if BLOCKSIZE==8
			if( ft > .4+evenodd*.2 ) ret |= 1ULL<<i;
	#else
			if( ft > .4+evenodd*.2 ) ret[i/64] |= 1ULL<<(i&63);
	#endif
#else
	#if BLOCKSIZE==8
			if( ft > .6 ) ret |= 1ULL<<i;
	#else
			if( ft > .6 ) ret[i/64] |= 1ULL<<(i&63);
	#endif
#endif
	}
	BBASSIGN( k->blockdata, ret );
}


static int H264FUNDeBruijnLog2( uint64_t v )
{
	// Note - if you are on a system with MSR or can compile to assembly, that is faster than this.
	// Otherwise, for normal C, this seems like a spicy way to roll.

	// Round v up!
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;

	static const int MultiplyDeBruijnBitPosition2[128] = 
	{
		0, // change to 1 if you want bitSize(0) = 1
		48, -1, -1, 31, -1, 15, 51, -1, 63, 5, -1, -1, -1, 19, -1, 
		23, 28, -1, -1, -1, 40, 36, 46, -1, 13, -1, -1, -1, 34, -1, 58,
		-1, 60, 2, 43, 55, -1, -1, -1, 50, 62, 4, -1, 18, 27, -1, 39, 
		45, -1, -1, 33, 57, -1, 1, 54, -1, 49, -1, 17, -1, -1, 32, -1,
		53, -1, 16, -1, -1, 52, -1, -1, -1, 64, 6, 7, 8, -1, 9, -1, 
		-1, -1, 20, 10, -1, -1, 24, -1, 29, -1, -1, 21, -1, 11, -1, -1,
		41, -1, 25, 37, -1, 47, -1, 30, 14, -1, -1, -1, -1, 22, -1, -1,
		35, 12, -1, -1, -1, 59, 42, -1, -1, 61, 3, 26, 38, 44, -1, 56
	};
	return MultiplyDeBruijnBitPosition2[(uint64_t)(v * 0x6c04f118e9966f6bUL) >> 57];
}

#endif

#endif


