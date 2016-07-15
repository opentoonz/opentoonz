
#include "tsystem.h"
//#include "texception.h"
#include "tfilepath.h"
#include "tproperty.h"
#include "tiio_gif.h"
#include "tenv.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include <QProcess>
#include <QDir>
#include <QStringList>


//===========================================================
//
//  TImageWriterGif
//
//===========================================================

class TImageWriterGif : public TImageWriter {
public:
	int m_frameIndex;

	TImageWriterGif(const TFilePath &path, int frameIndex, TLevelWriterGif *lwg)
		: TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
		m_lwg->addRef();
		
	}
	~TImageWriterGif() { m_lwg->release(); }

	bool is64bitOutputSupported() override { return false; }
	void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
	TLevelWriterGif *m_lwg;

};


//===========================================================
//
//  TLevelWriterGif;
//
//===========================================================

TLevelWriterGif::TLevelWriterGif(const TFilePath &path, TPropertyGroup *winfo)
	: TLevelWriter(path, winfo) {
	if (!m_properties) m_properties = new Tiio::GifWriterProperties();
	std::string scale = m_properties->getProperty("Scale")->getValueAsString();
	sscanf(scale.c_str(), "%d", &m_scale);
	TBoolProperty* looping = (TBoolProperty *)m_properties->getProperty("Looping");
	m_looping = looping->getValue();
	TBoolProperty* palette = (TBoolProperty *)m_properties->getProperty("Generate Palette");
	m_palette = palette->getValue();
	m_frameCount = 0;
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterGif::~TLevelWriterGif()
{
	QProcess createGif;
	QStringList args;
	QStringList paletteArgs;

	//get directory for ffmpeg (ffmpeg binaries also need to be in the folder)
	QString ffmpegPath = QDir::currentPath();
	
	//for debugging directory
	std::string ffmpegstrpath = ffmpegPath.toStdString();

	
	QString tempName = "tempOut%d.jpg";
	tempName = m_path.getQString() + tempName;
	//for debugging	
	std::string strPath = tempName.toStdString();
	
	int outLx = m_lx;
	int outLy = m_ly;

	//set scaling
	outLx = m_lx * m_scale / 100;
	outLy = m_ly * m_scale / 100;
	
	
	QString palette;
	QString filters = "fps=" + QString::number(m_frameRate) + ",scale=" + QString::number(outLx) + ":-1:flags=lanczos";
	if (m_palette)
	{
		palette = m_path.getQString() + "palette.png";
		paletteArgs << "-v";
		paletteArgs << "warning";
		paletteArgs << "-i";
		paletteArgs << tempName;
		paletteArgs << "-vf";
		paletteArgs << filters + ",palettegen";
		paletteArgs << "-y";
		paletteArgs << palette;


		//write the palette
	
		createGif.start(ffmpegPath + "/ffmpeg", paletteArgs);
		createGif.waitForFinished(-1);
		QString paletteResults = createGif.readAllStandardError();
		paletteResults += createGif.readAllStandardOutput();
		createGif.close();
		std::string strPaletteResults = paletteResults.toStdString();
	}

	//ffmpeg - v warning - i $1 - i $palette - lavfi "$filters [x]; [x][1:v] paletteuse" - y $2
	args << "-v";
	args << "warning";
	args << "-i";
	args << tempName;
	if (m_palette) {
		args << "-i";
		args << palette;
		args << "-lavfi";
		args << filters + " [x]; [x][1:v] paletteuse";
	}
	if (!m_looping)
	{
		args << "-loop";
		args << "-1";
	}
	args << "-y";
	args << m_path.getQString();

	std::string outPath = m_path.getQString().toStdString();

	

	//write the file
	createGif.start(ffmpegPath + "/ffmpeg", args);
	createGif.waitForFinished(-1);
	QString results = createGif.readAllStandardError();
	results += createGif.readAllStandardOutput();
	createGif.close();
	std::string strResults = results.toStdString();

	QString deletePath = m_path.getQString() + "tempOut";
	QString deleteFile;
	bool startedDelete = false;
	for (int i = 0; ; i++)
	{
		deleteFile = deletePath + QString::number(i) + ".jpg";
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
	if (m_palette && TSystem::doesExistFileOrLevel(TFilePath(palette)))
		TSystem::deleteFile(TFilePath(palette));
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterGif::getFrameWriter(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildGifExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageWriterP(0);
	int index = fid.getNumber();
	TImageWriterGif *iwg = new TImageWriterGif(m_path, index, this);
	return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterGif::setFrameRate(double fps)
{
	m_fps = fps;
	m_frameRate = fps;
}

void TLevelWriterGif::saveSoundTrack(TSoundTrack *st)
{
	return;
}


//-----------------------------------------------------------

void TLevelWriterGif::save(const TImageP &img, int frameIndex) {
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
	
	//TFilePath tempPath(TEnv::getStuffDir() + "projects/temp/");
	QString tempName = m_path.getQString() + "tempOut" + QString::number(frameIndex) + ".ppm";
	QString tempJpg = m_path.getQString() + "tempOut" + QString::number(frameIndex) + ".jpg";

	
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

	QStringList args;

	args << "-s";
	args << QString::number(m_lx) + "x" + QString::number(m_ly);
	args << "-pix_fmt";
	args << "rgb32";
	args << "-vcodec";
	args << "rawvideo";
	args << "-i";
	args << tempName;
	args << "-q";
	args << "1";
	args << "-y";
	args << tempJpg;


	//get directory for ffmpeg (ffmpeg binaries also need to be in the folder)
	QString ffmpegPath = QDir::currentPath();
	QProcess jpegConvert;
	//write the file
	jpegConvert.start(ffmpegPath + "/ffmpeg", args);
	jpegConvert.waitForFinished(-1);
	QString results = jpegConvert.readAllStandardError();
	results += jpegConvert.readAllStandardOutput();
	jpegConvert.close();
	std::string strResults = results.toStdString();

	TFilePath deleteCurrent(tempName);
	if (TSystem::doesExistFileOrLevel(deleteCurrent)) 
		TSystem::deleteFile(deleteCurrent);

	
}


////===========================================================
////
////  TImageReaderGif
////
////===========================================================
//
//class TImageReaderGif final : public TImageReader {
//public:
//	int m_frameIndex;
//
//	TImageReaderGif(const TFilePath &path, int index, TLevelReaderGif *lra)
//		: TImageReader(path), m_lra(lra), m_frameIndex(index) {
//		m_lra->addRef();
//	}
//	~TImageReaderGif() { m_lra->release(); }
//
//	TImageP load() override { return m_lra->load(m_frameIndex); }
//	TDimension getSize() const { return m_lra->getSize(); }
//	TRect getBBox() const { return TRect(); }
//
//private:
//	TLevelReaderGif *m_lra;
//
//	// not implemented
//	TImageReaderGif(const TImageReaderGif &);
//	TImageReaderGif &operator=(const TImageReaderGif &src);
//};
//
////===========================================================
////
////  TLevelReaderAvi
////
////===========================================================
//
//
//TLevelReaderGif::TLevelReaderGif(const TFilePath &path)
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
//TLevelReaderGif::~TLevelReaderGif() {}
//
////-----------------------------------------------------------
//
//TLevelP TLevelReaderGif::loadInfo() {
//	
//	if (m_numFrames == -1) return TLevelP();
//	TLevelP level;
//	for (int i = 1; i <= m_numFrames; i++) level->setFrame(i, TImageP());
//	return level;
//}
//
////-----------------------------------------------------------
//
//TImageReaderP TLevelReaderGif::getFrameReader(TFrameId fid) {
//	//if (IOError != 0)
//	//	throw TImageException(m_path, buildAVIExceptionString(IOError));
//	if (fid.getLetter() != 0) return TImageReaderP(0);
//	int index = fid.getNumber();
//
//	TImageReaderGif *irm = new TImageReaderGif(m_path, index, this);
//	return TImageReaderP(irm);
//}
//
////------------------------------------------------------------------------------
//
//TDimension TLevelReaderGif::getSize() {
//	return m_size;
//}
//
////------------------------------------------------
//
//TImageP TLevelReaderGif::load(int frameIndex) {
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
//	//time for i in{ 0..39 }; do ffmpeg - accurate_seek - ss `echo $i*60.0 | bc` - i input.gif - frames:v 1 period_down_$i.bmp; done;
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
//
//
//


Tiio::GifWriterProperties::GifWriterProperties()
	: m_scale("Scale", 1, 100, 100), m_looping("Looping", false), m_palette("Generate Palette", true) {
		bind(m_scale);
		bind(m_looping);
		bind(m_palette);
	
}

//Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
//Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }