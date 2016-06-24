#pragma once
#ifndef TOONZ_FFMPEG
#define TOONZ_FFMPEG
#include <QObject>

extern "C" {
#include "libavcodec\avcodec.h"
#include "libswscale\swscale.h"
#include "libavformat\avformat.h"
}

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
