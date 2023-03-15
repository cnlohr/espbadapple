#include <stdio.h>
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"
#include "ffmdecode.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>


//XXX TODO: Do something about the tearing at the bottom of the screen.
//Probably needs to be moved off into own file.
//XXX TODO: Add hatching for grey.

		/* Current recommended operating mode:
				./tiletest16 1 badapple-sm8628149.mp4
				./tiletest16 3 badapple-sm8628149.mp4 5000000 0
				./tiletest16 2 2 -4
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0
				./tiletest16 2 3 -7
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0
				./tiletest16 2 3 -10
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0.0001
				./tiletest16 2 3 -14
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0.0001
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0.0001
				./tiletest16 2 3 -16
				./tiletest16 3 badapple-sm8628149.mp4 20000 0.001
				./tiletest16 2 3 -16
				./tiletest16 3 badapple-sm8628149.mp4 10000 0.002

				./tiletest16 2 3 .0001
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0.001
				./tiletest16 2 3 .001
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0.01
				./tiletest16 2 3 .002
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0.01
				./tiletest16 2 3 .01
				./tiletest16 3 badapple.mp4 2048 0
				./tiletest16 2 3 .01
				./tiletest16 3 badapple.mp4 2048 0
				./tiletest16 2 4 .03  ## This was too aggressive. Knocked it down to 538 glyphs.
				./tiletest16 3 badapple.mp4 2048 0
				./tiletest16 4


		For 8-tile...
				./tiletest16 1 badapple-sm8628149.mp4
				./tiletest16 2 2 -1
				./tiletest16 3 badapple-sm8628149.mp4 2000000 0
				#./tiletest16 2 1 .0005 (maybe more?)
				./tiletest16 3 badapple-sm8628149.mp4 200000 .001
				./tiletest16 3 badapple-sm8628149.mp4 10000 .001
				./tiletest16 2 3 -2

				./tiletest16 3 badapple-sm8628149.mp4 200000 .002  # Get a sorted list out.
				./tiletest16 3 badapple-sm8628149.mp4 200000 .002  # Get a sorted list out.
			This gets you to under 2MB but you can keep going...  This starts to look awful.
				./tiletest16 2 2 .004 (maybe more?)
				./tiletest16 3 badapple-sm8628149.mp4 200000 .002  # Get a sorted list out.
			Gets you to 1.7MB
				./tiletest16 2 2 .02 (maybe more?)
				./tiletest16 3 badapple-sm8628149.mp4 200000 .01  # Get a sorted list out.
				./tiletest16 2 2 .03 (maybe more?)
				./tiletest16 3 badapple-sm8628149.mp4 200000 .02  # Get a sorted list out.
				./tiletest16 3 badapple-sm8628149.mp4 200000 .1  # Get a sorted list out.
				./tiletest16 4

		*/



int gwidth;
int gheight;
int firstframe = 1;
int maxframe;
int * framenos;

//#define FOR_ESP8266
#define SUPERTINY
//#define TINYWITHFLASH

#ifdef SUPERTINY

#define TILE_W 16
#define TILE_H 16
#define HALFTONE
#define EVERY_OTHER_FRAME 1
#define SFILL 0
#define LIMIT 0x60
#define LIMITA 0xa0
#define LIMITB 0x18
#define USE_PREVIOUS_THRESH 12 //For delta-frames.
#define USE_PREVIOUS_THRESH_S 6
#define USE_DELTA_FRAMES

#elif defined( TINYWITHFLASH )

#define TILE_W 8
#define TILE_H 8
#define HALFTONE
#define EVERY_OTHER_FRAME 0
#define SFILL 0
#define LIMIT 0x60
#define LIMITA 0xa0
#define LIMITB 0x18
#define USE_PREVIOUS_THRESH 8 //For delta-frames.
#define USE_PREVIOUS_THRESH_S 4
#define USE_DELTA_FRAMES

#elif defined(FOR_ESP8266)
//./decodevideo badapple.mp4 288 224

#define TILE_W 16
#define TILE_H 16
#define HALFTONE
#define SFILL 2
#define LIMIT 0x60
#define LIMITA 0xa0
#define LIMITB 0x18
#define USE_PREVIOUS_THRESH 8 //For delta-frames.
#define USE_PREVIOUS_THRESH_S 4
#define USE_DELTA_FRAMES

#else

//Maybe something with a little more beef?

#define TILE_W 16
#define TILE_H 16
#define HALFTONE
#define SFILL 3
#define LIMITA 0xa0
#define LIMITB 0x20

#define USE_PREVIOUS_THRESH 4 //For delta-frames.
#define USE_PREVIOUS_THRESH_S 2

#define USE_DELTA_FRAMES

#endif




//In our source data, we have 357 instances of exceeding RLE lenght if 8 bits.  It is less total
//data to store RLEs as doubles.
#define MAXRLE 8191 
#define T_RLE  uint16_t
//#define MAXRLE 127
//#define T_RLE  uint8_t

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

#define EXP_W (((int)(width)))
#define EXP_H (((int)(height)))
#define MAXGLYPHS 600000

