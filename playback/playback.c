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
ge_GIF * gifout;

#ifdef VPX_GREY4

int PValueAt( int x, int y, int * edge )
{
//	graphictype * framebuffer = (graphictype*)ctx.framebuffer;
	if( x < 0 ) { x = 0; *edge = 1; }
	if( x >= RESX ) { x = RESX-1; *edge = 1; }
	if( y < 0 ) { y = 0; *edge = 1; }
	if( y >= RESY ) { y = RESY-1; *edge = 1; }

	int tileplace = (x/BLOCKSIZE)+(y/BLOCKSIZE)*(RESX/BLOCKSIZE);

	glyphtype     tileid = ctx.curmap[tileplace];
	graphictype * sprite = (graphictype*)ctx.glyphdata[tileid];
	graphictype   g      = sprite[x&(BLOCKSIZE-1)];

	uint32_t v = (g>>((y%BLOCKSIZE)));
	int vout = (v & 1) | (( v & 0x100)>>7);
	return vout;
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

#elif defined( VPX_GREY3 )

int PValueAt( int x, int y, int * edge )
{
//	graphictype * framebuffer = (graphictype*)ctx.framebuffer;
	if( x < 0 ) { x = 0; *edge = 1; }
	if( x >= RESX ) { x = RESX-1; *edge = 1; }
	if( y < 0 ) { y = 0; *edge = 1; }
	if( y >= RESY ) { y = RESY-1; *edge = 1; }

	int tileplace = (x/BLOCKSIZE)+(y/BLOCKSIZE)*(RESX/BLOCKSIZE);

	glyphtype     tileid = ctx.curmap[tileplace];
	graphictype * sprite = (graphictype*)ctx.glyphdata[tileid];
	graphictype   g      = sprite[x&(BLOCKSIZE-1)];

	uint32_t v = (g>>((y%BLOCKSIZE)));
	int vout = (v & 1) | (( v & 0x100)>>7);
	return vout;
}

int SampleValueAt( int x, int y )
{
	int edge = 0;
	int iSampleAdd = PValueAt( x, y, &edge );
	int iSampleAddQty = 1;

	// Vaguely based on the grey4 from above... But, with a tighter kernel.

	if( (x & (BLOCKSIZE-1)) == (BLOCKSIZE-1) || (x & (BLOCKSIZE-1)) == 0 )
	{
		iSampleAddQty += 1;
		iSampleAdd += PValueAt( x-1, y, &edge )>>1;
		iSampleAdd += PValueAt( x+1, y, &edge )>>1;
	}
	if( (y & (BLOCKSIZE-1)) == (BLOCKSIZE-1) || (y & (BLOCKSIZE-1)) == 0 )
	{
		iSampleAddQty += 1;
		iSampleAdd += PValueAt( x, y-1, &edge )>>1;
		iSampleAdd += PValueAt( x, y+1, &edge )>>1;
	}
	if( iSampleAddQty == 1 ) return iSampleAdd;

	int r = iSampleAdd - iSampleAddQty;
	if( edge )
	{
		if( r > 1 ) r = 2;
		if( r < 2 ) r = 0;
	}
	else
	{
		if( r > 2 ) r = 2;
		if( r < 0 ) r = 0;
	}

	return r;
}

#define FAKESAMPLE8 1


int outx, outy, outsi;

uint8_t fba[RESY][RESX];

int KOut( uint16_t tg )
{
	static int kx, ky, okx, subframe;
	for( int innery = 0; innery < 8; innery++ )
	{
		fba[ky*8+innery][kx*8+okx] += ((tg>>(innery+subframe*8))&1) | (tg>>(innery+8))&1;
	}
	okx++;
	if( okx == 8 )
	{
		okx = 0;
		kx++;
		if( kx == (RESX/BLOCKSIZE) )
		{
			kx = 0;
			ky++;
			if( ky == RESY/BLOCKSIZE )
			{
				ky = 0;
				subframe++;
				if( subframe == 2 )
				{
					subframe = 0;
				}
			}
		}
	}
}

void EmitSample8()
{
	int bx, by;

	glyphtype * gm = ctx.curmap;
	int subframe;
	memset( fba, 0, sizeof( fba ) );

	for( subframe = 0; subframe < 2; subframe++ )
	{
		for( by = 0; by < RESY/BLOCKSIZE; by ++ )
		{
			for( bx = 0; bx < RESX/BLOCKSIZE; bx ++ )
			{
				// Happens per-column-in-block
				glyphtype gindex = gm[bx+by*RESX/BLOCKSIZE];
				graphictype * g = ctx.glyphdata[gindex];
				int sx = 0;
				if( bx > 0 )
				{
					uint32_t tgnext = g[1];
					uint32_t tg = g[0];
					uint32_t tgprev = ctx.glyphdata[gindex-1][7];

					/* Tricky, blend function:

						tgadd = (tgnext + tgprev)>>1;
						tgtot = tg + tgadd - 2;

						tgnext -> tgnexthi, tgnextlo
						tgprev -> tgprevhi, tgprevlo
						tg     -> tg    hi, tg    lo

						but for left and right we (prev+next)>>1
					*/

					KOut( tg );
					sx = 1;
				}
				int sxend = (bx < RESX/BLOCKSIZE-1) ? 7: 8;
				for( ; sx < sxend; sx++ )
				{
					uint16_t tg = g[sx];

					KOut( tg );
				}
				if( sx < 8 )
				{
					// Blend last.
					uint32_t tgprev = g[6];
					uint32_t tg = g[7];
					uint32_t tgnext = ctx.glyphdata[gindex+1][0];
					KOut( tg );
				}
			}
		}
	}

	int x, y;
	for( y = 0; y < RESY; y++ )
	for( x = 0; x < RESX; x++ )
	{
		float f = fba[y][x] * 128;
		if( f < 0 ) f = 0; 
		if( f > 255.5 ) f = 255.5;
		int v = f;
		uint8_t * gof = gifout->frame;
		int zx, zy;
		for( zx = 0; zx < ZOOM; zx++ )
		for( zy = 0; zy < ZOOM; zy++ )
			gof[zx+x*ZOOM + (zy+y*ZOOM)*RESX*ZOOM] = (f/120);
		uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
		CNFGColor( color );
		CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );
	}
}

#endif

int main()
{
	int x, y;
	int frame = 0;

	CNFGSetup( "test", 1024, 768 );

#ifdef VPX_GREY4
	static uint8_t palette[48] = { 0, 0, 0, 85, 85, 85, 171, 171, 171, 255, 255, 255 };
#elif defined( VPX_GREY3 )
	static uint8_t palette[48] = { 0, 0, 0, 128, 128, 128, 255, 255, 255 };
#endif
	gifout = ge_new_gif( "playback.gif", RESX*ZOOM, RESY*ZOOM, palette, 4, -1, 0 );

	ba_play_setup( &ctx );
	ba_audio_setup();

	FILE * fAudioDump = fopen( "audio.dump", "wb" );

	int outbuffertail = 0;
	int lasttail = 0;

	while( CNFGHandleInput() && frame < FRAMECT )
	{
		CNFGClearFrame();

		if( ba_play_frame( &ctx ) ) break;

#ifdef FAKESAMPLE8
		EmitSample8();
#else
		for( y = 0; y < RESY; y++ )
		{
			for( x = 0; x < RESX; )
			{
				{
					int sample = SampleValueAt( x, y );
					int f = sample;
#ifdef VPX_GREY4
					f = f * 85;
#elif defined( VPX_GREY3 )
					f = f * 128;
#endif
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
		}

#endif

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
