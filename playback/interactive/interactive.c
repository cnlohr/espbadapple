#include <stdio.h>

// XXX TODO: Handle partial bits in ba_play.h
// TODO: Add buttons to toggle on and off each filtering axis.
// TODO: Update hardware with new logic code for edge blending/filtering.
// TODO: Improve memory graph showing more memory.

#define WARNING(x...) printf( x );
#define CHECKPOINT(x...) { x; ba_i_checkpoint(); }
#define CHECKBITS_AUDIO(x) { bitsperframe_audio[frame] += x;}
#define CHECKBITS_VIDEO(x) { bitsperframe_video[frame] += x;}

int frame = 0;

double bitsperframe_audio[FRAMECT];
double bitsperframe_video[FRAMECT];

#define MAX_PREFRAMES 8192
int checkpoint_offset_by_frame[FRAMECT];
int checkpoint_offset_by_frame_virtual[FRAMECT+MAX_PREFRAMES];
int vframe, vframe_offset;

void ba_i_checkpoint();

// Various set things
const char * decodephase;

#define FIELDS(x) x(decodeglyph) x(decode_is0or1) x(decode_runsofar) x(decode_prob) x(decode_lb) \
	x(decode_cellid) x(decode_class) x(decode_run) x(decode_fromglyph) \
	x(decode_tileid) x(decode_level) x(decoding_glyphs) x(vpxcheck) x(vpxcpv) \
	x(audio_pullbit) x(audio_gotbit) x(audio_last_bitmode) \
	x(audio_golmb_exp) x(audio_golmb_v) x(audio_golmb_br) x(audio_golmb) x(audio_last_ofs) \
	x(audio_last_he) x(audio_pullhuff) x(audio_stack_place) x(audio_stack_remain) \
	x(audio_stack_offset) x(audio_backtrace) x(audio_newnote) x(audio_lenandrun) \
	x(audio_gotnote)

#define xcomma(y) y,
int FIELDS(xcomma) dummy;

#include "ba_play.h"

#define F_SPS 48000
#define AUDIO_BUFFER_SIZE 2048

int AS_PER_FRAME = F_SPS/30;

#include "ba_play_audio.h"


#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"


uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
ba_play_context ctx;


int cursor = 0;
int midCursor = 0;
int topCursor = 0;

struct checkpoint
{
	// Only updated if different.
	glyphtype	(* curmap)[BLKY*BLKX];
	uint8_t	  (* currun)[BLKY*BLKX];
	graphictype  (* glyphdata)[TILE_COUNT][DBLOCKSIZE/GRAPHICSIZE_WORDS];
	vpx_reader  * baplay_vpx;

	struct ba_audio_player_stack_element (*audio_stack)[ESPBADAPPLE_SONG_MAX_BACK_DEPTH];
	uint16_t   (*audio_playing_freq)[NUM_VOICES];
	uint16_t   (*audio_phase)[NUM_VOICES];
	int		(*audio_tstop)[NUM_VOICES];

	uint8_t 	* audio_sample_data;
	int 		audio_sample_data_frame;

	// Points to the farme # where the data actually resides.
	int curmap_frame;
	int currun_frame;
	int glyphdata_frame;
	int baplay_vpx_frame;

	int audio_stack_frame;
	int audio_playing_freq_frame;
	int audio_phase_frame;
	int audio_tstop_frame;

	int audio_nexttrel;
	int audio_ending;
	int audio_t;
	int audio_gotnotes;
	int audio_sub_t_sample;
	int audio_outbufferhead;
	int audio_stackplace;

	int frame;
	int vframe;
	int frameChanged;

	const char * decodephase;
	int FIELDS(xcomma) dummy;

} * checkpoints;

int nrcheckpoints;
int inPlayMode;
double fFrameElapse;

#include "extradrawing.h"