#if TILE_W==8 && TILE_H==8
typedef uint64_t tiledata;
#define GlyphsEqual( a, b ) ((a) == (b))
#define SetGlyph( a, b ) a = b;
#elif TILE_W==8 && TILE_H==4
typedef uint32_t tiledata;
#define GlyphsEqual( a, b ) ((a) == (b))
#define SetGlyph( a, b ) a = b;
#elif TILE_W==16 && TILE_H==16
typedef uint16_t tiledata[16];
#define GlyphsEqual( a, b ) ( ((uint64_t*)a)[0] == ((uint64_t*)b)[0] && ((uint64_t*)a)[1] == ((uint64_t*)b)[1] && ((uint64_t*)a)[2] == ((uint64_t*)b)[2] && ((uint64_t*)a)[3] == ((uint64_t*)b)[3] )
#define SetGlyph( a, b ) memcpy( a, b, 32 );
#else
#error TILE_W,TILE_H must be 8,4 or 8,8 or 16,16
#endif

struct glyph
{
	int32_t  flag;
	int32_t      qty;
	union
	{
		tiledata dat;
		uint32_t runlen;
	} dat;
} __attribute__((packed));
struct glyph gglyphs[MAXGLYPHS];
int glyphct;


int stage;
//Used in mode 3.
int total_quality_loss;
int highest_used_symbol = 0;
float weight_toward_earlier_symbols;

int BitDiff( tiledata a, tiledata b, int mintocare )
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

#if TILE_W==8 && TILE_H==8
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
#elif TILE_W==8 && TILE_H==4
	int ct = 0;
	uint32_t mismask = a^b;

	ct = BitsSetTable256[ (mismask)&0xff ] +
		BitsSetTable256[ (mismask>>8)&0xff ] +
		BitsSetTable256[ (mismask>>16)&0xff ] +
		BitsSetTable256[ (mismask>>24)&0xff ];
#elif TILE_W==16
	int ct = 0;
	for( i = 0; i < 16; i++ )
	{
		uint16_t diff = a[i]^b[i];
		ct += BitsSetTable256[diff>>8] + BitsSetTable256[diff & 0xff];
		if( ct >= mintocare ) { return mintocare; }
	}

#endif

	return ct;
}


