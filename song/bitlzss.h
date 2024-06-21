// XXX XXX INCOMPLETE ROUGH DRAFT

// CURRENTLY FOCUSING on the lzrss.

// Public domain bit-wise LZSS compression algorithm.
// Intended for small payloads.

#ifndef _BITLZSS_H
#define _BITLZSS_H

// In bits
#define WINDOWWIDTH 8
#define LENWIDTH 6 // Right now can't be more than 7.

#include <stdint.h>
#include <alloca.h>

// format:
//
//  1 [windowwidth = num raw bytes] [raw data]
//  0 [windowwidth = offset from here in reverse] [lenwidth = num bit to use]

// as a note, this, on the stack creates two buffers, using 8*declen + (1<<LENWIDTH)*max(enclen+2,declen+2) worth of bytes, don't use massive
// buffers.
//
// Encoded data is always in "lsb first", you select your data based on lsb_first.
//
// if lzrss is used, it will reference the encoded stream, making required RAM a bare minimum.
// if lzrss_max_recur == 0, then regular lzss is used.
//
// lzrss_max_recur defines the maximum recursion the lzrss can descend.
//
// return is 
static int CompressBitsLZSS( const uint8_t * decbuffer, int declen, uint8_t * encbuff, int max_enclen, int lsbfirst, int lzrss_max_recur );

static int DecompressBitsLZSS( const uint8_t * encbuffer, int inclen_bits, uint8_t * decbuff, int max_declen, int lsbfirst, int lzrss_max_recur );

#endif

#ifdef BITLZSS_IMPLEMENTATION

// Defines the largest window of uncompressed text we can match against.
#define MATCHES_WINDOW_MAX_BITS (1<<LENWIDTH)

static int ReadNumber( uint8_t * data, int at, int datasize, int width )
{
	if( at + width > datasize ) return -1;
	int ret = 0;
	int i;
	for( i = 0; i < width; i++ )
		ret |= (data[i+at]<<i);
	return ret;
}

//		ComputeMatchesOut( &matches[j], &matches_lens[j], encbuffer_bits, j, lzrss_max_recur );
static int ComputeMatches( uint8_t * matches, uint8_t * matches_lens, int max_match_len, uint8_t * encbufferbits, int encbufferbitsplace, int encbufferbitslen, int maxrecur )
{
	if( maxrecur == 0 ) return 0;
	int bitssofar = 0;

	for(;;)
	{
		if( encbufferbitsplace >= encbufferbitslen )
			goto tdone;

		int bit = encbufferbits[encbufferbitsplace];
		if( bit == 1 )
		{
			int window = ReadNumber( encbufferbits, encbufferbitsplace, encbufferbitslen, WINDOWWIDTH );
			if( window < 0 )
				goto tdone;

			encbufferbitsplace += WINDOWWIDTH;

			if( encbufferbitsplace + window > encbufferbitslen )
				goto tdone;

			int i;
			for( i = 0; i < window; i++ )
			{
				if( i + *matches_lens >= max_match_len )
					goto tdone;
				matches[i+*matches_lens] = encbufferbits[encbufferbitsplace+i];
			}

			*matches_lens += window;
			encbufferbitsplace += window;
		}
		else
		{
			int offset = ReadNumber( encbufferbits, encbufferbitsplace, encbufferbitslen, WINDOWWIDTH );
			if( offset < 0 )
				goto tdone;
			int length = ReadNumber( encbufferbits, encbufferbitsplace, encbufferbitslen, LENWIDTH );
			if( length < 0 )
				goto tdone;
			ComputeMatches( matches, matches_lens, max_match_len, encbufferbits, encbufferbitsplace, encbufferbitslen, maxrecur - 1 );
		}
	}

tdone:
	*matches_lens = bitssofar;
	return bitssofar;
}


static int DecompressBitsLZSS( const uint8_t * encbuffer, int inclen_bits, uint8_t * decbuff, int max_declen, int lsbfirst, int lzrss_max_recur )
{
	uint8_t * decbuffer_bits = alloca( max_declen * 8 );
	uint8_t * encbuffer_bits = alloca( inclen_bits );
	int j;
	for( j = 0; j < inclen_bits; j++ )
	{
		encbuffer_bits[j] = (encbuffer[j/8] >> j) & 1;
	}

	uint8_t matches_lens = 0;

	if( lzrss_max_recur == 0 )
	{
		fprintf( stderr, "Error: regular lzss not written\n" );
		return -5;
	}
	else
	{
		int r = ComputeMatches( decbuffer_bits, &matches_lens, max_declen * 8, encbuffer_bits, 0, inclen_bits, lzrss_max_recur );
		printf( "Decode: %d / %d\n", r, matches_lens );
		return 0;
	}
}


static int CompressBitsLZSS( const uint8_t * decbuffer, int declen, uint8_t * encbuff, int enclen, int lsbfirst, int lzrss_max_recur )
{
	int i;
	uint8_t * decbuffer_bits = alloca( declen * 8 );

	uint8_t * encbuffer_bits = alloca( enclen * 8 );

	int mlen = (((enclen>declen)?enclen:declen)+2)*8;
	uint8_t * matches = alloca( mlen * MATCHES_WINDOW_MAX_BITS );
	uint8_t * matches_lens = alloca( mlen * sizeof( uint8_t ) );  // 0x80 = term'd, 0x7f = # of bits

	int matches_complete = 0;

	for( i = 0; i < declen*8; i++ )
	{
		if( lsbfirst )
			decbuffer_bits[i] = !!(decbuffer[i/8] & (1<<(i&7)));
		else
			decbuffer_bits[i] = !!(decbuffer[i/8] & (1<<(7-(i&7))));
	}

	for( i = 0; i < mlen*8; i++ )
	{
		encbuffer_bits[i] = 0;
		matches_lens[i] = 0;
		encbuffer_bits[i] = 0;
	}

	int ilb = declen*8;
	int olb = 0;

	int running_mark = 0;

	for( i = 0; i < ilb; i++ )
	{
		int j;


		// Update encbuffer_matches and encbuffer_olen
		if( lzrss_max_recur )
		{
			for( j = matches_complete; j < olb; j++ )
			{
				ComputeMatches( &matches[j], &matches_lens[j], MATCHES_WINDOW_MAX_BITS, encbuffer_bits, j, olb, lzrss_max_recur );
				// If term'd and first, we can nudge along our matches complete marker.
				if( ( matches_lens[j] & 0x80 ) && j == matches_complete )
				{
					matches_complete++;
				}
			}

			int current_bit = decbuffer_bits[i];
XXX HOW TO DO HERE?

			// Search in the history from here for the best historical match.
		}
		else
		{
/*			for( j = encbuffer_matches_complete; j < ilb; j++ )
			{
				ComputeMatches( &matches[j], &matches_lens[j], encbuffer_bits + j, olb - j, lzrss_max_recur );
				// If term'd and first, we can nudge along our matches complete marker.
				if( ( matches_lens[j] & 0x80 ) && j == encbuffer_matches_complete )
				{
					encbuffer_matches_complete++;
				}
			}
*/
			fprintf( stderr, "Error: not implemented.\n" );
		}
	}
}

#endif

