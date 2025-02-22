#ifndef _BA_PLAY_AUDIO_H
#define _BA_PLAY_AUDIO_H

#include <stdint.h>

#define BAS_DECORATOR
#include "badapple_song.h"
#include "vpxcoding_tinyread.h"


#define NUM_VOICES 4
#define TIMESCALE ((F_SPS*60)/(138*4)) // 138 BPM

#define AUDIO_BUFFER_SIZE 1024


typedef uint16_t notetype;

#define song_index_mask ((1<<bas_required_output_buffer_po2_bits)-1)

#ifndef WARNING
#define WARNING( x... ) fprintf( stderr, x );
#endif

struct ba_audio_player_t
{
	reader reader = { 0 };	
	int nindex; // For reading a note
	int ntail;  // For having read a note
	//uint8_t voices[NUM_VOICES] = { 0 };
	//uint16_t tstop[NUM_VOICES] = { 0 };
	//uint32_t player->phase[NUM_VOICES] = { 0 };
	notetype   playing_notes[NUM_VOICES];
	int        tstop[NUM_VOICES];
	int nexttrel;
	uint8_t ending;
	uint8_t playing; // not ended.
	int t;
	int sub_t_sample;

	// Put at end because we only need to reference it once.
	notetype ba_audio_decomp_buffer[1<<bas_required_output_buffer_po2_bits];
} ba_player;

static notetype ba_audio_pull_note( struct ba_audio_player_t * player );

static void ba_audio_setup()
{
	struct ba_audio_player_t * player = &ba_player;
	memset( player, 0, sizeof( *player ) );
	vpx_reader_init( &player->reader, bas_data, sizeof( bas_data ) );
	player->playing = 1;
}

static int ba_audio_pull_note( struct ba_audio_player_t * player )
{
	// Originally: //!(( player->nindex ^ player->ntail ) & song_index_mask) ) but it was not needed.
	if( player->nindex == player->ntail )
	{
		// This will always read at least one note.
		if( vpx_read( &player->reader, bas_probability_of_peekback ) )
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
				runlen |= vpx_read( &player->reader, bprob ) << k;
			}
			slope = 0;
			for( k = 0; k < bas_bit_used_for_peekback_offset; k++ )
			{
				int bprob = 128 + slope;
				slope += bas_peekback_slope_for_offset;
				offset |= vpx_read( &player->reader, bprob ) << k;
			}

			runlen += bas_min_run_length + 1;
			int nindex = player->nindex;
			int endoffset = nindex + runlen;
			int startoffset = nindex - runlen - offset;
			if( startoffset < 0 || endoffset >= bas_songlen_notes )
			{
				WARNING( "Error: start/end %d %d/%d\n", startoffset, endoffset, bas_songlen_notes );
				return -5;
			}
			do
			{
				ba_audio_decomp_buffer[ nindex & song_index_mask ] = ba_audio_decomp_buffer[ startoffset & song_index_mask ];
				nindex++; startoffset++;
			} while( player->nindex != endoffset );

			player->nindex = nindex;
		}
		else
		{
			// Decoding a note
			int note = vpx_tree_read( &player->reader, bas_notes_probabilities, bas_note_probsize, bas_note_bits );
			int lenandrun = bas_lenandrun_codes[
				vpx_tree_read( &player->reader, bas_lenandrun_probabilities, bas_lenandrun_probsize, bas_lenandrun_bits )
			];
			ba_audio_decomp_buffer[(player->nindex++)&song_index_mask] = note | (lenandrun<<8);
		}
	}
	return ba_audio_decomp_buffer[ (player->ntail++) & song_index_mask ];
}

static inline void get_next_note( struct ba_audio_player_t * player )
{
	int i;
	for( i = 0; i < NUM_VOICES; i++ )
	{
		if( player->playing_notes[i] && player->tstop[i] <= player->t )
		{
			player->playing_notes[i] = 0;
		}
	}

	while( player->t >= player->nexttrel && !player->ending )
	{
		int note = ba_audio_pull_note( player );
		if( note < 0 )
		{
			player->ending = 1;
			break;
		}
		else
		{
			for( i = 0; i < NUM_VOICES; i++ )
			{
				if( player->voices[i] == 0 ) break;
			}
			if( i == NUM_VOICES )
			{
				WARNING( "WARNING: At time %d, too many voices\n", t );
			}
			else
			{
				player->playing_notes[i] = next_note;
				player->tstop[i] = ((next_note >> 3) & 0x1f) + t + 1;
				player->phase[i] = 0; // TODO: Do we want to randomize this to get some variety?
				int endurement = ((next_note) & 0x7);
				if( endurement == 7 ) endurement = 8; // XXX Special case scenario at ending.
				player->nexttrel = player->t + endurement;
				//fprintf( stderr, "NEXT: LEN %d -> %d -> %d (%04x)\n", endurement, t, nexttrel, next_note );
			}
		}
	}
}

static int ba_audio_fill_buffer( int8_t * outbuffer, int outbufferhead, int outbuffertail )
{
	struct ba_audio_player_t * player = &ba_player;

	if( outbufferhead == outbuffertail ) return ba_audio_ended;

	while( outbufferhead != outbuffertail )
	{
		int i;

		// By default, 5095 (at 46875SPS) or 5217 at 48000 SPS
		if( player->sub_t_sample++ >= TIMESCALE )
		{
			player->sub_t_sample = 0;
			player->t++;
			update_notes( player );
		}

		for( subsamp = 0; subsamp < TIMESCALE; subsamp++ )
		{
			float sample = 0;
			int i;
			for( i = 0; i < NUM_VOICES; i++ )
			{
				notetype pn = player->playing_notes[i];
				if( pn == 0 ) continue;

				int pitch = ((pn >> 8 ) & 0xff);

				if( !pitch ) continue;
//				float f = pow( 2, ((v & 0x7f) - 69)/12.0 ) * 440;

				{
					float placeins = fsin[i] / 3.1415926535 / 2;
asdfasdfasfd
					int pin = placeins;
					placeins -= pin;
					placeins *= 2.0;

					int ofs = 2.0;
					if( (v & 0x80) == 0 || 1 )
					{
						if( placeins > 1.0 ) placeins = 2.0 - placeins;
						placeins = placeins * 2.0 - 1.0;
						sample += placeins*0.2;
					}
					else
					{
						// A Tiny bit harher.  (Currently off)
						if( placeins > 1.0 ) placeins = 2.0 - placeins;
						placeins = placeins * 2.0 - 1.0;
						placeins *= 0.5;
						if( placeins > 0.0 ) placeins += 0.1; else placeins -= 0.1;
						sample += placeins*0.4;
					}
				}

				fsin[i] += f / F_SPS * 3.1415926 * 2.0;
			}
			//fprintf( stderr, "%f\n", sample );
			fwrite( &sample, 1, 4, stdout );
//			fflush( stdout );
		}

	}
}

#endif