void got_video_frame( unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
#ifdef EVERY_OTHER_FRAME
	if( frame % (EVERY_OTHER_FRAME+1) ) return;
#endif
	static int notfirst;
	int i, x, y;
	int comppl = 0;

	if( (width % TILE_W) )
	{
		fprintf( stderr, "Error: width is not divisible by TILE. %d %d %d %d\n", width, height, (int)(width % TILE_W), (int)(height % TILE_H) );
		exit( -1 );
	}

	height = ( height / TILE_H ) * TILE_H;

	if( !notfirst )
	{
		CNFGSetup( "badapple", width, height );
		printf( "Width: %d / Height: %d  (%d %d) (%d %d) Leftover %d %d\n", width, height, EXP_W, EXP_H, width / TILE_W, height / TILE_H, width % TILE_W, height % TILE_H );
		notfirst = 1;
	}

	int color = 0;
	int runningtime = 0;

	int glyphs = width/TILE_W*height/TILE_H;
	tiledata thisframe[glyphs];

	//encode
	for( y = 0; y < height; y+=TILE_H )
	for( x = 0; x < width; x+=TILE_W )
	{
		int bitx, bity;
		tiledata glyph;
		int glyphbit = 0;

#if TILE_W == 8
		glyph = 0;
		for( bity = 0; bity < TILE_H; bity++ )
		for( bitx = 0; bitx < TILE_W; bitx++ )
		{
#ifndef HALFTONE
			int on = rgbbuffer[(int)(((x+bitx)))+(int)((y+bity))*linesize]>LIMIT;
#else
			int on = rgbbuffer[(int)(((x+bitx)))+(int)((y+bity))*linesize]>(((bitx & 1) == (bity & 1))?LIMITA:LIMITB);
#endif
			glyph <<= 1;
			glyph |= on;
		}
		thisframe[(x/8)+(y*width)/64] = glyph;
#elif TILE_W == 16
		for( bity = 0; bity < TILE_H; bity++ )
		{
			int tglyph = 0;
			for( bitx = 0; bitx < TILE_W; bitx++ )
			{
#ifndef HALFTONE
				int on = rgbbuffer[(int)(((x+bitx)))+(int)((y+bity))*linesize]>LIMIT;
#else
				int on = rgbbuffer[(int)(((x+bitx)))+(int)((y+bity))*linesize]>(((bitx & 1) == (bity & 1))?LIMITA:LIMITB);
#endif

				tglyph <<= 1;
				tglyph |= on?1:0;
			}
			glyph[bity] = tglyph;
		}
		SetGlyph( thisframe[(x/16)+(y*width)/256], glyph );
#endif

	}


	if( stage == 1 )
	{
		int g;
		for( g = 0; g < glyphs; g++ )
		{
			tiledata tg;
			SetGlyph( tg, thisframe[g] );
			int h;
			for( h = 0; h < glyphct; h++ )
			{
//#define FIRST_PASS_CULL
#ifdef FIRST_PASS_CULL
				if( BitDiff( tg, gglyphs[h].dat.dat, 4 ) < 4 )
				{
					break;
				}
#else
				if( GlyphsEqual( tg, gglyphs[h].dat.dat ) )
				{
					break;
				}
#endif
			}
			//printf( "%d -> %llx -> %d\n", g, tg, h );
			if( h == glyphct )
			{
				SetGlyph( gglyphs[glyphct++].dat.dat, tg );
			}
			gglyphs[h].qty++;
		}

		printf( "%d %d\n", glyphct, frame );

//		uint32_t data[width*height];
//		CNFGUpdateScreenWithBitmap( (long unsigned int*)data, width, height );
//		printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

		int32_t data[width*height];
		memset( data, 0, sizeof(data ));

		for( y = 0; y < height; y++ )
		for( x = 0; x < width; x++ )
		{
#ifndef HALFTONE
			int on = rgbbuffer[(int)((x))+(int)(y)*linesize]>LIMIT;
#else
			int on = rgbbuffer[(int)((x))+(int)(y)*linesize]>(((x & 1) == (y & 1))?LIMITA:LIMITB);
#endif
			data[x+y*width] |= on?0xfffffff:0x00;
		}
		CNFGUpdateScreenWithBitmap( (uint32_t*)data, width, height );

		maxframe = frame;

	}
	else if( stage == 3 ) 	//refine:  Use the existing dictionary, only and try to fit based on that.
	{
		int i, g;
		uint8_t match_frame[glyphs];
		static uint32_t  * glyphlast;
		if( !glyphlast ) { glyphlast = calloc( glyphs, sizeof(uint32_t) ); }
		int32_t glyphmap[glyphs];
		int32_t data[width*height];
		memset( data, 0, sizeof(data ));
		for( g = 0; g < glyphs; g++ )
		{
			tiledata gl;
			SetGlyph( gl, thisframe[g] );
			//Step 1: look for an exact match.
			if( weight_toward_earlier_symbols == 0 )
				for( i = 0; i < glyphct; i++ )
				{
					if( GlyphsEqual( gglyphs[i].dat.dat , gl ) ) break;
				}
			else
			{
				for( i = 0; i < 2; i++ )
				{
					if( GlyphsEqual( gglyphs[i].dat.dat , gl ) ) break;
				}
				if( i == 2 ) i = glyphct;
			}
			//printf( "%d %d\n", i , glyphct );
			if( i == glyphct )
			{
				//Step 2: find best match.
				float bestdiff = 1e20;
				int bestid;
				for( i = 0; i < glyphct; i++ )
				{
					tiledata targ;
					SetGlyph( targ, gglyphs[i].dat.dat );
					float diff;
					float bias = 0;
					if( weight_toward_earlier_symbols >= 0 )
						bias = weight_toward_earlier_symbols * i;
					else
						bias = ((i>=2)?(-weight_toward_earlier_symbols):0);

					//Don't check impossible-to-hit solutions.
					if( bias > (TILE_W*TILE_H/2) ) break;

					int best = ( (int)(bestdiff-bias)+1 );
					if( best > 100 || best < 1 ) best = 10000;
					diff = BitDiff( targ, gl, best ) + bias;
					//printf( "%16lx %16lx  %d %f\n", targ, gl, i, diff );

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

				total_quality_loss += bestdiff;
				i = bestid;
				//printf( "Selected %d %f\n", i, bestdiff );
			}

			//XXX HERE: write out -1 for cells that didn't change.
			//Do something smart about it.

#ifdef USE_DELTA_FRAMES
			int last = glyphlast[g];
			int lastdiffS = BitDiff( gglyphs[last].dat.dat, gl, 1000 );
			int lastdiffC = BitDiff( gl, gglyphs[i].dat.dat, 1000 );

			//If the recommended glyph is 0 or 1, and it doesn't match perfectly, make it so.
#ifdef HALFTONE
			int disable_rle_for_this = i<3 && lastdiffS;
#else
			int disable_rle_for_this = i<2 && lastdiffS;
#endif
			if( ( ( lastdiffS < USE_PREVIOUS_THRESH		//Is it "good enough"?
				 || lastdiffS <= lastdiffC + USE_PREVIOUS_THRESH_S ) &&
					!disable_rle_for_this ) ||  last == i )		//Is it at least as good as the previously selected frag?
			{
				glyphmap[g] = -1;
				match_frame[g] = 1;
			}
			else
#endif
			{
				//Check to see if it should just stay the same...
				if( i > highest_used_symbol ) highest_used_symbol = i;
				glyphmap[g] = i;
				gglyphs[i].qty++;
				match_frame[g] = 0;
			}
		}
		fwrite( glyphmap, sizeof( glyphmap ), 1, f );
		for( g = 0; g < glyphs; g++ )
			if( glyphmap[g] >= 0 ) glyphlast[g] = glyphmap[g];

		for( y = 0; y < height/TILE_H; y++ )
		for( x = 0; x < width/TILE_W; x++ )
		{
			tiledata glyphdata;
			int g = x+y*(width/TILE_W);
			SetGlyph( glyphdata, gglyphs[glyphlast[g]].dat.dat );
			int lx = x * TILE_W;
			int ly = y * TILE_H;
			int px, py;
			for( py = 0; py < TILE_H; py++ )
			for( px = 0; px < TILE_W; px++ )
			{
#if TILE_W==8
				uint64_t bit = (glyphdata>>((TILE_W*TILE_H-1)-(px+py*TILE_W))) & 1;
#else
				int bit = glyphdata[py] & (1<<(15-px));
#endif
				uint32_t dat = bit?0xf00000:0x00000000;
				if( match_frame[g] ) dat|= 0xf0;
				data[px+x*TILE_W+(py+y*TILE_H)*width] = dat;
			}
		}

		for( y = 0; y < height; y++ )
		for( x = 0; x < width; x++ )
		{
#ifndef HALFTONE
			int on = rgbbuffer[(x)+(y)*linesize]>LIMIT;
#else
			int on = rgbbuffer[(x)+(y)*linesize]>(((x & 1) == (y & 1))?LIMITA:LIMITB);
#endif
			//data[x+y*width] |= on?0xf0:0x00;
		}
		CNFGUpdateScreenWithBitmap( (long unsigned int*)data, width, height );
		//CNFGSwapBuffers();
		//CNFGUpdateScreenWithBitmap( (long unsigned int*)data, width, height );
		printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

		maxframe = frame;


		//usleep(20000);

			//Map this to glyph "i"
	}


//	memcpy( oldbuffer, origrgb, linesize * height );
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


struct huff_for_sort
{
	int      ptr;
	int      qty;
	int		 oqty;
};

struct huff_tree
{
	int left;	//If MSB set, is leaf.  Otherwise is tree pointer.
	int right;

	uint64_t bitpattern;
	int      bitdepth;
};

//For finding the least used elements.
int compare_huff( const void * p, const void *q )
{
	const struct huff_for_sort *hp = (const struct huff_for_sort*)p;
	const struct huff_for_sort *hq = (const struct huff_for_sort*)q;
	if( hp->qty > hq->qty )
		return 1;
	else if( hp->qty < hq->qty )
		return -1;
	else
		return 0;
}

void FillOutTree( int place, struct huff_tree * ht, int bit_depth, uint64_t pattern )
{
	ht[place].bitpattern = pattern;
	ht[place].bitdepth = bit_depth;
	if( ht[place].left >= 0 )
		FillOutTree( ht[place].left, ht, bit_depth + 1, pattern );
	if( ht[place].right >= 0 )
		FillOutTree( ht[place].right, ht, bit_depth + 1, pattern | (1<<bit_depth) );
}

void OutputBufferToFile( FILE * outc, const char * dataname, const char * typename, int stride, int bytes, uint8_t * data )
{
	int elems = bytes/stride;
	fprintf( outc, "const %s %s[%d] = {", typename, dataname, elems );
	int i;
	for( i = 0; i < elems; i++ )
	{
		if( !(i % (16/stride) ) ) fprintf( outc, "\n\t" );
		uint32_t number = 0;
		int j;

		//Assume little-endian.
		for( j = 0; j < stride; j++ )
		{
			number |= data[i*stride+j]<<(j*8);
		}

		fprintf( outc, "0x%0*x, ", stride*2, number );
	}
	fprintf( outc, "};\n\n" );
}

int main( int argc, char ** argv )
{
	int width, height;
	if( argc < 2 ) goto help;
	FILE * fin = fopen( "videoout.dat", "rb" );
	fscanf( fin, "%d %d\n", &width, &height );
	int frame = 0;

	stage = atoi( argv[1] );
	if( !stage ) goto help;

		int line;
	if( stage == 1 )
	{
		if( argc < 2 )
		{
			fprintf( stderr, "Error: stage 1 but no video\n" );
			return -9; 
		}
		int line;


		while( !feof(fin) )
		{
			uint8_t buffer[width*height];
			fread( buffer, width, height, fin );
			got_video_frame( buffer, width, width, height, frame++ );
		}

		f = fopen( "rawtiledata.dat", "wb" );
		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
		printf( "Total: %d glyphs\n", glyphct );
		printf( "Writing %ld+4 bytes\n", sizeof( gglyphs[0] ) * glyphct );
	}
	else if( stage == 2 )
	{
		//CURRENTLY 2 isn't used, but we should probably use 2 to recondense the glyph dictionaries.
		if( argc < 4 )
		{
			fprintf( stderr, "Error: USage tiletest 2 [cutoff, try 2] [dynamic cutoff try 0.0005]\n" );
			exit( -9 );
		}
		float cutoff = atof( argv[2] );
		float cutoff_dyn = atof( argv[3] );
		f = fopen( "rawtiledata.dat", "rb" );
		printf( "%ld\n", sizeof( gglyphs[0] ) );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );

		printf( "Input %d glyphs\n", glyphct );
		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );

		int i, j;
		int qg1 = 0;

		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		int keepglyph = 0;
		for( i = 0; i < glyphct; i++ )
		{
			if( ( i % 1000 ) == 0 || i == glyphct-1 ) printf( "Glyph: %d / Kept %d / %d\n", i, keepglyph, i );
		//	printf( "Glyph: %d: ", i );
			//int closest = 0xffffff;
			//int closestindex = -1;
			int cutoff_mark;
			if( cutoff_dyn >= 0 )
				cutoff_mark = cutoff + cutoff_dyn * i+1;
			else if( gglyphs[i].qty > 0 )
			{
				int extraterm = -cutoff_dyn/gglyphs[i].qty;
				cutoff_mark = cutoff + extraterm;
			}
			else
				cutoff_mark = 10000;
			int best = 10000;
			for( j = 0; j < i; j++ )
			{
				if( gglyphs[j].qty > 0 && gglyphs[i].qty > 0 )
				{
					int bd = BitDiff( gglyphs[i].dat.dat, gglyphs[j].dat.dat, cutoff_mark );
					//if( bd <= closest )
					//{
					//	closest = bd;
					//	closestindex = j;
					//}
					if( bd < best ) best = bd;
					if( bd < cutoff_mark )
					{
						//Nerf lower count.  It's always 'i'
						gglyphs[i].qty = 0;
		//				printf( "Nix  %2d\n", bd );
						break;
					}

				}
			}

//			printf( "%3d ", closest );

			if( j == i ) keepglyph++;
		//	if( j == i )
		//		printf( "Keep (%d) %d (%d)\n",keepglyph++, best, gglyphs[j].qty );

		}


		//from a fresh run, cutoff = 2,.00005, Goes from 339089 to 83740
		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );

		int nr_to_nonzero = 0;
		for( i = 0; i < keepglyph; i++ )
		{
			if( gglyphs[i].qty ) nr_to_nonzero = i+1;
		}

		printf( "Pruned ending from %d to %d\n", keepglyph, nr_to_nonzero );
		glyphct = nr_to_nonzero;

		//printf( "%d\n", qg1 );

		//XXX TODO: Right now, we're selecting the most popular 256 tiles.
		//  Must do something to merge tiles and find out what the most useful 256 tiles are.

	#if 1
		f = fopen( "rawtiledata.dat", "wb" );
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
		#endif
		printf( "Output %d glyphs\n", glyphct );

	}
	else if( stage == 3 )
	{
		if( argc < 4 )
		{
			fprintf( stderr, "Error: stage 3 but need:  3 [nr_of_tiles] [weight for symbols earlier in the list, float]\n" );
			return -9; 
		}
		f = fopen( "rawtiledata.dat", "rb" );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );
		f = fopen ("outgframes.dat", "wb" );

		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		int tileout = atoi( argv[2] );
		if( tileout < glyphct ) 
			glyphct = tileout;

		int i;

		int tquat = 0;