void ba_i_checkpoint()
{
	//printf( "CHECKPOINT: %d\n", nrcheckpoints );
	checkpoints = realloc( checkpoints, (nrcheckpoints+1) * sizeof( struct checkpoint ) );
	struct checkpoint * cpp = nrcheckpoints ? &checkpoints[nrcheckpoints-1] : 0;
	struct checkpoint * cp = &checkpoints[nrcheckpoints];

	#define CPFIELD( field, ctf, size ) \
	if( !cpp || memcmp( cpp->field, ctf, size ) ) \
	{ \
		cp->field##_frame = nrcheckpoints; \
		cp->field = malloc( size ); \
		memcpy( cp->field, ctf, size ); \
	} \
	else \
	{ \
		cp->field = checkpoints[cpp->field##_frame].field; \
		cp->field##_frame = cpp->field##_frame; \
	}

	CPFIELD( curmap, ctx.curmap, sizeof( ctx.curmap ) );
	CPFIELD( currun, ctx.currun,  sizeof( ctx.currun ) );
	CPFIELD( glyphdata, ctx.glyphdata, sizeof( ctx.glyphdata ) );
	CPFIELD( baplay_vpx, &ctx.vpx_changes, sizeof( ctx.vpx_changes ) );
	CPFIELD( audio_stack, &ba_player.stack, sizeof( ba_player.stack ) );
	CPFIELD( audio_playing_freq, ba_player.playing_freq, sizeof( ba_player.playing_freq ) );
	CPFIELD( audio_phase, ba_player.phase, sizeof( ba_player.phase ) );
	CPFIELD( audio_tstop, ba_player.tstop, sizeof( ba_player.tstop ) );

	cp->audio_nexttrel = ba_player.nexttrel;
	cp->audio_ending = ba_player.ending;
	cp->audio_t = ba_player.t;
	cp->audio_gotnotes = ba_player.gotnotes;
	cp->audio_sub_t_sample = ba_player.sub_t_sample;
	cp->audio_outbufferhead = ba_player.outbufferhead;
	cp->audio_stackplace = ba_player.stackplace;

	cp->audio_sample_data = cpp ? cpp->audio_sample_data : 0 ;
	cp->audio_sample_data_frame = cpp ? cpp->audio_sample_data_frame : -1;

	cp->frameChanged = cpp ? ( frame != cpp->frame ) ? 1 : 0 : 0;
	cp->frame = frame;
	
	checkpoint_offset_by_frame[frame] = nrcheckpoints;
	checkpoint_offset_by_frame_virtual[vframe] = nrcheckpoints;


	if( cp->frameChanged ) vframe++;
	if( frame == 0 && ( nrcheckpoints % 500 == 0 ) && vframe < MAX_PREFRAMES ) vframe++;
	if( frame && !vframe_offset ) vframe_offset = vframe-1;
	cp->vframe = vframe;
	cp->decodephase = decodephase;

	#define xassign(tf) cp->tf = tf;
	FIELDS(xassign);

	nrcheckpoints++;
	//printf( "NRC: %d\n", nrcheckpoints );
}


#ifdef VPX_GREY4

int PValueAt( int x, int y, int * edge )
{
//	graphictype * framebuffer = (graphictype*)ctx.framebuffer;
	if( x < 0 ) { x = 0; *edge = 1; }
	if( x >= RESX ) { x = RESX-1; *edge = 1; }
	if( y < 0 ) { y = 0; *edge = 1; }
	if( y >= RESY ) { y = RESY-1; *edge = 1; }

	int tileplace = (x/BLOCKSIZE)+(y/BLOCKSIZE)*(RESX/BLOCKSIZE);

	glyphtype	 tileid = ctx.curmap[tileplace];
	graphictype * sprite = (graphictype*)ctx.glyphdata[tileid];
	graphictype   g	  = sprite[x&(BLOCKSIZE-1)];

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

	glyphtype	 tileid = ctx.curmap[tileplace];
	graphictype * sprite = (graphictype*)ctx.glyphdata[tileid];
	graphictype   g	  = sprite[x&(BLOCKSIZE-1)];

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
		//((tg>>(innery+subframe*8))&1) | 
		fba[ky*8+innery][kx*8+okx] += (tg>>(innery))&1;
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
	return 0;
}

void EmitPartial( graphictype tgprev, graphictype tg, graphictype tgnext, int subframe )
{
	// This should only need +2 regs (or 3 depending on how the optimizer slices it)
	// (so all should fit in working reg space)
	graphictype A = tgprev >> 8;
	graphictype B = tgprev;      // implied & 0xff
	graphictype C = tgnext >> 8;
	graphictype D = tgnext;      // implied & 0xff
	graphictype E = tg >> 8;
	graphictype F = tg;          // implied & 0xff

	if( subframe )
		tg = (D&E)|(B&E)|(B&C&F)|(A&D&F);     // 8 bits worth of MSB of this+(next+prev+1)/2-1 (Assuming values of 0,1,3)
	else
		tg = E|C|A|(D&F)|(B&F)|(B&D);       // 8 bits worth of MSB|LSB of this+(next+prev+1)/2-1

	KOut( tg );
}

// one pixel at a time.
int PixelBlend( int tgprev, int tg, int tgnext )
{
	if( tg == 2 ) tg = 3;
	if( tgprev == 2 ) tgprev = 3;
	if( tgnext == 2 ) tgnext = 3;
	// this+(next+prev+1)/2-1 assuming 0..3, skip 2.
	//printf( "%d\n", tg );
	tg = (tg + (tgprev+tgnext+1)/2-1);
	if( tg < 0 ) tg = 0;
	if( tg > 2 ) tg = 2;
	return tg;
}

void EmitSamples8( struct checkpoint * cp, float ofsx, float ofsy, float fzoom, glyphtype * gm,
	graphictype  glyphdata[TILE_COUNT][DBLOCKSIZE/GRAPHICSIZE_WORDS] )
{
	int bx, by;
	int subframe;
	memset( fba, 0, sizeof( fba ) );

	for( subframe = 0; subframe < 2; subframe++ )
	{
		int gmi = 0;
		for( by = 0; by < RESY/BLOCKSIZE; by ++ )
		{
			for( bx = 0; bx < RESX/BLOCKSIZE; bx ++ )
			{
				// Happens per-column-in-block
				glyphtype gindex = gm[gmi];

				if( cp->decodephase == "Decoding Bit" && cp->decode_cellid == gmi )
				{
					// Override map.
					gindex = cp->decode_tileid;
				}

				graphictype * g = glyphdata[gindex];
				int sx = 0;
				if( bx > 0 )
				{
					graphictype tgnext = g[1];
					graphictype tg = g[0];
					graphictype tgprev = (bx > 0 ) ? glyphdata[gm[gmi-1]][7] : tgnext;

					EmitPartial( tgprev, tg, tgnext, subframe );
					sx = 1;
				}
				int sxend = (bx < RESX/BLOCKSIZE-1) ? 7: 8;
				for( ; sx < sxend; sx++ )
				{
					uint16_t tg = g[sx];
					KOut( tg >> (subframe*8) );
				}
				if( sx < 8 )
				{
					// Blend last.
					graphictype tgprev = g[6];
					graphictype tg = g[7];
					graphictype tgnext = (bx<RESX/BLOCKSIZE-1)?glyphdata[gm[gmi+1]][0] : tgprev;
					EmitPartial( tgprev, tg, tgnext, subframe );
				}
				gmi++;
			}
		}
	}

	int x, y;
	for( y = 0; y < RESY; y++ )
	for( x = 0; x < RESX; x++ )
	{
		int vi = 0;
		if( ( y & 7 ) == 0 )
		{
			vi = PixelBlend( fba[y+1][x], fba[y][x], (y>0)?fba[y-1][x]:fba[y+1][x] );
		}
		else if( ( y & 7 ) == 7 )
		{
			vi = PixelBlend( (y < RESY-1)?fba[y+1][x]:fba[y-1][x], fba[y][x], fba[y-1][x] );
		}
		else
		{
			vi = fba[y][x];
		}

		float f = vi * 128;
		if( f < 0 ) f = 0; 
		if( f > 255.5 ) f = 255.5;
		int v = f;
		int zx, zy;
		//for( zx = 0; zx < ZOOM; zx++ )
		//for( zy = 0; zy < ZOOM; zy++ )
		//	gof[zx+x*ZOOM + (zy+y*ZOOM)*RESX*ZOOM] = (f/120);
		uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
		CNFGColor( color );
		CNFGTackRectangle( x*fzoom+ofsx, y*fzoom+ofsy, x*fzoom+fzoom+ofsx, y*fzoom+fzoom+ofsy );
	}
	CNFGSetLineWidth(1.0);
	CNFGColor( 0xc0c0c010 );
	for( y = 0; y < RESY; y++ )
	for( x = 0; x < RESX; x++ )
	{
		bx = x/BLOCKSIZE;
		by = y/BLOCKSIZE;
		CNFGTackSegment( x*fzoom+ofsx, y*fzoom+ofsy, x*fzoom+fzoom+ofsx, y*fzoom+ofsy );
		CNFGTackSegment( x*fzoom+ofsx, y*fzoom+ofsy, x*fzoom+ofsx, y*fzoom+ofsy );
		CNFGTackSegment( x*fzoom+ofsx, y*fzoom+fzoom+ofsy, x*fzoom+fzoom+ofsx, y*fzoom+fzoom+ofsy );
		CNFGTackSegment( x*fzoom+fzoom+ofsx, y*fzoom+ofsy, x*fzoom+fzoom+ofsx, y*fzoom+fzoom+ofsy );
	}
	for( y = 0; y < RESY; y+=BLOCKSIZE )
	for( x = 0; x < RESX; x+=BLOCKSIZE )
	{
		bx = x/BLOCKSIZE;
		by = y/BLOCKSIZE;
    	// cp 
		int cell_selected = cp->decode_cellid == bx+by*(RESX/BLOCKSIZE);
		CNFGColor( cell_selected ? 0xc0c0c0ff : 0xc0c0c020 );
		CNFGSetLineWidth(3.0);
		CNFGTackSegment( x*fzoom+ofsx, y*fzoom+ofsy, x*fzoom+fzoom*BLOCKSIZE+ofsx, y*fzoom+ofsy );
		CNFGTackSegment( x*fzoom+ofsx, y*fzoom+ofsy, x*fzoom+ofsx, y*fzoom+fzoom*BLOCKSIZE+ofsy );
		CNFGTackSegment( x*fzoom+ofsx, y*fzoom+fzoom*BLOCKSIZE+ofsy, x*fzoom+fzoom*BLOCKSIZE+ofsx, y*fzoom+fzoom*BLOCKSIZE+ofsy );
		CNFGTackSegment( x*fzoom+fzoom*BLOCKSIZE+ofsx, y*fzoom+ofsy, x*fzoom+fzoom*BLOCKSIZE+ofsx, y*fzoom+fzoom*BLOCKSIZE+ofsy );

		int crun = (*cp->currun)[bx+by*(RESX/BLOCKSIZE)];
		glyphtype gindex = gm[bx+by*(RESX/BLOCKSIZE)];
		if( cp->decodephase == "Decoding Bit" && cell_selected )
		{
			// Override map.
			gindex = cp->decode_tileid;
			crun = -1;
		}

		int bx = x / BLOCKSIZE;
		int by = y / BLOCKSIZE;
		DrawFormat( x*fzoom+ofsx+4*fzoom, y*fzoom+ofsy+1*fzoom,-.45*fzoom, cell_selected ? 0xc0c0c0ff : 0xc0c0c030, "%02x", gindex );
		DrawFormat( x*fzoom+ofsx+4*fzoom, y*fzoom+ofsy+5*fzoom,-.3*fzoom, cell_selected ? 0xc0c0c0ff : 0xc0c0c040, "%d", crun );
	}
}

#endif


void DrawTopGraph( Clay_RenderCommand * render )
{
	if( !checkpoints ) return;

	Clay_BoundingBox b = render->boundingBox;
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };
	float nindex = 0;
	int index = 0;
	int x;
	static int has_down_focus;

	if( mouseDownThisFrame ) has_down_focus = inBox(b);
	if( !isMouseDown ) has_down_focus = false;

	int did_scroll = 0;

	if( has_down_focus )
	{
		if( cursor_rel.x < 0 )
		{
			cursor += cursor_rel.x;
			topCursor += cursor_rel.x;
			did_scroll = 1;
		}
		if( cursor_rel.x >= b.width )
		{
			cursor += cursor_rel.x - b.width;
			topCursor += cursor_rel.x - b.width;
			did_scroll = 1;
		}

		if( cursor < 0 ) cursor = 0;
		if( cursor >= nrcheckpoints ) cursor = nrcheckpoints-1;
		if( topCursor < 0 ) topCursor = 0;
		if( topCursor >= nrcheckpoints ) topCursor = nrcheckpoints-1;
		if( did_scroll )
		{
			midCursor = cursor;
			inPlayMode = 0;
		}
	}

	int centerCursor = topCursor;

	int baseZoom = 1;
	int frameWidth = b.width/baseZoom;
	int f = centerCursor - frameWidth/2;

	for( x = 0; x < frameWidth; x++ )
	{
		if( x+f >= nrcheckpoints || x+f < 0 ) continue;
		struct checkpoint * cp = &checkpoints[x+f];

		int vSt = -1;
		int vStSt = 2;
		if( cp->frameChanged )
		{
			CNFGColor( 0xffffff80 );
			vStSt = 26;
			vSt = 0;
		}
		else if( cp->decodephase == "Running" )
		{
			CNFGColor( 0x808080ff );
			vSt = 3;
		}
		else if( cp->decodephase == "Run Stopped" )
		{
			CNFGColor( 0xffffffff );
			vSt = 3;
		}
		else if( cp->decodephase == "Decoding Bit" )
		{
			CNFGColor( 0x808080ff );
			vSt = 6;
		}
		else if( cp->decodephase == "Committing Tile" )
		{
			CNFGColor( 0xffffff80 );
			vSt = 6;
		}
		else if( cp->decodephase == "AUDIO: Note Complete" )
		{
			CNFGColor( 0xffffff80 );
			vSt = 9;
		}
		else if( cp->decodephase == "AUDIO: Perform 16th Note" )
		{
			CNFGColor( 0xffffffff );
			vStSt = 12;
			vSt = 12;
		}
		else if( cp->decodephase == "AUDIO: Pulling Note" )
		{
			CNFGColor( 0xffffffff );
			vSt = 12;
		}
		else if( cp->decodephase == "AUDIO: Popping back stack" )
		{
			CNFGColor( 0xffffff80 );
			vSt = 15;
		}
		else if( cp->decodephase == "AUDIO: Reading Next" )
		{
			CNFGColor( 0xffffff40 );
			vSt = 15;
		}
		else if( cp->decodephase == "AUDIO: Backtrack" )
		{
			CNFGColor( 0xffffffff );
			vSt = 18;
		}
		else if( cp->decodephase == "AUDIO: No Backtrack" )
		{
			CNFGColor( 0xffffff20 );
			vSt = 15;
		}
		else if( cp->decodephase == "AUDIO: Reading Note" )
		{
			CNFGColor( 0xffffff20 );
			vSt = 18;
		}
		else if( cp->decodephase == "AUDIO: Reading Length and Run" )
		{
			CNFGColor( 0xffffff10 );
			vSt = 18;
		}
		else if( cp->decodephase == "AUDIO: Backtrack" )
		{
			CNFGColor( 0xffffff40 );
			vSt = 21;
		}
		else if( cp->decodephase == "AUDIO: Reading Backtrack Run Length" )
		{
			CNFGColor( 0xffffff60 );
			vSt = 21;
		}
		else if( cp->decodephase == "AUDIO: Reading Backtrack Offset" )
		{
			CNFGColor( 0xffffff90 );
			vSt = 21;
		}
		else if( cp->decodephase == "AUDIO: Read Len And Run" )
		{
			CNFGColor( 0xffffffff );
			vSt = 21;
		}


		if( vSt >= 0 ) CNFGTackSegment( b.x + x*baseZoom, b.y+b.height-vSt-vStSt-2, b.x+x*baseZoom, b.y+b.height-vSt-2 );

		//CNFGColor( 0xc0c0c0ff );
		//CNFGTackSegment( b.x + x*baseZoom, b.y+b.height-(bits)/fMaxBits*b.height, b.x+x*baseZoom, b.y+b.height-bitsA/fMaxBits*b.height );
		if( ( inBox(b) || has_down_focus ) && (int)(x) == (int)(cursor_rel.x/baseZoom) )
		{
		    Clay_LayoutElement *openLayoutElement = Clay__GetOpenLayoutElement();
			CNFGColor( 0xffffff80 );
			CNFGTackSegment( b.x + x*baseZoom, b.y, b.x+x*baseZoom, b.y+b.height );
			if( has_down_focus && !did_scroll )
			{
				midCursor = cursor = x+f;
				inPlayMode = 0;
			}
		}

		if( (int)(x+f) == (int)(cursor) )
		{
			CNFGColor( 0xffffff30 );
			CNFGTackSegment( b.x + x*baseZoom, b.y, b.x+x*baseZoom, b.y+b.height );
		}
	}
}


