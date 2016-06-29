#pragma once

#ifndef TTIO_MP4_INCLUDED
#define TTIO_MP4_INCLUDED

extern "C" {
#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
#include "libavformat\avformat.h"
#include "libavutil\imgutils.h"
}

#include "tproperty.h"
#include "tlevel_io.h"
#include "tthreadmessage.h"

//===========================================================
//
//  TLevelWriterGif
//
//===========================================================

class TLevelWriterMp4 : public TLevelWriter {

public:
	TLevelWriterMp4(const TFilePath &path, TPropertyGroup *winfo);
	~TLevelWriterMp4();
	//FfmpegBridge* ffmpeg;
	void setFrameRate(double fps);

	TImageWriterP getFrameWriter(TFrameId fid) override;
	void save(const TImageP &image, int frameIndex);

	void saveSoundTrack(TSoundTrack *st);

	static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
		return new TLevelWriterMp4(path, winfo);
	}

private:
	int IOError;
	int m_bpp;
	int m_frameCount;
	AVCodecContext* m_codecContext;
	AVFormatContext	* m_formatCtx;
	void *m_buffer;
	bool m_initFinished;
	void mp4Init();
	std::string m_status;
	TThread::Mutex m_mutex;
};

//===========================================================================

namespace Tiio {

//===========================================================================

class Mp4WriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  //TBoolProperty m_matte;

  Mp4WriterProperties();
};



//===========================================================================

Tiio::Reader *makeMp4Reader();
Tiio::Writer *makeMp4Writer();

}  // namespace

#endif