#if 0
		for( i = 0; i < tileout; i++ )
		{
			printf( "%6d: %6d %16lx\n", i, gglyphs[i].qty, ((uint64_t*)gglyphs[i].dat.dat)[0] );
			tquat+= gglyphs[i].qty;
		}
#endif
		printf( "TQ: %d\n", tquat );

		//Reset glyph counts.
		for( i = 0; i < MAXGLYPHS; i++ )
		{
			gglyphs[i].qty  = 0;
		}
		weight_toward_earlier_symbols = atof( argv[3] );

		while( !feof(fin) )
		{
			uint8_t buffer[width*height];
			fread( buffer, width, height, fin );
			got_video_frame( buffer, width, width, height, frame++ );
		}


		highest_used_symbol++;
		if( glyphct > highest_used_symbol ) glyphct = highest_used_symbol;
		printf( "%d Symbols used... But writing %d\n", highest_used_symbol, glyphct );
		for( i = 0; i < glyphct; i++ )
		{
#if TILE_W==16
			printf( "%6d: %6d %16lx\n", i, gglyphs[i].qty, ((uint64_t*)gglyphs[i].dat.dat)[0] );
#else
			printf( "%6d: %6d %16lx\n", i, gglyphs[i].qty, gglyphs[i].dat.dat );
#endif
			tquat+= gglyphs[i].qty;
		}
		printf( "Total: %d\n", tquat );
		f = fopen( "rawtiledata.dat", "wb" );
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
		printf( "Quality loss: %d\n", total_quality_loss );
	}
	else if( stage == 4 )
	{
		//try compressing the outgframes.
		FILE * f = fopen( "outgframes.dat", "rb" );
		fseek( f, 0, SEEK_END );
		int len = ftell( f )/4;
		fseek( f, 0, SEEK_SET );
		uint32_t * gfdat_raw = malloc(len*4);
		if( fread( gfdat_raw, 1, len*4, f ) != len*4 ) { fprintf( stderr, "IO fault on read\n" ); exit( -9 ); }
		fclose( f );

		f = fopen( "rawtiledata.dat", "rb" );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );

		int i;
		int maxgf = 0;

		for( i = 0; i < MAXGLYPHS; i++ )
		{
			gglyphs[i].qty  = 0;
		}