void DrawMidGraph( Clay_RenderCommand * render )
{
	if( !checkpoints ) return;

	Clay_BoundingBox b = render->boundingBox;
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };
	float nindex = 0;
	float iadv = FRAMECT/b.width;
	int index = 0;
	int x;
	static int has_down_focus;

	if( mouseDownThisFrame ) has_down_focus = inBox(b);
	if( !isMouseDown ) has_down_focus = false;

	int centerFrame = checkpoints[midCursor].vframe;

	int baseZoom = 2;
	int frameWidth = b.width/baseZoom;
	int f = centerFrame - frameWidth/2;

	float fMaxBits = 0;
	for( x = 0; x < frameWidth; x++ )
	{
		float bitsA = (x+f-vframe_offset>=0 && x+f-vframe_offset < FRAMECT )?bitsperframe_audio[x+f-vframe_offset]:1;
		float bitsV = (x+f-vframe_offset>=0 && x+f-vframe_offset < FRAMECT )?bitsperframe_video[x+f-vframe_offset]:1;
		float bits = bitsA + bitsV;
		if( bits > fMaxBits ) fMaxBits = bits;
	}

	int gotcframe = 0;

	for( x = 0; x < frameWidth; x++ )
	{
		//if( x+f-vframe_offset >= FRAMECT ) continue;
		//if( x+f-vframe_offset < 0 ) continue;
		float bitsA = (x+f-vframe_offset>=0 && x+f-vframe_offset < FRAMECT )?bitsperframe_audio[x+f-vframe_offset]:1;
		float bitsV = (x+f-vframe_offset>=0 && x+f-vframe_offset < FRAMECT )?bitsperframe_video[x+f-vframe_offset]:1;
		float bits = bitsA + bitsV;

		CNFGColor( 0x808080ff );
		CNFGTackSegment( b.x + x*baseZoom, b.y+b.height-bitsA/fMaxBits*b.height, b.x+x*baseZoom, b.y+b.height );
		CNFGColor( 0xc0c0c0ff );
		CNFGTackSegment( b.x + x*baseZoom, b.y+b.height-(bits)/fMaxBits*b.height, b.x+x*baseZoom, b.y+b.height-bitsA/fMaxBits*b.height );
		if( ( inBox(b) || has_down_focus ) && (int)(x) == (int)(cursor_rel.x/baseZoom) )
		{
		    Clay_LayoutElement *openLayoutElement = Clay__GetOpenLayoutElement();
			CNFGColor( 0xffffffff );
			CNFGTackSegment( b.x + x*baseZoom, b.y, b.x+x*baseZoom, b.y+b.height-(bits)/fMaxBits*b.height );
			if( has_down_focus && x+f >= 0 && x+f-vframe_offset < FRAMECT )
			{
				inPlayMode = 0;
				topCursor = cursor = checkpoint_offset_by_frame_virtual[x+f];
			}
		}

		int thiscframe = (x+f>=0)?checkpoint_offset_by_frame_virtual[x+f]:0;
		if( thiscframe >= cursor && !gotcframe )
		{
			gotcframe = 1;
			CNFGColor( 0xffffff30 );
			CNFGTackSegment( b.x + x*baseZoom, b.y, b.x+x*baseZoom, b.y+b.height );
		}
	}
}

