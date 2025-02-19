#include <stdio.h>

#include "ba_play.h"

#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#include "gifenc.c"


void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

#define ZOOM 2

ba_play_context ctx;

int PValueAt( int x, int y )
{
	graphictype * framebuffer = (graphictype*)ctx.framebuffer;
	if( x < 0 ) x = 0;
	if( x >= RESX ) x = RESX-1;
	if( y < 0 ) y = 0;
	if( y >= RESY ) y = RESY-1;

	graphictype fbv = framebuffer[(x + y*RESX)*BITSETS_TILECOMP/(GRAPHICSIZE_WORDS*8)];
	int ofs = ((x + y*RESX)*BITSETS_TILECOMP)%(GRAPHICSIZE_WORDS*8);
	fbv >>= ofs;
	return (fbv&0x3);
}

int SampleValueAt( int x, int y )
{
	int iSampleAdd = PValueAt( x, y );
	int iSampleAddQty = 1;
	if( (x & (BLOCKSIZE-1)) == (BLOCKSIZE-1) || (x & (BLOCKSIZE-1)) == 0 )
	{
		iSampleAddQty += 2;
		iSampleAdd += PValueAt( x-1, y );
		iSampleAdd += PValueAt( x+1, y );
	}
	if( (y & (BLOCKSIZE-1)) == (BLOCKSIZE-1) || (y & (BLOCKSIZE-1)) == 0 )
	{
		iSampleAddQty += 2;
		iSampleAdd += PValueAt( x, y-1 );
		iSampleAdd += PValueAt( x, y+1 );
	}
	if( iSampleAddQty == 1 ) return iSampleAdd;

	// I have no rationale for why this looks good.  I tried it randomly and liked it.
	int r = iSampleAdd - iSampleAddQty - 1;
	if( r > 3 ) r = 3;
	if( r < 0 ) r = 0;
	return r;
}

int main()
{
	int x, y;
	int frame = 0;

	CNFGSetup( "test", 1024, 768 );

	static uint8_t palette[48] = { 0, 0, 0, 85, 85, 85, 171, 171, 171, 255, 255, 255 };
	ge_GIF * gifout = ge_new_gif( "playback.gif", RESX*ZOOM, RESY*ZOOM, palette, 4, -1, 0 );

	ba_play_setup( &ctx );

	while( CNFGHandleInput() && frame < FRAMECT )
	{
		CNFGClearFrame();

		if( ba_play_frame( &ctx ) ) break;

		for( y = 0; y < RESY; y++ )
		{
			for( x = 0; x < RESX; x++ )
			{
				//float f = (PValueAt( x, y ) + PValueAt( x+1,y+1)*.5 + PValueAt(x+1,y)*.5 + PValueAt(x,y+1)*.5)/2.5;
				//f = f * 2 - 255.0;
				int sample = SampleValueAt( x, y );
				int f = sample;
				f = f * 85;
				if( f < 0 ) f = 0; 
				if( f > 255.5 ) f = 255.5;
				int v = f;
				uint8_t * gof = gifout->frame;
				int zx, zy;
				for( zx = 0; zx < ZOOM; zx++ )
				for( zy = 0; zy < ZOOM; zy++ )
					gof[zx+x*ZOOM + (zy+y*ZOOM)*RESX*ZOOM] = sample;
				uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
				CNFGColor( color );
				CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );
			}
		}

		ge_add_frame(gifout, 2);

		CNFGSwapBuffers();
		frame++;
	}

	ge_close_gif( gifout );

	return 0;
}
