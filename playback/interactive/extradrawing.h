#ifndef _EXTRADRAWING_H
#define _EXTRADRAWING_H

#include <stdarg.h>


#define CLAY_IMPLEMENTATION
#include "clay.h"

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

const Clay_Color COLOR_LIGHT = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};

// Must include after rawdraw

Clay_Arena arena;


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
printf( "XA (%s) %d %d\n", stt, w, h );
	return (Clay_Dimensions){ w*2, h*2 };
}

void DrawTextClay( Clay_RenderCommand * renderCommand )
{
	Clay_TextRenderData * tr = (Clay_TextRenderData*)&renderCommand->renderData;
	CNFGPenX = renderCommand->boundingBox.x;
	CNFGPenY = renderCommand->boundingBox.y;
	SetColor( tr->textColor );
	char * stt = alloca( tr->stringContents.length + 1 );
	memcpy( stt, tr->stringContents.chars, tr->stringContents.length );
	stt[tr->stringContents.length] = 0;
	CNFGSetLineWidth( tr->fontSize/8 );
	CNFGDrawText( stt, tr->fontSize/8*2 );
}

void ExtraDrawingInit( int screenWidth, int screenHeight )
{
    uint64_t totalMemorySize = Clay_MinMemorySize();
    arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));

    // Note: screenWidth and screenHeight will need to come from your environment, Clay doesn't handle window related tasks
    Clay_Initialize(arena, (Clay_Dimensions) { screenWidth, screenHeight }, (Clay_ErrorHandler) { HandleClayErrors });

	Clay_SetMeasureTextFunction( clayTextDim, 0 );
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
	CNFGSetLineWidth( size );
	CNFGDrawText( buf, size*2 );
}



#endif
