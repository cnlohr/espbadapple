#include <stdio.h>

#define VPXCODING_WRITER
#define VPXCODING_READER
#include "vpxcoding.h"

#define CNRBTREE_IMPLEMENTATION
#include "cnrbtree.h"

#include "../comp2/bacommon.h"

#include <assert.h>


typedef uint64_t u64;
typedef uint32_t u32;

// Generate a u32/u32 multimap.
CNRBTREETEMPLATE( u32, u32, RBptrcmpnomatch, RBptrcpy, RBnullop );

int * tiles = 0;
int numtiles = 0;

#define BLKX (RESX/BLOCKSIZE)
#define BLKY (RESY/BLOCKSIZE)

#define MAXTILEDIFF 524288
int tilechanges[MAXTILEDIFF]; // Change to this
int tileruns[MAXTILEDIFF];    // After changing, run for this long.
int tilechangect;

int maxtileid_remapped = 0;
uint8_t bufferVPX[1024*1024];
uint8_t bufferVPXg[1024*1024];
uint8_t bufferVPXr[1024*1024];

int intlog2 (int x){return __builtin_ctz (x);}

float * glyphsold;
int glyphctold;
float * glyphsnew;


void VPDrawGlyph( int xofs, int yofs, int glyph )
{
	uint32_t bobig[BLOCKSIZE*2*BLOCKSIZE*2];
	int bit = 0;
	int x, y;
	if( glyph >= maxtileid_remapped )
	{
		fprintf( stderr, "Glyph %d/%d\n", glyph, maxtileid_remapped );
		exit( -9 );
	}
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		uint32_t v = (glyphsnew[x+y*BLOCKSIZE+(BLOCKSIZE*BLOCKSIZE)*glyph] > 0.5) ? 0xffffffff : 0xff000000;
		bobig[(2*x+0) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+0) + (2*y+1)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+1)*BLOCKSIZE*2] = v;
	}
	CNFGBlitImage( bobig, xofs, yofs, BLOCKSIZE*2, BLOCKSIZE*2 );
}

