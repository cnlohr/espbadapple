#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libavutil/buffer.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/imgutils.h>

//from decoding_encoding.c

#ifndef PIX_FMT_RGB24
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#endif

#include <string.h>
#include <stdlib.h>

#define INBUF_SIZE 4096

static int request_w, request_h;
struct SwsContext *resize;
AVFrame* frame2;
char * frame2data;


void setup_video_decode()
{
	// avcodec_register_all();
	// av_register_all();
	//printf( "REGISTERING\n" );
}

void got_video_frame( unsigned char * rgbbuffer, int linesize, int width, int height, int frame );


static int decode_write_frame( AVCodecContext *avctx,
							  AVFrame *frame, int *frame_count, AVPacket *pkt, int last, AVFrame* encoderRescaledFrame)
{
	int len = 0, got_frame = 0;

	do
	{
		if( pkt->size )
		{
			int sub = avcodec_send_packet( avctx, pkt );
		}
		av_frame_make_writable( frame );

		len = avcodec_receive_frame(avctx, frame );
		if( len == -11 ) break;
		if (len < 0) {
			fprintf(stderr, "Error while decoding frame %d (%d)\n", *frame_count, len);
			return len;
		}
		if( len == 0 )
		{
			got_frame = 1;
			len = pkt->size;
		}

		if( request_w == 0 && request_h == 0 ) { request_w = avctx->width; request_h = avctx->height; }

		if (got_frame) {
			int width2 = avctx->width;
			int height2 = avctx->height;

			if( resize == 0 )
				resize = sws_getContext(avctx->width, avctx->height, AV_PIX_FMT_YUV420P, width2, height2, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

			frame2 = av_frame_alloc();
			frame2->width = avctx->width;
			frame2->height = avctx->height;
			frame2->format = PIX_FMT_RGB24;
			av_frame_make_writable( frame2 );
			int outlen = av_image_get_buffer_size( PIX_FMT_RGB24, width2, height2, 1 );
			frame2data = av_malloc(outlen);
			av_image_fill_arrays( frame2->data, frame2->linesize, (uint8_t*)frame2data, PIX_FMT_RGB24, width2, height2, 1 );
			int r2 = sws_scale(resize, (const uint8_t * const*)frame->data, frame->linesize, 0, avctx->height, frame2->data, frame2->linesize);
			int r = av_frame_get_buffer( frame, 1 );

			if( r2 != height2 || r != 0 )
			{
				fprintf( stderr, "ERROR: Cannot get buffer!\n" );
			}
			else
			{
//				printf( "%p %08x %d %d %d %d\n", frame->data, ((uint32_t*)frame2->data[0])[2] ,r, r2, frame2->linesize[0], *frame_count );
				got_video_frame( ((uint8_t*)frame2->data[0]), frame2->linesize[0],width2, height2, *frame_count);
//				printf( "%p %08x %d %d %d\n", frame->data, ((uint32_t*)frame2->data[0])[2] ,r, r2, frame2->linesize[0] );
			}
			av_freep( &frame2->data[0] );
			av_frame_free( &frame2 );

		}
//skip_frame_got:
		if (pkt->data) {
			pkt->size -= len;
			pkt->data += len;
		}
	} while(1);
	return 0;
}


int video_decode( const char *filename, int reqw, int reqh)
{
	request_w = reqw;
	request_h = reqh;

	AVFrame* encoderRescaledFrame;
	AVFormatContext *fmt_ctx = 0;
	AVCodecContext *dec_ctx = 0;
	int video_stream_index = 0;
	int frame_count = 0;
	AVFrame *frame;
	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	AVPacket avpkt;
	int ret;
	int i;
	const AVCodec *dec;

	av_new_packet(&avpkt, sizeof( avpkt));


	/* set end of buffer to 0 (this ensures that no overreading happens for damaged mpeg streams) */
	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	printf( "Opening: %s\n", filename );
	if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return ret;
	}

//	dump_format(fmt_ctx, 0, filename, 0);

	/* select the video stream */
/*	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return ret;
	}*/

	for (i = 0 ; i < fmt_ctx->nb_streams; i++){
		printf( "%d\n", i );
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ){
			video_stream_index = i;
			break;
		}

	}

    AVCodecParameters* inputCodecParameters = fmt_ctx->streams[video_stream_index]->codecpar;
    dec = avcodec_find_decoder(inputCodecParameters->codec_id);
    dec_ctx = avcodec_alloc_context3(dec);
    if (avcodec_parameters_to_context(dec_ctx, inputCodecParameters) != 0)
    {
        fprintf( stderr, "Error: Can't get stream context\n" );
        return -1;
    }


	printf( "DEC CTX: %p id %d\n", dec_ctx, dec_ctx?dec_ctx->codec_id:0 );
	printf( "Dec: %p\n", dec );

	encoderRescaledFrame = av_frame_alloc();

	printf( "Stream index: %d (%p %p)\n", video_stream_index, dec_ctx, dec );

	/* init the video decoder */
	if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
		return ret;
	}

	printf( "OPENING: %d %s\n", ret, filename );

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	frame_count = 0;
	avpkt.data = NULL;
	avpkt.size = 0;
