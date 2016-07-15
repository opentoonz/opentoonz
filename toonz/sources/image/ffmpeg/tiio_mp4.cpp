
#include "tsystem.h"
#include "tiio_mp4.h"
#include "tenv.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include "tsound.h"
#include <QStringList>
#include <QDir>
//#include <QProcess>



//===========================================================
//
//  TImageWriterMp4
//
//===========================================================

class TImageWriterMp4 : public TImageWriter {
public:
	int m_frameIndex;

	TImageWriterMp4(const TFilePath &path, int frameIndex, TLevelWriterMp4 *lwg)
		: TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
		m_lwg->addRef();

	}
	~TImageWriterMp4() { m_lwg->release(); }

	bool is64bitOutputSupported() override { return false; }
	void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
	TLevelWriterMp4 *m_lwg;

};


//===========================================================
//
//  TLevelWriterMp4;
//
//===========================================================

TLevelWriterMp4::TLevelWriterMp4(const TFilePath &path, TPropertyGroup *winfo)
	: TLevelWriter(path, winfo) {
	if (!m_properties) m_properties = new Tiio::Mp4WriterProperties();
	std::string scale = m_properties->getProperty("Scale")->getValueAsString();
	sscanf(scale.c_str(), "%d", &m_scale);
	std::string quality = m_properties->getProperty("Quality")->getValueAsString();
	sscanf(quality.c_str(), "%d", &m_vidQuality);
	ffmpegWriter = new Ffmpeg();
	ffmpegWriter->setPath(m_path);
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterMp4::~TLevelWriterMp4()
{
	//QProcess createMp4;
	QStringList preIArgs;
	QStringList postIArgs;

	int outLx = m_lx;
	int outLy = m_ly;

	//set scaling
	if (m_scale != 0) {
		outLx = m_lx * m_scale / 100;
		outLy = m_ly * m_scale / 100;
	}

	//calculate quality (bitrate)
	int pixelCount = m_lx * m_ly;
	int bitRate = pixelCount / 150; //crude but gets decent values
	double quality = m_vidQuality / 100.0;
	double tempRate = (double)bitRate * quality;
	int finalBitrate = (int)tempRate;
	int crf = 51 - (m_vidQuality * 51 / 100);

	preIArgs << "-framerate";
	preIArgs << QString::number(m_frameRate);

	postIArgs << "-pix_fmt";
	postIArgs << "yuv420p";
	postIArgs << "-s";
	postIArgs << QString::number(outLx) + "x" + QString::number(outLy);
	postIArgs << "-b";
	postIArgs << QString::number(finalBitrate) + "k";
	postIArgs << "-y";

	ffmpegWriter->runFfmpeg(preIArgs, postIArgs);
	ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterMp4::getFrameWriter(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildGifExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageWriterP(0);
	int index = fid.getNumber();
	TImageWriterMp4 *iwg = new TImageWriterMp4(m_path, index, this);
	return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterMp4::setFrameRate(double fps)
{
	m_frameRate = fps;
	ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterMp4::saveSoundTrack(TSoundTrack *st)
{
	ffmpegWriter->saveSoundTrack(st);
}


//-----------------------------------------------------------

void TLevelWriterMp4::save(const TImageP &img, int frameIndex) {
	TRasterImageP image(img);
	m_lx = image->getRaster()->getLx();
	m_ly = image->getRaster()->getLy();
	ffmpegWriter->createIntermediateImage(img, frameIndex);
}


////===========================================================
////
////  TImageReaderMp4
////
////===========================================================
//
//class TImageReaderMp4 final : public TImageReader {
//public:
//	int m_frameIndex;
//
//	TImageReaderMp4(const TFilePath &path, int index, TLevelReaderMp4 *lra)
//		: TImageReader(path), m_lra(lra), m_frameIndex(index) {
//		m_lra->addRef();
//	}
//	~TImageReaderMp4() { m_lra->release(); }
//
//	TImageP load() override { return m_lra->load(m_frameIndex); }
//	TDimension getSize() const { return m_lra->getSize(); }
//	TRect getBBox() const { return TRect(); }
//
//private:
//	TLevelReaderMp4 *m_lra;
//
//	// not implemented
//	TImageReaderMp4(const TImageReaderMp4 &);
//	TImageReaderMp4 &operator=(const TImageReaderMp4 &src);
//};
//
////===========================================================
////
////  TLevelReaderAvi
////
////===========================================================
//
//
//TLevelReaderMp4::TLevelReaderMp4(const TFilePath &path)
//	: TLevelReader(path)
//
//
//{
//
//	QProcess probe;
//	QStringList fpsArgs;
//	QStringList sizeArgs;
//	QStringList frameNumArgs;
//
//	QString ffmpegPath = QDir::currentPath();
//	std::string ffmpegstrpath = ffmpegPath.toStdString();
//
//
//	//get fps
//	fpsArgs << "-v";
//	fpsArgs << "error";
//	fpsArgs << "-select_streams";
//	fpsArgs << "v:0";
//	fpsArgs << "-show_entries";
//	fpsArgs << "stream=avg_frame_rate";
//	fpsArgs << "-of";
//	fpsArgs << "default=noprint_wrappers=1:nokey=1";
//	fpsArgs << m_path.getQString();
//
//
//	probe.start(ffmpegPath + "/ffprobe", fpsArgs);
//	probe.waitForFinished(-1);
//	QString fpsResults = probe.readAllStandardError();
//	fpsResults += probe.readAllStandardOutput();
//	probe.close();
//
//	int fpsNum = fpsResults.split("/")[0].toInt();
//	int fpsDen = fpsResults.split("/")[1].toInt();
//	double fps = fpsNum / fpsDen;
//
//
//	//get size
//	sizeArgs << "-v";
//	sizeArgs << "error";
//	sizeArgs << "-of";
//	sizeArgs << "flat=s=_";
//	sizeArgs << "-select_streams";
//	sizeArgs << "v:0";
//	sizeArgs << "-show_entries";
//	sizeArgs << "stream=height,width";
//	sizeArgs << m_path.getQString();
//
//	probe.start(ffmpegPath + "/ffprobe", sizeArgs);
//	probe.waitForFinished(-1);
//	QString sizeResults = probe.readAllStandardError();
//	sizeResults += probe.readAllStandardOutput();
//	probe.close();
//	std::string szStr = sizeResults.toStdString();
//
//	QStringList split = sizeResults.split("\r");
//	int lx = split[0].split("=")[1].toInt();
//	int ly = split[1].split("=")[1].toInt();
//
//
//	//get total frames
//	frameNumArgs << "-v";
//	frameNumArgs << "error";
//	frameNumArgs << "-count_frames";
//	frameNumArgs << "-select_streams";
//	frameNumArgs << "v:0";
//	frameNumArgs << "-show_entries";
//	frameNumArgs << "stream=nb_read_frames";
//	frameNumArgs << "-of";
//	frameNumArgs << "default=nokey=1:noprint_wrappers=1";
//	frameNumArgs << m_path.getQString();
//
//
//	probe.start(ffmpegPath + "/ffprobe", frameNumArgs);
//	probe.waitForFinished(-1);
//	QString frameResults = probe.readAll();
//	//sizeResults += probe.readAllStandardOutput();
//	probe.close();
//	std::string framesStr = frameResults.toStdString();
//
//	m_numFrames = frameResults.toInt();
//	m_size = TDimension(lx, ly);
//
//	//convert frames
//	TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
//	QString tempName = "In%03d.rgb";
//	tempName = tempPath.getQString() + tempName;
//	QString tempStart = "In001.rgb";
//	tempStart = tempPath.getQString() + tempStart;
//	if (!TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
//		//for debugging	
//		//std::string strPath = tempName.toStdString();
//
//		//QString ffmpegPath = QDir::currentPath();
//		//std::string ffmpegstrpath = ffmpegPath.toStdString();
//		QProcess ffmpeg;
//		QStringList frameArgs;
//		//frameArgs << "-accurate_seek";
//		//frameArgs << "-ss";
//		//frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
//		frameArgs << "-i";
//		frameArgs << m_path.getQString();
//		frameArgs << "-y";
//		frameArgs << "-f";
//		frameArgs << "image2";
//		frameArgs << "-vcodec";
//		frameArgs << "rawvideo";
//		frameArgs << "-pix_fmt";
//		frameArgs << "rgb32";
//		frameArgs << tempName;
//
//		ffmpeg.start(ffmpegPath + "/ffmpeg", frameArgs);
//		ffmpeg.waitForFinished(15000);
//		QString frameConvertResults = ffmpeg.readAllStandardError();
//		//sizeResults += probe.readAllStandardOutput();
//		ffmpeg.close();
//		std::string framesConvertStr = frameResults.toStdString();
//	}
//
//	//set values
//	m_info = new TImageInfo();
//	m_info->m_frameRate = fps;
//	m_info->m_lx = lx;
//	m_info->m_ly = ly;
//	m_info->m_bitsPerSample = 8;
//	m_info->m_samplePerPixel = 4;
//
//
//
//
//}
////-----------------------------------------------------------
//
//TLevelReaderMp4::~TLevelReaderMp4() {}
//
////-----------------------------------------------------------
//
//TLevelP TLevelReaderMp4::loadInfo() {
//
//	if (m_numFrames == -1) return TLevelP();
//	TLevelP level;
//	for (int i = 1; i <= m_numFrames; i++) level->setFrame(i, TImageP());
//	return level;
//}
//
////-----------------------------------------------------------
//
//TImageReaderP TLevelReaderMp4::getFrameReader(TFrameId fid) {
//	//if (IOError != 0)
//	//	throw TImageException(m_path, buildAVIExceptionString(IOError));
//	if (fid.getLetter() != 0) return TImageReaderP(0);
//	int index = fid.getNumber();
//
//	TImageReaderMp4 *irm = new TImageReaderMp4(m_path, index, this);
//	return TImageReaderP(irm);
//}
//
////------------------------------------------------------------------------------
//
//TDimension TLevelReaderMp4::getSize() {
//	return m_size;
//}
//
////------------------------------------------------
//
//TImageP TLevelReaderMp4::load(int frameIndex) {
//
//	TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
//	QString number = QString("%1").arg(frameIndex, 3, 10, QChar('0'));
//	QString tempName = "In" + number + ".rgb";
//	tempName = tempPath.getQString() + tempName;
//
//	//for debugging	
//	std::string strPath = tempName.toStdString();
//
//	//This loads one image from the file, but it is slow.
//	//QString ffmpegPath = QDir::currentPath();
//	//std::string ffmpegstrpath = ffmpegPath.toStdString();
//	//QProcess ffmpeg;
//	//QStringList frameArgs;
//	//frameArgs << "-accurate_seek";
//	//frameArgs << "-ss";
//	//frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
//	//frameArgs << "-i";
//	//frameArgs << m_path.getQString();
//	//frameArgs << "-y";
//	//frameArgs << "-frames:v";
//	//frameArgs << "1";
//	//frameArgs << "-vcodec";
//	//frameArgs << "rawvideo";
//	//frameArgs << "-pix_fmt";
//	//frameArgs << "rgb32";
//	//frameArgs << tempName;
//
//	//ffmpeg.start(ffmpegPath + "/ffmpeg", frameArgs);
//	//ffmpeg.waitForFinished(5000);
//	//QString frameResults = ffmpeg.readAllStandardError();
//	////sizeResults += probe.readAllStandardOutput();
//	//ffmpeg.close();
//	//std::string framesStr = frameResults.toStdString();
//	//time for i in{ 0..39 }; do ffmpeg - accurate_seek - ss `echo $i*60.0 | bc` - i input.mp4 - frames:v 1 period_down_$i.bmp; done;
//
//	QFile file(tempName);
//	file.open(QIODevice::ReadOnly);
//	QByteArray blob = file.readAll();
//	file.close();
//
//	TRasterPT<TPixelRGBM32> ret;
//	ret.create(m_info->m_lx, m_info->m_ly);
//	ret->lock();
//	memcpy(ret->getRawData(), blob.data(), m_info->m_lx * m_info->m_ly * 4);
//	ret->unlock();
//	ret->yMirror();
//	return TRasterImageP(ret);
//
//	//return TRasterImageP();
//}





Tiio::Mp4WriterProperties::Mp4WriterProperties()
	: m_vidQuality("Quality", 1, 100, 65), m_scale("Scale", 1, 100, 100) {
	bind(m_vidQuality);
	bind(m_scale);

}

//Tiio::Reader* Tiio::makeMp4Reader(){ return nullptr; }
//Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; }