#ifndef _EXTRADRAWING_H
#define _EXTRADRAWING_H

#include <stdarg.h>
#include "os_generic.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"



// Must include after rawdraw

Clay_Arena arena;


const Clay_Color COLOR_BACKGROUND = (Clay_Color) {25, 25, 25, 255};
const Clay_Color COLOR_PADGREY = (Clay_Color) {50, 50, 50, 255};
const Clay_Color COLOR_BTNGREY = (Clay_Color) {100, 100, 100, 255};
const Clay_Color COLOR_BTNGREY2 = (Clay_Color) {200, 200, 200, 255};
const Clay_Color COLOR_WHITE = (Clay_Color) {255, 255, 255, 255};
#define MAX_BUTTONS 100


int buttonHasDownFocus[MAX_BUTTONS];
int buttonHoveredLastFrame[MAX_BUTTONS];

float hoverStates[MAX_BUTTONS];
int btnNo = 0;
float fDeltaTime = 0.0;
double fLast = 0.0;

int lastMouseDown;
int mouseUpThisFrame = 0;
int mouseDownThisFrame = 0;

#define MAX_LABELS 1024
char * labels[MAX_LABELS];
double labelDoneTime[MAX_LABELS];
int labelct = 0;
int btnClicked = 0;

int mousePositionX, mousePositionY, isMouseDown;

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { mousePositionX = x; mousePositionY = y; isMouseDown = bDown; }
void HandleMotion( int x, int y, int mask ) { mousePositionX = x; mousePositionY = y; isMouseDown = mask; }
int HandleDestroy() { return 0; }

void HandleClayErrors(Clay_ErrorData errorData) {
	// See the Clay_ErrorData struct for more information
	fprintf( stderr, "%s", errorData.errorText.chars);
}

Clay_Dimensions clayTextDim(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData)
{
	int w, h;

	char * stt = alloca( text.length + 1 );
	memcpy( stt, text.chars, text.length );
	stt[text.length] = 0;

	CNFGGetTextExtents( stt, &w, &h, config->fontSize / 8  );
	return (Clay_Dimensions){ w*2.25+1, h*1.75+1 };
}

void ExtraDrawingInit( int screenWidth, int screenHeight )
{
	uint64_t totalMemorySize = Clay_MinMemorySize();
	arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));

	// Note: screenWidth and screenHeight will need to come from your environment, Clay doesn't handle window related tasks
	Clay_Initialize(arena, (Clay_Dimensions) { screenWidth, screenHeight }, (Clay_ErrorHandler) { HandleClayErrors });

	Clay_SetMeasureTextFunction( clayTextDim, 0 );

	int j;
	for( j = 0; j < MAX_LABELS; j++ )
		labelDoneTime[j] = 0;
}

int inBox( Clay_BoundingBox b )
{
	Clay_Vector2 rel = { .x = mousePositionX - b.x, .y = mousePositionY - b.y };
	return ( rel.x < b.width && rel.y < b.height && rel.x >= 0 && rel.y >= 0 );
}

void SetColor( Clay_Color c )
{
	int r = c.r;
	int g = c.g;
	int b = c.b;
	int a = c.a;
	if( r < 0 ) r = 0; if( r > 255 ) r = 255;
	if( g < 0 ) g = 0; if( g > 255 ) g = 255;
	if( b < 0 ) b = 0; if( b > 255 ) b = 255;
	if( a < 0 ) a = 0; if( a > 255 ) a = 255;
	CNFGColor( a | (r<<24) | (g<<16) | (b<<8) );
}

void DrawRectangle( Clay_BoundingBox b, Clay_Color c )
{
	SetColor( c );
	CNFGTackRectangle( b.x, b.y, b.x + b.width, b.y + b.height );
}

void DrawTextClay( Clay_RenderCommand * renderCommand )
{
	Clay_TextRenderData * tr = (Clay_TextRenderData*)&renderCommand->renderData;
	CNFGPenX = renderCommand->boundingBox.x + 1;
	CNFGPenY = renderCommand->boundingBox.y + 1;
	char * stt = alloca( tr->stringContents.length + 1 );
	memcpy( stt, tr->stringContents.chars, tr->stringContents.length );
	stt[tr->stringContents.length] = 0;
	CNFGSetLineWidth( tr->fontSize/8 );
	CNFGColor( 0x000000ff );
	CNFGDrawText( stt, tr->fontSize/8*2 );
	CNFGPenX --;
	CNFGPenY --;
	SetColor( tr->textColor );
	CNFGDrawText( stt, tr->fontSize/8*2 );
}

