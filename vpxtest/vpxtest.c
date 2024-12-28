#include <stdio.h>

#define VPXCODING_WRITER
#define VPXCODING_READER
#define VPX_32BIT
#include "vpxcoding.h"

#define CNRBTREE_IMPLEMENTATION
#include "cnrbtree.h"

#include "bacommon.h"
#include "gifenc.c"

#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"

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
int tilechangesReal[MAXTILEDIFF]; // Change to this
int tileruns[MAXTILEDIFF];    // After changing, run for this long.
int tilechangect;

int maxtileid_remapped = 0;
uint8_t bufferVPX[1024*1024];
uint8_t bufferVPXg[1024*1024];
uint8_t bufferVPXr[1024*1024];

int intlog2 (int x){return __builtin_ctz (x);}
unsigned int intlog2roundup(unsigned int x) {
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return intlog2(x);
}

float * glyphsold;
int glyphctold;
float * glyphsnew;
uint64_t * glyphBnW;

#ifdef VPX_GORP_KERNEL_MOVE

float LERP( float a, float b, float t )
{
	return a * (1-t) + b * t;
}

float GPX( float fx, float fy, int curglyphs[BLKY][BLKX] )
{
	int gx = fx / BLOCKSIZE;
	int gy = fy / BLOCKSIZE;
	int x = (int)(fx + BLOCKSIZE) % BLOCKSIZE;
	int y = (int)(fy + BLOCKSIZE) % BLOCKSIZE;
	if( gy >= BLKY ) { gy = BLKY-1; y = BLOCKSIZE-1; } if( gy < 0 ) { gy = 0; y = 0; }
	if( gx >= BLKX ) { gx = BLKX-1; x = BLOCKSIZE-1; } if( gx < 0 ) { gx = 0; x = 0; }
	int glyph = curglyphs[gy][gx];
	//x &= ~1;
	//y &= ~1;
	return glyphsnew[(x)+(y)*BLOCKSIZE+(BLOCKSIZE*BLOCKSIZE)*(glyph)];
}

float GETPIX( int x, int y, int gx, int gy, int curglyphs[BLKY][BLKX], int prevglyphs[BLKY][BLKX] )
{
	int fx = x + gx * BLOCKSIZE;
	int fy = y + gy * BLOCKSIZE;

	float f = 0;
	if( 1 ) 
	{
		int hx = fx;
		int hy = fy;

		int tx, ty;

		#ifdef VPX_TAA
		f += GPX( hx + 0, hy - 1, curglyphs ) / 8.0;
		f += GPX( hx + 0, hy + 0, curglyphs ) / 8.0;
		f += GPX( hx + 0, hy + 1, curglyphs ) / 8.0;
		f += GPX( hx - 1, hy + 0, curglyphs ) / 8.0;
		f += GPX( hx + 1, hy + 0, curglyphs ) / 8.0;
		f += GPX( hx - 1, hy - 1, prevglyphs ) / 8.0;
		f += GPX( hx    , hy    , prevglyphs ) / 8.0;
		f += GPX( hx + 1, hy + 1, prevglyphs ) / 8.0;
		#else

		for( ty = 0; ty < 2; ty+=VPX_GORP_KERNEL_MOVE )
		for( tx = 0; tx < 2; tx+=VPX_GORP_KERNEL_MOVE )
			f += GPX( hx + tx, hy + ty, curglyphs ) / 4.0;
		#endif
	}
	else
	{
		const int ifkern = 4;
		const int kernel = 8;

		int hx = (fx / ifkern) * ifkern;
		int hy = (fy / ifkern) * ifkern;

		float f00 = GPX( hx       , hy,        curglyphs );
		float f10 = GPX( hx+ifkern, hy,        curglyphs );
		float f01 = GPX( hx       , hy+ifkern, curglyphs );
		float f11 = GPX( hx+ifkern, hy+ifkern, curglyphs );

		float f0 = LERP( f00, f10, (float)(fx%kernel)/kernel );
		float f1 = LERP( f01, f11, (float)(fx%kernel)/kernel );
		f = LERP( f0, f1, (float)(fy%kernel)/kernel );
	}

	return f;
}

#else
#define GETPIX(x,y,gx,gy,curglyphs,prevglyphs) glyphsnew[(x)+(y)*BLOCKSIZE+(BLOCKSIZE*BLOCKSIZE)*(glyph)]
#endif

