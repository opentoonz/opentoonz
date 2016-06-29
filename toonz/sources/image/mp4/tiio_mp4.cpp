#include <memory>

//#include "tmachine.h"
#include "tsystem.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_mp4.h"
#include "trasterimage.h"
#include "qmessagebox.h"
//#include "tiio.h"
//#include "../compatibility/tfile_io.h"
//#include "tpixel.h"

namespace {

	std::string buildGifExceptionString(int rc) {
		return "Unable to create gif.";
	}

}


//===========================================================
//
//  TImageWriterGif
//
//===========================================================

class TImageWriterMp4 : public TImageWriter {
public:
	int m_frameIndex;

	TImageWriterMp4(const TFilePath &path, int frameIndex, TLevelWriterMp4 *lwg)
		: TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
		m_lwg->addRef();
		
	}
	~TImageWriterMp4() { m_lwg->release(); }

	bool is64bitOutputSupported() override { return false; }
	void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
	TLevelWriterMp4 *m_lwg;

	// not implemented
	//TImageWriterGif(const TImageWriterGif &);
	//TImageWriterGif &operator=(const TImageWriterGif &src);
};


//===========================================================
//
//  TLevelWriterGif;
//
//===========================================================

TLevelWriterMp4::TLevelWriterMp4(const TFilePath &path, TPropertyGroup *winfo)
	: TLevelWriter(path, winfo) {
	m_frameCount = 0;
	m_initFinished = false;
	av_register_all();
	avcodec_register_all();
	m_status = "created";
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
	mp4Init();


}

//-----------------------------------------------------------

