#include "ch32fun.h"
#include <stdio.h>

// Physical configuration:
//  
// PD3/PD4 -> Differential output.
// 
// PD3 - 100uF - 220 Ohm -+-- Output +
//                        |
// PD4 -+- 220nF ---+-----+-- Output -
//      |           |
//      +- 100 Ohm -+

#define F_SPS (48000000/256/2) // Confirmed 256.

#define WARNING(x...)

#define BADATA_DECORATOR const __attribute__((section(".fixedflash")))
#define BAS_DECORATOR    const __attribute__((section(".fixedflash")))

#include "ba_play.h"
#include "ba_play_audio.h"

volatile uint8_t out_buffer_data[AUDIO_BUFFER_SIZE];
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
	//asm volatile( "nop\nnop\n");

#include "ssd1306mini.h"

const uint8_t ssd1306_init_array[] __attribute__((section(".fixedflash"))) =
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
	0xD9, 0x22, // Set Pre-Charge Period  (Not used)
	0xDB, 0x30, // Set VCOMH Deselect Level
	0xA4, // Entire display on (a5)/off(a4)
	0xA6, // Normal (a6)/inverse (a7)
	0x8D, 0x14, // Set Charge Pump
	0xAF, // Display On
	SSD1306_PAGEADDR, 0, 7, // Page setup, start at 0 and end at 7
};

//uint8_t ssd1306_buffer[64];

////////////////////////////////////////////////////////////////////////////////////////////

#if 0
void EmitEdge( uint32_t gou, uint32_t got, uint32_t gon, uint32_t subframe )
{
	int gouah = got | gou;
	int goual = got ^ gou; //Half-adder
	int gonah = got | gon;
	int gonal = got ^ gon; //Half-adder

	if( (subframe)&1 )
	{
		got>>= 8;
		gou>>= 8;
	}

	int go = 0;
	ssd1306_mini_i2c_sendbyte( go );
}
#endif

uint16_t pixelmap[64*6];

//pixelmap[64*6];
int pmp;
// From ttablegen.c
const uint8_t potable[16]  __attribute__((section(".fixedflash"))) = { 0x50, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xf9, 0xf9, 0xa4, 0xf9, 0xf9, 0xf9, 0xa4, 0xf9, 0xf9, 0xf9, };

void PMEmit( uint16_t pvo )
{

	// If we only cared about left/right blurring, we could just say 
	//	pixelmap[pmp++] = pvo; ... but we don't.
	pixelmap[pmp++] = pvo;
}

void EmitEdge( graphictype tgprev, graphictype tg, graphictype tgnext )
{
	// This should only need +2 regs (or 3 depending on how the optimizer slices it)
	// (so all should fit in working reg space)
	graphictype A = tgprev >> 8;
	graphictype B = tgprev;      // implied & 0xff
	graphictype C = tgnext >> 8;
	graphictype D = tgnext;      // implied & 0xff

	graphictype E = (B&C)|(A&D); // 8 bits worth of MSB of (next+prev+1)/2
	graphictype F = D|B;         // 8 bits worth of LSB of (next+prev+1)/2

	graphictype G = tg >> 8;
	graphictype H = tg;          // implied & 0xff

	//if( subframe )
	int tghi = (F&G)|(E&H);     // 8 bits worth of MSB of this+(next+prev+1)/2-1
	//else
	int tglo = G|E|(F&H);       // 8 bits worth of MSB|LSB of this+(next+prev+1)/2-1

	//ssd1306_mini_i2c_sendbyte( tg );
	PMEmit( (tghi << 8) | tglo );
}



