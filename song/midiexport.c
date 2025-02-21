#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

int place = 0;

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

	int minnote = 255;
	int maxnote = 0;

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
					if( !e )
						fprintf( stderr, "Matching note not found\n" );
					else
						e->OffTime = timenow;
					//fprintf( stderr, "Note off %d %02x %02x\n", minor, a, b );
					lastNote[a] = 0;
					break;
				case 0x90:
				{
					a = read1(); // channel
					b = read1(); // velocity

					if( lastNote[a] )
					{
						fprintf( stderr, "Warning: Note at %d has no off\n", e->OnTime );
						lastNote[a]->OffTime = timenow;
					}


					struct NoteEvent * e = &AllNoteEvents[notehead++];
					if( MAX_NOTE_EVENTS == notehead ) FATAL( "Too many notes\n" );
					e->OnTime = timenow;
					e->Track = trk;
					e->Note = a;

					if( e->Note < minnote ) minnote = e->Note;
					if( e->Note > maxnote ) maxnote = e->Note;

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

	FILE * fMRaw = fopen( "fmraw.dat", "wb" );
	FILE * fMXML = fopen( "fmraw.dat.xml", "w" );
	FILE * fMJSON = fopen( "fmraw.dat.json", "w" );

	fprintf( fMXML, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
	fprintf( fMXML, "<song>\n" );
	fprintf( fMJSON, "{\"notes\":[" );

	int eLastTime = 0;
	for( i = 0; i < notehead; i++ )
	{
		struct NoteEvent * e = &AllNoteEvents[i];
		{
			if( e->OffTime == 0 )
			{
				e->OffTime = e->OnTime;
				e->Disable = 1;
			}
			if( e->Note < 1 )
				e->Disable = 1;
		}

		if( !e->Disable )
		{
			uint8_t note_and_track = e->Note - minnote; // | (e->Track<<7);  XXX TODO: IF we want track data, add this back.
			uint16_t duration = (e->OffTime - e->OnTime + 1);
			uint16_t deltatime = (e->OnTime - eLastTime + 1);

			if( duration/120-1 > 31 )
			{
				fprintf( stderr, "Error: Warning: Note Too Long (at %d/%d/%d)\n", e->OnTime, i, notehead );
				duration = 120*12;
			}
			if( deltatime/120 == 7 )
			{
				fprintf( stderr, "Error: Warning: Interval Is exactly 7; special case, not allowed.\n" );
				deltatime = 7*120;
			}
			if( deltatime/120 == 8 )
			{
				deltatime = 7*120;
			}
			if( deltatime/120 > 8 )
			{
				fprintf( stderr, "Error: Warning: Interval Too Long (at %d/%d/%d) (%d)\n", e->OnTime, i, notehead, deltatime );
				deltatime = 7*120;
			}
			uint8_t combda = (((duration/120)-1)<<3) + (deltatime/120); 

//			fprintf( stderr, "%6d / %4d %4d %d %3d %d  -> %4d %4d -> %d %d %02x\n",
	//			e->OnTime,  e->OnTime - eLastTime, e->OffTime - e->OnTime + 1, e->Track, e->Note, e->Velocity, duration, deltatime, duration/120-1, deltatime/120, combda );

			// For video, show how big it is if you gzip with all the data, it's bigger than heatshrink after remvoing the entropy
			fwrite( &combda, 1, 1, fMRaw );
			fwrite( &note_and_track, 1, 1, fMRaw );

			fprintf( fMXML, "    <note>\n" );
			fprintf( fMXML, "        <pitch>%d</pitch>\n", e->Note );
			fprintf( fMXML, "        <ontime>%d</ontime>\n", e->OnTime );
			fprintf( fMXML, "        <offtime>%d</offtime>\n", e->OffTime );
			fprintf( fMXML, "    </note>\n" );
			fprintf( fMJSON, "{\"pitch\":%d,\"ontime\":%d,\"offtime\":%d}%c", e->Note, e->OnTime, e->OffTime, (notehead-1 == i)?' ' : ',' );

			eLastTime = e->OnTime;
		}
	}
	fprintf( fMXML, "</song>\n" );
	fclose( fMRaw );
	fclose( fMXML );
	fprintf( fMJSON, "]}\n" );
	fclose( fMJSON );


	fprintf( stderr, "Time Range: %d / %d\n", i, highesttime );
	fprintf( stderr, "Note Range: %d / %d\n", minnote, maxnote );

	return 0;
}