void VPDrawGlyph( int xofs, int yofs, int gx, int gy, int curglyphs[BLKY][BLKX], int prevglyphs[BLKY][BLKX] )
{
	uint32_t bobig[BLOCKSIZE*2*BLOCKSIZE*2];
	int bit = 0;
	int x, y;
	int glyph = curglyphs[gy][gx];
	if( glyph >= maxtileid_remapped )
	{
		fprintf( stderr, "Glyph %d/%d\n", glyph, maxtileid_remapped );
		exit( -9 );
	}
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		float f = GETPIX( x, y, gx, gy, curglyphs, prevglyphs);
		const int hardline = 0;
		uint32_t v;
		if( hardline )
		{
			v = ( f >= 0.5) ? 0xffffffff : 0xff000000;
		}
		else
		{
			v = 0xff000000;
			int a = f * 256;
			if( a > 255 ) a = 255;
			if( a < 0 ) a = 0;
			v |= a | (a<<8) | (a<<16);
		}

		bobig[(2*x+0) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+0) + (2*y+1)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+1)*BLOCKSIZE*2] = v;
	}
	CNFGBlitImage( bobig, xofs, yofs, BLOCKSIZE*2, BLOCKSIZE*2 );
}

void VPBlockDrawGif( ge_GIF * gifout, int xofs, int yofs, int vw, int gx, int gy, int curglyphs[BLKY][BLKX], int prevglyphs[BLKY][BLKX] )
{
	int x, y;
	int glyph = curglyphs[gy][gx];
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		float d = GETPIX( x, y, gx, gy, curglyphs, prevglyphs );
#ifdef VPX_GREY16
		int b = d * 15.9;
#elif defined( VPX_GREY4 )
		int b = d * 3.999;
#else
		int b = d * 1.9;
#endif
		uint8_t * v = gifout->frame;
		v[(2*x+0+xofs) + (2*y+0 + yofs)*vw] = b;
		v[(2*x+1+xofs) + (2*y+0 + yofs)*vw] = b;
		v[(2*x+0+xofs) + (2*y+1 + yofs)*vw] = b;
		v[(2*x+1+xofs) + (2*y+1 + yofs)*vw] = b;
	}
}


int FileLength( const char * filename )
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