void DrawBottomGraph( Clay_RenderCommand * render )
{
	if( !checkpoints ) return;
	Clay_BoundingBox b = render->boundingBox;
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };
	float nindex = 0;
	float iadv = vframe/b.width;
	int index = 0;
	float x;
	int bh = b.height;
	int bhm = b.height * 1.0;
	static float mbv = 1.0;
	static int has_down_focus;

	if( mouseDownThisFrame ) has_down_focus = inBox(b);
	if( !isMouseDown ) has_down_focus = false;


	int gotcframe = 0;

	for( x = 0; x < b.width; x++ )
	{
		nindex += iadv;
		float bitsA = 0;
		float bitsV = 0;
		for( ; index < nindex; index++ )
		{
			bitsA += (index-vframe_offset>=0 && index-vframe_offset < FRAMECT)?bitsperframe_audio[index-vframe_offset]:10;
			bitsV += (index-vframe_offset>=0 && index-vframe_offset < FRAMECT)?bitsperframe_video[index-vframe_offset]:10;
		}
		CNFGColor( 0x808080ff );
		CNFGTackSegment( b.x + x, b.y+bh-bitsA/mbv*bhm, b.x+x, b.y+bh );
		CNFGColor( 0xc0c0c0ff );
		CNFGTackSegment( b.x + x, b.y+bh-(bitsA+bitsV)/mbv*bhm, b.x+x, b.y+bh-bitsA/mbv*bh );
		if( ( inBox(b) || has_down_focus ) && (int)x == (int)cursor_rel.x )
		{
		    Clay_LayoutElement *openLayoutElement = Clay__GetOpenLayoutElement();
			CNFGColor( 0xffffffff );
			CNFGTackSegment( b.x + x, b.y, b.x+x, b.y+bh-(bitsA+bitsV)/mbv*bhm );
			if( has_down_focus )
			{
				inPlayMode = 0;
				topCursor = midCursor = cursor = checkpoint_offset_by_frame_virtual[(int)nindex];
			}
		}
		if( bitsA+bitsV > mbv ) mbv = bitsA+bitsV;

		int thiscframe = checkpoint_offset_by_frame_virtual[index];
		if( thiscframe >= cursor && !gotcframe )
		{
			gotcframe = 1;
			CNFGColor( 0xffffff30 );
			CNFGTackSegment( b.x+x, b.y, b.x+x, b.y+bh );
		}
	}
}

