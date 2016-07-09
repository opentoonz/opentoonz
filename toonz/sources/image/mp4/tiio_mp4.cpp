
#include "tsystem.h"
//#include "texception.h"
#include "tfilepath.h"
#include "tiio_mp4.h"
#include "tenv.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include "qprocess.h"
#include "qstringlist.h"
#include "qdir.h"


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
	m_frameCount = 0;
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterMp4::~TLevelWriterMp4()
{
	QProcess createMp4;
	QStringList args;

	TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
	QString tempName = "frame%d.ppm";
	tempName = tempPath.getQString() + tempName;
	//for debugging	
	std::string strPath = tempName.toStdString();

	args << "-framerate";
	args << QString::number(m_frameRate);
	args << "-s";
	//args << "1920x1080";
	args << QString::number(m_lx) + "x" + QString::number(m_ly);
	args << "-pix_fmt";
	args << "rgb32";
	args << "-vcodec";
	args << "rawvideo";
	args << "-i";
	args << tempName;
	args << "-c:v";
	args << "libopenh264";
	args << m_path.getQString();

	std::string outPath = m_path.getQString().toStdString();

	//get directory for ffmpeg (ffmpeg binaries also need to be in the folder)
	QString ffmpegPath = QDir::currentPath();
	//for debugging directory
	
	std::string ffmpegstrpath = ffmpegPath.toStdString();

	//write the file
	createMp4.start(ffmpegPath + "/ffmpeg", args);
	createMp4.waitForFinished(-1);
	createMp4.close();

	QString deletePath = tempPath.getQString() + "frame";
	QString deleteFile;
	bool startedDelete = false;
	for (int i = 0; ; i++)
	{
		deleteFile = deletePath + QString::number(i) + ".ppm";
		TFilePath deleteCurrent(deleteFile);
		if (TSystem::doesExistFileOrLevel(deleteCurrent)) {
			TSystem::deleteFile(deleteCurrent);
			startedDelete = true;
		}
		else {
			if (startedDelete == true)
				break;
		}
	}
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
	m_fps = fps;
	m_frameRate = fps;
}

void TLevelWriterMp4::saveSoundTrack(TSoundTrack *st)
{
	return;
}


//-----------------------------------------------------------

void TLevelWriterMp4::save(const TImageP &img, int frameIndex) {
	std::string saveStatus = "";
	TRasterImageP image(img);
	m_lx = image->getRaster()->getLx();
	m_ly = image->getRaster()->getLy();
	int linesize = image->getRaster()->getRowSize();
	int pixelSize = image->getRaster()->getPixelSize();
	image->getRaster()->yMirror();
	//lock raster to get data
	image->getRaster()->lock();

	uint8_t *buffin = image->getRaster()->getRawData();
	assert(buffin);
	
	TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
	QString tempName = "frame" + QString::number(frameIndex) + ".ppm";
	tempName = tempPath.getQString() + tempName;

	
	QByteArray ba = tempName.toLatin1();
	const char *charPath = ba.data();
	
	std::string strPath = tempName.toStdString();

	
	
	FILE* pFile = fopen(charPath, "wb");
	if (!pFile)
		return;

	// Write pixel data
	for (int y = 0; y<m_ly; y++)
		fwrite(buffin + y*linesize, 1, m_lx * pixelSize, pFile);

	m_frameCount++; 
	// Close file
	fclose(pFile);
	image->getRaster()->unlock();

	
}


//===========================================================
//
//  TImageReaderMp4
//
//===========================================================

class TImageReaderMp4 final : public TImageReader {
public:
	int m_frameIndex;

	TImageReaderMp4(const TFilePath &path, int index, TLevelReaderMp4 *lra)
		: TImageReader(path), m_lra(lra), m_frameIndex(index) {
		m_lra->addRef();
	}
	~TImageReaderMp4() { m_lra->release(); }

	TImageP load() override { return m_lra->load(m_frameIndex); }
	TDimension getSize() const { return m_lra->getSize(); }
	TRect getBBox() const { return TRect(); }

private:
	TLevelReaderMp4 *m_lra;

	// not implemented
	TImageReaderMp4(const TImageReaderMp4 &);
	TImageReaderMp4 &operator=(const TImageReaderMp4 &src);
};

//===========================================================
//
//  TLevelReaderAvi
//
//===========================================================


TLevelReaderMp4::TLevelReaderMp4(const TFilePath &path)
	: TLevelReader(path)


