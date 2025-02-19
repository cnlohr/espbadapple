#include <stdio.h>
#include <math.h>

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


#include "vpxtree.h"

#define VPX_PROB_MULT 257.0
#define VPX_PROB_SHIFT (-0.0)

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
int tilechangesfrom[MAXTILEDIFF]; // Change to this
int tilechangesReal[MAXTILEDIFF]; // Change to this
int tileruns[MAXTILEDIFF];    // After changing, run for this long.
int tilechangect;

int maxtilect_remapped = 0;
uint8_t bufferVPX[1024*1024];
uint8_t bufferVPXg[1024*1024];
uint8_t bufferVPXr[1024*1024];

int intctz (int x){return __builtin_ctz (x);}
unsigned int intlog2roundup(unsigned int x) {
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return intctz(x);
}

static int flipbits( int v, int bitsfortileid )
{
	int ret = 0;
	int n;
	for( n = 0; n < bitsfortileid; n++ )
	{
		if( v & (1<<n) )
			ret |= (1<<(bitsfortileid-n-1));
	}
	return ret;
}

void WriteFileStreamHeader( FILE * f, vpx_writer * w, const char * name )
{
	fprintf( f, "BADATA_DECORATOR uint8_t %s[%d] = {", name, w->pos );
	int i;
	for( i = 0; i < w->pos; i++ )
	{
		fprintf( f, "%s0x%02x, ", (i&15)?"":"\n\t", w->buffer[i] );
	}
	fprintf( f, "};\n\n" );
}

void WriteOutFile( FILE * f, const char * name, const char * fname )
{
	FILE * fi = fopen( fname, "rb" );
	fseek( fi, 0, SEEK_END );
	int len = (int)ftell(fi);
	fseek( fi, 0, SEEK_SET );
	uint8_t * data=  malloc( len );
	if( fread( data, len, 1, fi ) != 1 )
	{
		fprintf( stderr, "error: can't read data from %s\n", fname );
		exit( -5 );
	}
	fclose( fi );
	fprintf( f, "BADATA_DECORATOR uint8_t %s[%d] = {", name, len );
	int i;
	for( i = 0; i < len; i++ )
	{
		fprintf( f, "%s0x%02x, ", (i&15)?"":"\n\t", data[i] );
	}
	fprintf( f, "\n};\n\n" );
	free( data );
}// fDataOut, "sound_huffTL", "../song/huffTL_fmraw.dat" );


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
	if( glyph >= maxtilect_remapped )
	{
		fprintf( stderr, "Glyph %d/%d\n", glyph, maxtilect_remapped );
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

#if VPX_GORP_KERNEL_MOVE
		#define CONTRAST 10.0
//		printf( "%f %f\n", d, 1.0/(1.0+expf(-(d-0.5)*CONTRAST)) );
		d = 1.0/(1.0+expf(-(d-0.5)*CONTRAST));
#endif
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
	uint8_t startcell = 0;
	int i, j;
	if( argc != 4 )
	{
		fprintf( stderr, "Error: usage: ./vpxtest [stream.dat] [tiles.dat] [gif]\n" );
		exit( -5 );
	}
	CNFGSetup( "comp test", 1800, 1060 );
	int maxtilect = 0; // pre-remapped (Actually count)

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
		if( tile >= maxtilect )
		{
			maxtilect = tile+1;
		}
	}
	fclose( f );

	// Select default cell to start with.
	startcell = tiles[0];
	int startcellremap;

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
	int * tilecounts = calloc( 4, maxtilect );   // [newid]
	int * tileremap = calloc( 4, maxtilect );    // [newid] = originalid
	int * tileremapfwd = calloc( 4, maxtilect ); // [originalid] = newid

	int * fromtofrequency = calloc( 4, maxtilect*maxtilect );
	int * fromtofrequencyremap;

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

		int * tilecounts_temp = alloca( 4 * maxtilect );
		memset( tilecounts_temp, 0, 4 * maxtilect );

		static int tilechangerun[BLKY][BLKX] = { 0 };
		static int lasttile[BLKY][BLKX];

		int x, y, frame;

		for( y = 0; y < BLKY; y++ )
		for( x = 0; x < BLKX; x++ )
		{
			lasttile[y][x] = startcell;
		}

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

#if defined( SMART_TRANSITION_SKIP )
					if( frame + forward + 1 < frames && tiles[(x+y*(BLKX)) + BLKX*BLKY * (frame + forward + 1)] != next && forward < MAX_FORWARD-1 )
					{
						forward++;
						ratcheted++;
					}
#endif
#if defined( SKIP_ONLY_DOUBLE_UPDATES )
					if( forward == 1 ) forward++;
#endif

#ifdef SKIP_FIRST_AFTER_TRANSITION
					if(forward < 2 )
					{
						fprintf( stderr, "Error bad configuration\n" );
						exit( -5 );
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

					int lt = lasttile[y][x];
					tilechangesfrom[tilechangect] = lt;
					tilechangect++;

					fwrite( &t, 1, 1, fRawTileStream );
					fwrite( &t, 1, 2, fRawTileRunLen );

					// For knowing when to pull a new cell.
					tilechangerun[y][x] = forward;
					lasttile[y][x] = t;

					// New experiment: Synthesize a full matrix.
					fromtofrequency[lt*maxtilect+t]++;
				}
				else
				{
					tilechangerun[y][x]--;
				}
			}

#ifdef MONITOR_FRAME
			if( frame == MONITOR_FRAME )
			{
				for( y = 0; y < BLKY; y++ )
				{
					for( x = 0; x < BLKX; x++ )
					{
						printf( "%4d", lasttile[y][x] );
					}
					printf( "\n" );
				}
			}
#endif
		}
		fclose( fRawTileStream );
		fclose( fRawTileRunLen );

		cnrbtree_u32u32 * countmap = cnrbtree_u32u32_create();

		maxtilect_remapped = 0;
		for( i = 0; i < maxtilect; i++ )
		{
			if( tilecounts_temp[i] > 0 )
			{
				maxtilect_remapped++;
				RBA( countmap, tilecounts_temp[i] ) = i;
				//printf( "REMAP %d\n", i );
			}
			else
			{
				printf( "No uses of glyph %d old. Dropping.\n", i );
			}
		}

		int n = maxtilect_remapped-1;

#if 1
		// Re-Sorting.  NOTE: This doesn't actually save space, just makes it easier to reason about.
		RBFOREACH( u32u32, countmap, i )
		{
			tileremap[n] = i->data;
			tilecounts[n] = i->key;
			tileremapfwd[i->data] = n;
			// Display frequencies
			//printf( "Orig: %d  New: %d  Freq: %d\n", i->data, n, i->key );
			//printf( "%d,%d\n", n, i->key );
			n--;
		}
#else
		maxtilect_remapped = maxtilect;
		for( i = 0; i < maxtilect; i++ )
		{
			tileremap[i] = i;
			tileremapfwd[i] = i;
			tilecounts[i] = tilecounts_temp[i];
		}
#endif

		startcellremap = tileremapfwd[startcell];


		for( i = 0; i < MAXTILEDIFF; i++ )
		{
			tilechangesfrom[i] = tileremapfwd[tilechangesfrom[i]];
		}

		{
			fromtofrequencyremap = calloc( 4, maxtilect_remapped*maxtilect_remapped );
			FILE * fDirectionalTransitions = fopen( "directional_transitions.csv", "w" );
			for( i = 0; i < maxtilect_remapped; i++ ) // To
			{
				for( j = 0; j < maxtilect_remapped; j++ ) // From
				{
					int rmj = tileremap[j];
					int rmi = tileremap[i];
					int n = fromtofrequency[rmj*maxtilect+rmi];
					fprintf( fDirectionalTransitions, "%4d,", n );
					fromtofrequencyremap[j*maxtilect_remapped+i] = n;
				}
				fprintf( fDirectionalTransitions, "\n" );
			}
			fclose( fDirectionalTransitions );
		}

		for( i = 0; i < tilechangect; i++ )
		{
			int tin = tilechanges[i];
			int tile = tilechanges[i] = (tin>=0)?tileremapfwd[tin]:tin;

			int tinR = tilechangesReal[i];
			tilechangesReal[i] = tileremapfwd[tinR];

			if( tile >= maxtilect_remapped )
			{
				fprintf( stderr, "Encoding Glyph %d/%d\n", tile, maxtilect_remapped );
				exit( -9 );
			}
			//printf( "%d\n", i );
		}

		glyphsnew = malloc( (maxtilect_remapped)*BLOCKSIZE*BLOCKSIZE*4 );
		for( i = 0; i < maxtilect_remapped; i++ )
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
#ifdef MONITOR_FRAME
	if( 1 )
#else
	if( 0 )
#endif
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

#ifdef MONITOR_FRAME
			if( frame == MONITOR_FRAME )
			{
				for( y = 0; y < BLKY; y++ )
				{
					for( x = 0; x < BLKX; x++ )
					{
						printf( "%4d", curglyph[y][x] );
					}
					printf( "\n" );
				}
			}
#endif

			CNFGSwapBuffers();
			memcpy( prevglyph, curglyph, sizeof( curglyph ) );
		}
	}

#ifdef VPX_CODING_ALLOW_BACKTRACK
	// Allow referencing the previous tile.
	int probbacktrack = 0;
	{
		double chanceof0 = backtrackcount / (double)(tilechangect);
		int prob = chanceof0 * VPX_PROB_MULT - VPX_PROB_SHIFT;
		if( prob < 0 ) prob = 0;
		if( prob > 255 ) prob = 255;
		probbacktrack = prob;
	}
	printf( "Prob Backtrack: %d\n", probbacktrack );
#endif

	int bitsfortileid = intlog2roundup( maxtilect_remapped );

	// Compute the chances-of-tile table.
	// This is a triangular structure.

	float ftheoretical_bits_per_glyph_change = 0;

#ifdef USE_TILE_CLASSES

	// If we have a Power-of-2 number of cells, then we can leave off the last chance in the chancetable.
	int chancetable_len = (maxtilect_remapped == (1<<bitsfortileid)) ? (maxtilect_remapped-1): maxtilect_remapped;


	uint8_t ba_chancetable_glyph_dual[USE_TILE_CLASSES][chancetable_len];
	memset( ba_chancetable_glyph_dual, 0, sizeof(ba_chancetable_glyph_dual) );
	uint8_t selectchancebin[maxtilect_remapped];
	uint8_t ba_exportbinclass[(maxtilect_remapped+1)/2];
	{
		// i = to
		// j = from
		// fromtofrequencyremap[j*maxtilect_remapped+i] = n;
		// Figure out how to sort each of the "froms" into "tos"

		float frequencyset[USE_TILE_CLASSES][maxtilect_remapped];
		memset( frequencyset, 0, sizeof(frequencyset) );

		int count_in_group[USE_TILE_CLASSES] = { 0 };
		// Use the two most common cells as the architypes of the frequencies.
		int from;
		for( from = 0; from < maxtilect_remapped; from++ )
		{
			int bin = 0;

			if( from < USE_TILE_CLASSES )
			{
				bin = from;
			}
			else
			{
				// Otherwise, find which bin we fit into.
				// NOTE: I tried randomly assigning and that does really poorly.
				float binscore[USE_TILE_CLASSES] = { 0 };
				int b;
				for( b = 0; b < USE_TILE_CLASSES; b++ )
				{
					int to;
					int binsum = 0;
					for( to = 0; to < maxtilect_remapped; to++ )
					{
						//printf( "%d*%d,",frequencyset[b][to], fromtofrequencyremap[from*maxtilect_remapped+to] );
						binscore[b] += frequencyset[b][to] * fromtofrequencyremap[from*maxtilect_remapped+to];
						binsum += frequencyset[b][to];
					}
					// TODO: Tweak me!
					//binscore[b] /= binsum;
					// It seems like for the first iteration, not doing this is better.  Try turning this normalization back on some day.
				}

				int bestbin = 0;
				float bestbinscore = 0;
				for( b = 0; b < USE_TILE_CLASSES; b++ )
				{
					if( binscore[b] > bestbinscore )
					{
						bestbinscore = binscore[b];
						bestbin = b;
					}
				}

				bin = bestbin;
			}

			selectchancebin[from] = bin;

			int to;
			for( to = 0; to < maxtilect_remapped; to++ )
			{
				frequencyset[bin][to] += fromtofrequencyremap[from*maxtilect_remapped+to];
			}
		}

		// Perform some k-means iterations on the dataset to anneal the data set.
		int kmeansiter = 20;
		int least_matching_glyph = -1;  // What is the most poorly matching glyph, so if we need to fill in a bin, we can use this one.
		for( i = 0; i < kmeansiter; i++ )
		{
			int to, b;
			for( to = 0; to < maxtilect_remapped; to++ )
				for( b = 0; b < USE_TILE_CLASSES; b++ )
					frequencyset[b][to] = 0;

			for( from = 0; from < maxtilect_remapped; from++ )
			{
				int bin = selectchancebin[from];
				for( to = 0; to < maxtilect_remapped; to++ )
				{
					frequencyset[bin][to] += fromtofrequencyremap[from*maxtilect_remapped+to];
				}
			}

			for( b = 0; b < USE_TILE_CLASSES; b++ )
			{
				float binsum = 0;
				for( to = 0; to < maxtilect_remapped; to++ )
				{
					binsum += frequencyset[b][to];
				}
				for( to = 0; to < maxtilect_remapped; to++ )
				{
					frequencyset[b][to]/=binsum;
				}
				//printf( "%.1f(%d)\n", binsum, least_matching_glyph );
				if( binsum < 1 && least_matching_glyph != -1 )
				{
					// No matching things for this bin.  Pick one at random.
					selectchancebin[least_matching_glyph] = b;
					for( to = 0; to < maxtilect_remapped; to++ )
					{
						binsum += frequencyset[b][to] = fromtofrequencyremap[least_matching_glyph*maxtilect_remapped+to];
					}
					for( to = 0; to < maxtilect_remapped; to++ )
					{
						frequencyset[b][to]/=binsum;
					}
					//printf( "S: %d %.1f\n", least_matching_glyph, binsum );
					least_matching_glyph = -1;
				}
			}
			//printf( "\n" );

			float weakestscore = 1e20;
			for( from = 0; from < maxtilect_remapped; from++ )
			{
				int bin = 0;

				// Otherwise, find which bin we fit into.
				// NOTE: I tried randomly assigning and that does really poorly.
				float binscore[USE_TILE_CLASSES] = { 0 };
				int b;
				for( b = 0; b < USE_TILE_CLASSES; b++ )
				{
					int to;
					for( to = 0; to < maxtilect_remapped; to++ )
					{
						//printf( "%d*%d,",frequencyset[b][to], fromtofrequencyremap[from*maxtilect_remapped+to] );
						binscore[b] += frequencyset[b][to] * fromtofrequencyremap[from*maxtilect_remapped+to];
					}
				}

				int bestbin = 0;
				float bestbinscore = 0;
				for( b = 0; b < USE_TILE_CLASSES; b++ )
				{
					if( binscore[b] > bestbinscore )
					{
						bestbinscore = binscore[b];
						bestbin = b;
					}
				}

				bin = bestbin;

				selectchancebin[from] = bin;
				if( bestbinscore < weakestscore )
					least_matching_glyph = from;
			}
		}

		memset( ba_exportbinclass, 0, sizeof( ba_exportbinclass ) );
		for( from = 0; from < maxtilect_remapped; from++ )
		{
			int bin = selectchancebin[from];
			count_in_group[bin]++;
			ba_exportbinclass[from/2] |= bin<<((from&1)*4);
		}

#if 0
		int grouping;
		for( grouping = 0; grouping < USE_TILE_CLASSES; grouping++ )
		{
			printf( "GROUP: %d %d\n", grouping, count_in_group[grouping] );
		}
#endif

		int bin;
		int to;
		FILE * fk = fopen( "paired.csv", "w" );
		for( to = 0; to < maxtilect_remapped; to++ )
		{
			for( bin = 0; bin < USE_TILE_CLASSES; bin++ )
				fprintf( fk, "%f,", frequencyset[bin][to] );
			fprintf( fk, "\n" );
		}
		fclose( fk );

		for( bin = 0; bin < USE_TILE_CLASSES; bin++ )
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
				int placeinlevel = 0;
				for( maskcheck = 0; maskcheck < maxmask; maskcheck += lincmask )
				{
					float count1 = 0;
					float count0 = 0;
					for( n = 0; n < (1<<bitsfortileid); n++ )
					{
#ifdef PROB_ENDIAN_FLIP
						int tn = flipbits( n, bitsfortileid );
#else
						int tn = n;
#endif
						if( n >= maxtilect_remapped ) continue;

						if( ( tn & levelmask ) == (maskcheck) )
						{
							if( tn & comparemask )
								count1 += frequencyset[bin][n];
							else
								count0 += frequencyset[bin][n];
						}
					}
					double chanceof0 = count0 / (double)(count0 + count1);
					int prob = chanceof0 * VPX_PROB_MULT - VPX_PROB_SHIFT;
					if( prob < 0 ) prob = 0;
					if( prob > 255 ) prob = 255;
					int place = VPXTreePlaceByLevelPlace( level, placeinlevel, bitsfortileid );
					//printf( "Writing: %d (%d %d %d (%.4f %.4f = %d))\n", place, level, placeinlevel, bitsfortileid, count0, count1, prob );
					ba_chancetable_glyph_dual[bin][place] = prob;
					//printf( "%d: %08x %f %f (%d)\n", nout-1, maskcheck, count0, count1, prob );
					placeinlevel++;
				}
			}
		}

		printf( "TODO: Compute ftheoretical_bits_per_glyph_change\n" );
	}