//Perform a sort of space fill curve, seems to save about 15%
#if(SFILL>0)
		uint32_t * gfdat = malloc(len*4);
		int linecells = EXP_W/TILE_W;
		for( i = 0; i < len; i++ )
		{
			int frame = i / (EXP_W*EXP_H/(TILE_W*TILE_H));
			int cellinframe = i % (EXP_W*EXP_H/(TILE_W*TILE_H));

			int lower = cellinframe & ((1<<SFILL)-1);
			int upper = cellinframe % (((EXP_W/TILE_W))<<SFILL); ///XXX TODO This bit math might be wrong.

			//int mask = lower * 512/8;
			int x = upper>>SFILL;
			int y = lower + ((cellinframe/((EXP_W/TILE_W)<<SFILL))<<SFILL);

			//printf( "(%d %d)\n", x, y );
			//printf( "(%d,%d,%d)\n",cellinframe, x, y );
			gfdat[i] = gfdat_raw[x+y*linecells+frame*(EXP_W*EXP_H/(TILE_W*TILE_H))];
		}
#else
		uint32_t * gfdat = gfdat_raw;
#endif

#if 0
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
#endif
		printf( "LEN: %d\n", len );

		uint32_t * mapout = malloc( len * 4 );
		int mapelem = 0;


		printf( "Initial glyph count: %d\n", glyphct );
		int tqcells = 0;

		int initgglyphs = glyphct;

		for( i = 0; i < len; i++ )
		{
			int tglyph = gfdat[i];
			if( tglyph > initgglyphs )
			{
				printf( "WARNING! %d/%d\n", tglyph, initgglyphs );
			}
		}

