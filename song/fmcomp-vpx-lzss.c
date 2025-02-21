#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define VPXCODING_READER
#define VPXCODING_WRITER
#include "vpxcoding.h"

#include "vpxtree.h"

// Allow referencing things in the past.
// Kind of arbitrary tuning.
const int MinRL = 2;
const int MRBits = 10;
const int RLBits = 8;
const int MaxREV = (1<<MRBits)-1;
const int MaxRL = (1<<RLBits)-1;
// Tune the slopes for maximum compression
// I.e. for each message we start at 50/50 chance, but as we move up in bit significant, we reduce the chance it could be a 1.
const int rl_slope = (128/(RLBits-1));
const int mr_slope = (128/(MRBits-1));

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
	//fprintf( stderr, "Error: Requesting unknown value (%d)\n", value );
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

void WriteOutBuffer( FILE * fData, const char * name, uint8_t * data, int len )
{
	fprintf( fData, "BAS_DECORATOR const uint8_t %s[%d] = {", name, len );
	int i;
	for( i = 0; i < len; i++ )
	{
		if( (i & 0xf) == 0 )
		{
			fprintf( fData, "\n\t" );
		}
		fprintf( fData, "0x%02x, ", data[i] );
	}
	fprintf( fData, "};\n\n" );
}

int main()
{

	int i, j, k, l;
	FILE * f = fopen( "fmraw.dat", "rb" );

	struct DataTree dtNotes = { 0 };
	struct DataTree dtLenAndRun = { 0 };

	int numNotes = 0;

	uint32_t * completeNoteList = 0;

	FILE * fNoteList = fopen( "noteList.csv", "w" );

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

		fprintf( fNoteList, "%d,%d,%d,0x%04x\n", note, len, run, s );

		len |= run<<4;

		completeNoteList = realloc( completeNoteList, sizeof(completeNoteList[0]) * (numNotes+1) );
		completeNoteList[numNotes] = (note | (len<<8));
		numNotes++;
	}

	for( i = 0; i < numNotes; i++ )
	{
		uint32_t nn = completeNoteList[i];
		uint32_t nnnext = completeNoteList[i + 1];

		int note = nn&0xff;
		int len = (nn>>8)&0xf;
		int run = (nn>>12)&0xf;

		// Could do something here.
	}

	fclose( fNoteList );

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
			i += bestrl;
			numRev++;
		}
		else
		{
			// No readback found, we will have toa ctaully emit encode this note.
			int note = completeNoteList[i]&0xff;
			int len = completeNoteList[i]>>8;
			AddValue( &dtNotes, note );
			AddValue( &dtLenAndRun, len );
			if( note >= highestNoteCnt ) highestNoteCnt = note+1;
			numReg++;
		}
	}

	int bitsForNotes = VPXTreeBitsForMaxElement( highestNoteCnt );
	int bitsForLenAndRun = VPXTreeBitsForMaxElement( dtLenAndRun.numUnique );

	int treeSizeNotes = VPXTreeGetSize( highestNoteCnt, bitsForNotes );
	int treeSizeLenAndRun = VPXTreeGetSize( dtLenAndRun.numUnique, bitsForLenAndRun );

	uint8_t probabilitiesNotes[treeSizeNotes];
	uint8_t probabilitiesLenAndRun[treeSizeLenAndRun];

	{
		float fNoteFrequencies[highestNoteCnt];
		int n;

		for( n = 0; n < highestNoteCnt; n++ )
		{
			int idx = GetIndexFromValue( &dtNotes, n );
			if( idx < 0 )
				fNoteFrequencies[n] = 0;
			else
				fNoteFrequencies[n] = dtNotes.uniqueCount[idx];
		}

		VPXTreeGenerateProbabilities( probabilitiesNotes, treeSizeNotes, fNoteFrequencies, highestNoteCnt, bitsForNotes );
	}

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
		printf( "%2d %02x %f\n", i, dtLenAndRun.uniqueMap[i], dtLenAndRun.uniqueCount[i] );
	}

	for( i = 0; i < treeSizeLenAndRun; i++ )
	{
		printf( "%d ", probabilitiesLenAndRun[i] );
	}
	printf( "\n" );

	printf( "Theoretical:\n" );

	float fBitsNotes = 0;
	float fBitsLens = 0;

	for( i = 0; i < dtNotes.numUnique; i++ )
	{
		float fPortion = (dtNotes.uniqueCount[i] / numReg);
		if( dtNotes.uniqueCount[i] == 0 ) continue;
		float fBitContrib = -log(fPortion)/log(2.0);
		fBitsNotes += fBitContrib * dtNotes.uniqueCount[i];
	}

	for( i = 0; i < dtLenAndRun.numUnique; i++ )
	{
		float fPortion = (dtLenAndRun.uniqueCount[i] / numReg);
		if( dtLenAndRun.uniqueCount[i] == 0 ) continue;
		float fBitContrib = -log(fPortion)/log(2.0);
		fBitsLens += fBitContrib * dtLenAndRun.uniqueCount[i];
	}

	float options[2] = { numReg, numRev };
	float fBitsFromSwitch = 0;
	for( i = 0; i < 2; i++ )
	{
		float fPortion = (options[i] / (numReg+numRev));
		float fBitContrib = -log(fPortion)/log(2.0);
		fBitsFromSwitch += fBitContrib * options[i];
	}

	float fBitsFromRev = numRev * ( MRBits + RLBits );

	printf( "Notes: %.1f bits (%.1f bits per note)\n", fBitsNotes, fBitsNotes/numReg );
	printf( " Lens: %.1f bits (%.1f bits per note)\n", fBitsLens, fBitsLens/numReg );
	printf( "Switc: %.1f bits\n", fBitsFromSwitch );
	printf( " RevL: %.1f bits\n", fBitsFromRev );
	printf( "Total: %.1f bits (%.1f bytes)\n", fBitsLens + fBitsNotes + fBitsFromSwitch + fBitsFromRev, (fBitsLens + fBitsNotes + fBitsFromSwitch + fBitsFromRev)/8.0 );

	int probability_of_reverse = ( 256 * numReg ) / (numReg + numRev + 1);
	if( probability_of_reverse > 255 ) probability_of_reverse = 255;
	if( probability_of_reverse < 0 ) probability_of_reverse = 0;
	printf( "Reverse Probability: %d\n", probability_of_reverse );

	int deepestReverseSearch = 0;

	for( i = 0; i < numNotes; )
	{

		int note = completeNoteList[i]&0xff;
		int len = completeNoteList[i]>>8;

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
			//printf( "AT: %d BEST: LENG:%d  START:%d\n", i, bestrl, bestrunstart );
			int offset = i-bestrunstart-bestrl;
			//printf( "Emitting: %d %d (%d %d %d %d)\n", bestrl, offset, i, bestrunstart, bestrl, MinRL );

			int searchBack = i-bestrunstart; 
			if( searchBack >= deepestReverseSearch )
				deepestReverseSearch = searchBack-1; // Need one extra so that the decoder doesn't get confused and drop a block of exactly max size.

			// Need to make sure we allocate enough for the run. (I don't think this is needed, and should be covered by the above case)
			if( bestrl >= deepestReverseSearch )
				deepestReverseSearch = bestrl-1;

			int emit_best_rl = bestrl - MinRL - 1;

			vpx_write( &writer, 1, probability_of_reverse );

			//printf( "Backlook: %d %d\n", maxrl, maxms );
			//printf( "Reversing RUN:%4d OFFSET:%d\n", emit_best_rl, offset );
			int k;
			for( k = 0; k < RLBits; k++ )
			{
				int bprob = 128 + k*rl_slope;
				vpx_write( &writer, !!(emit_best_rl&1), bprob );
				emit_best_rl>>=1;
			}
			if( emit_best_rl ) fprintf( stderr, "ERROR: Invalid RL Emitted %d remain\n", emit_best_rl );

			for( k = 0; k < MRBits; k++ )
			{
				int bprob = 128 + k*mr_slope;
				vpx_write( &writer, !!(offset&1), bprob );
				offset>>=1;
			}
			if( offset ) fprintf( stderr, "ERROR: Invalid offset Emitted %d remain\n", offset );

			i += bestrl;
		}
		else
		{
			// We could do a (i<=MinRL)?0xff:, but the code to detect that is larger than fraction of a bit you would save.
			vpx_write( &writer, 0, probability_of_reverse );

			//printf( "EMIT: %02x %02x\n",  note, len );
			VPXTreeWriteSym( &writer, note,	probabilitiesNotes, treeSizeNotes, bitsForNotes );

			VPXTreeWriteSym( &writer, GetIndexFromValue( &dtLenAndRun, len ),
				probabilitiesLenAndRun, treeSizeLenAndRun, bitsForLenAndRun );
			i++;
		}
	}

	vpx_stop_encode( &writer );
	printf( "Reverse Peek: %d (reverses) / reg: %d (encoded notes)\n", numRev, numReg );
	printf( "Notes: %d\n", numNotes );
	uint32_t sum = writer.pos;
	printf( "Data: %d bytes\n", writer.pos );

	printf( "Notes: %d (Table Only)\n", treeSizeNotes );
	sum += treeSizeNotes;