#else
	uint8_t ba_chancetable_glyph[(1<<bitsfortileid)-1];
	memset( ba_chancetable_glyph, 0, sizeof(ba_chancetable_glyph) );
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
				for( n = 0; n < (1<<bitsfortileid); n++ )
				{
#ifdef PROB_ENDIAN_FLIP
					int tn = flipbits( n, bitsfortileid );
#else
					int tn = n;
#endif
					if( n >= maxtilect_remapped ) continue;

					if( ( tn & levelmask ) == maskcheck )
					{
						if( tn & comparemask )
							count1 += tilecounts[n];
						else
							count0 += tilecounts[n];
					}
				}
				double chanceof0 = count0 / (double)(count0 + count1);
				int prob = chanceof0 * VPX_PROB_MULT - VPX_PROB_SHIFT;
				if( prob < 0 ) prob = 0;
				if( prob > 255 ) prob = 255;
				int place = VPXTreePlaceByLevelPlace( level, nout, bitsfortileid );
				ba_chancetable_glyph[place] = prob;
				nout++;
				//printf( "%d: %08x %d %d (%d)\n", nout-1, maskcheck, count0, count1, prob );
			}
		}
	}
#endif

#ifdef SKIP_FIRST_AFTER_TRANSITION
	const int RSOPT = 1;
#else
	const int RSOPT = 0;
#endif

	// Now, we have to compress tilechanges & tileruns
	uint8_t ba_vpx_probs_by_tile_run[maxtilect_remapped];
	uint8_t ba_vpx_probs_by_tile_run_after_one[maxtilect_remapped];
#ifdef RUNCODES_CONTINUOUS
#ifdef RUNCODES_CONTINUOUS_BY_CLASS
	uint8_t ba_vpx_probs_by_tile_run_continuous[USE_TILE_CLASSES][RUNCODES_CONTINUOUS];
#else
	uint8_t ba_vpx_probs_by_tile_run_continuous[RUNCODES_CONTINUOUS];
