#include <memory>

//#include "tmachine.h"
#include "tsystem.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_mp4.h"
#include "tenv.h"
#include "trasterimage.h"
#include "qprocess.h"
#include "qstringlist.h"
#include "qdir.h"
//#include "tiio.h"
//#include "../compatibility/tfile_io.h"
//#include "tpixel.h"

namespace {

	std::string buildMp4ExceptionString(int rc) {
		return "Unable to create mp4.";
	}

}


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
	avcodec_register_all();
	if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterMp4::~TLevelWriterMp4()
{
	QProcess createMp4;
	QStringList args;

	TFilePath path(TEnv::getStuffDir() + "projects/temp/");
	QString tempName = "frame%d.ppm";
	tempName = path.getQString() + tempName;
	//for debugging	
	std::string strPath = tempName.toStdString();

	args << "-framerate";
	args << QString::number(m_frameRate);
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
	
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterMp4::getFrameWriter(TFrameId fid) {
	//if (IOError != 0)
	//	throw TImageException(m_path, buildGifExceptionString(IOError));
	if (fid.getLetter() != 0) return TImageWriterP(0);
	int index = fid.getNumber() - 1;
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
	int lx = image->getRaster()->getLx();
	int ly = image->getRaster()->getLy();
	int linesize = image->getRaster()->getRowSize();
	int pixelSize = image->getRaster()->getPixelSize();

	//lock raster to get data
	image->getRaster()->lock();
	uint8_t *buffin = image->getRaster()->getRawData();
	assert(buffin);

	//memcpy(m_buffer, buffin, lx * ly * m_bpp);
	image->getRaster()->unlock();

	//end get data

	//begin convert image to output format
	AVFrame *inFrame = av_frame_alloc();

	//int inSize = lx * ly;
	//int inNumBytes = pixelSize * lx * ly;
	//uint8_t *inbuf = (uint8_t *)av_malloc(inNumBytes);
	//uint8_t *inPicture_buf = (uint8_t *)av_malloc(inNumBytes);
	//inFrame->data[0] = inbuf;

	avpicture_fill((AVPicture *)inFrame, buffin, AV_PIX_FMT_RGB32,
		lx, ly);

	int pts = frameIndex;

	inFrame->pts = pts;

	//set up scaling context
	struct SwsContext *sws_ctx = NULL;
	int frameFinished;


	sws_ctx = sws_getContext(lx,
		ly,
		AV_PIX_FMT_RGB32,
		lx,
		ly,
		//m_codecContext->pix_fmt,
		AV_PIX_FMT_RGB24,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

	


	//begin example
	
	AVFrame* outFrame = av_frame_alloc();
	if (!outFrame) {
		saveStatus = "Could not allocate video frame\n";
	}
	outFrame->format = AV_PIX_FMT_RGB24;
	outFrame->width = lx;
	outFrame->height = ly;

	int ret = av_image_alloc(outFrame->data, outFrame->linesize, lx, ly,
		AV_PIX_FMT_RGB24, 32);
	if (ret < 0) {
		saveStatus = "Could not allocate raw picture buffer\n";
	}

	//convert from RGBA to YUV420P
	int swsSuccess = sws_scale(sws_ctx, (uint8_t const * const *)inFrame->data,
		inFrame->linesize, 0, ly,
		outFrame->data, outFrame->linesize);
	TFilePath path(TEnv::getStuffDir() + "projects/temp/");
	QString tempName = "frame" + QString::number(frameIndex) + ".ppm";
	tempName = path.getQString() + tempName;

	//QString path = TEnv::getStuffDir().getQString() + "/projects/temp/" + tempName;
	QByteArray ba = tempName.toLatin1();
	const char *charPath = ba.data();
	//const char* charPath = tempName.toLatin1().data();
	std::string strPath = tempName.toStdString();

	
	
	FILE* pFile = fopen(charPath, "wb");
	if (!pFile)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", lx, ly);

	// Write pixel data
	for (int y = 0; y<ly; y++)
		fwrite(outFrame->data[0] + y*outFrame->linesize[0], 1, lx * 3, pFile);
	m_frameCount++; 
	// Close file
	fclose(pFile);
	av_free(inFrame);
	av_free(outFrame);
	
}


Tiio::Mp4WriterProperties::Mp4WriterProperties(){}

Tiio::Reader* Tiio::makeMp4Reader(){ return nullptr; }
Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; }