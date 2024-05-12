#include <stdio.h>
#include <math.h>
#include <stdbool.h>

// Only used in compute distance function.
#include <immintrin.h>

#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }


// Doing MSE flattens out the glyph usage
// BUT by not MSE'ing it looks the same to me
// but it "should" be better 
//#define MSE

// Target glyphs, and how quickly to try approaching it.
#define TARGET_GLYPH_COUNT 256
#define GLYPH_COUNT_REDUCE_PER_FRAME 5
// How many glpyhs to start at?
#define KMEANS 4096
// How long to train?
#define KMEANSITER 512


#define BLOCKSIZE 8
#define HALFTONE  1
typedef uint64_t blocktype;

int video_w;
int video_h;
int num_video_frames;
uint8_t * rawVideoData;
const char * tilesFile;
const char * streamFile;

struct block
{
	float intensity[BLOCKSIZE*BLOCKSIZE]; // For when we start culling blocks.
	blocktype blockdata;
	uint32_t count;
	uint32_t scratch;
	uint64_t extra1;
	uint64_t extra2;
};

struct block * allblocks;
struct block * allblocks_alloc;
int numblocks;

void DrawBlock( int xofs, int yofs, struct block * bb, int boolean );

void UpdateBlockDataFromIntensity( struct block * k )
{
	int i;
	blocktype bt = 0;
	for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
	{
		if( k->intensity[i] > 0.5 )
			bt |= (1ULL<<i);
	}
	k->blockdata = bt;
}


float ComputeDistance( const struct block * b, const struct block * c )
{
	int j;
	float diff = 0;
/*	const float * ib = b->intensity;
	const float * ic = c->intensity;
	for( j = 0; j < BLOCKSIZE*BLOCKSIZE; j++ )
	{
		float id = (ib[j] - ic[j]);
		//diff += id*id;
		if( id < 0 ) id *= -1;
		diff += id;
	}
	return  diff ;
*/

	__m256 rundiff = _mm256_set1_ps( 0 );
	__m256 negzero = _mm256_set1_ps( -0.0f );
	for( j = 0; j < BLOCKSIZE*BLOCKSIZE; j+=8 )
	{
		__m256 ib, ic, diff;
		ib = _mm256_load_ps( &b->intensity[j] );
		ic = _mm256_load_ps( &c->intensity[j] );
		diff = _mm256_sub_ps( ib, ic );

#ifdef MSE
		diff = _mm256_mul_ps( diff, diff );
#else
		diff = _mm256_andnot_ps( negzero, diff );
#endif

		rundiff = _mm256_add_ps( rundiff, diff );
	}

	__m128 front = _mm256_extractf128_ps( rundiff, 0 );
	__m128 back  = _mm256_extractf128_ps( rundiff, 1 );
	__m128 vdiff  = _mm_hadd_ps( front, back );
	float diffA = vdiff[0] + vdiff[1] + vdiff[2] + vdiff[3];
	//float diffB = rundiff[0] + rundiff[1] + rundiff[2] + rundiff[3] + rundiff[4] + rundiff[5] + rundiff[6] + rundiff[7];
	diff = diffA;

#ifdef MSE
	return sqrt(diff);
#else
	return diff;
#endif
}

void BlockFillIntensity( struct block * b )
{
	int i;
	for( i = 0; i < BLOCKSIZE * BLOCKSIZE; i++ )
	{
		b->intensity[i] = !!( b->blockdata & ( 1ULL << i ) );
	}
}

struct block * AppendBlock( blocktype b )
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
		allblocks_alloc = realloc( allblocks_alloc, numblocks * sizeof( struct block ) + 32 );
		allblocks = (struct block*) (((uintptr_t)(((uint8_t*)allblocks_alloc)+31))&(~31)); // force alignment
		check = allblocks + (numblocks - 1);
		memset( check, 0, sizeof( struct block ) );
		check->count = 0;
		check->blockdata = b;
	}
	check->count++;
	BlockFillIntensity( check );
	return check;
}

blocktype ExtractBlock( uint8_t * image, int iw, int ih, int x, int y )
{
	int stride = iw;
	uint8_t * iof = image + (y * BLOCKSIZE * stride) + (x * BLOCKSIZE);
	blocktype ret = 0;
	int ix, iy;
	int bpl = 0;

	for( iy = 0; iy < 8; iy++ )
	{
		for( ix = 0; ix < 8; ix++ )
		{
			uint8_t c = iof[ix];

#ifndef HALFTONE
			if( c > 190 ) ret |= 1ULL<<bpl;
#else
			int evenodd = (ix+iy)&1;
			if( c > 100+evenodd*60 ) ret |= 1ULL<<bpl;
#endif

			bpl++;
		}
		iof += stride;
	}
	return ret;
}



