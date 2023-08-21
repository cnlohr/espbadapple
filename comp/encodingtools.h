#ifndef _ENCODINGTOOLS_H
#define _ENCODINGTOOLS_H

#include <stdint.h>

static int ETDeBruijnLog2( uint64_t v );  // Used internally.
static int ETEmitU( uint8_t * odata, int maxbits, int * tbytes, uint64_t data, int bits );

// Exponential-Golumb Coding
// https://en.wikipedia.org/wiki/Exponential-Golomb_coding
static int ETEmitSE( uint8_t * odata, int maxbits, int * tbytes, int64_t data );
static int ETEmitUE( uint8_t * odata, int maxbits, int * tbytes, int64_t data );



// 
static int ETDeBruijnLog2( uint64_t v )
{
	// https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
	// https://stackoverflow.com/questions/21888140/de-bruijn-algorithm-binary-digit-count-64bits-c-sharp

	// Note - if you are on a system with MSR or can compile to assembly, that is faster than this.
	// Otherwise, for normal C, this seems like a spicy way to roll.

	// Round v up!
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;

	static const int MultiplyDeBruijnBitPosition2[128] = 
	{
		0, // change to 1 if you want bitSize(0) = 1
		48, -1, -1, 31, -1, 15, 51, -1, 63, 5, -1, -1, -1, 19, -1, 
		23, 28, -1, -1, -1, 40, 36, 46, -1, 13, -1, -1, -1, 34, -1, 58,
		-1, 60, 2, 43, 55, -1, -1, -1, 50, 62, 4, -1, 18, 27, -1, 39, 
		45, -1, -1, 33, 57, -1, 1, 54, -1, 49, -1, 17, -1, -1, 32, -1,
		53, -1, 16, -1, -1, 52, -1, -1, -1, 64, 6, 7, 8, -1, 9, -1, 
		-1, -1, 20, 10, -1, -1, 24, -1, 29, -1, -1, 21, -1, 11, -1, -1,
		41, -1, 25, 37, -1, 47, -1, 30, 14, -1, -1, -1, -1, 22, -1, -1,
		35, 12, -1, -1, -1, 59, 42, -1, -1, 61, 3, 26, 38, 44, -1, 56
	};
	return MultiplyDeBruijnBitPosition2[(uint64_t)(v * 0x6c04f118e9966f6bUL) >> 57];
}

static int ETEmitU( uint8_t * odata, int maxbits, int * tbytes, uint64_t data, int bits )
{
	int tbt = *tbytes;
	for( bits--; bits >= 0 ; bits-- )
	{
		uint8_t b = ( data >> bits ) & 1;
		odata[tbt>>3] |= b << (7-(tbt&7));
		if( ++tbt >= maxbits )
			return -1;
	}
	*tbytes = tbt;
	return 0;
}

//For signed numbers.

static int ETEmitSE( uint8_t * odata, int maxbits, int * tbytes, int64_t data )
{
	if( data == 0 )
	{
		return ETEmitU( odata, maxbits, tbytes, 1, 1 );
	}
	uint64_t enc;
	if( data < 0 )
		enc = ((-data) << 1 ) | 1;
	else
		enc = data << 1;

	int numbits = ETDeBruijnLog2( enc );
	ETEmitU( odata, maxbits, tbytes, 0, numbits-1 );
	return ETEmitU( odata, maxbits, tbytes, enc, numbits );
}

// Sometimes written ue(v)
static int ETEmitUE( uint8_t * odata, int maxbits, int * tbytes, int64_t data )
{
	if( data == 0 )
	{
		return ETEmitU( odata, maxbits, tbytes, 1, 1 );
	}
	data++;
	int numbits = ETDeBruijnLog2( data );
	ETEmitU( odata, maxbits, tbytes, 0, numbits-1 );
	return ETEmitU( odata, maxbits, tbytes, data, numbits );
}

#endif


