#include <stdio.h>
#include <stdlib.h>

#define HUFFER_IMPLEMENTATION
#include "../comp2/hufftreegen.h"


// TODO: Can we also store substrings here?

int main()
{
	int i, j, k, l;
	FILE * f = fopen( "fmraw.dat", "rb" );

	hufftype * symbols = 0;
	hufffreq * symcounts = 0;
	int numsym = 0;

	hufftype * lenss = 0;
	hufffreq * lencountss = 0;
	int numlens = 0;

	uint16_t * notearray = 0;
	int notecount = 0;
	uint16_t * lenarray = 0;
	int lencount = 0;


	uint16_t usedmask = 0;
	while( !feof( f ) )
	{
		uint16_t s;
		int r = fread( &s, 1, 2, f );
		if( r == 0 ) break;
		if( r != 2 )
		{
			fprintf( stderr, "Error: can't read symbol.\n" );
			return -6;
		}

		usedmask |= s;
		int note = s>>3;
		int len = s&7;;
		numsym = HuffmanAppendHelper( &symbols, &symcounts, numsym, note );
		notearray = realloc( notearray, (notecount + 1) * sizeof( notearray[0] ) );
		notearray[notecount++] = note;

		numlens = HuffmanAppendHelper( &lenss, &lencountss, numlens, len );
		lenarray = realloc( lenarray, (lencount + 1) * sizeof( lenarray[0] ) );
		lenarray[lencount++] = len;
	}
	
	int hufflen;
	huffelement * he = GenerateHuffmanTree( symbols, symcounts, numsym, &hufflen );

	int htlen = 0;
	huffup * hu = GenPairTable( he, &htlen );

	int hufflenl;
	huffelement * hel = GenerateHuffmanTree( lenss, lencountss, numlens, &hufflenl );

	int htlenl = 0;
	huffup * hul = GenPairTable( hel, &htlenl );

	printf( "NOTES:\n" );
	for( i = 0; i < htlen; i++ )
	{
		huffup * thu = hu + i;
		printf( "%3d: %04x :%5d : ", i, thu->value, thu->freq );

		for( k = 0; k < thu->bitlen; k++ )
			printf( "%c", thu->bitstream[k]+'0' );
		printf( "\n" );
	}
	printf( "LENS:\n" );
	for( i = 0; i < htlenl; i++ )
	{
		huffup * thu = hul + i;
		printf( "%3d: %04x :%5d : ", i, thu->value, thu->freq );

		for( k = 0; k < thu->bitlen; k++ )
			printf( "%c", thu->bitstream[k]+'0' );
		printf( "\n" );
	}


	FILE * fTN = fopen( "huffTN_fmraw.dat", "wb" );
	FILE * fTL = fopen( "huffTL_fmraw.dat", "wb" );
	FILE * fD = fopen( "huffD_fmraw.dat", "wb" );

	int maxpdA = 0;
	int maxpdB = 0;
	int htnlen = 0;
	for( i = 0; i < hufflen; i++ )
	{
		huffelement * h = he + i;
		if( h->is_term )
		{
			uint32_t sym = h->value;
			fwrite( &sym, 1, 2, fTN );
			htnlen += 2;
		}
		else
		{
			int pd0 = h->pair0 - i;
			int pd1 = h->pair1 - i;
			if( pd0 < 0 || pd1 < 0 )
			{
				fprintf( stderr, "Error: Illegal pd\n" );
				return -5;
			}
			if( pd0 > maxpdA ) maxpdA = pd0;
			if( pd1 > maxpdB ) maxpdB = pd1;
			uint32_t sym = 0x8000 | (pd0) | (pd1<<8);
			fwrite( &sym, 1, 2, fTN );
			htnlen += 2;
		}
	}

	printf( "max pd %d / %d\n", maxpdA, maxpdB );

	maxpdA = 0;
	maxpdB = 0;
	int htnlen2 = 0;
	for( i = 0; i < hufflenl; i++ )
	{
		huffelement * h = hel + i;
		if( h->is_term )
		{
			uint32_t sym = h->value;
			fwrite( &sym, 1, 1, fTL );
			htnlen2 += 1;
		}
		else
		{
			int pd0 = h->pair0 - i;
			int pd1 = h->pair1 - i;
			if( pd0 < 0 || pd1 < 0 )
			{
				fprintf( stderr, "Error: Illegal pd\n" );
				return -5;
			}
			if( pd0 > maxpdA ) maxpdA = pd0;
			if( pd1 > maxpdB ) maxpdB = pd1;
			uint32_t sym = 0x80 | (pd0) | (pd1<<4);
			fwrite( &sym, 1, 1, fTL );
			htnlen2 += 1;
		}
	}


	printf( "max pd %d / %d\n", maxpdA, maxpdB );

	uint8_t runbyte = 0;
	uint8_t runbyteplace = 0;
	int total_bytes = 0;
	for( i = 0; i < notecount; i++ )
	{
		int n = notearray[i];
		for( k = 0; k < htlen; k++ )
		{
			huffup * thu = hu + k;
			if( thu->value == n )
			{
				int l;
				for( l = 0; l < thu->bitlen; l++ )
				{
					runbyte |= thu->bitstream[l] << runbyteplace;
					runbyteplace++;
					if( runbyteplace == 8 )
					{
						total_bytes++;
						fwrite( &runbyte, 1, 1, fD );
						runbyte = 0;
						runbyteplace = 0;
					}
				}
				break;
			}
		}
		if( k == htlen )
		{
			fprintf( stderr, "Fault: Internal Error (%04x not in map)\n", n );
			return -4;
		}

		int l = lenarray[i];
		for( k = 0; k < htlenl; k++ )
		{
			huffup * thul = hul + k;
			if( thul->value == l )
			{
				int l;
				for( l = 0; l < thul->bitlen; l++ );
				{
					runbyte |= thul->bitstream[l] << runbyteplace;
					runbyteplace++;
					if( runbyteplace == 8 )
					{
						total_bytes++;
						fwrite( &runbyte, 1, 1, fD );
						runbyte = 0;
						runbyteplace = 0;
					}
				}
				break;
			}
		}
		if( k == htlenl )
		{
			fprintf( stderr, "Fault: Internal Error (run %d not in map)\n", l );
			return -4;
		}
	}

	printf( "Used mask: %04x\n", usedmask );
	printf( "Huff Tree (N): %d bytes\n", htnlen );
	printf( "Huff Tree (D): %d bytes\n", htnlen2 );
	printf( "Data len: %d bytes\n", total_bytes );
	printf( "TOTAL: %d bytes\n", htnlen + htnlen2 + total_bytes );
	return 0;
}
