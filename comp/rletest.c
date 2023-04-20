#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"
#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"
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

// Compression amounts are w/o hufftile on.
//#define COMPWIDTH 255		// 5025397 bytes @ 280x240
//#define COMPWIDTH 4095	// 6923202 bytes @ 280x240
//#define COMPWIDTH 65535		// 9188240 bytes @ 280x240
#define COMPWIDTH 1073741824	// 48134919/8 = 6016865 bytes @ 280x240

//#define USE_HUFF_TREE 0

// With huff tree  (TODO: Finish this)
#define USE_HUFF_TREE 1




uint32_t histogram[1<<25];

#if COMPWIDTH > 65535
uint32_t compbuffer[67108864];
uint8_t odatastream[67108864];
int odatatbitcount = 0;
int comppl = 0;
int lastcomp = 0;
int color = 0;
int runningtime = 0;
#endif


int intlog2( unsigned int v )
{
	unsigned int r = 0;
	while (v >>= 1)
	{
		r++;
	}
	return r;
}


void got_video_frame( const unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	static int notfirst;
	int i, x, y;

	if( !notfirst )
	{
		CNFGSetup( "badapple", width, height );
		notfirst = 1;
	}

#if COMPWIDTH > 65535
	int color = 0;
	int runningtime = 0;
	int comppl = 0;
	uint16_t compbuffer[262144];
	int startbl = odatatbitcount;
#elif COMPWIDTH > 255
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
				compbuffer[comppl++] = COMPWIDTH;
				runningtime = 0;
			}
		}
	}

//#if COMPWIDTH > 65535
//#else
	compbuffer[comppl++] = runningtime;
//#endif

#if COMPWIDTH == 1073741824
//	static int ocolor = 0;
//	static int thispl = -1;
	int ocolor = 0;
	int thispl = 1;
	for( i = 0; i < comppl; i++ )
	{
		int cp = compbuffer[i];
		ETEmitUE( odatastream, sizeof(odatastream), &odatatbitcount, cp );
		histogram[cp]++;
	}
	wordcount += (comppl-lastcomp);
#elif COMPWIDTH == 4095
	int thispl = 1;
	int ocolor = 0;
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
#elif COMPWIDTH == 255
	int ocolor = 0;
	fwrite( compbuffer, comppl, 1, f );
	wordcount += comppl;
	int thispl = 1;
#elif COMPWIDTH == 65535
	int ocolor = 0;
	fwrite( compbuffer, comppl, 2, f );
	wordcount += comppl*2;
	int thispl = 1;
#else
	#error not-outputtable-compwidth
#endif

	int orunningtime = compbuffer[thispl-1];
	int dontinvert = 0;
	if( orunningtime == COMPWIDTH ) dontinvert = 1;
	uint32_t data[width*height];
	int dpl = 0;
	for( y = 0; y < height; y++ )
	for( x = 0; x < width; x++ )
	{
		if( orunningtime == 0 )
		{
			if( !dontinvert ) ocolor = !ocolor;
			dontinvert = 0;
			orunningtime = compbuffer[thispl++];
			if( orunningtime == COMPWIDTH ) dontinvert = 1;
		}
		else
			orunningtime--;

		data[dpl++] =(ocolor)?0xffffff:0x000000 ; 
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
//#if COMPWIDTH > 65535
//	printf( "%d %d %d %d -> %d %d\n", frame, linesize, width, height, comppl-lastcomp, odatatbitcount - startbl );
//	lastcomp = comppl;
//#else
	printf( "%d %d %d %d -> %d\n", frame, linesize, width, height, comppl );
//#endif
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
	// Show histogram
	int nohsum = 0;
	int totalhs = 0;
	int totalhsum = 0;
	int nohcount = 0;
	for( i = 0; i < sizeof(histogram)/sizeof(histogram[0]); i++ )
	{
		int h = histogram[i];
		if( h >= 8 )
		{
			printf( "%d: %d\n", i, histogram[i] );
			totalhs++;
			totalhsum += h;
		}
		else if( h > 0 )
		{
			nohsum += h;
			nohcount++;
		}
	}
	printf( "Total Hs: %d\nTotal HSum: %d\nNo-H Count: %d\nNo-H Sum: %d\n", totalhs, totalhsum, nohcount, nohsum );
	printf( "\n" );
#endif
	printf( "Total: %d symbols/bytes\n", wordcount );

#if COMPWIDTH > 65535
	printf( "Total Bits: %d\n", odatatbitcount );
#endif


#if USE_HUFF_TREE
	int huffct = 0;
	int huffcutoff = 8;
	for( i = 0; i < sizeof(histogram)/sizeof(histogram[0]); i++ )
	{
		if( histogram[i] >= huffcutoff )
		{
			huffct++;
		}
	}

	// We make a huffman tree, but the codes in the tree are the bit runs + 32.
	// This is so if we encounter a code whose code is < 32, that indicates
	// that there immediately follows a raw number encoded with that number of
	// bits.

	huffct+=32; // We need a code for all the huffs' that didn't make the cut.
	uint32_t * huffvals = calloc( sizeof( uint32_t ), huffct + 32 );
	uint32_t * hufffreqs = calloc( sizeof( uint32_t ), huffct + 32 );
	int h = 32; // first 32 are fixed codepoints.
	int highestval = 0;

	for( i = 0; i < sizeof(histogram)/sizeof(histogram[0]); i++ )
	{
		int ct = histogram[i];
		if( ct >= huffcutoff )
		{
			// If it exists and is common, it gets in the normal table.
			huffvals[h] = i+32;
			highestval = i+32;
			hufffreqs[h] = ct;
			h++;
		}
		else if( ct )
		{
			// If it exists but is uncommon, it just gets a # of bits.
			int num_bits_in_ct = intlog2( i );
			huffvals[num_bits_in_ct] = num_bits_in_ct;
			hufffreqs[num_bits_in_ct]++;
		}
	}

	printf( "HHUFFS: %d\n", h );
	int hufflen = 0;
	huffelement * tree = GenerateHuffmanTree( huffvals, hufffreqs, h, &hufflen );

	printf( "HHUFFS LL: %d\n", hufflen );

        for( i = 0; i < hufflen; i++ )
        {
                huffelement * e = tree + i;
                printf( "%d: %d [%d %d] [VAL %d] FREQ %d\n", i, e->is_term, e->pair0, e->pair1, e->value, e->freq );
        }

	int htlen = 0;
	huffup * bitlists = GenPairTable( tree, &htlen );

	huffup ** flatlook = calloc( sizeof( huffup * ), highestval+1 );
	for( i = 0; i < htlen; i++ )
	{
		assert( bitlists[i].value <= highestval );
		flatlook[bitlists[i].value] = &bitlists[i];
		printf( "%d : %d %d\n", i, bitlists[i].value, bitlists[i].bitlen );
	}
	return -9;
	int totalbits = 0;
	for( i = 0; i < comppl; i++ )
	{
		int symv = compbuffer[i] + 32;
		if( symv <= highestval )
		{
			if( flatlook[symv] )
			{
				totalbits += flatlook[symv]->bitlen;
				continue;
			}
			else printf( "%d %p\n", symv, flatlook[symv] );
		}

		int sl = intlog2( compbuffer[i] );
		if( flatlook[sl] == 0 )
		{
			printf( "ERROR: COULD NOT FIND CODE FOR SPACING %d\n", compbuffer[i] );
		}
		else
		{
			totalbits += flatlook[sl]->bitlen;
			totalbits += sl;
		}
	}

	printf( "Bitstream Length Bits: %d\n", totalbits );

#endif

}