void DrawMemory( Clay_RenderCommand * render )
{
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];

	const uint8_t * base = cp->decoding_glyphs?ba_glyphdata:ba_video_payload;
	vpx_reader * v = cp->baplay_vpx;
	if( !v ) return;
	int dlen = v->buffer_end - base;

	float fx = b.x;
	float fy = b.y;

	int bo;
	float pairsize = 32;
	int bol = b.width / pairsize;
	int bofs = 0;
	for( bo = -bol/2; bo < bol/2; bo++ )
	{
		int tp = bo + v->buffer - base;
		if( tp >= 0 && tp < dlen )
			DrawFormat( bofs*pairsize+fx+pairsize/2, fy+3, -2, 0xffffffff, "%02x", base[tp] );
		if( bo == 0 )
		{
			CNFGColor( 0xffffffff );
			CNFGDrawBox( bofs*pairsize+fx, fy, bofs*pairsize+fx+pairsize, fy + pairsize-7 );
			CNFGTackSegment( bofs*pairsize+fx-pairsize*3, fy + pairsize-7, bofs*pairsize+fx-5, fy + pairsize-7 );
		}
		bofs++;
	}
}

void DrawVPX( Clay_RenderCommand * render )
{
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];
	vpx_reader * v = cp->baplay_vpx;
	if( !v ) return;

	float fx = b.x;
	float fy = b.y;
	char vs[33] = { 0 };

	int i;

	if( cp->vpxcheck )
	{
		for( i = 0; i < 32; i++ )
		{
			sprintf( vs + i, "%01x", (((i<8)?v->value:cp->vpxcpv) >> (31-i)) &1 );
		}
	}
	else
	{
		for( i = 0; i < 32; i++ )
		{
			int vc = v->count;
			if( vc < 0 ) vc = 0;
			if( i >= vc + 8 )
			{
				if( v->count < 0 )
					sprintf( vs + i, "x" );
				else
					sprintf( vs + i, "." );
			}
			else
				sprintf( vs + i, "%01x", (v->value >> (31-i)) &1 );
		}
		//for( ; i < 25; i++ ) vs[i] = ' ';
	}

	DrawFormat( fx+b.width/2-8, fy+4, -2, cp->vpxcheck?0x606060ff:0xffffffff, "%32s %3d", vs, v->range );
}