#endif
#endif
	{
		int probcountmap[maxtilect_remapped];
		int glyphcounts[maxtilect_remapped];
		int probcountmap_after_one[maxtilect_remapped];
		int glyphcounts_after_one[maxtilect_remapped];
		int curtile[BLKY][BLKX];
		int n, bx, by;

		for( n = 0; n < maxtilect_remapped; n++ )
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

#ifdef RUNCODES_CONTINUOUS
#ifdef USE_TILE_CLASSES
		uint32_t runcounts_tile_run_up_until_num[USE_TILE_CLASSES][RUNCODES_CONTINUOUS] = { 0 };
		uint32_t runcounts_tile_run_up_until_denom[USE_TILE_CLASSES][RUNCODES_CONTINUOUS] = { 0 };
#else
		uint32_t runcounts_tile_run_up_until_num[RUNCODES_CONTINUOUS] = { 0 };
		uint32_t runcounts_tile_run_up_until_denom[RUNCODES_CONTINUOUS] = { 0 };
#endif
#endif
		for( n = 0; n < tilechangect; n++ )
		{
			int t = tilechanges[n];
			int lt = tilechangesfrom[n];
			int r = tileruns[n];

			if( t >= 0 )
			{
				int rc = 0;
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
				rc = r+1-RSOPT;
#else
				// This combines probability at any given time.
				glyphcounts[t]++;
				probcountmap[t]+=r+1-RSOPT;
				rc = r+1-RSOPT;
#endif
#ifdef RUNCODES_CONTINUOUS
#ifdef USE_TILE_CLASSES
				int k;
#ifdef TILE_CLASSES_RUNCODE_FORWARD
				int class = selectchancebin[t];
#else
				int class = selectchancebin[lt];
#endif
				for( k = 0; k < rc-1; k++ )
				{
					runcounts_tile_run_up_until_denom[class][k]++;
				}
				if( k < RUNCODES_CONTINUOUS -1 )
				{
					runcounts_tile_run_up_until_num[class][k]++;
					runcounts_tile_run_up_until_denom[class][k]++;
				}
				else
				{
					runcounts_tile_run_up_until_num[class][RUNCODES_CONTINUOUS-1]++;
					runcounts_tile_run_up_until_denom[class][RUNCODES_CONTINUOUS-1] += k - (RUNCODES_CONTINUOUS-1) + 1;
				}
#else
				int k;
				for( k = 0; k < rc-1 && k < RUNCODES_CONTINUOUS; k++ )
				{
					runcounts_tile_run_up_until_denom[k]++;
				}
				if( k < RUNCODES_CONTINUOUS -1 )
				{
					runcounts_tile_run_up_until_num[k]++;
					runcounts_tile_run_up_until_denom[k]++;
				}
				else
				{
					// Make last element the average.
					runcounts_tile_run_up_until_num[RUNCODES_CONTINUOUS-1]++;
					runcounts_tile_run_up_until_denom[RUNCODES_CONTINUOUS-1] += k - (RUNCODES_CONTINUOUS-1) + 1;
				}
#endif
#endif
			}
		}


#ifdef RUNCODES_CONTINUOUS
		int b;
#ifdef USE_TILE_CLASSES
		for( b = 0; b < USE_TILE_CLASSES; b++ )
		{
#else
		{
#endif
			for( n = 0; n < RUNCODES_CONTINUOUS; n++ )
			{
				double gratio;
				int prob;
				//printf( "%d  %d/%d\n", n, runcounts_tile_run_up_until_num[n],runcounts_tile_run_up_until_denom[n] );
#ifdef USE_TILE_CLASSES
				gratio = runcounts_tile_run_up_until_num[b][n] * 1.0 / runcounts_tile_run_up_until_denom[b][n];
#else
				gratio = runcounts_tile_run_up_until_num[n] * 1.0 / runcounts_tile_run_up_until_denom[n];
#endif
				prob = gratio * VPX_PROB_MULT - VPX_PROB_SHIFT;
				if( prob < 0 ) prob = 0; 
				if( prob > 255 ) prob = 255;
#ifdef RUNCODES_CONTINUOUS_BY_CLASS
				ba_vpx_probs_by_tile_run_continuous[b][n] = prob;
#else
				ba_vpx_probs_by_tile_run_continuous[n] = prob;
#endif
			}


		}
#else
		for( n = 0; n < maxtilect_remapped; n++ )
		{
			double gratio;
			int prob;
			gratio = glyphcounts[n] * 1.0 / probcountmap[n];
			prob = gratio * VPX_PROB_MULT - VPX_PROB_SHIFT;
			if( prob < 0 ) prob = 0; 
			if( prob > 255 ) prob = 255;
			ba_vpx_probs_by_tile_run[n] = prob;

#ifdef RUNCODES_TWOLEVEL
			gratio = glyphcounts_after_one[n] * 1.0 / probcountmap_after_one[n];
			prob = gratio * VPX_PROB_MULT - VPX_PROB_SHIFT
			if( prob < 0 ) prob = 0; 
			if( prob > 255 ) prob = 255;
			ba_vpx_probs_by_tile_run_after_one[n] = prob;
#endif
		}
#endif


	}


	// Just a quick test to see if we can compress the tilemaps.
	FILE * fRawTiles = fopen( "TEST_rawtiles.dat", "wb" );
#if defined( VPX_GREY4 )
	#define BITSETS_TILECOMP 2
#elif defined( VPX_GREY16 )
	#define BITSETS_TILECOMP 4
#else
	#define BITSETS_TILECOMP 1
#endif
	int bnw[BLOCKSIZE*BLOCKSIZE*maxtilect_remapped*BITSETS_TILECOMP];
	for( i = 0; i < maxtilect_remapped; i++ )
	{
		float * fg = &glyphsnew[i*BLOCKSIZE*BLOCKSIZE];
		int y, x;
		for( y = 0; y < BLOCKSIZE; y++ )
		{
			uint32_t osym = 0;
			for( x = 0; x < BLOCKSIZE; x++ )
			{
				int c = (int)(fg[x+y*BLOCKSIZE] * 255) >> (8-BITSETS_TILECOMP);
				osym = (osym<<BITSETS_TILECOMP) | c;

				int k;
				for( k = 0; k < BITSETS_TILECOMP; k++ )
				{
					bnw[((x+y*BLOCKSIZE)+i*BLOCKSIZE*BLOCKSIZE)*BITSETS_TILECOMP + k] = (c>>(BITSETS_TILECOMP-k-1))&1;
				}
			}
			fwrite( &osym, BITSETS_TILECOMP*BLOCKSIZE/8, 1, fRawTiles );
		}
	}
	fclose( fRawTiles );

	uint8_t vpx_glyph_tiles_buffer[1024*256];
	int vpx_glyph_tiles_buffer_len = 0;
	uint8_t ba_vpx_glyph_probability_run_0_or_1[2][MAXPIXELRUNTOSTORE];
	vpx_writer w_glyphdata = { 0 };
#if defined( SIMGREY4 )
	{
		
	}
#else
	{
		int runsets0to0[MAXPIXELRUNTOSTORE] = { 0 };
		int runsets0to1[MAXPIXELRUNTOSTORE] = { 0 };
		int runsets1to0[MAXPIXELRUNTOSTORE] = { 0 };
		int runsets1to1[MAXPIXELRUNTOSTORE] = { 0 };
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE*maxtilect_remapped*BITSETS_TILECOMP; i+=BITSETS_TILECOMP )
		{
			int color = bnw[i];
			int j;
			int b = 0;
			for( j = i+1; j < BLOCKSIZE*BLOCKSIZE*maxtilect_remapped*BITSETS_TILECOMP; j+=BITSETS_TILECOMP )
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
				if( b == MAXPIXELRUNTOSTORE ) break;
			}
		}
		for( int j = 0; j < MAXPIXELRUNTOSTORE; j++ )
		{
			double chanceof0 = runsets0to0[j] / (double)(runsets0to0[j]+runsets0to1[j]);
			int prob = chanceof0 * VPX_PROB_MULT - VPX_PROB_SHIFT;
			if( prob < 0 ) prob = 0;
			if( prob > 255 ) prob = 255;
			int prob0 = ba_vpx_glyph_probability_run_0_or_1[0][j] = prob;

			chanceof0 = runsets1to0[j] / (double)(runsets1to0[j]+runsets1to1[j]);
			prob = chanceof0 * VPX_PROB_MULT - VPX_PROB_SHIFT;
			if( prob < 0 ) prob = 0;
			if( prob > 255 ) prob = 255;
			int prob1 = ba_vpx_glyph_probability_run_0_or_1[1][j] = prob;
			//printf( "%d (%d/%d) (%d/%d) %4d%4d\n", j, runsets0to0[j], runsets0to1[j], runsets1to0[j], runsets1to1[j] , prob0, prob1 );
		}

		vpx_start_encode( &w_glyphdata, vpx_glyph_tiles_buffer, sizeof(vpx_glyph_tiles_buffer));

		int runsofar = 0;
		int is0or1 = 0;

		for( i = 0; i < BLOCKSIZE*BLOCKSIZE*maxtilect_remapped; i+=1 )
		{
			uint8_t * prob = ba_vpx_glyph_probability_run_0_or_1[is0or1];
			int tprob = prob[runsofar];

			if( ( (i) & ((BLOCKSIZE*BLOCKSIZE)-1)) == 0 )
			{
				tprob = 128;
				runsofar = MAXPIXELRUNTOSTORE-1;
			}


			int color = bnw[i*2];
			//printf( "%d: %d\n", color, tprob );
			vpx_write(&w_glyphdata, color, tprob );
			//static FILE * fCheck; if( !fCheck ) fCheck = fopen( "check.csv", "w" );
			//fprintf( fCheck, "%d,%d\n", color, tprob );

			int subpixel;
			for( subpixel = 1; subpixel < BITSETS_TILECOMP; subpixel++ )
			{
				int cpx = bnw[i*2+subpixel];
				vpx_write(&w_glyphdata, cpx, color?GSC1:GSC0 );
				//fprintf( fCheck, "%d,%d\n", cpx, color?GSC1:GSC0 );
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

		vpx_stop_encode(&w_glyphdata);
		vpx_glyph_tiles_buffer_len = w_glyphdata.pos;
	}
#endif


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

		uint32_t tileids[maxtilect_remapped];
		uint32_t frequencies[maxtilect_remapped];

		int i;
		for( i = 0; i < maxtilect_remapped; i++ )
		{
			tileids[i] = i;
			frequencies[i] = 0;
		}

		for( i = 0; i < tilechangect; i++ )
		{
			frequencies[tilechangesReal[i]]++;
		}

		huffelement * tree = GenerateHuffmanTree( tileids, frequencies, maxtilect_remapped, &hufftreelen );

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

		int idsofhu[maxtilect_remapped];
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

#ifndef INVERT_RUNCODE_COMPRESSION
	for( n = 0; n < tilechangect; n++ )
	{
		int tile = tilechanges[n];
		int tileLast = tilechangesfrom[n];
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

#ifdef USE_TILE_CLASSES
			int fromclass = selectchancebin[tilechangesfrom[n]];
#endif

			int ut = tile;
#ifdef PROB_ENDIAN_FLIP
			ut = flipbits( ut, bitsfortileid );
#endif

			for( level = 0; level < bitsfortileid; level++ )
			{
				int comparemask = 1<<(bitsfortileid-level-1); //i.e. 0x02 one fewer than the levelmask
				int bit = !!(ut & comparemask);
#ifdef USE_TILE_CLASSES
				probability = ba_chancetable_glyph_dual[fromclass][probplace];
#else
				probability = ba_chancetable_glyph[probplace];
#endif

				vpx_write(&w_glyphs, bit, probability);
#ifndef VPX_USE_HUFFMAN_TILES
				vpx_write(&w_combined, bit, probability);
#endif
				if( bit )
					probplace += 1<<(bitsfortileid-level-1);
				else
					probplace++;
			}
		}

		int run = tileruns[n] - RSOPT;


#ifdef RUNCODES_CONTINUOUS
		int b;
		//printf( "%d ", run );
#if RUNCODES_CONTINUOUS_BY_CLASS
#ifdef TILE_CLASSES_RUNCODE_FORWARD
		int class = selectchancebin[tile];
#else
		int class = selectchancebin[tileLast];
#endif
		for( b = 0; b < run; b++ )
		{
			probability = ba_vpx_probs_by_tile_run_continuous[class][(b < RUNCODES_CONTINUOUS - 1)?b:(RUNCODES_CONTINUOUS - 1)];
			//printf( "%d ", probability );
			vpx_write(&w_run, 1, probability);
			vpx_write(&w_combined, 1, probability);
		}
		probability = ba_vpx_probs_by_tile_run_continuous[class][(b < RUNCODES_CONTINUOUS - 1)?b:(RUNCODES_CONTINUOUS - 1)];
		//printf( "%d\n", probability );
		vpx_write(&w_run, 0, probability);
		vpx_write(&w_combined, 0, probability);
#else
		for( b = 0; b < run; b++ )
		{
			probability = ba_vpx_probs_by_tile_run_continuous[(b < RUNCODES_CONTINUOUS - 1)?b:(RUNCODES_CONTINUOUS - 1)];
			//printf( "%d ", probability );
			vpx_write(&w_run, 1, probability);
			vpx_write(&w_combined, 1, probability);
		}
		probability = ba_vpx_probs_by_tile_run_continuous[(b < RUNCODES_CONTINUOUS - 1)?b:(RUNCODES_CONTINUOUS - 1)];
		//printf( "%d\n", probability );
		vpx_write(&w_run, 0, probability);
		vpx_write(&w_combined, 0, probability);
#endif
#else
		probability = ba_vpx_probs_by_tile_run[tileReal];
		int b;
		for( b = 0; b < run; b++ )
		{
			vpx_write(&w_run, 1, probability);
			vpx_write(&w_combined, 1, probability);
#ifdef RUNCODES_TWOLEVEL
			probability = ba_vpx_probs_by_tile_run_after_one[tileReal];
#endif
		}
		vpx_write(&w_run, 0, probability);
		vpx_write(&w_combined, 0, probability);
#endif

		symsum++;
	}

