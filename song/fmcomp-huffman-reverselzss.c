#include <stdio.h>
#include <stdlib.h>

#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"

const int MinRL = 2;
const int MRBits = 16;
const int RLBits = 16;
const int MaxREV = (1<<MRBits)-1;
const int MaxRL = (1<<RLBits)-1;

FILE * fData, *fD;

uint8_t runbyte = 0;
uint8_t runbyteplace = 0;
int total_bytes = 0;
int bitcount = 0;

void EmitBit( int ib )
{
	runbyte |= ib << runbyteplace;
	runbyteplace++;

	if( runbyteplace == 8 )
	{
		fprintf( fData, "0x%02x%s", runbyte, ((total_bytes%16)!=15)?", " : ",\n\t" );
		total_bytes++;
		fwrite( &runbyte, 1, 1, fD );
		runbyte = 0;
		runbyteplace = 0;
	}
	bitcount++;
}



static inline int BitsForNumber( unsigned number )
{
	if( number == 0 ) return 0;
#if (defined( __GNUC__ ) || defined( __clang__ ))
	return 32 - __builtin_clz( number - 1 );
#else
	int n = 32;
	unsigned y;
	unsigned x = number - 1;
	y = x >>16; if (y != 0) { n = n -16; x = y; }
	y = x >> 8; if (y != 0) { n = n - 8; x = y; }
	y = x >> 4; if (y != 0) { n = n - 4; x = y; }
	y = x >> 2; if (y != 0) { n = n - 2; x = y; }
	y = x >> 1; if (y != 0) return 32 - (n - 2);
	return 32 - (n - x);
#endif
}