void DrawCellState( Clay_RenderCommand * render )
{
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];
	vpx_reader * v = cp->baplay_vpx;
	if( !v ) return;

	float fx = b.x;
	float fy = b.y;
	char vs[37] = { 0 };

	int i;

	if( cp->decodephase == "Running" || cp->decodephase == "Run Stopped" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "G:%02x CLS:%02x RUN:%3d",
			cp->decode_fromglyph, cp->decode_class, cp->decode_run );


		DrawFormat( fx+b.width/2-8, fy+4+24, -2, 0xffffffff, "%4.1f%% Got Bit:%d -> %s", 100.0-cp->decode_prob/2.55f, cp->decode_lb, cp->decodephase );
	}
	else if( cp->decodephase == "Decoding Bit" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "G:%02x CLS:%02x LEVEL:%3d",
			cp->decode_fromglyph, cp->decode_class, cp->decode_level );


//				CHECKPOINT( decode_tileid = tile, decode_lb = bit, vpxcheck = 0, decode_level = level, decode_prob = probability, decodephase = "Decoding Bit" );

		DrawFormat( fx+b.width/2-8, fy+4+24, -2, 0xffffffff, "%4.1f%% Got Bit:%d -> %02x", 100.0-cp->decode_prob/2.55f, cp->decode_lb, cp->decode_tileid );

		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase );
	}
	else
	{
		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase );
	}
}