#else

	{
		int curtiles[BLKY][BLKX];
		int runs[BLKY][BLKX];
		int y, x, frame;
		for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				curtiles[y][x] = startcellremap;
				runs[y][x] = 0;
			}

		for( frame = 0; frame < frames; frame++ )
		{
			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				int t = tiles[(x+y*(BLKX)) + BLKX*BLKY * (frame + 0)];
				int newtile = tileremapfwd[t];
				int oldtile = curtiles[y][x];

				int class = selectchancebin[oldtile];

				int b = runs[y][x];
				int probability = ba_vpx_probs_by_tile_run_continuous[class][(b < RUNCODES_CONTINUOUS - 1)?b:(RUNCODES_CONTINUOUS - 1)];

				if( oldtile == newtile )
				{
					vpx_write(&w_run, 1, probability);
					vpx_write(&w_combined, 1, probability);
					// Keep emitting 1 for run.
					b++;
					if( b > RUNCODES_CONTINUOUS-1 )
						b = RUNCODES_CONTINUOUS-1;
					runs[y][x] = b;
				}
				else
				{
					vpx_write(&w_run, 0, probability);
					vpx_write(&w_combined, 0, probability);

					symsum++;
					int ut = newtile;
#ifdef PROB_ENDIAN_FLIP
					ut = flipbits( ut, bitsfortileid );
#endif
					int probplace = 0;
					int level;
					for( level = 0; level < bitsfortileid; level++ )
					{
						int comparemask = 1<<(bitsfortileid-level-1); //i.e. 0x02 one fewer than the levelmask
						int bit = !!(ut & comparemask);

						probability = ba_chancetable_glyph_dual[class][probplace];
						vpx_write(&w_glyphs, bit, probability);
						vpx_write(&w_combined, bit, probability);

						if( bit )
							probplace += 1<<(bitsfortileid-level-1);
						else
							probplace++;
					}

					curtiles[y][x] = newtile;
					runs[y][x] = 0;
				}
			}
		}
	}
