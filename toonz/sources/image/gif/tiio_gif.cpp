#include <memory>

//#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_gif.h"
//#include "tiio.h"
//#include "../compatibility/tfile_io.h"
//#include "tpixel.h"


Tiio::GifWriterProperties::GifWriterProperties(){}

Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }










int TnzFfmpeg::TestFfmpeg() {
av_register_all();
avcodec_register_all();
TFilePath* path();

AVFormatContext* pFormatContext = nullptr;

// Open video file
if (avformat_open_input(&pFormatContext, "file:D:\\MyCode\\opentoonzbuild\\RelWithDebInfo\\ball.avi", NULL, 0) != 0)
return -1; // Couldn't open file

// Retrieve stream information
if (avformat_find_stream_info(pFormatContext, NULL)<0)
return -1; // Couldn't find stream information

int i;
AVCodecContext *pCodecCtxOrig = NULL;
AVCodecContext *pCodecCtx = NULL;

// Find the first video stream
int videoStream = -1;
for (i = 0; i<pFormatContext->nb_streams; i++)
if (pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
videoStream = i;
break;
}
if (videoStream == -1)
return -1; // Didn't find a video stream

// Get a pointer to the codec context for the video stream
pCodecCtxOrig = pFormatContext->streams[videoStream]->codec;

AVCodec *pCodec = NULL;

// Find the decoder for the video stream
pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
if (pCodec == NULL) {
fprintf(stderr, "Unsupported codec!\n");
return -1; // Codec not found
}
// Copy context
pCodecCtx = avcodec_alloc_context3(pCodec);
if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
fprintf(stderr, "Couldn't copy codec context");
return -1; // Error copying codec context
}
// Open codec
if (avcodec_open2(pCodecCtx, pCodec, NULL)<0)
return -1; // Could not open codec

AVFrame *pFrame = NULL;

// Allocate video frame
pFrame = av_frame_alloc();

// Allocate an AVFrame structure
AVFrame* pFrameRGB = av_frame_alloc();
if (pFrameRGB == NULL)
return -1;

uint8_t *buffer = NULL;
int numBytes;
// Determine required buffer size and allocate buffer
numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
pCodecCtx->height);
buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

// Assign appropriate parts of buffer to image planes in pFrameRGB
// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
// of AVPicture
avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
pCodecCtx->width, pCodecCtx->height);


struct SwsContext *sws_ctx = NULL;
int frameFinished;
AVPacket packet;
// initialize SWS context for software scaling
sws_ctx = sws_getContext(pCodecCtx->width,
pCodecCtx->height,
pCodecCtx->pix_fmt,
pCodecCtx->width,
pCodecCtx->height,
AV_PIX_FMT_RGB24,
SWS_BILINEAR,
NULL,
NULL,
NULL
);

i = 0;
while (av_read_frame(pFormatContext, &packet) >= 0) {
// Is this a packet from the video stream?
if (packet.stream_index == videoStream) {
// Decode video frame
avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

// Did we get a video frame?
if (frameFinished) {
// Convert the image from its native format to RGB
sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
pFrame->linesize, 0, pCodecCtx->height,
pFrameRGB->data, pFrameRGB->linesize);

// Save the frame to disk
if (++i <= 48)
SaveFrame(pFrameRGB, pCodecCtx->width,
pCodecCtx->height, i);
}
}

// Free the packet that was allocated by av_read_frame
av_free_packet(&packet);
}

// Free the RGB image
av_free(buffer);
av_free(pFrameRGB);

// Free the YUV frame
av_free(pFrame);

// Close the codecs
avcodec_close(pCodecCtx);
avcodec_close(pCodecCtxOrig);

// Close the video file
avformat_close_input(&pFormatContext);

return 0;


}


void TnzFfmpeg::SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
FILE *pFile;
char szFilename[32];
int  y;

// Open file
sprintf(szFilename, "ballframe%d.ppm", iFrame);
pFile = fopen(szFilename, "wb");
if (pFile == NULL)
return;

// Write header
fprintf(pFile, "P6\n%d %d\n255\n", width, height);

// Write pixel data
for (y = 0; y<height; y++)
fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

// Close file
fclose(pFile);
}