void DrawCellStateAudio( Clay_RenderCommand * render )
{
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];
	vpx_reader * v = cp->baplay_vpx;
	if( !v ) return;

	float fx = b.x;
	float fy = b.y;
	char vs[37] = { 0 };

	int i;
#if 0
	if( cp->decodephase == "Running" || cp->decodephase == "Run Stopped" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "G:%02x CLS:%02x RUN:%3d",
			cp->decode_fromglyph, cp->decode_class, cp->decode_run );


		DrawFormat( fx+b.width/2-8, fy+4+24, -2, 0xffffffff, "%4.1f%% Got Bit:%d -> %s", cp->decode_prob/2.55f, cp->decode_lb, cp->decodephase );
	}
	else if( cp->decodephase == "Decoding Bit" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "G:%02x CLS:%02x LEVEL:%3d",
			cp->decode_fromglyph, cp->decode_class, cp->decode_level );


//				CHECKPOINT( decode_tileid = tile, decode_lb = bit, vpxcheck = 0, decode_level = level, decode_prob = probability, decodephase = "Decoding Bit" );

		DrawFormat( fx+b.width/2-8, fy+4+24, -2, 0xffffffff, "%4.1f%% Got Bit:%d -> %02x", cp->decode_prob/2.55f, cp->decode_lb, cp->decode_tileid );

		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase );
	}
	else
#endif
	{
		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase );
	}
}

void DrawGeneral( Clay_RenderCommand * render )
{
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];

	float fx, fy, fzoom;

	fx = b.x;
	fy = b.y;
	fzoom = CLAY__MIN( b.width / RESX, b.height / RESY );

	EmitSamples8( cp, fx, fy, fzoom, (glyphtype *)cp->curmap, (void*)cp->glyphdata );
}

