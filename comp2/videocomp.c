#include <stdio.h>

#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

#define BLOCKSIZE 8
#define HALFTONE  1
typedef uint64_t blocktype;

struct block
{
	blocktype blockdata;
	float intensity[BLOCKSIZE*BLOCKSIZE]; // For when we start culling blocks.
	int count;
};

struct block * allblocks;
int numblocks;

void AppendBlock( blocktype b )
{
	int i;
	struct block * check = allblocks;
	struct block * bend = check + numblocks;

	while( check != bend )
	{
		if( check->blockdata == b ) break;
		check++;
	}

	if( check == bend )
	{
		numblocks++;
		allblocks = realloc( allblocks, numblocks * sizeof( struct block ) );
		check = allblocks + (numblocks - 1);
		memset( check, 0, sizeof( struct block ) );
		check->count = 0;
		check->blockdata = b;
	}
	check->count++;
	for( i = 0; i < BLOCKSIZE * BLOCKSIZE; i++ )
	{
		check->intensity[i] += !!( b & ( 1<< i ) );
	}
}

blocktype ExtractBlock( uint8_t * image, int iw, int ih, int x, int y )
{
	int stride = (BLOCKSIZE)*iw;
	uint8_t * iof = image + (y * stride) + (x * BLOCKSIZE);
	blocktype ret = 0;
	int ix, iy;
	int bpl = 0;
	for( iy = 0; iy < 8; iy++ )
	{
		for( ix = 0; ix < 8; ix++ )
		{
			uint8_t c = iof[ix];
			printf( "%3d %16llx ", c, ret );

#ifdef HALFTONE
			if( c > 190 ) ret |= 1ULL<<bpl;
#else
			int evenodd = (x+y)&1;
			if( c > 80+evenodd*120 ) ret |= 1ULL<<bpl;
#endif

/*			else if( c > 100 )
			{
				if( (x^y)&1 ) ret |= 1ULL<<bpl;
			}
*/
			bpl++;
		}
		iof += stride;
		printf( "-- %d %d %d %d %016llx\n", stride, ix, iy, bpl, ret );
	}
//	printf( "%llx\n", ret );
	return ret;
}

void DrawBlock( int xofs, int yofs, blocktype b )
{
	uint32_t boo[BLOCKSIZE*BLOCKSIZE] = { 0 };
	int i;
	for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
	{
		if( b & (1<<i) )
			boo[i] = 0xffffffff;
		else
			boo[i] = 0xff000000;
	}
	CNFGBlitImage( boo, xofs, yofs, BLOCKSIZE, BLOCKSIZE );
}


int main( int argc, char ** argv )
{
	char cts[1024];

	if( argc != 5 ) goto fail;

	int w = atoi( argv[2] );
	int h = atoi( argv[3] );
	if( w <= 0 || h <= 0 ) goto fail;
	int x, y, i;

	FILE * f = fopen( argv[1], "rb" );
	uint8_t * tbuf = malloc( w * h );
//	uint32_t * tempc = malloc( w * h * 4 );

	CNFGSetup( "comp test", 640, 480 );
	while( 1 )
	{
		CNFGClearFrame();
		if( !CNFGHandleInput() ) break;
		int r = fread( tbuf, w*h, 1, f );

		if( r < 1 ) break;

		//for( i = 0; i < w*h; i++ )
		//	tempc[i] = tbuf[i] << 0 | tbuf[i] << 8 | tbuf[i] << 16 | 0xff000000;

		for( y = 0; y < h / BLOCKSIZE; y++ )
		for( x = 0; x < w / BLOCKSIZE; x++ )
		{
			blocktype b = ExtractBlock( tbuf, w, h, x, y );
			DrawBlock( x*BLOCKSIZE, y*BLOCKSIZE, b );
			AppendBlock( b );
		}

		sprintf( cts, "%d\n", numblocks );
		CNFGPenX = w + 1;
		CNFGPenY = 0;
		CNFGColor( 0xffffffff );
		CNFGDrawText( cts, 2 );

		CNFGSwapBuffers();
		usleep(100000);
	}
	printf( "Found %d glyphs\n", numblocks );

	return 0;
fail:
	fprintf( stderr, "Error: Usage: videocomp [file] [w] [h] [out gif]\n" );
	return -9;

}
 
