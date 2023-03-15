#include <stdint.h>
#include "outsettings.h"
#include <stdio.h>

#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"
#include <stdlib.h>
#include "gifenc.h"
static ge_GIF *gif;

#define TILEX (FWIDTH/TILE_W)
#define TILEY (FHEIGHT/TILE_H)

int      frame;
uint32_t framebuffer[FWIDTH*FHEIGHT];
uint16_t tilemap[TILEX*TILEY];

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

int tileupdatemap[TILEY*TILEX];


void EmitFrametile( int16_t tile )
{
	static int tileinframe = 0;
	int sf = 1<<SFILLE;

#if SFILLE > 0
	int tilex = (tileinframe>>SFILLE)%(TILEX);
	int tiley = tileinframe&(sf-1)|
		((tileinframe>>SFILLE)/TILEX)<<SFILLE;
#else
	int tilex = tileinframe % TILEX;
	int tiley = tileinframe / TILEX;
#endif

//	printf( "%d  / %d,%d [%d[%d\n", tileinframe, tilex, tiley, SFILLE, TILEX );

	tileinframe++;

	if( tile >= 0 )
	{
		tilemap[tilex+tiley*TILEX] = tile;
		tileupdatemap[tilex+tiley*TILEX]++;
	}

	if( tileinframe == TILEX*TILEY )
	{

		//Draw map to screen.
		int x, y, lx, ly;
		for( y = 0; y < TILEY; y++ )
		for( x = 0; x < TILEX; x++ )
		{
			uint16_t tile = tilemap[x+y*TILEX];
			//XXX TODO For different tile sizes.
#if TILE_W == 16
			uint16_t * gd = (uint16_t*)&glyphdata;

			gd = &gd[tile*TILE_W];

			for( ly = 0; ly < TILE_H; ly++ )
			{
				for( lx = 0; lx < TILE_W; lx++ )
				{
					framebuffer[(lx+x*TILE_W)+(ly+y*TILE_H)*FWIDTH] = (gd[ly]&(1<<(TILE_W-lx-1)))?0xfffffff:0;
				}
			}
#elif TILE_W == 12
			uint16_t * gd = (uint16_t*)&glyphdata;
			gd = &gd[tile*9];
			int tp = 0;
			for( ly = 0; ly < TILE_H; ly++ )
			{
				for( lx = 0; lx < TILE_W; lx++ )
				{
					int bon = ((gd[0]<<tp) & 0x8000);
					tp++;
					if( tp == 16 ) tp = 0, gd++;
					framebuffer[(lx+x*TILE_W)+(ly+y*TILE_H)*FWIDTH] = bon?0xfffffff:0;
				}
			}
#else
			uint8_t * gd = (uint8_t*)&glyphdata;
			gd = &gd[tile*TILE_W];
			for( ly = 0; ly < TILE_H; ly++ )
			{
				for( lx = 0; lx < TILE_W; lx++ )
				{
					framebuffer[(lx+x*TILE_W)+(ly+y*TILE_H)*FWIDTH] = (gd[7-ly]&(1<<(TILE_W-lx-1)))?0xfffffff:0;
				}
			}


#endif
		}

		CNFGUpdateScreenWithBitmap( framebuffer, FWIDTH, FHEIGHT );
		//CNFGBlitImage( framebuffer, 0, 0, FWIDTH, FHEIGHT );
		//CNFGSwapBuffers();
		//usleep(30000);

		if( frame > 0 && frame < 6568 )
		{
			for( y = 0; y < FHEIGHT; y++ )
			for( x = 0; x < FWIDTH; x++ )
			{
				gif->frame[(x+y*FWIDTH)] = framebuffer[x+y*FWIDTH]?1:0;
			}
		        ge_add_frame(gif, 7);
		}


		tileinframe = 0;
		frame++;
		//printf( "Frame: %d\n", frame );
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
	int videodatawords = 0;

	for( ; frame < FRAMES;  )
	{
		uint16_t tablekey;
		tablekey = ROOT_HUFF;

		// Process through the raw data.
		do
		{
			if( filewordplace & 32 ) //filewordplace>=32, but written to leverage jump-if-bit-in-register-set.
			{
				int r = fread( &fileword, 4, 1, videodata );
				videodatawords++;
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
			rle &= 0x3fff;
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

	int x, y;
	for( y = 0; y < TILEY; y++)
	{
		for( x = 0; x < TILEX; x++ )
		{
			printf( "%4d  ", tileupdatemap[x+y*TILEX] );
		}
		printf( "\n" );
	}

	printf( "%d frames\n", FRAMES );
	printf( "Glyph Data: %d\n", sizeof( glyphdata ) );
	printf( "RLE Data: %d\n", sizeof( rledata ) );
	printf( "Huff Data: %d\n", sizeof( huffdata ) );
	printf( "Raw Data: %d\n", videodatawords * 4 );
	printf( "Sum of data: %d\n", sizeof( glyphdata ) + sizeof( rledata ) + sizeof( huffdata ) + videodatawords * 4 );
	return 0;
}



