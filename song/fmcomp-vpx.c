#include <stdio.h>
#include <stdlib.h>

//XXX THIS DOES NOT WORK YET.  DATA OUTPUT IS BAD.

#define VPXCODING_READER
#define VPXCODING_WRITER
#include "vpxcoding.h"

#include "vpxtree.h"

// TODO: Can we also store substrings here?

struct DataTree
{
	int numUnique;
	int numList;
	uint32_t * uniqueMap;
	float * uniqueCount; // Float to play nice with the vpx tree code.
	uint32_t * fullList;
};

int GetIndexFromValue( struct DataTree * dt, uint32_t value )
{
	int i;
	for( i = 0; i < dt->numUnique; i++ )
	{
		if( dt->uniqueMap[i] == value )
		{
			return i;
		}
	}
	fprintf( stderr, "Error: Requesting unknown value (%d)\n", value );
	return -1;
}

void AddValue( struct DataTree * dt, uint32_t value )
{
	int nu = dt->numList;
	dt->fullList = realloc( dt->fullList, (nu+1) * sizeof(dt->fullList[0]) );
	dt->fullList[nu] = value;
	dt->numList = nu + 1;
	int n;
	for( n = 0; n < dt->numUnique; n++ )
	{
		if( dt->uniqueMap[n] == value )
		{
			dt->uniqueCount[n]++;
			return;
		}
	}

	dt->uniqueMap = realloc( dt->uniqueMap, (n+1)*sizeof(dt->uniqueMap[0]) );
	dt->uniqueMap[n] = value;
	dt->uniqueCount = realloc( dt->uniqueCount, (n+1)*sizeof(dt->uniqueCount[0]) );
	dt->uniqueCount[n] = 1;
	dt->numUnique = n+1;
}

int main()
{
	int i, j, k, l;
	FILE * f = fopen( "fmraw.dat", "rb" );

	struct DataTree dtNotes = { 0 };
	struct DataTree dtLenAndRun = { 0 };

	int numNotes = 0;

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

		//usedmask |= s;
		int note = (s>>8)&0xff; // ( (next_note >> 8 ) & 0xff) + 47;
		int len = (s>>3)&0x1f;       // ( (next_note >> 3 ) & 0x1f) + t + 1;
		int run = (s)&0x07;

		// Combine note and run.
		len |= run<<8;

		AddValue( &dtNotes, note );
		AddValue( &dtLenAndRun, len );
		numNotes++;
	}

	int bitsForNotes = VPXTreeBitsForMaxElement( dtNotes.numUnique );
	int bitsForLenAndRun = VPXTreeBitsForMaxElement( dtLenAndRun.numUnique );

	int treeSizeNotes = VPXTreeGetSize( dtNotes.numUnique, bitsForNotes );
	int treeSizeLenAndRun = VPXTreeGetSize( dtLenAndRun.numUnique, bitsForLenAndRun );

	uint8_t probabilitiesNotes[treeSizeNotes];
	uint8_t probabilitiesLenAndRun[treeSizeLenAndRun];

	printf( "===========\n" );
	VPXTreeGenerateProbabilities( probabilitiesNotes, treeSizeNotes, dtNotes.uniqueCount, dtNotes.numUnique, bitsForNotes );
	printf( "===========\n" );
	VPXTreeGenerateProbabilities( probabilitiesLenAndRun, treeSizeLenAndRun, dtLenAndRun.uniqueCount, dtLenAndRun.numUnique, bitsForLenAndRun );

	vpx_writer writer = { 0 };

	uint8_t vpxbuffer[1024*16];
	vpx_start_encode( &writer, vpxbuffer, sizeof(vpxbuffer) );

	printf( "Bits: %d/%d\n", bitsForNotes, bitsForLenAndRun );

	for( i = 0; i < dtNotes.numUnique; i++ )
	{
		printf( "%2d %02x %f\n", i, dtNotes.uniqueMap[i], dtNotes.uniqueCount[i] );
	}

	for( i = 0; i < treeSizeNotes; i++ )
	{
		printf( "%d ", probabilitiesNotes[i] );
	}
	printf( "\n" );

	for( i = 0; i < dtLenAndRun.numUnique; i++ )
	{
		printf( "%2d %04x %f\n", i, dtLenAndRun.uniqueMap[i], dtLenAndRun.uniqueCount[i] );
	}

	for( i = 0; i < treeSizeLenAndRun; i++ )
	{
		printf( "%d ", probabilitiesLenAndRun[i] );
	}
	printf( "\n" );

	for( i = 0; i < numNotes; i++ )
	{
		uint32_t note = dtNotes.fullList[i];
		uint32_t lenAndRun = dtLenAndRun.fullList[i];
		VPXTreeWriteSym( &writer, GetIndexFromValue( &dtNotes, note ),
			probabilitiesNotes, treeSizeNotes, bitsForNotes );

		VPXTreeWriteSym( &writer, GetIndexFromValue( &dtLenAndRun, dtLenAndRun.fullList[i] ),
			probabilitiesLenAndRun, treeSizeLenAndRun, bitsForLenAndRun );
	}
	printf( "Notes: %d\n", numNotes );
	uint32_t sum = writer.pos;
	printf( "Data: %d bytes\n", writer.pos );

	printf( "Notes: %d / %d\n", treeSizeNotes, dtNotes.numUnique * 1 );
	sum += treeSizeNotes;
	sum += dtNotes.numUnique * 1;

	printf( "LenAndRun: %d / %d\n", treeSizeLenAndRun, dtLenAndRun.numUnique * 2 );
	sum += treeSizeLenAndRun;
	sum += dtLenAndRun.numUnique * 2;

	printf( "Total: %d\n", sum );
	//unsigned int pos;
	//unsigned int size;

	
#if 0
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


	FILE * fTN = fopen( "huffTN_fmraw.dat", "wb" );
	FILE * fTL = fopen( "huffTL_fmraw.dat", "wb" );
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
			fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
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
			fwrite( &sym, 1, 2, fTL );
			htnlen2 += 2;
			fprintf( fData, "0x%02x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
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
	printf( "Huff Tree (D): %d bytes\n", htnlen2 );
	printf( "Data len: %d bytes\n", total_bytes );
	printf( "TOTAL: %d bytes\n", htnlen + htnlen2 + total_bytes );
#endif
	return 0;
}