int main()
{
	CNFGSetup( "comp test", 1800, 1060 );
	int i;
	int maxtileid = 0; // pre-remapped

	FILE * f = fopen( "../comp2/stream-64x48x8.dat", "rb" );
	while( !feof( f ) )
	{
		uint32_t tile;
		if( !fread( &tile, 1, 4, f ) )
		{
			if( !feof( f ) ) fprintf( stderr, "Error reading stream\n" );
			break;
		}
		tiles = realloc( tiles, (numtiles+1)*4 );
		tiles[numtiles] = tile;
		numtiles++;
		if( tile > maxtileid ) maxtileid = tile;
	}
	fclose( f );

	FILE * fT = fopen( "../comp2/tiles-64x48x8.dat", "rb" );
	while( !feof( fT ) )
	{
		glyphsold = realloc( glyphsold, (glyphctold+1)*BLOCKSIZE*BLOCKSIZE*4 );
		if( !fread( &glyphsold[glyphctold*BLOCKSIZE*BLOCKSIZE], 1, BLOCKSIZE*BLOCKSIZE*4, fT ) )
		{
			if( !feof( fT ) ) fprintf( stderr, "Error reading glyph\n" );
			break;
		}
		glyphctold++;
	}
	fclose( fT );

	printf( "Num tiles: %d\n", numtiles);
	printf( "Num glyphs: %d\n", glyphctold);
	int * tilecounts = calloc( 4, maxtileid );   // [newid]
	int * tileremap = calloc( 4, maxtileid );    // [newid] = originalid
	int * tileremapfwd = calloc( 4, maxtileid ); // [originalid] = newid

	int maxrun = 0;
	int frames = numtiles / BLKX / BLKY;

	{
		int * tilecounts_temp = alloca( 4 * maxtileid );
		memset( tilecounts_temp, 0, 4 * maxtileid );

		int tilechangerun[BLKY][BLKX] = { 0 };
		int x, y, frame;
		for( frame = 0; frame < frames; frame++ )
		{
			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				int tcr = tilechangerun[y][x];
				if( tcr == 0 )
				{
					int t = tiles[(x+y*(BLKX)) + BLKX*BLKY * (frame + 0)];
					int forward;
					for( forward = 1; frame + forward < frames; forward++ )
					{
						if( tiles[(x+y*(BLKX)) + BLKX*BLKY * (frame + forward)] != t )
							break;
					}
#ifndef SKIP_FIRST_AFTER_TRANSITION
					forward--;
#endif
					// Our product:
					//int tilechanges[MAXTILEDIFF];
					//int tileruns[MAXTILEDIFF];
					// based on
					//int tilechanges;
					
					tilechanges[tilechangect] = t;
					tileruns[tilechangect] = forward;
					tilechangect++;

					// For knowing when to pull a new cell.
					tilechangerun[y][x] = forward;

					// For frequency monitoring.
					tilecounts_temp[t]++;
				}
				else
				{
					tilechangerun[y][x]--;
				}
			}
		}

		cnrbtree_u32u32 * countmap = cnrbtree_u32u32_create();

		for( i = 0; i < maxtileid; i++ )
		{
			RBA( countmap, tilecounts_temp[i] ) = i;
		}

		int n = maxtileid-1;
		RBFOREACH( u32u32, countmap, i )
		{
			tileremap[n] = i->data;
			tilecounts[n] = i->key;
			tileremapfwd[i->data] = n;
			n--;
		}
		maxtileid_remapped = maxtileid - n;

		for( i = 0; i < tilechangect; i++ )
		{
			tilechanges[i] = tileremapfwd[tilechanges[i]];
		}

		glyphsnew = malloc( (maxtileid_remapped)*BLOCKSIZE*BLOCKSIZE*4 );
		for( i = 0; i < maxtileid_remapped; i++ )
		{
			int oldid = tileremap[i];
			memcpy( &glyphsnew[BLOCKSIZE*BLOCKSIZE*i],
				&glyphsold[BLOCKSIZE*BLOCKSIZE*oldid], BLOCKSIZE*BLOCKSIZE*4 );
		}
	}

	printf( "Changes: %d\n", tilechangect );

	// We can decode entirely based on tilechanges, tileruns, for tilechanges.
	//int tilechanges[MAXTILEDIFF]; // Change to this
	//int tileruns[MAXTILEDIFF];    // After changing, run for this long.
	// based on int tilechangect;


	// test validate
	if( 0 )
	{
		CNFGClearFrame();
		int curglyph[BLKY][BLKX] = { 0 };
		int currun[BLKY][BLKX] = { 0 };
		int playptr = 0;
		int frame;
		int x, y;
		for( frame = 0; frame < frames; frame++ )
		{
			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				VPDrawGlyph( x*BLOCKSIZE*2, y*BLOCKSIZE*2, curglyph[y][x] );
				if( currun[y][x] == 0 )
				{
					curglyph[y][x] = tilechanges[playptr];
					currun[y][x] = tileruns[playptr];
					playptr++;
				}
				else
				{
					currun[y][x]--;
				}
			}
			CNFGSwapBuffers();
			usleep(10000);
		}
	}

	// Now, we have to compress tilechanges & tileruns
	uint8_t vpx_probs_by_tile_run[maxtileid_remapped];
	{
		int probcountmap[maxtileid_remapped];
		int glyphcounts[maxtileid_remapped];
		int curtile[BLKY][BLKX];
		int n, bx, by;

		for( n = 0; n < maxtileid_remapped; n++ )
		{
			probcountmap[n] = 0;
			glyphcounts[n] = 0;
		}

		for( by = 0; by < BLKY; by++ )
		for( bx = 0; bx < BLKX; bx++ )
		{
			curtile[by][bx] = -1;
		}

		for( n = 0; n < tilechangect; n++ )
		{
			int t = tilechanges[n];
			int r = tileruns[n];

			glyphcounts[t]++;
			probcountmap[t]+=r+1;
		}

		for( n = 0; n < maxtileid_remapped; n++ )
		{
			double gratio = glyphcounts[n] * 1.0 / probcountmap[n];

			int prob = ( gratio * 257.0 ) - 1.5;

			if( prob < 0 ) prob = 0; 
			if( prob > 255 ) prob = 255;
			vpx_probs_by_tile_run[n] = prob;
			//printf( "%d %d %d %d\n", n, prob,  glyphcounts[n], probcountmap[n]);
		}
	}

	int bitsfortileid = intlog2( maxtileid_remapped );

	// Compute the chances-of-tile table.
	// This is a triangular structure.
	uint8_t chancetable_glyph[1<<bitsfortileid];
	memset( chancetable_glyph, 0, sizeof(chancetable_glyph) );
	{
		int nout = 0;
		int n = 0;
		int level = 0;
	
		for( level = 0; level < bitsfortileid; level++ )
		{
			int maxmask = 1<<bitsfortileid;
			int levelmask = (0xffffffffULL >> (32 - level)) << (bitsfortileid-level); // i.e. 0xfc (number of bits that must match)
			int comparemask = 1<<(bitsfortileid-level-1); //i.e. 0x02 one fewer than the levelmask
			int lincmask = comparemask<<1;
			int maskcheck = 0;
			for( maskcheck = 0; maskcheck < maxmask; maskcheck += lincmask )
			{
				int count1 = 0;
				int count0 = 0;
				for( n = 0; n < maxtileid_remapped; n++ )
				{
					if( ( n & levelmask ) == maskcheck )
					{
						if( n & comparemask )
							count1 += tilecounts[n];
						else
							count0 += tilecounts[n];
					}
				}
				double chanceof0 = count0 / (double)(count0 + count1);
				int prob = chanceof0 * 257 - 0.5;
				if( prob < 0 ) prob = 0;
				if( prob > 255 ) prob = 255;
				chancetable_glyph[nout++] = prob;
				//printf( "%d: %08x %d %d (%d)\n", nout-1, maskcheck, count0, count1, prob );
			}
		}
	}

	int checkchanges = 0;
	int bytesum = 0;
	int symsum = 0;
	int n;
	vpx_writer w_glyphs = { 0 };
	vpx_start_encode( &w_glyphs, bufferVPXg, sizeof(bufferVPXg));
	vpx_writer w_run = { 0 };
	vpx_start_encode( &w_run, bufferVPXr, sizeof(bufferVPXr));
	vpx_writer w_combined = { 0 };
	vpx_start_encode( &w_combined, bufferVPX, sizeof(bufferVPX));

	for( n = 0; n < tilechangect; n++ )
	{
		int tile = tilechanges[n];
		int run = tileruns[n];
		// First we encode the block ID.
		// Then we encode the run length.
		int level;
		checkchanges++;

		int probplace = 0;
		int probability = 0;
		for( level = 0; level < bitsfortileid; level++ )
		{
			int comparemask = 1<<(bitsfortileid-level-1); //i.e. 0x02 one fewer than the levelmask
			int bit = !!(tile & comparemask);
			probability = chancetable_glyph[probplace];
			vpx_write(&w_glyphs, bit, probability);
			vpx_write(&w_combined, bit, probability);
			probplace = ((1<<(level+1)) - 1 + ((tile)>>(bitsfortileid-level-1)));
		}

		probability = vpx_probs_by_tile_run[tile];
		int b;
		for( b = 0; b < run; b++ )
		{
			vpx_write(&w_run, 1, probability);
			vpx_write(&w_combined, 1, probability);
		}
		vpx_write(&w_run, 0, probability);
		vpx_write(&w_combined, 0, probability);

		symsum++;
	}
	vpx_stop_encode(&w_glyphs);
	int glyphbytes = w_glyphs.pos;

	vpx_stop_encode(&w_run);
	int runbytes = w_run.pos;

	vpx_stop_encode(&w_combined);
	int combinedstream = w_combined.pos;

	printf( " Num Changes:%6d\n", symsum );
	printf( "      Stream:%6d bits / bytes:%6d\n", glyphbytes*8, glyphbytes );
	printf( "         Run:%6d bits / bytes:%6d\n", runbytes*8, runbytes );
	printf( "\n" );
	printf( "    Combined:%6d bits / bytes:%6d\n", combinedstream*8, combinedstream );
	printf( " + Tile Prob:%6d bits / bytes:%6d\n", (int)sizeof(chancetable_glyph) * 8, (int)sizeof(chancetable_glyph) );
	printf( " +  Run Prob:%6d bits / bytes:%6d\n", (int)sizeof(vpx_probs_by_tile_run) * 8, (int)sizeof(vpx_probs_by_tile_run) );

	// test validate
	if( 1 )
	{
		CNFGClearFrame();
		int curglyph[BLKY][BLKX] = { 0 };
		int currun[BLKY][BLKX] = { 0 };
		int playptr = 0;
		int frame;
		int x, y;

		vpx_reader reader;
		vpx_reader_init(&reader, bufferVPX, w_combined.pos, 0, 0 );

		for( frame = 0; frame < frames; frame++ )
		{
			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				if( currun[y][x] == 0 )
				{
					int tile = 0;

					int bitsfortileid = intlog2( maxtileid_remapped );
					int level;

					int probplace = 0;
					int probability = 0;
					for( level = 0; level < bitsfortileid; level++ )
					{
						probability = chancetable_glyph[probplace];
						int bit = vpx_read( &reader, probability );
						tile |= bit<<(bitsfortileid-level-1);
						probplace = ((1<<(level+1)) - 1 + ((tile)>>(bitsfortileid-level-1)));
					}
					curglyph[y][x] = tile;

					probability = vpx_probs_by_tile_run[tile];
					int run = 0;
					while( vpx_read(&reader, probability) )
						run++;
					currun[y][x] = run;
					playptr++;
				}
				else
				{
					currun[y][x]--;
				}
				VPDrawGlyph( x*BLOCKSIZE*2, y*BLOCKSIZE*2, curglyph[y][x] );
			}
			CNFGSwapBuffers();
			usleep(10000);
		}
	}
}

