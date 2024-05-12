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

	int nrtokens = 0;
	uint32_t * token_stream = 0;

	int tid = 0;

#if 0
	// for 64x48, 736735 bits needed
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
#else

	// this method does block-at-a-time, full video per block.
	// apples-to-apples, takes 659079 bits, an 11% savings!
	{
		int bx, by;
		for( by = 0; by < video_h/BLOCKSIZE; by++ )
		for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
		{
			uint32_t lastblock = 0;

			for( frame = 0; frame < num_video_frames; frame++ )
			{

				uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

				if( glyphid != lastblock )
				{
					if( running )
					{
						token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
						token_stream[nrtokens++] = FLAG_RLE | running;
					}

					token_stream = realloc( token_stream, (nrtokens+1) * sizeof( token_stream[0] ) );
					token_stream[nrtokens++] = glyphid;

					lastblock = glyphid;
					running = 0;
				}
				else
				{
					running++;
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

			int bx, by;
			for( by = 0; by < video_h/BLOCKSIZE; by++ )
			for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
			{
				uint32_t lastblock = 0;

				for( frame = 0; frame < num_video_frames; frame++ )
				{
					uint32_t glyphid = streamdata[(bx+by*(vbw)) + vbw*vbh * frame];

					if( glyphid != lastblock) 
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


						lastblock = glyphid;
						running = 0;
					}
					else
					{
						running++;
					}

					
				}
			}
			//CNFGSwapBuffers();
		printf( "Bitstream Length Bits: %d\n", bistreamlen );
		}

	}

#endif




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
			blocktype bt = glyphdata[glyphid];
			int bx = blockidhere % vbw;
			int by = blockidhere / vbw;
			DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
		}

		frame++;
	} while( tid < nrtokens );
}


