#include <stdio.h>

#include "ba_play.h"

#define F_SPS 48000
#define AUDIO_BUFFER_SIZE 2048


#include "ba_play_audio.h"


#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#include "gifenc.c"


void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
int HandleDestroy() { return 0; }

#define ZOOM 2

uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
ba_play_context ctx;

int PValueAt( int x, int y, int * edge )
{
	graphictype * framebuffer = (graphictype*)ctx.framebuffer;
	if( x < 0 ) { x = 0; *edge = 1; }
	if( x >= RESX ) { x = RESX-1; *edge = 1; }
	if( y < 0 ) { y = 0; *edge = 1; }
	if( y >= RESY ) { y = RESY-1; *edge = 1; }

	graphictype fbv = framebuffer[(x + y*RESX)*BITSETS_TILECOMP/(GRAPHICSIZE_WORDS*8)];
	int ofs = ((x + y*RESX)*BITSETS_TILECOMP)%(GRAPHICSIZE_WORDS*8);
	fbv >>= ofs;
	return (fbv&0x3);
}

int SampleValueAt( int x, int y )
{
	int edge = 0;
	int iSampleAdd = PValueAt( x, y, &edge );
	int iSampleAddQty = 1;
	if( (x & (BLOCKSIZE-1)) == (BLOCKSIZE-1) || (x & (BLOCKSIZE-1)) == 0 )
	{
		iSampleAddQty += 2;
		iSampleAdd += PValueAt( x-1, y, &edge );
		iSampleAdd += PValueAt( x+1, y, &edge );
	}
	if( (y & (BLOCKSIZE-1)) == (BLOCKSIZE-1) || (y & (BLOCKSIZE-1)) == 0 )
	{
		iSampleAddQty += 2;
		iSampleAdd += PValueAt( x, y-1, &edge );
		iSampleAdd += PValueAt( x, y+1, &edge );
	}
	if( iSampleAddQty == 1 ) return iSampleAdd;

	// I have no rationale for why this looks good.  I tried it randomly and liked it.
	int r = iSampleAdd - iSampleAddQty;
	if( edge )
	{
		if( r > 1 ) r = 3;
		if( r < 2 ) r = 0;
	}
	else
	{
		if( r > 3 ) r = 3;
		if( r < 0 ) r = 0;
	}

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
	ba_audio_setup();

	FILE * fAudioDump = fopen( "audio.dump", "wb" );

	int outbuffertail = 0;
	int lasttail = 0;

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

		frame++;

		lasttail = outbuffertail;
		outbuffertail = (F_SPS/30*frame) % AUDIO_BUFFER_SIZE;
		ba_audio_fill_buffer( out_buffer_data, outbuffertail );

		for( int n = lasttail; n != outbuffertail; n = (n+1)%AUDIO_BUFFER_SIZE )
		{
			float fo = out_buffer_data[n] / 128.0 - 1.0;
			fwrite( &fo, 1, 4, fAudioDump );
		}

		CNFGSwapBuffers();
	}

	ge_close_gif( gifout );

	return 0;
}
