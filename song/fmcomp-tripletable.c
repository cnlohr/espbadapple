#include <stdio.h>
#include <stdlib.h>

#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"


// TODO: Can we also store substrings here?

int main()
{
	int i, j, k, l;
	FILE * f = fopen( "fmraw.dat", "rb" );

	// Notes
	hufftype * symbols = 0;
	hufffreq * symcounts = 0;
	int numsym = 0;

	// The play length of the note
	hufftype * lenss = 0;
	hufffreq * lencountss = 0;
	int numlens = 0;

	// How long between each run (Not used)
	hufftype * runss = 0;
	hufffreq * runcountss = 0;
	int numrun = 0;

	uint16_t * notearray = 0;
	int notecount = 0;
	uint16_t * lenarray = 0;
	int lencount = 0;
	uint16_t * runarray = 0;
	int runcount = 0;


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
		int note = (s>>8)&0xff; // ( (next_note >> 8 ) & 0xff) + 47;
		int len = (s>>3)&0x1f;       // ( (next_note >> 3 ) & 0x1f) + t + 1;
		int run = (s)&0x07;

		// Combine note and run.
		//len |= run<<5;

		numsym = HuffmanAppendHelper( &symbols, &symcounts, numsym, note );
		notearray = realloc( notearray, (notecount + 1) * sizeof( notearray[0] ) );
		notearray[notecount++] = note;

		numlens = HuffmanAppendHelper( &lenss, &lencountss, numlens, len );
		lenarray = realloc( lenarray, (lencount + 1) * sizeof( lenarray[0] ) );
		lenarray[lencount++] = len;

		numrun = HuffmanAppendHelper( &runss, &runcountss, numrun, run );
		runarray = realloc( runarray, (runcount + 1) * sizeof( runarray[0] ) );
		runarray[runcount++] = run;
	}
	
	int hufflen;
	huffelement * he = GenerateHuffmanTree( symbols, symcounts, numsym, &hufflen );

	int htlen = 0;
	huffup * hu = GenPairTable( he, &htlen );

	int hufflenl;
	huffelement * hel = GenerateHuffmanTree( lenss, lencountss, numlens, &hufflenl );

	int htlenl = 0;
	huffup * hul = GenPairTable( hel, &htlenl );

	int hufflenr;
	huffelement * her = GenerateHuffmanTree( runss, runcountss, numrun, &hufflenr );

	int htlenr = 0;
	huffup * hur = GenPairTable( her, &htlenr );


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

	printf( "RUNS:\n" );
	for( i = 0; i < htlenr; i++ )
	{
		huffup * thu = hur + i;
		printf( "%3d: %04x :%5d : ", i, thu->value, thu->freq );

		for( k = 0; k < thu->bitlen; k++ )
			printf( "%c", thu->bitstream[k]+'0' );
		printf( "\n" );
	}


	FILE * fTN = fopen( "huffTN_fmraw.dat", "wb" );
	FILE * fTL = fopen( "huffTL_fmraw.dat", "wb" );
	FILE * fTR = fopen( "huffTR_fmraw.dat", "wb" );
	FILE * fD = fopen( "huffD_fmraw.dat", "wb" );

	FILE * fData = fopen( "espbadapple_song.h", "wb" );

	fprintf( fData, "#ifndef ESPBADAPPLE_SONG_H\n" );
	fprintf( fData, "#define ESPBADAPPLE_SONG_H\n\n" );
	fprintf( fData, "#include <stdint.h>\n\n" );

	fprintf( fData, "static uint16_t espbadapple_song_huffnote[%d] = {\n\t", hufflen );
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
			fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
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
			fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
			htnlen += 2;
		}
	}
	fprintf( fData, "};\n\n" );
	fprintf( fData, "static uint8_t espbadapple_song_hufflen[%d] = {\n\t", hufflenl );

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
			fprintf( fData, "0x%02x%s", sym, ((i%16)!=15)?", " : ",\n\t" );
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
			fprintf( fData, "0x%02x%s", sym, ((i%16)!=15)?", " : ",\n\t" );
		}
	}

	fprintf( fData, "};\n\n" );


	fprintf( fData, "static uint8_t espbadapple_song_huffrun[%d] = {\n\t", hufflenr );

	printf( "max pd %d / %d\n", maxpdA, maxpdB );

	maxpdA = 0;
	maxpdB = 0;
	int htnlen3 = 0;
	for( i = 0; i < hufflenr; i++ )
	{
		huffelement * h = hel + i;
		if( h->is_term )
		{
			uint32_t sym = h->value;
			fwrite( &sym, 1, 1, fTR );
			htnlen3 += 1;
			fprintf( fData, "0x%02x%s", sym, ((i%16)!=15)?", " : ",\n\t" );
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
			fwrite( &sym, 1, 1, fTR );
			htnlen3 += 1;
			fprintf( fData, "0x%02x%s", sym, ((i%16)!=15)?", " : ",\n\t" );
		}
	}

	fprintf( fData, "};\n\n" );

	fprintf( fData, "static uint8_t espbadapple_song_data[] = {\n\t" );

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
						fprintf( fData, "0x%02x%s", runbyte, ((total_bytes%16)!=15)?", " : ",\n\t" );
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
						fprintf( fData, "0x%02x%s", runbyte, ((total_bytes%16)!=15)?", " : ",\n\t" );
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

		int r = runarray[i];
		for( k = 0; k < htlenr; k++ )
		{
			huffup * thul = hur + k;
			if( thul->value == l )
			{
				int l;
				for( l = 0; l < thul->bitlen; l++ );
				{
					runbyte |= thul->bitstream[l] << runbyteplace;
					runbyteplace++;
					if( runbyteplace == 8 )
					{
						fprintf( fData, "0x%02x%s", runbyte, ((total_bytes%16)!=15)?", " : ",\n\t" );
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

	if( runbyteplace )
	{
		fwrite( &runbyte, 1, 1, fD );
		total_bytes++;
		fprintf( fData, "0x%02x%s", runbyte, ((total_bytes%16)!=15)?", " : ",\n\t" );
	}

	fprintf( fData, " };\n\n" );
	fprintf( fData, "#endif" );
	fclose( fData );
	printf( "Used mask: %04x\n", usedmask );
	printf( "Huff Tree (N): %d bytes\n", htnlen );
	printf( "Huff Tree (D): %d bytes\n", htnlen3 );
	printf( "Huff Tree (D): %d bytes\n", htnlen3 );
	printf( "Data len: %d bytes\n", total_bytes );
	printf( "TOTAL: %d bytes\n", htnlen + htnlen2 + htnlen3 + total_bytes );
	return 0;
}
