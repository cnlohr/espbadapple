// XXX XXX INCOMPLETE ROUGH DRAFT

// CURRENTLY FOCUSING on the lzrss.

// Public domain bit-wise LZSS compression algorithm.
// Intended for small payloads.

#ifndef _BITLZSS_H
#define _BITLZSS_H

// In bits
#define WINDOWWIDTH 8
#define LENWIDTH 6 // Right now can't be more than 7.

#define FLAG_HISTORY 0
#define FLAG_LITERAL 1
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

static int WriteNumber( uint8_t * data, int at, int datasize, int number_to_write, int width )
{
	if( at + width > datasize ) return -1;
	int p;
	for( p = 0; p < width; p++ )
	{
		data[at+p] = (number_to_write>>p)&1;
	}
	return 0;
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
		if( bit == FLAG_LITERAL )
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

int CheckMatchLen( uint8_t * matchbits, uint8_t * decbits, int declen_bits )
{
	int k;
	for( k = 0; k < declen_bits; k++ )
	{
		if( matchbits[k] != decbits[k] ) break;
	}
	return k;
}


static int CompressBitsLZSS( const uint8_t * decbuffer, int declen, uint8_t * encbuff, int enclen, int lsbfirst, int lzrss_max_recur )
{
	int i;
	int declen_bits = declen * 8;
	uint8_t * decbuffer_bits = alloca( declen_bits );

	int encbuffer_bits_len = enclen * 8;
	uint8_t * encbuffer_bits = alloca( enclen * 8 );

	int mlen = (((enclen>declen)?enclen:declen)+2)*8;
	uint8_t * matches = alloca( mlen * MATCHES_WINDOW_MAX_BITS );
	uint8_t * matches_lens = alloca( mlen * sizeof( uint8_t ) );  // 0x80 = term'd, 0x7f = # of bits

	int matches_complete = 0;

	for( i = 0; i < declen_bits; i++ )
	{
		if( lsbfirst )
			decbuffer_bits[i] = !!(decbuffer[i/8] & (1<<(i&7)));
		else
			decbuffer_bits[i] = !!(decbuffer[i/8] & (1<<(7-(i&7))));
	}

	for( i = 0; i < encbuffer_bits_len; i++ )
	{
		matches_lens[i] = 0;
		encbuffer_bits[i] = 0;
	}

	int olb = 0;

	int literal_mark = 0;


	if( lzrss_max_recur )
	{
		int place_of_running_mark;

		// Seed with a 0 size window.
		if( WriteNumber( encbuffer_bits, 0, encbuffer_bits_len, FLAG_LITERAL, 1 ) ) goto earlyabort;
		olb += 1;
		literal_mark = olb;
		if( WriteNumber( encbuffer_bits, literal_mark, encbuffer_bits_len, 0, WINDOWWIDTH ) ) goto earlyabort;
		olb += WINDOWWIDTH;

		for( i = 0; i < declen_bits; i++ )
		{
			int j;


			// Update encbuffer_matches and encbuffer_olen
			for( j = matches_complete; j < olb; j++ )
			{
				ComputeMatches( &matches[j], &matches_lens[j], MATCHES_WINDOW_MAX_BITS, encbuffer_bits, j, olb, lzrss_max_recur );
				// If term'd and first, we can nudge along our matches complete marker.
				if( ( matches_lens[j] & 0x80 ) && j == matches_complete )
				{
					matches_complete++;
				}
			}

			// From here, let's explore if there are any forward-matches we could use that would be "worth it" to use history.
			int bestmatchlen = 0;
			int bestmatchoffset = 0;
			for( j = olb - (1<<WINDOWWIDTH); j < olb; j++ )
			{
				int matchlen = CheckMatchLen( &matches[j], decbuffer_bits + i, declen_bits - i );
				if( matchlen > bestmatchlen )
				{
					bestmatchlen = matchlen - 1;
					if( bestmatchlen >= LENWIDTH ) bestmatchlen = LENWIDTH-1;
					bestmatchoffset = olb - j - 1;
				}
			}

			if( bestmatchlen > (WINDOWWIDTH*2+LENWIDTH) )
			{
				// Emit the hop
				if( WriteNumber( encbuffer_bits, olb, encbuffer_bits_len, FLAG_LITERAL, 0 ) ) goto earlyabort;
				olb += 1;
				if( WriteNumber( encbuffer_bits, literal_mark, encbuffer_bits_len, bestmatchoffset, WINDOWWIDTH ) ) goto earlyabort;
				olb += WINDOWWIDTH;				
				if( WriteNumber( encbuffer_bits, literal_mark, encbuffer_bits_len, bestmatchlen, LENWIDTH ) ) goto earlyabort;
				olb += LENWIDTH;				
				
				literal_mark = olb;
				i += bestmatchlen + 1;

				// Emit a literal
				if( WriteNumber( encbuffer_bits, olb, encbuffer_bits_len, FLAG_LITERAL, 1 ) ) goto earlyabort;
				olb += 1;
				literal_mark = olb;
				if( WriteNumber( encbuffer_bits, literal_mark, encbuffer_bits_len, 0, WINDOWWIDTH ) ) goto earlyabort;
				olb += WINDOWWIDTH;				
			}
			else
			{
				if( olb == (literal_mark + WINDOWWIDTH) )
				{
					// Re-up.
					// Emit a literal
					if( WriteNumber( encbuffer_bits, olb, encbuffer_bits_len, FLAG_LITERAL, 1 ) ) goto earlyabort;
					olb += 1;
					literal_mark = olb;
					if( WriteNumber( encbuffer_bits, literal_mark, encbuffer_bits_len, 0, WINDOWWIDTH ) ) goto earlyabort;
					olb += WINDOWWIDTH;				
				}
				encbuffer_bits[olb++] = decbuffer_bits[i];
				WriteNumber( encbuffer_bits, literal_mark, encbuffer_bits_len, olb - (literal_mark + WINDOWWIDTH), WINDOWWIDTH );
			}
			

			// Search in the history from here for the best historical match.
		}
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
	return olb;
earlyabort:
	return -1;
}

#endif

