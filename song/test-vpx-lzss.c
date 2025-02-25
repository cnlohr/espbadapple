#include <stdint.h>
#include <stdio.h>

#include "ba_play_audio.h"

int main()
{
	ba_audio_setup( &ba_player );
	int nindex = 0;
	do
	{
		notetype nt = ba_audio_pull_note( &ba_player );
		printf( "%04x %d\n", nt, nindex );
		nindex++;
	}
	while( nindex < bas_songlen_notes );
	printf( "%d/%d\n",nindex, bas_songlen_notes ); 
	return 0;
}