int main( int argc, char ** argv )
{
	if( argc != 4 )
	{
		fprintf( stderr, "Error: usage: ./vpxtest [stream.dat] [tiles.dat] [gif]\n" );
		exit( -5 );
	}
	CNFGSetup( "comp test", 1800, 1060 );
	int i;
	int maxtileid = 0; // pre-remapped

	FILE * f = fopen( argv[1], "rb" );
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

	FILE * fT = fopen( argv[2], "rb" );
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
#if defined( SMART_TRANSITION_SKIP ) || defined( SKIP_ONLY_DOUBLE_UPDATES )
	int ratcheted = 0;
#endif
#ifdef VPX_CODING_ALLOW_BACKTRACK
	int backtrackcount = 0;
#endif
	{
		FILE * fRawTileStream = fopen( "TEST_streamids.dat", "wb" );
		FILE * fRawTileRunLen = fopen( "TEST_runlens.dat", "wb" );

		int * tilecounts_temp = alloca( 4 * maxtileid );
		memset( tilecounts_temp, 0, 4 * maxtileid );

		int tilechangerun[BLKY][BLKX] = { 0 };
		int lasttile[BLKY][BLKX] = { 0 };
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
					int next = t;
					for( forward = 1; frame + forward < frames; forward++ )
						if( (next = tiles[(x+y*(BLKX)) + BLKX*BLKY * (frame + forward)] ) != t )
							break;

#if defined( SKIP_ONLY_DOUBLE_UPDATES )
					if( forward == 1 ) forward++;
#elif defined( SMART_TRANSITION_SKIP )
					if( frame + forward + 1 < frames && tiles[(x+y*(BLKX)) + BLKX*BLKY * (frame + forward + 1)] != next )
					{
						forward++;
						ratcheted++;
					}
#endif

					forward--;

					// Our product:
					//int tilechanges[MAXTILEDIFF];
					//int tileruns[MAXTILEDIFF];
					// based on
					//int tilechanges;

#ifdef VPX_CODING_ALLOW_BACKTRACK
					// Allow row-wrapping.
					int backtile = (x == 0 && y == 0 ) ? lasttile[BLKY-1][BLKX-1] : lasttile[y][x-1];
					if( t == backtile )
					{
						backtrackcount++;
						tilechanges[tilechangect] = -1;
					}
					else
#endif			
					{
						tilechanges[tilechangect] = t;
						tilecounts_temp[t]++;					// For frequency monitoring.
					}
					tilechangesReal[tilechangect] = t;
					tileruns[tilechangect] = forward;
					tilechangect++;

					fwrite( &t, 1, 1, fRawTileStream );
					fwrite( &t, 1, 2, fRawTileRunLen );

					// For knowing when to pull a new cell.
					tilechangerun[y][x] = forward;
					lasttile[y][x] = t;
				}
				else
				{
					tilechangerun[y][x]--;
				}
			}
		}
		fclose( fRawTileStream );
		fclose( fRawTileRunLen );

		cnrbtree_u32u32 * countmap = cnrbtree_u32u32_create();

		maxtileid_remapped = 0;
		for( i = 0; i < maxtileid; i++ )
		{
			if( tilecounts_temp[i] > 0 )
			{
				maxtileid_remapped++;
				RBA( countmap, tilecounts_temp[i] ) = i;
			}
			else
			{
				printf( "No uses of glyph %d old. Dropping.\n", i );
			}
		}

		int n = maxtileid_remapped-1;

		RBFOREACH( u32u32, countmap, i )
		{
			tileremap[n] = i->data;
			tilecounts[n] = i->key;
			tileremapfwd[i->data] = n;
			printf( "Orig: %d  New: %d  Freq: %d\n", i->data, n, i->key );
			n--;
		}

		for( i = 0; i < tilechangect; i++ )
		{
			int tin = tilechanges[i];
			int tile = tilechanges[i] = (tin>=0)?tileremapfwd[tin]:tin;

			int tinR = tilechangesReal[i];
			tilechangesReal[i] = tileremapfwd[tinR];

			if( tile >= maxtileid_remapped )
			{
				fprintf( stderr, "Encoding Glyph %d/%d\n", tile, maxtileid_remapped );
				exit( -9 );
			}
		}

		glyphsnew = malloc( (maxtileid_remapped)*BLOCKSIZE*BLOCKSIZE*4 );
		for( i = 0; i < maxtileid_remapped; i++ )
		{
			int oldid = tileremap[i];
			memcpy( &glyphsnew[BLOCKSIZE*BLOCKSIZE*i],
				&glyphsold[BLOCKSIZE*BLOCKSIZE*oldid], BLOCKSIZE*BLOCKSIZE*4 );
			struct block b; 
			memcpy( b.intensity, &glyphsnew[BLOCKSIZE*BLOCKSIZE*i], BLOCKSIZE*BLOCKSIZE*4 );
			UpdateBlockDataFromIntensity( &b );
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
		int prevglyph[BLKY][BLKX] = { 0 };
		int currun[BLKY][BLKX] = { 0 };
		int playptr = 0;
		int frame;
		int x, y;
		for( frame = 0; frame < frames; frame++ )
		{
			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				VPDrawGlyph( x*BLOCKSIZE*2, y*BLOCKSIZE*2, x, y, curglyph, prevglyph );
				if( currun[y][x] == 0 )
				{
					curglyph[y][x] = tilechangesReal[playptr];
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
			memcpy( prevglyph, curglyph, sizeof( curglyph ) );
		}
	}

#ifdef VPX_CODING_ALLOW_BACKTRACK
	// Allow referencing the previous tile.
	int probbacktrack = 0;
	{
		double chanceof0 = backtrackcount / (double)(tilechangect);
		int prob = chanceof0 * 257 - 0.5;
		if( prob < 0 ) prob = 0;
		if( prob > 255 ) prob = 255;
		probbacktrack = prob;
	}
	printf( "Prob Backtrack: %d\n", probbacktrack );
#endif

	int bitsfortileid = intlog2roundup( maxtileid_remapped );

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

#ifdef SKIP_FIRST_AFTER_TRANSITION
	const int RSOPT = 1;
#else
	const int RSOPT = 0;
#endif

	// Now, we have to compress tilechanges & tileruns
	uint8_t vpx_probs_by_tile_run[maxtileid_remapped];
	uint8_t vpx_probs_by_tile_run_after_one[maxtileid_remapped];
	{
		int probcountmap[maxtileid_remapped];
		int glyphcounts[maxtileid_remapped];
		int probcountmap_after_one[maxtileid_remapped];
		int glyphcounts_after_one[maxtileid_remapped];
		int curtile[BLKY][BLKX];
		int n, bx, by;

		for( n = 0; n < maxtileid_remapped; n++ )
		{
			probcountmap[n] = 0;
			glyphcounts[n] = 0;
			probcountmap_after_one[n] = 0;
			glyphcounts_after_one[n] = 0;
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

			if( t >= 0 )
			{
#ifdef RUNCODES_TWOLEVEL
				// Glyphcounts is used only for the first.
				if( r > RSOPT )
				{
					glyphcounts_after_one[t]++;
					probcountmap_after_one[t]+=r-RSOPT;
				}

				// XXX TODO: How do we optimally check only the first bit,
				// without lumping it into everything else.
				glyphcounts[t]++;
				probcountmap[t]+=r+1-RSOPT;
#else
				// This combines probability at any given time.
				glyphcounts[t]++;
				probcountmap[t]+=r+1-RSOPT;
#endif
			}
		}

		for( n = 0; n < maxtileid_remapped; n++ )
		{
			double gratio;
			int prob;
			gratio = glyphcounts[n] * 1.0 / probcountmap[n];
			prob = ( gratio * 257.0 ) - 1.5;
			if( prob < 0 ) prob = 0; 
			if( prob > 255 ) prob = 255;
			vpx_probs_by_tile_run[n] = prob;

			gratio = glyphcounts_after_one[n] * 1.0 / probcountmap_after_one[n];
			prob = ( gratio * 257.0 ) - 1.5;
			if( prob < 0 ) prob = 0; 
			if( prob > 255 ) prob = 255;
			vpx_probs_by_tile_run_after_one[n] = prob;

			//printf( "%d [%d %d] %d %d\n", n, vpx_probs_by_tile_run[n],
			//	vpx_probs_by_tile_run_after_one[n], glyphcounts[n], probcountmap[n]);
		}
	}

	uint8_t vpx_glyph_tiles_buffer[1024*32];
	int vpx_glyph_tiles_buffer_len = 0;
	#define MAXRUNTOSTORE 8
	uint8_t prob_from_0_or_1[2][MAXRUNTOSTORE];

#if defined( VPX_GREY4 ) || defined( VPX_GREY16 ) || defined( SIMGREY4 )
	if( 0 )
#else
	if( 1 )
#endif
	{
		// Just a quick test to see if we can compress the tilemaps.
		int i;
		FILE * fRawTiles = fopen( "TEST_rawtiles.dat", "wb" );
		int bnw[BLOCKSIZE*BLOCKSIZE*maxtileid_remapped];
		for( i = 0; i < maxtileid_remapped; i++ )
		{
			float * fg = &glyphsnew[i*BLOCKSIZE*BLOCKSIZE];
			int y, x;
			for( y = 0; y < BLOCKSIZE; y++ )
			{
				int byte = 0;
				for( x = 0; x < BLOCKSIZE; x++ )
				{
					int c = bnw[(x+y*BLOCKSIZE)+i*BLOCKSIZE*BLOCKSIZE] = fg[x+y*BLOCKSIZE] > 0.5;
					byte = (byte<<1) | c;
				}
				fwrite( &byte, 1, 1, fRawTiles );
			}
		}
		fclose( fRawTiles );

		int runsets0to0[MAXRUNTOSTORE] = { 0 };
		int runsets0to1[MAXRUNTOSTORE] = { 0 };
		int runsets1to0[MAXRUNTOSTORE] = { 0 };
		int runsets1to1[MAXRUNTOSTORE] = { 0 };
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE*maxtileid_remapped; i++ )
		{
			int color = bnw[i];
			int j;
			int b = 0;
			for( j = i+1; j < BLOCKSIZE*BLOCKSIZE*maxtileid_remapped; j++ )
			{
				int p = bnw[j];
				if( color == 0 )
				{
					if( p == 0 )
						runsets0to0[b]++;
					else
					{
						runsets0to1[b]++;
						break;
					}
				}
				else
				{
					if( p == 0 )
					{
						runsets1to0[b]++;
						break;
					}
					else
					{
						runsets1to1[b]++;
					}
				}
				b++;
				if( b == MAXRUNTOSTORE ) break;
			}
		}
		for( int j = 0; j < MAXRUNTOSTORE; j++ )
		{
			double chanceof0 = runsets0to0[j] / (double)(runsets0to0[j]+runsets0to1[j]);
			int prob = chanceof0 * 257 - 0.5;
			if( prob < 0 ) prob = 0;
			if( prob > 255 ) prob = 255;
			int prob0 = prob_from_0_or_1[0][j] = prob;

			chanceof0 = runsets1to0[j] / (double)(runsets1to0[j]+runsets1to1[j]);
			prob = chanceof0 * 257 - 0.5;
			if( prob < 0 ) prob = 0;
			if( prob > 255 ) prob = 255;
			int prob1 = prob_from_0_or_1[1][j] = prob;
			//printf( "%d (%d/%d) (%d/%d) %4d%4d\n", j, runsets0to0[j], runsets0to1[j], runsets1to0[j], runsets1to1[j] , prob0, prob1 );
		}

		vpx_writer w_glyphdata = { 0 };
		vpx_start_encode( &w_glyphdata, vpx_glyph_tiles_buffer, sizeof(vpx_glyph_tiles_buffer));

		int runsofar = 0;
		int is0or1 = bnw[0];

		for( i = 0; i < BLOCKSIZE*BLOCKSIZE*maxtileid_remapped; i++ )
		{
			uint8_t * prob = prob_from_0_or_1[is0or1];
			int tprob = prob[runsofar];

			if( ( i & ((BLOCKSIZE*BLOCKSIZE)-1)) == 0 )
				tprob = 128;

			int color = bnw[i];
			//printf( "%d: %d\n", color, tprob );
			vpx_write(&w_glyphdata, color, tprob );

			if( color != is0or1 )
			{
				is0or1 = color;
				runsofar = 0;
			}
			else if( runsofar < MAXRUNTOSTORE-1 )
			{
				runsofar++;
			}
		}

		vpx_stop_encode(&w_glyphdata);
		vpx_glyph_tiles_buffer_len = w_glyphdata.pos;
//		exit( 0 );
	}


