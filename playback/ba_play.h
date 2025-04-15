#ifndef _BA_PLAY_H
#define _BA_PLAY_H

#include <stdint.h>

#define VPXCODING_READER
#define VPX_32BIT

#define VPXCODING_AUTOGEN_NORM
//#include "vpxcoding.h"
#include "vpxcoding_tinyread.h"

#define BA_CONFIG_ONLY
#include "bacommon.h"

#ifndef BADATA_DECORATOR
#define BADATA_DECORATOR const static
#endif
#include "badapple_data.h"

#if defined( VPX_GREY4 ) || defined( VPX_GREY3 )
	#define BITSETS_TILECOMP 2
#elif defined( VPX_GREY16 )
	#define BITSETS_TILECOMP 4
#else
	#define BITSETS_TILECOMP 1
#endif

#define DBLOCKSIZE (BLOCKSIZE*BLOCKSIZE*BITSETS_TILECOMP/8)

#ifndef CHECKPOINT
#define CHECKPOINT(x...)
#endif

#ifndef CHECKBITS_VIDEO
#define CHECKBITS_VIDEO(x...)
#endif

#if TILE_COUNT > 256
typedef uint16_t glyphtype;
#else
typedef uint8_t glyphtype;
#endif

#if BLOCKSIZE > 8
#define GRAPHICSIZE_WORDS 4
typedef uint32_t graphictype;
#else
#define GRAPHICSIZE_WORDS 2
typedef uint16_t graphictype;
#endif


typedef struct ba_play_context_
{
	vpx_reader vpx_changes;
	glyphtype  curmap[BLKY*BLKX];
	uint8_t    currun[BLKY*BLKX];  // Not actually current run, only used for VPX change probability calculation.  Limited to RUNCODES_CONTINUOUS.
	graphictype   glyphdata[TILE_COUNT][DBLOCKSIZE/GRAPHICSIZE_WORDS];
	//graphictype   framebuffer[RESX*RESY*BITSETS_TILECOMP/(GRAPHICSIZE_WORDS*2)];
} ba_play_context;

static int ba_play_setup( ba_play_context * ctx )
{

	// Load glyphs into RAM.
	{
#ifdef _VPX_TINYREAD_SFH_H
		vpx_reader_init( &ctx->vpx_changes, ba_glyphdata, sizeof(ba_glyphdata) );
#else
		vpx_reader_init( &ctx->vpx_changes, ba_glyphdata, sizeof(ba_glyphdata), 0, 0 );
#endif
		int g = 0;
		int runsofar = 0;
		int is0or1 = 0;
		CHECKPOINT( decodephase = "Reading Glyphs", decoding_glyphs = 1 );
		//graphictype * gd = &ctx->glyphdata[0];
		//int cpixel = 0;
		//int writeg = 0;
		for( g = 0; g < TILE_COUNT*BLOCKSIZE*BLOCKSIZE; g+=1 )
		{
			int tprob = ba_vpx_glyph_probability_run_0_or_1[is0or1][runsofar];
			if( ((g) & (BLOCKSIZE*BLOCKSIZE-1)) == 0 )
			{
				tprob = 128;
				runsofar = MAXPIXELRUNTOSTORE-1;
			}
			int subpixel;
			int gid = ((g/BLOCKSIZE/BLOCKSIZE)); 
			int lx = (g&(BLOCKSIZE-1))%BLOCKSIZE;
			int ly = ((g/BLOCKSIZE)&(BLOCKSIZE-1))%BLOCKSIZE;
			for( subpixel = 0; subpixel < BITSETS_TILECOMP; subpixel++ )
			{
				int lb = vpx_read( &ctx->vpx_changes, tprob );
				CHECKPOINT( decodeglyph = g, decode_is0or1 = is0or1, decode_runsofar = runsofar, decode_prob = tprob, decode_lb = lb, vpxcheck = 0 );
				if( subpixel == 0 )
				{
					if( lb != is0or1 )
					{
						is0or1 = lb;
						runsofar = 0;
					}
					else if( runsofar < MAXPIXELRUNTOSTORE-1 )
					{
						runsofar++;
					}
					tprob = lb?GSC1:GSC0;
				}
				ctx->glyphdata[gid][lx] |= lb<<(ly+BLOCKSIZE*(BITSETS_TILECOMP-1)-subpixel*BLOCKSIZE); // Yuck.. bit ordering here is weird.
			}
		}
	}

	CHECKPOINT(decodephase = "Initializing Cells" );

	int n;
	for( n = 0; n < BLKX*BLKY; n++ )
	{
		ctx->curmap[n] = INITIALIZE_CELL;
	}
	memset( ctx->currun, 0, sizeof( ctx->currun ) );
	//memset( ctx->framebuffer, 0, sizeof(ctx->framebuffer) );
	
	vpx_reader * rctx = &ctx->vpx_changes;
#ifdef _VPX_TINYREAD_SFH_H
	vpx_reader_init( rctx, ba_video_payload, sizeof(ba_video_payload) );
#else
	vpx_reader_init( rctx, ba_video_payload, sizeof(ba_video_payload), 0, 0 );
#endif

	CHECKPOINT(decodephase = "Initialize Complete", decoding_glyphs = 0 );

	return 0;
}

