#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define CNFG_IMPLEMENTATION

#include "rawdraw_sf.h"

int gwidth;
int gheight;
int firstframe = 1;
int maxframe;
int * framenos;

void initframes( const unsigned char * frame, int linesize )
{
	int x, y;
	firstframe = 0;

	printf( "First frame got.\n" );
}

FILE * f;

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }
int wordcount = 0;

void got_video_frame( const unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	static int notfirst;
	int i, x, y;
	int comppl = 0;

	if( !notfirst )
	{
		CNFGSetup( "badapple", width, height );
		notfirst = 1;
	}

	int color = 0;
	int runningtime = 0;
	uint8_t compbuffer[16384];

	//encode
	for( y = 0; y < height; y++ )
	for( x = 0; x < width; x++ )
	{
		int thiscolor = rgbbuffer[x+y*linesize]>0x60;

		//I've tried swizziling it, but it doesn't seem to help.

		if( thiscolor != color )
		{
			compbuffer[comppl++] = runningtime;
			color = thiscolor;
			runningtime = 0;
		}
		else
		{
			runningtime++;
			if( runningtime >= 256 )
			{
				compbuffer[comppl++] = (runningtime-1);// | COMPWIDTH;
				runningtime = 0;
			}
		}
	}

	compbuffer[comppl++] = runningtime;

	fwrite( compbuffer, comppl, 1, f );
	wordcount += comppl;

	color = 0;
	runningtime = compbuffer[0];

	int wasruntime = compbuffer[0];
	int thispl = 1;
	uint32_t data[width*height];
	int dpl = 0;
	for( y = 0; y < height; y++ )
	for( x = 0; x < width; x++ )
	{
		if( runningtime == 0 )
		{
			if( wasruntime != 255 ) color = !color;
			runningtime = compbuffer[thispl++];
			wasruntime = runningtime;
		}
		else
			runningtime--;

		data[dpl++] =(color)?0xffffff:0x000000 ; 
		//CNFGColor( (color)?0xffffff:0x000000 );
		//CNFGTackPixel( x, y );

/*		CNFGColor( 0x0 );
		CNFGTackSegment( 0, i, 100, i );
		CNFGColor( 0xffffff );
		CNFGTackSegment( 100, i, 200, i );*/
	}
	CNFGBlitImage( data, 0, 0, width, height );
	CNFGSwapBuffers();

	//usleep( 2000 );
	printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

	maxframe = frame;
}

int main( int argc, char ** argv )
{
	f = fopen( "rle255.dat", "wb" );
	int line;
	
	int width, height;
	int frame = 0;
	FILE * f = fopen( "videoout.dat", "rb" );
	int r = fscanf( f, "%d %d\n", &width, &height );
	if( r != 2 )
	{
		fprintf( stderr, "Error: Video format\n" );
		return -9;
	}
	while( !feof(f) )
	{
		uint8_t buffer[width*height];
		int r = fread( buffer, width, height, f );
		if( r != height )
		{
			fprintf( stderr, "Error reading video\n" );
		}
		got_video_frame( buffer, width, width, height, frame++ );
	}
	printf( "Total: %d bytes\n", wordcount );
}