#define DO_RLE  //Use this.

#ifdef DO_RLE

		int nr_rles = 0;
		//
		//Tricky - we actually want to remove the first two glyphs, since 0 and 1 will be RLE encoded.
		//for( i = 0; i < glyphct-2; i++ )
		//{
		//	memcpy( gglyphs + i, gglyphs + i + 2, sizeof( gglyphs[0] ) );
		//}
		//glyphct-=2;
		//initgglyphs -= 2;
		//memset( gglyphs + i, 0, sizeof( gglyphs[0] ) * 2 );
		//FALSE.  This causes many more problems than it solves.  We do not want to do this.


		for( i = 0; i < len; i++ )
		{
			int tglyph = gfdat[i];
			if( tglyph == 0 || tglyph == 1 || tglyph == -1 )  			//Detect first two glyphs.  They're special.  Need to RLE them.
			{
				int runlen = 1;
				i++;
				for( ; i < len && runlen < MAXRLE-1; i++ )
				{
					if( gfdat[i] == tglyph ) runlen++;
					else break;
				}
				i--;

				//printf( "RUNLEN: %d\n", runlen );

				tqcells += runlen;

				int flag = tglyph+2;
				int dat = runlen;
				int k;
				struct glyph * g;
				for( k = 0; k < glyphct; k++ )
				{
					g = &gglyphs[k];
					if( g->flag == flag && g->dat.runlen == dat ) break;
				}

				if( k == glyphct )
				{
					g = &gglyphs[glyphct];
					glyphct++;
					g->qty = 1;
					g->flag = flag;
					g->dat.runlen = dat;
					nr_rles++;
				}
				else
				{
					g->qty++;
				}
				mapout[mapelem++] = k;
				//printf( "+" );
				//printf( "MK: %d %d %d %d   %x\n", mapelem, tqcells, mapout[mapelem-1], glyphct, runlen );
			}
			else
			{
				struct glyph * g = &gglyphs[tglyph];
				g->qty++;
				tqcells ++;
				mapout[mapelem++] = tglyph;
				if( tglyph >= initgglyphs )
				{
					fprintf( stderr, "Error: original glyph exceeded table position.\n" );
				}
				//printf( "MA: %d %d %d\n", mapelem, tqcells, mapout[mapelem-1] );
				//printf( ":" );

			}
			int mo = mapout[mapelem-1];
			struct glyph * g = &gglyphs[mo];
			//printf( "YO %6d -> %d -> %16x  [%6d %d]\n", mo, g->flag, g->dat.runlen, mapelem, glyphct );

		}
#elif 0
		//This was an experiment to see if we shoud use some other mechanism to store run length
		//instead of huff.  Answer: No.  Use huff.
#else
		for( i = 0; i < len; i++ )
		{
			int tglyph = gfdat[i];
			struct glyph * g = &gglyphs[tglyph];
			g->qty++;
			tqcells ++;
			mapout[mapelem++] = tglyph;
			//printf( "MA: %d %d %d\n", mapelem, tqcells, mapout[mapelem-1] );
		}
#endif

		printf( "Total maps: %d\n", mapelem );
		printf( "Got cells: %d [check]\n", len );
		printf( "Accounted cells: %d [check]\n", tqcells );

		//Now, gglyphs is populated, and we have a static mapping of "mapout" to them.
		//Must now huffman compress the tree.



#if 0
		int tquat = 0;
		for( i = 0; i < glyphct; i++ )
		{
			struct glyph * g = &gglyphs[i];
			printf( "%4d %5d %d %16lx\n", i, g->qty, g->flag, g->dat );
			tquat += g->qty;
		}
		printf( "Total: %d\n", tquat );
		printf( "Glyph size: %d (Should match)\n", mapelem );
#endif

		struct huff_for_sort hfs[glyphct*2+2];
		struct huff_tree     ht[glyphct*2+2];

		//Build huffman tree.
		for( i = 0; i < glyphct; i++ )
		{
			struct huff_for_sort * h = &hfs[i];
			hfs[i].ptr = i;
			hfs[i].qty = gglyphs[i].qty;
			hfs[i].oqty = gglyphs[i].qty;
			ht[i].left = -1;
			ht[i].right = -1;
		}

		for( ; i < glyphct*2; i++ )
		{
			hfs[i].ptr = i;
			hfs[i].qty = 0;
			hfs[i].oqty = 0;
			ht[i].left = -1;
			ht[i].right = -1;
		}
		int next_tree = glyphct;