static int ba_play_frame( ba_play_context * ctx )
{
#ifndef INVERT_RUNCODE_COMPRESSION
#error This decoder is only written for inverted runcode compression
#endif

	vpx_reader * rctx = &ctx->vpx_changes;

	const uint8_t * byteplace_at_start = rctx->buffer;
	int count_at_start = rctx->count;

	int bx = 0, by = 0;
	int n;
	//CHECKPOINT(decodephase = "Advancing Frame");

	for( n = 0; n < BLKY*BLKX; n++ )
	{
		// Need to pull off a transition pair.
		int fromglyph = ctx->curmap[n];
		int run = ctx->currun[n];
#ifdef USE_TILE_CLASSES
#if USE_TILE_CLASSES > 16
		int fromclass = (ba_exportbinclass[fromglyph]);
#else
		int fromclass = (ba_exportbinclass[fromglyph>>1]>>((fromglyph&1)<<2))&0xf;
#endif
		int probability = ba_vpx_probs_by_tile_run_continuous[fromclass][run];
#else
		int probability = ba_vpx_probs_by_tile_run_continuous[run];
#endif
		int level;
		int tile;

		int lb = vpx_read( rctx, probability );
		CHECKPOINT( decode_cellid = n, decode_lb = lb, vpxcheck = 0, decode_class = fromclass, decode_fromglyph = fromglyph, decode_run = run, decode_prob = probability, decodephase = lb?"Running":"Run Stopped" );

		if( lb == 1 )
		{
			//CHECKPOINT( decode_lb = 1, decodephase = "Continue", vpxcheck = 0 );
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
				CHECKPOINT( decode_tileid = tile, decode_lb = bit, vpxcheck = 0, decode_level = level, decode_prob = probability, decodephase = "Decoding Bit" );

				if( bit )
					probplace += 1<<ppo;
				else
					probplace++;
				ppo--;
			}

			ctx->curmap[n] = tile;
			ctx->currun[n] = 0;
			CHECKPOINT( decodephase = "Committing Tile" );

			// Update framebuffer.
#if 0
			graphictype * fbo = (graphictype*)&ctx->framebuffer[(bx*BLOCKSIZE + by*BLOCKSIZE*RESX)*BITSETS_TILECOMP/(GRAPHICSIZE_WORDS*8)];
			//printf( "%d / %d\n", (bx + by*BLOCKSIZE)*BLOCKSIZE*BITSETS_TILECOMP/32, sizeof( ctx->framebuffer ) / 4 );
			int ly;
			for( ly = 0; ly < BLOCKSIZE; ly++ )
			{
				fbo[0] = *(glyph++);
				fbo += RESX * BITSETS_TILECOMP / (GRAPHICSIZE_WORDS*8);
			}
#endif
		}

		bx++;
		if( bx >= BLKX )
		{
			bx = 0;
			by++;
		}
	}

	const uint8_t * byteplace_at_end = rctx->buffer;
	int count_at_end = rctx->count;
	int gpused = count_at_start - count_at_end;
	if( gpused < 0 ) gpused = 0;
	int bitused = (byteplace_at_end-byteplace_at_start)*8 + gpused;
	CHECKBITS_VIDEO( bitused );
	return 0;
}


#endif