void ComputeKMeans()
{
	struct block kmeanses[KMEANS] __attribute__((aligned(256))) = { 0 } ;

	// Computed average
	float mkd_val[KMEANS][BLOCKSIZE*BLOCKSIZE];
	float mkd_cnt[KMEANS];
	int   kmeansdead[KMEANS] = { 0 };

	int i;
	int it;
	int km;
	for( km = 0; km < KMEANS; km++ )
	{
		kmeanses[km].blockdata = 
			(((blocktype)(rand()%0xffff))<<0ULL) |
			(((blocktype)(rand()%0xffff))<<16ULL) |
			(((blocktype)(rand()%0xffff))<<32ULL) |
			(((blocktype)(rand()%0xffff))<<48ULL);
		BlockFillIntensity( &kmeanses[km] );
	}

	int videoframeno = 0;
	for( it = 0; it < KMEANSITER; it++ )
	{
		CNFGClearFrame();
		short w,h;
		CNFGGetDimensions( &w, &h );
		memset( mkd_val, 0, sizeof( mkd_val ) );
		memset( mkd_cnt, 0, sizeof( mkd_cnt ) );

		struct block * worstfit = 0;
		float worstfitmatch = 0;
		struct block * b = allblocks;
		struct block * bend = b + numblocks;
		for( ; b != bend; b++ )
		{
			int mink = 0;
			float mka = 1e20;
			for( km = 0; km < KMEANS; km++ )
			{
				if( kmeansdead[km] ) continue;
				struct block * k = &kmeanses[km];
				float fd = ComputeDistance( b, k );
				if( fd < mka )
				{
					mka = fd;
					mink = km;
				}
			}


			if( mka > worstfitmatch )
			{
				float fd = ComputeDistance( b, &kmeanses[mink] );
				int i;
				worstfit = b;
				worstfitmatch = mka;
			}

			b->scratch = mink;

			for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
			{
				mkd_val[mink][i] += b->intensity[i] * b->count;
			}
			mkd_cnt[mink] += b->count;
		}

		int goodglyphs = 0;
		for( km = 0; km < KMEANS; km++ )
		{
			if( !kmeansdead[km] )
			{
				goodglyphs++;
			}
		}


		// Find new k's optimal intensities.
		for( km = 0; km < KMEANS; km++ )
		{
			struct block * k = &kmeanses[km];
			if( kmeansdead[km] ) continue;
			float new_intensities[BLOCKSIZE*BLOCKSIZE];
			float * kmf = k->intensity;
			float count = mkd_cnt[km];
			float * valf = mkd_val[km];
			if( count == 0 )
			{
/*
				// Mulligan
				k->blockdata = 
					(((blocktype)(rand()%0xffff))<<0ULL) |
					(((blocktype)(rand()%0xffff))<<16ULL) |
					(((blocktype)(rand()%0xffff))<<32ULL) |
					(((blocktype)(rand()%0xffff))<<48ULL);
				BlockFillIntensity( k );
*/
			}
			else
			{
				for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
				{
					new_intensities[i] = valf[i] / count;
				}
				for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
				{
					//k->intensity[i] = k->intensity[i] * 0.9 + new_intensities[i] * 0.1;
					k->intensity[i] = new_intensities[i];
				}
				UpdateBlockDataFromIntensity( k );
			}
			k->count = count;
		}

		int x, y;
		const int draww = BLOCKSIZE * 6.0;
		const int drawh = BLOCKSIZE * 5.0;
		const int drawxofs = 300;
		const int drawsperline = ceil( sqrt(goodglyphs ) );
		int dk = 0;
		for( i = 0; i < KMEANS; i++ )
		{
			if( kmeansdead[i] ) continue;
			x = dk % drawsperline;
			y = dk / drawsperline;
			dk++;

			struct block * k = &kmeanses[i];
			DrawBlock( x * draww + drawxofs, y * drawh, k, false );

			CNFGPenX = x * draww + drawxofs;
			CNFGPenY = y * drawh + BLOCKSIZE*2+2;
			char st[1024];
			sprintf( st, "%d", k->count );
			CNFGColor( 0xffffffff );
			CNFGDrawText( st, 2 );
		}

		// Fun! Every time, find the least used glyph, and assign it to a random glyph.
		// This is kind of useless.
		if( 0 )
		{
			int minct = 100000000;
			struct block * whichmink = 0;
			for( km = 0; km < KMEANS; km++ )
			{
				struct block * k = &kmeanses[km];
				
				if( k->count < minct )
				{
					minct = k->count;
					whichmink = k;
				}
			}


			if( 0 )
			{
				// random glyph (this is bad.)  --> CONSIDER: What if we pick poorly represented glyphs?
				whichmink->blockdata = worstfit->blockdata;
				BlockFillIntensity( whichmink );
				DrawBlock( 0, 400, worstfit, false );

				//printf( "%d %d %016lx\n", whichmink - kmeanses, minct, whichmink->blockdata );
			}
			else
			{
				// Mulligan
				whichmink->blockdata = 
					(((blocktype)(rand()%0xffff))<<0ULL) |
					(((blocktype)(rand()%0xffff))<<16ULL) |
					(((blocktype)(rand()%0xffff))<<32ULL) |
					(((blocktype)(rand()%0xffff))<<48ULL);
				BlockFillIntensity( whichmink );
			}
		}

		// Kill off k means
		if( 1 )
		{
			int minct = 100000000;
			int whichmink = 0;
			int killoffretry = 0;
			if( videoframeno > 1 )
			{
				int remaintokill = goodglyphs - TARGET_GLYPH_COUNT;
				if( remaintokill > GLYPH_COUNT_REDUCE_PER_FRAME ) remaintokill = GLYPH_COUNT_REDUCE_PER_FRAME;
				if( remaintokill < 0 ) remaintokill = 0;
				killoffretry = remaintokill;
			}

			int i;
			for( i = 0; i < killoffretry; i++ )
			{
				minct = 100000000;
				whichmink = 0;
				for( km = 0; km < KMEANS; km++ )
				{
					if( kmeansdead[km] ) continue;
					struct block * k = &kmeanses[km];
					
					if( k->count < minct )
					{
						minct = k->count;
						whichmink = km;
					}

					if( k->count == 0 )
						kmeansdead[km] = 1;
				}



				// Also, kill off the least loved frame.
				kmeansdead[whichmink] = 1;
			}

			int goodglyphs = 0;
			for( km = 0; km < KMEANS; km++ )
			{
				if( !kmeansdead[km] )
				{
					goodglyphs++;
				}
			}

			CNFGPenX = 0;
			CNFGPenY = 600;
			char cts[1024];
			sprintf( cts, "Glyphs: %d\nFrames: %d\n", goodglyphs,videoframeno );
			CNFGDrawText( cts, 2 );

			// Kill off later?
			//if( minct == 0 )
			//	kmeansdead[whichmink] = 1;
		}


		// Use new k-means blocks to render next video frame.
		for( y = 0; y < video_h/BLOCKSIZE; y++ )
		for( x = 0; x < video_w/BLOCKSIZE; x++ )
		{
			uint8_t * tbuf = &rawVideoData[videoframeno*video_w*video_h];
			blocktype bt = ExtractBlock( tbuf, video_w, video_h, x, y );
			struct block b = { 0 };
			b.blockdata = bt;
			BlockFillIntensity( &b );

			int mink = 0;
			float mka = 1e20;
			for( km = 0; km < KMEANS; km++ )
			{
				struct block * k = &kmeanses[km];
				if( kmeansdead[km] ) continue;
				float fd = ComputeDistance( &b, k );
				if( fd < mka )
				{
					mka = fd;
					mink = km;
				}
			}

			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2, &b, false );
			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2 + 200, &kmeanses[mink], false );
			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2 + 400, &kmeanses[mink], true );
			//memcpy( &rawVideoData[(frames-1)*video_w*video_h], tbuf, video_w*video_h );
		}

		videoframeno++;
		CNFGSwapBuffers();
	}

	// One KMeans is computed, output the glyphs to a file, and re-render the video stream.
	FILE * fTiles = fopen( tilesFile, "wb" );
	for( km = 0; km < KMEANS; km++ )
	{
		if( kmeansdead[km] ) continue;
		fwrite( &kmeanses[km].blockdata, sizeof( kmeanses[km].blockdata ), 1, fTiles );
	}
	fclose( fTiles );

	FILE * fStream = fopen( streamFile, "wb" );

	for( videoframeno = 0; videoframeno < num_video_frames; videoframeno++ )
	{
		int x, y;
		CNFGClearFrame();
		// Use new k-means blocks to render next video frame.
		for( y = 0; y < video_h/BLOCKSIZE; y++ )
		for( x = 0; x < video_w/BLOCKSIZE; x++ )
		{
			uint8_t * tbuf = &rawVideoData[videoframeno*video_w*video_h];
			blocktype bt = ExtractBlock( tbuf, video_w, video_h, x, y );
			struct block b = { 0 };
			b.blockdata = bt;
			BlockFillIntensity( &b );

			uint32_t mink = 0;
			float mka = 1e20;
			uint32_t outkmid = 0;
			for( km = 0; km < KMEANS; km++ )
			{
				struct block * k = &kmeanses[km];
				if( kmeansdead[km] ) continue;
				float fd = ComputeDistance( &b, k );
				if( fd < mka )
				{
					mka = fd;
					mink = outkmid;
				}
				outkmid++;
			}

			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2, &b, false );
			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2 + 200, &kmeanses[mink], false );
			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2 + 400, &kmeanses[mink], true );
			//memcpy( &rawVideoData[(frames-1)*video_w*video_h], tbuf, video_w*video_h );
			
			fwrite( &mink, sizeof( mink ), 1, fStream );
		}
		fclose( fStream );
		CNFGSwapBuffers();
	}

}