void DrawFormat( int x, int y, int size, uint32_t color, const char * fmt, ... )
{
	char buf[4096];
	va_list va;
	va_start (va, fmt);
	vsprintf (buf, fmt, va);
	va_end (va);
	CNFGPenX = x;
	CNFGPenY = y;
	CNFGColor( color );
	if( size < 0 )
	{
		size = -size;
		int w, h;
		CNFGGetTextExtents( buf, &w, &h, size*2 );
		CNFGPenX -= w/2;
	}
	CNFGSetLineWidth( size );
	CNFGDrawText( buf, size*2 );
}

void LayoutStart()
{
	double Now = OGGetAbsoluteTime();
	if( fLast == 0.0 )
		fDeltaTime = 0.0;
	else
		fDeltaTime = Now - fLast;
	fLast = Now;

	btnNo = 0;
	mouseUpThisFrame = !isMouseDown && lastMouseDown;
	mouseDownThisFrame = isMouseDown && !lastMouseDown;
	lastMouseDown = isMouseDown;
	labelct = 0;
	btnClicked = 0;
}



Clay_String saprintf( const char * fmt, ... )
{
	static char buf[4096];
	va_list va;
	va_start (va, fmt);
	int len = vsprintf (buf, fmt, va);
	va_end (va);

	double ldt = 0;
	//if( !done ) ldt = labelDoneTime[labelct] = CLAY__MIN( CLAY__MAX( labelDoneTime[labelct] - fDeltaTime*15, 0.0 ), len );
	ldt = labelDoneTime[labelct] += fDeltaTime*15;

	double roundup = ceilf( ldt );

	if( len > roundup )
	{
		len = roundup;
	}

	char * r = labels[labelct] = realloc( labels[labelct], len+1 );
	memcpy( r, buf, len );
	r[len] = 0;

	float partial = ldt - (int)ldt;
	if( partial > 0.001 && roundup < len )
	{
		r[len-1] = partial * 96+32;
	}

	labelct++;
	return (Clay_String){ .length = len, .chars = r };
}


Clay_String saprintf_g( int done, const char * fmt, ... )
{
	static char buf[4096];
	va_list va;
	va_start (va, fmt);
	int len = vsprintf (buf, fmt, va);
	va_end (va);

	double ldt = 0;
	if( !done ) ldt = labelDoneTime[labelct] = CLAY__MIN( CLAY__MAX( labelDoneTime[labelct] - fDeltaTime*15, 0.0 ), len );
	else ldt = labelDoneTime[labelct] += fDeltaTime*15;

	double roundup = ceilf( ldt );

	if( len > roundup )
	{
		len = roundup;
	}

	char * r = labels[labelct] = realloc( labels[labelct], len+1 );
	memcpy( r, buf, len );
	r[len] = 0;

	float partial = ldt - (int)ldt;
	if( partial > 0.001 && roundup < len )
	{
		r[len-1] = partial * 96+32;
	}

	labelct++;
	return (Clay_String){ .length = len, .chars = r };
}


void HandleButtonHover(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData)
{
	int bn = (int)userData;
	float f = hoverStates[bn];
	f += fDeltaTime*6.0;
	if( f > 0.5 ) f = 0.5;
	if( isMouseDown ) f = 0.0;
	hoverStates[bn] = f;

	if( mouseDownThisFrame ) buttonHasDownFocus[bn] = 1;
	buttonHoveredLastFrame[bn] = 1;
}


Clay_Color ClayButton()
{
	float f = hoverStates[btnNo];
	float fs = f;
	f -= fDeltaTime;
	if( f < 0.05 ) f = 0.05;
	hoverStates[btnNo] = f;

	Clay_OnHover( HandleButtonHover, btnNo );
	f = hoverStates[btnNo];

	if( mouseUpThisFrame )
	{
		btnClicked = buttonHasDownFocus[btnNo] && buttonHoveredLastFrame[btnNo];
		buttonHasDownFocus[btnNo] = 0;
	}
	buttonHoveredLastFrame[btnNo] = 0;
	btnNo++;
	return (Clay_Color){ fs*255.0, fs*255.0, fs*255.0, 255.0 };
}



#endif
