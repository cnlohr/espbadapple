#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"

int main()
{
	uint32_t elems[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	uint32_t freqs[12] = { 1, 3, 7, 100, 200, 1, 13, 200, 5, 10000, 0, 900 };
	int hufflen = 0;
	huffelement * es = GenerateHuffmanTree( elems, freqs, 12, &hufflen );
	printf( "Hufflen: %d\n", hufflen );
	int i;
	for( i = 0; i < hufflen; i++ )
	{
		huffelement * e = es + i;
		printf( "%d: %d [%d %d] [%d] %d\n", i, e->is_term, e->pair0, e->pair1, e->value, e->freq );
	}

	int helen = 0;
	huffup * he = GenPairTable( es, &helen );
	for( i = 0; i < helen; i++ )
	{
		printf( "%d -> %d [%d] = ", i, he[i].value, he[i].bitlen );
		int j;
		for( j = 0; j < he[i].bitlen; j++ )
			printf( "%d", he[i].bitstream[j] );
		printf( "\n" );
	}
}

