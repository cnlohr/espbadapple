#include <stdint.h>
#include <stdio.h>

#define BAS_DECORATOR
#include "espbadapple_song.h"

#include "vpxcoding_tinyread.h"

typedef uint16_t notetype;

#define song_index_mask ((1<<bas_required_output_buffer_po2_bits)-1)
notetype decomp_buffer[1<<bas_required_output_buffer_po2_bits];
vpx_reader reader = { 0 };	
int nindex;
int ntail;

void SetupNotes()
{
	vpx_reader_init( &reader, bas_data, sizeof( bas_data ) );
}

notetype PullNote()
{
	// Originally: //!(( nindex ^ ntail ) & song_index_mask) ) but it was not needed.
	if( nindex == ntail )
	{
		// This will always read at least one note.
		if( vpx_read( &reader, bas_probability_of_peekback ) )
		{
			// Doing a peekback
			//printf( "Backlook: %d %d\n", maxrl, maxms );
			int runlen = 0;
			int offset = 0;
			int k;
			int slope = 0;
			for( k = 0; k < bas_bit_used_for_peekback_length; k++ )
			{
				int bprob = 128 + slope;
				slope += bas_peekback_slope_for_length;
				runlen |= vpx_read( &reader, bprob ) << k;
			}
			slope = 0;
			for( k = 0; k < bas_bit_used_for_peekback_offset; k++ )
			{
				int bprob = 128 + slope;
				slope += bas_peekback_slope_for_offset;
				offset |= vpx_read( &reader, bprob ) << k;
			}

			runlen += bas_min_run_length + 1;
			int endoffset = nindex + runlen;
			int startoffset = nindex - runlen - offset;
			if( startoffset < 0 || endoffset >= bas_songlen_notes )
			{
				fprintf( stderr, "Error: start/end %d %d/%d\n", startoffset, endoffset, bas_songlen_notes );
				return -5;
			}
			do
			{
				decomp_buffer[ nindex & song_index_mask ] = decomp_buffer[ startoffset & song_index_mask ];
				nindex++; startoffset++;
			} while( nindex != endoffset );
		}
		else
		{
			// Decoding a note
			int note = vpx_tree_read( &reader, bas_notes_probabilities, bas_note_probsize, bas_note_bits );
			int lenandrun = bas_lenandrun_codes[
				vpx_tree_read( &reader, bas_lenandrun_probabilities, bas_lenandrun_probsize, bas_lenandrun_bits )
			];
			decomp_buffer[(nindex++)&song_index_mask] = note | (lenandrun<<8);
		}
	}
	return decomp_buffer[ (ntail++) & song_index_mask ];
}

int main()
{
	SetupNotes();
	do
	{
		printf( "%04x %d\n", PullNote(), nindex );
	}
	while( nindex < bas_songlen_notes );
	printf( "%d/%d\n",nindex, bas_songlen_notes ); 
	return 0;
}


