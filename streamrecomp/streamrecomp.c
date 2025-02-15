#include <stdio.h>
#include <math.h>
#include "bacommon.h"

#define CNFG_IMPLEMENTATION
#define CNFGOGL

#include "rawdraw_sf.h"

uint8_t videodata[FRAMECT][RESY][RESX];
float tiles[TARGET_GLYPH_COUNT][BLOCKSIZE][BLOCKSIZE];
uint32_t streamin[FRAMECT][RESY/BLOCKSIZE][RESX/BLOCKSIZE];

#define ZOOM 4

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

	int frame, x, y;
	for( frame = 0; frame < FRAMECT; frame++ )
	{
		CNFGHandleInput();
		for( y = 0; y < RESY; y++ )
		{
			for( x = 0; x < RESX; x++ )
			{
				uint32_t v = videodata[frame][y][x];
				float orig = v / 255.0;
				uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );

				int bid = streamin[frame][y/8][x/8];
				float t = tiles[bid][y%8][x%8];

				if( t < 0 ) t = 0; if( t >= 1 ) t = 1;
				v = t * 255.5;

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
		usleep(1000);
	}
//CFLAGS:=-O2 -g -I../common -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE}
	return 0;
}

