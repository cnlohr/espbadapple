#include <stdio.h>
#include "bacommon.h"
#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"
#include "gifenc.c"

int streamcount;
uint32_t * streamdata;

int glyphct;
float * glyphfloat;
struct block * glyphdata;

int video_w;
int video_h;
int num_video_frames;

// 0x8000 is huffman key for indicating which entry to jump to.
#define FLAG_RLE 0x4000

ge_GIF * gifout;


int main( int argc, char ** argv )
{
	int frame = 0;
	int i;
	int block = 0;
	CNFGSetup( "comp test", 1800, 1060 );

	if( argc != 6 )
	{
		fprintf( stderr, "Usage: streamcomp [stream] [tiles] [video] [w] [h]\n" );
		return 0;
	}

	video_w = atoi( argv[4] );
	video_h = atoi( argv[5] );

	FILE * f = fopen( argv[1], "rb" );
	fseek( f, 0, SEEK_END );
	streamcount = ftell( f ) / sizeof( streamdata[0] );
	streamdata = malloc( streamcount * sizeof( streamdata[0] ) );
	fseek( f, 0, SEEK_SET );
	fread( streamdata, streamcount, sizeof( streamdata[0] ), f );
	fclose( f );

	f = fopen( argv[2], "rb" );
	fseek( f, 0, SEEK_END );
	glyphct = ftell( f ) / sizeof( glyphfloat[0] );
	glyphfloat = malloc( glyphct * sizeof( glyphfloat[0] ) );
	fseek( f, 0, SEEK_SET );
	fread( glyphfloat, glyphct, sizeof( glyphfloat[0] ) , f );
	fclose( f );

	glyphct = glyphct / (BLOCKSIZE*BLOCKSIZE);

	glyphdata = malloc( glyphct * sizeof(glyphdata[0]) );
	for( block = 0; block < glyphct; block++ )
	{
		struct block * bb = glyphdata + block;
		float * fd = glyphfloat + block * BLOCKSIZE * BLOCKSIZE;
		memcpy( bb->intensity, fd, sizeof( bb->intensity ) );
		UpdateBlockDataFromIntensity( bb );
	}


	num_video_frames = streamcount / ( video_w * video_h / ( BLOCKSIZE * BLOCKSIZE ) );

	printf( "Read %d glyphs, and %d elements (From %d frames)\n", glyphct, streamcount, num_video_frames );

	int vbh = video_h/BLOCKSIZE;
	int vbw = video_w/BLOCKSIZE;
	uint32_t blockmap[vbh*vbw];
	memset( blockmap, 0, vbh*vbw*sizeof(uint32_t) );

	int running = 0;

	int tid = 0;

#ifdef COMPRESSION_BLOCK_RASTER
	// For 64x48 (test1), huffman stream was 596979 bits @64x48
	//  Or 671880 if not skipping extra runs.

	int nrtokens = 0;
	uint32_t * token_stream = 0;

	// for 64x48, 820046 bits
	// This method rasterizes left-to-right and top-to-bottom.

	for( frame = 0; frame < num_video_frames; frame++ )
	{
		//CNFGClearFrame();
		//if( !CNFGHandleInput() ) break;

		int bx, by;
		for( by = 0; by < video_h/BLOCKSIZE; by++ )
		for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
		{
			uint32_t glyphid = streamdata[block++];

			if( glyphid != blockmap[bx+by*(video_w/BLOCKSIZE)] )
			{
				if( running )
				{
					token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					token_stream[nrtokens++] = FLAG_RLE | running;
				}

				token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
				token_stream[nrtokens++] = glyphid;

				blockmap[bx+by*(video_w/BLOCKSIZE)] = glyphid;
				running = 0;
			}
			else
			{
				running++;
			}

			//blocktype bt = &glyphdata[glyphid&GLYPH_INVERSION_MASK];
			//DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
		}

		//CNFGSwapBuffers();
	}

	printf( "Processed %d frames\n", frame );
	printf( "Number of stream TIDs: %d\n", nrtokens );

	// Compress the TIDs
	{
		uint32_t * unique_tokens = 0;
		uint32_t * token_counts = 0;
		int unique_tok_ct = 0;
		int i;
		for( i = 0; i < nrtokens; i++ )
		{
			uint32_t t = token_stream[i];
			int j;
			for( j = 0; j < unique_tok_ct; j++ )
			{
				if( unique_tokens[j] == t ) break;
			}
			if( j == unique_tok_ct )
			{
				unique_tokens = realloc( unique_tokens, ( unique_tok_ct + 1) * sizeof( uint32_t ) );
				token_counts = realloc( token_counts,  ( unique_tok_ct + 1) * sizeof( uint32_t ) );
				unique_tokens[unique_tok_ct] = t;
				token_counts[unique_tok_ct] = 1;
				unique_tok_ct++;
			}
			else
			{
				token_counts[j]++;
			}
		}

		int hufflen;
		huffup * hu;
		huffelement * e = GenerateHuffmanTree( unique_tokens, token_counts, unique_tok_ct, &hufflen );
		printf( "Huff len: %d\n", hufflen );

		int htlen;
		hu = GenPairTable( e, &htlen );


#if 0
		for( i = 0; i < htlen; i++ )
		{
			int j;
			printf( "%d - ", i );
			int len = hu[i].bitlen;
			printf( "%d\n", len );
			for( j = 0; j <  len ; j++ )
				printf( "%c", hu[i].bitstream[j] + '0' );
			printf( "\n" );
		}
#endif

		char * bitstreamo = 0;
		int bistreamlen = 0;
		memset( blockmap, 0, vbh*vbw*sizeof(uint32_t) );

		block = 0;
		running = 0;

		// Go through video again.
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			//CNFGClearFrame();
			//if( !CNFGHandleInput() ) break;

			int bx, by;
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[block++];

				if( glyphid != blockmap[bx+by*(video_w/BLOCKSIZE)] )
				{
					if( running )
					{
						uint32_t thistok = FLAG_RLE | running;

						int h;
						for( h = 0; h < htlen; h++ )
							if( hu[h].value == thistok ) break;
						int b;
						if( h == htlen )
						{
							fprintf( stderr, "Error: Can't find element %04x\n", thistok );
							return -9;
						}
						for( b = 0; b < hu[h].bitlen; b++ )
						{
							bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
							bitstreamo[bistreamlen++] = hu[h].bitstream[b];
						}
					}

					uint32_t thistok = glyphid;
					int h;
					for( h = 0; h < htlen; h++ )
						if( hu[h].value == thistok ) break;
					int b;
					for( b = 0; b < hu[h].bitlen; b++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = hu[h].bitstream[b];
					}


					blockmap[bx+by*(video_w/BLOCKSIZE)] = glyphid;
					running = 0;
				}
				else
				{
					running++;
				}

				//blocktype bt = &glyphdata[glyphid&GLYPH_INVERSION_MASK];
				//DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
			}

			//CNFGSwapBuffers();
		}


		printf( "Bitstream Length Bits: %d\n", bistreamlen );
		printf( "Huff Length: %d\n", hufflen * 32 );
	}




	CNFGClearFrame();

	uint32_t running_tok = 0;
	uint32_t tokplace = 0;
	memset( blockmap, 0, sizeof( blockmap ) );
	do
	{
		uint32_t tok = token_stream[tid++];

		int rle = 1;
		if( tok & FLAG_RLE )
		{
			rle = tok & (~FLAG_RLE);
		}
		else
		{
			blockmap[tokplace % (vbw*vbh)] = tok;
		}

		int j;
		for( j = 0; j < rle; j++ )
		{
			int blockidhere = ((tokplace++) % (vbw*vbh));
			if( blockidhere == 0 )
			{
				CNFGSwapBuffers();
				CNFGClearFrame();
				if( !CNFGHandleInput() ) break;
			}
			uint32_t glyphid = blockmap[blockidhere];
			struct block * b = &glyphdata[glyphid&GLYPH_INVERSION_MASK];
			int bx = blockidhere % vbw;
			int by = blockidhere / vbw;
			DrawBlock( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, b, true, glpyhid );
		}

		frame++;
	} while( tid < nrtokens );

