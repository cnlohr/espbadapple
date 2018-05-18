#include <stdint.h>
#include "outsettings.h"
#include <stdio.h>
#include "DrawFunctions.h"
#include <stdlib.h>
#include "gifenc.h"
static ge_GIF *gif;

#define TILEX (FWIDTH/TILE)
#define TILEY (FHEIGHT/TILE)

int      frame;
uint32_t framebuffer[FWIDTH*FHEIGHT];
uint16_t tilemap[TILEX*TILEY];

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }


void EmitFrametile( int16_t tile )
{
	int sf = 1<<SFILLE;
	static int tileinframe = 0;

#if SFILLE > 0
	int tilex = (tileinframe>>SFILLE)%(TILEX);
	int tiley = tileinframe&(sf-1)|
		((tileinframe>>SFILLE)/TILEX)<<SFILLE;
#else
	int tilex = tileinframe % TILEX;
	int tiley = tileinframe / TILEX;
#endif

//	printf( "%d  / %d,%d [%d[%d\n", tileinframe, tilex, tiley, SFILLE, TILEX );

	if( tile >= 0 ) tilemap[tilex+tiley*TILEX] = tile;

	tileinframe++;

	if( tileinframe == TILEX*TILEY )
	{
		//Draw map to screen.
		int x, y, lx, ly;
		for( y = 0; y < TILEY; y++ )
		for( x = 0; x < TILEX; x++ )
		{
			uint16_t tile = tilemap[x+y*TILEX];
			//XXX TODO For different tile sizes.
			uint16_t * gd = (uint16_t*)&glyphdata;

			gd = &gd[tile*TILE];

			for( ly = 0; ly < TILE; ly++ )
			{
				for( lx = 0; lx < TILE; lx++ )
				{
					framebuffer[(lx+x*TILE)+(ly+y*TILE)*FWIDTH] = (gd[ly]&(1<<(TILE-lx-1)))?0xfffffff:0;
				}
			}
		}
		
		CNFGUpdateScreenWithBitmap( framebuffer, FWIDTH, FHEIGHT );
		CNFGSwapBuffers();
		usleep(30000);
		if( frame > 5100 && frame < 6600 )
		{
			for( y = 0; y < FHEIGHT; y++ )
			for( x = 0; x < FWIDTH; x++ )
			{
				gif->frame[(x+y*FWIDTH)] = framebuffer[x+y*FWIDTH]?1:0;
			}
	        ge_add_frame(gif, 4);
		}


		tileinframe = 0;
		frame++;
	}
}

int main()
{
	CNFGSetup( "badapple", FWIDTH, FHEIGHT );

	gif = ge_new_gif(
		"example.gif",  /* file name */
		FWIDTH, FHEIGHT,           /* canvas size */
		(uint8_t []) {  /* palette */
		    0x00, 0x00, 0x00, /* 0 -> black */
		    0xFF, 0xff, 0xff, /* 1 -> white */
		},
		1,              /* palette depth == log2(# of colors) */
		1               /* infinite loop */
		);

	FILE * videodata = fopen( "outvideo.dat", "rb" );
	if( !videodata )
	{
		fprintf( stderr, "Error: cannot open videodata.dat\n" );
		return -1;
	}

	int bitno = 0;
	uint32_t fileword;
	int filewordplace = 32; //Will trigger a new read immediately.
	int token_number = 0;

	for( ; frame < FRAMES;  )
	{
		uint16_t tablekey;
		tablekey = ROOT_HUFF;

		do
		{
			if( filewordplace & 32 ) //filewordplace>=32, but written to leverage jump-if-bit-in-register-set.
			{
				int r = fread( &fileword, 4, 1, videodata );
				if( r <= 0 )
				{
					fprintf( stderr, "Error: premature end of file (%d)\n", r );
					exit( 1 );
				}
				filewordplace = 0;
			}
			int bit = (fileword>>filewordplace)&1;
//			printf( "%d", bit );
			filewordplace++;
			tablekey = huffdata[tablekey*2+bit];
		} while( !(tablekey & 0x8000) );

		//RLE?
		if( tablekey & 0x4000 ) //RLE
		{
			uint16_t rle = rledata[tablekey&0x3fff];
			int flags = rle;
			rle&=0x3fff;
			//printf( "RLE: %d %d\n", rle, flags>>14 );
			while( rle-- )
			{
				if( flags & 0x4000 )
					EmitFrametile(0);
				else if( flags & 0x8000 )
					EmitFrametile(1);
				else
					EmitFrametile(-1);
			}
			//tilemap[TILEX*TILEY];
		}
		else					//GLYPH
		{
			//printf( "Emit: %d\n", tablekey );
			EmitFrametile( tablekey & 0x3fff );
		}

	}

    ge_close_gif(gif);

	printf( "End ok.\n" );

	return 0;
}



