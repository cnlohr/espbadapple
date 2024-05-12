#include <stdio.h>
#include "bacommon.h"
#include "hufftreegen.h"

int streamcount;
uint32_t * streamdata;

int glyphct;
blocktype * glyphdata;

int video_w;
int video_h;
int num_video_frames;


int main( int argc, char ** argv )
{
	if( argc != 6 )
	{
		fprintf( stderr, "Usage: streamcomp [stream] [tiles] [video] [w] [h]\n" );
		return 0;
	}

	video_w = atoi( argv[4] );
	video_h = atoi( argv[5] );

	FILE * f = fopen( argv[1], "rb" );
	fseek( f, 0, SEEK_END );
	streamcount = ftell( f ) / sizeof( streamdata[0] );
	streamdata = malloc( streamcount * sizeof( streamdata[0] ) );
	fseek( f, 0, SEEK_SET );
	fread( streamdata, streamcount, sizeof( streamdata[0] ), f );
	fclose( f );

	f = fopen( argv[2], "rb" );
	fseek( f, 0, SEEK_END );
	glyphct = ftell( f ) / sizeof( glyphdata[0] );
	glyphdata = malloc( glyphct * sizeof( glyphdata[0] ) );
	fseek( f, 0, SEEK_SET );
	fread( glyphdata, glyphct, sizeof( glyphdata[0] ) , f );
	fclose( f );

	num_video_frames = streamcount / ( video_w * video_h / ( BLOCKSIZE * BLOCKSIZE ) );

	printf( "Read %d glyphs, and %d elements (From %d frames)\n", glyphct, streamcount, num_video_frames );
	int i;
	int frame = 0;
	int block = 0;
	CNFGSetup( "comp test", 1800, 900 );

	uint32_t blockmap[video_h/BLOCKSIZE][video_w/BLOCKSIZE];

	for( frame = 0; frame < num_video_frames; frame++ )
	{
		CNFGClearFrame();
		if( !CNFGHandleInput() ) break;

		int bx, by;
		for( by = 0; by < video_h/BLOCKSIZE; by++ )
		for( bx = 0; bx < video_w/BLOCKSIZE; bx++ )
		{
			uint32_t glyphid = streamdata[block++];
			blocktype bt = glyphdata[glyphid];
			printf( "%d %d %d %d\n", bx, by, block, glyphid );
			DrawBlockBasic( bx * BLOCKSIZE*2, by * BLOCKSIZE*2, bt );
		}

		CNFGSwapBuffers();
		frame++;
	}
}