//	sum += dtNotes.numUnique * 1;

	printf( "LenAndRun: %d + %d\n", treeSizeLenAndRun, dtLenAndRun.numUnique * 1 );
	sum += treeSizeLenAndRun;
	sum += dtLenAndRun.numUnique * 1;

	printf( "Total: %d\n", sum );
	//unsigned int pos;
	//unsigned int size;

	FILE * fData = fopen( "espbadapple_song.h", "wb" );
	fprintf( fData, "#ifndef _ESPBADAPPLE_SONG_H\n" );
	fprintf( fData, "#define _ESPBADAPPLE_SONG_H\n" );
	fprintf( fData, "\n" );
	fprintf( fData, "#define bas_required_output_buffer %d\n", deepestReverseSearch );
	fprintf( fData, "#define bas_required_output_buffer_po2_bits %d\n", VPXTreeBitsForMaxElement( deepestReverseSearch ) );
	fprintf( fData, "\n" );
	fprintf( fData, "const uint8_t bas_probability_of_peekback = %d;\n", probability_of_reverse );
	fprintf( fData, "const uint8_t bas_peekback_slope_for_length = %d;\n", rl_slope );
	fprintf( fData, "const uint8_t bas_peekback_slope_for_offset = %d;\n", mr_slope );
	fprintf( fData, "const uint8_t bas_bit_used_for_peekback_length = %d;\n", RLBits );
	fprintf( fData, "const uint8_t bas_bit_used_for_peekback_offset = %d;\n", MRBits );
	fprintf( fData, "\n" );
	fprintf( fData, "#define bas_min_run_length %d\n", MinRL );
	fprintf( fData, "#define bas_note_probsize %d\n", treeSizeNotes );
	fprintf( fData, "#define bas_note_bits %d\n", bitsForNotes );
	fprintf( fData, "#define bas_lenandrun_probsize %d\n", treeSizeLenAndRun );
	fprintf( fData, "#define bas_lenandrun_bits %d\n", bitsForLenAndRun );
	fprintf( fData, "#define bas_songlen_notes %d\n", numNotes );
	fprintf( fData, "\n" );

	WriteOutBuffer( fData, "bas_notes_probabilities", probabilitiesNotes, treeSizeNotes );
	WriteOutBuffer( fData, "bas_lenandrun_probabilities", probabilitiesLenAndRun, treeSizeLenAndRun );


	uint8_t codeinfos[dtLenAndRun.numUnique];
	for( i = 0; i < dtLenAndRun.numUnique; i++ )
		codeinfos[i] = dtLenAndRun.uniqueMap[i];
	WriteOutBuffer( fData, "bas_lenandrun_codes", codeinfos, dtLenAndRun.numUnique );

	WriteOutBuffer( fData, "bas_data", vpxbuffer, writer.pos );

	fprintf( fData, "\n" );
	fprintf( fData, "#endif\n" );



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