void DrawBlock( int xofs, int yofs, struct block * bb, int boolean )
{
	uint32_t boo[BLOCKSIZE*BLOCKSIZE] = { 0 };
	int i;
	if( boolean )
	{
		blocktype b = bb->blockdata;
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			if( b & (1ULL<<i) )
				boo[i] = 0xffffffff;
			else
				boo[i] = 0xff000000;
		}
	}
	else
	{
		float * fi = bb->intensity;
		for( i = 0; i < BLOCKSIZE*BLOCKSIZE; i++ )
		{
			float f = fi[i];
			int c = f * 255;
			if( c > 255 ) c = 255;
			if( c < 0 ) c = 0;
			boo[i] = (c) | (c<<8) | (c<<16) | (0xff000000);
		}
	}

	uint32_t bobig[BLOCKSIZE*BLOCKSIZE*4];
	int x, y;
	for( y = 0; y < BLOCKSIZE; y++ )
	for( x = 0; x < BLOCKSIZE; x++ )
	{
		uint32_t v = boo[x+y*BLOCKSIZE];
		bobig[(2*x+0) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+0)*BLOCKSIZE*2] = v;
		bobig[(2*x+0) + (2*y+1)*BLOCKSIZE*2] = v;
		bobig[(2*x+1) + (2*y+1)*BLOCKSIZE*2] = v;
	}
	CNFGBlitImage( bobig, xofs, yofs, BLOCKSIZE*2, BLOCKSIZE*2 );
}