int WXPORT(main)()
{
	int x, y;
	short w, h;
	CNFGSetup( "Badder Apple", 1920/2, 1080/2 );
	ExtraDrawingInit( 1920/2, 1080/2 );

#ifdef VPX_GREY4
	static uint8_t palette[48] = { 0, 0, 0, 85, 85, 85, 171, 171, 171, 255, 255, 255 };
#elif defined( VPX_GREY3 )
	static uint8_t palette[48] = { 0, 0, 0, 128, 128, 128, 255, 255, 255 };
#endif

	ba_play_setup( &ctx );
	ba_audio_setup();

	int outbuffertail = 0;
	int lasttail = 0;
	double Now = OGGetAbsoluteTime();
	double Last = Now;
	while( CNFGHandleInput() )
	{
		Now = OGGetAbsoluteTime();
		double dTime = Now - Last;
		Last = Now;
		if( frame < FRAMECT )
		{

			if( ba_play_frame( &ctx ) ) break;


			frame++;

			CHECKPOINT( decodephase = "Frame Done", decode_cellid = -1 );
			lasttail = outbuffertail;

			outbuffertail = (AS_PER_FRAME + outbuffertail) % AUDIO_BUFFER_SIZE;
			ba_audio_fill_buffer( out_buffer_data, outbuffertail );

			struct checkpoint * cp = &checkpoints[nrcheckpoints-1];
			uint8_t * ad = cp->audio_sample_data = malloc( AS_PER_FRAME );
			cp->audio_sample_data_frame = nrcheckpoints;
			for( int n = lasttail; n != outbuffertail; n = (n+1)%AUDIO_BUFFER_SIZE )
			{
				*(ad++) = out_buffer_data[n];
				float fo = out_buffer_data[n] / 128.0 - 1.0;
				//fwrite( &fo, 1, 4, fAudioDump );
			}
		}
		CNFGClearFrame();
		Clay_SetPointerState((Clay_Vector2) { mousePositionX, mousePositionY }, isMouseDown);
		Clay_BeginLayout();

		int padding = 4;
		int paddingChild = 4;

		LayoutStart();
		{
			CLAY({ .id = CLAY_ID("OuterContainer"), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_BACKGROUND })
			{

				CLAY({
					.id = CLAY_ID("Top Bar"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf_g( 1, "Badder Apple" ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}
					int tframe = checkpoints?checkpoints[cursor].frame:0;
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } /*, .backgroundColor = COLOR_PADGREY */ } )
					{
						CLAY_TEXT(saprintf( "%d/%d (Frame %d)", cursor, nrcheckpoints, tframe ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } /*, .backgroundColor = COLOR_PADGREY */} )
					{
						CLAY_TEXT(saprintf( "A:%3.0f b, V:%3.0f b", bitsperframe_audio[tframe], bitsperframe_video[tframe] ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf_g( (frame < FRAMECT), "Dec %d", frame), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					}
				}

				CLAY({
					.id = CLAY_ID("Main Body"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					struct checkpoint * cp = 0;
					if( cursor >= 0 && cursor < nrcheckpoints )
					{
						cp = &checkpoints[cursor];
					}

					CLAY({
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
						.backgroundColor = COLOR_PADGREY
					})
					if( cp && cp->decodephase )
					{
						if( cp->decodephase && strncmp( cp->decodephase, "AUDIO", 5 ) == 0 )
						{
							CLAY({ .custom = { .customData = DrawCellStateAudio } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}
						}
						else
						{
							CLAY({ .custom = { .customData = DrawMemory } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}
							CLAY({ .custom = { .customData = DrawVPX } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}
							CLAY({ .custom = { .customData = DrawCellState } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}
						}
					}
					else
					{
						// Null. Can't draw a side info bar for this.
					}

					CLAY({ .custom = { .customData = DrawGeneral } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(300), .height = CLAY_SIZING_GROW(300) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					{
						CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

				}
#if 0
				CLAY({
					.id = CLAY_ID("Bottom Mid"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{

				}

#endif

				CLAY({
					.id = CLAY_ID("Mid Bottom Bar"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked ) { if( cursor > 0 ) cursor--; topCursor = cursor; }

					CLAY({ .custom = { .customData = DrawTopGraph } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					{
						CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING(">"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked ) { if( cursor < nrcheckpoints-1 ) cursor++; topCursor = cursor; }
				}


				CLAY({
					.id = CLAY_ID("Bottom Bar"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("<<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { int tframe = checkpoints?checkpoints[cursor].frame:0; for( ; cursor > 0; cursor-- ) if( checkpoints[cursor].frame < tframe - 150 ) { break; } midCursor = topCursor = cursor; }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("|<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { int tframe = checkpoints?checkpoints[cursor].frame:0; for( ; cursor > 0; cursor-- ) if( checkpoints[cursor].frame != tframe ) { break; } midCursor = topCursor = cursor; }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("\x0f"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { inPlayMode = 0; }

					CLAY({ .custom = { .customData = DrawMidGraph } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					{
						CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("\x10"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { inPlayMode = !inPlayMode; }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING(">|"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { int tframe = checkpoints[cursor].frame; int k = cursor; for( ; k < nrcheckpoints; k++ ) if( checkpoints[k].frame != tframe ) { for( ; k < nrcheckpoints; k++ ) if( checkpoints[k].frame == tframe+1 ) cursor = k; else break; break; } midCursor = topCursor = cursor; }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING(">>"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { int tframe = checkpoints[cursor].frame; int k = cursor; for( ; k < nrcheckpoints; k++ ) if( checkpoints[k].frame > tframe + 150 ) { cursor = k-1; break; } midCursor = topCursor = cursor; }
				}

				CLAY({
					.id = CLAY_ID("Final Bottom Bar Holder"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) } },
					.backgroundColor = COLOR_PADGREY
				})
				CLAY({
					.id = CLAY_ID("Final Bottom Bar"),
					.custom = { .customData = DrawBottomGraph },
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(32), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
				}

			}
		}


		if( inPlayMode )
		{
			fFrameElapse += dTime;
			if( fFrameElapse > 1.0/30.0 )
			{
				if( fFrameElapse > 3.0/30.0 ) fFrameElapse = 3.0/30.0;
				fFrameElapse -= 1.0/30.0;
				int tFrame = checkpoints[cursor].frame-1;
				if( tFrame + 1 < FRAMECT && checkpoint_offset_by_frame[tFrame+1]+1 > 0 && checkpoint_offset_by_frame[tFrame+1]+1 < nrcheckpoints 	)
					midCursor = topCursor = cursor = checkpoint_offset_by_frame[tFrame+1]+1;
			}
		}


		// All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
		Clay_RenderCommandArray renderCommands = Clay_EndLayout();


		// More comprehensive rendering examples can be found in the renderers/ directory
		for (int i = 0; i < renderCommands.length; i++) {
			Clay_RenderCommand *renderCommand = &renderCommands.internalArray[i];
			switch (renderCommand->commandType) {
				case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
					DrawRectangle( renderCommand->boundingBox, renderCommand->renderData.rectangle.backgroundColor);
					break;
				case CLAY_RENDER_COMMAND_TYPE_TEXT:
					DrawTextClay( renderCommand );
					break;
				case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
					((void (*)( Clay_RenderCommand *))(renderCommand->renderData.custom.customData))(renderCommand);
					break;
				case CLAY_RENDER_COMMAND_TYPE_NONE:
				case CLAY_RENDER_COMMAND_TYPE_BORDER:
				case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
				case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
					break;
			}
		}

		//DrawFormat( 50, 200, 2, 0xffffffff, "Test %d\n", frame );
		CNFGGetDimensions( &w, &h );
		Clay_SetLayoutDimensions((Clay_Dimensions) { w, h });

		CNFGSwapBuffers();
	}
	return 0;
}



