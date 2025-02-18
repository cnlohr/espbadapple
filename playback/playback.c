#include <stdio.h>

#include "ba_play.h"

#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

#define ZOOM 4

ba_play_context ctx;

int main()
{
	int x, y;
	int frame = 0;

	CNFGSetup( "test", 1024, 768 );

	ba_play_setup( &ctx );

	while( CNFGHandleInput() && frame < FRAMECT )
	{
		CNFGClearFrame();

		if( ba_play_frame( &ctx ) ) break;

		uint8_t * framebuffer = (uint8_t*)ctx.framebuffer;

		for( y = 0; y < RESY; y++ )
		{
			for( x = 0; x < RESX; x++ )
			{
				uint32_t fbv = framebuffer[(x + y*RESX)*BITSETS_TILECOMP/8];
				int ofs = ((x + y*RESX)*BITSETS_TILECOMP)%8;
				fbv >>= ofs;
				uint32_t v = (fbv&0x3) * 255.0 / 3.0;
//printf( "%d %d\n", ofs, v );
				uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );
			}
		}

		CNFGSwapBuffers();
		usleep(10000);
		frame++;
	}
	return 0;
}
