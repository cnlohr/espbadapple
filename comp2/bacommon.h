#ifndef _BACOMMON_H
#define _BACOMMON_H
// Doing MSE flattens out the glyph usage
// BUT by not MSE'ing it looks the same to me
// but it "should" be better 
//#define MSE

// Target glyphs, and how quickly to try approaching it.
#define TARGET_GLYPH_COUNT 256
#define GLYPH_COUNT_REDUCE_PER_FRAME 5
// How many glpyhs to start at?
#define KMEANS 4096
// How long to train?
#define KMEANSITER 512

// DO NOT change this without code changes!
#define BLOCKSIZE 8
#define HALFTONE  1
typedef uint64_t blocktype;

struct block
{
	float intensity[BLOCKSIZE*BLOCKSIZE]; // For when we start culling blocks.
	blocktype blockdata;
	uint32_t count;
	uint32_t scratch;
	uint64_t extra1;
	uint64_t extra2;
};


#endif