#endif

	vpx_stop_encode(&w_glyphs);
	int glyphbytes = w_glyphs.pos;

	vpx_stop_encode(&w_run);
	int runbytes = w_run.pos;

	vpx_stop_encode(&w_combined);
	int combinedstream = w_combined.pos;

	printf( "      Glyphs:%7d\n", maxtilect_remapped );
	printf( " Num Changes:%7d\n", symsum );
#ifdef SMART_TRANSITION_SKIP
	printf( "   Ratcheted:%7d\n", ratcheted );
#endif
	printf( "      Stream:%7d bits / bytes:%6d (%.3f bits per glyph change, %.3f theoretical)\n", glyphbytes*8, glyphbytes, glyphbytes*8.0/symsum, ftheoretical_bits_per_glyph_change );
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



#ifdef USE_TILE_CLASSES
	printf( " +Tile Class:%7d bits / bytes:%6d\n", (int)sizeof(ba_exportbinclass)*8, (int)sizeof(ba_exportbinclass) );
	printf( " + Tile Prob:%7d bits / bytes:%6d\n", (int)sizeof(ba_chancetable_glyph_dual) * 8, (int)sizeof(ba_chancetable_glyph_dual) );
	sum += (int)sizeof(ba_chancetable_glyph_dual) + sizeof(ba_exportbinclass);
#else
	printf( " + Tile Prob:%7d bits / bytes:%6d\n", (int)sizeof(ba_chancetable_glyph) * 8, (int)sizeof(ba_chancetable_glyph) );
	sum += (int)sizeof(ba_chancetable_glyph);
#endif

#ifdef RUNCODES_CONTINUOUS
	printf( " +  Run Prob:%7d bits / bytes:%6d\n", (int)sizeof(ba_vpx_probs_by_tile_run_continuous) * 8, (int)sizeof(ba_vpx_probs_by_tile_run_continuous) );
	sum += (int)RUNCODES_CONTINUOUS;
#else
	printf( " +  Run Prob:%7d bits / bytes:%6d\n", (int)sizeof(ba_vpx_probs_by_tile_run) * 8, (int)sizeof(ba_vpx_probs_by_tile_run) );
	sum += (int)sizeof(ba_vpx_probs_by_tile_run);
#ifdef RUNCODES_TWOLEVEL
	printf( " + Run Prob2:%7d bits / bytes:%6d\n", (int)sizeof(ba_vpx_probs_by_tile_run_after_one) * 8, (int)sizeof(ba_vpx_probs_by_tile_run_after_one) );
	sum += (int)sizeof(ba_vpx_probs_by_tile_run_after_one);
#endif
#endif
	int glyphsize = BLOCKSIZE * BLOCKSIZE / 8 * maxtilect_remapped;
#ifdef VPX_GREY4
	glyphsize *= 2;
#elif defined( VPX_GREY16 )
	glyphsize *= 4;
