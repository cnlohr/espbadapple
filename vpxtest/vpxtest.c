#include <stdio.h>

#define VPXCODING_WRITER
#include "vpxcoding.h"

#define CNRBTREE_IMPLEMENTATION
#include "cnrbtree.h"

#include "../comp2/bacommon.h"

typedef uint64_t u64;
typedef uint32_t u32;

// Generate a u32/u32 multimap.
CNRBTREETEMPLATE( u32, u32, RBptrcmpnomatch, RBptrcpy, RBnullop );

int * tiles = 0;
int numtiles = 0;

#define BLKX (RESX/BLOCKSIZE)
#define BLKY (RESY/BLOCKSIZE)

#define MAXTILEDEPTH 4096

int tilechangesqty[BLKY][BLKX] = { 0 };
int tilechangesto[BLKY][BLKX][MAXTILEDEPTH] = { 0 };
int tilechangesrle[BLKY][BLKX][MAXTILEDEPTH] = { 0 };  // [depth] = how long to hold [depth-1] of tilechangesto.
int tilechangerun[BLKY][BLKX] = { 0 };
int maxtileid_remapped = 0;
uint8_t bufferVPX[1024*1024];

int intlog2 (int x){return __builtin_ctz (x);}

int main()
{
	int i;
	int maxtileid = 0; // pre-remapped

	FILE * f = fopen( "../comp2/stream-64x48x8.dat", "rb" );
	while( !feof( f ) )
	{
		uint32_t tile;
		if( !fread( &tile, 1, 4, f ) )
		{
			fprintf( stderr, "Error reading tile\n" );
		}
		tiles = realloc( tiles, (numtiles+1)*4 );
		tiles[numtiles] = tile;
		numtiles++;
		if( tile > maxtileid ) maxtileid = tile;
	}

	int * tilecounts = calloc( 4, maxtileid );   // [newid]
	int * tileremap = calloc( 4, maxtileid );    // [newid] = originalid
	int * tileremapfwd = calloc( 4, maxtileid ); // [originalid] = newid

	int maxrun = 0;
	int frames = numtiles / BLKX / BLKY;

	{
		int * tilecounts_temp = alloca( 4 * maxtileid );
		memset( tilecounts_temp, 0, 4 * maxtileid );

		int init_tile[BLKY][BLKX] = { -1 };
		int x, y, frame;
		int tid = 0;
		for( frame = 0; frame < frames; frame++ )
		for( y = 0; y < BLKY; y++ )
		for( x = 0; x < BLKX; x++ )
		{
			int t = tiles[tid++];
			int ct = init_tile[y][x];
			if( t != ct && frame != 0 )
			{
				int pl = tilechangesqty[y][x];
				tilechangesto[y][x][pl] = t;
				tilecounts_temp[t]++;
				init_tile[y][x] = t;
				int run = tilechangesrle[y][x][pl] = tilechangerun[y][x] - 1;
				if( run > maxrun ) maxrun = run;
				tilechangesqty[y][x] = pl+1;
				tilechangerun[y][x] = 0;
			}
			tilechangerun[y][x]++;
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
	}


	// Tile run frequency.
	int tilerunfreq[maxtileid][maxrun];
	memset( tilerunfreq, 0, sizeof( tilerunfreq ) );

	// Update the tilechangesto map with new tile IDs
	int x, y, dep;
	for( y = 0; y < BLKY; y++ )
	{
		for( x = 0; x < BLKX; x++ )
		{
			int td = tilechangesqty[y][x];
			for( dep = 0; dep < td; dep++ )
			{
				int tid = tilechangesto[y][x][dep] = tileremapfwd[tilechangesto[y][x][dep]];

				int pdid = 0;
				if( dep > 0 )
					pdid = tilechangesto[y][x][dep-1];

				int run = tilechangesrle[y][x][dep];
				//printf( "%d %d %d -> %d,%d  %d,%d\n", x, y, dep, tid,run, maxtileid, maxrun );
				tilerunfreq[pdid][run]++;
			}
		}
	}

	
	{
		FILE * fRunFreqs = fopen( "runfreqs.csv", "w" );
		fprintf( fRunFreqs, "Run," );
		for( x = 0; x < maxtileid; x++ )
		{
			fprintf( fRunFreqs, "%d%c", x, (x==maxtileid-1)?'\n':',' );
		}

		fprintf( fRunFreqs, "Sum," );
		for( x = 0; x < maxtileid; x++ )
		{
			int tsum = 0;
			for( y = 0; y < maxrun; y++ )
			{
				tsum += tilerunfreq[x][y] * y;
			}
			fprintf( fRunFreqs, "%.3f%c", tsum/(double)tilecounts[x], (x==maxtileid-1)?'\n':',' );
		}

		fprintf( fRunFreqs, "Counts," );
		for( x = 0; x < maxtileid; x++ )
		{
			fprintf( fRunFreqs, "%d%c", tilecounts[x], (x==maxtileid-1)?'\n':',' );
		}

		for( y = 0; y < maxrun; y++ )
		{
			fprintf( fRunFreqs, "%d,", y );
			for( x = 0; x < maxtileid; x++ )
			{
				//printf( "%d %d  %d %d\n", x, y, maxtileid, maxrun );
				fprintf( fRunFreqs, "%d%c", tilerunfreq[x][y], (x==maxtileid-1)?'\n':',' );
			}
		}
		fclose( fRunFreqs );
	}

	for( i = 0; i < maxtileid; i++ )
	{
		printf( "%d %d %d\n", i, tilecounts[i], tileremap[i] );
	}

	// Compute the chances-of-tile table.
	

	for( y = 0; y < BLKY; y++ )
	{
		for( x = 0; x < BLKX; x++ )
		{
			int changes = tilechangesqty[y][x];
			vpx_writer w = { 0 };
			vpx_start_encode( &w, bufferVPX, sizeof(bufferVPX));
			for( i = 0; i < changes; i++ )
			{
				// First we encode the block ID.
				// Then we encode the run length.
				int tile = tilechangesto[y][x][i];
				int runlen = tilechangesrle[y][x][i];

				int bits = intlog2( maxtileid_remapped );
				int b;
				for( b = 0; b < bits; b++ )
				{
					int tilemask = bits - b;
					int chance_of_0 = 0
				}

				int data = dummydata[i];
				int bits = 8;
				int bit;
				for (bit = bits - 1; bit >= 0; bit--)
				{
					int outbit = (data >> bit) & 1;
					vpx_write(&w, outbit, probability);
				}
			}
			vpx_stop_encode(&w);
*/
}

