#include <stdio.h>

#define TITLE "Badder Apple"

#define WARNING(x...) printf( x );
#define CHECKPOINT(x...) { x; ba_i_checkpoint(); }
#define CHECKBITS_AUDIO(x) { bitsperframe_audio[frame] += x;}
#define CHECKBITS_VIDEO(x) { bitsperframe_video[frame] += x;}

int is_vertical = 0;
int frame = 0;
short screenw, screenh;

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
	x(audio_pullbit) x(audio_gotbit) x(audio_last_bitmode) x(audio_bpr) \
	x(audio_golmb_exp) x(audio_golmb_v) x(audio_golmb_br) x(audio_golmb) x(audio_last_ofs) \
	x(audio_last_he) x(audio_pullhuff) x(audio_stack_place) x(audio_stack_remain) \
	x(audio_stack_offset) x(audio_backtrace) x(audio_newnote) x(audio_lenandrun) \
	x(audio_gotnote) x(audio_backtrack_offset) x(audio_backtrack_runlen) \
	x(audio_backtrack_remain) x(audio_sixteenth)

#define xcomma(y) y,
int FIELDS(xcomma) dummy;

#define GLYPH_DONE_DECODE EarlyGlyphDecodeFrameDone

void EarlyGlyphDecodeFrameDone( int frame );

#include "ba_play.h"

#ifdef __wasm__
#define F_SPS 44100
#else
#define F_SPS 48000
#endif
#define AUDIO_BUFFER_SIZE 2048

int AS_PER_FRAME = F_SPS/30;

#include "ba_play_audio.h"


#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"


uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
ba_play_context ctx;

uint64_t * audio_notes_playing_by_sixteenth;
int highest_sixteenth;
int * audio_notes_playing_by_sixteenth_to_cpid;
float * audioOut = 0;

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

	float * audio_play;

	const char * decodephase;
	int FIELDS(xcomma) dummy;

} * checkpoints;

int nrcheckpoints;
int inPlayMode;
int ShowHelp = 0;
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

	cp->audio_play = audioOut;

	if( cp->frameChanged ) vframe++;
	if( frame == 0 && ( nrcheckpoints % 500 == 0 ) && vframe < MAX_PREFRAMES ) vframe++;
	if( frame && !vframe_offset ) vframe_offset = vframe-1;
	cp->vframe = vframe;
	cp->decodephase = decodephase;

	#define xassign(tf) cp->tf = tf;
	FIELDS(xassign);

	if( audio_sixteenth >= highest_sixteenth )
	{
		audio_notes_playing_by_sixteenth = realloc( audio_notes_playing_by_sixteenth, (audio_sixteenth+1) * sizeof( audio_notes_playing_by_sixteenth[0] ) );
		audio_notes_playing_by_sixteenth_to_cpid = realloc( audio_notes_playing_by_sixteenth_to_cpid, (audio_sixteenth+1) * sizeof( audio_notes_playing_by_sixteenth_to_cpid[0] ) );
		memset( &audio_notes_playing_by_sixteenth[highest_sixteenth+1], 0, sizeof( audio_notes_playing_by_sixteenth[0] ) * (audio_sixteenth-audio_sixteenth) );
		memset( &audio_notes_playing_by_sixteenth_to_cpid[highest_sixteenth+1], 0, sizeof( audio_notes_playing_by_sixteenth_to_cpid[0] ) * (audio_sixteenth-audio_sixteenth) );
		highest_sixteenth = audio_sixteenth+1;
	}

	audio_notes_playing_by_sixteenth[audio_sixteenth] = 
		(((uint64_t)ba_player.playing_freq[0])<<0) |
		(((uint64_t)ba_player.playing_freq[1])<<16) |
		(((uint64_t)ba_player.playing_freq[2])<<32) |
		(((uint64_t)ba_player.playing_freq[3])<<48);
	audio_notes_playing_by_sixteenth_to_cpid[audio_sixteenth] = nrcheckpoints;

	nrcheckpoints++;
	//printf( "NRC: %d\n", nrcheckpoints );
}

#include "treepres.h"
struct treepresnode * HuffNoteNodes;
int HuffNoteNodeCount;
struct treepresnode * HuffLenRunNodes;
int HuffLenRunNodeCount;

double dTime;

float timelerp( float cur, float new, float speed )
{
	// See https://github.com/cnlohr/cnlohr_tricks/?tab=readme-ov-file#iir-filtering
	float coeff = exp( -dTime * speed );
	return cur * (1.0-coeff) + new * coeff;
}

int digits( int num )
{
	int dig = 0;
	do
	{
		dig++;
		num/=10;
	} while( num );
	return dig;
}

int AudioEnabled = false;

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
				//if( bx > 0 )
				{
					graphictype tgnext = g[1];
					graphictype tg = g[0];
					graphictype tgprev = (bx > 0 ) ? glyphdata[gm[gmi-1]][7] : tgnext;

					EmitPartial( tgprev, tg, tgnext, subframe );
					sx = 1;
				}
				int sxend = 7;// (bx < RESX/BLOCKSIZE-1) ? 7: 8;
				for( ; sx < sxend; sx++ )
				{
					uint16_t tg = g[sx];
					KOut( tg >> (subframe*8) );
				}
				//if( sx < 8 )
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

	uint8_t raw_icon_data[RESX*RESY/2] = { 0 };

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

		if( vi > 2 ) vi = 2;
		if( vi < 0 ) vi = 0;
		raw_icon_data[((RESY-y-1)*RESX + x)/2] |= (vi)<<(!(x&1)*4);
	}

#ifdef __wasm__
	// Only update favicon every other frame.
	static int dispframe;
	if( dispframe++ & 1 )
	{
		void ChangeFavicon( uint8_t * raw_icon_data, int w, int h );
		ChangeFavicon( raw_icon_data, RESX, RESY );
	}
