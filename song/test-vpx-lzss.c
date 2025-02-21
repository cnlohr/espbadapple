#include <stdint.h>
#include <stdio.h>

#include "ba_play_audio.h"

int main()
{
	SetupAudio();
	do
	{
		notetype nt = PullNote();
		printf( "%04x %d\n", nt, nindex );
	}
	while( nindex < bas_songlen_notes );
	printf( "%d/%d\n",nindex, bas_songlen_notes ); 
	return 0;
}


