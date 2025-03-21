#include <stdio.h>

// XXX TODO: Handle partial bits in ba_play.h

#define CHECKPOINT(x...) { x; ba_i_checkpoint(); }
#define CHECKBITS_AUDIO(x) { bitsperframe_audio[frame] += x;}
#define CHECKBITS_VIDEO(x) { bitsperframe_video[frame] += x;}

int frame = 0;

double bitsperframe_audio[FRAMECT];
double bitsperframe_video[FRAMECT];
int    checkpoint_offset_by_frame[FRAMECT];
void ba_i_checkpoint();

// Various set things
const char * decodephase;

#define FIELDS(x) x(decodeglyph) x(decode_is0or1) x(decode_runsofar) x(decode_prob) x(decode_lb) \
	x(decode_cellid) x(decode_class) x(decode_run) x(decode_fromglyph) x(decode_probability) \
	x(decode_tileid) x(decode_level) x(audio_pullbit) x(audio_gotbit) x(audio_last_bitmode) \
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

#include "extradrawing.h"

uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
ba_play_context ctx;


int cursor = 0;

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

	int FIELDS(xcomma) dummy;

} * checkpoints;

int nrcheckpoints;

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

	cp->frame = frame;

	checkpoint_offset_by_frame[frame] = nrcheckpoints;
	#define xassign(tf) cp->tf = tf;
	FIELDS(xassign);

	nrcheckpoints++;
}

#define ZOOM 2

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
}

void EmitPartial( graphictype tgprev, graphictype tg, graphictype tgnext, int subframe )
{
	// This should only need +2 regs (or 3 depending on how the optimizer slices it)
	// (so all should fit in working reg space)
	graphictype A = tgprev >> 8;
	graphictype B = tgprev;	  // implied & 0xff
	graphictype C = tgnext >> 8;
	graphictype D = tgnext;	  // implied & 0xff

	graphictype E = (B&C)|(A&D); // 8 bits worth of MSB of (next+prev+1)/2
	graphictype F = D|B;		 // 8 bits worth of LSB of (next+prev+1)/2

	graphictype G = tg >> 8;
	graphictype H = tg;		  // implied & 0xff

	if( subframe )
		tg = (F&G)|(E&H);	 // 8 bits worth of MSB of this+(next+prev+1)/2-1
	else
		tg = G|E|(F&H);	   // 8 bits worth of MSB|LSB of this+(next+prev+1)/2-1

	KOut( tg );
}

void EmitSamples8()
{
	int bx, by;

	glyphtype * gm = ctx.curmap;
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
				graphictype * g = ctx.glyphdata[gindex];
				int sx = 0;
				if( bx > 0 )
				{
					graphictype tgnext = g[1];
					graphictype tg = g[0];
					graphictype tgprev = (bx > 0 ) ? ctx.glyphdata[gm[gmi-1]][7] : tgnext;

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
					graphictype tgnext = (bx<RESX/BLOCKSIZE-1)?ctx.glyphdata[gm[gmi-1]][0] : tgprev;
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
		float f = fba[y][x] * 128;
		if( f < 0 ) f = 0; 
		if( f > 255.5 ) f = 255.5;
		int v = f;
		int zx, zy;
		//for( zx = 0; zx < ZOOM; zx++ )
		//for( zy = 0; zy < ZOOM; zy++ )
		//	gof[zx+x*ZOOM + (zy+y*ZOOM)*RESX*ZOOM] = (f/120);
		uint32_t color = (v<<24) | (v<<16) | (v<<8) | 0xFF;
		CNFGColor( color );
		CNFGTackRectangle( x*ZOOM, y*ZOOM, x*ZOOM+ZOOM, y*ZOOM+ZOOM );
	}
}

#endif

void DrawBottomGraph( Clay_RenderCommand * render )
{
	Clay_BoundingBox b = render->boundingBox;
	Clay_Vector2 cursor_rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };
	float nindex = 0;
	float iadv = FRAMECT/b.width;
	int index = 0;
	float x;
	static float mbv = 1.0;
	static int has_down_focus;

	if( mouseDownThisFrame ) has_down_focus = Clay_Hovered();
	if( !isMouseDown ) has_down_focus = false;

	for( x = 0; x < b.width; x++ )
	{
		nindex += iadv;
		float bitsA = 0;
		float bitsV = 0;
		for( ; index < nindex; index++ )
		{
			bitsA += bitsperframe_audio[index];
			bitsV += bitsperframe_video[index];
		}
		CNFGColor( 0x808080ff );
		CNFGTackSegment( b.x + x, b.y+b.height-bitsA/mbv*b.height, b.x+x, b.y+b.height );
		CNFGColor( 0xc0c0c0ff );
		CNFGTackSegment( b.x + x, b.y+b.height-(bitsA+bitsV)/mbv*b.height, b.x+x, b.y+b.height-bitsA/mbv*b.height );
		if( ( Clay_Hovered() || has_down_focus ) && (int)x == (int)cursor_rel.x )
		{
		    Clay_LayoutElement *openLayoutElement = Clay__GetOpenLayoutElement();
			CNFGColor( 0xffffffff );
			CNFGTackSegment( b.x + x, b.y, b.x+x, b.y+b.height-(bitsA+bitsV)/mbv*b.height );
			if( has_down_focus )
			{
				cursor = checkpoint_offset_by_frame[(int)nindex];
			}
		}
		if( bitsA+bitsV > mbv ) mbv = bitsA+bitsV;
	}
}