#endif

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
	const uint8_t * vb = v->buffer ? v->buffer : base;
	int dlen = v->buffer_end - base;

	float fx = b.x;
	float fy = b.y;

	int bo;
	float pairsize = 32;
	int bol = b.width / pairsize;
	int bofs = 0;
	for( bo = -bol/2; bo < bol/2; bo++ )
	{
		int tp = bo + vb - base;
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
	char vs[25] = { 0 };

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

//	printf( "%08x\n", v->value );

	// Skip first 8 bits because they _are_ the range.
	DrawFormat( fx+b.width/2-8, fy+4, -2, cp->vpxcheck?0x606060ff:0xffffffff, "%3d\x1b%s %3d", v->value>>24, vs + 8, v->range );
}

void DrawMemoryAndVPX( Clay_RenderCommand * render )
{
	render->boundingBox.height *= 0.45;
	DrawMemory( render );
	render->boundingBox.y += 39;
	DrawVPX( render );
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
		float fPerthou = (100.0-cp->decode_prob/2.55f)*10.0;
		int perh = ((int)fPerthou/10);
		int perd = (int)(((int)fPerthou)%10);
		DrawFormat( fx+b.width/2-8, fy+4+24, -2, 0xffffffff, "%3d.%1d%% Got Bit:%d -> %s", perh, perd, cp->decode_lb, cp->decodephase?cp->decodephase:"(Unknown)" );
	}
	else if( cp->decodephase == "Decoding Bit" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "G:%02x CLS:%02x LEVEL:%3d",
			cp->decode_fromglyph, cp->decode_class, cp->decode_level );
		float fPerthou = (100.0-cp->decode_prob/2.55f)*10.0;
		int perh = ((int)fPerthou/10);
		int perd = (int)(((int)fPerthou)%10);


		char tilestr[BITS_FOR_TILE_ID+1] = { 0 };
		for( i = 0; i < BITS_FOR_TILE_ID; i++ )
		{
			tilestr[i] = 
				(i <= cp->decode_level) ?
				(((cp->decode_tileid>>(BITS_FOR_TILE_ID-i-1))&1)?'1':'0') :
				'x';
		}
		DrawFormat( fx+b.width/2-8, fy+4+24, -2, 0xffffffff, "%3d.%1d%% Got Bit:%d -> %s (%02x)", perh, perd, cp->decode_lb, tilestr, cp->decode_tileid );
		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase?cp->decodephase:"(Unknown)");
	}
	else if( cp->decodephase == "Frame Done" )
	{
		DrawFormat( fx+b.width/2-8, fy+4+24*0, -2, 0xffffffff, "Frame: %d", cp->frame );

		const uint8_t * base = cp->decoding_glyphs?ba_glyphdata:ba_video_payload;
		vpx_reader * v = cp->baplay_vpx;
		if( base && v )
		{
			const uint8_t * vb = v->buffer ? v->buffer : base;
			int dlen = v->buffer_end - base;
			float percent = 100.0*(vb-base) /sizeof(ba_video_payload);
			DrawFormat( fx+b.width/2-8, fy+4+24*1, -2, 0xffffffff, "Video Byte %d/%d %d.%1d%%", vb-base, sizeof(ba_video_payload), (int)percent, (int)(percent*10)%10 );
		}
	}
	else if( cp->decodephase == "Reading Glyphs" )
	{
		//decodeglyph = g, decode_is0or1 = is0or1, decode_runsofar = runsofar, decode_prob = tprob, decode_lb = lb, vpxcheck = 0
		DrawFormat( fx+b.width/2-8, fy+4+24*0, -2, 0xffffffff, "Read Pixel: %d", cp->decodeglyph );
		DrawFormat( fx+b.width/2-8, fy+4+24*1, -2, 0xffffffff, "Was:%d Run:%d Prob:%d, Bit:%d", 
			cp->decode_is0or1,  cp->decode_runsofar, cp->decode_prob, cp->decode_lb );

	}
	else
	{
		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase?cp->decodephase:"(Unknown)" );
	}
}

