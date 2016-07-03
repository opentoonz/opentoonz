#pragma once

#ifndef TTIO_MP4_INCLUDED
#define TTIO_MP4_INCLUDED

extern "C" {
//#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
//#include "libavformat\avformat.h"
//#include "libavutil\imgutils.h"
#include <math.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
//#include <libavutil/channel_layout.h>
//#include <libavutil/common.h>
#include <libavutil/imgutils.h>
//#include <libavutil/mathematics.h>
//#include <libavutil/samplefmt.h>
}

#include "tproperty.h"
#include "tlevel_io.h"
#include "tthreadmessage.h"

//===========================================================
//
//  TLevelWriterMp4
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
	int m_frameCount;
	double m_fps;

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
