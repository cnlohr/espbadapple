#!/bin/tcc -run

// This generates a table used for taking the values for adjacent pixels and a current value. 

#include <stdio.h>

int main()
{
	int pprev;
	int pnext;
	int pthis;
	uint8_t potable[16] = { 0 };

	for( pprev = 0; pprev < 4; pprev++ )
	for( pnext = 0; pnext < 4; pnext++ )
	for( pthis = 0; pthis < 4; pthis++ )
	{
		int vt = (pthis < 2)?pthis:2;
		int vn = (pnext < 2)?pnext:2;
		int vp = (pprev < 2)?pprev:2;
		int vo = vt + (vn+vp+1)/2 - 1;
		if( vo < 0 ) vo = 0;
		if( vo > 2 ) vo = 3;

		int index = pprev * 4 + pnext;
		int bito = pthis * 2;
		potable[index] |= vo << bito;
	}

	int i;
	printf( "const uint8_t potable[16] = { " );
	for( i = 0; i < sizeof( potable ); i++ )
	{
		printf( "0x%02x, ", potable[i] );
	}
	printf( "};\n" );

	return 0;
}
