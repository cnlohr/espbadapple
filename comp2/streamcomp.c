#include <stdio.h>
#include "bacommon.h"
#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"
#include "gifenc.c"


#define VPXCODING_WRITER
#define VPXCODING_READER
#include "../vpxtest/vpxcoding.h"

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

	int * raw_block_frequencies = 0;
	int num_raw_block_frequencies = 0;
	for( i = 0; i < streamcount; i++ )
	{
		if( streamdata[i] >= num_raw_block_frequencies )
		{
			int osize = num_raw_block_frequencies;
			num_raw_block_frequencies = streamdata[i] + 1;
			raw_block_frequencies = realloc( raw_block_frequencies, num_raw_block_frequencies * sizeof( raw_block_frequencies[0] ) );
			int j;
			for( j = osize; j != num_raw_block_frequencies; j++ )
				raw_block_frequencies[j] = 0;
		}
		raw_block_frequencies[streamdata[i]]++;
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

			//blocktype bt = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];
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

				//blocktype bt = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];
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
			struct block * b = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];
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
#if 0
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
				struct block * b = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];
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
		int total_bits = bistreamlen + hufflen[0] * 32 + hufflen[1] * 32 +glyphct * BLOCKSIZE * BLOCKSIZE;
		printf( "Bytes: %d\n", (total_bits+7)/8 );
		printf( "Bits Per Frame: %.1f\n", (float)total_bits/(float)num_video_frames );


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
				struct block * b = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];
				DrawBlock( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, b, true, glyphid );

				BlockUpdateGif( gifout, bx * BLOCKSIZE*2, by * BLOCKSIZE*2, video_w*2, b, glyphid );
			}

			ge_add_frame(gifout, 2);

			CNFGSwapBuffers();
			frame++;
		}
		ge_close_gif( gifout );
	}
#elif defined( COMPRESSION_TWO_HUFF_TRY_2 )

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
					token_stream[0] = realloc( token_stream[0], (nrtokens[which_tree]+1) * sizeof( token_stream[0] ) );
					token_stream[0][nrtokens[0]++] = glyphid;

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
					token_stream[1] = realloc( token_stream[1], (nrtokens[1]+1) * sizeof( token_stream[1] ) );
					token_stream[1][nrtokens[1]++] = forward-1;

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
#ifdef USE_VPX_LEN
		for( toktype = 0; toktype < 1; toktype++ )
#else
		for( toktype = 0; toktype < 2; toktype++ )
#endif
		{
			hufftree[toktype] = GenerateHuffmanTree( unique_tokens[toktype], token_counts[toktype], unique_tok_ct[toktype], &hufflen[toktype] );
			printf( "Huff len: %d\n", hufflen[toktype] );
			hu[toktype] = GenPairTable( hufftree[toktype], &htlen[toktype] );

			char soname[1024];
			snprintf( soname, sizeof(soname), "huffman_%d.dat", toktype );
			FILE * f = fopen( soname, "wb" );

			int maxa = 0, maxb = 0;
			int maxtok = 0;
			for( i = 0; i < hufflen[toktype]; i++ )
			{
				uint32_t symout;
				if( hufftree[toktype][i].is_term )
				{
					if( hufftree[toktype][i].value > maxtok )
						maxtok = hufftree[toktype][i].value;
					symout = (hufftree[toktype][i].value) | 0x800000;
				}
				else
				{
					int dpa = hufftree[toktype][i].pair0 - i;
					int dpb = hufftree[toktype][i].pair1 - i;
					if( dpa < 0 || dpb < 0 ) fprintf( stderr, "Error: can't use differential coding to encode this (%d / %d / %d)\n", 
						hufftree[toktype][i].pair0, hufftree[toktype][i].pair1, i );
					if( dpa > maxa )
						maxa = dpa;
					if( dpb > maxb )
						maxb = dpb;
					symout = dpa | (dpb<<12);
				}
				fwrite( &symout, 1, 3, f );
			}
			fclose( f );
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

			printf( "Tree Extents: %d -> %x / %x / %x\n", toktype, maxtok, maxa, maxb );

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
		int bistreamlentiles = 0;
		int bistreamlenruns = 0;

		block = 0;

		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );



#ifdef USE_VPX_LEN
		int vpx_probs_by_tile[glyphct];
		hufflen[1] = 0; // Null out RLE huffman table.
		{
			int probcountmap[glyphct];
			int glyphcounts[glyphct];
			int curtile[video_h/BLOCKSIZE][video_w/BLOCKSIZE];
			int curtileruns[video_h/BLOCKSIZE][video_w/BLOCKSIZE];
			int n;

			for( n = 0; n < glyphct; n++ )
			{
				probcountmap[n] = 0;
				glyphcounts[n] = 0;
			}

			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				curtile[by][bx] = -1;
				curtileruns[by][bx] = 0;
			}

			// Go frame-at-a-time, but keep track of the last block and running, on a per-block basis.
			for( frame = 0; frame < num_video_frames; frame++ )
			{
				for( by = 0; by < video_h/BLOCKSIZE; by++ )
				for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
				{
					uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

					if( curtile[by][bx] != glyphid )
					{
						if( curtileruns[by][bx] > 0 )
						{
							glyphcounts[curtile[by][bx]]++;
							probcountmap[curtile[by][bx]]+=curtileruns[by][bx];
						}
						curtileruns[by][bx] = 0;
						curtile[by][bx] = glyphid;
					}
					curtileruns[by][bx]++;
				}
			}

			for( n = 0; n < glyphct; n++ )
			{
				int prob = (glyphcounts[n] * 257.0 / probcountmap[n]) - 0.5;
				if( prob < 0 ) prob = 0; 
				if( prob > 255 ) prob = 255;
				vpx_probs_by_tile[n] = prob;
			}
		}

		int vpx_encoded_len = 0;


		vpx_writer vpx_writers[video_h/BLOCKSIZE][video_w/BLOCKSIZE];
		char * bufferVPX[video_h/BLOCKSIZE][video_w/BLOCKSIZE];
		for( by = 0; by < video_h/BLOCKSIZE; by++ )
		for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
		{
			bufferVPX[by][bx] = malloc( 1024*1024 );
			vpx_start_encode( &vpx_writers[by][bx], bufferVPX[by][bx], 1024*1024);
		}
#endif


		FILE * fSymbolList = fopen( "symbol_list.txt", "w" );

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

					huffelement * tr = hufftree[0];//] = { 0 };
					int hrplace = 0;

					for( b = 0; b < hu[0][h].bitlen; b++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						int bit = hu[0][h].bitstream[b];

						hrplace = bit?tr[hrplace].pair1 : tr[hrplace].pair0;

						bitstreamo[bistreamlen++] = bit;
						bistreamlentiles++;
					}

					fprintf( fSymbolList, "%d, ", glyphid );

					int forward;
					for( forward = 1; frame + forward < num_video_frames; forward++ )
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
			
#ifdef USE_UEGB
					// Buggy doesn't quite work right (But also doesn't offer savings)
					int fwd_to_encode = forward - 1;
					if( fwd_to_encode == 0 )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = 1;
					}
					fwd_to_encode++;
					int numbits = H264FUNDeBruijnLog2( fwd_to_encode );
					for( h = 0; h < numbits-1; h++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = 0;
					}
					for( h = 0; h < numbits; h++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = ((fwd_to_encode)>>(numbits-h-1)) & 1;
					}
#elif USE_VPX_LEN
					int n;
					for( n = 0; n < forward-1; n++ )
						vpx_write( &vpx_writers[by][bx], 1, vpx_probs_by_tile[glyphid] );
					vpx_write( &vpx_writers[by][bx], 0, vpx_probs_by_tile[glyphid] );
#else

					for( h = 0; h < htlen[1]; h++ )
						if( hu[1][h].value == (forward-1) ) break;
					if( h == htlen[1] ) { fprintf( stderr, "Error: Missing symbol %d\n", forward ); exit( -6 ); }
					for( b = 0; b < hu[1][h].bitlen; b++ )
					{
						bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
						bitstreamo[bistreamlen++] = hu[1][h].bitstream[b];
						bistreamlenruns++;
					}
					fprintf( fSymbolList, "%d\n", (forward-1) );

#endif
					running[by][bx] = forward;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