void DrawAudioStack( struct checkpoint * cp, int x, int y, int w, int h )
{
	if( !cp->audio_stack ) return;
	int k;
	int maxpt = 0;
	const int song_length_bits = sizeof( espbadapple_song_data ) * 8;
	//struct ba_audio_player_stack_element * sp = &(*cp->audio_stack)[cp->audio_stack_place];
	for( k = 0; k <= cp->audio_stack_place; k++ )
	{
		struct ba_audio_player_stack_element * s = &(*cp->audio_stack)[k];
		if( s->offset > maxpt )
		{
			maxpt = s->offset;
		}
	}

	const int scrollalong = 0;

	int margin = 2;
	float pxscale = scrollalong ? 1 : ((w-margin)/(float)song_length_bits);
	float wm = (w - margin)/pxscale;
	float center = scrollalong ? cp->audio_bpr : song_length_bits/2;
	int vx;
	for( vx = center-wm/2; vx < center + wm/2; vx++ )
	{
		int bp = (vx-center+wm/2)*pxscale+margin;
		if( vx < 0 ) continue;
		if( vx >= song_length_bits ) break;
		uint32_t v = espbadapple_song_data[vx>>5];
		int bit = (v>>(vx&31))&1;
		CNFGColor( bit?0xf0f0f080 : 0x10101080 );
		CNFGTackSegment( bp+x-1, h-10+y, bp+x-1, h-2+y );
	}
	CNFGColor( 0xf0f0f0ff );
	CNFGTackSegment( wm/2+x, h-12+y, wm/2+x, h-10+y );

	int step = 1;
	for( k = cp->audio_stack_place; k >= 0; k-- )
	{
		struct ba_audio_player_stack_element * s = &(*cp->audio_stack)[k];
		//struct ba_audio_player_stack_element * sm1 = &(*cp->audio_stack)[k-1];
		int bp = (s->offset-center+wm/2)*pxscale;
		int bpm1 = bp+10;
		if( bp >= w ) bp = w-1;
		if( bpm1 >= w ) bpm1 = w-1;
		CNFGTackSegment( x+bp, h-12-step*10+y, x+bpm1, h-12-step*10+y );
		CNFGTackSegment( x+bp, h-12-step*10+y, x+bp, h-8-step*10+y );

		int otherside = ( bp < 40 ) ? digits(s->remain)*7+12 : 0;
		DrawFormat( x+bp-digits(s->remain)*7+otherside, h-16-step*11+y, 1, 0xffffffff, "%d", s->remain );
		CNFGSetLineWidth(2);
		step++;
	}

	char bitstream_prev[5] = { 0 };
	char bitstream_this[2] = { 0 };
	char bitstream[120] = { 0 };
	
	int i;
	for( i = 0; i < sizeof(bitstream_prev)-1; i++ )
	{
		int vx = cp->audio_bpr + i - sizeof(bitstream_prev) + 1;
		if( vx >= 0 )
		{
			uint32_t v = espbadapple_song_data[vx>>5];
			int bit = (v>>(vx&31))&1;
			bitstream_prev[i] = '0' + bit;
		}
		else
		{
			bitstream_prev[i] = ' ';
		}
	}
	for( i = 0; i < sizeof(bitstream_this)-1; i++ )
	{
		int vx = cp->audio_bpr + i;
		if( vx >= sizeof(espbadapple_song_data)*8 )
		{
			bitstream_this[i] = sizeof(espbadapple_song_data)*8 == vx ? '.' : ' ';;
		}
		else
		{
			uint32_t v = espbadapple_song_data[vx>>5];
			int bit = (v>>(vx&31))&1;
			bitstream_this[i] = '0' + bit;
		}
	}
	for( i = 0; i < sizeof(bitstream)-1; i++ )
	{
		int vx = cp->audio_bpr + i + sizeof(bitstream_this)-1;
		if( vx >= sizeof(espbadapple_song_data)*8 )
		{
			bitstream[i] = sizeof(espbadapple_song_data)*8 == vx ? '.' : ' ';
		}
		else
		{
			uint32_t v = espbadapple_song_data[vx>>5];
			int bit = (v>>(vx&31))&1;
			bitstream[i] = '0' + bit;
		}
	}

	int xofs = 6*(sizeof(bitstream_prev)+1)+4+15;
	int xofs2 = 6*(sizeof(bitstream_prev)+1)+21+15;
	DrawFormat( x + 2, y+4, 2, 0xffffffff, "%s", bitstream_prev );
	DrawFormat( x + 2+xofs, y+4, 2, 0xffffffff, "%s", bitstream_this );
	DrawFormat( x + 2+xofs2, y+4, 2, 0xffffff80, "%s", bitstream );
	CNFGColor( 0xffffffff );
	CNFGDrawBox( x+xofs-3, y + 1, x+12+xofs+3, y+25 );


//	printf( "%d %d %d\n", cp->audio_stack_place, cp->audio_stack_remain );
		// audio_stack_place = stackplace, audio_stack_remain = player->stack[stackplace].remain
		//struct ba_audio_player_stack_element (*audio_stack)[ESPBADAPPLE_SONG_MAX_BACK_DEPTH];
}

int NeedsHuffman()
{
	struct checkpoint * cp = &checkpoints[cursor];
	return cp->decodephase == "AUDIO: Reading Note" || cp->decodephase == "AUDIO: Reading Length and Run";
}

int NeedsExpGolomb()
{
	struct checkpoint * cp = &checkpoints[cursor];
	return cp->decodephase == "AUDIO: Backtrack" || cp->decodephase == "AUDIO: Reading Backtrack Run Length" || cp->decodephase == "AUDIO: Reading Backtrack Offset";
}

void DrawCellStateAudioExpGolomb( Clay_RenderCommand * render )
{
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGFlushRender();

	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];

	//audio_golmb_exp
	//audio_golmb_br

	char gexp[64] = { 0 };
	int i;
	for( i = 0; i < cp->audio_golmb_exp+1; i++ )
	{
		if( cp->audio_golmb_br >= i )
		{
			gexp[i] = ((cp->audio_golmb_v>>(cp->audio_golmb_br - (i)))&1) ? '1' : '0';
		}
		else
		{
			gexp[i] = '.';
		}
	}

	DrawFormat( b.x + b.width/2, b.y+2, -2, 0xffffffff, "Exp Golomb: %s %3d", gexp, cp->audio_golmb_v - 1 );
}

void DrawAudioTrack( Clay_RenderCommand * render )
{
	int original_scissors_box[4];
	CNFGGetScissors( original_scissors_box );
	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGFlushRender();

	CNFGSetScissors( (int[4]){ (int)b.x, screenh-((int)b.y+(int)b.height), (int)b.width, (int)b.height } );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];
	if( !cp ) goto ending;

	float fx = b.x;
	float fy = b.y;

	CNFGColor( 0xc0c0c090 );

	int sxth;
	float sper = b.width / 21.0;
	float xst = b.x + sper * 10;
	float yst = b.height/2.5;

	int center_audio_sixteenth = cp->audio_sixteenth;

	// Microscrolling
	float partial = 0.0;
	if( highest_sixteenth > center_audio_sixteenth && center_audio_sixteenth > 0 )
	{
		int thisid = audio_notes_playing_by_sixteenth_to_cpid[center_audio_sixteenth-1];
		int nextid = audio_notes_playing_by_sixteenth_to_cpid[center_audio_sixteenth];
		partial = (cursor - thisid)/(float)(nextid - thisid);
	}

	CNFGSetLineWidth(1.0);

	CNFGColor( 0xc0c0c010 );
	for( sxth = center_audio_sixteenth-10; sxth <= center_audio_sixteenth+11; sxth++ )
	{
		if( sxth < 0 ) continue;
		if( sxth >= highest_sixteenth ) continue;

		float relpos = sxth - cp->audio_sixteenth - partial;
		
		CNFGTackSegment( xst + sper*relpos, b.y, xst + sper*relpos, b.y+b.height );
	}
	CNFGSetLineWidth(2.0);

	uint16_t * apf = cp->audio_playing_freq ? *cp->audio_playing_freq : 0;
	int * apfs = cp->audio_tstop ? *cp->audio_tstop : 0;

	for( sxth = center_audio_sixteenth-10; sxth <= center_audio_sixteenth+11; sxth++ )
	{
		if( sxth < 0 ) continue;
		if( sxth >= highest_sixteenth ) continue;
		uint64_t sixteenth = audio_notes_playing_by_sixteenth[sxth];
		int n = 0;
		for( n = 0; n < 4; n++ )
		{
			int fr = (sixteenth >> (16*n))&0xffff;
			if( fr )
			{
				float note = 7.1-log(fr);
				float relpos = sxth - cp->audio_sixteenth - partial;
				CNFGColor( ( sxth > center_audio_sixteenth ) ? 0xf0f0f018 : 0xc0c0c0b0 );
				CNFGTackRectangle( xst + sper*relpos, b.y + yst * note, xst + sper*(relpos+1), b.y + yst * note + 10 );
			}

			if( apf && cp->audio_sixteenth == sxth )
			{
				int stop = apfs[n];
				int fr = apf[n];

				if( !fr ) continue;
				float note = 7.1-log(fr);
				float relpos = sxth - cp->audio_sixteenth - partial;
				CNFGColor( 0xffffffff );
				CNFGTackRectangle( xst + sper*relpos, b.y + yst * note, xst + sper*(relpos+stop-sxth), b.y + yst * note + 10 );
			}
		}
	}

	CNFGTackSegment( xst, b.y, xst, b.y+b.height );


	DrawAudioStack( cp, fx, fy, b.width, b.height );		