void EmitGolomb( int ib )
{
	int bits = BitsForNumber( ib );
	int i;
	for( i = 1; i < bits; i++ )
		EmitBit( 1 );
	EmitBit( 0 );
	for( i = 0; i < bits; i++ )
		EmitBit( (ib>>(bits-i-1)) & 1 );
}

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


	uint16_t usedmask = 0;
	int numNotes = 0;


	uint32_t * completeNoteList = 0;
	uint32_t * regNoteList = 0;

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


		completeNoteList = realloc( completeNoteList, sizeof(completeNoteList[0]) * (numNotes+1) );
		completeNoteList[numNotes] = s;

		//printf( "Append: %d %d\n", note, len );

		numNotes++;
	}

	// STAGE 1: DRY RUN.
	int numReg = 0;
	int numRev = 0;

	int highestNoteCnt = 0;

	for( i = 0; i < numNotes; i++ )
	{
		// Search for repeated sections.
		int searchStart = i - MaxREV - MaxRL - MinRL;
		if( searchStart < 0 ) searchStart = 0;
		int s;
		int bestrl = 0, bestrunstart = 0;
		for( s = searchStart; s <= i; s++ )
		{
			int ml;
			int mlc;
			int rl;

			// Midway through a backseek.  Can't use it.
//			if( bitmaplocation[s] < 0 ) continue;
//			if( bitmaplocation[s] < bitcount - MaxREV ) continue;

			for( 
				ml = s, mlc = i, rl = 0;
				ml < i && mlc < numNotes && rl < MaxRL;
				ml++, mlc++, rl++ )
			{
				if( completeNoteList[ml] != completeNoteList[mlc] ) break;
			}

			if( rl > bestrl )
			{
				bestrl = rl;
				bestrunstart = s;
			}
		}
		if( bestrl > MinRL )
		{
			//printf( "Found Readback at %d (%d %d) (D: %d)\n", i, i-bestrunstart, bestrl, i-bestrunstart-bestrl );
			i += bestrl-1;
			numRev++;

			//printf( "AT: %d BEST: LENG:%d  START:%d\n", i, bestrl, bestrunstart );
			int offset = i-bestrunstart-bestrl;
			//printf( "Emitting: %d %d (%d %d %d %d)\n", bestrl, offset, i, bestrunstart, bestrl, MinRL );

			int emit_best_rl = bestrl - MinRL - 1;

			// Output emit_best_rl, RLBits, Prob1RL
			// Output offset, MRBits, Prob1MR

			// Output emit_best_rl
			// Output offset
		}
		else
		{
			// No readback found, we will have toa ctaully emit encode this note.
			//AddValue( &dtNotes, note );
			//AddValue( &dtLenAndRun, len );

			regNoteList = realloc( regNoteList, sizeof(regNoteList[0]) * (numReg+1) );
			regNoteList[numReg] = completeNoteList[i];

			numReg++;
		}
	}


	printf( "Rev/Reg: %d %d\n", numRev, numReg );

	// this is only a rough approximation of the distribution that will be used.
	for( int r = 0; r < numReg; r++ )
	{
		int nv = regNoteList[r];

		int note = nv>>8;
		int len = nv&0xff;

		if( note >= highestNoteCnt ) highestNoteCnt = note+1;

		numsym = HuffmanAppendHelper( &symbols, &symcounts, numsym, note );
		numlens = HuffmanAppendHelper( &lenss, &lencountss, numlens, len );
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
	fD = fopen( "huffD_fmraw.dat", "wb" );

	fData = fopen( "../playback/badapple_song_huffman_reverselzss.h", "wb" );

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

	printf( "NOTES: %d\n", numNotes );


	for( i = 0; i < numNotes; i++ )
	{
		// Search for repeated sections.
		int searchStart = i - MaxREV - MaxRL - MinRL;
		if( searchStart < 0 ) searchStart = 0;
		int s;
		int bestrl = 0, bestrunstart = 0;
		for( s = searchStart; s <= i; s++ )
		{
			int ml;
			int mlc;
			int rl;
			for( 
				ml = s, mlc = i, rl = 0;
				ml < i && mlc < numNotes && rl < MaxRL;
				ml++, mlc++, rl++ )
			{
				if( completeNoteList[ml] != completeNoteList[mlc] ) break;
			}

			if( rl > bestrl && // Make sure it's best
				s + MinRL + rl + MaxREV >= i )
			{
				bestrl = rl;
				bestrunstart = s;
			}
		}
		if( bestrl > MinRL )
		{
			//printf( "Found Readback at %d (%d %d) (D: %d)\n", i, i-bestrunstart, bestrl, i-bestrunstart-bestrl );
			i += bestrl-1;
			numRev++;

			//printf( "AT: %d BEST: LENG:%d  START:%d\n", i, bestrl, bestrunstart );
			int offset = i-bestrunstart-bestrl;
			//printf( "Emitting: %d %d (%d %d %d %d)\n", bestrl, offset, i, bestrunstart, bestrl, MinRL );

			int emit_best_rl = bestrl - MinRL - 1;

			// Output emit_best_rl, RLBits, Prob1RL
			// Output offset, MRBits, Prob1MR

			// Output emit_best_rl
			// Output offset
		}
		else
		{
			// No readback found, we will have toa ctaully emit encode this note.
			//AddValue( &dtNotes, note );
			//AddValue( &dtLenAndRun, len );

			regNoteList = realloc( regNoteList, sizeof(regNoteList[0]) * (numReg+1) );
			regNoteList[numReg] = completeNoteList[i];

			numReg++;
		}
	}



	int bitmaplocation[numNotes];


	for( i = 0; i < numNotes; i++ )
	{
		// Search for repeated sections.
		int searchStart = i - MaxREV - MaxRL - MinRL;
		if( searchStart < 0 ) searchStart = 0;
		int s;
		int bestrl = 0, bestrunstart = 0;
		for( s = searchStart; s <= i; s++ )
		{
			int ml;
			int mlc;
			int rl;

			// Midway through a backseek.  Can't use it.
			if( bitmaplocation[s] < 0 ) continue;
			if( bitmaplocation[s] < bitcount - MaxREV ) continue;

			for( 
				ml = s, mlc = i, rl = 0;
				ml < i && mlc < numNotes && rl < MaxRL;
				ml++, mlc++, rl++ )
			{
				if( completeNoteList[ml] != completeNoteList[mlc] ) break;
			}

			if( rl > bestrl )
			{
				bestrl = rl;
				bestrunstart = s;
			}
		}
		if( bestrl > MinRL )
		{
			EmitBit( 1 );
			int bcstart = bitcount;
			bitmaplocation[i] = bitcount;

			int sourcebplace = bitmaplocation[bestrunstart];

			//printf( "Found Readback at %d (%d %d) (D: %d)\n", i, i-bestrunstart, bestrl, i-bestrunstart-bestrl );
			int endr = i + bestrl-1;

			for( ; i < endr; i++ )
			{
				// We can't jump to the middle of the callback (unless we decided to include more info).
				bitmaplocation[i] = -1;
			}
			numRev++;

			//printf( "AT: %d BEST: LENG:%d  START:%d\n", i, bestrl, bestrunstart );
			int offset = i-bestrunstart-bestrl;
			//printf( "Emitting: %d %d (%d %d %d %d)\n", bestrl, offset, i, bestrunstart, bestrl, MinRL );

			int emit_best_rl = bestrl - MinRL - 1;

			// Output emit_best_rl, RLBits, Prob1RL
			// Output offset, MRBits, Prob1MR
			// Output emit_best_rl
			// Output offset
			EmitGolomb( emit_best_rl );
			EmitGolomb( offset );

			//printf( "EMITT  %d %d at %d from %d(%d) BL:%d\n", emit_best_rl, offset, bitcount, sourcebplace, bestrunstart, bitcount - bcstart );
		}
		else
		{
			EmitBit( 0 );
			int n = completeNoteList[i] >> 8;
			int bitcountatstart = bitcount;
			bitmaplocation[i] = bitcount;

			for( k = 0; k < htlen; k++ )
			{
				huffup * thu = hu + k;
				if( thu->value == n )
				{
					int ll;
					for( ll = 0; ll < thu->bitlen; ll++ )
					{
						EmitBit( thu->bitstream[ll] );
					}
					break;
				}
			}
			if( k == htlen )
			{
				fprintf( stderr, "Fault: Internal Error (%04x not in map)\n", n );
				return -4;
			}
			int lev =  completeNoteList[i] & 0xff;
			for( k = 0; k < htlenl; k++ )
			{
				huffup * thul = hul + k;
				if( thul->value == lev )
				{
					int ll;
					for( ll = 0; ll < thul->bitlen; ll++ )
					{
						EmitBit( ll );
					}
					break;
				}
			}
			if( k == htlenl )
			{
				fprintf( stderr, "Fault: Internal Error (run %d not in map)\n", l );
				return -4;
			}
			//printf( "Write: %d\n", bitcount, bitcountatstart );
		}
	}

	printf( "Bitcount: %d\n", bitcount );

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
	printf( "Data len: %d bytes (%d -> %d)\n", total_bytes, bitcount, (bitcount+7)/8 );
	printf( "TOTAL: %d bytes\n", htnlen + htnlen2 + total_bytes );
	return 0;
}
