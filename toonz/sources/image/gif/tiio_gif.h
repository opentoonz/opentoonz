#pragma once

#ifndef TTIO_GIF_INCLUDED
#define TTIO_GIF_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "tthreadmessage.h"

class FfmpegBridge;
/*
#pragma once
#ifndef TOONZ_FFMPEG
#define TOONZ_FFMPEG


class TnzFfmpeg : public QObject
{

Q_OBJECT

public:
TnzFfmpeg();
~TnzFfmpeg();
int TestFfmpeg();
//bool SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);
void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);
};
#endif

*/

//===========================================================
//
//  TLevelReaderGif
//
//===========================================================

//class TLevelReaderGif : public TLevelReader {
//public:
//	TLevelReaderGif(const TFilePath &path);
//	~TLevelReaderGif();
//	TLevelP loadInfo() override;
//	TImageReaderP getFrameReader(TFrameId fid) override;
//
//	const TImageInfo *getImageInfo(TFrameId fid) { return m_info; }
//	const TImageInfo *getImageInfo() { return m_info; }
//
//	void enableRandomAccessRead(bool enable);
//	void load(const TRasterP &rasP, int frameIndex, const TPoint &pos,
//		int shrinkX = 1, int shrinkY = 1);
//	//TDimension getSize() const { return TDimension(m_lx, m_ly); }
//	//TRect getBBox() const { return TRect(0, 0, m_lx - 1, m_ly - 1); }
//
//	//virtual TSoundTrack *loadSoundTrack();
//	static TLevelReader *create(const TFilePath &f) {
//		return new TLevelReaderGif(f);
//	}
//
//
//};

//===========================================================
//
//  TLevelWriterGif
//
//===========================================================

class TLevelWriterGif : public TLevelWriter {

public:
	TLevelWriterGif(const TFilePath &path, TPropertyGroup *winfo);
	~TLevelWriterGif();
	FfmpegBridge* ffmpeg;
	void setFrameRate(double fps);
	
	TImageWriterP getFrameWriter(TFrameId fid) override;
	void save(const TImageP &image, int frameIndex);

	void saveSoundTrack(TSoundTrack *st);

	static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
		return new TLevelWriterGif(path, winfo);
	}

private:
	int IOError;
	int m_bpp;
	void *m_buffer;
	TThread::Mutex m_mutex;
};

//===========================================================================

namespace Tiio {

//===========================================================================

class GifWriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  //TBoolProperty m_matte;

  GifWriterProperties();
};

//===========================================================================

Tiio::Reader *makeGifReader();
Tiio::Writer *makeGifWriter();

}  // namespace

#endif
