#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

int place = 0;

#define F_SPS 48000

#define TIMESCALE 44

#define MAX_NOTE_EVENTS 65536

#define FATAL( x... )	{ fprintf( stderr, x ); fprintf( stderr, "at byte %d\n", place ); exit( -5 ); }

int ungot = -1;

int read1()
{
	if( ungot != -1 )
	{
		int r = ungot;
		ungot = -1;
		return r;
	}

	int c = getchar();
	if( c == EOF )
		return -1;
	place++;
	return c;
}

void unget( uint8_t c )
{
	if( ungot != -1 ) FATAL( "Can't double unget\n" );
	ungot = c;
}

int read2()
{
	int r = read1();
	return (r << 8) | read1();
}

int read4()
{
	int r = read2();
	return (r << 16) | read2();
}


int readvar()
{
	int c = read1();
	if( c & 0x80 )
	{
		int c2 = read1();
		c = ((c&0x7f)<<7) | (c2 & 0x7f);
		if( c2 & 0x80 )
		{
			int c3 = read1();
			c = (c<<7) | (c3 & 0x7f);
			if( c3 & 0x80 )
			{
				FATAL( "readvar too deep\n" );
			}
		}
	}
	return c;
}


struct NoteEvent
{
	int OnTime;
	int OffTime;
	int Disable;
	int Track;
	int Note;
	int Velocity;
};

int NoteCompare( const void * a, const void *b )
{
	struct NoteEvent * na = (struct NoteEvent*)a;
	struct NoteEvent * nb = (struct NoteEvent*)b;
	if( na->OnTime < nb->OnTime )
		return -1;
	if( na->OnTime > nb->OnTime )
		return 1;

	if( na->Track < nb->Track )
		return -1;
	if( na->Track > nb->Track )
		return 1;

	if( na->Note < nb->Note )
		return -1;
	if( na->Note > nb->Note )
		return 1;

	if( na->OffTime < nb->OffTime )
		return -1;
	if( na->OffTime > nb->OffTime )
		return 1;
	return 0;
}

struct NoteEvent AllNoteEvents[MAX_NOTE_EVENTS];
int notehead = 0;

