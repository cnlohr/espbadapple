#include <stdio.h>
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#include "bacommon.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

int streamcount;
uint32_t * streamdata;

int glyphct;
blocktype * glyphdata;

int main( int argc, char ** argv )
{
	if( argc != 4 )
	{
		fprintf( stderr, "Usage: streamcomp [stream] [tiles] [video]\n" );
		return 0;
	}

	FILE * f = fopen( argv[1], "rb" );
	fseek( f, 0, SEEK_END );
	streamcount = ftell( f ) / sizeof( streamdata[0] ) );
	streamdata = malloc( streamcount * sizeof( streamdata[0] ) );
	fread( streamdata, streamlen, sizeof( streamdata[0] ), f );
	fseek( f, 0, SEEK_SET ); 
	fclose( f );

	f = fopen( argv[2], "rb" );
	fseek( f, 0, SEEK_END );
	int streamlen = ftell( f );
	streamdata = malloc( streamlen );
	fread( streamdata, streamlen, 1, f );
	fseek( f, 0, SEEK_SET ); 

}


