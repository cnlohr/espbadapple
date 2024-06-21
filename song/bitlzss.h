// XXX XXX INCOMPLETE ROUGH DRAFT

// Public domain bit-wise LZSS compression algorithm.
// Intended for small payloads.

#ifndef _BITLZSS_H
#define _BITLZSS_H

// as a note, this, on the stack creates two buffers, using 8*inlen + 80*max(outlen+2,inlen+2) worth of bytes, don't use massive
// buffers.
//
// Output is always in "lsb first" mode.
//
// if lzrss is used, it will reference the output stream, making required RAM a bare minimum.
// if lzrss_max_recur == 0, then regular lzss is used.
//
//
// return is 
static int CompressBitsLZSS( uint8_t * inbuffer, int inlen, uint8_t * outbuff, int outlen, int lsbfirst, int lzrss_max_recur );

#endif

#ifdef BITLZSS

#define MATCHES_WINDOW_MAX_BITS (64*8)

static int ComputeMatches( uint64_t * matches, uint8_t * matches_lens, uint8_t * outbufferbits, int outbufferbitsplace, int maxrecur )
{
	if( maxrecur == 0 ) return 0;
}

static int CompressBitsLZSS( uint8_t * inbuffer, int inlen, uint8_t outbuff, int outlen, int lsbfirst, int lzrss_max_recur )
{
	int i;
	uint8_t * inbuffer_bits = alloca( inlen * 8 );

	uint8_t * outbuffer_bits = alloca( outlen * 8 );

	int mlen = (((outlen>inlen)?outlen:inlen)+2)*8;
	uint64_t * matches = alloca( mlen * MATCHES_WINDOW_MAX/8 );
	uint8_t * matches_lens = alloca( mlen * sizeof( uint8_t ) );  // 0x80 = term'd, 0x3f = # of bits

	int matches_complete = 0;

	for( i = 0; i < inlen*8; i++ )
	{
		if( lsbfirst )
			inbuffer_bits[i] = !!(inbuffer[i/8] & (1<<(i&7)));
		else
			inbuffer_bits[i] = !!(inbuffer[i/8] & (1<<(7-(i&7))));
	}

	for( i = 0; i < mlen*8; i++ )
	{
		outbuffer_bits[i] = 0;
		outbuffer_matches[i] = 0;
		outbuffer_olen[i] = 0;
	}

	int ilb = inlen*8;
	int olb = 0;

	int i;
	for( i = 0; i < ilb; i++ )
	{
		int j;


		// Update outbuffer_matches and outbuffer_olen
		if( lzrss_max_recur )
		{
			for( j = outbuffer_matches_complete; j < olb; j++ )
			{
				ComputeMatchesOut( &matches[j], &matches_lens[j], outbuffer_bits, j, lzrss_max_recur );
				// If term'd and first, we can nudge along our matches complete marker.
				if( ( matches_lens[j] & 0x80 ) && j == outbuffer_matches_complete )
				{
					outbuffer_matches_complete++;
				}
			}
		}
		else
		{
			for( j = outbuffer_matches_complete; j < ilb; j++ )
			{
				ComputeMatchesOut( &matches[j], &matches_lens[j], outbuffer_bits + j, olb - j, lzrss_max_recur );
				// If term'd and first, we can nudge along our matches complete marker.
				if( ( matches_lens[j] & 0x80 ) && j == outbuffer_matches_complete )
				{
					outbuffer_matches_complete++;
				}
			}
		}
	}
}

#endif

