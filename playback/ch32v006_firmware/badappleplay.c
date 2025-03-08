#include "ch32fun.h"
#include <stdio.h>


#define WARNING(x...)

#define BADATA_DECORATOR const __attribute__((section(".fixedflash")))
#define BAS_DECORATOR    const __attribute__((section(".fixedflash")))

#include "ba_play.h"
#include "ba_play_audio.h"

uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
ba_play_context ctx;

volatile uint32_t kas = 0;

////////////////////////////////////////////////////////////////////////////////////////////


#define SSD1306_CUSTOM_INIT_ARRAY 1
#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_FULLUSE
#define SSD1306_W (64) 
#define SSD1306_H (64)
#define SSD1306_OFFSET 32

#define SSD1306_RST_PIN PC3

#define I2CDELAY_FUNC( x )
	// asm volatile( "nop\nnop\n");

#include "ssd1306.h"
#include "ssd1306mini.h"

	const uint8_t ssd1306_init_array[] =
	{
		0xAE, // Display off
		0x20, 0x00, // Horizontal addresing mode
		0x00, 0x12, 0x40, 0xB0,
		0xD5, 0xf0, // Function Selection   <<< This controls scan speed. F0 is fastest.  The LSN = D divisor.
		0xA8, 0x2F, // Set Multiplex Ratio
		0xD3, 0x00, // Set Display Offset
		0x40,
		0xA1, // Segment remap
		0xC8, // Set COM output scan direction
		0xDA, 0x12, // Set COM pins hardware configuration
		0x81, 0xcf, // Contrast control
		//0xD9, 0x22, // Set Pre-Charge Period  (Not used)
		0xDB, 0x30, // Set VCOMH Deselect Level
		0xA4, // Entire display on (a5)/off(a4)
		0xA6, // Normal (a6)/inverse (a7)
		0x8D, 0x14, // Set Charge Pump
		0xAF, // Display On
		SSD1306_PAGEADDR, 0, 7, // Page setup, start at 0 and end at 7
	};

//uint8_t ssd1306_buffer[64];

////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	funGpioInitAll();
	SystemInit();
	ssd1306_mini_i2c_setup();

	// Trying another mode
	ssd1306_mini_pkt_send( ssd1306_init_array, sizeof(ssd1306_init_array), 1 );

	ba_play_setup( &ctx );
	ba_audio_setup();

	int lasttail = 0;
	int outbuffertail = 0;
	int frame = 0;

	int32_t nextFrame = SysTick->CNT;
	uint32_t freeTime;

	int subframe = 0;

	while(1)
	{
		freeTime = nextFrame - SysTick->CNT;
		while( (int32_t)(SysTick->CNT - nextFrame) < 0 );
		nextFrame += 400000; 
		// 1600000 is 30Hz
		// 800000 is 60Hz
		// 533333 is 90Hz -- 90Hz seems about the max you can go with default 0xd5 settings.
		// 400000 is 120Hz -- Only possible when cranking D5 at 0xF0 and 0xa8 == 0x31

		// Move the cursor "off screen"
		// Scan over two scanlines to hide updates

		ssd1306_mini_pkt_send( 
			(const uint8_t[]){0xD3, 0x32, 0xA8, 0x01,
			// Column start address (0 = reset)
			// Column end address (127 = reset)
			SSD1306_COLUMNADDR, SSD1306_OFFSET, SSD1306_OFFSET+SSD1306_W-1, 0xb0 },
			8, 1 );


		// Send data
		int y;

		ssd1306_mini_i2c_sendstart();
		ssd1306_mini_i2c_sendbyte( SSD1306_I2C_ADDR<<1 );
		ssd1306_mini_i2c_sendbyte( 0x40 ); // Data
		

#if 1
		if( subframe == 3 )
		{
			if( frame == FRAMECT ) asm volatile( "j 0" );
			ba_play_frame( &ctx );
			lasttail = outbuffertail;
			subframe = 0;
			frame++;
		}
#endif

		//Delay_Us(100);
#if 0
		int n;
		uint8_t go = (subframe & 1) ? 0xff : 0x00;
		for( n = 0; n < 6*8*8; n++ )
			ssd1306_mini_i2c_sendbyte( go );
#else
		glyphtype * gm = ctx.curmap;
		for( y = 0; y < 6; y++ )
		{
			int x;
			for( x = 0; x < 8; x++ )
			{
				glyphtype gindex = *(gm++);
				graphictype * g = ctx.glyphdata[gindex];
				
					//int go = (subframe & 1)?0xff:0x00;
				int lg;
				for( lg = 0; lg< 8; lg ++ )
				{
					int go = g[lg];
					if( (subframe)&1 )
						go >>= 8;
					ssd1306_mini_i2c_sendbyte( go );
				}
			}
			//memset( ssd1306_buffer, n, sizeof( ssd1306_buffer ) );
			//int k;
			//for( k = 0; k < sizeof(ssd1306_buffer); k++ ) ssd1306_buffer[k] = frame;
			//ssd1306_mini_data(ssd1306_buffer, sizeof(ssd1306_buffer));
		}
#endif

		ssd1306_mini_i2c_sendstop();

		// Make it so it "would be" off screen but only by 2 pixels.
		// Overscan screen by 2 pixels, but release from 2-scanline mode.
		ssd1306_mini_pkt_send( (const uint8_t[]){0xD3, 0x3e, 0xA8, 0x31}, 4, 1 ); 

#if 1
		outbuffertail = (F_SPS/30*frame) % AUDIO_BUFFER_SIZE;
		ba_audio_fill_buffer( out_buffer_data, outbuffertail );

		for( int n = lasttail; n != outbuffertail; n = (n+1)%AUDIO_BUFFER_SIZE )
		{
			//float fo = out_buffer_data[n] / 128.0 - 1.0;
			//fwrite( &fo, 1, 4, fAudioDump );
			// Do something with out_buffer_data.
		}
#endif
		subframe++;
	}
}

