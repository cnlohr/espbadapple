#include <stdio.h>
#include "DrawFunctions.h"
#include "ffmdecode.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>


int gwidth;
int gheight;
int firstframe = 1;
int maxframe;
int * framenos;

#define TILE 16
#define LIMIT 0x40
#define SFILL 3


#define W_DECIMATE 1
#define H_DECIMATE 1

#define MAKE_GIF_AT_STAGE_3

#ifdef MAKE_GIF_AT_STAGE_3
#include "gifenc.h"
static ge_GIF *gif;
#endif

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

#define EXP_W (512>>W_DECIMATE)
#define EXP_H (384>>H_DECIMATE)
#define MAXGLYPHS 600000

#if TILE==8
typedef uint64_t tiledata;
#define GlyphsEqual( a, b ) ((a) == (b))
#define SetGlyph( a, b ) a = b;
#elif TILE==16
typedef uint16_t tiledata[16];
#define GlyphsEqual( a, b ) ( ((uint64_t*)a)[0] == ((uint64_t*)b)[0] && ((uint64_t*)a)[1] == ((uint64_t*)b)[1] && ((uint64_t*)a)[2] == ((uint64_t*)b)[2] && ((uint64_t*)a)[3] == ((uint64_t*)b)[3] )
#define SetGlyph( a, b ) memcpy( a, b, 32 );
#else
#error TILE must be 8 or 16.
#endif

struct glyph
{
	uint8_t  flag;
	int      qty;
	union
	{
		tiledata dat;
		uint32_t runlen;
	} dat;
};
struct glyph gglyphs[MAXGLYPHS];
int glyphct;


int stage;
//Used in mode 3.
int total_quality_loss;
int highest_used_symbol = 0;
float weight_toward_earlier_symbols;

int BitDiff( tiledata a, tiledata b )
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

#if TILE==8
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
#elif TILE==16
	int ct = 0;
	for( i = 0; i < 16; i++ )
	{
		uint16_t diff = a[i]^b[i];
		ct += BitsSetTable256[diff>>8] + BitsSetTable256[diff & 0xff];
	}

#endif

	return ct;
}

