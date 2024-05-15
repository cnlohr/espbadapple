
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
	//printf( "* %d %d %d %p %p %d\n", width, height, linesize, fd, fd+width*height, frame );
	for( y = 0; y < targh; y++ )
	{
		for( x = 0; x < targw; x++ )
		{
			int ix, iy;
			int ixs = x * width / targw;
			int iys = y * height / targh;
			int ixe = (x+1) * width / targw;
			int iye = (y+1) * height / targh;
			int ct = 0;
			int val = 0;
			for( iy = iys; iy < iye; iy++ )
			for( ix = ixs; ix < ixe; ix++ )
			{
				val += (rgbbuffer[ix*3+0+iy*linesize] + rgbbuffer[ix*3+1+iy*linesize] + rgbbuffer[ix*3+2+iy*linesize]);
				ct+=3;
			}
			*(fd++) = val / ct;
		}
	}
	fwrite( data, targw, targh, f );
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
	//fprintf( f, "%d %d\n", targw, targh );
	setup_video_decode();
	video_decode( argv[1], targw, targh );
	fclose( f );
	printf( "Got %d frames\n", frames );
	return 0;
}

