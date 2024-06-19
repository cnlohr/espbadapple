// Public Domain Huffman Table Creator
//  2022-2024 cnlohr
// You may license this under MIT/x11, NewBSD, CC0, MIT0 or Public Domain
// Pass in a bunch of data, and the count of that data.
// It will give you an optimal huffman tree.

#ifndef _HUFFTREEGEN_H
#define _HUFFTREEGEN_H

#include <string.h>
#include <stdint.h>

// Header-only huffman-table creator.

#ifndef hufftype
#define hufftype uint32_t
#endif

#ifndef hufffreq
#define hufffreq uint32_t
#endif


typedef struct
{
	hufffreq freq;
	uint8_t is_term;

	// valid if is_term is 0.
	int pair0;
	int pair1;

	// valid if is_term is 1.
	hufftype value;
} huffelement;

int HuffmanAppendHelper( hufftype ** vals, hufffreq ** counts, int elems, hufftype t );

huffelement * GenerateHuffmanTree( hufftype * data, hufffreq * frequencies, int numpairs, int * hufflen );

typedef struct
{
	hufftype value;
	int bitlen;
	uint8_t * bitstream;
	hufffreq freq;
} huffup;

huffup * GenPairTable( huffelement * table, int * htlen );

#ifdef HUFFER_IMPLEMENTATION

/*
static void PrintHeap( int * heap, int len, huffelement * tab )
{
	int i;
	printf( "Len: %d\n", len );
	for( i = 0; i < len; i++ )
		printf( "%d %d  %d\n", i, heap[i], tab[heap[i]].freq );
}
*/

static void PercDown( int p, int size, huffelement * tab, int * heap )
{
	hufffreq f = tab[heap[p]].freq;
	do
	{
		int left = 2*p+1;
		int right = left+1;
		int smallest = p;
		hufffreq tf = f;
		hufffreq fcomp = f;
		if( left < size && (tf = tab[heap[left]].freq ) < fcomp )
		{
			smallest = left;
			fcomp = tf;
		}
		if( right < size && (tf = tab[heap[right]].freq ) < fcomp )
		{
			smallest = right;
		}
		if( smallest == p ) return;
		// Otherwise, swap.
		int temp = heap[p];
		heap[p] = heap[smallest];
		heap[smallest] = temp;
		p = smallest;
	} while( 1 );
}

huffelement * GenerateHuffmanTree( hufftype * data, hufffreq * frequencies, int numpairs, int * hufflen )
{
	int e, i;

	if( numpairs < 1 )
	{
		if( hufflen ) *hufflen = 0;
		return 0;
	}

	int oecount = 0;

	for( i = 0; i < numpairs; i++ )
	{
		// Cull out any zero frequency elements.
		if( frequencies[i] ) oecount++;
	}

	// We actually want a min-heap of all of the possible symbols
	// to accelerate the generation of the tree.

	int hl = *hufflen = oecount * 2 - 1;
	huffelement * tab = calloc( sizeof( huffelement ), hl );
	int * minheap = calloc( sizeof( int ), oecount );
	int minhlen = 0;

	for( i = oecount-1, e = 0; i < hl; i++, e++ )
	{
		while( frequencies[e] == 0 ) e++;
		huffelement * t = &tab[i];
		t->is_term = 1;
		t->value = data[e];
		int tf = t->freq = frequencies[e];

		// Append value to heap.
		int h = i;
		int p = minhlen++;
		minheap[p] = i;

		while( p )
		{
			int parent = (p-1)/2;
			int ph = minheap[parent];
			int pf = tab[ph].freq;
			if( pf <= tf ) // All is well.
				break;

			// Otherwise, flip.
			minheap[parent] = h;
			minheap[p] = ph;
			//tf = pf;
			//h = ph;
			p = parent;
		}


/*printf( "***HEAP*** %d\n", minhlen );
int tmp;
for( tmp=0; tmp<minhlen;tmp++)
{
	printf( "%d - FRQ: %d\n", minheap[tmp], tab[minheap[tmp]].freq );
}
*/

	}

	for( i = oecount-2; i >= 0; i-- )
	{
		huffelement * e = &tab[i];
		// Filling in the huffman tree from the back, forward.
		e->pair0 = minheap[0];
		minheap[0] = minheap[--minhlen];

		// Percolate-down.
		PercDown( 0, minhlen, tab, minheap );

		e->pair1 = minheap[0];

		e->is_term = 0;

		e->freq = tab[e->pair0].freq + tab[e->pair1].freq;

		minheap[0] = i;
		PercDown( 0, minhlen, tab, minheap );

	//	printf( "SECOND!!!\n" );
	//	PrintHeap( minheap, minhlen, tab );
	}


	return tab;
}

void InternalHuffT( huffelement * tab, int p, huffup ** out, int * huffuplen, uint8_t ** dstr, int * dstrlen )
{
	huffelement * ths = &tab[p];
	if( ths->is_term )
	{
		int newlen = ++(*huffuplen);
		*out = realloc( *out, sizeof( huffup ) * ( newlen ) );
		huffup * e = &(*out)[newlen-1];
		e->value = ths->value;

		e->bitlen = *dstrlen;
		e->bitstream = malloc( *dstrlen + 1 );
		memcpy( e->bitstream, *dstr, *dstrlen );
		e->freq = ths->freq;
	}
	else
	{
		int dla = *dstrlen;
		*dstr = realloc( *dstr, ++(*dstrlen) );
		(*dstr)[dla] = 0;
		InternalHuffT( tab, tab[p].pair0, out, huffuplen, dstr, dstrlen );
		(*dstr)[dla] = 1;
		InternalHuffT( tab, tab[p].pair1, out, huffuplen, dstr, dstrlen );
		(*dstrlen)--;
	}
}

huffup * GenPairTable( huffelement * table, int * htlen )
{
	huffup * ret = 0;
	int huffuplen = 0;

	uint8_t * dstr = malloc( 1 );
	int dstrlen = 0;
	dstr[0] = 0;

	// Generate the table from a tree, recursively.
	InternalHuffT( table, 0, &ret, &huffuplen, &dstr, &dstrlen );
	if( htlen ) *htlen = huffuplen;
	return ret;
}

int HuffmanAppendHelper( hufftype ** vals, hufffreq ** counts, int elems, hufftype t )
{
	int j;
	for( j = 0; j < elems; j++ )
	{
		if( (*vals)[j] == t )
		{
			(*counts)[j]++;
			return elems;
		}
	}
	elems++;
	*vals = realloc( *vals, (elems)*sizeof(hufftype) );
	*counts = realloc( *counts, (elems)*sizeof(hufffreq) );
	(*vals)[elems-1] = t;
	(*counts)[elems-1] = 1;
	return elems;
}

#endif

#endif

