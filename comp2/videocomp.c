#include <stdio.h>
#include <math.h>
#include <stdbool.h>

// Only used in compute distance function.
#include <immintrin.h>

#include "bacommon.h"

int video_w;
int video_h;
int num_video_frames;
uint8_t * rawVideoData;
const char * tilesFile;
const char * streamFile;

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

	__m256 diffs = _mm256_set1_ps( 0 );
	const __m256 signmask = _mm256_set1_ps( -0.0f );
	for( j = 0; j < BLOCKSIZE*BLOCKSIZE; j += 8 )
	{
		__m256 intensityb, intensityc, diffhere;
		intensityb = _mm256_load_ps( &b->intensity[j] );
		intensityc = _mm256_load_ps( &c->intensity[j] );
		diffhere = _mm256_sub_ps( intensityb, intensityc );

#ifdef MSE
		diffhere = _mm256_mul_ps( diffhere, diffhere );
#else
		diffhere = _mm256_andnot_ps( signmask, diffhere );
#endif

		diffs = _mm256_add_ps( diffs, diffhere );
	}

	__m128 diffslo = _mm256_extractf128_ps( diffs, 0 );
	__m128 diffshi  = _mm256_extractf128_ps( diffs, 1 );
	__m128 diffssum  = _mm_hadd_ps( diffslo, diffshi );
	float OutputVal[4];
	_mm_store_ps(OutputVal, diffssum);
	diff = OutputVal[0] + OutputVal[1] + OutputVal[2] + OutputVal[3];

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

#ifndef HALFTONE_EN
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
	struct block* kmeanses_unaligned = calloc(1, sizeof(struct block) * KMEANS + 31);
	struct block* kmeanses = (struct block*) (((uintptr_t)(((uint8_t*)kmeanses_unaligned)+31))&(~31)); // force alignment

	// Computed average
	float* mkd_val = calloc(KMEANS * BLOCKSIZE * BLOCKSIZE, sizeof(float));
	float* mkd_cnt = calloc(KMEANS, sizeof(float));
	int* kmeansdead = calloc(KMEANS, sizeof(int));

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
		memset( mkd_val, 0, KMEANS * BLOCKSIZE * BLOCKSIZE * sizeof(float) );
		memset( mkd_cnt, 0, KMEANS * sizeof(float) );

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
				mkd_val[(BLOCKSIZE * BLOCKSIZE) * mink + i] += b->intensity[i] * b->count;
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
			float * valf = &mkd_val[km*BLOCKSIZE*BLOCKSIZE];
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
		const int draww = BLOCKSIZE * 6;
		const int drawh = BLOCKSIZE * 5;
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
		int thisframe = rand() % num_video_frames;
		for( y = 0; y < video_h/BLOCKSIZE; y++ )
		for( x = 0; x < video_w/BLOCKSIZE; x++ )
		{
			uint8_t * tbuf = &rawVideoData[thisframe*video_w*video_h];
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
	if( !fTiles )
	{
		fprintf( stderr, "Failed to open tile file for writing\n" );
		exit( -7 );
	}
	for( km = 0; km < KMEANS; km++ )
	{
		if( kmeansdead[km] ) continue;
		fwrite( &kmeanses[km].blockdata, sizeof( kmeanses[km].blockdata ), 1, fTiles );
	}
	fclose( fTiles );

	FILE * fStream = fopen( streamFile, "wb" );
	if( !fStream )
	{
		fprintf( stderr, "Failed to open stream file for writing\n" );
		exit( -7 );
	}

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
			uint32_t minkwrite = 0;
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
					mink = km;
					minkwrite = outkmid;
				}
				outkmid++;
			}

			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2, &b, false );
			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2 + 200, &kmeanses[mink], false );
			DrawBlock( x * BLOCKSIZE*2, y * BLOCKSIZE*2 + 400, &kmeanses[mink], true );
			//memcpy( &rawVideoData[(frames-1)*video_w*video_h], tbuf, video_w*video_h );
			
			fwrite( &minkwrite, sizeof( minkwrite ), 1, fStream );
		}
		CNFGSwapBuffers();
	}
	fclose( fStream );

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

	fseek(f, 0, SEEK_END);
	int frame_count = ftell(f) / (video_w * video_h);
	rawVideoData = malloc(frame_count * video_w * video_h);
	fseek(f, 0, SEEK_SET);

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
 
