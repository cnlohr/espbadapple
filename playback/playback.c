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

float PValueAt( int x, int y )
{
	uint8_t * framebuffer = (uint8_t*)ctx.framebuffer;
	if( x < 0 ) x = 0;
	if( x >= RESX ) x = RESX-1;
	if( y < 0 ) y = 0;
	if( y >= RESY ) y = RESY-1;

	uint32_t fbv = framebuffer[(x + y*RESX)*BITSETS_TILECOMP/8];
	int ofs = ((x + y*RESX)*BITSETS_TILECOMP)%8;
	fbv >>= ofs;
	return (fbv&0x3) * 255.0 / 3.0;
}

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

		for( y = 0; y < RESY; y++ )
		{
			for( x = 0; x < RESX; x++ )
			{
				float f = (PValueAt( x, y ) + PValueAt( x+1,y+1)*.5 + PValueAt(x+1,y)*.5 + PValueAt(x,y+1)*.5)/2.5;
				f = f * 2 - 255.0;
				if( f < 0 ) f = 0; 
				if( f > 255.5 ) f = 255.5;
				int v = f;
				uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );
			}
		}

		CNFGSwapBuffers();
		usleep(20000);
		frame++;
	}
	return 0;
}
