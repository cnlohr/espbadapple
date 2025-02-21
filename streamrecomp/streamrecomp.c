#include <stdio.h>
#include <math.h>
#include "bacommon.h"

#define CNFG_IMPLEMENTATION
#define CNFGOGL

#include "rawdraw_sf.h"

#define BSX (RESX/BLOCKSIZE)
#define BSY (RESY/BLOCKSIZE)

float fRemoveThreshold = -0.1; // How different can frames be but this gets preserved?

uint8_t videodata[FRAMECT][RESY][RESX];
float tiles[TARGET_GLYPH_COUNT][BLOCKSIZE][BLOCKSIZE];
uint32_t streamin[FRAMECT][BSY][BSX];
int dropflag[FRAMECT][BSY][BSX];

#define ZOOM 4
#define FRAMEOFFSET 0

float LossFrom( int tile, int x, int y, int f )
{
	float fr = 0;
	int lx, ly;
	for( ly = 0; ly < BLOCKSIZE; ly++ )
	{
		for( lx = 0; lx < BLOCKSIZE; lx++ )
		{
			float t = tiles[tile][ly][lx];
			float p = videodata[f+FRAMEOFFSET][ly+y*BLOCKSIZE][lx+x*BLOCKSIZE] / 255.0;
			float d = t - p;
			if( d < 0 ) d *= -1;
			fr += d;
			//printf( "[%.3f %.3f]", t, p );
		}
		//printf( "\n" );
	}
	//printf( "\n" );
	return fr;
}

void CommitRemoval( int x, int y, int frame )
{
	if( frame == 0 )
		return;
	int stp = streamin[frame-1][y][x];
	int st = streamin[frame][y][x];
	int f;	
	for( f = frame; f < FRAMECT; f++ )
	{
		int gl = streamin[frame][y][x];
		if( gl != st ) break;
		streamin[frame][y][x] = stp;
		dropflag[frame][y][x] = 1;
	}
}


float ComputeCostOfRemoving( int x, int y, int frame )
{
	if( frame == 0 )
		return 1e10;
	// First compute the lost regularly.
	int f;
	int stp = streamin[frame-1][y][x];
	int st = streamin[frame][y][x];
	if( stp == st )
	{
		fprintf( stderr, "Warning: Trying to check invalid frame change (%d %d %d)\n", x, y, frame );
		return 1e10;
	}

	float defloss = 0;
	for( f = frame; f < FRAMECT-FRAMEOFFSET; f++ )
	{
		int gl = streamin[f][y][x];
		if( st != gl )
		{
			break;
		}

		defloss += LossFrom( st, x, y, f );
	}

	float newloss = 0;
	for( f = frame; f < FRAMECT-FRAMEOFFSET; f++ )
	{
		int gl = streamin[f][y][x];
		if( st != gl )
		{
			break;
		}
		newloss += LossFrom( stp, x, y, f );
	}
	//printf( "%f / %f %f\n", defloss, newloss, newloss - defloss );
	return newloss - defloss;
}

int main()
{
	CNFGSetup( "Test", 1024, 768 );
	char fname[1024];
	sprintf( fname, "../comp2/videoout-%dx%dx%d.dat", RESX, RESY, BLOCKSIZE );
	FILE * f = fopen( fname, "rb" );
	if( !f ) { fprintf( stderr, "Error opening %s\n", fname ); return -5; };
	if( fread( videodata, sizeof(videodata), 1, f ) != 1 )
	{
		fprintf( stderr, "Error reading from %s\n", fname );
		return -5;
	}
	fclose( f );

	sprintf( fname, "../comp2/tiles-%dx%dx%d.dat", RESX, RESY, BLOCKSIZE );
	f = fopen( fname, "rb" );
	if( !f ) { fprintf( stderr, "Error opening %s\n", fname ); return -5; };
	if( fread( tiles, sizeof(tiles), 1, f ) != 1 )
	{
		fprintf( stderr, "Error reading from %s\n", fname );
		return -5;
	}
	fclose( f );

	sprintf( fname, "../comp2/stream-%dx%dx%d.dat", RESX, RESY, BLOCKSIZE );
	f = fopen( fname, "rb" );
	if( !f ) { fprintf( stderr, "Error opening %s\n", fname ); return -5; };
	if( fread( streamin, sizeof( streamin), 1, f ) != 1 )
	{
		fprintf( stderr, "Error reading from %s\n", fname );
		return -5;
	}
	fclose( f );


	int changes = 0;
	int removed = 0;
	printf( "%d / %d %d %d\n", (int)sizeof(streamin), FRAMECT, BSY, BSX );
	do
	{
		changes = 0;
		removed = 0;
		int mcx, mcy, mcf;
		float minDiff = 1e10;
		int x, y, frame;
		for( y = 0; y < BSY; y++ )
		{
			for( x = 0; x < BSX; x++ )
			{
				int lc = streamin[0][y][x];
				//printf( "%d %d %d %d %d\n", x, y, frame, changes, removed );
				for( frame = 1; frame < FRAMECT; frame++ )
				{
					int tc = streamin[frame][y][x];
					if( tc != lc )
					{
						float costDiff = ComputeCostOfRemoving( x, y, frame );
						printf( "%f\n", costDiff );
						if( costDiff < fRemoveThreshold )
						{
							CommitRemoval( x, y, frame );
							removed++;
						}
						else
						{
							if( costDiff < minDiff )
							{
								minDiff = costDiff;
								mcx = x;
								mcy = y;
								mcf = frame;
							}
							lc = tc;
							changes++;
						}
					}
				}
			}
		}
		CommitRemoval( mcx, mcy, mcf );
		printf( "%d, %d, %d, %f, %d, %d\n", mcx, mcy, mcf, minDiff, changes, removed );
	} while( false ); // while( changes > 55000 );
	FILE * so = fopen( "stream_stripped.dat", "wb" );
	fwrite( streamin, 1, sizeof( streamin ), so );
	fclose( so );

#if 0
	int frame, x, y;
	for( frame = 0; frame < FRAMECT-FRAMEOFFSET; frame++ )
	{
		CNFGClearFrame();
		CNFGHandleInput();
		for( y = 0; y < RESY; y++ )
		{
			for( x = 0; x < RESX; x++ )
			{
				uint32_t v = videodata[frame+FRAMEOFFSET][y][x];
				float orig = v / 255.0;
				uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );

				int bid = streamin[frame][y/8][x/8];
				int df = dropflag[frame][y/8][x/8];
				float t = tiles[bid][y%8][x%8];

				if( t < 0 ) t = 0; if( t >= 1 ) t = 1;
				v = t * 255.5;

				if( df )
					color = 0xFF | 0x00 | (v<<8) | 0xFF;
				else
					color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM + RESX*ZOOM + 100, y*ZOOM, x*ZOOM + RESX*ZOOM + 100 + ZOOM, y*ZOOM+ZOOM );

				float dif = (orig - t)*(orig - t);
				t = dif * 2.0;
				if( t < 0 ) t = 0; if( t >= 1 ) t = 1;
				v = t * 255.5;

				color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM, y*ZOOM + RESY*ZOOM + 100, x*ZOOM + ZOOM, y*ZOOM+ZOOM + RESY*ZOOM + 100 );
			}
		}
		CNFGSwapBuffers();
		//usleep(40000);
	}
#endif
	return 0;
}

