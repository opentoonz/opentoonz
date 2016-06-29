#include <memory>

//#include "tmachine.h"
#include "tsystem.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_gif.h"
#include "trasterimage.h"
#include "qmessagebox.h"
//#include "tiio.h"
//#include "../compatibility/tfile_io.h"
//#include "tpixel.h"

extern "C" {
#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
#include "libavformat\avformat.h"
#include "libavutil\imgutils.h"
}

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

class TImageWriterGif : public TImageWriter {
public:
	int m_frameIndex;

	TImageWriterGif(const TFilePath &path, int frameIndex, TLevelWriterGif *lwg)
		: TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
		m_lwg->addRef();
	}
	~TImageWriterGif() { m_lwg->release(); }

	bool is64bitOutputSupported() override { return false; }
	void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
	TLevelWriterGif *m_lwg;

	// not implemented
	//TImageWriterGif(const TImageWriterGif &);
	//TImageWriterGif &operator=(const TImageWriterGif &src);
};


//===========================================================
//
//  TLevelWriterGif;
//
//===========================================================

TLevelWriterGif::TLevelWriterGif(const TFilePath &path, TPropertyGroup *winfo)
	: TLevelWriter(path, winfo) {
	av_register_all();
	avcodec_register_all();
	//ffmpeg = new ::FfmpegBridge();
	//ffmpeg->init();
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
	
}

//-----------------------------------------------------------

TLevelWriterGif::~TLevelWriterGif()
{
	//delete ffmpeg;
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterGif::getFrameWriter(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildGifExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageWriterP(0);
	int index = fid.getNumber() - 1;
	TImageWriterGif *iwg = new TImageWriterGif(m_path, index, this);
	return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterGif::setFrameRate(double fps)
{
	return;
}

void TLevelWriterGif::saveSoundTrack(TSoundTrack *st)
{
	return;
}

//-----------------------------------------------------------

void TLevelWriterGif::save(const TImageP &img, int frameIndex) {
	if (img->getType() !=TImage::Type::RASTER)
		return;
	TRasterImageP image(img);
	int lx = image->getRaster()->getLx();
	int ly = image->getRaster()->getLy();

	int pixelSize = image->getRaster()->getPixelSize();

	QMutexLocker sl(&m_mutex);

	image->getRaster()->lock();
	uint8_t *buffin = image->getRaster()->getRawData();
	assert(buffin);

	//memcpy(m_buffer, buffin, lx * ly * m_bpp / 8);
	image->getRaster()->unlock();

	std::string status;
		AVCodec *codec;
		AVCodecContext *c = NULL;
		int i, ret, x, y, got_output;
		FILE *f;
		AVFrame *frame;
		AVPacket pkt;
		uint8_t endcode[] = { 0, 0, 1, 0xb7 };
		printf("Encode video file %s\n", m_path);
		/* find the mpeg1 video encoder */
		codec = avcodec_find_encoder((AVCodecID)AV_CODEC_ID_GIF);
		
		if (!codec) {
			fprintf(stderr, "Codec not found\n");
			exit(1);
		}
		c = avcodec_alloc_context3(codec);
		if (!c) {
			status = "Could not allocate video codec context\n";
			fprintf(stderr, "Could not allocate video codec context\n");
			exit(1);
		}
		/* put sample parameters */
		//c->bit_rate = 400000;
		/* resolution must be a multiple of two */
		c->width = lx;
		c->height = ly;
		/* frames per second */
		//c->time_base = (AVRational){ 1, 25 };
		//c->gop_size = 10; /* emit one intra frame every ten frames */
		//c->max_b_frames = 1;
		
		c->pix_fmt = AV_PIX_FMT_BGR8;
		//if (codec_id == AV_CODEC_ID_H264)
		//	av_opt_set(c->priv_data, "preset", "slow", 0);
		/* open it */
		int err = avcodec_open2(c, codec, NULL);
		if (err < 0) {
			status = "Could not open codec" + std::to_string(err);
			fprintf(stderr, "Could not open codec\n");
			//exit(1);
		}
		f = fopen(m_path, "wb");
		if (!f) {
			status = "Could not open";
			fprintf(stderr, "Could not open %s\n", m_path);
			//exit(1);
		}
		frame = av_frame_alloc();
		if (!frame) {
			status = "Could not allocate video frame";
			fprintf(stderr, "Could not allocate video frame\n");
			//exit(1);
		}
		frame->format = AV_PIX_FMT_RGB32;
		frame->width = c->width;
		frame->height = c->height;
		frame->colorspace = AVCOL_SPC_RGB;
		
		/* the image can be allocated by any means and av_image_alloc() is
		* just the most convenient way if av_malloc() is to be used */
		ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
			c->pix_fmt, 32);
		avpicture_fill((AVPicture *)frame, buffin, AV_PIX_FMT_RGB32,
			c->width, c->height);
		if (ret < 0) {
			status = "Could not allocate raw picture buffer";
			fprintf(stderr, "Could not allocate raw picture buffer\n");
			exit(1);
		}
		/* encode 1 second of video */
		//for (i = 0; i < 25; i++) {
			av_init_packet(&pkt);
			pkt.data = NULL;    // packet data will be allocated by the encoder
			pkt.size = 0;
			fflush(stdout);
			/* prepare a dummy image */
			/* Y */
			//for (y = 0; y < c->height; y++) {
			//	for (x = 0; x < c->width; x++) {
			//		frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
			//	}
			//}
			///* Cb and Cr */
			//for (y = 0; y < c->height / 2; y++) {
			//	for (x = 0; x < c->width / 2; x++) {
			//		frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
			//		frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
			//	}
			//}
			//frame->pts = i;
			/* encode the image */
			ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
			if (ret < 0) {
				status = "Error encoding frame";
				fprintf(stderr, "Error encoding frame\n");
				exit(1);
			}
			if (got_output) {
				status = "Write frame";
				printf("Write frame %3d (size=%5d)\n", i, pkt.size);
				fwrite(pkt.data, 1, pkt.size, f);
				av_free_packet(&pkt);
			}
		//}
		/* get the delayed frames */
		for (got_output = 1; got_output; i++) {
			fflush(stdout);
			ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
			if (ret < 0) {
				status = "Error encoding frame";
				fprintf(stderr, "Error encoding frame\n");
				exit(1);
			}
			if (got_output) {
				printf("Write frame %3d (size=%5d)\n", i, pkt.size);
				fwrite(pkt.data, 1, pkt.size, f);
				av_free_packet(&pkt);
			}
		}
		/* add sequence end code to have a real mpeg file */
		//fwrite(endcode, 1, sizeof(endcode), f);
		fclose(f);
		avcodec_close(c);
		av_free(c);
		av_freep(&frame->data[0]);
		av_frame_free(&frame);
		printf("\n");
	
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


Tiio::GifWriterProperties::GifWriterProperties(){}

Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }

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


