#include <stdio.h>
#include "bacommon.h"
#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"

int streamcount;
uint32_t * streamdata;

int glyphct;
blocktype * glyphdata;

int video_w;
int video_h;
int num_video_frames;

#define FLAG_RLE 0x8000

int main( int argc, char ** argv )
{
	CNFGSetup( "comp test", 1800, 900 );

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
	glyphct = ftell( f ) / sizeof( glyphdata[0] );
	glyphdata = malloc( glyphct * sizeof( glyphdata[0] ) );
	fseek( f, 0, SEEK_SET );
	fread( glyphdata, glyphct, sizeof( glyphdata[0] ) , f );
	fclose( f );

	num_video_frames = streamcount / ( video_w * video_h / ( BLOCKSIZE * BLOCKSIZE ) );

	printf( "Read %d glyphs, and %d elements (From %d frames)\n", glyphct, streamcount, num_video_frames );
	int i;
	int frame = 0;
	int block = 0;

	int vbh = video_h/BLOCKSIZE;
	int vbw = video_w/BLOCKSIZE;
	uint32_t blockmap[vbh*vbw];
	memset( blockmap, 0, vbh*vbw*sizeof(uint32_t) );

	int running = 0;

	int tid = 0;

#if 0
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

			blocktype bt = glyphdata[glyphid];
			//DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
		}

		//CNFGSwapBuffers();
	}

	printf( "Processed %d frames\n", frame );
	printf( "Number of stream TIDs: %d\n", nrtokens );

	uint8_t * huffman_data;
	uint32_t huffman_bit_count = 0;
	uint16_t huffman_dictionary = 0;

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

		block = 0;

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

				blocktype bt = glyphdata[glyphid];
				//DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
			}

			//CNFGSwapBuffers();
		}


		printf( "Bitstream Length Bits: %d\n", bistreamlen );
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
				usleep(10000);
				if( !CNFGHandleInput() ) break;
			}
			uint32_t glyphid = blockmap[blockidhere];
			blocktype bt = glyphdata[glyphid];
			int bx = blockidhere % vbw;
			int by = blockidhere / vbw;
			DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
		}

		frame++;
	} while( tid < nrtokens );

#elif 0

	int nrtokens = 0;
	uint32_t * token_stream = 0;

	// this method does block-at-a-time, full video per block.
	// apples-to-apples, takes 538295 bits (AB TEST #4A)  34% smaller!!!
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
//printf( "+ %04x\n", FLAG_RLE | forward );
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

		uint8_t * huffman_data;
		uint32_t huffman_bit_count = 0;
		uint16_t huffman_dictionary = 0;

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

		int bitstream_place = 0;
		//char * bitstreamo = 0;
		//int bistreamlen = 0;
		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );
		int32_t next_tok = -1;

		int k;
		for( k = 0; k < 200; k++ )
		{
			printf( "%d", bitstreamo[k] );
		}
		printf( "\n" );

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
				blocktype bt = glyphdata[glyphid];
				DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
			}

			CNFGSwapBuffers();
			frame++;
		}
	}
#else

	// this method does block-at-a-time, full video per block.
	// BUT, With SPLIT tables for glyph ID / 

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

		uint8_t * huffman_data;
		uint32_t huffman_bit_count = 0;
		uint16_t huffman_dictionary = 0;

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
#if 1
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
					if( h == htlen ) { fprintf( stderr, "Error: Missing symbol %d\n", glyphid ); exit( -6 ); }
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

		int bitstream_place = 0;
		//char * bitstreamo = 0;
		//int bistreamlen = 0;
		memset( running, 0, sizeof(running) );
		memset( lastblock, 0, sizeof( lastblock ) );
		int32_t next_tok = -1;

		int k;
		for( k = 0; k < 200; k++ )
		{
			printf( "%d", bitstreamo[k] );
		}
		printf( "\n" );

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
				blocktype bt = glyphdata[glyphid];
				DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
			}

			usleep(2000);
			CNFGSwapBuffers();
			frame++;
		}
	}


#endif

}