#if USE_VPX_LEN
		for( by = 0; by < video_h/BLOCKSIZE; by++ )
		for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
		{
			vpx_stop_encode(&vpx_writers[by][bx]);
			vpx_encoded_len += vpx_writers[by][bx].pos;
		}
		// Total 
#endif



		//CNFGSwapBuffers();
		printf( "Bitstream Length Bits: %d (%d + %d)\n", bistreamlen, bistreamlentiles, bistreamlenruns );
		printf( "Huff Length: %d %d\n", hufflen[0] * 24, hufflen[1] * 24 );
		int vpxaddbits = 0;
#ifdef USE_VPX_LEN
		printf( "VPX Probability Len (bits): %d\n", glyphct * 8);
		printf( "VPX Length (bits) %d\n", vpx_encoded_len * 8 );
		vpxaddbits = vpx_encoded_len * 8 + glyphct;
#endif
		printf( "Glyph Length: %d\n", glyphct * BLOCKSIZE * BLOCKSIZE );
		printf( "Total: %d\n", bistreamlen + hufflen[0] * 24 + hufflen[1] * 24 +glyphct * BLOCKSIZE * BLOCKSIZE + vpxaddbits ); 
		int total_bits = bistreamlen + hufflen[0] * 24 + hufflen[1] * 24 +glyphct * BLOCKSIZE * BLOCKSIZE + vpxaddbits;
		printf( "Bytes: %d\n", (total_bits+7)/8 );
		printf( "Bits Per Frame: %.1f\n", (float)total_bits/(float)num_video_frames );


		int bitstream_place = 0;
		//char * bitstreamo = 0;
		//int bistreamlen = 0;
		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );
		int32_t next_tok = -1;


		uint8_t palette[12] = { 0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0 };
		gifout = ge_new_gif( argv[3], video_w*2, video_h*2, palette, 2, 2, 1 );

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



#ifdef USE_VPX_LEN
		vpx_reader reader[vbh][vbw];
		for( by = 0; by < vbh; by++ )
		for( bx = 0; bx < vbw; bx++ )
		{
			vpx_reader_init(&reader[by][bx], bufferVPX[by][bx], vpx_writers[by][bx].pos, 0, 0 );
		}
#endif

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

					e = hufftree[0];
					while( !e->is_term )
					{
						char c = bitstreamo[bitstream_place++];
						if( c == 0 ) e = &hufftree[0][e->pair0];
						else if( c == 1 ) e = &hufftree[0][e->pair1];
						else { fprintf( stderr, "Error: Invalid symbol A %d\n", c ); exit( -5 ); }
					}
					tok = e->value;

					lastblock[by][bx] = tok;

#ifdef USE_UEGB
					//XXX Buggy doesn't quite work right.
					int zeroes = 0;
					for( zeroes = 0; bitstreamo[bitstream_place++] == 0; zeroes++ );
					if( zeroes == 0 )
					{
						next_rle = 0 + 1;
					}
					else
					{
						int k;
						int v = 1<<zeroes;
						for( k = 0; k < zeroes-1; k++ )
						{
							v = v | (bitstreamo[bitstream_place++]<<(zeroes - k));
						}
						next_rle = v - 1 + 1;
					}
#elif USE_VPX_LEN
					int ones = 0;
					while( vpx_read(&reader[by][bx], vpx_probs_by_tile[lastblock[by][bx]]) )
						ones++;
					next_rle = ones+1;

#else
					e = hufftree[1];
					while( !e->is_term )
					{
						char c = bitstreamo[bitstream_place++];
						if( c == 0 ) e = &hufftree[1][e->pair0];
						else if( c == 1 ) e = &hufftree[1][e->pair1];
						else { fprintf( stderr, "Error: Invalid symbol B %d\n", c ); exit( -5 ); }
					}

					next_rle = e->value+1;