#ifdef VPX_USE_HUFFMAN_TILES
	uint8_t huffman_tile_stream[1024*512];
	int huffman_tile_stream_length_bits = 0;
	int huffman_tile_stream_length_bytes = 0;
	int hufftreelen = 0;
	uint32_t huffman_tree[1024];
	{
		//int tilechangesReal[MAXTILEDIFF]; // Change to this
		//int tilechangect;

		//huffelement * GenerateHuffmanTree( hufftype * data, hufffreq * frequencies, int numpairs, int * hufflen );

		uint32_t tileids[maxtileid_remapped];
		uint32_t frequencies[maxtileid_remapped];

		int i;
		for( i = 0; i < maxtileid_remapped; i++ )
		{
			tileids[i] = i;
			frequencies[i] = 0;
		}

		for( i = 0; i < tilechangect; i++ )
		{
			frequencies[tilechangesReal[i]]++;
		}

		huffelement * tree = GenerateHuffmanTree( tileids, frequencies, maxtileid_remapped, &hufftreelen );

		for( i = 0; i < hufftreelen; i++ )
		{
			huffelement * e = tree + i;
			if( e->is_term )
				huffman_tree[i] = 0x800000 | e->value;
			else
				huffman_tree[i] = e->pair0 | (e->pair1<<12);
		}

		int htlen;
		huffup * hu = GenPairTable( tree, &htlen );
		int len_bits = 0;

		int idsofhu[maxtileid_remapped];
		for( i = 0; i < htlen; i++ )
		{
			idsofhu[hu[i].value] = i;
		}

		if( 0 )
		{
			for( i = 0; i < htlen; i++ )
			{
				int idx = idsofhu[i];
				printf( "%d: %d/%d/ %d %d > ",i, hu[idx].freq, frequencies[i], hu[idx].value, hu[idx].bitlen );
				int k;
				for( k = 0; k < hu[idx].bitlen; k++ )
				{
					printf( "%d", hu[idx].bitstream[k] );
				}
				printf( "\n" );
			}
			for( i = 0; i < hufftreelen; i++ )
			{
				printf( "%d %d->%d:  %d  (%d/%d)\n", i, tree[i].value, tree[i].freq, tree[i].is_term, tree[i].pair0, tree[i].pair1 );
			}
		}

		int bytepl = 0;
		uint8_t bytev = 0;
		for( i = 0; i < tilechangect; i++ )
		{
			int tile = tilechangesReal[i];
			int huid = idsofhu[tile];
			int bl = hu[huid].bitlen;
			//printf( "%d -> %d -> %d -> %d\n", i, tile, huid, bl ); 
			int j;
			for( j = 0; j < bl; j++ )
			{
				bytepl++;
				bytev = (bytev<<1) | hu[huid].bitstream[j];
	
				if( bytepl == 8 )
				{
					huffman_tile_stream[huffman_tile_stream_length_bytes++] = bytev;
					bytepl = 0;
				}
			}
			len_bits += bl;
		}

		if( bytepl )
		{
			huffman_tile_stream[huffman_tile_stream_length_bytes++] = bytev;
		}

		huffman_tile_stream_length_bits = len_bits;
		huffman_tile_stream_length_bytes = (len_bits+7)/8;
	}