ending:
	CNFGFlushRender();

	CNFGSetScissors( original_scissors_box );
}

void DrawCellStateAudioHuffman( Clay_RenderCommand * render )
{
	static float cx;
	static float cy;

	int original_scissors_box[4];
	CNFGGetScissors( original_scissors_box );

	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGFlushRender();

	CNFGSetScissors( (int[4]){ (int)b.x, screenh-((int)b.y+(int)b.height), (int)b.width, (int)b.height } );

	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];

	float fx = b.x;
	float fy = b.y;

	int isnote = cp->decodephase == "AUDIO: Reading Note"; // Otherwise length-and-run.

	//DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "HUFFMAN DRAW %d", isnote );

	struct treepresnode * tree = (isnote)?HuffNoteNodes:HuffLenRunNodes;
	int ct = (isnote)?HuffNoteNodeCount:HuffLenRunNodeCount;

	// TODO: Update cx, cy with position.

	int i;
	for( i = 0; i < ct; i++ )
	{
		struct treepresnode * n = tree + i;
		float ox = n->x + b.width/2;
		float oy = n->y + b.height/2;


		ox -= cx;
		oy -= cy;

		struct treepresnode * c = n->child[0];
		if( c )
		{
			CNFGColor( 0x000000ff );
			CNFGTackSegment( b.x + ox, b.y + oy, b.x + c->x + b.width/2 - cx, b.y + c->y + b.height/2 - cy );
		}
		c = n->child[1];
		if( c )
		{
			CNFGColor( 0xc0c0c080 );
			CNFGTackSegment( b.x + ox, b.y + oy, b.x + c->x + b.width/2 - cx, b.y + c->y + b.height/2 - cy );
		}
		CNFGColor( (i == 0 ) ? 0x00000080 : 0xf0f0f080 );
		CNFGTackRectangle( b.x + ox - 14, b.y + oy - 12, b.x + ox + 14, b.y + oy + 12);
		DrawFormat( b.x + ox, b.y + oy - 10, -2, 0xffffffff, n->label );

		if( i == cp->audio_last_ofs || ( cp->audio_pullhuff && cp->audio_pullhuff == n->value ) )
		{
			cx = timelerp( n->x, cx, 5 );
			cy = timelerp( n->y, cy, 5 );
			CNFGColor( 0xffffffff );
			CNFGDrawBox( b.x + ox - 14, b.y + oy - 12, b.x + ox + 14, b.y + oy + 12);
		}
	}
	CNFGFlushRender();

	CNFGSetScissors( original_scissors_box );
}

void DrawCellStateAudioStack( Clay_RenderCommand * render )
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

	DrawAudioStack( cp, fx, fy, b.width, b.height );		
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

	fx += 2; // Add some margin.

	int i;
	if( cp->decodephase == "AUDIO: Pulling Note" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Pulling new note" );
	}
	else if( cp->decodephase == "AUDIO: Reading Next" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Reading Next" );
	}
	else if( cp->decodephase == "AUDIO: No Backtrack" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "No Backtrack" );
	}
	else if( cp->decodephase == "AUDIO: Reading Note" )
	{
		// This is a multi-part reading.
		// This will also include a huffman table.
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Reading Note" );
	}
	else if( cp->decodephase == "AUDIO: Perform 16th Note" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Audio Tick" );
	}
	else if( cp->decodephase == "AUDIO: Processed Note" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Processed Note" );
	}
	else if( cp->decodephase == "AUDIO: Reading Length and Run" )
	{
		// This is a multi-part reading.
		DrawFormat( fx, fy+4, 2, 0xffffffff, "Read a new note\nNote:%2d / Length: ?? / Run: ??", cp->audio_newnote );
	}
	else if( cp->decodephase == "AUDIO: Read Len And Run" )
	{
		//audio_lenandrun, audio_newnote
		DrawFormat( fx, fy+4, 2, 0xffffffff, "Read a new note\nNote:%2d / Length:%2d / Run:%2d", cp->audio_newnote, (cp->audio_lenandrun & 7)+1, ((cp->audio_lenandrun >> 3) & 0x1f)+1 );
	}
	else if( cp->decodephase == "AUDIO: Ending" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Audio Complete" );
	}
	else if( cp->decodephase == "AUDIO: Backtrack" )
	{
		// XXX TODO
	}
	else if( cp->decodephase == "AUDIO: Committed Backtrack" )
	{
		DrawFormat( fx+b.width/2-8, fy+4, -2, 0xffffffff, "Committed Backtrack\nOffset: %d\nRunlen: %d (%d)", cp->audio_backtrack_offset, cp->audio_backtrack_runlen, cp->audio_backtrack_remain );
	}
	else
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
		DrawFormat( fx+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "%s", cp->decodephase?cp->decodephase:"(Unknown)" );
	}
}