TLevelWriterMp4::~TLevelWriterMp4()
{
	//delete ffmpeg;
	int numBytes = avpicture_get_size(m_codecContext->pix_fmt, m_codecContext->width, m_codecContext->height);
	int got_packet_ptr = 1;

	int ret;
	//        for(; got_packet_ptr != 0; i++)
	while (got_packet_ptr)
	{
		uint8_t *outbuf = (uint8_t *)malloc(numBytes);

		AVPacket packet;
		av_init_packet(&packet);
		packet.data = outbuf;
		packet.size = numBytes;

		ret = avcodec_encode_video2(m_codecContext, &packet, NULL, &got_packet_ptr);
		if (got_packet_ptr)
		{
			av_interleaved_write_frame(m_formatCtx, &packet);
		}

		av_free_packet(&packet);
		free(outbuf);
	}

	av_write_trailer(m_formatCtx);

	avcodec_close(m_codecContext);
	av_free(m_codecContext);
	printf("\n");
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterMp4::getFrameWriter(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildGifExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageWriterP(0);
	int index = fid.getNumber() - 1;
	TImageWriterMp4 *iwg = new TImageWriterMp4(m_path, index, this);
	return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterMp4::setFrameRate(double fps)
{
	return;
}

void TLevelWriterMp4::saveSoundTrack(TSoundTrack *st)
{
	return;
}

void TLevelWriterMp4::mp4Init() {
	std::string status = "";
	int width = 1920;
	int height = 1080;
	AVOutputFormat *oformat = av_guess_format(NULL, m_path.getQString().toLatin1().data(), NULL);
	if (oformat == NULL)
	{
		oformat = av_guess_format("mp4", NULL, NULL);
	}

	// oformat->video_codec is AV_CODEC_ID_H264
	AVCodec *codec = avcodec_find_encoder(oformat->video_codec);

	m_codecContext = avcodec_alloc_context3(codec);
	m_codecContext->codec_id = oformat->video_codec;
	m_codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	m_codecContext->gop_size = 30;
	m_codecContext->bit_rate = width * height * 4;
	m_codecContext->width = width;
	m_codecContext->height = height;
	AVRational avr = { 1, m_frameRate };
	m_codecContext->time_base = avr;
	m_codecContext->max_b_frames = 1;
	m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

	m_formatCtx = avformat_alloc_context();
	m_formatCtx->oformat = oformat;
	m_formatCtx->video_codec_id = oformat->video_codec;

	//snprintf(m_formatCtx->filename, sizeof(m_formatCtx->filename), "%s", outputFileName);

	AVStream *videoStream = avformat_new_stream(m_formatCtx, codec);
	if (!videoStream)
	{
		printf("Could not allocate stream\n");
	}
	videoStream->codec = m_codecContext;

	if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
	{
		m_codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	avcodec_open2(m_codecContext, codec, NULL);
	avio_open(&m_formatCtx->pb, m_path.getQString().toLatin1().data(), AVIO_FLAG_READ_WRITE);
	if (m_formatCtx->pb == NULL) {
		//cerr << "Could not open for writing" << endl;
		status = "Could not open for writing";
		//return;
	}
	//avio_open(&m_formatCtx->pb, m_path.getQString().toLatin1().data(), AVIO_FLAG_WRITE);
	//avformat_write_header(m_formatCtx, NULL);
	if (avformat_write_header(m_formatCtx, NULL) != 0) {
		status = "Could not write header";
		//return -1;
	}
	m_initFinished = true;
}
//-----------------------------------------------------------

void TLevelWriterMp4::save(const TImageP &img, int frameIndex) {
	TRasterImageP image(img);
	int lx = image->getRaster()->getLx();
	int ly = image->getRaster()->getLy();
	int linesize = image->getRaster()->getRowSize();
	int pixelSize = image->getRaster()->getPixelSize();

	QMutexLocker sl(&m_mutex);

	image->getRaster()->lock();
	uint8_t *buffin = image->getRaster()->getRawData();
	assert(buffin);

	//memcpy(m_buffer, buffin, lx * ly * m_bpp / 8);
	image->getRaster()->unlock();

	AVFrame *frame = av_frame_alloc();
	AVFrame *outFrame = av_frame_alloc();

	/* alloc image and output buffer */

	int inSize = lx * ly;
	int inNumBytes = pixelSize * lx * ly;

	int outSize = m_codecContext->width * m_codecContext->height;
	int outNumBytes = avpicture_get_size(m_codecContext->pix_fmt, m_codecContext->width, m_codecContext->height);


	uint8_t *outbuf = (uint8_t *)malloc(outNumBytes);
	uint8_t *picture_buf = (uint8_t *)av_malloc(outNumBytes);

	int ret = av_image_fill_arrays(frame->data, frame->linesize, picture_buf, AV_PIX_FMT_RGB32, m_codecContext->width, m_codecContext->height, 1);

	frame->data[0] = picture_buf;
	frame->data[1] = frame->data[0] + inSize;
	frame->data[2] = frame->data[1] + inSize / 4;
	frame->linesize[0] = linesize;
	frame->linesize[1] = linesize / 2;
	frame->linesize[2] = linesize / 2;

	int ret2 = av_image_fill_arrays(outFrame->data, outFrame->linesize, picture_buf, m_codecContext->pix_fmt, m_codecContext->width, m_codecContext->height, 1);

	outFrame->data[0] = picture_buf;
	outFrame->data[1] = frame->data[0] + inSize;
	outFrame->data[2] = frame->data[1] + inSize / 4;
	outFrame->linesize[0] = m_codecContext->width;
	outFrame->linesize[1] = m_codecContext->width / 2;
	outFrame->linesize[2] = m_codecContext->width / 2;

	fflush(stdout);

	avpicture_fill((AVPicture *)frame, buffin, AV_PIX_FMT_RGB32,
		m_codecContext->width, m_codecContext->height);

	/*for (int y = 0; y < m_codecContext->height; y++)
	{
		for (int x = 0; x < m_codecContext->width; x++)
		{
			unsigned char b = buffin[(y * m_codecContext->width + x) * 4 + 0];
			unsigned char g = buffin[(y * m_codecContext->width + x) * 4 + 1];
			unsigned char r = buffin[(y * m_codecContext->width + x) * 4 + 2];

			unsigned char Y = (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;

			frame->data[0][y * frame->linesize[0] + x] = Y;

			if (y % 2 == 0 && x % 2 == 0)
			{
				unsigned char V = (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
				unsigned char U = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;

				frame->data[1][y / 2 * frame->linesize[1] + x / 2] = U;
				frame->data[2][y / 2 * frame->linesize[2] + x / 2] = V;
			}
		}
	}*/

	int pts = frameIndex;//(1.0 / 30.0) * 90.0 * frameIndex;

	frame->pts = pts;//av_rescale_q(m_codecContext->coded_frame->pts, m_codecContext->time_base, formatCtx->streams[0]->time_base); //(1.0 / 30.0) * 90.0 * frameIndex;

	struct SwsContext *sws_ctx = NULL;
	int frameFinished;
	//AVPacket packet;
	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(lx,
		ly,
		AV_PIX_FMT_RGB32,
		lx,
		ly,
		AV_PIX_FMT_YUV420P,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

	sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
			frame->linesize, 0, ly,
			(uint8_t* const *)outFrame->data, outFrame->linesize);

	

	int got_packet_ptr;
	AVPacket packet;
	av_init_packet(&packet);
	//packet.data = outbuf;
	//packet.size = outNumBytes;
	//packet.stream_index = m_formatCtx->streams[0]->index;
	//packet.flags |= AV_PKT_FLAG_KEY;
	//packet.pts = packet.dts = pts;
	m_codecContext->coded_frame->pts = pts;

	ret = avcodec_encode_video2(m_codecContext, &packet, outFrame, &got_packet_ptr);
	if (got_packet_ptr != 0)
	{
		m_codecContext->coded_frame->pts = pts;  // Set the time stamp

		if (m_codecContext->coded_frame->pts != (0x8000000000000000LL))
		{
			pts = av_rescale_q(m_codecContext->coded_frame->pts, m_codecContext->time_base, m_formatCtx->streams[0]->time_base);
		}
		packet.pts = pts;
		if (m_codecContext->coded_frame->key_frame)
		{
			packet.flags |= AV_PKT_FLAG_KEY;
		}


		av_interleaved_write_frame(m_formatCtx, &packet);
		av_free_packet(&packet);
	}

	//free(picture_buf);
	//free(outbuf);
	av_free(frame);
	//printf("\n");

	//std::string status;
	//AVCodec *codec;
	//AVCodecContext *c = NULL;
	//int i, ret, x, y, got_output;
	//FILE *f;
	//AVFrame *frame;
	//AVPacket pkt;
	//uint8_t endcode[] = { 0, 0, 1, 0xb7 };
	//printf("Encode video file %s\n", m_path);
	///* find the mpeg1 video encoder */
	//codec = avcodec_find_encoder((AVCodecID)AV_CODEC_ID_H264);

	//if (!codec) {
	//	fprintf(stderr, "Codec not found\n");
	//	exit(1);
	//}
	//c = avcodec_alloc_context3(codec);
	//if (!c) {
	//	status = "Could not allocate video codec context\n";
	//	fprintf(stderr, "Could not allocate video codec context\n");
	//	exit(1);
	//}
	///* put sample parameters */
	////c->bit_rate = 400000;
	///* resolution must be a multiple of two */
	//c->width = lx;
	//c->height = ly;
	///* frames per second */
	////c->time_base = (AVRational){ 1, 25 };
	////c->gop_size = 10; /* emit one intra frame every ten frames */
	////c->max_b_frames = 1;

	//c->pix_fmt = AV_PIX_FMT_YUV420P;
	////if (codec_id == AV_CODEC_ID_H264)
	////	av_opt_set(c->priv_data, "preset", "slow", 0);
	///* open it */
	//int err = avcodec_open2(c, codec, NULL);
	//if (err < 0) {
	//	status = "Could not open codec" + std::to_string(err);
	//	fprintf(stderr, "Could not open codec\n");
	//	//exit(1);
	//}
	//f = fopen(m_path, "wb");
	//if (!f) {
	//	status = "Could not open";
	//	fprintf(stderr, "Could not open %s\n", m_path);
	//	//exit(1);
	//}
	//frame = av_frame_alloc();
	//if (!frame) {
	//	status = "Could not allocate video frame";
	//	fprintf(stderr, "Could not allocate video frame\n");
	//	//exit(1);
	//}
	//frame->format = AV_PIX_FMT_RGB32;
	//frame->width = c->width;
	//frame->height = c->height;
	//frame->colorspace = AVCOL_SPC_RGB;

	///* the image can be allocated by any means and av_image_alloc() is
	//* just the most convenient way if av_malloc() is to be used */
	//ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
	//	c->pix_fmt, 32);
	//avpicture_fill((AVPicture *)frame, buffin, AV_PIX_FMT_RGB32,
	//	c->width, c->height);
	//if (ret < 0) {
	//	status = "Could not allocate raw picture buffer";
	//	fprintf(stderr, "Could not allocate raw picture buffer\n");
	//	exit(1);
	//}
	///* encode 1 second of video */
	//i = 0;
	////for (i = 0; i < 25; i++) {
	//av_init_packet(&pkt);
	//pkt.data = NULL;    // packet data will be allocated by the encoder
	//pkt.size = 0;
	//fflush(stdout);
	///* prepare a dummy image */
	///* Y */
	////for (y = 0; y < c->height; y++) {
	////	for (x = 0; x < c->width; x++) {
	////		frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
	////	}
	////}
	/////* Cb and Cr */
	////for (y = 0; y < c->height / 2; y++) {
	////	for (x = 0; x < c->width / 2; x++) {
	////		frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
	////		frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
	////	}
	////}
	////frame->pts = i;
	///* encode the image */
	//ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
	//if (ret < 0) {
	//	status = "Error encoding frame";
	//	fprintf(stderr, "Error encoding frame\n");
	//	exit(1);
	//}
	//if (got_output) {
	//	status = "Write frame";
	//	printf("Write frame %3d (size=%5d)\n", i, pkt.size);
	//	fwrite(pkt.data, 1, pkt.size, f);
	//	av_free_packet(&pkt);
	//}
	////}
	///* get the delayed frames */
	//for (got_output = 1; got_output; i++) {
	//	fflush(stdout);
	//	ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
	//	if (ret < 0) {
	//		status = "Error encoding frame";
	//		fprintf(stderr, "Error encoding frame\n");
	//		exit(1);
	//	}
	//	if (got_output) {
	//		printf("Write frame %3d (size=%5d)\n", i, pkt.size);
	//		fwrite(pkt.data, 1, pkt.size, f);
	//		av_free_packet(&pkt);
	//	}
	//}
	///* add sequence end code to have a real mpeg file */
	////fwrite(endcode, 1, sizeof(endcode), f);
	//fclose(f);
	//avcodec_close(c);
	//av_free(c);
	//av_freep(&frame->data[0]);
	//av_frame_free(&frame);
	//printf("\n");

	//AVCodec *output = avcodec_find_encoder((AVCodecID)AV_CODEC_ID_GIF);
	//AVCodecContext *contextOutput = avcodec_alloc_context3(output);
	//contextOutput->width = lx;
	//contextOutput->height = ly;
	//contextOutput->pix_fmt = AV_PIX_FMT_RGB24;
	//AVFormatContext* formatCtx = avformat_alloc_context();
	//formatCtx->video_codec_id = (AVCodecID)AV_CODEC_ID_GIF;
	////contextOutput->codec_id = oformat->video_codec;
	////contextOutput->codec_type = AVMEDIA_TYPE_VIDEO;
	////contextOutput->gop_size = 30;
	////contextOutput->bit_rate = width * height * 4
	//
	////contextOutput->time_base = (AVRational){ 1, frameRate };
	////contextOutput->max_b_frames = 1;
	//
	//// Allocate video frame
	////AVOutputFormat *oformat
	//
	////formatCtx->oformat = oformat;
	//
	// 
	//AVFrame *pFrame = av_frame_alloc();
	////AVFrame *frame = av_frame_alloc();
	//// Allocate an AVFrame structure
	//AVFrame* pFrameRGB = av_frame_alloc();
	///*if (pFrameRGB == NULL)
	//	return;
	//*/
	////uint8_t *buffer = NULL;
	////int numBytes;
	//// Determine required buffer size and allocate buffer
	////numBytes = avpicture_get_size(AV_PIX_FMT_RGB32, lx, ly);
	////buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	//// Assign appropriate parts of buffer to image planes in pFrameRGBM
	//// Note that pFrameRGBM is an AVFrame, but AVFrame is a superset
	//// of AVPicture
	//

	///* alloc image and output buffer */

	////int size = contextOutput->width * contextOutput->height;
	////int numBytes = avpicture_get_size(contextOutput->pix_fmt, contextOutput->width, contextOutput->height);

	////uint8_t *outbuf = (uint8_t *)malloc(numBytes);
	////uint8_t *picture_buf = (uint8_t *)av_malloc(numBytes);
	////int* linesize = (int*)(lx * 6);
	////av_image_fill_arrays(pFrame->data, linesize, buffin, AV_PIX_FMT_RGB32, lx, ly, 1);
	//avpicture_fill((AVPicture *)pFrame, buffin, AV_PIX_FMT_RGB32,lx, ly);
	//
	//
	//avpicture_fill((AVPicture *)pFrameRGB, buffin, AV_PIX_FMT_RGB24,
	//	lx, ly);
	//avcodec_send_frame(contextOutput, pFrame);

	//struct SwsContext *sws_ctx = NULL;
	//int frameFinished;
	////AVPacket packet;
	//// initialize SWS context for software scaling
	//sws_ctx = sws_getContext(lx,
	//	ly,
	//	AV_PIX_FMT_RGB32,
	//	lx,
	//	ly,
	//	AV_PIX_FMT_RGB24,
	//	SWS_BILINEAR,
	//	NULL,
	//	NULL,
	//	NULL
	//	);

	//int i = 0;


	//

	////av_opt_set_double(coutput->priv_data, "max-intra-rate", 90, 0);
	////av_opt_set(coutput->priv_data, "quality", "realtime", 0);

	////while (av_read_frame(pFormatContext, &packet) >= 0) {
	//	// Is this a packet from the video stream?
	//	//if (packet.stream_index == videoStream) {
	//		// Decode video frame
	//		//avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

	//		// Did we get a video frame?
	//		//if (frameFinished) {
	//			// Convert the image from its native format to RGB
	//			sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
	//				pFrame->linesize, 0, ly,
	//				pFrameRGB->data, pFrameRGB->linesize);

	//			// Save the frame to disk
	//			if (++i <= 48)
	//				SaveFrame(pFrameRGB, lx,
	//				ly, i);
	//		//}
	//	//}

	//	// Free the packet that was allocated by av_read_frame
	//	//av_free_packet(&packet);
	//	// Free the RGB image
	//	av_free(buffin);
	//	av_free(pFrameRGB);

	//	// Free the YUV frame
	//	av_free(pFrame);

	//	// Close the codecs
	//	//avcodec_close(pCodecCtx);
	//	//avcodec_close(pCodecCtxOrig);

	//	// Close the video file
	//	//avformat_close_input(&pFormatContext);
	////}

	//

	//return;




}


Tiio::Mp4WriterProperties::Mp4WriterProperties(){}

Tiio::Reader* Tiio::makeMp4Reader(){ return nullptr; }
Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; }

//===========================================================
//
//  FfmpegBridge;
//
//===========================================================
/*

class FfmpegBridge
{
public:
FfmpegBridge(){};
void init() {
av_register_all();
avcodec_register_all();

}
};






void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
FILE *pFile;
char szFilename[32];
int  y;

// Open file
sprintf(szFilename, "ballframe%d.gif", iFrame);
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

*/