#endif


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
		int tileReal = tilechangesReal[n];
		// First we encode the block ID.
		// Then we encode the run length.
		int level;
		checkchanges++;

		int probplace = 0;
		int probability = 0;

#ifdef VPX_CODING_ALLOW_BACKTRACK
		if( tile == -1 )
		{
			vpx_write(&w_glyphs, 0, probbacktrack);
			vpx_write(&w_combined, 0, probbacktrack);
		}
		else
		{
			vpx_write(&w_glyphs, 1, probbacktrack);
			vpx_write(&w_combined, 1, probbacktrack);
#else
		{
#endif
			for( level = 0; level < bitsfortileid; level++ )
			{
				int comparemask = 1<<(bitsfortileid-level-1); //i.e. 0x02 one fewer than the levelmask
				int bit = !!(tile & comparemask);
				probability = chancetable_glyph[probplace];
				vpx_write(&w_glyphs, bit, probability);
#ifndef VPX_USE_HUFFMAN_TILES
				vpx_write(&w_combined, bit, probability);
#endif
				probplace = ((1<<(level+1)) - 1 + ((tile)>>(bitsfortileid-level-1)));
			}
		}

		int run = tileruns[n] - RSOPT;
		probability = vpx_probs_by_tile_run[tileReal];
		int b;
		for( b = 0; b < run; b++ )
		{
			vpx_write(&w_run, 1, probability);
			vpx_write(&w_combined, 1, probability);
#ifdef RUNCODES_TWOLEVEL
			probability = vpx_probs_by_tile_run_after_one[tileReal];
#endif
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

	printf( "      Glyphs:%7d\n", maxtileid_remapped + 1 );
	printf( " Num Changes:%7d\n", symsum );
#ifdef SMART_TRANSITION_SKIP
	printf( "   Ratcheted:%7d\n", ratcheted );
#endif
	printf( "      Stream:%7d bits / bytes:%6d\n", glyphbytes*8, glyphbytes );
	printf( "         Run:%7d bits / bytes:%6d\n", runbytes*8, runbytes );
	printf( "\n" );
	int sum = 0;
	printf( "    Combined:%7d bits / bytes:%6d\n", combinedstream*8, combinedstream );
	sum += combinedstream;

#ifdef VPX_USE_HUFFMAN_TILES
	printf( "    HuffmanD:%7d bits / bytes:%6d\n", huffman_tile_stream_length_bytes*8, huffman_tile_stream_length_bytes );
	printf( "    HuffmanT:%7d bits / bytes:%6d\n", hufftreelen*3*8, hufftreelen*3 );
	sum += huffman_tile_stream_length_bytes + hufftreelen*3;
#endif

	printf( " + Tile Prob:%7d bits / bytes:%6d\n", (int)sizeof(chancetable_glyph) * 8, (int)sizeof(chancetable_glyph) );
	sum += (int)sizeof(chancetable_glyph);
	printf( " +  Run Prob:%7d bits / bytes:%6d\n", (int)sizeof(vpx_probs_by_tile_run) * 8, (int)sizeof(vpx_probs_by_tile_run) );
	sum += (int)sizeof(vpx_probs_by_tile_run);
#ifdef RUNCODES_TWOLEVEL
	printf( " + Run Prob2:%7d bits / bytes:%6d\n", (int)sizeof(vpx_probs_by_tile_run_after_one) * 8, (int)sizeof(vpx_probs_by_tile_run_after_one) );
	sum += (int)sizeof(vpx_probs_by_tile_run_after_one);
#endif

	int glyphsize = BLOCKSIZE * BLOCKSIZE / 8 * maxtileid_remapped;
#ifdef VPX_GREY4
	glyphsize *= 2;
#elif defined( VPX_GREY16 )
	glyphsize *= 4;
#endif

	if( vpx_glyph_tiles_buffer_len )
	{
		printf( " +COMPGlyphs:%7d bits / bytes:%6d\n", (vpx_glyph_tiles_buffer_len + (int)sizeof(prob_from_0_or_1)) * 8, (vpx_glyph_tiles_buffer_len + (int)sizeof(prob_from_0_or_1)) );
		//printf( " N/A  Glyphs:%7d bits / bytes:%6d\n", glyphsize * 8, glyphsize );
		//printf( " N/A   CDATA:%7d bits / bytes:%6d\n", (int)sizeof(prob_from_0_or_1) * 8, (int)sizeof(prob_from_0_or_1) );
		sum += (vpx_glyph_tiles_buffer_len + (int)sizeof(prob_from_0_or_1));
	}
	else
	{
		printf( " +    Glyphs:%7d bits / bytes:%6d\n", glyphsize * 8, glyphsize );
		sum += glyphsize;
	}

	int sHuffD = FileLength( "../song/huffD_fmraw.dat" );
	int sHuffTL = FileLength( "../song/huffTL_fmraw.dat" );
	int sHuffTN = FileLength( "../song/huffTN_fmraw.dat" );
	if( sHuffD > 0 )
	{
		printf( " + Sound (D):%7d bits / bytes:%6d\n", sHuffD * 8, sHuffD );
		sum += sHuffD;
	}
	if( sHuffTL > 0 )
	{
		printf( " + Sound (L):%7d bits / bytes:%6d\n", sHuffTL * 8, sHuffTL );
		sum += sHuffTL;
	}
	if( sHuffTN > 0 )
	{
		printf( " + Sound (N):%7d bits / bytes:%6d\n", sHuffTN * 8, sHuffTN );
		sum += sHuffTN;
	}

	printf( "\n" );
	printf( " Total video: %6d Bytes (%d bits) (%.3f bits/frame) (%d frames)\n", sum, sum*8, sum*8.0/frames, frames );

	// test validate
	if( 1 )
	{
		if( vpx_glyph_tiles_buffer_len )
		{

			//uint8_t vpx_glyph_tiles_buffer[1024*32];
			//int vpx_glyph_tiles_buffer_len = 0;
			//#define MAXRUNTOSTORE 8
			//uint8_t prob_from_0_or_1[2][MAXRUNTOSTORE];
			vpx_reader reader_tiles;
			vpx_reader_init(&reader_tiles, vpx_glyph_tiles_buffer, vpx_glyph_tiles_buffer_len, 0, 0 );

			// Decompress glyph data.
			int runsofar = 0;
			int is0or1 = 0;
			int n = 0;

			for( i = 0; i < BLOCKSIZE*BLOCKSIZE*maxtileid_remapped; i++ )
			{
				uint8_t * prob = prob_from_0_or_1[is0or1];
				int tprob = prob[runsofar];

				if( ( i & ((BLOCKSIZE*BLOCKSIZE)-1)) == 0 )
					tprob = 128;

				int color = vpx_read( &reader_tiles, tprob );

				glyphsnew[n++] = color ? 1 : 0;

				if( color != is0or1 )
				{
					is0or1 = color;
					runsofar = 0;
				}
				else if( runsofar < MAXRUNTOSTORE-1 )
				{
					runsofar++;
				}
			}

		}



		CNFGClearFrame();
		int curglyph[BLKY][BLKX] = { 0 };
		int prevglyph[BLKY][BLKX] = { 0 };
		int currun[BLKY][BLKX] = { 0 };
		int playptr = 0;
		int frame;
		int x, y;

		vpx_reader reader;
		vpx_reader_init(&reader, bufferVPX, w_combined.pos, 0, 0 );

#ifdef VPX_GREY4
		uint8_t palette[48] = { 0, 0, 0, 85, 85, 85, 171, 171, 171, 255, 255, 255 };
#elif defined( VPX_GREY16 )
		uint8_t palette[48] = {
			0, 0, 0, 17, 17, 17, 34, 34, 34, 51, 51, 51,
			68, 68, 68, 85, 85, 85, 102, 102, 102, 119, 119, 119,
			136, 136, 136, 153, 153, 153, 170, 170, 170, 187, 187, 187,
			204, 204, 204, 221, 221, 221, 238, 238, 238, 255, 255, 255 };
#else
		uint8_t palette[48] = { 0, 0, 0, 255, 255, 255 };
#endif
		ge_GIF * gifout = ge_new_gif( argv[3], RESX*2, RESY*2, palette, 4, -1, 0 );

		FILE * bituse = fopen ( "bituse.txt", "w" );
		FILE * bitusekbits = fopen ( "bitusepersec.txt", "w" );
		int bitssofarthissec = 0;
		for( frame = 0; frame < frames; frame++ )
		{
			intptr_t startbuffer = (intptr_t)reader.buffer;
			int startcount = reader.count;
			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				if( currun[y][x] == 0 )
				{
					int tile = 0;

					int bitsfortileid = intlog2roundup( maxtileid_remapped );
					int level;

					int probability = 0;

#ifdef VPX_USE_HUFFMAN_TILES
					static int huffmanbit = 0;
					int b = 0;
					uint32_t treepos = 0;
					do
					{
						uint32_t te = huffman_tree[treepos];
						if( te & 0x800000 )
						{
							tile = te & 0x1ff;
							break;
						}
						int b = (huffman_tile_stream[huffmanbit/8]>>(7-(huffmanbit&7)))&1;
						huffmanbit++;
						//printf( "%d/%08x/%d // %d %08x\n", b, te, treepos, huffmanbit, huffman_tile_stream[huffmanbit/8] );
						treepos = (te >> ((b)*12)) & 0xfff;
					} while( huffmanbit < huffman_tile_stream_length_bits);

#else
#ifdef VPX_CODING_ALLOW_BACKTRACK
					int nbacktrack = vpx_read( &reader, probbacktrack );

					if( nbacktrack == 0 )
					{
						tile = (x == 0 && y == 0 ) ? curglyph[BLKY-1][BLKX-1] : curglyph[y][x-1];
					}
					else
#endif
					{
						int probplace = 0;
						for( level = 0; level < bitsfortileid; level++ )
						{
							probability = chancetable_glyph[probplace];
							int bit = vpx_read( &reader, probability );
							tile |= bit<<(bitsfortileid-level-1);
							probplace = ((1<<(level+1)) - 1 + ((tile)>>(bitsfortileid-level-1)));
						}
					}
#endif

					curglyph[y][x] = tile;

					probability = vpx_probs_by_tile_run[tile];
					int run = 0;
					while( vpx_read(&reader, probability) )
					{
#ifdef RUNCODES_TWOLEVEL
						probability = vpx_probs_by_tile_run_after_one[tile];
#endif
						run++;
					}
					currun[y][x] = run + RSOPT;
					playptr++;

				}
				else
				{
					currun[y][x]--;
				}
			}

			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				VPDrawGlyph( x*BLOCKSIZE*2, y*BLOCKSIZE*2, x, y, curglyph, prevglyph );
				VPBlockDrawGif( gifout, x * BLOCKSIZE*2, y * BLOCKSIZE*2, RESX*2, x, y, curglyph, prevglyph );
			}

			ge_add_frame(gifout, 2);
			CNFGSwapBuffers();
			//usleep(100000);
			int endcount = reader.count;
			intptr_t endbuffer = (intptr_t)reader.buffer;
			int bits_used_this_frame = (startcount - endcount) + (endbuffer - startbuffer) * 8;
			bitssofarthissec += bits_used_this_frame;
			fprintf( bituse, "%d\n", bits_used_this_frame );
			if( ( frame % 30 ) == 29 )
			{
				fprintf( bitusekbits, "%d\n", bitssofarthissec );
				bitssofarthissec = 0;
			}
			memcpy( prevglyph, curglyph, sizeof( curglyph ) );
		}
		fclose( bituse );
		fclose( bitusekbits );
		ge_close_gif( gifout );
	}

}