#endif
					running[by][bx] = next_rle;
				}
				else
				{
					running[by][bx]--;
				}

				uint32_t glyphid = lastblock[by][bx];
				struct block * b = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];

				DrawBlock( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, b, true, glyphid );

				BlockUpdateGif( gifout, bx * BLOCKSIZE*2, by * BLOCKSIZE*2, video_w*2, b->blockdata, glyphid );
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

	printf( "Block Transition Count: %d\n", num_raw_block_frequencies );

	// For generating visualization of block transitions.
	int * block_transition_counts = malloc( sizeof(int) * num_raw_block_frequencies * num_raw_block_frequencies );
	int raw_block_counts[num_raw_block_frequencies];

	memset( raw_block_counts, 0, sizeof( raw_block_counts ) );

	memset( block_transition_counts, 0, sizeof( sizeof(int) * num_raw_block_frequencies * num_raw_block_frequencies ) );

	{
#ifdef ENCODE_HISTORY
		int encode_history[ENCODE_HISTORY] = { 0 };
		int encode_history_head = 0;
		int total_hist = 0;
#endif

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
					int emit_glyphid = glyphid;
#ifdef ENCODE_HISTORY
					int ihist = (encode_history_head - 1 + ENCODE_HISTORY) % ENCODE_HISTORY;
					int ihistend = encode_history_head;
					int ihistr = 0;
					for( ; ihist != ihistend; ihist = ( ihist - 1 + ENCODE_HISTORY ) % ENCODE_HISTORY )
					{
						// TODO: Can we ignore the most common emitted blocks?
//printf( "Check %d[%d] ==? %d   // %d %d\n", encode_history[ihist], ihist, emit_glyphid, ihist, ihistend ); 
						if( encode_history[ihist] == emit_glyphid ) break;
						ihistr++;
						if( ihistr >= total_hist ) break;
					}
					if( ihist != ihistend && encode_history[ihist] == emit_glyphid && raw_block_frequencies[emit_glyphid] < ENCODE_HISTORY_MIN_FOR_SEL )
					{
						printf( "HITA %04x (%d %d)\n",  emit_glyphid, emit_glyphid, raw_block_frequencies[emit_glyphid] );
						emit_glyphid = ihistr | ENCODE_HISTORY_FLAG;
					}
					else
					{
						encode_history[encode_history_head] = emit_glyphid;
						encode_history_head = ( encode_history_head + 1 ) % ENCODE_HISTORY;
						total_hist++;
					}
#endif


#ifdef HUFFMAN_ALLOW_CODE_FOR_INVERT
					int last = lastblock[by][bx];
					if( (last ^ emit_glyphid) == GLYPH_INVERSION_MASK )
						token_stream[nrtokens++] = GLYPH_NOATTRIB_MASK;
					else
						token_stream[nrtokens++] = emit_glyphid;
#else
					token_stream[nrtokens++] = emit_glyphid;
#endif

					int forward;
					for( forward = 0; frame + forward < num_video_frames; forward++ )
					{
						if( streamdata[(bx+by*(vbw)) + vbw*vbh * (frame + forward)] != glyphid )
							break;
					}
			
					if( forward > 1 )
					{
						forward--;
						token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						token_stream[nrtokens++] = FLAG_RLE | forward;
					}
					running[by][bx] = forward;
					block_transition_counts[lastblock[by][bx]*num_raw_block_frequencies + glyphid]++;
					raw_block_counts[glyphid]++;
					lastblock[by][bx] = glyphid;
				}
				else
				{
					running[by][bx]--;
				}
			}
		}

		{
			FILE * fTransitions = fopen( "stats_glyph_transitions.csv", "w" );
			int f, t;
			for( f = 0; f < num_raw_block_frequencies; f++ )
			{
				fprintf( fTransitions, "%d,", raw_block_counts[f] );
			}
			fprintf( fTransitions, "\n" );
			for( t = 0; t < num_raw_block_frequencies; t++ )
			{
				for( f = 0; f < num_raw_block_frequencies; f++ )
				{
					fprintf( fTransitions, "%d,", block_transition_counts[f*num_raw_block_frequencies + t] );
				}
				fprintf( fTransitions, "\n" );
			}
			fclose( fTransitions );
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
				printf( "%4d - %4x - ", i, hu[i].value );
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

		for( i = 0; i < nrtokens; i++ )
		{
			uint32_t t = token_stream[i];
			int h;
			for( h = 0; h < htlen; h++ )
				if( hu[h].value == t ) break;
			if( h == htlen ) { fprintf( stderr, "Error: Missing symbol %d\n", t ); exit( -6 ); }
			int b;
			for( b = 0; b < hu[h].bitlen; b++ )
			{
				bitstreamo = realloc( bitstreamo, (bistreamlen+1) );
				bitstreamo[bistreamlen++] = hu[h].bitstream[b];
			}
		}


		//////////////////////////////////////////////////////////////////////////
		// GLYPH COMPRESSION
		//////////////////////////////////////////////////////////////////////////

		int glyphcompbits = 0;
#ifdef GLYPH_COMPRESS_HUFFMAN
#if BLOCKSIZE!=8
		#error GLYPH_COMPRESS_HUFFMAN only works on 8-bit tiles
#endif
		{
			// Try glyph compression
			hufftype * gcodes = 0;
			hufffreq * gcounts = 0;
			int numgs = 0;
			uint32_t * runs = 0;
			int numruns = 0;
			for( i = 0; i < glyphct; i++ )
			{
				struct block * bb = glyphdata + i;
				int b;
				int rc = 0;
				int run = 0;
				for( b = 0; b < 64; b++ )
				{
					if( rc != ((bb->blockdata>>b)&1) )
					{
						numgs = HuffmanAppendHelper( &gcodes, &gcounts, numgs, run );
						runs = realloc( runs, (numruns+1)*sizeof(runs[0]));
						runs[numruns] = run;
						numruns++;
						run = 0;
					}
					else
					{
						run++;
					}
				}
				numgs = HuffmanAppendHelper( &gcodes, &gcounts, numgs, run );
				runs = realloc( runs, (numruns+1)*sizeof(runs[0]));
				runs[numruns] = run;
				numruns++;
			}
			int i;
			printf( "Numgsx: %d\n", numgs );
			for( i = 0; i < numgs; i++ )
			{
				printf( "%3d-%3d-%3d\n", i, gcounts[i], gcodes[i] );
			}
			int hufflen = 0;
			huffelement * het = GenerateHuffmanTree( gcodes, gcounts, numgs, &hufflen );

			int hulen = 0;
			huffup * hu = GenPairTable( het, &hulen );

#if 0
			printf( "HuffLens: %d %d\n", hufflen, hulen );
			for( i = 0; i < hulen; i++ )
			{
				huffup * u = hu + i;
				printf( "%3d%5d - ", u->value, u->freq );
				int k = 0;
				for( k = 0; k < u->bitlen; k++ )
					printf( "%c", u->bitstream[k] + '0' );
				printf( "\n" );
			}
#endif
			FILE * fgo = fopen( "glyphstream_out_table.dat", "wb" );
			if( hufflen > 255 )
			{
				fprintf( stderr, "Error: Glyph compression output not valid for this situation (HL: %d)\n", hufflen );
				return -44;
			}
			for( i = 0; i < hufflen; i++ )
			{
				huffelement * e = het + i;
				uint8_t A = 0;
				uint8_t B = 0;
				if( e->is_term )
				{
					A = 0;
					if( e->value > 255 )
					{
						fprintf( stderr, "Error: Glyph compression output not valid for this situation (GV: %d)\n", e->value );
						return -44;
					}
					B = e->value;
				}
				else
				{
					if( e->pair0 == 0 )
					{
						fprintf( stderr, "Error: Glyph compression output not valid for this situation (PAIR0: %d)\n", e->pair0 );
						return -44;
					}
					A = e->pair0;
					B = e->pair1;
				}
				fwrite( &A, 1, 1, fgo );
				fwrite( &B, 1, 1, fgo );
				glyphcompbits += 16;
			}
			fclose( fgo );

			fgo = fopen( "glyphstream_out_data.dat", "wb" );
			uint8_t runout = 0;
			uint8_t runcount = 0;
			for( i = 0; i < numruns; i++ )
			{
				uint32_t r = runs[i];
				int j;
				for( j = 0; j < hulen; j++ )
				{
					huffup * u = hu + j;
					if( u->value == r )
					{
						int bl = u->bitlen;
						glyphcompbits += bl;
						int t;
						for( t = 0; t < bl; t++ )
						{
							runout |= u->bitstream[t];
							runout<<=1;
							runcount++;
							if( runcount == 8 )
							{
								runcount = 0;
								fwrite( &runout, 1, 1, fgo );
								runout = 0;
								glyphcompbits+=8;
							}
						}
						break;
					}
				}
				if( j == hulen )
				{
					fprintf( stderr, "Error: could not find glyph element %d in table\n", r );
				}
			}
			if( runcount )
			{
				fwrite( &runout, 1, 1, fgo );
				glyphcompbits+=8;
			}
		}
#elif defined( GLYPH_COMPRESS_HUFFMAN_DATA )
#if BLOCKSIZE!=8
		#error GLYPH_COMPRESS_HUFFMAN only works on 8-bit tiles
#endif
		{
			// Try glyph compression
			hufftype * gcodes = 0;
			hufffreq * gcounts = 0;
			int numgs = 0;
			int prev1 = -1;
			int prev2 = -2;
			for( i = 0; i < glyphct; i++ )
			{
				struct block * bb = glyphdata + i;
				int b;
				int rc = 0;
				int run = 0;
				for( b = 0; b < 8; b++ )
				{
					uint8_t line = ((bb->blockdata) >> (b*8)) & 0xff;
					int code = line;
					if( line == prev1 ) code = 0xff; //XXX WARNING this is where we limit to 253 glyphs
					else if( line == prev2 ) code = 0xfe;
					prev2 = prev1;
					prev1 = line;
					numgs = HuffmanAppendHelper( &gcodes, &gcounts, numgs, code );
				}
			}
			int hufflen = 0;
			huffelement * het = GenerateHuffmanTree( gcodes, gcounts, numgs, &hufflen );

			int hulen = 0;
			huffup * hu = GenPairTable( het, &hulen );

#if 1
			printf( "HuffLens: %d %d\n", hufflen, hulen );
			for( i = 0; i < hulen; i++ )
			{
				huffup * u = hu + i;
				printf( "%3d %5d - ", u->value, u->freq );
				int k = 0;
				for( k = 0; k < u->bitlen; k++ )
					printf( "%c", u->bitstream[k] + '0' );
				printf( "\n" );
			}
#endif
			FILE * fgo = fopen( "glyphstream_out_table.dat", "wb" );
			if( hufflen > 255 )
			{
				fprintf( stderr, "Error: Glyph compression output not valid for this situation (HL: %d)\n", hufflen );
				return -44;
			}

			for( i = 0; i < hufflen; i++ )
			{
				huffelement * e = het + i;
				uint8_t A = 0;
				uint8_t B = 0;
				if( e->is_term )
				{
					A = 0;
					if( e->value > 255 )
					{
						fprintf( stderr, "Error: Glyph compression output not valid for this situation (GV: %d)\n", e->value );
						return -44;
					}
					B = e->value;
				}
				else
				{
					if( e->pair0 == 0 )
					{
						fprintf( stderr, "Error: Glyph compression output not valid for this situation (PAIR0: %d)\n", e->pair0 );
						return -44;
					}
					A = e->pair0;
					B = e->pair1;
				}
				fwrite( &A, 1, 1, fgo );
				fwrite( &B, 1, 1, fgo );
				glyphcompbits += 16;
			}
			fclose( fgo );

			fgo = fopen( "glyphstream_out_data.dat", "wb" );
			uint8_t runout = 0;
			uint8_t runcount = 0;
			prev1 = -1;
			prev2 = -2;
			for( i = 0; i < glyphct; i++ )
			{
				struct block * bb = glyphdata + i;
				int b;
				int rc = 0;
				int run = 0;
				for( b = 0; b < 8; b++ )
				{
					uint8_t line_check = ((bb->blockdata) >> (b*8)) & 0xff;

					int line = line_check;
					if( line_check == prev1 ) line = 0xff; //XXX WARNING this is where we limit to 253 glyphs
					else if( line_check == prev2 ) line = 0xfe;
					prev2 = prev1;
					prev1 = line_check;

					int j;
					for( j = 0; j < hulen; j++ )
					{
						huffup * u = hu + j;
						if( u->value == line )
						{
							int bl = u->bitlen;
							glyphcompbits += bl;
							int t;
							for( t = 0; t < bl; t++ )
							{
								runout |= u->bitstream[t];
								runout<<=1;
								runcount++;
								if( runcount == 8 )
								{
									runcount = 0;
									fwrite( &runout, 1, 1, fgo );
									runout = 0;
									glyphcompbits+=8;
								}
							}
							break;
						}
					}
					if( j == hulen )
					{
						fprintf( stderr, "Error: could not find glyph element %d in table\n", line );
					}
				}
			}
			if( runcount )
			{
				fwrite( &runout, 1, 1, fgo );
				glyphcompbits+=8;
			}
		}
#endif



		{
			FILE * fgo = fopen( "glyph_stream_raw.dat", "wb" );
			for( i = 0; i < glyphct; i++ )
			{
				struct block * bb = glyphdata + i;
#if BLOCKSIZE==8
				fwrite( &bb->blockdata, sizeof( bb->blockdata ), 1, fgo );
#elif BLOCKSIZE==16
				fwrite( bb->blockdata, sizeof( bb->blockdata ), 1, fgo );
#else 
				#error Confusing blocksize.
#endif
			}
			fclose( fgo );
		}


		//CNFGSwapBuffers();
		printf( "Bitstream Length Bits: %d\n", bistreamlen );
		printf( "Huff Length: %d\n", hufflen * 24 );
		printf( "Glyph Length Bits: %d -> %d (compressed if used) \n", glyphct * BLOCKSIZE * BLOCKSIZE, glyphcompbits );
		int total_bits = bistreamlen + hufflen * 24 +glyphct * BLOCKSIZE * BLOCKSIZE;
		printf( "Total: %d\n", bistreamlen + hufflen * 24 +glyphct * BLOCKSIZE * BLOCKSIZE ); 
		printf( "Bytes: %d\n", (total_bits+7)/8 );
		printf( "Bits Per Frame: %.1f\n", (float)total_bits/(float)num_video_frames );

		FILE * fH = fopen( "huffman_tree_table.dat", "wb" );
		int ema = 0, emv = 0;
		for( i = 0; i < hufflen; i++ )
		{
			huffelement * e = hufftree + i;
			int A, B;
			if( e->is_term )
			{
				if( e->value > 0xffff )
				{
					fprintf( stderr, "Error: Invalid value (%04x)\n", e->value );
					return -6;
				}
				A = e->value & 0xff;
				B = e->value >> 8;
				if( e->value > emv ) emv = e->value;
				B |= 0x800;
			}
			else
			{
				// This makes it easier to compress if we compress this.
				A = e->pair0 - i;
				B = e->pair1 - i;
				if( A < 0 || B < 0 )
				{
					fprintf( stderr, "Error: Huffman tree going backwards\n" );
				}

				if( ema < A ) ema = A;
				if( ema < B ) ema = B;

				if( A > 0x7ff || B > 0x7ff )
				{
					fprintf( stderr, "Too Big (%d %d)\n", A, B );
					return -5;
				}
			}
			uint32_t VO = A | (B << 12);
			fwrite( &VO, 3, 1, fH );
		}
		printf( "Max Huff ID: %d / %d\n", ema, emv );
		fclose( fH );

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


#ifdef ENCODE_HISTORY
		memset( encode_history, 0, sizeof(encode_history) );
		encode_history_head = 0;
#endif



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


#ifdef ENCODE_HISTORY
					if( tok & ENCODE_HISTORY_FLAG )
					{
						int behind = (tok & 0xfff)+1;
						int histpos = (encode_history_head - behind + ENCODE_HISTORY*2) % ENCODE_HISTORY;
						tok = encode_history[histpos];
					}
					else
					{
						encode_history[encode_history_head] = tok;
						encode_history_head = ( encode_history_head + 1 ) % ENCODE_HISTORY;
						total_hist++;
					}
#endif

#ifdef HUFFMAN_ALLOW_CODE_FOR_INVERT
					if( tok == GLYPH_NOATTRIB_MASK )
					{
						tok = lastblock[by][bx] ^ GLYPH_INVERSION_MASK;
					}
#endif

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
				struct block * b = &glyphdata[glyphid&GLYPH_NOATTRIB_MASK];
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


