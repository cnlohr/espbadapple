#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#include "encodingtools.h"

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

//#define COMPWIDTH 2048
// This actually is dynamic.
#define COMPWIDTH 1073741824

#if COMPWIDTH > 32768
uint32_t compbuffer[67108864];
uint8_t odatastream[67108864];
int odatatbitcount = 0;
int comppl = 0;
int lastcomp = 0;
uint32_t histogram[1<<26];
int color = 0;
int runningtime = 0;
#endif




void got_video_frame( const unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	static int notfirst;
	int i, x, y;

	if( !notfirst )
	{
		CNFGSetup( "badapple", width, height );
		notfirst = 1;
	}

#if COMPWIDTH > 32768
#elif COMPWIDTH > 128
	int color = 0;
	int runningtime = 0;
	int comppl = 0;
	uint16_t compbuffer[262144];
#else
	int color = 0;
	int runningtime = 0;
	int comppl = 0;
	uint8_t compbuffer[262144];
#endif
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
			if( runningtime >= COMPWIDTH )
			{
				compbuffer[comppl++] = (runningtime-1) | COMPWIDTH;
				runningtime = 0;
			}
		}
	}

	compbuffer[comppl++] = runningtime;

#if COMPWIDTH == 1073741824
	for( i = lastcomp; i < comppl; i++ )
	{
		ETEmitUE( odatastream, sizeof(odatastream), &odatatbitcount, compbuffer[i] );
		histogram[compbuffer[i]]++;
	}
	wordcount += (comppl-lastcomp);
	lastcomp = comppl;	
#elif COMPWIDTH == 2048
	if( comppl & 1 )
	{
		compbuffer[comppl++] = 0;
	}

	for( i = 0; i < comppl/2; i++ )
	{
		uint8_t first = compbuffer[i*2+0]&0xff;
		uint8_t second = compbuffer[i*2+1]&0xff;
		uint8_t third = (((compbuffer[i*2+0]>>4)&0xf0)|(compbuffer[i*2+1]>>8));
		fprintf( f, "%c%c%c", first, second, third );
		wordcount += 3;
	}
#elif COMPWIDTH == 128
	fwrite( compbuffer, comppl, 1, f );
	wordcount += comppl;
#elif COMPWIDTH == 32768
	fwrite( compbuffer, comppl, 2, f );
	wordcount += comppl*2;
#else
	#error not-outputtable-compwidth
#endif

	color = 0;
	runningtime = compbuffer[0];
	int thispl = 1;
	uint32_t data[width*height];
	int dpl = 0;
	for( y = 0; y < height; y++ )
	for( x = 0; x < width; x++ )
	{
		if( !(runningtime&(COMPWIDTH-1)) )
		{
			if( !(runningtime & COMPWIDTH) )
				color = !color;
			runningtime = compbuffer[thispl++];
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
	CNFGUpdateScreenWithBitmap( data, width, height );
//	CNFGSwapBuffers();
//	usleep( 20000 );

	printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

	maxframe = frame;
}

int main( int argc, char ** argv )
{
	f = fopen( "rawdata.dat", "wb" );
	int line;
	
	int width, height;
	int frame = 0;
	FILE * f = fopen( "videoout.dat", "rb" );
	fscanf( f, "%d %d\n", &width, &height );
	while( !feof(f) )
	{
		uint8_t buffer[width*height];
		fread( buffer, width, height, f );
		got_video_frame( buffer, width, width, height, frame++ );
	}

	int i;
#if 1
	for( i = 0; i < sizeof(histogram)/sizeof(histogram[0]); i++ )
	{
		if( histogram[i] ) printf( "%d: %d\n", i, histogram[i] );
	}
	printf( "\n" );
#endif
	printf( "Total: %d symbols/bytes\n", wordcount );


#if COMPWIDTH > 32768
	printf( "Total Bits: %d\n", odatatbitcount );
#endif
}

