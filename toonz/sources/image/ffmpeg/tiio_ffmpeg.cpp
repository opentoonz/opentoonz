#include "tiio_ffmpeg.h"
#include "tsystem.h"
#include "tsound.h"
#include "trasterimage.h"
#include <QProcess>
#include <QDir>
#include <QtGui/QImage>
#include "toonz/preferences.h"

Ffmpeg::Ffmpeg() {
	if (checkFfmpeg()) {
		m_ffmpegExists = true;
	}
	if (checkFfprobe())
		m_ffprobeExists = true;
	m_ffmpegPath = Preferences::instance()->getFfmpegPath();
	std::string strPath = m_ffmpegPath.toStdString();
	m_intermediateFormat = "png";
}
Ffmpeg::~Ffmpeg() {}

bool Ffmpeg::checkFfmpeg() {
	//check the user defined path in preferences first
	QString path = Preferences::instance()->getFfmpegPath() + "//ffmpeg";
#if defined(_WIN32)
	path = path + ".exe";
#endif
	if (TSystem::doesExistFileOrLevel(TFilePath(path)))
		return true;

	//check the OpenToonz root directory next
	path = QDir::currentPath() + "/ffmpeg";
#if defined(_WIN32)
	path = path + ".exe";
#endif
	if (TSystem::doesExistFileOrLevel(TFilePath(path))){
		Preferences::instance()->setFfmpegPath(QDir::currentPath().toStdString());
		return true;
	}

	//give up
	return false;
}

bool Ffmpeg::checkFfprobe() {
	//check the user defined path in preferences first
	QString path = Preferences::instance()->getFfmpegPath() + "//ffprobe";
#if defined(_WIN32)
	path = path + ".exe";
#endif
	if (TSystem::doesExistFileOrLevel(TFilePath(path)))
		return true;

	//check the OpenToonz root directory next
	path = QDir::currentPath() + "/ffprobe";
#if defined(_WIN32)
	path = path + ".exe";
#endif
	if (TSystem::doesExistFileOrLevel(TFilePath(path))){
		Preferences::instance()->setFfmpegPath(QDir::currentPath().toStdString());
		return true;
	}

	//give up
	return false;
}

void Ffmpeg::setFrameRate(double fps) {
	m_frameRate = fps;
}

void Ffmpeg::setPath(TFilePath path) {
	m_path = path;
}

void Ffmpeg::createIntermediateImage(const TImageP &img, int frameIndex) {
	QString tempPath = m_path.getQString() + "tempOut" + QString::number(frameIndex) + "." + m_intermediateFormat;
	std::string saveStatus = "";
	TRasterImageP image(img);
	m_lx = image->getRaster()->getLx();
	m_ly = image->getRaster()->getLy();
	m_bpp = image->getRaster()->getPixelSize();
	int totalBytes = m_lx * m_ly * m_bpp;
	image->getRaster()->yMirror();

	//lock raster to get data
	image->getRaster()->lock();
	void *buffin = image->getRaster()->getRawData();
	assert(buffin);
	void *buffer = malloc(totalBytes);
	memcpy(buffer, buffin, totalBytes);

	image->getRaster()->unlock();

	//create QImage save format
	QByteArray ba = m_intermediateFormat.toUpper().toLatin1();
	const char *format = ba.data();

	QImage* qi = new QImage((uint8_t*)buffer, m_lx, m_ly, QImage::Format_ARGB32);
	qi->save(tempPath, format, -1);
	free(buffer);
	delete qi;
	m_cleanUpList.push_back(tempPath);
	m_frameCount++;
}

void Ffmpeg::runFfmpeg(QStringList preIArgs, QStringList postIArgs, bool includesInPath, bool includesOutPath, bool overWriteFiles) {
	QString tempName = "tempOut%d." + m_intermediateFormat;
	tempName = m_path.getQString() + tempName;

	QStringList args;
	args = args + preIArgs;
	if (!includesInPath) {  //NOTE:  if including the in path, it needs to be in the preIArgs argument.
		args << "-i";
		args << tempName;
	}
	if (m_hasSoundTrack)
		args = args + m_audioArgs;
	args = args + postIArgs;
	if (overWriteFiles && !includesOutPath) { //if includesOutPath is true, you need to include the overwrite in your postIArgs.
		args << "-y";
	}
	if (!includesOutPath) {
		args << m_path.getQString();
	}

	//write the file
	QProcess ffmpeg;
	ffmpeg.start(m_ffmpegPath + "/ffmpeg", args);
	ffmpeg.waitForFinished(-1);
	QString results = ffmpeg.readAllStandardError();
	results += ffmpeg.readAllStandardOutput();
	ffmpeg.close();
	std::string strResults = results.toStdString();
}

void Ffmpeg::runFfprobe(QStringList args) {
	QProcess ffmpeg;
	ffmpeg.start(m_ffmpegPath + "/ffmpeg", args);
	ffmpeg.waitForFinished(-1);
	QString results = ffmpeg.readAllStandardError();
	results += ffmpeg.readAllStandardOutput();
	ffmpeg.close();
	std::string strResults = results.toStdString();
}

void Ffmpeg::saveSoundTrack(TSoundTrack *st)
{
	m_sampleRate = st->getSampleRate();
	m_channelCount = st->getChannelCount();
	m_bitsPerSample = st->getBitPerSample();
	//LONG count = st->getSampleCount();
	int bufSize = st->getSampleCount() * st->getSampleSize();
	const UCHAR *buffer = st->getRawData();

	m_audioPath = m_path.getQString() + "tempOut.raw";
	m_audioFormat = "s" + QString::number(m_bitsPerSample);
	if (m_bitsPerSample > 8)
		m_audioFormat = m_audioFormat + "le";
	std::string strPath = m_audioPath.toStdString();

	QByteArray data;
	data.insert(0, (char *)buffer, bufSize);

	QFile file(m_audioPath);
	file.open(QIODevice::WriteOnly);
	file.write(data);
	file.close();
	m_hasSoundTrack = true;

	m_audioArgs << "-f";
	m_audioArgs << m_audioFormat;
	m_audioArgs << "-ar";
	m_audioArgs << QString::number(m_sampleRate);
	m_audioArgs << "-ac";
	m_audioArgs << QString::number(m_channelCount);
	m_audioArgs << "-i";
	m_audioArgs << m_audioPath;

	//add file to framesWritten for cleanup
	m_cleanUpList.push_back(m_audioPath);
}

void Ffmpeg::addToCleanUp(QString path) {
	if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
		m_cleanUpList.push_back(path);
	}
}

void Ffmpeg::cleanUpFiles() {
	for (QString path : m_cleanUpList) {
		if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
			TSystem::deleteFile(TFilePath(path));
		}
	}
}