/*
struct huff_for_sort
{
	int      ptr;
	int      qty;
};
struct huff_tree
{
	uint16_t left;	//If MSB set, is leaf.  Otherwise is tree pointer.
	uint16_t right;
};
*/
		int valid_hfsct = glyphct;

		int iteration = 0;

		for( iteration = 0; ; iteration++ )
		{
			qsort( hfs, valid_hfsct, sizeof( hfs[0] ), &compare_huff );

			struct huff_for_sort * nhs = &hfs[next_tree];
			struct huff_tree *   nht = &ht[next_tree];

			//If no more pairs, break;
			if( hfs[1].qty == 0x7fffffff ) break;

			//Join the first two elements into the next huff_tree.
			nhs->oqty = nhs->qty = hfs[0].qty + hfs[1].qty;
			nhs->ptr = next_tree;
			nht->left = hfs[0].ptr;
			nht->right = hfs[1].ptr;
			//printf( "Rolling up %d = %04x %04x\n", next_tree, nht->left, nht->right );
			valid_hfsct++;
			next_tree++;

			//Take the leaves out of the running.
			hfs[0].qty = 0x7fffffff;
			hfs[1].qty = 0x7fffffff;
		}
		printf( "Stopped after %d\n", iteration );

		//qsort( hfs, valid_hfsct, sizeof( hfs[0] ), &compare_huff );
		//Already re-sorted.

		//Now, write addresses into all of the tree nodes.
		FillOutTree( hfs[0].ptr, &ht[0], 0, 0 );

#if 1
		for( i = 0; i < glyphct*2-1; i++ )
		{
			int gid = hfs[i].ptr;

			if( gid < glyphct )
			{
				struct glyph * g = &gglyphs[gid];
				printf( "MZ: %4d %4d %5d %d %d  [[%d %lx]] \n", gid, i, hfs[i].oqty, g->flag, g->dat.runlen, ht[gid].bitdepth, ht[gid].bitpattern );
			}
			else
			{
				printf( "MY: [(%d, %d)NODE %d %d (%d %d)] [[%d %lx]]\n", hfs[i].oqty, gid, ht[gid].left, ht[gid].right, hfs[i].ptr, hfs[i].qty, ht[gid].bitdepth, ht[gid].bitpattern );
			}
		}