#elif defined( COMPRESSION_TWO_HUFF )

	// this method does block-at-a-time, full video per block.
	// BUT, With SPLIT tables for glyph ID / huffman stream is 516224 bits @64x48
	// XXX DEAD END XXX
	// This is for TWO huffman trees.

	{
		int nrtokens = 0;
		uint32_t * token_stream = 0;

		int bx, by;
		uint32_t lastblock[vbh][vbw];
		uint32_t running[vbh][vbw];
		memset( lastblock, 0, sizeof( lastblock  )) ;
		memset( running, 0, sizeof(running) );

		// Go frame-at-a-time, but keep track of the last block and running, on a per-block basis.
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( running[by][bx] == 0 )
				{
					token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					token_stream[nrtokens++] = glyphid;

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					{
						forward--;
//printf( "+ %04x\n", FLAG_RLE | forward );
						token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						token_stream[nrtokens++] = forward;
					}
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		int tid = 0;
		printf( "Processed %d frames\n", frame );
		printf( "Number of stream TIDs: %d\n", nrtokens );

		// Compress the TIDs
		uint32_t * unique_tokens[2] = { 0 };
		uint32_t * token_counts[2] = { 0 };
		int unique_tok_ct[2] = { 0 };
		int i;
		for( i = 0; i < nrtokens; i+=2 )
		{
			int wt = 0;

			for( wt = 0; wt < 2; wt++ )
			{
				uint32_t t = token_stream[i+wt];
				int j;
				for( j = 0; j < unique_tok_ct[wt]; j++ )
				{
					if( unique_tokens[wt][j] == t ) break;
				}
				if( j == unique_tok_ct[wt] )
				{
					unique_tokens[wt] = realloc( unique_tokens[wt], ( unique_tok_ct[wt] + 1) * sizeof( uint32_t ) );
					token_counts[wt] = realloc( token_counts[wt],  ( unique_tok_ct[wt] + 1) * sizeof( uint32_t ) );
					unique_tokens[wt][unique_tok_ct[wt]] = t;
					token_counts[wt][unique_tok_ct[wt]] = 1;
					unique_tok_ct[wt]++;
				}
				else
				{
					token_counts[wt][j]++;
				}
			}
		}

		int hufflen[2];
		huffup * hu[2];
		huffelement * hufftree[2];
		int htlen[2];



		int wt;
		for( wt = 0; wt < 2; wt++ )
		{
#if 0
			for( i = 0; i < unique_tok_ct[wt]; i++ )
			{
				printf( "%d - %d - %d\n", i, unique_tokens[wt][i], token_counts[wt][i] );
			}
#endif

			hufftree[wt] = GenerateHuffmanTree( unique_tokens[wt], token_counts[wt], unique_tok_ct[wt], &hufflen[wt] );
			printf( "Huff len: %d\n", hufflen[wt] );
			hu[wt] = GenPairTable( hufftree[wt], &htlen[wt] );
#if 1
			for( i = 0; i < htlen[wt]; i++ )
			{
				int j;
				printf( "%4d - %4d - ", i, hu[wt][i].value );
				int len = hu[wt][i].bitlen;
				printf( "%4d - FREQ:%6d - ", len, hu[wt][i].freq );
				for( j = 0; j <  len ; j++ )
					printf( "%c", hu[wt][i].bitstream[j] + '0' );
				printf( "\n" );
			}
#endif
		}

		char * bitstreamo = 0;
		int bistreamlen = 0;

		block = 0;

		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );

		// Same as above loop, but we are actually producing compressed output.
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( running[by][bx] == 0 )
				{
					//token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					//token_stream[nrtokens++] = glyphid;

					// Instead do the compression.
					int h;
					for( h = 0; h < htlen[0]; h++ )
						if( hu[0][h].value == glyphid ) break;
					if( h == htlen[0] ) { fprintf( stderr, "Error: Missing symbol %d\n", glyphid ); exit( -6 ); }
					int b;
//						printf( "EMIT: %04x\n",  glyphid );

					for( b = 0; b < hu[0][h].bitlen; b++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = hu[0][h].bitstream[b];
					}

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					//if( forward > 1 )
					{
						// Actually want frames _in the future_
						forward--;
//						printf( "EMIT_F: %04x\n", FLAG_RLE | forward );

						//token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						//token_stream[nrtokens++] = FLAG_RLE | forward;
						for( h = 0; h < htlen[1]; h++ )
							if( hu[1][h].value == forward ) break;
						if( h == htlen[1] ) { fprintf( stderr, "Error: Missing symbol %d\n", FLAG_RLE | forward ); exit( -6 ); }
						for( b = 0; b < hu[1][h].bitlen; b++ )
						{
							bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
							bitstreamo[bistreamlen++] = hu[1][h].bitstream[b];
						}
					}
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		//CNFGSwapBuffers();
		printf( "Bitstream Length Bits: %d\n", bistreamlen );
		printf( "Huff Length: %d %d\n", hufflen[0] * 32, hufflen[1] * 32 );
		printf( "Glyph Length: %d\n", glyphct * BLOCKSIZE * BLOCKSIZE );
		printf( "Total: %d\n", bistreamlen + hufflen[0] * 32 + hufflen[1] * 32 +glyphct * BLOCKSIZE * BLOCKSIZE ); 


		int bitstream_place = 0;
		//char * bitstreamo = 0;
		//int bistreamlen = 0;
		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );
		int32_t next_tok = -1;

		int k;
	//	for( k = 0; k < 200; k++ )
	//	{
	//		printf( "%d", bitstreamo[k] );
	//	}
	//	printf( "\n" );

		while( bitstream_place < bistreamlen )
		{
			CNFGClearFrame();
			if( !CNFGHandleInput() ) break;
			for( by = 0; by < vbh; by++ )
			for( bx = 0; bx < vbw; bx++ )
			{
				if( running[by][bx] == 0 )
				{
					// Pull a token from "here"
					huffelement * e = hufftree[0];
					while( !e->is_term )
					{
						char c = bitstreamo[bitstream_place++];
						if( c == 0 ) e = &hufftree[0][e->pair0];
						else if( c == 1 ) e = &hufftree[0][e->pair1];
						else fprintf( stderr, "Error: Invalid symbol %d\n", c );
					}
					uint32_t tok = e->value;
					lastblock[by][bx] = tok;

					// Peek ahead.
					e = hufftree[1];
					while( !e->is_term )
					{
						char c = bitstreamo[bitstream_place++];
						if( c == 0 ) e = &hufftree[1][e->pair0];
						else if( c == 1 ) e = &hufftree[1][e->pair1];
						else fprintf( stderr, "Error: Invalid symbol %d\n", c );
					}
					uint32_t rle = e->value;
					// It WAS an RLE.
					running[by][bx] = rle;
				}
				else
				{
					running[by][bx]--;
				}

				uint32_t glyphid = lastblock[by][bx];
				struct block * b = &glyphdata[glyphid&GLYPH_INVERSION_MASK];
				DrawBlock( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, b, true, glpyhid );
			}

			CNFGSwapBuffers();
			frame++;
		}
	}