{

	QProcess probe;
	QStringList fpsArgs;
	QStringList sizeArgs;
	QStringList frameNumArgs;

	QString ffmpegPath = QDir::currentPath();
	std::string ffmpegstrpath = ffmpegPath.toStdString();
	

	//get fps
	fpsArgs << "-v";
	fpsArgs << "error";
	fpsArgs << "-select_streams";
	fpsArgs << "v:0";
	fpsArgs << "-show_entries";
	fpsArgs << "stream=avg_frame_rate";
	fpsArgs << "-of";
	fpsArgs << "default=noprint_wrappers=1:nokey=1";
	fpsArgs << m_path.getQString();

	
	probe.start(ffmpegPath + "/ffprobe", fpsArgs);
	probe.waitForFinished(-1);
	QString fpsResults = probe.readAllStandardError();
	fpsResults += probe.readAllStandardOutput();
	probe.close();

	int fpsNum = fpsResults.split("/")[0].toInt();
	int fpsDen = fpsResults.split("/")[1].toInt();
	double fps = fpsNum / fpsDen;


	//get size
	sizeArgs << "-v";
	sizeArgs << "error";
	sizeArgs << "-of";
	sizeArgs << "flat=s=_";
	sizeArgs << "-select_streams";
	sizeArgs << "v:0";
	sizeArgs << "-show_entries";
	sizeArgs << "stream=height,width";
	sizeArgs << m_path.getQString();

	probe.start(ffmpegPath + "/ffprobe", sizeArgs);
	probe.waitForFinished(-1);
	QString sizeResults = probe.readAllStandardError();
	sizeResults += probe.readAllStandardOutput();
	probe.close();
	std::string szStr = sizeResults.toStdString();

	QStringList split = sizeResults.split("\r");
	int lx = split[0].split("=")[1].toInt();
	int ly = split[1].split("=")[1].toInt();
	
	
	//get total frames
	frameNumArgs << "-v";
	frameNumArgs << "error";
	frameNumArgs << "-count_frames";
	frameNumArgs << "-select_streams";
	frameNumArgs << "v:0";
	frameNumArgs << "-show_entries";
	frameNumArgs << "stream=nb_read_frames";
	frameNumArgs << "-of";
	frameNumArgs << "default=nokey=1:noprint_wrappers=1";
	frameNumArgs << m_path.getQString();
	

	probe.start(ffmpegPath + "/ffprobe", frameNumArgs);
	probe.waitForFinished(-1);
	QString frameResults = probe.readAll();
	//sizeResults += probe.readAllStandardOutput();
	probe.close();
	std::string framesStr = frameResults.toStdString();
	
	m_numFrames = frameResults.toInt();
	m_size = TDimension(lx, ly);

	//convert frames
	TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
	QString tempName = "frame%03d.rgb";
	tempName = tempPath.getQString() + tempName;
	QString tempStart = "frame001.rgb";
	tempStart = tempPath.getQString() + tempStart;
	if (!TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
		//for debugging	
		//std::string strPath = tempName.toStdString();

		//QString ffmpegPath = QDir::currentPath();
		//std::string ffmpegstrpath = ffmpegPath.toStdString();
		QProcess ffmpeg;
		QStringList frameArgs;
		//frameArgs << "-accurate_seek";
		//frameArgs << "-ss";
		//frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
		frameArgs << "-i";
		frameArgs << m_path.getQString();
		frameArgs << "-y";
		frameArgs << "-f";
		frameArgs << "image2";
		frameArgs << "-vcodec";
		frameArgs << "rawvideo";
		frameArgs << "-pix_fmt";
		frameArgs << "rgb32";
		frameArgs << tempName;

		ffmpeg.start(ffmpegPath + "/ffmpeg", frameArgs);
		ffmpeg.waitForFinished(15000);
		QString frameConvertResults = ffmpeg.readAllStandardError();
		//sizeResults += probe.readAllStandardOutput();
		ffmpeg.close();
		std::string framesConvertStr = frameResults.toStdString();
	}

	//set values
	m_info = new TImageInfo();
	m_info->m_frameRate = fps;
	m_info->m_lx = lx;
	m_info->m_ly = ly;
	m_info->m_bitsPerSample = 8;
	m_info->m_samplePerPixel = 4;

	


}
//-----------------------------------------------------------

TLevelReaderMp4::~TLevelReaderMp4() {}

//-----------------------------------------------------------

TLevelP TLevelReaderMp4::loadInfo() {
	
	if (m_numFrames == -1) return TLevelP();
	TLevelP level;
	for (int i = 1; i <= m_numFrames; i++) level->setFrame(i, TImageP());
	return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderMp4::getFrameReader(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildAVIExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageReaderP(0);
	int index = fid.getNumber();

	TImageReaderMp4 *irm = new TImageReaderMp4(m_path, index, this);
	return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderMp4::getSize() {
	return m_size;
}

//------------------------------------------------

TImageP TLevelReaderMp4::load(int frameIndex) {
	
	TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
	QString number = QString("%1").arg(frameIndex, 3, 10, QChar('0'));
	QString tempName = "frame" + number + ".rgb";
	tempName = tempPath.getQString() + tempName;
	
	//for debugging	
	std::string strPath = tempName.toStdString();
	//
	//QString ffmpegPath = QDir::currentPath();
	//std::string ffmpegstrpath = ffmpegPath.toStdString();
	//QProcess ffmpeg;
	//QStringList frameArgs;
	//frameArgs << "-accurate_seek";
	//frameArgs << "-ss";
	//frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
	//frameArgs << "-i";
	//frameArgs << m_path.getQString();
	//frameArgs << "-y";
	//frameArgs << "-frames:v";
	//frameArgs << "1";
	//frameArgs << "-vcodec";
	//frameArgs << "rawvideo";
	//frameArgs << "-pix_fmt";
	//frameArgs << "rgb32";
	//frameArgs << tempName;

	//ffmpeg.start(ffmpegPath + "/ffmpeg", frameArgs);
	//ffmpeg.waitForFinished(5000);
	//QString frameResults = ffmpeg.readAllStandardError();
	////sizeResults += probe.readAllStandardOutput();
	//ffmpeg.close();
	//std::string framesStr = frameResults.toStdString();
	//time for i in{ 0..39 }; do ffmpeg - accurate_seek - ss `echo $i*60.0 | bc` - i input.mp4 - frames:v 1 period_down_$i.bmp; done;
	
	QFile file(tempName);
	file.open(QIODevice::ReadOnly);
	QByteArray blob = file.readAll();
	file.close();

	TRasterPT<TPixelRGBM32> ret;
	ret.create(m_info->m_lx, m_info->m_ly);
	ret->lock();
	memcpy(ret->getRawData(), blob.data(), m_info->m_lx * m_info->m_ly * 4);
	ret->unlock();
	ret->yMirror();
	return TRasterImageP(ret);
	
	//return TRasterImageP();
}





Tiio::Mp4WriterProperties::Mp4WriterProperties(){}

Tiio::Reader* Tiio::makeMp4Reader(){ return nullptr; }
Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; }