#endif

		//Now, we need to make the output bit stream that would match this huff table.
		int totalbits = 0;
		int faults = 0;
		int tcells = 0;
		for( i = 0; i < mapelem; i++ )
		{
			int mo = mapout[i];
			totalbits += ht[mo].bitdepth;
			//if( ht[mo].bitdepth < 5 ) faults++;
			struct glyph * g = &gglyphs[mo];
			//printf( "XO %6d -> %d -> %16x\n", mo, g->flag, g->dat );
			if( g->flag )
				tcells += g->dat.runlen;
			else
				tcells++;
			//printf( "MO: %d %5d %2d %10x   %2d %16x  %d\n", i, mo, ht[mo].bitdepth, ht[mo].bitpattern, g->flag, g->dat, tcells  );
		}
		printf( "Total cells: %d [please check this (Should be w*h*frames/TILE_SIZE]\n", tcells );
		printf( "Total frames: %d\n", tcells/(EXP_W*EXP_H/(TILE_W*TILE_H)) );
		printf( "Total maps: %d\n", mapelem );
		printf( "Total bits: %d (Of bitstream, not glyphs/maps)\n", totalbits );
		printf( "Total bytes: %d (Of bitstream, not glyphs/maps)\n", (totalbits+7)/8 );
		printf( "Total huffman entries: %d\n", glyphct*2-1 );
		printf( "Symbols: %d (Groups of %d x %d pixels in dictionary)\n", initgglyphs, TILE_W, TILE_H );
		printf( "RLEs: %d\n", nr_rles );

		int nr_huffs = glyphct-1;  //We know that there are exactly this many huffman nodes because of the way the trees are generated.

		FILE * outc = fopen( "outdata.c", "w" );
		fprintf( outc, "#include \"outsettings.h\"\n" );
		fprintf( outc, "#include <stdint.h>\n" );

		int glyphelemlist = 0;
		int rleelemlist = 0;
		int huffelemlist = 0;

		{
			tiledata tiles[initgglyphs];
			for( i = 0; i < initgglyphs; i++ )
			{
				SetGlyph( tiles[i], gglyphs[i].dat.dat );
			}
			f = fopen( "outglyph.dat", "wb" );
			fwrite( tiles, sizeof(tiles), 1, f );
			fclose( f );
			OutputBufferToFile( outc, "glyphdata", "uint32_t", 4, sizeof(tiles), (uint8_t*)tiles );
			glyphelemlist = sizeof(tiles)/4;
		}
		{
			f = fopen( "outrles.dat", "wb" );
			T_RLE rles[nr_rles];
			for( i = 0; i < nr_rles; i++ )
			{
				rles[i] = gglyphs[i+initgglyphs].dat.runlen;
				//If white, mark it as such in the RLE tag.
				if( gglyphs[i+initgglyphs].flag == 3 ) //White
				{
					rles[i] |= 0x8000;
				}
				if( gglyphs[i+initgglyphs].flag == 2 ) //Black
				{
					rles[i] |= 0x4000;
				}
			}
			fwrite( rles, sizeof(rles), 1, f );
			fprintf( outc, "\n//RLE Data MSB's are the length, and the lsb is whether it's white or black.\n" );
			OutputBufferToFile( outc, "rledata", (sizeof(T_RLE)<2)?"uint8_t":"uint16_t", sizeof(T_RLE), sizeof(rles), (uint8_t*)rles );
			fclose( f );
			rleelemlist = sizeof(rles)/sizeof(T_RLE);
		}
		printf( "IG in hex: %04x\n", initgglyphs );
		{
			f = fopen( "hufftable.dat", "wb" );
			int huffroot = hfs[0].ptr;

			uint16_t huffs[nr_huffs*2];
			for( i = 0; i < nr_huffs; i++ )
			{
				int huffno = glyphct + i;
				int l = ht[huffno].left;
				int r = ht[huffno].right;

				if( l >= initgglyphs + nr_rles ) l -= nr_rles + initgglyphs;
				else if( l >= initgglyphs ) l = (l - initgglyphs) | 0xc000; //RLE
				else l = l | 0x8000;  //Regular data

				if( r >= initgglyphs + nr_rles ) r -= nr_rles + initgglyphs;
				else if( r >= initgglyphs ) r = (r - initgglyphs) | 0xc000; //RLE
				else r = r | 0x8000;  //Regular glyph

				huffs[0+i*2] = l;
				huffs[1+i*2] = r;
			}
			fwrite( huffs, sizeof(huffs), 1, f );
			fclose( f );

			fprintf( outc, "//0x8000 = glyph, 0xc000 = rle, otherwise, points inside table.\n" );
			
			OutputBufferToFile( outc, "huffdata", "uint16_t", 2, sizeof(huffs), (uint8_t*)huffs );
			huffelemlist = sizeof(huffs)/2;
		}
		fclose( outc );


		//Last step (For tomorrow) encode the data!  Specifically, mapout[mapelem++] and how it maps to ht[gid].bitdepth
		{
			f = fopen( "outvideo.dat", "wb" );
			uint32_t outbuffer = 0;
			int outplace = 0;
			int totalbits = 0;
			for( i = 0; i < mapelem; i++ )
			{
				int me = mapout[i];
				uint64_t bitpattern = ht[me].bitpattern;   //lsbit first.
				int      bitdepth = ht[me].bitdepth;
				int j;
				//printf( "TOK: %04x %d\n", me, i );
				for( j = 0; j < bitdepth; j++ )
				{
					int bit = bitpattern & 1;
					bitpattern>>=1;
					outbuffer |= bit << outplace;
					outplace++;
					if( outplace == 32 )
					{
						fwrite( &outbuffer, 4, 1, f );
						//printf( "DUMP %08x\n", outbuffer );
						outbuffer = 0;
						outplace = 0;
					}
					//printf( "%d", bit );
					totalbits++;
				}
				//printf( "\n" );
			}
			if( outplace )
			{
				fwrite( &outbuffer, 4, 1, f );
			}

			fclose( f );
		}
		f = fopen( "outsettings.h", "w" );
		fprintf( f, "#ifndef _BADAPPLE_SETTINGS_H\n" );
		fprintf( f, "#define _BADAPPLE_SETTINGS_H\n\n" );
		fprintf( f, "#include <stdint.h>\n" );
		fprintf( f, "#define TILE_W %d\n", TILE_W );
		fprintf( f, "#define TILE_H %d\n", TILE_H );
		fprintf( f, "#define SFILLE %d\n", SFILL );
		fprintf( f, "#define NR_TILES %d\n", initgglyphs );
		fprintf( f, "#define NR_RLES %d\n",  nr_rles );
		fprintf( f, "#define NR_HUFFS %d\n",  nr_huffs );
		fprintf( f, "#define ROOT_HUFF %d\n", (hfs[0].ptr-glyphct) );
		fprintf( f, "#define FWIDTH %d\n", EXP_W );
		fprintf( f, "#define FHEIGHT %d\n", EXP_H );
		fprintf( f, "#define TOTALBITS %d\n", totalbits );
		fprintf( f, "#define FRAMES %d\n", tcells/(EXP_W*EXP_H/(TILE_W*TILE_H)) );
		fprintf( f, "extern const uint32_t glyphdata[%d];\n", glyphelemlist );
		fprintf( f, "extern const %s rledata[%d];\n", (sizeof(T_RLE)<2)?"uint8_t":"uint16_t", rleelemlist );
		fprintf( f, "extern const uint16_t huffdata[%d];\n", huffelemlist );
		fprintf( f, "\n#endif\n" );

		printf( "Grand Total Payload bytes: %d\n", (totalbits+7)/8 + (initgglyphs*TILE_W*TILE_H/8) + ((sizeof(T_RLE)<2)?1:2)*rleelemlist + huffelemlist * 2 );

		fclose( f );


	//	mapout[mapelem++] = tglyph;
/*
		f = fopen( "tile_with_rle.dat", "wb" );
		//glyphct = tileout;
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );

		//printf( "LEN: %d / max: %d\n", len, maxgf );
		*/
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
	fprintf( stderr, "Stage: 3: Restrict tile set based on video, also produce tile matches. (Run this step multiple times with decreasing ranges, down to 254 or whatever the desired # of symbols is.)\n" );
	fprintf( stderr, "Stage: 5: Try to compress output data set.\n" );
	return -1;
}