#endif

	if( vpx_glyph_tiles_buffer_len )
	{
		printf( " +COMPGlyphs:%7d bits / bytes:%6d\n", (vpx_glyph_tiles_buffer_len + (int)sizeof(ba_vpx_glyph_probability_run_0_or_1)) * 8, (vpx_glyph_tiles_buffer_len + (int)sizeof(ba_vpx_glyph_probability_run_0_or_1)) );
		//printf( " N/A  Glyphs:%7d bits / bytes:%6d\n", glyphsize * 8, glyphsize );
		//printf( " N/A   CDATA:%7d bits / bytes:%6d\n", (int)sizeof(ba_vpx_glyph_probability_run_0_or_1) * 8, (int)sizeof(ba_vpx_glyph_probability_run_0_or_1) );
		sum += (vpx_glyph_tiles_buffer_len + (int)sizeof(ba_vpx_glyph_probability_run_0_or_1));
	}
	else
	{
		printf( " +    Glyphs:%7d bits / bytes:%6d\n", glyphsize * 8, glyphsize );
		sum += glyphsize;
	}

	int sHuffD = FileLength( "../song/huffD_fmraw.dat" );
	int sHuffTL = FileLength( "../song/huffTL_fmraw.dat" );
	int sHuffTN = FileLength( "../song/huffTN_fmraw.dat" );
	int soundsum = 0;
	if( sHuffD > 0 )
	{
		printf( " + Sound (D):%7d bits / bytes:%6d\n", sHuffD * 8, sHuffD );
		soundsum += sHuffD;
	}
	if( sHuffTL > 0 )
	{
		printf( " + Sound (L):%7d bits / bytes:%6d\n", sHuffTL * 8, sHuffTL );
		soundsum += sHuffTL;
	}
	if( sHuffTN > 0 )
	{
		printf( " + Sound (N):%7d bits / bytes:%6d\n", sHuffTN * 8, sHuffTN );
		soundsum += sHuffTN;
	}

	printf( "\n" );
	//printf( " Video: %6d Bytes (%d bits) (%.3f bits/frame) (%d frames)\n", sum, sum*8, sum*8.0/frames, frames );
	sum += soundsum;
	printf( " Total: %6d Bytes (%d bits) (%.3f bits/frame) (%d frames)\n", sum, sum*8, sum*8.0/frames, frames );

	// test validate
	if( 1 )
	{
		if( vpx_glyph_tiles_buffer_len )
		{

			//uint8_t vpx_glyph_tiles_buffer[1024*32];
			//int vpx_glyph_tiles_buffer_len = 0;
			//#define MAXPIXELRUNTOSTORE 8
			//uint8_t ba_vpx_glyph_probability_run_0_or_1[2][MAXPIXELRUNTOSTORE];
			vpx_reader reader_tiles;
			vpx_reader_init(&reader_tiles, vpx_glyph_tiles_buffer, vpx_glyph_tiles_buffer_len, 0, 0 );

			// Decompress glyph data.
			int runsofar = 0;
			int is0or1 = 0;
			int n = 0;
			int k = 0;
			int crun = 0;

			for( i = 0; i < BLOCKSIZE*BLOCKSIZE*maxtilect_remapped; i++ )
			{
				uint8_t * prob = ba_vpx_glyph_probability_run_0_or_1[is0or1];
				int tprob = prob[runsofar];

				if( ( (i) & ((BLOCKSIZE*BLOCKSIZE)-1)) == 0 )
				{
					tprob = 128;
					runsofar = MAXPIXELRUNTOSTORE-1;
				}
				int color = vpx_read( &reader_tiles, tprob );
				//static FILE * fVerify; if( !fVerify ) fVerify = fopen( "verify.csv", "w" );
				//fprintf( fVerify, "%d,%d\n", color, tprob );

				int subpixel;
				crun |= (color ? 1 : 0) << (BITSETS_TILECOMP-k-1);
				for( subpixel = 1; subpixel < BITSETS_TILECOMP; subpixel++ )
				{
					int lc = vpx_read(&reader_tiles, color?GSC1:GSC0 );
					//fprintf( fVerify, "%d,%d\n", lc, color?GSC1:GSC0 );
					k++;
					crun |= (lc ? 1 : 0) << (BITSETS_TILECOMP-k-1);
				}

				glyphsnew[n++] = ((float)crun) / ((1<<BITSETS_TILECOMP)-1);
				k = 0;
				crun = 0;

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



		CNFGClearFrame();
		int curglyph[BLKY][BLKX] = { 0 };
		int prevglyph[BLKY][BLKX] = { 0 };
		int currun[BLKY][BLKX] = { 0 };
		int playptr = 0;
		int frame;
		int x, y;

		for( y = 0; y < BLKY; y++ )
		for( x = 0; x < BLKX; x++ )
		{
			curglyph[y][x] = startcellremap;
		}

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
#ifdef INVERT_RUNCODE_COMPRESSION
				int fromglyph = curglyph[y][x];
				int fromclass = (ba_exportbinclass[fromglyph/2] >> ((fromglyph&1)*4))&0xf;
				int run = currun[y][x];
				int probability = ba_vpx_probs_by_tile_run_continuous[fromclass][(run < RUNCODES_CONTINUOUS - 1)?run:(RUNCODES_CONTINUOUS - 1)];
				int level;
				int tile;
				static int kk;

				if( vpx_read( &reader, probability ) == 1 )
				{
					// keep going
					run = run + 1;
					if( run > RUNCODES_CONTINUOUS-1 )
						run = RUNCODES_CONTINUOUS-1;
					currun[y][x] = run;
				}
				else
				{
					// Have a new thing.
					int probplace = 0;
					tile = 0;
					for( level = 0; level < bitsfortileid; level++ )
					{

#ifdef USE_TILE_CLASSES
						probability = ba_chancetable_glyph_dual[fromclass][probplace];
#else
						probability = ba_chancetable_glyph[probplace];
#endif
						int bit = vpx_read( &reader, probability );
						tile |= bit<<(bitsfortileid-level-1);
						if( bit )
							probplace += 1<<(bitsfortileid-level-1);
						else
							probplace++;
					}
					curglyph[y][x] = tile;
					currun[y][x] = 0;
				}

#else // !INVERT_RUNCODE_COMPRESSION
				if( currun[y][x] == 0 )
				{
					int tile = 0;

					int bitsfortileid = intlog2roundup( maxtilect_remapped );
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

					int fromglyph = curglyph[y][x];
#ifdef USE_TILE_CLASSES
					int fromclass = (ba_exportbinclass[fromglyph/2] >> ((fromglyph&1)*4))&0xf;
#endif

					{
						int probplace = 0;
						for( level = 0; level < bitsfortileid; level++ )
						{
#ifdef USE_TILE_CLASSES
							probability = ba_chancetable_glyph_dual[fromclass][probplace];
#else
							probability = ba_chancetable_glyph[probplace];
#endif
							int bit = vpx_read( &reader, probability );
							tile |= bit<<(bitsfortileid-level-1);
							if( bit )
								probplace += 1<<(bitsfortileid-level-1);
							else
								probplace++;
						}
					}
#endif

					curglyph[y][x] = tile;

#ifdef USE_TILE_CLASSES
#ifdef TILE_CLASSES_RUNCODE_FORWARD
					int runclass = (ba_exportbinclass[tile/2] >> ((tile&1)*4))&0xf;
#else
					int runclass = (ba_exportbinclass[fromglyph/2] >> ((fromglyph&1)*4))&0xf;
#endif
#endif

#ifdef RUNCODES_CONTINUOUS
					int run = 0;
					do
					{
#ifdef RUNCODES_CONTINUOUS_BY_CLASS
						probability = ba_vpx_probs_by_tile_run_continuous[runclass][(run < RUNCODES_CONTINUOUS - 1)?run:(RUNCODES_CONTINUOUS - 1)];
#else
						probability = ba_vpx_probs_by_tile_run_continuous[(run < RUNCODES_CONTINUOUS - 1)?run:(RUNCODES_CONTINUOUS - 1)];
#endif
						if( vpx_read(&reader, probability) == 0 ) break;
						run++;
					}
					while( true );
#else

					probability = ba_vpx_probs_by_tile_run[tile];
					int run = 0;
					while( vpx_read(&reader, probability) )
					{
#ifdef RUNCODES_TWOLEVEL
						probability = ba_vpx_probs_by_tile_run_after_one[tile];
#endif
						run++;
					}
#endif
					currun[y][x] = run + RSOPT;
					playptr++;

				}
				else
				{
					currun[y][x]--;
				}
#endif
			}

			for( y = 0; y < BLKY; y++ )
			for( x = 0; x < BLKX; x++ )
			{
				VPDrawGlyph( x*BLOCKSIZE*2, y*BLOCKSIZE*2, x, y, curglyph, prevglyph );
				VPBlockDrawGif( gifout, x * BLOCKSIZE*2, y * BLOCKSIZE*2, RESX*2, x, y, curglyph, prevglyph );
			}

#ifdef MONITOR_FRAME
			if( frame == MONITOR_FRAME )
			{
				for( y = 0; y < BLKY; y++ )
				{
					for( x = 0; x < BLKX; x++ )
					{
						printf( "%4d", curglyph[y][x] );
					}
					printf( "\n" );
				}
				exit( 0 );
			}
#endif

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

	FILE * fDataOut = fopen( "badapple_data.h", "w" );
	fprintf( fDataOut, "#ifndef BADAPPLE_DATA_H\n" );
	fprintf( fDataOut, "#define BADAPPLE_DATA_H\n" );
	fprintf( fDataOut, "\n" );
	fprintf( fDataOut, "#define VPXCODING_READER\n" );
	fprintf( fDataOut, "#define VPX_32BIT\n" );
	fprintf( fDataOut, "#include \"vpxcoding.h\"\n" );
	fprintf( fDataOut, "\n" );
	fprintf( fDataOut, "#define RESX %d\n", RESX );
	fprintf( fDataOut, "#define RESY %d\n", RESY );
	fprintf( fDataOut, "#define BLOCKSIZE %d\n", BLOCKSIZE );
	fprintf( fDataOut, "\n" );
	fprintf( fDataOut, "#define BLKX (RESX/BLOCKSIZE)\n" );
	fprintf( fDataOut, "#define BLKY (RESY/BLOCKSIZE)\n" );
	fprintf( fDataOut, "\n" );
	fprintf( fDataOut, "#define INITIALIZE_CELL %d\n", startcellremap );
	fprintf( fDataOut, "\n" );
#ifdef RUNCODES_CONTINUOUS
	fprintf( fDataOut, "#define RUNCODES_CONTINUOUS %d\n", RUNCODES_CONTINUOUS );
	fprintf( fDataOut, "\n" );
#endif
#ifdef RUNCODES_TWOLEVEL
	fprintf( fDataOut, "#define RUNCODES_TWOLEVEL %d\n", RUNCODES_TWOLEVEL );
	fprintf( fDataOut, "\n" );
#endif
#ifdef MAXPIXELRUNTOSTORE
	fprintf( fDataOut, "#define MAXPIXELRUNTOSTORE %d\n", MAXPIXELRUNTOSTORE );
	fprintf( fDataOut, "\n" );
#endif

#ifdef USE_TILE_CLASSES
	fprintf( fDataOut, "#define USE_TILE_CLASSES %d\n", USE_TILE_CLASSES );
	fprintf( fDataOut, "\n" );
#endif

	fprintf( fDataOut, "#define TILE_COUNT %d\n", maxtilect_remapped );
	fprintf( fDataOut, "#define BITS_FOR_TILE_ID %d\n\n", bitsfortileid );

	fprintf( fDataOut, "// Sound\n\n" );

	WriteOutFile( fDataOut, "sound_huffTL", "../song/huffTL_fmraw.dat" );
	WriteOutFile( fDataOut, "sound_huffTN", "../song/huffTN_fmraw.dat" );
	WriteOutFile( fDataOut, "sound_huffD", "../song/huffD_fmraw.dat" );

	fprintf( fDataOut, "// Video\n\n" );

	// Output stream 
#ifdef USE_TILE_CLASSES
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_exportbinclass[%d] = {", (int)sizeof(ba_exportbinclass) );
	for( j = 0; j < sizeof(ba_exportbinclass); j++ )
	{
		fprintf( fDataOut, "%s0x%02x,", ((j & 0xf) == 0x0) ? "\n\t" : " ", ba_exportbinclass[j] );
	}
	fprintf( fDataOut, "\n};\n\n" );

	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_chancetable_glyph_dual[%d][%d] = {", USE_TILE_CLASSES, chancetable_len );
	for( i = 0; i < USE_TILE_CLASSES; i++ )
	{
		fprintf( fDataOut, "\n\t{ " );
		for( j = 0; j < chancetable_len; j++ )
		{
			fprintf( fDataOut, "%s%3d,", ((j & 0xf) == 0x0) ? "\n\t" : " ", ba_chancetable_glyph_dual[i][j] );
		}
		fprintf( fDataOut, "}," );
	}
	fprintf( fDataOut, "\n};\n\n" );
#else
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_chancetable_glyph[%d] = {", (int)sizeof(ba_chancetable_glyph) );
	for( j = 0; j < sizeof(ba_chancetable_glyph); j++ )
	{
		fprintf( fDataOut, "%s%3d,", ((j & 0xf) == 0x0) ? "\n\t" : " ", ba_chancetable_glyph[j] );
	}
	fprintf( fDataOut, "\n};\n\n" );
#endif

#ifdef RUNCODES_CONTINUOUS
#ifdef RUNCODES_CONTINUOUS_BY_CLASS
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_vpx_probs_by_tile_run_continuous[USE_TILE_CLASSES][RUNCODES_CONTINUOUS] = {" );
	for( j = 0; j < USE_TILE_CLASSES; j++ )
	{
		fprintf( fDataOut, "\n\t{ " );
		for( i = 0; i < RUNCODES_CONTINUOUS; i++ )
		{
			fprintf( fDataOut, "%3d,", ba_vpx_probs_by_tile_run_continuous[j][i] );
		}
		fprintf( fDataOut, "}%s", (j < (USE_TILE_CLASSES-1)) ? "," : "" );
	}
	fprintf( fDataOut, "\n};\n\n" );
#else
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_vpx_probs_by_tile_run_continuous[RUNCODES_CONTINUOUS] = {" );
	for( j = 0; j < RUNCODES_CONTINUOUS; j++ )
	{
		fprintf( fDataOut, "%s%3d,", ((j & 0xf) == 0) ? "\n\t" : " ", ba_vpx_probs_by_tile_run_continuous[j] );
	}
	fprintf( fDataOut, "\n};\n\n" );
#endif
#else
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_vpx_probs_by_tile_run[%d] = {", maxtilect_remapped );
	for( j = 0; j < maxtilect_remapped; j++ )
	{
		fprintf( fDataOut, "%s%3d,",  ((j & 0xf) == 0x0) ? "\n\t" : " ", ba_vpx_probs_by_tile_run[j] );
	}
	fprintf( fDataOut, "\n};\n\n" );
#ifdef RUNCODES_TWOLEVEL
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_ba_vpx_probs_by_tile_run_after_one[%d] = {", maxtilect_remapped );
	for( j = 0; j < maxtilect_remapped; j++ )
	{
		fprintf( fDataOut, "%s%3d,", ((j & 0xf) == 0x0) ? "\n\t" : " ", ba_vpx_probs_by_tile_run_after_one[j] );
	}
	fprintf( fDataOut, "\n};\n\n" );
#endif
#endif

#ifdef MAXPIXELRUNTOSTORE
	fprintf( fDataOut, "// For glyph pixel data\n" );
	fprintf( fDataOut, "BADATA_DECORATOR uint8_t ba_vpx_glyph_probability_run_0_or_1[2][MAXPIXELRUNTOSTORE] = {" );
	for( j = 0; j < 2; j++ )
	{
		fprintf( fDataOut, "\n\t{ " );
		for( i = 0; i < MAXPIXELRUNTOSTORE; i++ )
		{
			fprintf( fDataOut, "%s%3d,", ((i & 0x1f) == 0x1f) ? "\n\t" : " ", ba_vpx_glyph_probability_run_0_or_1[j][i] );
		}
		fprintf( fDataOut, "}%s", (j < 1) ? "," : "" );
	}
	fprintf( fDataOut, "\n};\n\n" );
#endif

	WriteFileStreamHeader( fDataOut, &w_combined, "ba_video_payload" );
	WriteFileStreamHeader( fDataOut, &w_glyphdata, "ba_glyphdata" );

	fprintf( fDataOut, "#endif\n" );
	fclose( fDataOut );
}