int main( int argc, char ** argv )
{
	char cts[1024];
	srand( 0 );

	if( argc != 6 ) goto fail;

	video_w = atoi( argv[2] );
	video_h = atoi( argv[3] );
	if( video_w <= 0 || video_h <= 0 ) goto fail;
	int x, y, i;

	tilesFile = argv[4];
	streamFile = argv[5];


	FILE * f = fopen( argv[1], "rb" );
	uint8_t * tbuf = malloc( video_w * video_h );

	int frames = 0;
	CNFGSetup( "comp test", 1800, 900 );
	while( 1 )
	{
		CNFGClearFrame();
		if( !CNFGHandleInput() ) break;
		int r = fread( tbuf, video_w*video_h, 1, f );

		if( r < 1 ) break;

		for( y = 0; y < video_h / BLOCKSIZE; y++ )
		for( x = 0; x < video_w / BLOCKSIZE; x++ )
		{
			blocktype b = ExtractBlock( tbuf, video_w, video_h, x, y );
			struct block * bck = AppendBlock( b );
			DrawBlock( x*BLOCKSIZE*2, y*BLOCKSIZE*2, bck, false );
		}

		sprintf( cts, "%d\n", numblocks );
		CNFGPenX = video_w*2 + 1;
		CNFGPenY = 0;
		CNFGColor( 0xffffffff );
		CNFGDrawText( cts, 2 );

		CNFGSwapBuffers();

		frames++;
		rawVideoData = realloc( rawVideoData, frames * video_w * video_h );
		memcpy( &rawVideoData[(frames-1)*video_w*video_h], tbuf, video_w*video_h );

		//usleep(10000);
	}
	num_video_frames = frames;
	printf( "Found %d glyphs\n", numblocks );

	ComputeKMeans();

	return 0;
fail:
	fprintf( stderr, "Error: Usage: videocomp [file] [w] [h] [out gif]\n" );
	return -9;

}
 
