#include "ch32fun.h"
#include <stdio.h>

#define WARNING(x...)

#define BADATA_DECORATOR const __attribute__((section(".fixedflash")))
#define BAS_DECORATOR    const __attribute__((section(".fixedflash")))

#include "ba_play.h"
#include "ba_play_audio.h"

uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
ba_play_context ctx;

int main()
{
	SystemInit();
	printf( "A\n" );

	while(1)
	{
		printf( "%08x\n", espbadapple_song_data[0] );
	}

	ba_play_setup( &ctx );
	ba_audio_setup();

	int lasttail = 0;
	int outbuffertail = 0;
	int frame = 0;

	while(1)
	{
		if( ba_play_frame( &ctx ) ) break;
		PrintHex( 100 );

		lasttail = outbuffertail;
		outbuffertail = (F_SPS/30*frame) % AUDIO_BUFFER_SIZE;
		ba_audio_fill_buffer( out_buffer_data, outbuffertail );

		for( int n = lasttail; n != outbuffertail; n = (n+1)%AUDIO_BUFFER_SIZE )
		{
			//float fo = out_buffer_data[n] / 128.0 - 1.0;
			//fwrite( &fo, 1, 4, fAudioDump );
			// Do something with out_buffer_data.
		}

		frame++;
	}
}

