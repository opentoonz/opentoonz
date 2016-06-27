#pragma once

#ifndef TTIO_GIF_INCLUDED
#define TTIO_GIF_INCLUDED

extern "C" {
#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
#include "libavformat\avformat.h"
}

//#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"
#include "tlevel_io.h"


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

class TLevelReaderGif : public TLevelReader {
public:
	TLevelReaderGif(const TFilePath &path);
	~TLevelReaderGif();
	TLevelP loadInfo() override;
	TImageReaderP getFrameReader(TFrameId fid) override;
};

//===========================================================
//
//  TLevelWriterGif
//
//===========================================================

class TLevelWriterGif : public TLevelWriter {
	TLevelWriterGif(const TFilePath &path, TPropertyGroup *winfo);
	~TLevelWriterGif();

	TImageWriterP getFrameWriter(TFrameId fid) override;
	void save(const TImageP &image, int frameIndex);
	static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
		return new TLevelWriterGif(path, winfo);
	}
	
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