#elif defined( COMPRESSION_UNIFIED_BY_BLOCK_AND_TWO_HUFF )

	int nrtokens[2] = { 0 };
	uint32_t * token_stream[2] = { 0 };


	// this method does block-at-a-time, full video per block.
	// apples-to-apples, huffman stream is 397697 bits @64x48. (Test 1)
	// With forcing length every frame, 621525 bits @64x48. (i.e. no //if( forward > 1 ))
	// XXX TODO: Test but without the "don't include 0 length runs"

	{
		int bx, by;
		uint32_t lastblock[vbh][vbw];
		uint32_t running[vbh][vbw];
		memset( lastblock, 0, sizeof( lastblock  )) ;
		memset( running, 0, sizeof(running) );

		// Go frame-at-a-time, but keep track of the last block and running, on a per-block basis.
		int which_tree = 0;
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( running[by][bx] == 0 )
				{
					token_stream[which_tree] = realloc( token_stream[which_tree], (nrtokens[which_tree]+1) * sizeof( token_stream[which_tree] ) );
					token_stream[which_tree][nrtokens[which_tree]++] = glyphid;
//					printf( "EMIT (%d %d) %d -> %d\n", bx, by, which_tree, glyphid );

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					if( forward > 1 )
					{
						forward--;
						token_stream[1] = realloc( token_stream[1], (nrtokens[1]+1) * sizeof( token_stream[1] ) );
						token_stream[1][nrtokens[1]++] = FLAG_RLE | forward;
//						printf( "EMIT (%d %d) 1 -> %d\n", bx, by, FLAG_RLE | forward );
						which_tree = 0;
					}
					else
					{
						which_tree = 1;
					}
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		int tid = 0;
		printf( "Processed %d frames\n", frame );
		printf( "Number of stream TIDs: %d\n", nrtokens[0] );
		printf( "Number of stream TIDs: %d\n", nrtokens[1] );

		// Compress the TIDs
		uint32_t * unique_tokens[2] = { 0 };
		uint32_t * token_counts[2] = { 0 };
		int unique_tok_ct[2] = { 0 };
		int i;
		int toktype;
		for( toktype = 0; toktype < 2; toktype++ )
		for( i = 0; i < nrtokens[toktype]; i++ )
		{
			uint32_t t = token_stream[toktype][i];
			int j;
			for( j = 0; j < unique_tok_ct[toktype]; j++ )
			{
				if( unique_tokens[toktype][j] == t ) break;
			}
			if( j == unique_tok_ct[toktype] )
			{
				unique_tokens[toktype] = realloc( unique_tokens[toktype], ( unique_tok_ct[toktype] + 1) * sizeof( uint32_t ) );
				token_counts[toktype] = realloc( token_counts[toktype],  ( unique_tok_ct[toktype] + 1) * sizeof( uint32_t ) );
				unique_tokens[toktype][unique_tok_ct[toktype]] = t;
				token_counts[toktype][unique_tok_ct[toktype]] = 1;
				unique_tok_ct[toktype]++;
			}
			else
			{
				token_counts[toktype][j]++;
			}
		}

		int hufflen[2] = { 0 };
		huffup * hu[2] = { 0 };
		huffelement * hufftree[2] = { 0 };
		int htlen[2] = { 0 };

		for( toktype = 0; toktype < 2; toktype++ )
		{
			hufftree[toktype] = GenerateHuffmanTree( unique_tokens[toktype], token_counts[toktype], unique_tok_ct[toktype], &hufflen[toktype] );
			printf( "Huff len: %d\n", hufflen[toktype] );
			hu[toktype] = GenPairTable( hufftree[toktype], &htlen[toktype] );

#if 1
			for( i = 0; i < htlen[toktype]; i++ )
			{
				int j;
				printf( "%4d - %5d - ", i, hu[toktype][i].value );
				int len = hu[toktype][i].bitlen;
				printf( "%4d - FREQ:%6d - ", len, hu[toktype][i].freq );
				for( j = 0; j <  len ; j++ )
					printf( "%c", hu[toktype][i].bitstream[j] + '0' );
				printf( "\n" );
			}
#endif


		}

/*
printf( "HTLEN: %d\n", htlen );
int tmp;
for( tmp = 0; tmp < htlen; tmp++ )
{
	printf( "%04x : %d\n", hu[tmp].value, hu[tmp].bitlen );
}
*/
#if 0
	for( i = 0; i < htlen; i++ )
	{
		int j;
		printf( "%d - ", i );
		int len = hu[i].bitlen;
		printf( "%d\n", len );
		for( j = 0; j <  len ; j++ )
			printf( "%c", hu[i].bitstream[j] + '0' );
		printf( "\n" );
	}
#endif

		char * bitstreamo = 0;
		int bistreamlen = 0;

		block = 0;

		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );

		int use_which_table = 0;

		// Same as above loop, but we are actually producing compressed output.
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( running[by][bx] == 0 )
				{
					//token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					//token_stream[nrtokens++] = glyphid;

					// Instead do the compression.
					int h;
					for( h = 0; h < htlen[use_which_table]; h++ )
						if( hu[use_which_table][h].value == glyphid ) break;
					if( h == htlen[use_which_table] ) { fprintf( stderr, "Error: Missing symbol %d\n", glyphid ); exit( -6 ); }
					int b;
//						printf( "EMIT: %04x\n",  glyphid );

					for( b = 0; b < hu[use_which_table][h].bitlen; b++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = hu[use_which_table][h].bitstream[b];
					}

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					if( forward > 1 )
					{
						// Actually want frames _in the future_
						forward--;
//						printf( "EMIT_F: %04x\n", FLAG_RLE | forward );

						//token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						//token_stream[nrtokens++] = FLAG_RLE | forward;
						for( h = 0; h < htlen[1]; h++ )
							if( hu[1][h].value == (FLAG_RLE | forward) ) break;
						if( h == htlen[1] ) { fprintf( stderr, "Error: Missing symbol %d\n", FLAG_RLE | forward ); exit( -6 ); }
						for( b = 0; b < hu[1][h].bitlen; b++ )
						{
							bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
							bitstreamo[bistreamlen++] = hu[1][h].bitstream[b];
						}
						use_which_table = 0;
					}
					else
					{
						use_which_table = 1;
					}
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		//CNFGSwapBuffers();
		printf( "Bitstream Length Bits: %d\n", bistreamlen );
		printf( "Huff Length: %d %d\n", hufflen[0] * 32, hufflen[1] * 32 );
		printf( "Glyph Length: %d\n", glyphct * BLOCKSIZE * BLOCKSIZE );
		printf( "Total: %d\n", bistreamlen + hufflen[0] * 32 + hufflen[1] * 32 +glyphct * BLOCKSIZE * BLOCKSIZE ); 


		int bitstream_place = 0;
		//char * bitstreamo = 0;
		//int bistreamlen = 0;
		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );
		int32_t next_tok = -1;


		uint8_t palette[6] = { 0, 0, 0, 255, 255, 255 };
		gifout = ge_new_gif( argv[3], video_w*2, video_h*2, palette, 2, 0 );

		{
			FILE * f = fopen( "bitstream_out.dat", "wb" );
			int jlen = (bistreamlen + 7) / 8;
			int i;
			uint8_t * payload = calloc( jlen, 1 ); 
			for( i = 0; i < bistreamlen; i++ )
			{
				payload[i/8] |= bitstreamo[i]<<(i&7);
			}
			fwrite( payload, jlen, 1, f );
			fclose( f );
		}


		int32_t next_rle = -1;
		while( bitstream_place < bistreamlen )
		{
			CNFGClearFrame();
			if( !CNFGHandleInput() ) break;
			for( by = 0; by < vbh; by++ )
			for( bx = 0; bx < vbw; bx++ )
			{
				if( running[by][bx] == 0 )
				{
					// Pull a token from "here"
					uint32_t tok;
					huffelement * e;
					if( next_rle >= 0 )
					{
						tok = next_rle;
					}
					else
					{
						e = hufftree[0];
						while( !e->is_term )
						{
							char c = bitstreamo[bitstream_place++];
							if( c == 0 ) e = &hufftree[0][e->pair0];
							else if( c == 1 ) e = &hufftree[0][e->pair1];
							else { fprintf( stderr, "Error: Invalid symbol A %d\n", c ); exit( -5 ); }
						}
						tok = e->value;
					}

					lastblock[by][bx] = tok;

					// Peek ahead.
					e = hufftree[1];
					while( !e->is_term )
					{
						char c = bitstreamo[bitstream_place++];
						if( c == 0 ) e = &hufftree[1][e->pair0];
						else if( c == 1 ) e = &hufftree[1][e->pair1];
						else { fprintf( stderr, "Error: Invalid symbol B %d\n", c ); exit( -5 ); }
					}
					next_rle = e->value;

					if( next_rle & FLAG_RLE )
					{
						// It WAS an RLE.
						running[by][bx] = next_rle & (~FLAG_RLE);
						next_rle = -1;
					}
					else
					{
						running[by][bx] = 1;
					}
				}
				else
				{
					running[by][bx]--;
				}

				uint32_t glyphid = lastblock[by][bx];
				struct block * b = &glyphdata[glyphid&GLYPH_INVERSION_MASK];
				DrawBlock( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, b, true, glpyhid );

				BlockUpdateGif( gifout, bx * BLOCKSIZE*2, by * BLOCKSIZE*2, video_w*2, bt, glpyhid );
			}

			ge_add_frame(gifout, 2);

			CNFGSwapBuffers();
			frame++;
		}
		ge_close_gif( gifout );
	}

