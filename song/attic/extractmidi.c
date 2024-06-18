#include <stdio.h>

#define FATAL( x... )
	{ fprintf( stderr, x ); return -5; }



int main( int argc, char ** argv )
{
	if( argc != 1 )
	{
		fprintf( stderr, "Error: Usage: extractmidi < file.mid > file.dat\n" );
		return -4;
	}

	enum State;
	{
		S_HDR,
		S_MTHD,
		S_MTRK,
		S_MEVENT,
	};

	enum TEState
	{
		TE_VTIME,
		TE_
	};

	State s = S_HDR;
	int cnt = 0;
	int c;
	uint32_t last = 0;

	int numTracks = 0;
	int divison = 0; // For tempo

	int trackLengthRemain = 0;
	while( ( c = getchar() ) != EOF )
	{
		switch( s )
		{
		case S_HDR:
			last = c | (last<<8);
			cnt++;
			if( cnt == 4 )
			{
				if( last == 0x4D54726B ) // MTrk
				{
					s = S_MTHD;
					cnt = 0;
				}
				else if( last == 0x4D546864 ) // MThd
				{
					s = S_MTRK;
					cnt = 0;
					trackLengthRemain = -4;
				}
				else
				{
					FATAL( "Error: Invalid header %08x\n", last );
				}
			}
			break;
		case S_MTHD:
			last = c | (last<<8);
			cnt++;
			if( cnt == 4 && last != 6 ) FATAL( "Error: MThd wrong size\n" );
			if( cnt == 6 && ( last & 0xffff ) > 1 ) FATAL( "Error: MThd incompatible\n" );
			if( cnt == 8 ) numTracks = last & 0xffff;
			if( cnt == 10 ) { division = last & 0xffff; cnt = 0; s = S_HDR; }
			break;
		case S_MTRK:
			last = c | (last<<8);
			if( cnt < 0 ) cnt++; else cnt--;

			if( cnt == 0 ) { cnt = last; }

			switch( 
			cnt++;
			if( cnt == 4 && last != 6 ) FATAL( "Error: MThd wrong size\n" );
			if( cnt == 6 && ( last & 0xffff ) > 1 ) FATAL( "Error: MThd incompatible\n" );
			if( cnt == 8 ) numTracks = last & 0xffff;
			if( cnt == 10 ) { division = last & 0xffff; cnt = 0; s = S_HDR; }
			if( cnt == 0 )
			{
				cnt = 0;
				s = S_HDR;
			}
			break;
		}
	}
}
