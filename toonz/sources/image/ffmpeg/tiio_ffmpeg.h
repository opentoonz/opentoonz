#pragma once

#ifndef TTIO_FFMPEG_INCLUDED
#define TTIO_FFMPEG_INCLUDED

//#include "tproperty.h"
#include "tlevel_io.h"
#include <QVector>
#include <QStringList>
//#include "tthreadmessage.h"

class Ffmpeg {

public:
	Ffmpeg();
	~Ffmpeg();
	void createIntermediateImage(const TImageP &image, int frameIndex);
	void runFfmpeg(QStringList preIArgs, QStringList postIArgs);
	void runFfprobe(QStringList args);
	void cleanUpFiles();
	void setFrameRate(double fps);
	void setPath(TFilePath path);
	void saveSoundTrack(TSoundTrack *st);
	bool static checkFfmpeg();
	bool static checkFfprobe();



private:
	QString m_intermediateFormat, m_ffmpegPath, m_audioPath, m_audioFormat;
	int m_frameCount, m_lx, m_ly, m_bpp, m_bitsPerSample, m_channelCount;
	double m_frameRate;
	bool m_ffmpegExists = false, m_ffprobeExists = false, m_hasSoundTrack = false;
	TFilePath m_path;
	QVector<int> m_frameIndexWritten;
	QStringList m_audioArgs;
	TUINT32 m_sampleRate;
};

#endif