#elif defined( COMPRESSION_UNIFIED_BY_BLOCK )

	int nrtokens = 0;
	uint32_t * token_stream = 0;

	// this method does block-at-a-time, full video per block.
	// apples-to-apples, huffman stream is 397697 bits @64x48. (Test 1)
	// With forcing length every frame, 621525 bits @64x48. (i.e. no //if( forward > 1 ))
	// XXX TODO: Test but without the "don't include 0 length runs"

	{
		int bx, by;
		uint32_t lastblock[vbh][vbw];
		uint32_t running[vbh][vbw];
		memset( lastblock, 0, sizeof( lastblock  )) ;
		memset( running, 0, sizeof(running) );

		// Go frame-at-a-time, but keep track of the last block and running, on a per-block basis.
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( running[by][bx] == 0 )
				{
					token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					token_stream[nrtokens++] = glyphid;

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					if( forward > 1 )
					{
						forward--;
						token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						token_stream[nrtokens++] = FLAG_RLE | forward;
					}
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		int tid = 0;
		printf( "Processed %d frames\n", frame );
		printf( "Number of stream TIDs: %d\n", nrtokens );

		// Compress the TIDs
		uint32_t * unique_tokens = 0;
		uint32_t * token_counts = 0;
		int unique_tok_ct = 0;
		int i;
		for( i = 0; i < nrtokens; i++ )
		{
			uint32_t t = token_stream[i];
			int j;
			for( j = 0; j < unique_tok_ct; j++ )
			{
				if( unique_tokens[j] == t ) break;
			}
			if( j == unique_tok_ct )
			{
				unique_tokens = realloc( unique_tokens, ( unique_tok_ct + 1) * sizeof( uint32_t ) );
				token_counts = realloc( token_counts,  ( unique_tok_ct + 1) * sizeof( uint32_t ) );
				unique_tokens[unique_tok_ct] = t;
				token_counts[unique_tok_ct] = 1;
				unique_tok_ct++;
			}
			else
			{
				token_counts[j]++;
			}
		}

		int hufflen;
		huffup * hu;
		huffelement * hufftree = GenerateHuffmanTree( unique_tokens, token_counts, unique_tok_ct, &hufflen );
		printf( "Huff len: %d\n", hufflen );

		int htlen;
		hu = GenPairTable( hufftree, &htlen );

#if 1
			for( i = 0; i < htlen; i++ )
			{
				int j;
				printf( "%4d - %5d - ", i, hu[i].value );
				int len = hu[i].bitlen;
				printf( "%4d - FREQ:%6d - ", len, hu[i].freq );
				for( j = 0; j <  len ; j++ )
					printf( "%c", hu[i].bitstream[j] + '0' );
				printf( "\n" );
			}
#endif

/*
printf( "HTLEN: %d\n", htlen );
int tmp;
for( tmp = 0; tmp < htlen; tmp++ )
{
	printf( "%04x : %d\n", hu[tmp].value, hu[tmp].bitlen );
}
*/
#if 0
	for( i = 0; i < htlen; i++ )
	{
		int j;
		printf( "%d - ", i );
		int len = hu[i].bitlen;
		printf( "%d\n", len );
		for( j = 0; j <  len ; j++ )
			printf( "%c", hu[i].bitstream[j] + '0' );
		printf( "\n" );
	}
#endif

		char * bitstreamo = 0;
		int bistreamlen = 0;

		block = 0;

		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );

		// Same as above loop, but we are actually producing compressed output.
		for( frame = 0; frame < num_video_frames; frame++ )
		{
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( running[by][bx] == 0 )
				{
					//token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					//token_stream[nrtokens++] = glyphid;

					// Instead do the compression.
					int h;
					for( h = 0; h < htlen; h++ )
						if( hu[h].value == glyphid ) break;
					if( h == htlen ) { fprintf( stderr, "Error: Missing symbol %d\n", glyphid ); exit( -6 ); }
					int b;
//						printf( "EMIT: %04x\n",  glyphid );

					for( b = 0; b < hu[h].bitlen; b++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = hu[h].bitstream[b];
					}

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					if( forward > 1 )
					{
						// Actually want frames _in the future_
						forward--;
//						printf( "EMIT_F: %04x\n", FLAG_RLE | forward );

						//token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						//token_stream[nrtokens++] = FLAG_RLE | forward;

						if( forward >= FLAG_RLE )
						{
							fprintf( stderr, "Error: Run too long\n" );
						}
						for( h = 0; h < htlen; h++ )
							if( hu[h].value == (FLAG_RLE | forward) ) break;
						if( h == htlen ) { fprintf( stderr, "Error: Missing symbol %d\n", FLAG_RLE | forward ); exit( -6 ); }
						for( b = 0; b < hu[h].bitlen; b++ )
						{
							bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
							bitstreamo[bistreamlen++] = hu[h].bitstream[b];
						}
					}
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		//CNFGSwapBuffers();
		printf( "Bitstream Length Bits: %d\n", bistreamlen );
		printf( "Huff Length: %d\n", hufflen * 32 );
		printf( "Glyph Length: %d\n", glyphct * BLOCKSIZE * BLOCKSIZE );
		int total_bits = bistreamlen + hufflen * 32 +glyphct * BLOCKSIZE * BLOCKSIZE;
		printf( "Total: %d\n", bistreamlen + hufflen * 32 +glyphct * BLOCKSIZE * BLOCKSIZE ); 
		printf( "Bytes: %d\n", (total_bits+7)/8 );
		printf( "Bits Per Frame: %.1f\n", (float)total_bits/(float)num_video_frames );

		int bitstream_place = 0;
		//char * bitstreamo = 0;
		//int bistreamlen = 0;
		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );
		int32_t next_tok = -1;


		uint8_t palette[48] = { 0, 0, 0, 255, 255, 255 };
		gifout = ge_new_gif( argv[3], video_w*2, video_h*2, palette, 4, 0 );

		{
			FILE * f = fopen( "bitstream_out.dat", "wb" );
			int jlen = (bistreamlen + 7) / 8;
			int i;
			uint8_t * payload = calloc( jlen, 1 ); 
			for( i = 0; i < bistreamlen; i++ )
			{
				payload[i/8] |= bitstreamo[i]<<(i&7);
			}
			fwrite( payload, jlen, 1, f );
			fclose( f );
		}


		while( bitstream_place < bistreamlen )
		{
			CNFGClearFrame();
			if( !CNFGHandleInput() ) break;
			for( by = 0; by < vbh; by++ )
			for( bx = 0; bx < vbw; bx++ )
			{
				if( running[by][bx] == 0 )
				{
					// Pull a token from "here"
					huffelement * e = hufftree;
					while( !e->is_term )
					{
						char c = bitstreamo[bitstream_place++];
						if( c == 0 ) e = &hufftree[e->pair0];
						else if( c == 1 ) e = &hufftree[e->pair1];
						else fprintf( stderr, "Error: Invalid symbol %d\n", c );
					}
					uint32_t tok = e->value;
					lastblock[by][bx] = tok;

					// Peek ahead.
					int place_backup = bitstream_place;
					e = hufftree;
					while( !e->is_term )
					{
						char c = bitstreamo[place_backup++];
						if( c == 0 ) e = &hufftree[e->pair0];
						else if( c == 1 ) e = &hufftree[e->pair1];
						else fprintf( stderr, "Error: Invalid symbol %d\n", c );
					}
					uint32_t possible_rle = e->value;

					if( possible_rle & FLAG_RLE )
					{
						// It WAS an RLE.
						bitstream_place = place_backup;
						running[by][bx] = possible_rle & (~FLAG_RLE);
					}
					else
					{
						running[by][bx] = 1;
					}
				}
				else
				{
					running[by][bx]--;
				}

				uint32_t glyphid = lastblock[by][bx];
				struct block * b = &glyphdata[glyphid&GLYPH_INVERSION_MASK];
				DrawBlock( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, b, true, glyphid );
				BlockUpdateGif( gifout, bx * BLOCKSIZE*2, by * BLOCKSIZE*2, video_w*2, b->blockdata, glyphid );
			}

			ge_add_frame(gifout, 2);


			CNFGSwapBuffers();
			frame++;
		}
		ge_close_gif( gifout );
	}
#else
	#error need a compression scheme defined
#endif

}