void DrawVPXDetail( Clay_RenderCommand * render )
{
	int original_scissors_box[4];
	CNFGGetScissors( original_scissors_box );

	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGFlushRender();

	CNFGSetScissors( (int[4]){ (int)b.x, screenh-((int)b.y+(int)b.height), (int)b.width, (int)b.height } );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	// comparison @ r->value / r->range vs prob

	int kc = 0;
	const int rrange = 5;
	int bcount = 0;
	vpx_reader * vpx_pr = (vpx_reader *)1;
	struct checkpoint * cpprev = 0;
	for( kc = cursor; kc >= 0; kc-- )
	{
		struct checkpoint * cp = (kc < 0) ? 0 : &checkpoints[kc];

		if( cp->baplay_vpx != vpx_pr )
			bcount++;

		if( cp->baplay_vpx )
			vpx_pr = cp->baplay_vpx;

		cpprev = cp;

		if( bcount >= rrange + 2 ) break;
	}


	int column_width = 12;
	float margin = 2;
	float bypm = b.y + margin;
	float mh = b.height - margin*2;
	float ry = b.y + margin;

	int pass = 0;
	int kcbackup = kc;
	vpx_reader * vpx_pr_backup = (vpx_reader *)vpx_pr;
	for( pass = 0; pass < 2; pass++ )
	{
		vpx_pr = vpx_pr_backup;
		kc = kcbackup;
		int dispct = 0;

		for( ; dispct < rrange * 2 + 1; kc++ )
		{
			if( kc >= nrcheckpoints )
			{
				float rx = b.x + dispct*(b.width-margin*2) / (rrange*2+1) + margin;
				DrawFormat( rx + column_width + 8, ry + mh/2, -2, 0xffffff5f, "\x01" );
				break;
			}
			if( kc < 0 ) { dispct++; continue; }
			struct checkpoint * cp = &checkpoints[kc];
			if( !cp ) { dispct++; continue; }
			vpx_reader  * vpx = cp->baplay_vpx;
			if( !vpx || vpx_pr == (vpx_reader *)1 ) { dispct++; continue; }

			struct checkpoint * cpnext = cp;
			int kcnext = kc;
			while( cpnext->baplay_vpx == cp->baplay_vpx )
			{
				kcnext++;
				cpnext++;
				if( kcnext >= nrcheckpoints )
				{
					cpnext = 0;
					break;
				}
			}

			if( vpx_pr == vpx ) { continue; }

			vpx_reader * vpx_pr_use = (vpx_reader*)vpx_pr;

			float rx = b.x + dispct*(b.width-margin*2) / (rrange*2+1) + margin;
			float rxnext = b.x + (dispct+1)*(b.width-margin*2) / (rrange*2+1) + margin;

			if( dispct == rrange )
			{
				CNFGColor( 0xf0f0f010 );
				CNFGTackRectangle( rx-15, b.y, rx + 27, b.y+b.height );
			}

			CNFGColor( 0xf0f0f040 );
			CNFGDrawBox( rx, b.y+margin, rx + column_width, b.y+mh + margin );

			dispct++;

			float range = vpx_pr_use->range + 0.0001;
			float rangenext = vpx->range + 0.0001;

			float upratio = (vpx_pr_use->range) / 256.0;
			float uprationext = (vpx->range) / 256.0;

			float ratio = 1.0 - ( (vpx_pr_use->value>>24) / range ) * upratio;
			float rationext = 1.0 - ( (vpx->value>>24) / rangenext ) * uprationext;
			float ratioo = 1.0 - cp->decode_prob / 256.0 * upratio;
			float ratioonext = 1.0 - (cpnext?cpnext->decode_prob:0) / 256.0 * uprationext;

			float uprationext_proj = (vpx_pr_use->range-cp->decode_prob + 1) / 256.0;

			int gpused = vpx_pr_use->count - vpx->count;
			//if( gpused < 0 ) gpused = 0;
			gpused = (vpx->buffer-vpx_pr_use->buffer)*8 + gpused;



			if( pass == 0 )
			{
				CNFGSetLineWidth(1.0);

				CNFGColor( 0xffffff50 );
				CNFGTackPoly( (RDPoint[]){
					{ rx + column_width, bypm + mh * (1.0-upratio) },
					{ rxnext, bypm + mh * (1.0-uprationext) },
					{ rxnext, bypm + mh * ratioonext },
					{ rx + column_width, bypm + mh * ratioo },
				}, 4 );
				CNFGTackPoly( (RDPoint[]){
					{ rx, bypm + mh * (1.0-upratio) },
					{ rx + column_width, bypm + mh * (1.0-upratio) },
					{ rx + column_width, bypm + mh * ratioo },
					{ rx, bypm + mh * ratioo },
				}, 4 );

				CNFGTackSegment(
					rx + column_width, bypm + mh * (1.0-upratio) ,
					rxnext, bypm + mh * (1.0-uprationext) );

				CNFGTackSegment(
					rx, bypm + mh * (1.0-upratio),
					 rx + column_width, bypm + mh * (1.0-upratio) );

				// This turned out to just be confusing
				if( 0 )
				if( vpx_pr_use->range- cp->decode_prob != vpx->range )
				{
					CNFGColor( 0xffffff10 );
					CNFGSetLineWidth(2.0);
					CNFGTackSegment(
						rx + column_width, bypm + mh * (1.0-upratio) ,
						rxnext, bypm + mh * (1.0-uprationext_proj) );
				}


				CNFGColor( 0x00000050 );
				CNFGTackPoly( (RDPoint[]){
					{ rx + column_width, bypm + mh },
					{ rxnext, bypm + mh },
					{ rxnext, bypm + mh * ratioonext },
					{ rx + column_width, bypm + mh * ratioo },
				}, 4 );

				CNFGTackPoly( (RDPoint[]){
					{ rx, bypm + mh },
					{ rx + column_width, bypm + mh },
					{ rx + column_width, bypm + mh * ratioo },
					{ rx, bypm + mh * ratioo }
				}, 4 );
			}
			else
			{
				CNFGSetLineWidth(2.0);

				CNFGColor( 0xf0f0f0c0 );
				// Boundary
				//CNFGTackSegment( rx, b.y + mh * ratio + margin,  rx + column_width, b.y + mh * ratio + margin );

				DrawHashAt( rx + column_width / 2, bypm + mh * ratio, column_width / 2-2 );

				CNFGTackSegment(
					rx + column_width/2, bypm + mh * ratio,
					rxnext + column_width/2, bypm + mh * rationext );


				if( gpused > 0 )
				{
					int hm = 0;
					float xadvance = (rxnext-rx) / (float)gpused;
					float xst = rx + column_width/2;
					float yadvance = ((bypm + mh * rationext) - (bypm + mh * ratio)) / (float)gpused;
					float yst = bypm + mh * ratio;
					xst += xadvance * 0.5;
					yst += yadvance * 0.5;
					for( hm = 0; hm < gpused; hm++ )
					{
						//DrawHashAt( xst, yst, column_width / 2-2 );
						CNFGDrawBox(
							xst - column_width / 2-1, yst - column_width / 2-1 ,
							xst + column_width / 2-2, yst + column_width / 2-2  );
						xst += xadvance;
						yst += yadvance;
					}
				}


				// Mark
				//CNFGTackSegment( rx + column_width + 8, b.y + mh * ratioo + margin, rx - 8, b.y + mh * ratioo + margin );

				DrawFormat( rx + column_width + 8, ry + mh/2, -2, 0xffffff5f, "%d", cp->decode_lb );
			}
			//decode_prob
			//decode_lb

			cpprev = cp;
			vpx_pr = vpx;
		}
	}

	//DrawFormat( b.x+b.width/2-8, fy+4+24*2, -2, 0xffffffff, "VPX DETAIL" );

/*

	BD_VALUE bigsplit;
	unsigned int range;
	unsigned int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;
	unsigned int bit = 0;

	bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);

	range = split;

	if (value >= bigsplit) {
		range = r->range - split;
		value = value - bigsplit;
		bit = 1;
	}

	const unsigned char shift = vpx_norm[(unsigned char)range];
	r->range = range << shift;
	r->value = value << shift;
	r->count = count - shift;
	return bit;*/
	vpx_reader  * baplay_vpx;

ending:
	CNFGFlushRender();
	CNFGSetScissors( original_scissors_box );
}