void UpdatePixelMap()
{
	int y;
	glyphtype * gm = ctx.curmap;
	pmp = 0;
	for( y = 0; y < 6; y++ )
	{
		int x;
		for( x = 0; x < 8; x++ )
		{
			glyphtype gindex = *(gm);
			graphictype * g     = ctx.glyphdata[gindex];
			graphictype * gprev = ctx.glyphdata[gm[-1]];
			graphictype * gnext = ctx.glyphdata[gm[ 1]];
			gm++;
			
			//int go = (subframe & 1)?0xff:0x00;
			int lg;
			{
				uint32_t got = g[0];
				uint32_t gon = g[1];
				uint32_t gou = (x>0)?gprev[7]:gon;

				EmitEdge( gou, got, gon );
			}
			for( lg = 1; lg < 7; lg ++ )
			{
				int go = g[lg];

				// Bits in this word scan left-to-right.
				// LG is "x" from left-to-right
				//if( (subframe)&1 )
				//	go >>= 8;
				//ssd1306_mini_i2c_sendbyte( go );
				PMEmit( go );
			}
			{
				uint32_t got = g[7];
				uint32_t gou = g[6];
				uint32_t gon = (x<8)?gnext[0]:gou;
				EmitEdge( gou, got, gon );
			}
		}
	}
	
}

