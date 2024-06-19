#include <stdio.h>
#include <math.h>
#include <stdint.h>

#define MAXNOTES 65536
#define F_SPS 48000
#define TIMESCALE ((F_SPS*60)/(138*2))
#define NUM_VOICES 3

// XXX TODO: Can we remove track # from the note #?

int main()
{
	FILE * fMRaw = fopen( "fmraw.dat", "rb" );
	fprintf( stderr, "Opening: fmraw.dat (%d)\n", !!fMRaw );
	uint8_t voices[NUM_VOICES] = { 0 };
	uint16_t tstop[NUM_VOICES] = { 0 };

	float fsin[NUM_VOICES] = { 0 };

	int notepl = 0;
	int t;
	int nextevent = 0;


	uint16_t next_note;
	if( fread( &next_note, 2, 1, fMRaw ) != 1 )
	{
		fprintf( stderr, "Error opening file\n" );
		return -5;
	}

	int tend = 1<<30;
	int nexttrel = 0;
	int trun = 0;
	int playout = 0;
	for( t = 0; t < tend; t++ )
	{
		fprintf( stderr, "T: %d\n", t );

		int i;
		for( i = 0; i < NUM_VOICES; i++ )
		{
			if( voices[i] && tstop[i] <= t )
			{
				fprintf( stderr, "Removing [%d]\n", i );
				voices[i] = 0;
			}
		}

		while( t >= nexttrel )
		{

			int i;
			for( i = 0; i < NUM_VOICES; i++ )
			{
				if( voices[i] == 0 ) break;
			}
			if( i == NUM_VOICES )
			{
				fprintf( stderr, "WARNING: At time %d, too many voices\n", t );
			}
			else
			{
				voices[i] = (next_note & 0xff) + 47;
				tstop[i] = ((next_note >> 11) & 0x1f) + t + 1;
				fprintf( stderr, "Adding [%04x] (%d/%d) [%d]\n", next_note, t, tstop[i], i );
				fsin[i] = 0;
			}

			if( fread( &next_note, 2, 1, fMRaw ) != 1 )
			{
				tend = t + 8;
				nexttrel = 10000;
			}
			else
			{
				nexttrel = t + ((next_note >> 8) & 0x7);
				fprintf( stderr, "NEXT: %d -> %d (%04x)\n", t, nexttrel, next_note );
			}
		}

		fprintf( stderr, "%2d %2d %2d %2d\n", voices[0], voices[1], voices[2], voices[3] );


		int subsamp = 0;
		for( subsamp = 0; subsamp < TIMESCALE; subsamp++ )
		{
			float sample = 0;
			int i;
			for( i = 0; i < NUM_VOICES; i++ )
			{
				uint16_t v = voices[i];
				if( !v ) continue;
				float f = pow( 2, ((v & 0x7f) - 69)/12.0 ) * 440;

				// Detune based on track.
				//f += (i)-1;

//				if( v->Track == 0 )
//				{
//					sample += sin(fsin[i])*0.2;
//				}
//				else
				{
					float placeins = fsin[i] / 3.1415926535 / 2;
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

		fflush( stdout );

	}
	
}