//	decode_write_frame( dec_ctx, frame, &frame_count, &avpkt, 1);

	avpkt.data = inbuf;
//	memset( &avpkt, 0, sizeof( avpkt ) );  Nope.
	while( av_read_frame(fmt_ctx, &avpkt) >= 0 )
	{
		if (avpkt.stream_index == video_stream_index)
		{
			while (avpkt.size > 0)
			{
				if (decode_write_frame( dec_ctx, frame, &frame_count, &avpkt, 0, encoderRescaledFrame) < 0)
					continue;
				frame_count++;
			}
		}
		else
		{
		}
//Uuhhh... we should need this?
//		av_free_packet( &avpkt );
	}


	if (dec_ctx)
		avcodec_close(dec_ctx);
	avformat_close_input(&fmt_ctx);
	av_frame_free(&frame);
	printf( "Done.\n" );
	return 0;
}


/*
int video_decode( const char *filename)
{
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	AVFrame *pFrameRGB;
	AVFrame *pFrame;
	uint8_t *buffer;
	int numBytes;
	int i;
	int frameFinished;
	int videoStream;
	AVPacket packet;

	if(av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL)!=0)
		return -1; // Couldn't open file
	if(av_find_stream_info(pFormatCtx)<0)
		return -1; // Couldn't find stream information

	dump_format(pFormatCtx, 0, filename, 0);

	// Find the first video stream
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
	{
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}

	if(videoStream==-1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;


	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open(pCodecCtx, pCodec)<0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame=avcodec_alloc_frame();

	// Allocate an AVFrame structure
	pFrameRGB=avcodec_alloc_frame();
	if(pFrameRGB==NULL)
	  return -1;

	numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
							pCodecCtx->height);
	buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
		pCodecCtx->width, pCodecCtx->height);

	i=0;
	while(av_read_frame(pFormatCtx, &packet)>=0)
	{
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream)
		{

			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
				&packet);

			// Did we get a video frame?
			if(frameFinished) {
				// Convert the image from its native format to RGB
				img_convert((AVPicture *)pFrameRGB, PIX_FMT_RGB24, 
					(AVPicture*)pFrame, pCodecCtx->pix_fmt, 
					pCodecCtx->width, pCodecCtx->height);

				// callback!
				got_video_frame( pFrameRGB, pFrameRGB->linesize[0], pCodecCtx->width, 
						pCodecCtx->height, i);
				i++;
			}
		}
	}

	// Free the packet that was allocated by av_read_frame
	av_free_packet(&packet);
	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	av_close_input_file(pFormatCtx);

	return 0;
}

*/

