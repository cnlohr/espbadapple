#include <stdio.h>
#include "DrawFunctions.h"
#include "ffmdecode.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

int wordcount = 0;

#define MAXGLYPHS 200000

struct glyph
{
	uint64_t dat;
	int      qty;
};
struct glyph gglyphs[MAXGLYPHS];
int glyphct;


int stage;

int BitDiff( uint64_t a, uint64_t b )
{
	int i;
	static uint8_t BitsSetTable256[256];
	static uint8_t did_init;
	if( !did_init )
	{
		BitsSetTable256[0] = 0;
		for (int i = 0; i < 256; i++)
		{
			BitsSetTable256[i] = (i & 1) + BitsSetTable256[i / 2];
		}
		did_init = 1;
	}

	int ct = 0;
	uint64_t mismask = a^b;

	ct = BitsSetTable256[ (mismask)&0xff ] +
		BitsSetTable256[ (mismask>>8)&0xff ] +
		BitsSetTable256[ (mismask>>16)&0xff ] +
		BitsSetTable256[ (mismask>>24)&0xff ] +
		BitsSetTable256[ (mismask>>32)&0xff ] +
		BitsSetTable256[ (mismask>>40)&0xff ] +
		BitsSetTable256[ (mismask>>48)&0xff ] +
		BitsSetTable256[ (mismask>>56)&0xff ];

	return ct;
}

void got_video_frame( const unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	static int notfirst;
	int i, x, y;
	int comppl = 0;

	if( (width % 8) || (height % 8)) 
	{
		fprintf( stderr, "Error: width is not divisible by 8.\n" );
		exit( -1 );
	}

	if( !notfirst )
	{
		CNFGSetup( "badapple", width, height );
		notfirst = 1;
	}

	int color = 0;
	int runningtime = 0;

	int glyphs = width/8*height/8;
	uint64_t thisframe[glyphs];

	//encode
	for( y = 0; y < height; y+=8 )
	for( x = 0; x < width; x+=8 )
	{
		int bitx, bity;
		uint64_t glyph = 0;
		int glyphbit = 0;
		for( bity = 0; bity < 8; bity++ )
		for( bitx = 0; bitx < 8; bitx++ )
		{
			int on = rgbbuffer[(x+bitx)*3+(y+bity)*linesize]>0x60;
			glyph <<= 1;
			glyph |= on;
		}
		thisframe[(x/8)+(y*width)/64] = glyph;
	}


	if( stage == 1 )
	{
		int g;
		for( g = 0; g < glyphs; g++ )
		{
			uint64_t tg = thisframe[g];
			int h;
			for( h = 0; h < glyphct; h++ )
			{
				if( tg == gglyphs[h].dat )
				{
					break;
				}
			}
			//printf( "%d -> %llx -> %d\n", g, tg, h );
			if( h == glyphct )
			{
				gglyphs[glyphct++].dat = tg;
			}
			gglyphs[h].qty++;
		}

		printf( "%d\n", glyphct );

//		uint32_t data[width*height];
//		CNFGUpdateScreenWithBitmap( (long unsigned int*)data, width, height );
//		printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

		maxframe = frame;

	}
	else if( stage == 3 )
	{
		int i, g;

		uint32_t glyphmap[glyphs];
		uint32_t data[width*height];
		memset( data, 0, sizeof(data ));
		for( g = 0; g < glyphs; g++ )
		{
			uint64_t gl = thisframe[g];
			//Step 1: look for an exact match.
			for( i = 0; i < glyphct; i++ )
			{
				if( gglyphs[i].dat == gl ) break;
			}

			if( i == glyphct )
			{
				//Step 2: find best match.
				int bestdiff = 255;
				int bestid;
				for( i = 0; i < glyphct; i++ )
				{
					uint64_t targ = gglyphs[i].dat;
					int diff = BitDiff( targ, gl );
					if( diff < bestdiff ) //NOTE: Want to select first instance of good match, to help weight huffman tree if we use one.
					{
						bestdiff = diff;
						bestid = i;
					}
				}
#if 0 //Mark the problem areas with green.
				int cx, cy;
				int tx = (g%(width/8))*8;
				int ty = (g/(width/8))*8;
				printf( "%d %d\n", tx, ty );
				for( cy = 0; cy < 8; cy++ )
				for( cx = 0; cx < 8; cx++ )
				{
					int green = bestdiff<<4;
					if( green > 255 ) green = 255;
					data[ (cx+tx) + (cy+ty)*width ] = green<<8;
				}
				printf( "%d\n", bestdiff );
#endif
				i = bestid;
			}
			glyphmap[g] = i;
			gglyphs[i].qty++;
		}
		fwrite( glyphmap, sizeof( glyphmap ), 1, f );

		for( y = 0; y < height/8; y++ )
		for( x = 0; x < width/8; x++ )
		{
			uint64_t glyphdata = gglyphs[glyphmap[x+y*(width/8)]].dat;
			int lx = x * 8;
			int ly = y * 8;
			int px, py;
			for( py = 0; py < 8; py++ )
			for( px = 0; px < 8; px++ )
			{
				uint64_t bit = (glyphdata>>(63-(px+py*8))) & 1;
				data[px+x*8+(py+y*8)*width] |= bit?0xf00000:0x00000000;
			}
		}

		for( y = 0; y < height; y++ )
		for( x = 0; x < width; x++ )
		{
			int on = rgbbuffer[(x)*3+(y)*linesize]>0x60;
			//data[x+y*width] |= on?0xf0:0x00;
		}
		CNFGUpdateScreenWithBitmap( (long unsigned int*)data, width, height );

		printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

		maxframe = frame;
		//usleep(17000);

			//Map this to glyph "i"
	}
}
/*
	color = 0;
	runningtime = compbuffer[0];
	int thispl = 1;
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
	}

*/

