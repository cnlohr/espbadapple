#include <stdio.h>

#define BITLZSS_IMPLEMENTATION

#include "bitlzss.h"


#include <stdio.h>
#include <string.h>

int main()
{
	const char * stext = "hello world, hello world today this hello world is a test of encoding a string of text today this hello see how it goes.\n";
	int slen = strlen( stext );

	uint8_t encoded_buffer[1024];

	int r = CompressBitsLZSS( stext, slen, encoded_buffer, sizeof(encoded_buffer), 1, 8 );
	printf( "CompressBitsLZSS R: %d\n", r );

	uint8_t decoded_buffer[1024] = { 0 };
	r = DecompressBitsLZSS( encoded_buffer, r, decoded_buffer, sizeof( decoded_buffer ), 1, 8 );
	printf( "DecompresBitsLZSS R: %d\n", r );
	puts( decoded_buffer );

}

