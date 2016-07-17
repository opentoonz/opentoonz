
#include "tsystem.h"
#include "tiio_gif.h"
#include "trasterimage.h"
#include "timageinfo.h"
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
	ffmpegWriter = new Ffmpeg();
	ffmpegWriter->setPath(m_path);
	//m_frameCount = 0;
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterGif::~TLevelWriterGif()
{
	QStringList preIArgs;
	QStringList postIArgs;
	QStringList palettePreIArgs;
	QStringList palettePostIArgs;

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
		palettePreIArgs << "-v";
		palettePreIArgs << "warning";
	
		palettePostIArgs << "-vf";
		palettePostIArgs << filters + ",palettegen";
		palettePostIArgs << palette;


		//write the palette
		ffmpegWriter->runFfmpeg(palettePreIArgs, palettePostIArgs, false, true, true);
		ffmpegWriter->addToCleanUp(palette);
	}

	preIArgs << "-v";
	preIArgs << "warning";
	
	if (m_palette) {
		postIArgs << "-i";
		postIArgs << palette;
		postIArgs << "-lavfi";
		postIArgs << filters + " [x]; [x][1:v] paletteuse";
	}
	
	if (!m_looping)	{
		postIArgs << "-loop";
		postIArgs << "-1";
	}

	std::string outPath = m_path.getQString().toStdString();

	ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
	ffmpegWriter->cleanUpFiles();
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
	//m_fps = fps;
	m_frameRate = fps;
	ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterGif::saveSoundTrack(TSoundTrack *st)
{
	return;
}


//-----------------------------------------------------------

void TLevelWriterGif::save(const TImageP &img, int frameIndex) {
	TRasterImageP image(img);
	m_lx = image->getRaster()->getLx();
	m_ly = image->getRaster()->getLy();
	ffmpegWriter->createIntermediateImage(img, frameIndex);
}


//===========================================================
//
//  TImageReaderGif
//
//===========================================================

class TImageReaderGif final : public TImageReader {
public:
	int m_frameIndex;

	TImageReaderGif(const TFilePath &path, int index, TLevelReaderGif *lra)
		: TImageReader(path), m_lra(lra), m_frameIndex(index) {
		m_lra->addRef();
	}
	~TImageReaderGif() { m_lra->release(); }

	TImageP load() override { return m_lra->load(m_frameIndex); }
	TDimension getSize() const { return m_lra->getSize(); }
	TRect getBBox() const { return TRect(); }

private:
	TLevelReaderGif *m_lra;

	// not implemented
	TImageReaderGif(const TImageReaderGif &);
	TImageReaderGif &operator=(const TImageReaderGif &src);
};

//===========================================================
//
//  TLevelReaderAvi
//
//===========================================================


TLevelReaderGif::TLevelReaderGif(const TFilePath &path)
	: TLevelReader(path)


{

	
	ffmpegReader = new Ffmpeg();
	//QProcess probe;
	QStringList fpsArgs;
	QStringList sizeArgs;
	QStringList frameNumArgs;

	//QString ffmpegPath = QDir::currentPath();
	//std::string ffmpegstrpath = ffmpegPath.toStdString();
	

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

	QString fpsResults = ffmpegReader->runFfprobe(fpsArgs);
	//probe.start(ffmpegPath + "/ffprobe", fpsArgs);
	//probe.waitForFinished(-1);
	//QString fpsResults = probe.readAllStandardError();
	//fpsResults += probe.readAllStandardOutput();
	//probe.close();

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

	//probe.start(ffmpegPath + "/ffprobe", sizeArgs);
	//probe.waitForFinished(-1);
	//QString sizeResults = probe.readAllStandardError();
	//sizeResults += probe.readAllStandardOutput();
	//probe.close();
	//std::string szStr = sizeResults.toStdString();

	QString sizeResults = ffmpegReader->runFfprobe(sizeArgs);

	QStringList split = sizeResults.split("\r");
	m_lx = split[0].split("=")[1].toInt();
	m_ly = split[1].split("=")[1].toInt();
	
	
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
	

	//probe.start(ffmpegPath + "/ffprobe", frameNumArgs);
	//probe.waitForFinished(-1);
	//QString frameResults = probe.readAll();
	//sizeResults += probe.readAllStandardOutput();
	//probe.close();
	//std::string framesStr = frameResults.toStdString();

	QString frameResults = ffmpegReader->runFfprobe(frameNumArgs);
	
	m_numFrames = frameResults.toInt();
	m_size = TDimension(m_lx, m_ly);

	////convert frames
	//TFilePath tempPath(m_path.getQString());
	//QString tempName = "In%04d.png";
	//tempName = tempPath.getQString() + tempName;
	//QString tempStart = "In0001.png";
	//tempStart = tempPath.getQString() + tempStart;
	//QString tempBase = tempPath.getQString() + "In";
	//QString addToDelete;
	//if (!TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
	//	//for debugging	
	//	//std::string strPath = tempName.toStdString();

	//	//QString ffmpegPath = QDir::currentPath();
	//	//std::string ffmpegstrpath = ffmpegPath.toStdString();
	//	//QProcess ffmpeg;
	//	QStringList preIFrameArgs;
	//	QStringList postIFrameArgs;
	//	//frameArgs << "-accurate_seek";
	//	//frameArgs << "-ss";
	//	//frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
	//	preIFrameArgs << "-i";
	//	preIFrameArgs << m_path.getQString();
	//	//frameArgs << "-y";
	//	postIFrameArgs << "-f";
	//	postIFrameArgs << "image2";
	//	//postIFrameArgs << "-vcodec";
	//	//postIFrameArgs << "rawvideo";
	//	//postIFrameArgs << "-pix_fmt";
	//	//postIFrameArgs << "rgb32";
	//	postIFrameArgs << tempName;

	//	//ffmpeg.start(ffmpegPath + "/ffmpeg", frameArgs);
	//	//ffmpeg.waitForFinished(15000);
	//	//QString frameConvertResults = ffmpeg.readAllStandardError();
	//	//sizeResults += probe.readAllStandardOutput();
	//	//ffmpeg.close();
	//	//std::string framesConvertStr = frameResults.toStdString();
	//	ffmpegReader->runFfmpeg(preIFrameArgs, postIFrameArgs, true, true, true);

	//	for (int i = 1; i <= m_numFrames; i++)
	//	{
	//		QString number = QString("%1").arg(i, 4, 10, QChar('0'));
	//		addToDelete = tempBase + number + ".png";
	//		std::string delPath = addToDelete.toStdString();
	//		ffmpegReader->addToCleanUp(addToDelete);
	//	}
	//}

	//set values
	m_info = new TImageInfo();
	m_info->m_frameRate = fps;
	m_info->m_lx = m_lx;
	m_info->m_ly = m_ly;
	m_info->m_bitsPerSample = 8;
	m_info->m_samplePerPixel = 4;

	


}
//-----------------------------------------------------------

