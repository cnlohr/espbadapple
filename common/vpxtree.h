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


#if 0
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
#endif


/*
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

				treeplace--;
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