void got_video_frame( const unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	static int notfirst;
	int i, x, y;
	int comppl = 0;

	width>>=W_DECIMATE;
	height>>=H_DECIMATE;

	if( (width % TILE) || (height % TILE)) 
	{
		fprintf( stderr, "Error: width is not divisible by TILE.\n" );
		exit( -1 );
	}

	if( !notfirst )
	{
		CNFGSetup( "badapple", width, height );
		notfirst = 1;
	}

	int color = 0;
	int runningtime = 0;

	int glyphs = width/TILE*height/TILE;
	tiledata thisframe[glyphs];

	//encode
	for( y = 0; y < height; y+=TILE )
	for( x = 0; x < width; x+=TILE )
	{
		int bitx, bity;
		tiledata glyph;
		int glyphbit = 0;

#if TILE == 8
		glyph = 0;
		for( bity = 0; bity < 8; bity++ )
		for( bitx = 0; bitx < 8; bitx++ )
		{
			int on = rgbbuffer[(((x+bitx)*3)<<W_DECIMATE)+((y+bity)<<H_DECIMATE)*linesize]>LIMIT;
			glyph <<= 1;
			glyph |= on;
		}
		thisframe[(x/8)+(y*width)/64] = glyph;
#elif TILE == 16
		for( bity = 0; bity < 16; bity++ )
		{
			int tglyph = 0;
			for( bitx = 0; bitx < 16; bitx++ )
			{
				int on = rgbbuffer[(((x+bitx)*3)<<W_DECIMATE)+((y+bity)<<H_DECIMATE)*linesize]>LIMIT;
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
				if( BitDiff( tg, gglyphs[h].dat.dat ) < 4 )
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
					if( bias > (TILE*TILE/2) ) break;

					diff = BitDiff( targ, gl ) + bias;
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
			if( i > highest_used_symbol ) highest_used_symbol = i;
			glyphmap[g] = i;
			gglyphs[i].qty++;
		}
		fwrite( glyphmap, sizeof( glyphmap ), 1, f );

		for( y = 0; y < height/TILE; y++ )
		for( x = 0; x < width/TILE; x++ )
		{
			tiledata glyphdata;
			SetGlyph( glyphdata, gglyphs[glyphmap[x+y*(width/TILE)]].dat.dat );
			int lx = x * TILE;
			int ly = y * TILE;
			int px, py;
			for( py = 0; py < TILE; py++ )
			for( px = 0; px < TILE; px++ )
			{
#if TILE==8
				uint64_t bit = (glyphdata>>(63-(px+py*8))) & 1;
#else
				int bit = glyphdata[py] & (1<<(15-px));
#endif
				data[px+x*TILE+(py+y*TILE)*width] |= bit?0xf00000:0x00000000;
			}
		}

		for( y = 0; y < height; y++ )
		for( x = 0; x < width; x++ )
		{
			int on = rgbbuffer[(x)*3+(y)*linesize]>LIMIT;
			//data[x+y*width] |= on?0xf0:0x00;
		}
		CNFGUpdateScreenWithBitmap( (long unsigned int*)data, width, height );

		printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );

		maxframe = frame;


#ifdef MAKE_GIF_AT_STAGE_3
		if( !gif )
		{
			gif = ge_new_gif(
				"example.gif",  /* file name */
				width, height,           /* canvas size */
				(uint8_t []) {  /* palette */
				    0x00, 0x00, 0x00, /* 0 -> black */
				    0xFF, 0xff, 0xff, /* 1 -> white */
				},
				1,              /* palette depth == log2(# of colors) */
				1               /* infinite loop */
    			);
		}
		if( frame > 5000 && frame < 6600 )
		{
			for( y = 0; y < height; y++ )
			for( x = 0; x < width; x++ )
			{
				gif->frame[(x+y*width)] = data[x+y*width]?1:0;
			}
	        ge_add_frame(gif, 3);
		}
#endif

		//usleep(20000);

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
		int line;
		setup_video_decode();

		video_decode( argv[2] );

		f = fopen( "rawtiledata.dat", "wb" );
		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
		printf( "Total: %d glyphs\n", glyphct );
	}
	else if( stage == 2 )
	{
		//CURRENTLY 2 isn't used, but we should probably use 2 to recondense the glyph dictionaries.

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
				printf( "%6d / %6d / %016lx\n", i, gglyphs[i].qty, gglyphs[i].dat.dat );
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
		if( argc < 5 )
		{
			fprintf( stderr, "Error: stage 3 but need:  3 [avi] [nr_of_tiles] [weight for symbols earlier in the list, float]\n" );
			return -9; 
		}
		f = fopen( "rawtiledata.dat", "rb" );
		if( fread( &glyphct, sizeof( glyphct ), 1, f ) != 1 ) goto iofault;
		if( fread( gglyphs, sizeof( gglyphs[0] ), glyphct, f ) != glyphct ) goto iofault;
		fclose( f );
		f = fopen ("outgframes.dat", "wb" );

		qsort( gglyphs, glyphct, sizeof( gglyphs[0] ), &compare_ggs );
		int tileout = atoi( argv[3] );
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
		weight_toward_earlier_symbols = atof( argv[4] );

		setup_video_decode();
		video_decode( argv[2] );

		highest_used_symbol++;
		printf( "Writing %d/%d Symbols\n", highest_used_symbol, tileout );
		if( highest_used_symbol < tileout ) tileout = highest_used_symbol;

		for( i = 0; i < tileout; i++ )
		{
			printf( "%6d: %6d %16lx\n", i, gglyphs[i].qty, ((uint64_t*)gglyphs[i].dat.dat)[0] );
			tquat+= gglyphs[i].qty;
		}
		printf( "Total: %d\n", tquat );
		f = fopen( "rawtiledata.dat", "wb" );
		glyphct = tileout;
		fwrite( &glyphct, sizeof( glyphct ), 1, f );
		fwrite( gglyphs, sizeof( gglyphs[0] ), glyphct, f );
		fclose( f );
		printf( "Quality loss: %d\n", total_quality_loss );
#ifdef MAKE_GIF_AT_STAGE_3
	    ge_close_gif(gif);
#endif

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
#ifdef SFILL
		uint32_t * gfdat = malloc(len*4);
		int linecells = EXP_W/TILE;
		for( i = 0; i < len; i++ )
		{
			int frame = i / (EXP_W*EXP_H/(TILE*TILE));
			int cellinframe = i % (EXP_W*EXP_H/(TILE*TILE));

			int lower = cellinframe & ((1<<SFILL)-1);
			int upper = cellinframe & (((EXP_W/TILE)-1)<<SFILL); ///XXX TODO This bit math might be wrong.

			//int mask = lower * 512/8;
			int x = upper>>SFILL;
			int y = lower + ((cellinframe/((EXP_W/TILE)<<SFILL))<<SFILL);
			//printf( "(%d %d)\n", x, y );
			//printf( "(%d,%d,%d)\n",cellinframe, x, y );
			gfdat[i] = gfdat_raw[x+y*linecells+frame*(EXP_W*EXP_H/(TILE*TILE))];
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
		//Tricky - we actually want to remove the first two glyphs, since 0 and 1 will be RLE encoded.
		for( i = 0; i < glyphct-2; i++ )
		{
			memcpy( gglyphs + i, gglyphs + i + 2, sizeof( gglyphs[0] ) );
		}
		glyphct-=2;
		memset( gglyphs + i, 0, sizeof( gglyphs[0] ) * 2 );
		for( i = 0; i < len; i++ )
		{
			int tglyph = gfdat[i];
			//printf( "%d/%d\n", tglyph, initgglyphs );
			//Detect first two glyphs.  They're special.  Need to RLE them.
			if( tglyph == 0 || tglyph == 1 )
			{
				int runlen = 1;
				i++;
				for( ; i < len; i++ )
				{
					if( gfdat[i] == tglyph ) runlen++;
					else break;
				}
				i--;

				//printf( "RUNLEN: %d\n", runlen );

				tqcells += runlen;

				int flag = tglyph+1;
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
				struct glyph * g = &gglyphs[tglyph-2];
				g->qty++;
				tqcells ++;
				mapout[mapelem++] = tglyph-2;
				if( tglyph-2 >= initgglyphs )
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
				printf( "MZ: %4d %4d %5d %d %16lx   [[%d %lx]] \n", gid, i, hfs[i].oqty, g->flag, g->dat.dat, ht[gid].bitdepth, ht[gid].bitpattern );
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
		printf( "Total cells: %d [please check this]\n", tcells );
		printf( "Total frames: %d\n", tcells/(EXP_W*EXP_H/(TILE*TILE)) );
		printf( "Total maps: %d\n", mapelem );
		printf( "Total bits: %d\n", totalbits );
		printf( "Total bytes: %d\n", (totalbits+7)/8 );
		printf( "Total huffman entries: %d\n", glyphct*2-1 );
		printf( "Glyphs: %d\n", glyphct );
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