int main( int argc, char ** argv )
{
	if( argc != 1 )
	{
		fprintf( stderr, "Error: Usage: extractmidi < file.mid > file.dat\n" );
		return -4;
	}

	if( read4() != 0x4D546864 ) FATAL( "First block is not MThd\n" );
	if( read4() != 6 ) FATAL( "MThd malformatted\n" );
	if( read2() > 1 ) FATAL( "Invalid file type\n" );

	int numTracks = read2();
	int deltaTime = read2();

	int trk;
	int timenow = 0;
	int highesttime = 0;

	struct NoteEvent * lastNote[128] = { 0 };	

	for( trk = 0; trk < numTracks; trk++ )
	{
		if( read4() != 0x4D54726B ) FATAL( "Not a MTrk\n" );
		int len = read4();
		int trkend = place + len;
		timenow = 0;
		while( place != trkend )
		{
			int timed = readvar();
			//fprintf( stderr, "Timed: %5d:", timed );
			timenow += timed;
			if( timenow > highesttime ) highesttime = timenow;
			int eventtype = read1();
			if( eventtype == 0xff )
			{
				int mv = read1();
				int metalen = readvar();
				fprintf( stderr, "Meta: %d Len: %d\n\t", mv, metalen );
				int j;
				for( j = 0; j < metalen; j++ )
				{
					int mv = read1();
					fprintf( stderr, " %c %02x", (mv>=32&&mv<=127)?mv:'.', mv);
				}
				fprintf( stderr,"\n" );
			}
			else
			{
				//fprintf( stderr, "Message: %02x ", eventtype );
				int minor = eventtype & 0xf;
				int a, b;
				switch ( eventtype & 0xf0 )
				{
				default:
					FATAL( "Unknown message %d %02x\n", minor, eventtype );
				case 0x80:
					a = read1(); // channel
					b = read1(); // velocity
					
					struct NoteEvent * e = lastNote[a];
					if( !e ) fprintf( stderr, "Matching note not found\n" );
					//fprintf( stderr, "Note off %d %02x %02x\n", minor, a, b );
					e->OffTime = timenow;
					break;
				case 0x90:
				{
					a = read1(); // channel
					b = read1(); // velocity

					struct NoteEvent * e = &AllNoteEvents[notehead++];
					if( MAX_NOTE_EVENTS == notehead ) FATAL( "Too many notes\n" );
					e->OnTime = timenow;
					e->Track = trk;
					e->Note = a;
					e->Velocity = b;
					lastNote[a] = e;
					//fprintf( stderr, "Note on  %d %02x %02x\n", minor, a, b );
					break;
				}
				case 0xa0:
					fprintf( stderr, "Key pressure %d %02x %02x\n", minor, read1(), read1() );
					break;
				case 0xb0:
					fprintf( stderr, "Controller Change %d %02x %02x\n", minor, read1(), read1() );
					break;
				case 0xc0:
					fprintf( stderr, "Program Change %d %02x\n", minor, read1() );
					break;
				case 0xd0:
					fprintf( stderr, "Channel Key Pressure %d %02x\n", minor, read1() );
					break;
				case 0xe0:
					fprintf( stderr, "Pitch Bend %d %02x %02x\n", minor, read1(), read1() );
					break;
				}
			}
			if( place > trkend )
				FATAL( "Track overrun" );
		}
	}

	qsort( AllNoteEvents, notehead, sizeof( AllNoteEvents[0] ), NoteCompare );

	int i;
	for( i = 0; i < notehead; i++ )
	{
		struct NoteEvent * e = &AllNoteEvents[i];
		{
			if( e->OffTime == 0 )
			{
				e->OffTime = e->OnTime;
				e->Disable = 1;
				fprintf( stderr, "Warning: Note at %d has no off\n", e->OnTime );
			}
			if( e->Note < 1 )
				e->Disable = 1;
		}
		fprintf( stderr, "%5d %4d %d %3d %d\n",
			e->OnTime, e->OffTime - e->OnTime, e->Track, e->Note, e->Velocity );
	}

	#define NUM_VOICES 4
	struct NoteEvent * voices[NUM_VOICES] = { 0 };
	float fsin[NUM_VOICES] = { 0 };

	int notepl = 0;
	int t;
	int nextevent = 0;
	for( t = 0; t < highesttime; t++ )
	{
		struct NoteEvent * nn = &AllNoteEvents[nextevent];
		if( t >= nn->OnTime )
		{
			nextevent++;
			if( !nn->Disable && nextevent < notehead ) 
			{
				int i;
				for( i = 0; i < NUM_VOICES; i++ )
				{
					if( voices[i] == 0 ) break;
				}
				if( i == NUM_VOICES )
				{
					fprintf( stderr, "WARNING: At time %d, too many voices\n", i );
				}
				else
				{
					fprintf( stderr, "Adding %d (%d/%d) [%d]\n", nn->Note, t, highesttime, i );
					voices[i] = nn;
					fsin[i] = 0;
				}
			}
		}

		int subsamp = 0;
		for( subsamp = 0; subsamp < TIMESCALE; subsamp++ )
		{
			float sample = 0;
			int i;
			for( i = 0; i < NUM_VOICES; i++ )
			{
				struct NoteEvent * v = voices[i];
				if( !v ) continue;
				float f = pow( 2, (v->Note - 69)/12.0 ) * 440;

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
					if( v->Track == 0 )
					{
						if( placeins > 1.0 ) placeins = 2.0 - placeins;
						placeins = placeins * 2.0 - 1.0;
						sample += placeins*0.2;
					}
					else
					{
						// A Tiny bit harher.
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

		int i;
		for( i = 0; i < NUM_VOICES; i++ )
			if( voices[i] && t >= voices[i]->OffTime )
			{
				fprintf( stderr, "Removing %d [%d]\n", voices[i]->Note, i );
				voices[i] = 0;
			}
	}
	
	fprintf( stderr, "%d / %d\n", t, highesttime );

	return 0;
}
