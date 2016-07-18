#include "tiio_ffmpeg.h"
#include "tsystem.h"
#include "tsound.h"

#include <QProcess>
#include <QDir>
#include <QtGui/QImage>
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"

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
	QString path = Preferences::instance()->getFfmpegPath() + "/ffmpeg";
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
	QString path = Preferences::instance()->getFfmpegPath() + "/ffprobe";
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

TFilePath Ffmpeg::getFfmpegCache() {
	QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
	if (!TSystem::doesExistFileOrLevel(TFilePath(cacheRoot + "/ffmpeg"))) {
		TSystem::mkDir(TFilePath(cacheRoot + "/ffmpeg"));
	}
	std::string ffmpegPath = TFilePath(cacheRoot + "/ffmpeg").getQString().toStdString();
	return TFilePath(cacheRoot + "/ffmpeg");
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
	ffmpeg.waitForFinished();
	QString results = ffmpeg.readAllStandardError();
	results += ffmpeg.readAllStandardOutput();
	ffmpeg.close();
	std::string strResults = results.toStdString();
}

QString Ffmpeg::runFfprobe(QStringList args) {
	QProcess ffmpeg;
	ffmpeg.start(m_ffmpegPath + "/ffprobe", args);
	ffmpeg.waitForFinished();
	QString results = ffmpeg.readAllStandardError();
	results += ffmpeg.readAllStandardOutput();
	ffmpeg.close();
	std::string strResults = results.toStdString();
	return results;
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

TRasterImageP Ffmpeg::getImage(int frameIndex) {
	QString ffmpegCachePath = getFfmpegCache().getQString();
	QString tempPath = ffmpegCachePath + "//" + QString::fromStdString(m_path.getName()) + QString::fromStdString(m_path.getType());
	std::string tmpPath = tempPath.toStdString();
	//QString tempPath= m_path.getQString();
	QString number = QString("%1").arg(frameIndex, 4, 10, QChar('0'));
	QString tempName = "In" + number + ".png";
	tempName = tempPath + tempName;

	//for debugging	
	std::string strPath = tempName.toStdString();
	if (TSystem::doesExistFileOrLevel(TFilePath(tempName))) {
		QImage *temp = new QImage(tempName, "PNG");
		if (temp){
			QImage tempToo = temp->convertToFormat(QImage::Format_ARGB32);
			delete temp;
			const UCHAR *bits = tempToo.bits();

			TRasterPT<TPixelRGBM32> ret;
			ret.create(m_lx, m_ly);
			ret->lock();
			memcpy(ret->getRawData(), bits, m_lx * m_ly * 4);
			ret->unlock();
			ret->yMirror();
			return TRasterImageP(ret);
		}
	}
	else return TRasterImageP();

}

double Ffmpeg::getFrameRate() {
	if (m_frameCount > 0) {
		QStringList fpsArgs;
		fpsArgs << "-v";
		fpsArgs << "error";
		fpsArgs << "-select_streams";
		fpsArgs << "v:0";
		fpsArgs << "-show_entries";
		fpsArgs << "stream=avg_frame_rate";
		fpsArgs << "-of";
		fpsArgs << "default=noprint_wrappers=1:nokey=1";
		fpsArgs << m_path.getQString();
		QString fpsResults = runFfprobe(fpsArgs);

		int fpsNum = fpsResults.split("/")[0].toInt();
		int fpsDen = fpsResults.split("/")[1].toInt();
		if (fpsDen > 0) {
			m_frameRate = fpsNum / fpsDen;
		}
	}
	return m_frameRate;
}

TDimension Ffmpeg::getSize() {
	QStringList sizeArgs;
	sizeArgs << "-v";
	sizeArgs << "error";
	sizeArgs << "-of";
	sizeArgs << "flat=s=_";
	sizeArgs << "-select_streams";
	sizeArgs << "v:0";
	sizeArgs << "-show_entries";
	sizeArgs << "stream=height,width";
	sizeArgs << m_path.getQString();

	QString sizeResults = runFfprobe(sizeArgs);
	QStringList split = sizeResults.split("\r");
	m_lx = split[0].split("=")[1].toInt();
	m_ly = split[1].split("=")[1].toInt();
	return TDimension(m_lx, m_ly);

}

int Ffmpeg::getFrameCount() {
	QStringList frameCountArgs;
	frameCountArgs << "-v";
	frameCountArgs << "error";
	frameCountArgs << "-count_frames";
	frameCountArgs << "-select_streams";
	frameCountArgs << "v:0";
	frameCountArgs << "-show_entries";
	frameCountArgs << "stream=nb_read_frames";
	frameCountArgs << "-of";
	frameCountArgs << "default=nokey=1:noprint_wrappers=1";
	frameCountArgs << m_path.getQString();

	QString frameResults = runFfprobe(frameCountArgs);
	m_frameCount = frameResults.toInt();
	return m_frameCount;
}

void Ffmpeg::getFramesFromMovie() {
	QString ffmpegCachePath = getFfmpegCache().getQString();
	QString tempPath = ffmpegCachePath + "//" + QString::fromStdString(m_path.getName()) + QString::fromStdString(m_path.getType());
	std::string tmpPath = tempPath.toStdString();
	QString tempName = "In%04d.png";
	tempName = tempPath + tempName;
	QString tempStart = "In0001.png";
	tempStart = tempPath + tempStart;
	QString tempBase = tempPath + "In";
	QString addToDelete;
	if (!TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
		//for debugging	
		std::string strPath = tempName.toStdString();

		QStringList preIFrameArgs;
		QStringList postIFrameArgs;
		//frameArgs << "-accurate_seek";
		//frameArgs << "-ss";
		//frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
		preIFrameArgs << "-i";
		preIFrameArgs << m_path.getQString();
		postIFrameArgs << "-y";
		postIFrameArgs << "-f";
		postIFrameArgs << "image2";

		postIFrameArgs << tempName;

		runFfmpeg(preIFrameArgs, postIFrameArgs, true, true, true);

		for (int i = 1; i <= m_frameCount; i++)
		{
			QString number = QString("%1").arg(i, 4, 10, QChar('0'));
			addToDelete = tempBase + number + ".png";
			std::string delPath = addToDelete.toStdString();
			addToCleanUp(addToDelete);
		}
	}
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