int main()
{
	funGpioInitAll();
	SystemInit();
	ssd1306_mini_i2c_setup();

	// Trying another mode
	ssd1306_mini_pkt_send( ssd1306_init_array, sizeof(ssd1306_init_array), 1 );

	DMA1_Channel2->CFGR = 0;

	ba_play_setup( &ctx );
	ba_audio_setup();

	int frame = 0;

	int32_t nextFrame = SysTick->CNT;

	int subframe = 0;

	// Setup PD3/PD4 as TIM2_CH1/TIM2_CH2 as output
	// TIM2_RM=111 (Full)
	RCC->APB1PCENR |= RCC_APB1Periph_TIM2;
	RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;

	AFIO->PCFR1 = AFIO_PCFR1_TIM2_RM_0 | AFIO_PCFR1_TIM2_RM_1 | AFIO_PCFR1_TIM2_RM_2;

	TIM2->PSC = 0x0001;
	TIM2->ATRLR = 255; // Confirmed: 255 here = PWM period of 256.

	// for channel 1 and 2, let CCxS stay 00 (output), set OCxM to 110 (PWM I)
	// enabling preload causes the new pulse width in compare capture register only to come into effect when UG bit in SWEVGR is set (= initiate update) (auto-clears)
	TIM2->CHCTLR1 = 
		TIM2_CHCTLR1_OC1M_2 | TIM2_CHCTLR1_OC1M_1 | TIM2_CHCTLR1_OC1PE |
		TIM2_CHCTLR1_OC2M_2 | TIM2_CHCTLR1_OC2M_1 | TIM2_CHCTLR1_OC2PE;

	// Enable Channel outputs, set default state (based on TIM2_DEFAULT)
	TIM2->CCER = TIM2_CCER_CC1E // | (TIM_CC1P & TIM2_DEFAULT);
	           | TIM2_CCER_CC2E;// | (TIM_CC2P & TIM2_DEFAULT);

	// initialize counter
	TIM2->SWEVGR = TIM2_SWEVGR_UG | TIM2_SWEVGR_TG;
	TIM2->DMAINTENR = TIM1_DMAINTENR_TDE | TIM1_DMAINTENR_UDE;

	// CTLR1: default is up, events generated, edge align
	// enable auto-reload of preload
	TIM2->CTLR1 = TIM2_CTLR1_ARPE | TIM2_CTLR1_CEN;

	// Enable TIM2

	TIM2->CH1CVR = 128;
	TIM2->CH2CVR = 128; 

	// Weeeird... PUPD works better than GPIO_CFGLR_OUT_AF_PP
	funPinMode( PD3, GPIO_CFGLR_IN_PUPD );
	funPinMode( PD4, GPIO_CFGLR_IN_PUPD );
	funPinMode( PD3, GPIO_CFGLR_OUT_AF_PP );
	funPinMode( PD4, GPIO_CFGLR_OUT_AF_PP );


	funPinMode( PD6, GPIO_CFGLR_OUT_PP );

	while(1)
	{
		//PrintHex( DMA1_Channel2->CNTR );
		//TIM2->CH1CVR = subframe * 5;
		//out_buffer_data[frame&(AUDIO_BUFFER_SIZE-1)] = (frame&0x15)+128;

		if( subframe == 4 )
		{
			if( frame == FRAMECT ) asm volatile( "j 0" );
			if( frame == 46 )
			{
				// Start playing music at frame 37.
				// Triggered off TIM2UP
				DMA1_Channel2->CNTR = AUDIO_BUFFER_SIZE;
				DMA1_Channel2->MADDR = (uint32_t)out_buffer_data;
				DMA1_Channel2->PADDR = (uint32_t)&TIM2->CH2CVR; // This is the output register for out buffer.
				DMA1_Channel2->CFGR = 
					DMA_CFGR1_DIR |                      // MEM2PERIPHERAL
					DMA_CFGR1_PL_0 |                     // Med priority.
					0 |                                  // 8-bit memory
					DMA_CFGR1_PSIZE_0 |                  // 16-bit peripheral  XXX TRICKY XXX You MUST do this when writing to a timer.
					DMA_CFGR1_MINC |                     // Increase memory.
					DMA_CFGR1_CIRC |                     // Circular mode.
					DMA_CFGR1_EN;                        // Enable
			}
			ba_play_frame( &ctx );
			subframe = 0;
			frame++;
		}
		else if( subframe == 1 )
		{
			UpdatePixelMap();
		}

		funDigitalWrite( PD6, 0 );

		while( (int32_t)(SysTick->CNT - nextFrame) < 0 )
		{
		}

		funDigitalWrite( PD6, 1 );

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

		funDigitalWrite( PD6, 0 );

		if( 1 ) // New, filtered output.
		{

			int pvx;
			int pvy;

			int i;
			uint16_t * pmo = pixelmap;
			for( i = 0; i < sizeof(pixelmap)/2; i++ )
			{
				if( pvx == 64 ) { pvx = 0; pvy++; }
				int pvo = pmo[i];
				uint16_t pvr = pvo & 0x7e7e;

				int pprev, pnext, pthis;

				if( i < 64 )
					pprev = ((pvo>>8)&2) | ((pvo>>1)&1);
				else
				{
					int kpre = pmo[i-64];
					pprev = ((kpre>>14)&2) | ((kpre>>7)&1);
				}
				pnext = ((pvo>>8)&2) | ((pvo>>1)&1);

				pprev = pnext;

				pthis = ((pvo>>7)&2) | ((pvo>>0)&1);

				int pol = (potable[pnext+pprev*4]>>pthis)&3;
				pvr |= (pol & 1) | (pol&2)<<7;

				if( i >= 256 )
					pnext = ((pvo>>13)&2) | ((pvo>>6)&1);
				else
				{
					int knext = pmo[i+64];
					pnext = ((knext>>7)&2) | (knext &1);
				}
				pprev = ((pvo>>13)&2) | ((pvo>>6)&1);
				pthis = ((pvo>>14)&2) | ((pvo>>7)&1);

				pol = (potable[pnext+pprev*4]>>pthis)&3;
				pvr |= (pol & 1)<<7 | (pol&2)<<14;

				//pixelbase[pmp] = pvo;
				uint16_t po = pvr;

				if( subframe & 1 )
					ssd1306_mini_i2c_sendbyte( po>>8 );
				else
					ssd1306_mini_i2c_sendbyte( po );
			}
		}
		else
		{

			glyphtype * gm = ctx.curmap;
			// 2.59ms
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
		}
		funDigitalWrite( PD6, 1 );

		ssd1306_mini_i2c_sendstop();

		// Make it so it "would be" off screen but only by 2 pixels.
		// Overscan screen by 2 pixels, but release from 2-scanline mode.
		ssd1306_mini_pkt_send( (const uint8_t[]){0xD3, 0x3e, 0xA8, 0x31}, 4, 1 ); 

		int v = AUDIO_BUFFER_SIZE - DMA1_Channel2->CNTR - 1;
		if( v < 0 ) v = 0;
		//PrintHex( out_buffer_data[0] );
		//outbuffertail += 400;//(F_SPS/120*frame) % AUDIO_BUFFER_SIZE;
		//if( outbuffertail >= AUDIO_BUFFER_SIZE ) outbuffertail -= AUDIO_BUFFER_SIZE;
		ba_audio_fill_buffer( out_buffer_data, v );

		funDigitalWrite( PD6, 0 );
		subframe++;
		funDigitalWrite( PD6, 1 );
	}
}

