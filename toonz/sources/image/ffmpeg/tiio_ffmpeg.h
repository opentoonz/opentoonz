#pragma once

#ifndef TTIO_FFMPEG_INCLUDED
#define TTIO_FFMPEG_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include <QVector>
#include <QStringList>


class Ffmpeg {

public:
	Ffmpeg();
	~Ffmpeg();
	void createIntermediateImage(const TImageP &image, int frameIndex);
	void runFfmpeg(QStringList preIArgs, QStringList postIArgs, bool includesInPath, bool includesOutPath, bool overWriteFiles);
	void runFfmpeg(QStringList preIArgs, QStringList postIArgs, TFilePath path);
	QString runFfprobe(QStringList args);
	void cleanUpFiles();
	void addToCleanUp(QString);
	void setFrameRate(double fps);
	void setPath(TFilePath path);
	void saveSoundTrack(TSoundTrack *st);
	static bool checkFfmpeg();
	static bool checkFfprobe();
	TRasterImageP getImage(QString path, int lx, int ly);

private:
	QString m_intermediateFormat, m_ffmpegPath, m_audioPath, m_audioFormat;
	int m_frameCount, m_lx, m_ly, m_bpp, m_bitsPerSample, m_channelCount;
	double m_frameRate;
	bool m_ffmpegExists = false, m_ffprobeExists = false, m_hasSoundTrack = false;
	TFilePath m_path;
	QVector<QString> m_cleanUpList;
	QStringList m_audioArgs;
	TUINT32 m_sampleRate;
};

#endif