void DrawGlyphSet( Clay_RenderCommand * render )
{
	int original_scissors_box[4];
	CNFGGetScissors( original_scissors_box );

	if( !checkpoints && cursor >= 0 && cursor < nrcheckpoints ) return;
	
	Clay_BoundingBox b = render->boundingBox;
	CNFGColor( COLOR_BACKPAD_HEX );
	CNFGFlushRender();

	CNFGSetScissors( (int[4]){ (int)b.x, screenh-((int)b.y+(int)b.height), (int)b.width, (int)b.height } );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };

	struct checkpoint * cp = &checkpoints[cursor];
	if( !cp ) goto ending;
	if( !cp->glyphdata ) goto ending;

	float margin = 2;
	float fx = b.x + margin;
	float fy = b.y + margin;
	float fw = b.width - margin*2;
	float fh = b.height - margin*2;

	CNFGColor( 0xc0c0c090 );

	float outsideaspectratio = fw / fh;

	int optimalwidth = 1;
	int optimalheight = 1;
	int testwidth = 0;
	float bestratio = 1e20;
	for( testwidth = 1; testwidth <= TILE_COUNT+1; testwidth++ )
	{
		int nh = TILE_COUNT / testwidth;
		nh += (TILE_COUNT % testwidth) ? 1 : 0;
		float aspectratio = testwidth / (float)nh;
		float ratiodiff = aspectratio - outsideaspectratio;
		ratiodiff *= ratiodiff;
		if( ratiodiff > bestratio )
		{
			break;
		}
		bestratio = ratiodiff;
		optimalheight = nh;
		optimalwidth = testwidth;
	}

	if( optimalwidth == 0 || optimalheight == 0 ) goto ending;

	float scale_based_on_width = fw / optimalwidth;
	float scale_based_on_height = fh / optimalheight;
	float scale = (scale_based_on_width<scale_based_on_height) ? scale_based_on_width : scale_based_on_height;
	float center_adjust = (fw - optimalwidth * scale)/2;

	fx += center_adjust;

	int x, y;
	for( y = 0; y < optimalheight; y++ )
	{
		for( x = 0; x < optimalwidth; x++ )
		{
			int tileid = x + y * optimalwidth;
			if( tileid >= TILE_COUNT ) continue;
			int sx, sy;
			for( sy = 0; sy < BLOCKSIZE; sy++ )
			for( sx = 0; sx < BLOCKSIZE; sx++ )
			{
				int pxid = sx + sy * BLOCKSIZE;
				graphictype px = (*cp->glyphdata)[tileid][pxid/GRAPHICSIZE_WORDS/4];
				int pxval = (px >> ((pxid)%(8))) & 0x101;
				pxval = ( pxval & 1 ) | ( pxval >> 7 );
				CNFGColor( (uint32_t[]){0x000000ff,0x808080ff,0xffffffff,0xf0f0f0ff}[pxval] );
				float tx = fx + ( sy + x * 10 + 1 ) * scale / 10.0;
				float ty = fy + ( sx + y * 10 + 1 ) * scale / 10.0;
				CNFGTackRectangle( tx, ty, tx+scale/8.0, ty+scale/8.0 );
			}

			int selected = 0;


			if( cp->decodephase == "Reading Glyphs" )
			{
				if( tileid == cp->decodeglyph/64 )
				{
					selected = 1;
				}
			}
			else if( cp->decode_cellid >= 0 && cp->curmap )
			{
				glyphtype gindex = (*cp->curmap)[cp->decode_cellid];

				// Animate the search.
				if( cp->decodephase == "Decoding Bit" )
				{
					gindex = cp->decode_tileid;
				}

				if( tileid == gindex )
				{
					selected = 1;
				}
			}

			// Otherwise, while decoding.

			if( selected )
			{
				CNFGColor( 0xffffffff );
				float tx = fx + ( x ) * scale - 1;
				float ty = fy + ( y ) * scale - 2;
				CNFGDrawBox( tx, ty, tx+scale, ty+scale );
			}
		}
	}

ending:
	CNFGFlushRender();
	CNFGSetScissors( original_scissors_box );
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

	float xoffset = (b.width - fzoom * RESX)/2;
	fx += xoffset;

	EmitSamples8( cp, fx, fy, fzoom, (glyphtype *)cp->curmap, (void*)cp->glyphdata );
}

