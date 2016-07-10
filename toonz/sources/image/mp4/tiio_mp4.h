#pragma once

#ifndef TTIO_MP4_INCLUDED
#define TTIO_MP4_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
//#include "tthreadmessage.h"

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
	int m_frameCount, m_lx, m_ly;
	double m_fps;
	int m_scale;
	int m_vidQuality;
};

//===========================================================
//
//  TLevelReaderMp4
//
//===========================================================

class TLevelReaderMp4 final : public TLevelReader {
public:
	TLevelReaderMp4(const TFilePath &path);
	~TLevelReaderMp4();
	TImageReaderP getFrameReader(TFrameId fid) override;

	static TLevelReader *create(const TFilePath &f) {
		return new TLevelReaderMp4(f);
	}

	
	TLevelP loadInfo() override;
	TImageP load(int frameIndex);
	TDimension getSize();
	TThread::Mutex m_mutex;
	void *m_decompressedBuffer;
private:
	TDimension m_size;
	int m_numFrames, m_lx, m_ly;
};

//===========================================================================

namespace Tiio {

//===========================================================================

class Mp4WriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  //TBoolProperty m_matte;
	TIntProperty m_vidQuality;
	TIntProperty m_scale;
    Mp4WriterProperties();
};



//===========================================================================

//Tiio::Reader *makeMp4Reader();
//Tiio::Writer *makeMp4Writer();

}  // namespace

#endif
