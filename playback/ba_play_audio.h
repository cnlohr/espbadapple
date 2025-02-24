#ifndef _BA_PLAY_AUDIO_H
#define _BA_PLAY_AUDIO_H

#include <stdint.h>
#include <math.h>

#ifndef BAS_DECORATOR
#define BAS_DECORATOR
#endif
#include "badapple_song.h"
#include "vpxcoding_tinyread.h"

#ifndef F_SPS
#define F_SPS 46875
#endif

#define NUM_VOICES 4
#define TIMESCALE ((F_SPS*60)/(138*4)) // 138 BPM

// MUST BE POWER-OF-TWO
#ifndef AUDIO_BUFFER_SIZE
#define AUDIO_BUFFER_SIZE 2048
#endif

// Note 0 in MIDI is -69 from A 440.  We are offset at 47, because the lowest note in our stream is note 47.
#define SYFN( n ) \
	(uint16_t)((pow( 2, (((float)n + 47.0) - 69.0)/12.0 ) * 440.0 * 65536.0 * 2.0 / 2.0 / (float)F_SPS) + 0.5)

const BAS_DECORATOR uint16_t frequencies[bas_highest_note_count] = {
	SYFN( 0), SYFN( 1), SYFN( 2), SYFN( 3), SYFN( 4), SYFN( 5), SYFN( 6), SYFN( 7), SYFN( 8), SYFN( 9),
	SYFN(10), SYFN(11), SYFN(12), SYFN(13), SYFN(14), SYFN(15), SYFN(16), SYFN(17), SYFN(18), SYFN(19),
	SYFN(20), SYFN(21), SYFN(22), SYFN(23), SYFN(24), SYFN(25), SYFN(26), SYFN(27), SYFN(28), SYFN(29),
	SYFN(30), SYFN(31), SYFN(32) };

typedef uint16_t notetype;

#define song_index_mask ((1<<bas_required_output_buffer_po2_bits)-1)

#ifndef WARNING
#define WARNING( x... ) fprintf( stderr, x );
#endif

struct ba_audio_player_t
{
	vpx_reader reader;	
	int nindex; // For reading a note
	int ntail;  // For having read a note
	uint16_t   playing_freq[NUM_VOICES];
	uint16_t   phase[NUM_VOICES];
	int        tstop[NUM_VOICES];
	int nexttrel;
	int ending;
	int t;
	int gotnotes;
	int sub_t_sample;
	int outbufferhead;

	// Put at end because we only need to reference it once.
	notetype decomp_buffer[1<<bas_required_output_buffer_po2_bits];
} ba_player;

static int ba_audio_pull_note( struct ba_audio_player_t * player );

static void ba_audio_setup()
{
	struct ba_audio_player_t * player = &ba_player;
	memset( player, 0, sizeof( *player ) );
	vpx_reader_init( &player->reader, bas_data, sizeof( bas_data ) );
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
				int bprob = bas_peekback_base_for_length + slope;
				slope += bas_peekback_slope_for_length;
				runlen |= vpx_read( &player->reader, bprob ) << k;
			}
			slope = 0;
			for( k = 0; k < bas_bit_used_for_peekback_offset; k++ )
			{
				int bprob = bas_peekback_base_for_offset + slope;
				slope += bas_peekback_slope_for_offset;
				offset |= vpx_read( &player->reader, bprob ) << k;
			}

			runlen += bas_min_run_length + 1;
			int nindex = player->nindex;
			int endoffset = nindex + runlen;
			int startoffset = nindex - runlen - offset;
			player->gotnotes += runlen;
			if( startoffset < 0 || endoffset >= bas_songlen_notes )
			{
				WARNING( "Error: start/end %d %d/%d\n", startoffset, endoffset, bas_songlen_notes );
				return -5;
			}
			do
			{
				int n = player->decomp_buffer[ nindex & song_index_mask ] = player->decomp_buffer[ startoffset & song_index_mask ];
				nindex++; startoffset++;
				//printf( "%d %d %d\n", startoffset, player
			} while( nindex != endoffset );
			player->nindex = nindex;
		}
		else
		{
			// Decoding a note
			int note = vpx_tree_read( &player->reader, bas_notes_probabilities, bas_note_probsize, bas_note_bits );
			int coded_len_and_run = vpx_tree_read( &player->reader, bas_lenandrun_probabilities, bas_lenandrun_probsize, bas_lenandrun_bits );
			int lenandrun = bas_lenandrun_codes[coded_len_and_run];
			player->gotnotes++;
			int n = player->decomp_buffer[(player->nindex++)&song_index_mask] = lenandrun | (note<<8);
		}
	}
	if( player->gotnotes >= bas_songlen_notes )
		player->ending = 1;
	return player->decomp_buffer[ (player->ntail++) & song_index_mask ];
}

static inline void perform_16th_note( struct ba_audio_player_t * player )
{
	int i;
	for( i = 0; i < NUM_VOICES; i++ )
	{
		if( player->playing_freq[i] && player->tstop[i] <= player->t )
		{
			player->playing_freq[i] = 0;
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
				if( player->playing_freq[i] == 0 ) break;
			}

			if( i == NUM_VOICES )
			{
				WARNING( "WARNING: At time %d, too many voices\n", player->t );
			}
			else
			{
				player->playing_freq[i] = frequencies[note >> 8];
				player->tstop[i] = ((note >> 3) & 0x1f) + player->t + 1;
				//player->phase[i] = 16384; // TODO: Do we want to randomize this to get some variety?  For now, let's try it in the center?  We could randomize by just not setting it.  TODO Try this.
				int endurement = ((note) & 0x7);
				if( endurement == 7 ) { endurement = 8; } // XXX Special case scenario at ending.
				//printf( "%d %d %d STOP: %d  ENDURE: %d\n", ((note >> 3) & 0x1f), note & 7, note>>8, player->tstop[i], endurement );
				player->nexttrel = player->t + endurement;
				//printf( "%d\n", player->nexttrel );
				//fprintf( stderr, "NEXT: LEN %d -> %d -> %d (%04x)\n", endurement, t, nexttrel, next_note );
			}
		}
	}
}

static int ba_audio_fill_buffer( int8_t * outbuffer, int outbuffertail )
{
	int i;
	struct ba_audio_player_t * player = &ba_player;

	int outbufferhead = player->outbufferhead;
	if( outbufferhead == outbuffertail ) return 1;

	while( outbufferhead != outbuffertail )
	{
		// By default, 5095 (at 46875 SPS) or 5217 at 48000 SPS
		if( player->sub_t_sample-- < 0 )
		{
			player->t++;
			perform_16th_note( player );
			player->sub_t_sample += TIMESCALE;
		}
		
		int sample = 0;
		for( i = 0; i < NUM_VOICES; i++ )
		{
			int pn = player->playing_freq[i];
			if( pn == 0 )
			{
				sample += 16384;
			}
			else
			{
				int pitch = pn;
				int phase = player->phase[i];
				phase += pn;
				player->phase[i] = phase;
				int ts = phase;
				if( ts >= 32768 ) ts = 65536-ts;
				sample += ts;
			}
		}

		outbuffer[outbufferhead] = sample >> (1+8);
		outbufferhead = ( outbufferhead + 1 ) & ( AUDIO_BUFFER_SIZE - 1);
	}
	player->outbufferhead = outbufferhead;
	return 0;
}

#endif

