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
	av_register_all();
	avcodec_register_all();
	m_status = "created";
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
	mp4Init();


}

//-----------------------------------------------------------

TLevelWriterMp4::~TLevelWriterMp4()
{
	int status = 0;
	int numBytes = avpicture_get_size(m_codecContext->pix_fmt, m_codecContext->width, m_codecContext->height);
	int got_packet_ptr = 1;

	int ret;
	
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

	status = av_write_trailer(m_formatCtx);
	if (status < 0)
		exit(1);

	avcodec_close(m_codecContext);
	av_free(m_codecContext);
	avio_close(m_formatCtx->pb);
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
	m_fps = fps;
	m_frameRate = fps;
}

void TLevelWriterMp4::saveSoundTrack(TSoundTrack *st)
{
	return;
}

void TLevelWriterMp4::mp4Init() {
	std::string status = "";
	//these need to be replaced with actual values from the camera
	int width = 1920;
	int height = 1080;

	//get an output format
	AVOutputFormat *oformat = av_guess_format(NULL, m_path.getQString().toLatin1().data(), NULL);
	if (oformat == NULL)
	{
		oformat = av_guess_format("mp4", NULL, NULL);
	}

	// oformat->video_codec is AV_CODEC_ID_H264
	AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);

	m_codecContext = avcodec_alloc_context3(codec);
	m_codecContext->codec_id = AV_CODEC_ID_H264;
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
		
		status = "Could not open for writing";
		
	}
	//avio_open(&m_formatCtx->pb, m_path.getQString().toLatin1().data(), AVIO_FLAG_WRITE);
	//avformat_write_header(m_formatCtx, NULL);
	if (avformat_write_header(m_formatCtx, NULL) != 0) {
		status = "Could not write header";
		
	}
}
//-----------------------------------------------------------

void TLevelWriterMp4::save(const TImageP &img, int frameIndex) {
	TRasterImageP image(img);
	int lx = image->getRaster()->getLx();
	int ly = image->getRaster()->getLy();
	m_bpp = image->getRaster()->getPixelSize();
	int linesize = image->getRaster()->getRowSize();
	int pixelSize = image->getRaster()->getPixelSize();

	//what does this do?
	QMutexLocker sl(&m_mutex);

	//lock raster to get data
	image->getRaster()->lock();
	uint8_t *buffin = image->getRaster()->getRawData();
	assert(buffin);

	//memcpy(m_buffer, buffin, lx * ly * m_bpp);
	image->getRaster()->unlock();

	AVFrame *inFrame = av_frame_alloc();
	AVFrame *outFrame = av_frame_alloc();

	/* alloc image and output buffer */

	int inSize = lx * ly;
	int inNumBytes = pixelSize * lx * ly;

	int outSize = m_codecContext->width * m_codecContext->height;
	int outNumBytes = avpicture_get_size(m_codecContext->pix_fmt, m_codecContext->width, m_codecContext->height);

	uint8_t *inbuf = (uint8_t *)av_malloc(inNumBytes);
	uint8_t *inPicture_buf = (uint8_t *)av_malloc(inNumBytes);
		
	uint8_t *outbuf = (uint8_t *)av_malloc(outNumBytes);
	uint8_t *outPicture_buf = (uint8_t *)av_malloc(outNumBytes);

	int ret = av_image_fill_arrays(inFrame->data, inFrame->linesize, buffin, AV_PIX_FMT_RGB32, m_codecContext->width, m_codecContext->height, 1);

	//RGB only has one data plane
	inFrame->data[0] = inPicture_buf;
	inFrame->linesize[0] = linesize;
	
	int ret2 = av_image_fill_arrays(outFrame->data, outFrame->linesize, outPicture_buf, m_codecContext->pix_fmt, m_codecContext->width, m_codecContext->height, 1);

	//YUV data has one for each y, u, v
	outFrame->data[0] = outPicture_buf;
	outFrame->data[1] = outFrame->data[0] + outSize;
	outFrame->data[2] = outFrame->data[1] + outSize / 4;
	outFrame->linesize[0] = m_codecContext->width;
	outFrame->linesize[1] = m_codecContext->width / 2;
	outFrame->linesize[2] = m_codecContext->width / 2;

	fflush(stdout);

	//avpicture_fill((AVPicture *)inFrame, buffin, AV_PIX_FMT_RGB32,
	//	m_codecContext->width, m_codecContext->height);

	

	int pts = frameIndex;

	inFrame->pts = pts;
	
	//set up scaling context
	struct SwsContext *sws_ctx = NULL;
	int frameFinished;

	
	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(lx,
		ly,
		AV_PIX_FMT_RGB32,
		lx,
		ly,
		m_codecContext->pix_fmt,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

	//convert from RGBA to YUV420P
	sws_scale(sws_ctx, (uint8_t const * const *)inFrame->data,
			inFrame->linesize, 0, ly,
			(uint8_t* const *)outFrame->data, outFrame->linesize);
	
	

	int got_packet_ptr;
	AVPacket packet;
	av_init_packet(&packet);	
	packet.data = outbuf;		//if I comment these out
	packet.size = outNumBytes;	//the packet is empty
	//packet.stream_index = m_formatCtx->streams[0]->index;
	packet.flags |= AV_PKT_FLAG_KEY;
	packet.pts = packet.dts = pts;
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

		//nothing to write
		av_interleaved_write_frame(m_formatCtx, &packet);
		av_free_packet(&packet);
	}
	else {
		exit(1);
	}
	av_free(inbuf);
	av_free(inPicture_buf);
	av_free(outPicture_buf);
	av_free(outbuf);
	av_free(inFrame);
	av_free(outFrame);
	
}


Tiio::Mp4WriterProperties::Mp4WriterProperties(){}

Tiio::Reader* Tiio::makeMp4Reader(){ return nullptr; }
Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; }