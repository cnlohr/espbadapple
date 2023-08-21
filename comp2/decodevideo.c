
#include "ffmdecode.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int targw, targh;

uint8_t * data;
int frames;
FILE * f ;
void got_video_frame( unsigned char * rgbbuffer, int linesize, int width, int height, int frame )
{
	//else data = realloc( data, width*height*(++frames));
	uint8_t * fd = data;
	int x;
	int y;
	printf( "%d %d %d %p %p %d\n", width, height, linesize, fd, fd+width*height, frame );
	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			*(fd++) = (rgbbuffer[x*3+0+y*linesize] + rgbbuffer[x*3+1+y*linesize] + rgbbuffer[x*3+2+y*linesize])/3;
		}
	}
	fwrite( data, width, height, f );
	frames++;	
}


int main( int argc, char ** argv )
{
	if( argc != 5 )
	{
		fprintf( stderr, "Usage: [tool] [video] [out x] [out y] [outfile]\n" );
		return -9;
	}
	targw = atoi( argv[2] );
	targh = atoi( argv[3] );
	data = malloc( targw*targh );
	f = fopen( argv[4], "wb" );
	fprintf( f, "%d %d\n", targw, targh );
	setup_video_decode();
	video_decode( argv[1], targw, targh );
	fclose( f );
	return 0;
}