int compare_ggs( const void * pp, const void *qq) {
	const struct glyph *p = (const struct glyph *)pp;
	const struct glyph *q = (const struct glyph *)qq;
    int x = q->qty;
    int y = p->qty;

    /* Avoid return x - y, which can cause undefined behaviour
       because of signed integer overflow. */
    if (x < y)
        return -1;  // Return -1 if you want ascending, 1 if you want descending order. 
    else if (x > y)
        return 1;   // Return 1 if you want ascending, -1 if you want descending order. 

    return 0;
}


int main( int argc, char ** argv )
{
	if( argc < 2 ) goto help;

	stage = atoi( argv[1] );
	if( !stage ) goto help;

	if( stage == 1 )
	{
		if( argc < 3 )
		{
			fprintf( stderr, "Error: stage 1 but no video\n" );
			return -9; 
		}
		f = fopen( "rawtiledata.dat", "wb" );
		int line;
		setup_video_decode();

		video_decode( argv[2] );

		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
		printf( "Total: %d glyphs\n", glyphct );
	}
	else if( stage == 2 )
	{
		f = fopen( "rawtiledata.dat", "rb" );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );

		int i;
		int qg1 = 0;

		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );

		for( i = 0; i < glyphct; i++ )
		{
			if( gglyphs[i].qty > 1 )
			{
				printf( "%6d / %6d / %016lx\n", i, gglyphs[i].qty, gglyphs[i].dat );
				qg1++;
			}
		}
		printf( "%d\n", qg1 );

		//XXX TODO: Right now, we're selecting the most popular 256 tiles.
		//  Must do something to merge tiles and find out what the most useful 256 tiles are.

		f = fopen( "rawtiledata.dat", "wb" );
		glyphct = qg1;
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
	}
	else if( stage == 3 )
	{
		if( argc < 4 )
		{
			fprintf( stderr, "Error: stage 3 but need:  3 [avi] [nr_of_tiles] \n" );
			return -9; 
		}
		f = fopen( "rawtiledata.dat", "rb" );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );
		f = fopen ("outgframes.dat", "wb" );

		//Reset glyph counts.
		int i;
		for( i = 0; i < glyphct; i++ )
			gglyphs[i].qty = 0;

		setup_video_decode();
		video_decode( argv[2] );

		//Sort tiles.
		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		int tileout = atoi( argv[3] );

		for( i = 0; i < tileout; i++ )
		{
			printf( "%d: %d\n", i, gglyphs[i].qty );
		}

		f = fopen( "rawtiledata.dat", "wb" );
		glyphct = tileout;
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
	}
	else if( stage == 4 )
	{
		//try compressing the outgframes.
		FILE * f = fopen( "outgframes.dat", "rb" );
		fseek( f, 0, SEEK_END );
		int len = ftell( f )/4;
		fseek( f, 0, SEEK_SET );
		uint32_t * gfdat = malloc(len*4);
		int ignore = fread( gfdat, len, 1, f );
		fclose( f );

		f = fopen( "rawtiledata.dat", "rb" );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );

		int i;
		int maxgf = 0;
		printf( "LEN: %d\n", len );
		for( i = 0; i < len; i++ )
		{
			if( gfdat[i] > maxgf ) maxgf = gfdat[i];
			gglyphs[gfdat[i]].qty++;
		}
		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		for( i = 0; i < maxgf; i++ )
		{
			printf( "%d\n", gglyphs[i].qty );
		}

		//First, we should encode the RLE somehow, or at least RLE of types 0 and 1.
		//since they are 3 orders of magnitude more common than any other symbol.
		//Maybe all this step should do is create extra symbols for RLE of the first two symbols.
		printf( "LEN: %d / max: %d\n", len, maxgf );
	}

	else goto help;

	return 0;
iofault:
	fprintf( stderr, "I/O fault\n" );
	return -2;
help:
	fprintf( stderr, "Error: usage: tiletest [stage] [avi file]\n" );
	fprintf( stderr, "Stage: 1: Parse into full tile set (huge quantities) set.\n" );
	fprintf( stderr, "Stage: 2: Show frequency of tiles in set, removing all orphan events.\n" );
	fprintf( stderr, "Stage: 3: Restrict tile set based on video, also produce tile matches. (Run this step multiple times with decreasing ranges, down to 254.)\n" );
	fprintf( stderr, "Stage: 4: Try to compress output data set.\n" );
	return -1;
}