TLevelReaderGif::~TLevelReaderGif() {
	ffmpegReader->cleanUpFiles();
}

//-----------------------------------------------------------

TLevelP TLevelReaderGif::loadInfo() {
	
	if (m_numFrames == -1) return TLevelP();
	TLevelP level;
	for (int i = 1; i <= m_numFrames; i++) level->setFrame(i, TImageP());
	return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderGif::getFrameReader(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildAVIExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageReaderP(0);
	int index = fid.getNumber();

	TImageReaderGif *irm = new TImageReaderGif(m_path, index, this);
	return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderGif::getSize() {
	return m_size;
}

//------------------------------------------------

TImageP TLevelReaderGif::load(int frameIndex) {
	
	TFilePath tempPath(m_path.getQString());
	QString number = QString("%1").arg(frameIndex, 4, 10, QChar('0'));
	QString tempName = "In" + number + ".png";
	tempName = tempPath.getQString() + tempName;
	
	//for debugging	
	std::string strPath = tempName.toStdString();
	
	//This loads one image from the file, but it is slow.
	//QString ffmpegPath = QDir::currentPath();
	//std::string ffmpegstrpath = ffmpegPath.toStdString();
	//QProcess ffmpeg;
	QStringList preIFrameArgs;
	QStringList postIFrameArgs;
	preIFrameArgs << "-accurate_seek";
	preIFrameArgs << "-ss";
	preIFrameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);
	preIFrameArgs << "-i";
	preIFrameArgs << m_path.getQString();
	postIFrameArgs << "-y";
	postIFrameArgs << "-frames:v";
	postIFrameArgs << "1";
	//postIFrameArgs << "-vcodec";
	//postIFrameArgs << "rawvideo";
	//postIFrameArgs << "-pix_fmt";
	//postIFrameArgs << "rgb32";
	postIFrameArgs << tempName;

	ffmpegReader->runFfmpeg(preIFrameArgs, postIFrameArgs, true, true, true);
	

	//ffmpeg.start(ffmpegPath + "/ffmpeg", frameArgs);
	//ffmpeg.waitForFinished(5000);
	//QString frameResults = ffmpeg.readAllStandardError();
	//sizeResults += probe.readAllStandardOutput();
	//ffmpeg.close();
	//std::string framesStr = frameResults.toStdString();
	//time for i in{ 0..39 }; do ffmpeg - accurate_seek - ss `echo $i*60.0 | bc` - i input.gif - frames:v 1 period_down_$i.bmp; done;
	ffmpegReader->addToCleanUp(tempName);
	return ffmpegReader->getImage(tempName, m_lx, m_ly);
	

	/*QFile file(tempName);
	file.open(QIODevice::ReadOnly);
	QByteArray blob = file.readAll();
	file.close();

	TRasterPT<TPixelRGBM32> ret;
	ret.create(m_info->m_lx, m_info->m_ly);
	ret->lock();
	memcpy(ret->getRawData(), blob.data(), m_info->m_lx * m_info->m_ly * 4);
	ret->unlock();
	ret->yMirror();
	return TRasterImageP(ret);*/

	//return ffmpegReader->getImage(tempName, m_lx, m_ly);
	
	//return TRasterImageP();
}





Tiio::GifWriterProperties::GifWriterProperties()
	: m_scale("Scale", 1, 100, 100), m_looping("Looping", false), m_palette("Generate Palette", true) {
		bind(m_scale);
		bind(m_looping);
		bind(m_palette);
	
}

//Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
//Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }