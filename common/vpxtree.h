// Explaination on how to store trees for storing multiple symbols when doing VPX coding.

// MSB is always at root of tree so you can lop off unused portions of the tree.
//
// Tree:
// L0  0
// L1   1               128
// L2    2     65        129       192
// L3     3 34   66  97   130  161  193   224
//
// L0  0
// L1   1           8
// L2    2     5     9     12
// L3     3  4  6  7  10 11 13 14
//
// This could be thought of 2 ways. MSB down or LSB up.
//  For MSB Down
//   if( next MSB bit ) { place += 2^(total_bits-level) } else { place++ }
//  For LSB Up:
//   For each bit in max bits:
//     if( next LSB bit ) { place += 2^level } else { place++ }
//
// This way lower codes are always stored to the left side of the tree
// so the right side can be lopped off.
//

#ifndef VPX_PROB_MULT
#define VPX_PROB_MULT 257.0
#endif

#ifndef VPX_PROB_SHIFT
#define VPX_PROB_SHIFT (-0.0)
#endif

static int VPXTreePlaceByLevelPlace( int level, int placeinlevel, int totallevels )
{
	int l;
	int p = 0;
	int vtoencode = placeinlevel << (totallevels-level);
	for( l = 0; l < level; l++ )
	{
		if( vtoencode & (1<<(totallevels-1)) )
			p += 1<<(totallevels-l-1);
		else
			p++;
		vtoencode <<= 1;
	}
	return p;
}

static inline int VPXTreeBitsForMaxElement( unsigned elements )
{
#if 0 && (defined( __GNUC__ ) || defined( __clang__ ))
	return 32 - __builtin_clz( elements );
#else
	int n = 32;
	unsigned y;
	unsigned x = elements;
	y = x >>16; if (y != 0) { n = n -16; x = y; }
	y = x >> 8; if (y != 0) { n = n - 8; x = y; }
	y = x >> 4; if (y != 0) { n = n - 4; x = y; }
	y = x >> 2; if (y != 0) { n = n - 2; x = y; }
	y = x >> 1; if (y != 0) return 32 - (n - 2);
	return 32 - (n - x);
#endif
}

static int VPXTreeGetSize( unsigned elements, unsigned needed_bits )
{
	int chancetable_len = 0;
	int levelplace = needed_bits-1;
	int level;
	int n = elements - 1;
	for( level = 0; level < needed_bits; level++ )
	{
		int comparemask = 1<<(needed_bits-level-1); //i.e. 0x02 one fewer than the levelmask
		int bit = !!(n & comparemask);
		if( bit )
			chancetable_len += 1<<(needed_bits-level-1);
		else
			chancetable_len++;
	}
	return chancetable_len;
}

// OUTPUTS probabilities
static void VPXTreeGenerateProbabilities( uint8_t * probabilities, unsigned nr_probabilities, float * frequencies, unsigned elements, unsigned needed_bits )
{
	int level;
	for( level = 0; level < needed_bits; level++ )
	{
		int maxmask = 1<<needed_bits;
		int levelmask = (0xffffffffULL >> (32 - level)) << (needed_bits-level); // i.e. 0xfc (number of bits that must match)
		int comparemask = 1<<(needed_bits-level-1); //i.e. 0x02 one fewer than the levelmask
		int lincmask = comparemask<<1;
		int maskcheck = 0;
		int placeinlevel = 0;
		for( maskcheck = 0; maskcheck < maxmask; maskcheck += lincmask )
		{
			float count1 = 0;
			float count0 = 0;
			int n;
			for( n = 0; n < (1<<needed_bits); n++ )
			{
				int tn = n;
				if( n >= elements ) continue;

				if( ( tn & levelmask ) == (maskcheck) )
				{
					if( tn & comparemask )
						count1 += frequencies[n];
					else
						count0 += frequencies[n];
				}
			}
			double chanceof0 = count0 / (double)(count0 + count1);
			int prob = chanceof0 * VPX_PROB_MULT - VPX_PROB_SHIFT;
			if( prob < 0 ) prob = 0;
			if( prob > 255 ) prob = 255;
			int place = VPXTreePlaceByLevelPlace( level, placeinlevel, needed_bits );
			if( place < nr_probabilities )
				probabilities[place] = prob;
			placeinlevel++;
		}
	}
}

static int VPXTreeRead( uint8_t * probabilities, int bits_for_max_element )
{
}

/*

Notes:

Output:
  0 
  1   8 
  2   5   9  12 
  3   4   6   7  10  11  13  14 

  0  0  0  0  0  0  0  0
  1  1  1  1  8  8  8  8
  2  2  5  5  9  9 12 12
  3  4  6  7 10 11 13 14

  0  0  0  0  0  0  0  0
  1  8  1  8  1  8  1  8
  2  9  5 12  2  9  5 12
  3 10  6 13  4 11  7 14


int main()
{
	int tree_bits = 4;

	int n;
	int l;
	for( l = 0; l < tree_bits; l++ )
	{
		for( n = 0; n < 1<<l; n++ )
		{
			printf( "%3d ", TreePlaceByLevelPlace( l, n, tree_bits ) );
		}
		printf( "\n" );
	}

	printf( "\n" );
	// Computing, top-down (MSB first)
	// You would use this for ENCODING or DECODING a VPX Tree, when navigating downward.
	for( l = 0; l < tree_bits; l++ )
	{
		for( n = 0; n < 1<<(tree_bits-1); n++ )
		{
			int p = 0;
			int tl;


			// levelplace starts
			int levelplace = tree_bits-1;

			// For each bit, pull off an MSB.
			for( tl = 0; tl < l; tl++ )
			{
				// msb here is not actually MSB but LSB, but when decoding you would use this, and you would "produce" MSB first.
				// This is the logic you would actually use.  Pretend (1<<(treeplace-1)) is your own code.
				int msb = n & (1<<(levelplace-1));
				if( msb )
					p += 1<<levelplace;
				else
					p++;

				levelplace--;
			}
			printf( "%3d", p );
		}
		printf( "\n" );
	}

	printf( "\n" );
	// Computing, bottom-up (LSB first)
	// You will almost never need to do this.
	for( l = 0; l < tree_bits; l++ )
	{
		for( n = 0; n < 1<<(tree_bits-1); n++ )
		{
			int p = 0;
			int tl;


			// levelplace starts
			int levelplace = tree_bits-1;

			// For each bit, pull off an MSB.
			for( tl = 0; tl < l; tl++ )
			{
				// msb here is not actually MSB but LSB, but when decoding you would use this, and you would "produce" MSB first.
				// This is the logic you would actually use.  Pretend (1<<(treeplace-1)) is your own code.
				int lsb = (n >> tl) & 1;
				if( lsb )
					p += 1<<levelplace;
				else
					p++;

				levelplace--;
			}
			printf( "%3d", p );
		}
		printf( "\n" );
	}
	
}

*/
