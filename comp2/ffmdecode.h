#ifndef _FFMDECODE_H
#define _FFMDECODE_H

void setup_video_decode();
int video_decode( const char *filename, int rw, int rh ); //set rw and rh to 0 for native resolution.

#endif