void RenderFrame()
{
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
#ifdef __wasm__
				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(32), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
				if( AudioEnabled )
					CLAY_TEXT(CLAY_STRING("\x04"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				else
					CLAY_TEXT(CLAY_STRING("\x0e"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));

				int ToggleAudio();
				int ToggleFullscreen();
				int IsFullscreen();

				if( btnClicked ) { AudioEnabled = ToggleAudio(); }

				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(32), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
				if( IsFullscreen() )
					CLAY_TEXT(CLAY_STRING( "\x1f" ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				else
					CLAY_TEXT(CLAY_STRING( "\x12" ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked ) { ToggleFullscreen(); }

#endif
				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(32), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
				if( ShowHelp )
					CLAY_TEXT(CLAY_STRING( "X" ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				else
					CLAY_TEXT(CLAY_STRING( "?" ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked ) { ShowHelp = !ShowHelp; }

				if( screenw > 440 )
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf_g( 1, TITLE ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}
				}

				int tframe = checkpoints?checkpoints[cursor].frame:0;
				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } /*, .backgroundColor = COLOR_PADGREY */ } )
				{
					CLAY_TEXT(saprintf( "%d/%d (Frame %d)", cursor, nrcheckpoints, tframe ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
				}

				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(45), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } /*, .backgroundColor = COLOR_PADGREY */} )
				{
					CLAY_TEXT(saprintf( is_vertical?"%3db\n%3db":"A:%3d b, V:%3d b", (int)bitsperframe_audio[tframe], (int)bitsperframe_video[tframe] ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
				}

				if( !is_vertical && frame < FRAMECT )
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(40), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf_g( (frame < FRAMECT), "Dec %d", frame), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					}
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

					if( is_vertical )
					{
						CLAY({ .custom = { .customData = ( cp->decodephase == "Reading Glyphs" ) ? DrawGlyphSet : DrawGeneral } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(200), .height = CLAY_SIZING_GROW(150) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						{
							CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
						}
					}

					int need_to_display_audio_track = 0;
					int doing_audio = 0;
					int doing_mem_and_vpx = 0;
					if( cp->decodephase && strncmp( cp->decodephase, "AUDIO", 5 ) == 0 )
					{
						CLAY({ .custom = { .customData = DrawCellStateAudio } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
						{
							CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
						}

						if( NeedsHuffman() )
						{

							CLAY({ .custom = { .customData = DrawCellStateAudioStack } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}

							CLAY({ .custom = { .customData = DrawCellStateAudioHuffman } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								//CLAY_TEXT(CLAY_STRING( " \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}
						}
						else if( NeedsExpGolomb() )
						{
							CLAY({ .custom = { .customData = DrawCellStateAudioExpGolomb } ,.layout = {
								.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
								.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}
							need_to_display_audio_track = 1;
						}
						else
						{

							CLAY({ .custom = {  } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
								CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
							}


							need_to_display_audio_track = 1;
						}
						doing_audio = 1;
					}
					else
					{
						doing_mem_and_vpx = 1;
						CLAY({ .custom = { .customData = DrawMemoryAndVPX } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
						{
							CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
						}
						CLAY({ .custom = { .customData = DrawCellState } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
						{
							CLAY_TEXT(CLAY_STRING( " \n \n " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
						}
					}

					if( cp->decodephase == "Frame Done" ) need_to_display_audio_track = 1;

					if( need_to_display_audio_track )
					{
						CLAY({ .custom = { .customData = DrawAudioTrack } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(10), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
						{
						}
					}
					else if( !doing_audio )
					{
						if( ( doing_mem_and_vpx ) &&
							cp->decodephase != "Committing Tile" &&
							cp->decodephase != "START" )
						{
							CLAY({ .custom = { .customData = DrawVPXDetail } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(25), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
							}
						}
						else
						{
							CLAY({ .custom = { .customData = DrawGlyphSet } ,.layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(25), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild } } )
							{
							}
						}
					}
				}
				else
				{
					// Null. Can't draw a side info bar for this.
				}

				if( !is_vertical )
				{
					CLAY({ .custom = { .customData = ( cp->decodephase == "Reading Glyphs" ) ? DrawGlyphSet : DrawGeneral } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(200), .height = CLAY_SIZING_GROW(150) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					{
						CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}
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
					CLAY_TEXT(CLAY_STRING("<<<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked ) { if( cursor > 0 ) cursor--; topCursor = cursor; }

				CLAY({ .custom = { .customData = DrawTopGraph } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
				{
					CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
				}

				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					CLAY_TEXT(CLAY_STRING(">>>"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked ) { if( cursor < nrcheckpoints-1 ) cursor++; topCursor = cursor; }
			}


			CLAY({
				.id = CLAY_ID("Bottom Bar"),
				.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(10) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
				.backgroundColor = COLOR_PADGREY
			})
			{
				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					CLAY_TEXT(CLAY_STRING("<<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked && checkpoints ) { int tframe = checkpoints?checkpoints[cursor].frame:0; for( ; cursor > 0; cursor-- ) if( checkpoints[cursor].frame < tframe - 150 ) { break; } midCursor = topCursor = cursor; }

				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					CLAY_TEXT(CLAY_STRING("|<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked && checkpoints ) { int tframe = checkpoints?checkpoints[cursor].frame:0; for( ; cursor > 0; cursor-- ) if( checkpoints[cursor].frame != tframe ) { break; } midCursor = topCursor = cursor; }

				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(32), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					CLAY_TEXT(CLAY_STRING("\x0f"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
				if( btnClicked && checkpoints ) { inPlayMode = 0; }

				CLAY({ .custom = { .customData = DrawMidGraph } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
				{
					CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
				}

				CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(32), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
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
				.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(10) } },
				.backgroundColor = COLOR_PADGREY
			})
			CLAY({
				.id = CLAY_ID("Final Bottom Bar"),
				.custom = { .customData = DrawBottomGraph },
				.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(32), .width = CLAY_SIZING_GROW(10) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
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
			if( tFrame + 1 < FRAMECT && checkpoint_offset_by_frame[tFrame+1]+1 > 0 && checkpoint_offset_by_frame[tFrame+1]+1 < nrcheckpoints )
			{
				midCursor = topCursor = cursor = checkpoint_offset_by_frame[tFrame+1]+1;
			}
		}
	}

#ifdef __wasm__
	static int last_played_audio_frame = -1;
	struct checkpoint * cp = &checkpoints[cursor];
	if( cp && cp->audio_play && cp->frame != last_played_audio_frame )
	{
		void FeedWebAudio( float *, int );
		FeedWebAudio( cp->audio_play, AS_PER_FRAME );
		last_played_audio_frame = cp->frame;
	}
#endif

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
	CNFGGetDimensions( &screenw, &screenh );
	Clay_SetLayoutDimensions((Clay_Dimensions) { screenw, screenh });

	is_vertical = screenh > screenw;

	CNFGSetScissors( (int[4]){ 0, 0, screenw, screenh } );

	// Debug mouse input.
	//DrawFormat( 50, 50, 2, 0xc0c0c0ff, "%d %d %d", mousePositionX, mousePositionY, isMouseDown );

	int linkhover = 0;

	if( ShowHelp )
	{
		CNFGColor( 0x000000c0 );
		CNFGTackRectangle( 0, 0, screenw, screenh );
		DrawFormatShadow( screenw/2, 48, -2, 0xffffffff, "Explore badder apple one bit at a time" );

		linkhover = mousePositionX > screenw/2-220 && mousePositionX < screenw/2+220 && mousePositionY < 122+24 && mousePositionY > 96+24;
		DrawFormatShadow( screenw/2, 120, -2, 0xffffffff, "https://github.com/cnlohr/badderapple" );
		CNFGTackSegment( screenw/2-222, (linkhover?120:122)+24, screenw/2+222, (linkhover?120:122)+24 );
#ifdef __wasm__
		void NavigateLink( const char * url );
		void ChangeCursorToPointer( int yes );
		if( linkhover && mouseUpThisFrame )
			NavigateLink( "https://github.com/cnlohr/badderapple" );
		ChangeCursorToPointer( linkhover );
#endif
		CNFGColor( 0x000000f8 );
		CNFGTackRectangle( 118, 10, 408, 36 );
		DrawFormatShadow( 120, 12, 2, 0xffffffff, "\x1b Click X to close help." );

		DrawFormatShadow( screenw/2, screenh/2-48, -2, 0xffffffff, "[Space] to toggle playing" );
		DrawFormatShadow( screenw/2, screenh/2-24, -2, 0xffffffff, "left \x1b right \x1a to advance bytes" );
		DrawFormatShadow( screenw/2, screenh/2+0 , -2, 0xffffffff, "pgup/pgdn \x17 to advance frames" );
		DrawFormatShadow( screenw/2, screenh/2+24, -2, 0xffffffff, "home/end to begin/end" );
		DrawFormatShadow( screenw/2, screenh-114, -2, 0xffffffff, "\x1b Click and drag to scrub bytes \x1a" );
		DrawFormatShadow( screenw/2, screenh-72, -2, 0xffffffff, "\x1b Click and drag to scrub frames \x1a" );
		DrawFormatShadow( screenw/2, screenh-30, -2, 0xffffffff, "\x1b Full Decoded Overview \x1a" );
	}

	CNFGSwapBuffers();
}

void EarlyGlyphDecodeFrameDone( int frame )
{
	cursor = nrcheckpoints - 1;
	RenderFrame();
}


int WXPORT(main)()
{
	int x, y;

	HuffNoteNodes = GenTreeFromTable( espbadapple_song_huffnote, sizeof(espbadapple_song_huffnote)/sizeof(espbadapple_song_huffnote[0]), &HuffNoteNodeCount );
	HuffLenRunNodes = GenTreeFromTable( espbadapple_song_hufflen, sizeof(espbadapple_song_hufflen)/sizeof(espbadapple_song_hufflen[0]), &HuffLenRunNodeCount );

#ifdef __wasm__
	CNFGSetupFullscreen( TITLE, 0 );
#else
	CNFGSetup( TITLE, 1920/2, 1080/2 );
#endif

	ExtraDrawingInit( 1920/2, 1080/2 );

#ifdef VPX_GREY4
	static uint8_t palette[48] = { 0, 0, 0, 85, 85, 85, 171, 171, 171, 255, 255, 255 };
#elif defined( VPX_GREY3 )
	static uint8_t palette[48] = { 0, 0, 0, 128, 128, 128, 255, 255, 255 };
#endif

	CHECKPOINT( decodephase = "START" );
	ba_play_setup( &ctx );
	ba_audio_setup();

	int outbuffertail = 0;
	int lasttail = 0;
	double Now = OGGetAbsoluteTime();
	double Last = Now;

	inPlayMode = 1;

	while( CNFGHandleInput() )
	{
		Now = OGGetAbsoluteTime();
		dTime = Now - Last;
		Last = Now;
		if( frame < FRAMECT )
		{

			if( ba_play_frame( &ctx ) ) break;


			frame++;

			CHECKPOINT( decodephase = "Frame Done", decode_cellid = -1 );
			audioOut = 0;

			lasttail = outbuffertail;

			if( frame >= START_AUDIO_AT_FRAME )
			{
				outbuffertail = (AS_PER_FRAME + outbuffertail) % AUDIO_BUFFER_SIZE;
				ba_audio_fill_buffer( out_buffer_data, outbuffertail );

				struct checkpoint * cp = &checkpoints[nrcheckpoints-1];
				//uint8_t * ad = cp->audio_sample_data = malloc( AS_PER_FRAME );
				cp->audio_sample_data_frame = nrcheckpoints;

#ifdef __wasm__
				audioOut = malloc( sizeof(float) * AS_PER_FRAME );
				int samp = 0;
				for( int n = lasttail; n != outbuffertail; n = (n+1)%AUDIO_BUFFER_SIZE )
				{
					//*(ad++) = out_buffer_data[n];
					audioOut[samp++] = out_buffer_data[n] / 128.0 - 1.0;
					//fwrite( &fo, 1, 4, fAudioDump );
				}
#endif
			}
		}

		RenderFrame();
	}
	return 0;
}