int main()
{
	int x, y;
	short w, h;
	CNFGSetup( "test", 1920/2, 1080/2 );
	ExtraDrawingInit( 1920/2, 1080/2 );

#ifdef VPX_GREY4
	static uint8_t palette[48] = { 0, 0, 0, 85, 85, 85, 171, 171, 171, 255, 255, 255 };
#elif defined( VPX_GREY3 )
	static uint8_t palette[48] = { 0, 0, 0, 128, 128, 128, 255, 255, 255 };
#endif

	ba_play_setup( &ctx );
	ba_audio_setup();

	FILE * fAudioDump = fopen( "audio.dump", "wb" );

	int outbuffertail = 0;
	int lasttail = 0;

	while( CNFGHandleInput() )
	{
		if( frame < FRAMECT )
		{

			if( ba_play_frame( &ctx ) ) break;


			frame++;

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
				fwrite( &fo, 1, 4, fAudioDump );
			}
		}
		CNFGClearFrame();
		Clay_SetPointerState((Clay_Vector2) { mousePositionX, mousePositionY }, isMouseDown);
		Clay_BeginLayout();

#if 0
		Clay_ElementDeclaration sidebarItemConfig = (Clay_ElementDeclaration) {
			.layout = {
				.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) }
			},
			.backgroundColor = COLOR_PADGREY
		};

		//EmitSamples8();
		CLAY({ .id = CLAY_ID("OuterContainer"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }, .backgroundColor = COLOR_BACKGROUND })
		{
			CLAY({
				.id = CLAY_ID("SideBar"),
				.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
				.backgroundColor = COLOR_BTNGREY
			})
			{
				CLAY({ .id = CLAY_ID("ProfilePictureOuter"), .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } }, .backgroundColor = COLOR_BTNGREY2 })
				{
					//CLAY({ .id = CLAY_ID("ProfilePicture"), .layout = { .sizing = { .width = CLAY_SIZING_FIXED(60), .height = CLAY_SIZING_FIXED(60) }}, }) {}
					CLAY_TEXT(CLAY_STRING("Clay - UI Library 1234"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255} }));
				}

				// Standard C code like loops etc work inside components
				for (int i = 0; i < 5; i++) {
					//SidebarItemComponent();
						CLAY(sidebarItemConfig) {
					}
				}

				CLAY({ .id = CLAY_ID("SideBottom"), .layout = {  .padding = CLAY_PADDING_ALL(16), .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }, .backgroundColor = COLOR_PADGREY })
				{
					CLAY_TEXT(CLAY_STRING("Bottom Text"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255} }));	
				}
			}


			CLAY({ .id = CLAY_ID("MainContent"), .layout = {  .padding = CLAY_PADDING_ALL(16),  .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } }, .backgroundColor = COLOR_PADGREY })
			{
				CLAY_TEXT(CLAY_STRING("Right Text"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255} }));	
			}
		}
#endif

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
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf_g( 1, "Main Body" ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_LEFT, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}
				}




				CLAY({
					.id = CLAY_ID("Bottom Mid"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					int tframe = checkpoints?checkpoints[cursor].frame:0;
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf( "Event %d / %d (Frame %d)", cursor, nrcheckpoints, tframe ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(saprintf( "Audio: %.0f bits, Video: %.0f bits", bitsperframe_audio[tframe], bitsperframe_video[tframe] ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}
				}


				CLAY({
					.id = CLAY_ID("Mid Bottom Bar"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked ) { if( cursor > 0 ) cursor--; }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING(">"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked ) { if( cursor < nrcheckpoints-1 ) cursor++; }
				}


				CLAY({
					.id = CLAY_ID("Bottom Bar"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("<<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked ) printf( "CLICKED\n" );

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING("|<"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { int tframe = checkpoints?checkpoints[cursor].frame:0; int k = cursor; for( ; k >= 0; k-- ) if( checkpoints[k].frame != tframe ) { cursor = k; break; } }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = COLOR_PADGREY } )
					{
						CLAY_TEXT(CLAY_STRING( " " ), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));	
					}

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING(">|"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked && checkpoints ) { int tframe = checkpoints[cursor].frame; int k = cursor; for( ; k < nrcheckpoints; k++ ) if( checkpoints[k].frame != tframe ) { for( ; k < nrcheckpoints; k++ ) if( checkpoints[k].frame == tframe+1 ) cursor = k; else break; break; } }

					CLAY({ .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
						CLAY_TEXT(CLAY_STRING(">>"), CLAY_TEXT_CONFIG({ .textAlignment = CLAY_TEXT_ALIGN_CENTER, .fontSize = 16, .textColor = {255, 255, 255, 255} }));
					if( btnClicked ) printf( "CLICKED\n" );
				}

				CLAY({
					.id = CLAY_ID("Final Bottom Bar"),
					.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild },
					.backgroundColor = COLOR_PADGREY
				})
				{
					CLAY({ .custom = { .customData = DrawBottomGraph } , .layout = { .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = paddingChild }, .backgroundColor = ClayButton() } )
					{
						CLAY({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(16) } } }) {}
					}
				}

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
			}
		}

		//DrawFormat( 50, 200, 2, 0xffffffff, "Test %d\n", frame );
		CNFGGetDimensions( &w, &h );
		Clay_SetLayoutDimensions((Clay_Dimensions) { w, h });

		CNFGSwapBuffers();
	}
	return 0;
}



