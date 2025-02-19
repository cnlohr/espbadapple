#ifndef _BA_PLAY_H
#define _BA_PLAY_H

#include <stdint.h>

#define VPXCODING_READER
#define VPXCODING_CUSTOM_VPXNORM
#define VPX_32BIT
extern uint8_t vpx_norm[256];
#include "vpxcoding.h"

#define BA_CONFIG_ONLY
#include "bacommon.h"

#define BADATA_DECORATOR const static
#include "badapple_data.h"

#if defined( VPX_GREY4 )
	#define BITSETS_TILECOMP 2
#elif defined( VPX_GREY16 )
	#define BITSETS_TILECOMP 4
#else
	#define BITSETS_TILECOMP 1
#endif

#define DBLOCKSIZE (BLOCKSIZE*BLOCKSIZE*BITSETS_TILECOMP/8)

uint8_t vpx_norm[256];

#if TILE_COUNT > 256
typedef uint16_t glyphtype;
#else
typedef uint8_t glyphtype;
#endif

typedef struct ba_play_context_
{
	vpx_reader vpx_changes;
	glyphtype  curmap[BLKY*BLKX];
	uint8_t    currun[BLKY*BLKX];
	uint16_t   glyphdata[TILE_COUNT][DBLOCKSIZE/2];
	uint16_t   framebuffer[RESX*RESY*BITSETS_TILECOMP/16];
} ba_play_context;

int ba_play_setup( ba_play_context * ctx )
{
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

	// Load glyphs into RAM.
	{
		vpx_reader r;
		vpx_reader_init( &r, ba_glyphdata, sizeof(ba_glyphdata), 0, 0 );
		int g, p;
		int runsofar = 0;
		int is0or1 = 0;
		int crun = 0;
		uint16_t * gd = &ctx->glyphdata[0][0];
		int cpixel = 0;
		int writeg = 0;
		for( g = 0; g < TILE_COUNT*BLOCKSIZE*BLOCKSIZE; g+=1 )
		{
			int tprob = ba_vpx_glyph_probability_run_0_or_1[is0or1][runsofar];
			if( ((g) & (BLOCKSIZE*BLOCKSIZE-1)) == 0 )
			{
				tprob = 128;
				runsofar = MAXPIXELRUNTOSTORE-1;
			}
			int color = vpx_read( &r, tprob );
			cpixel |=  color<<(++writeg);

			int subpixel;
			for( subpixel = 1; subpixel < BITSETS_TILECOMP; subpixel++ )
			{
				int lb = vpx_read( &r, color?GSC1:GSC0 );
				cpixel |= lb<<(writeg-1); // Yuck.. bit ordering here is weird.
				writeg++;
			}

			if( (g & ((16/BITSETS_TILECOMP)-1)) == (((16/BITSETS_TILECOMP)-1)) )
			{
				*(gd++) = cpixel;
				writeg = cpixel = 0;
			}

			if( color != is0or1 )
			{
				is0or1 = color;
				runsofar = 0;
			}
			else if( runsofar < MAXPIXELRUNTOSTORE-1 )
			{
				runsofar++;
			}

		}
	}

	int n;
	for( n = 0; n < BLKX*BLKY; n++ )
	{
		ctx->curmap[n] = INITIALIZE_CELL;
	}
	memset( ctx->currun, 0, sizeof( ctx->currun ) );
	memset( ctx->framebuffer, 0, sizeof(ctx->framebuffer) );
	
	vpx_reader * rctx = &ctx->vpx_changes;
	vpx_reader_init( rctx, ba_video_payload, sizeof(ba_video_payload), 0, 0 );
}

int ba_play_frame( ba_play_context * ctx )
{
#ifndef INVERT_RUNCODE_COMPRESSION
#error This decoder is only written for inverted runcode compression
#endif

	vpx_reader * rctx = &ctx->vpx_changes;

	int bx = 0, by = 0;
	int n;

static int kk;

	for( n = 0; n < BLKY*BLKX; n++ )
	{
		// Need to pull off a transition pair.
		int fromglyph = ctx->curmap[n];
		int run = ctx->currun[n];
#ifdef USE_TILE_CLASSES
		int fromclass = (ba_exportbinclass[fromglyph>>1]>>((fromglyph&1)<<2))&0xf;
		int probability = ba_vpx_probs_by_tile_run_continuous[fromclass][run];
#else
		int probability = ba_vpx_probs_by_tile_run_continuous[run];
#endif
		int level;
		int tile;

		if( vpx_read( rctx, probability ) == 1 )
		{
			// keep going
			run = run + 1;
			if( run > RUNCODES_CONTINUOUS-1 )
				run = RUNCODES_CONTINUOUS-1;
			ctx->currun[n] = run;
		}
		else
		{
			// Have a new thing.
			int probplace = 0;
			tile = 0;
			int ppo = (BITS_FOR_TILE_ID-1);
			for( level = 0; level < BITS_FOR_TILE_ID; level++ )
			{
#ifdef USE_TILE_CLASSES
				probability = ba_chancetable_glyph_dual[fromclass][probplace];
#else
				probability = ba_chancetable_glyph[probplace];
#endif
				int bit = vpx_read( rctx, probability );
				tile |= bit<<ppo;

				if( bit )
					probplace += 1<<ppo;
				else
					probplace++;
				ppo--;
			}
			ctx->curmap[n] = tile;
			ctx->currun[n] = 0;

			// Update framebuffer.
			uint16_t * glyph = (uint16_t*)ctx->glyphdata[tile];
			uint16_t * fbo = (uint16_t*)&ctx->framebuffer[(bx*BLOCKSIZE + by*BLOCKSIZE*RESX)*BITSETS_TILECOMP/16];
			//printf( "%d / %d\n", (bx + by*BLOCKSIZE)*BLOCKSIZE*BITSETS_TILECOMP/32, sizeof( ctx->framebuffer ) / 4 );
			int lx, ly;
			for( ly = 0; ly < BLOCKSIZE; ly++ )
			{
				fbo[0] = *(glyph++);
				fbo += RESX * BITSETS_TILECOMP / 16;
			}
		}

		bx++;
		if( bx >= BLKX )
		{
			bx = 0;
			by++;
		}
	}
	return 0;
}


#endif


