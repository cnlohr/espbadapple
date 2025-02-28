#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#define HUFFER_IMPLEMENTATION
#include "hufftreegen.h"

const int MinRL = 2;
#define OFFSET_MINIMUM 7
#define MAX_BACK_DEPTH 18

FILE * fData, *fD;

// Combine run + note + length
//#define SINGLETABLE

// TODO: NOTE:
//   There are some serious optimizations that are possible even from here.
//      1. Instead of outputting a 1 or a 0 bit for every symbol or reverse pull.
//         This is tricky though, because, you would need to "cork" the output and
//         rewrite it once you know the proper length.  When we tested it, it only
//         saved about 10 bytes, so we decided it was not worth it.


// Combine run + note + length
//#define SINGLETABLE

// Huffman generation trees.
huffup * hu;
huffup * hul;

// Huffman decoding trees.
huffelement * he;
huffelement * hel;

uint32_t runword = 0;
uint8_t runwordplace = 0;
int total_bytes = 0;
int bitcount = 0;

char * bitlist = 0; // size = bitcount

void EmitBit( int ib )
{
	runword |= ib << runwordplace;
	runwordplace++;
	if( runwordplace == 32 )
	{
		if( fData ) fprintf( fData, "0x%08x%s", runword, ((total_bytes%32)!=28)?", " : ",\n\t" );
		total_bytes+=4;
		if( fD ) fwrite( &runword, 1, 4, fD );
		runword = 0;
		runwordplace = 0;
	}
	bitlist = realloc( bitlist, bitcount+1 );
	bitlist[bitcount] = ib;
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


int EmitExpGolomb( int ib )
{
	int bitsemit = 0;
	int bits = (ib == 0) ? 1 : BitsForNumber( ib+2 );
	int i;
	for( i = 1; i < bits; i++ )
	{
		EmitBit( 0 );
		bitsemit++;
	}

	if( bits )
	{
		ib++;
		for( i = 0; i < bits; i++ )
		{
			EmitBit( ((ib)>>(bits-i-1)) & 1 );
			bitsemit++;
		}
	}

	return bitsemit;
}

int PullBit( int * bp )
{
	if( *bp >= bitcount ) return -2;
	return bitlist[(*bp)++];
}

int PullExpGolomb( int * bp )
{
	int exp = 0;
	do
	{
		int b = PullBit( bp );
		if( b < 0 ) return b;
		if( b != 0 ) break;
		exp++;
	} while( 1 );

	int br;
	int v = 1;
	for( br = 0; br < exp; br++ )
	{
		v = v << 1;
		int b = PullBit( bp );
		if( b < 0 ) return b;
		v |= b;
	}
	return v-1;
}

int PullHuff( int * bp, huffelement * he )
{
	int ofs = 0;
	huffelement * e = he + ofs;
	do
	{
		if( e->is_term ) return e->value;

		int b = PullBit( bp );
		if( b < 0 ) return b;
//printf( "%d (%d)", b, *bp );

		//  ofs + 1 + if encoded in table
		ofs = (b ? (e->pair1) : (e->pair0) );
		e = he + ofs;
	} while( 1 );
}

int DecodeMatch( int startbit, uint32_t * notes_to_match, int length_max_to_match, int * depth )
{
//	printf( "RMRS: %d\n", startbit );
	// First, pull off 
	// Read from char * bitlist = 0; // size = bitcount
	int matchno = 0;
	int bp = startbit;
	//printf( "Decode Match Check At %d\n", startbit );
	do
	{
		//printf( "CHECKP @ bp = %d\n", bp );
		int bpstart = bp;
		int class = PullBit( &bp );
		if( class < 0 )
		{
			//printf( "Class fail at %d\n", bp );
			return matchno;
		}
		//printf( "   Class %d @ %d\n", class, bp-1 );
		if( class == 0 )
		{
			// Note + Len
			int note = PullHuff( &bp, he );
			int lenandrun = PullHuff( &bp, hel );
			int cv = (note<<8) | lenandrun;
			//printf( "   CV %04x == %04x (matchno = %d)  (Values: %d %d)\n", cv, notes_to_match[matchno], matchno, note, lenandrun );
			if( cv != notes_to_match[matchno] || note < 0 || lenandrun < 0 )
			{
				//printf( "Breakout A %d != %d  (%d %d)\n", cv, notes_to_match[matchno], note, lenandrun );
				return matchno;
			}
			// Otherwise we're good!
			matchno++;
		}
		else
		{
			// Rewind
			if( (*depth)++ > MAX_BACK_DEPTH ) return matchno;
			int runlen = PullExpGolomb( &bp );
			int offset = PullExpGolomb( &bp );

			if( runlen < 0 || offset < 0 ) return matchno;
			//printf( "DEGOL %d %d BPIN: %d\n", runlen, offset, bp );
			runlen = runlen + MinRL + 1;
			offset = offset + OFFSET_MINIMUM + runlen;
			int bpjump = bpstart - offset;
			// Check for end of sequence or if bp points to something in the past.
			if( bpjump < 0 || bpjump >= bitcount-1 ) return matchno;
			//printf( "   OFFSET %d, %d (BP: %d)\n", runlen, offset, bp );
			int dmtm = length_max_to_match - matchno;
			if( dmtm > runlen ) dmtm = runlen;
			int dm = DecodeMatch( bpjump, notes_to_match + matchno, dmtm, depth );
			//printf( "DMCHECK %d %d @ BP = %d\n", dm, dmtm, bp );
			if( dm != dmtm )
			{
				//printf( "DM Disagree: %d != %d\n", dm, dmtm );
				return matchno + dm;
			}

			matchno += dm;
		}
	} while( matchno < length_max_to_match );
	//printf( "Match End: %d >= %d\n", matchno, length_max_to_match );
	return matchno;
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
		int searchStart = 0; //i - MaxREV - MaxRL - MinRL;
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
				ml < i && mlc < numNotes; //&& rl < MaxRL;
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


	int nHighestNote = 0;
	int nLowestNote = INT_MAX;
	// this is only a rough approximation of the distribution that will be used.
	for( int r = 0; r < numReg; r++ )
	{
		int nv = regNoteList[r];
		int pitch = nv>>8;
		if( pitch > nHighestNote ) nHighestNote = pitch;
		if( pitch < nLowestNote ) nLowestNote = pitch;
#ifdef SINGLETABLE
		int note = nv;
#else
		int note = nv>>8;
		int len = nv&0xff;
#endif
		if( note >= highestNoteCnt ) highestNoteCnt = note+1;
		numsym = HuffmanAppendHelper( &symbols, &symcounts, numsym, note );
#ifndef SINGLETABLE
		numlens = HuffmanAppendHelper( &lenss, &lencountss, numlens, len );
#endif
	}


	
	int hufflen;
	he = GenerateHuffmanTree( symbols, symcounts, numsym, &hufflen );

	int htlen = 0;
	hu = GenPairTable( he, &htlen );

#ifndef SINGLETABLE
	int hufflenl;
	hel = GenerateHuffmanTree( lenss, lencountss, numlens, &hufflenl );

	int htlenl = 0;
	hul = GenPairTable( hel, &htlenl );
#endif

	float principal_length_note = 0;
	float huffman_length_note = 0;
	float principal_length_rl = 0;
	float huffman_length_rl = 0;

	// Total notes = numReg
	printf( "NOTES:\n" );

	for( i = 0; i < htlen; i++ )
	{
		huffup * thu = hu + i;
		printf( "%3d: %04x :%5d : ", i, thu->value, thu->freq );

		for( k = 0; k < thu->bitlen; k++ )
			printf( "%c", thu->bitstream[k]+'0' );

		huffman_length_note += thu->freq * thu->bitlen;
		principal_length_note += thu->freq * -log( thu->freq / (float)numReg ) / log(2);

		printf( "\n" );
	}
#ifndef SINGLETABLE
	printf( "LENS:\n" );
	for( i = 0; i < htlenl; i++ )
	{
		huffup * thu = hul + i;
		printf( "%3d: %04x :%5d : ", i, thu->value, thu->freq );

		for( k = 0; k < thu->bitlen; k++ )
			printf( "%c", thu->bitstream[k]+'0' );

		huffman_length_rl += thu->freq * thu->bitlen;
		principal_length_rl += thu->freq * -log( thu->freq / (float)numReg ) / log(2);

		printf( "\n" );
	}
#endif

	printf( "Expected Huffman Length Note: %.0f bits\n", huffman_length_note );
	printf( "Principal Length Note: %.0f bits\n", principal_length_note );
	printf( "Expected Huffman Length RL: %.0f bits\n", huffman_length_rl );
	printf( "Principal Length RL: %.0f bits\n", principal_length_rl );

	printf( "Expected Huffman Length: %.0f bits / %.0f bytes\n", (huffman_length_note+huffman_length_rl),(huffman_length_note+huffman_length_rl)/8.0 );
	printf( "Principal Length: %.0f bits / %.0f bytes\n", (principal_length_note+principal_length_rl),(principal_length_note+principal_length_rl)/8.0 );


	FILE * fTN = fopen( "huffTN_fmraw.dat", "wb" );
	FILE * fTL = fopen( "huffTL_fmraw.dat", "wb" );
	fD = fopen( "huffD_fmraw.dat", "wb" );

	fData = fopen( "../playback/badapple_song_huffman_reverselzss.h", "wb" );

	fprintf( fData, "#ifndef ESPBADAPPLE_SONG_H\n" );
	fprintf( fData, "#define ESPBADAPPLE_SONG_H\n\n" );
	fprintf( fData, "#include <stdint.h>\n\n" );

	fprintf( fData, "#define ESPBADAPPLE_SONG_MINRL %d\n", MinRL );
	fprintf( fData, "#define ESPBADAPPLE_SONG_OFFSET_MINIMUM %d\n", OFFSET_MINIMUM );
	fprintf( fData, "#define ESPBADAPPLE_SONG_MAX_BACK_DEPTH %d\n", MAX_BACK_DEPTH );
	fprintf( fData, "#define ESPBADAPPLE_SONG_MAX_BACK_DEPTH %d\n", MAX_BACK_DEPTH );
	fprintf( fData, "#define ESPBADAPPLE_SONG_HIGHEST_NOTE %d\n", nHighestNote );
	fprintf( fData, "#define ESPBADAPPLE_SONG_LOWEST_NOTE %d\n", nLowestNote );
	fprintf( fData, "#define ESPBADAPPLE_SONG_LENGTH %d\n", numNotes );

	fprintf( fData, "\n" );
	fprintf( fData, "BAS_DECORATOR uint16_t espbadapple_song_huffnote[%d] = {\n\t", hufflen - numsym );
	int maxpdA = 0;
	int maxpdB = 0;
	int htnlen = 0;
	for( i = 0; i < hufflen; i++ )
	{
		huffelement * h = he + i;
		if( h->is_term )
		{
			//uint32_t sym = h->value;
			//fwrite( &sym, 1, 2, fTN );
			//fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
			//htnlen += 2;
		}
		else
		{
			int pd0 = h->pair0 - i - 1;
			int pd1 = h->pair1 - i - 1;

			huffelement * h0 = he + h->pair0;
			huffelement * h1 = he + h->pair1;

			if( pd0 < 0 || pd1 < 0 )
			{
				fprintf( stderr, "Error: Illegal pd\n" );
				return -5;
			}
			if( pd0 > maxpdA ) maxpdA = pd0;
			if( pd1 > maxpdB ) maxpdB = pd1;


			if( h0->is_term )
				pd0 = h0->value | 0x80;
			if( h1->is_term )
				pd1 = h1->value | 0x80;


			uint32_t sym = (pd0) | (pd1<<8);
			fwrite( &sym, 1, 2, fTN );
			fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
			htnlen += 2;
		}
	}
	fprintf( fData, "};\n\n" );

	printf( "max pd %d / %d\n", maxpdA, maxpdB );

	int htnlen2 = 0;

#ifndef SINGLETABLE
	fprintf( fData, "BAS_DECORATOR uint16_t espbadapple_song_hufflen[%d] = {\n\t", hufflenl - numlens );

	maxpdA = 0;
	maxpdB = 0;
	for( i = 0; i < hufflenl; i++ )
	{
		huffelement * h = hel + i;
		if( h->is_term )
		{
			//uint32_t sym = h->value;
			//fwrite( &sym, 1, 1, fTL );
			//htnlen2 += 1;
			//fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
		}
		else
		{
			int pd0 = h->pair0 - i - 1;
			int pd1 = h->pair1 - i - 1;

			huffelement * h0 = hel + h->pair0;
			huffelement * h1 = hel + h->pair1;

			if( pd0 < 0 || pd1 < 0 )
			{
				fprintf( stderr, "Error: Illegal pd\n" );
				return -5;
			}
			if( pd0 > maxpdA ) maxpdA = pd0;
			if( pd1 > maxpdB ) maxpdB = pd1;

			//printf( "%d %d  %02x %02x  %02x %02x\n", h0->is_term, h1->is_term, pd0, pd1, h0->value, h1->value );

			if( h0->is_term )
				pd0 = h0->value | 0x80;
			if( h1->is_term )
				pd1 = h1->value | 0x80;

			uint32_t sym = (pd0) | (pd1<<8);
			fwrite( &sym, 1, 2, fTL );
			htnlen2 += 2;
			fprintf( fData, "0x%04x%s", sym, ((i%12)!=11)?", " : ",\n\t" );
		}
	}

	fprintf( fData, "};\n\n" );
#endif

	fprintf( fData, "BAS_DECORATOR uint32_t espbadapple_song_data[] = {\n\t" );

	printf( "max pd %d / %d\n", maxpdA, maxpdB );

	printf( "Rev/Reg: %d %d\n", numRev, numReg );

	printf( "NOTES: %d\n", numNotes );

	for( i = 0; i < numNotes; i++ )
	{
		// Search for repeated sections.
		int searchStart = 0;//i - MaxREV - MaxRL - MinRL;
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
				ml < i && mlc < numNotes; // && rl < MaxRL;
				ml++, mlc++, rl++ )
			{
				if( completeNoteList[ml] != completeNoteList[mlc] ) break;
			}

			if( rl > bestrl )// && // Make sure it's best
				//s + MinRL + rl + MaxREV >= i )
			{
				bestrl = rl;
				bestrunstart = s;
			}
		}
		printf( "Byte: %d / MRL: %d\n", i, bestrl );
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



	int emit_bits_data = 0;
	int emit_bits_backtrack = 0;
	int emit_bits_class = 0;

	int actualReg = 0, actualRev = 0;
	for( i = 0; i < numNotes; i++ )
	{
		int bestrl = -1;
		int bests = -1;
		int s = 0;
		for( s = bitcount - 1; s >= 0; s-- )
		{
			int depth = 0;
			int dm = DecodeMatch( s, completeNoteList + i, numNotes - i, &depth );
			//printf( "Check [at byte %d]: %d -> %d -> %d\n", i, s, dm, depth );
			int sderate = (log(bitcount-s+1)/log(2)) / 9;
			if( dm - sderate > bestrl )
			{
				bestrl = dm;
				bests = s;
			}
		}
		if( bestrl > MinRL )
		{
			emit_bits_class++;
			int startplace = bitcount;
			printf( "OUTPUT   CB @ bp =%5d bestrl=%3d bests=%3d ", bitcount, bestrl, bests );
			EmitBit( 1 );
			i += bestrl - 1;
			int offset = startplace - bests - bestrl - OFFSET_MINIMUM;
			if( offset < 0 )
			{
				fprintf( stderr, "Error: OFFSET_MINIMUM is too large (%d - %d - %d - %d = %d)\n", startplace, bests, bestrl, OFFSET_MINIMUM, offset );
				exit ( -5 );
			}
			int emit_best_rl = bestrl - MinRL - 1;
			//printf( "WRITE %d %d\n", emit_best_rl, offset );
			printf( "Write: %d %d\n", emit_best_rl, offset );
			emit_bits_backtrack += EmitExpGolomb( emit_best_rl );
			emit_bits_backtrack += EmitExpGolomb( offset );
			actualRev++;
		}
		else
		{
			emit_bits_class++;
			printf( "OUTPUT DATA @ bp = %d (Values %02x %02x (index %d))\n", bitcount,  completeNoteList[i]>>8,  completeNoteList[i]&0xff, i );
			EmitBit( 0 );
#ifndef SINGLETABLE
			int n = completeNoteList[i] >> 8;
#else
			int n = completeNoteList[i];
#endif
			int bitcountatstart = bitcount;
			bitmaplocation[i] = bitcount;

			for( k = 0; k < htlen; k++ )
			{
				huffup * thu = hu + k;
				if( thu->value == n )
				{
					//printf( "Emitting NOTE %04x at %d\n", n, bitcount );
					int ll;
					emit_bits_data += thu->bitlen;
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
#ifndef SINGLETABLE
			int lev =  completeNoteList[i] & 0xff;
			for( k = 0; k < htlenl; k++ )
			{
				huffup * thul = hul + k;
				if( thul->value == lev )
				{
					int ll;
					//printf( "Emitting LEN %04x at %d\n", lev, bitcount );
					emit_bits_data += thul->bitlen;
					for( ll = 0; ll < thul->bitlen; ll++ )
					{
						EmitBit( thul->bitstream[ll] );
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
#endif
			actualReg ++;
		}
	}

	printf( "Actual Rev/Reg: %d/%d\n", actualRev, actualReg );
	printf( "Data Usage: %d bits / %d bytes\n", emit_bits_data, emit_bits_data/8 );
	printf( "Backtrack Usage: %d bits / %d bytes\n", emit_bits_backtrack, emit_bits_backtrack/8 );
#ifndef SINGLETABLE
	printf( "Class Usage: %d bits / %d bytes\n", emit_bits_class, emit_bits_class/8 );
#endif
	printf( "Total: %d bits / %d bytes\n", bitcount, (bitcount+7)/8  );

	if( runwordplace )
	{
		fwrite( &runword, 1, 4, fD );
		total_bytes+=4;
		fprintf( fData, "0x%08x%s", runword, ((total_bytes%8)!=7)?", " : ",\n\t" );
	}

	fprintf( fData, " };\n\n" );
	fprintf( fData, "#endif" );
	fclose( fData );
	printf( "Used mask: %04x\n", usedmask );
	printf( "Huff Tree (N): %d bytes\n", htnlen );
#ifndef SINGLETABLE
	printf( "Huff Tree (D): %d bytes\n", htnlen2 );
#endif
	printf( "Written Data: %d bytes\n", total_bytes );
	printf( "TOTAL: %d bytes\n", htnlen + htnlen2 + total_bytes );
	return 0;
}
