

#include "moviegenerator.h"

#include "timage.h"
#include "tflash.h"
#include "tpalette.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tvectorrenderdata.h"
#include "tcolorstyles.h"
#include "tofflinegl.h"
#include "tsop.h"
#include "tcolorfunctions.h"
#include "tfilepath_io.h"
#include "toutputproperties.h"
#include "formatsettingspopups.h"
#include "tsystem.h"

#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/sceneproperties.h"
#include "toonz/onionskinmask.h"
#include "toonz/screensavermaker.h"
#include "toonz/tstageobject.h"
#include "toonz/dpiscale.h"
#include "toonz/stage.h"
#include "toonz/stage2.h"
#include "toonz/stagevisitor.h"
#include "toonz/stageplayer.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "toonz/trasterimageutils.h"

#include "tapp.h"
#include "toonz/tframehandle.h"

#ifdef MACOSX
namespace
{
//spostare magari in misc;
//ATTENZIONE: cosi' com'e' il codice e' SBAGLITO. Controllare. Inoltre non si capisce
// perche' e' sotto ifdef
void checkAndCorrectPremultipliedImage(TRaster32P ras)
{
	const int lx = ras->getLx();
	for (int y = 0; y < ras->getLy(); y++) {
		TPixel32 *pix = ras->pixels(y);
		TPixel32 *endPix = pix + lx;
		while (pix < endPix) {
			if (pix->r > pix->m)
				pix->r = pix->m;
			if (pix->g > pix->m)
				pix->g = pix->m;
			if (pix->b > pix->m)
				pix->b = pix->m;
			++pix;
		}
	}
}
}
#endif

//=============================================================================
// MovieGenerator::Imp
//-----------------------------------------------------------------------------

class MovieGenerator::Imp
{
public:
	TFilePath m_filepath;
	MovieGenerator::Listener *m_listener;
	TDimension m_cameraSize;
	TPixel32 m_bgColor;
	double m_fps;
	bool m_valid;
	IntPair m_renderRange;
	bool m_useMarkers;
	int m_sceneCount;
	int m_columnIndex;
	OnionSkinMask m_osMask;

	Imp(const TFilePath &fp, const TDimension cameraSize, double fps)
		: m_filepath(fp), m_listener(0), m_cameraSize(cameraSize), m_bgColor(TPixel32::White), m_fps(fps), m_valid(false), m_renderRange(0, -1), m_useMarkers(false), m_sceneCount(1), m_columnIndex(-1), m_osMask()
	{
	}
	virtual ~Imp() {}

	virtual void startScene(const ToonzScene &scene, int r) = 0;
	virtual void addSoundtrack(const ToonzScene &scene, int frameOffset, int sceneFrameCount) = 0;
	virtual bool addFrame(ToonzScene &scene, int r, bool isLast) = 0;
	virtual void close() = 0;
};

//===================================================================

//
// AlphaChecker controlla se, sulla base dei parametri, viene utilizzato
// il canale di trasparenza (se la proprieta' booleana Alpha Channel esiste
// e vale true, oppure la "Resolution" vale L"32 bits"
//
class AlphaChecker : public TProperty::Visitor
{
	bool m_alphaEnabled;

public:
	AlphaChecker() : m_alphaEnabled(false) {}
	bool isAlphaEnabled() const { return m_alphaEnabled; }

	void visit(TDoubleProperty *p) {}
	void visit(TIntProperty *p) {}
	void visit(TBoolProperty *p)
	{
		if (p->getName() == "Alpha Channel")
			m_alphaEnabled = p->getValue();
	}
	void visit(TStringProperty *p) {}
	void visit(TEnumProperty *p)
	{
		if (p->getName() == "Resolution")
			m_alphaEnabled = p->getValue() == L"32 bits";
	}
};

//=============================================================================
// RasterMovieGenerator ( animazioni raster)
//-----------------------------------------------------------------------------

class RasterMovieGenerator
	: public MovieGenerator::Imp
{

public:
	bool m_isFrames;
	TLevelWriterP m_lw;
	bool m_alphaEnabled; //! alpha channel is generated (to disk)
	bool m_alphaNeeded;  //! alpha channel will be visible (i.e. transparent background)

	TAffine m_viewAff;
	TOfflineGL &m_offlineGlContext;
	int m_frameIndex;
	bool m_started;
	TSoundTrackP m_st;
	long m_whiteSample; //! ?? something audio related
	TPropertyGroup *m_fileOptions;
	int m_status;

	RasterMovieGenerator(
		const TFilePath &fp,
		const TDimension cameraSize,
		TOutputProperties &properties)
		: Imp(fp, cameraSize, properties.getFrameRate()), m_frameIndex(1), m_started(false), m_offlineGlContext(*TOfflineGL::getStock(cameraSize)), m_st(0), m_whiteSample(0), m_fileOptions(0), m_alphaEnabled(false), m_alphaNeeded(false), m_status(0)
	{
		m_bgColor = TPixel32(255, 255, 255, 0);
		TPointD center(0.5 * cameraSize.lx, 0.5 * cameraSize.ly);
		m_viewAff = TTranslation(center);
		std::string ext = fp.getType();
		m_isFrames = ext != "avi" && ext != "mov" && ext != "3gp";
		m_fileOptions = properties.getFileFormatProperties(ext);
	}

	void start(const ToonzScene &scene)
	{
		m_status = 3;
		m_alphaNeeded = scene.getProperties()->getBgColor().m < 255;
		assert(m_started == false);
		m_started = true;
		if (TSystem::doesExistFileOrLevel(m_filepath))
			TSystem::removeFileOrLevel(m_filepath);

		m_lw = TLevelWriterP(m_filepath);
		m_lw->setFrameRate(m_fps);
		if (m_lw->getProperties() && m_fileOptions)
			m_lw->getProperties()->setProperties(m_fileOptions);

		if (m_st)
			m_lw->saveSoundTrack(m_st.getPointer());
	}

	bool addFrame(ToonzScene &scene, int row, bool isLast)
	{
		assert(m_status == 3);
		if (!m_started)
			start(scene);

		TDimension cameraRes = scene.getCurrentCamera()->getRes();
		TDimensionD cameraSize = scene.getCurrentCamera()->getSize();

		TPointD center(0.5 * cameraSize.lx, 0.5 * cameraSize.ly);

		double sx = (double)m_offlineGlContext.getLx() / (double)cameraRes.lx;
		double sy = (double)m_offlineGlContext.getLy() / (double)cameraRes.ly;
		double sc = std::min(sx, sy);

		// TAffine cameraAff = scene.getXsheet()->getPlacement(TStageObjectId::CameraId(0), row);
		TAffine cameraAff = scene.getXsheet()->getCameraAff(row);

		double dpiScale = (1.0 / Stage::inch) * (double)cameraRes.lx / cameraSize.lx;

		//TAffine viewAff = TScale(dpiScale*sc) * TTranslation(center)* cameraAff.inv();
		TAffine viewAff =
			TTranslation(0.5 * cameraRes.lx, 0.5 * cameraRes.ly) * TScale(dpiScale * sc) * cameraAff.inv();

		TRect clipRect(m_offlineGlContext.getBounds());
		TPixel32 bgColor = scene.getProperties()->getBgColor();

		m_offlineGlContext.makeCurrent();
		TPixel32 bgClearColor = m_bgColor;

		if (m_alphaEnabled && m_alphaNeeded) {
			const double maxValue = 255.0;
			double alpha = (double)bgClearColor.m / maxValue;
			bgClearColor.r *= alpha;
			bgClearColor.g *= alpha;
			bgClearColor.b *= alpha;
		}
		m_offlineGlContext.clear(bgClearColor);

		Stage::VisitArgs args;
		args.m_scene = &scene;
		args.m_xsh = scene.getXsheet();
		args.m_row = row;
		args.m_col = m_columnIndex;
		args.m_osm = &m_osMask;

		ImagePainter::VisualSettings vs;
		Stage::OpenGlPainter painter(viewAff, clipRect, vs, false, true);

		Stage::visit(painter, args);
		/*
       painter, 
       &scene,
       scene.getXsheet(), row, 
       m_columnIndex, m_osMask, 
       false,0);
     */
		TImageWriterP writer = m_lw->getFrameWriter(m_frameIndex++);
		if (!writer)
			return false;

#ifdef MACOSX
		glFinish(); // per fissare il bieco baco su Mac/G3
#endif

		TRaster32P raster = m_offlineGlContext.getRaster();

#ifdef MACOSX
		if (m_alphaEnabled && m_alphaNeeded)
			checkAndCorrectPremultipliedImage(raster);
#endif

		if (Preferences::instance()->isSceneNumberingEnabled())
			TRasterImageUtils::addSceneNumbering(TRasterImageP(raster), m_frameIndex - 1, scene.getSceneName(), row + 1);
		TRasterImageP img(raster);
		writer->save(img);

		return true;
	}

	void addSoundtrack(const ToonzScene &scene, int frameOffset, int sceneFrameCount)
	{
		assert(m_status <= 2);
		m_status = 2;
		TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
		prop->m_frameRate = m_fps;
		TSoundTrack *snd = scene.getXsheet()->makeSound(prop);
		if (!snd || m_filepath.getDots() == "..") {
			m_whiteSample += sceneFrameCount * 918;
			return;
		}
		long samplePerFrame = snd->getSampleRate() / m_fps;
		TSoundTrackP snd1 = snd->extract(frameOffset * samplePerFrame, (TINT32)((frameOffset + sceneFrameCount - 1) * samplePerFrame));
		if (!m_st) {
			m_st = TSoundTrack::create(snd1->getFormat(), m_whiteSample);
			m_whiteSample = 0;
		}
		TINT32 fromSample = m_st->getSampleCount();
		TINT32 numSample = std::max(
			TINT32(sceneFrameCount * samplePerFrame),
			snd1->getSampleCount());
		m_st = TSop::insertBlank(m_st, fromSample, numSample + m_whiteSample);
		m_st->copy(snd1, TINT32(fromSample + m_whiteSample));
		m_whiteSample = 0;
	}

	void startScene(const ToonzScene &scene, int r)
	{
		if (!m_started)
			start(scene);
	}

	void close()
	{
		m_lw = TLevelWriterP();
		m_st = TSoundTrackP();
	}
};

//=============================================================================
// FlashStagePainter
//-----------------------------------------------------------------------------

class FlashStagePainter : public Stage::Visitor
{
	TFlash &m_flash;
	TAffine m_cameraAff;
	TPixel32 m_bgColor;

public:
	FlashStagePainter(
		TFlash &flash, TAffine &cameraAff,
		const ImagePainter::VisualSettings &vs, const TPixel32 &bgColor)
		: Visitor(vs), m_flash(flash), m_cameraAff(cameraAff), m_bgColor(bgColor) {}

	void onImage(const Stage::Player &player)
	{
		const TImageP &img = player.image();
		m_flash.pushMatrix();
		m_flash.multMatrix(m_cameraAff * player.m_placement * player.m_dpiAff);

		TColorFunction *cf = 0;
		TColorFader fader;
		if (player.m_onionSkinDistance != c_noOnionSkin) {
			fader = TColorFader(m_bgColor, OnionSkinMask::getOnionSkinFade(player.m_onionSkinDistance));
			cf = &fader;
		}

		if (img->getPalette())
			img->getPalette()->setFrame(player.m_frame);
		m_flash.draw(img, cf);

		m_flash.popMatrix();
	}
	void beginMask() { m_flash.beginMask(); }
	void endMask() { m_flash.endMask(); }
	void enableMask() { m_flash.enableMask(); }
	void disableMask() { m_flash.disableMask(); }
};

//=============================================================================
// FlashMovieGenerator
//-----------------------------------------------------------------------------

class FlashMovieGenerator
	: public MovieGenerator::Imp
{

public:
	TFlash m_flash;
	TAffine m_viewAff;
	int m_frameIndex;
	int m_sceneIndex;
	int m_frameCountLoader;
	bool m_screenSaverMode;

	FlashMovieGenerator(
		const TFilePath &fp,
		const TDimension cameraSize,
		TOutputProperties &properties)
		: Imp(fp, cameraSize, properties.getFrameRate()), m_flash(cameraSize.lx, cameraSize.ly, 0, properties.getFrameRate(), properties.getFileFormatProperties("swf")), m_frameIndex(0), m_sceneIndex(0), m_frameCountLoader(0), m_screenSaverMode(false)
	{

		TPointD center(0.5 * cameraSize.lx, 0.5 * cameraSize.ly);
		m_viewAff = TAffine();
		m_screenSaverMode = fp.getType() == "scr";
	}

	void addSoundtrack(const ToonzScene &scene, int frameOffset, int sceneFrameCount)
	{
	}

	void startScene(const ToonzScene &scene, int r)
	{
		m_flash.cleanCachedImages();
		m_flash.setBackgroundColor(m_bgColor);
		TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
		prop->m_frameRate = m_fps;
		TSoundTrackP snd = scene.getXsheet()->makeSound(prop);
		if (snd) {
			if (m_useMarkers) {
				long samplePerFrame = snd->getSampleRate() / m_fps;
				snd = snd->extract((TINT32)(m_renderRange.first * samplePerFrame),
								   (TINT32)(m_renderRange.second * samplePerFrame));
			}
			m_flash.putSound(snd, 0);
		}
		if (m_sceneIndex == 0 && m_sceneCount == 1) {
			if (m_renderRange.first == m_renderRange.second)
				m_frameCountLoader = 0;
			else
				m_frameCountLoader = 1;
		} else if (m_sceneIndex == 0)
			m_frameCountLoader = m_renderRange.second - m_renderRange.first + 1;
		m_sceneIndex++;
	}

	bool addFrame(ToonzScene &scene, int row, bool isLast)
	{
		TAffine cameraView = scene.getXsheet()->getPlacement(TStageObjectId::CameraId(0), row).inv();
		TPixel32 bgColor = scene.getProperties()->getBgColor();

		TStageObject *cameraPegbar = scene.getXsheet()->getStageObject(TStageObjectId::CameraId(0));
		assert(cameraPegbar);
		TCamera *camera = cameraPegbar->getCamera();
		assert(camera);
		TAffine dpiAff = getDpiAffine(camera).inv();

		TAffine aff = cameraView * dpiAff;

		Stage::VisitArgs args;
		args.m_scene = &scene;
		args.m_xsh = scene.getXsheet();
		args.m_row = row;
		args.m_col = m_columnIndex;
		args.m_osm = &m_osMask;

		ImagePainter::VisualSettings vs;
		FlashStagePainter painter(m_flash, aff, vs, bgColor);
		m_flash.beginFrame(++m_frameIndex);

		Stage::visit(painter, args);
		/*
		             &scene,
				         scene.getXsheet(),
								 row,
								 m_columnIndex,
								 m_osMask,
								 false, 0);
                 */

		m_frameIndex =
			m_flash.endFrame(isLast, m_frameCountLoader, (m_sceneCount == m_sceneIndex));
		return true;
	}

	void close()
	{
		TFilePath flashFn = m_filepath.withType("swf");
		FILE *fp = fopen(flashFn, "wb");
		if (!fp)
			throw TException("Can't open file");
		m_flash.writeMovie(fp);
		fclose(fp);
		if (m_screenSaverMode) {
			makeScreenSaver(m_filepath, flashFn, "prova");
		}
	}
};

//=============================================================================
// MovieGenerator
//-----------------------------------------------------------------------------

MovieGenerator::MovieGenerator(
	const TFilePath &path,
	const TDimension &resolution,
	TOutputProperties &outputProperties,
	bool useMarkers)
{
	if (path.getType() == "swf" || path.getType() == "scr")
		m_imp.reset(new FlashMovieGenerator(path, resolution, outputProperties));
	else
		m_imp.reset(new RasterMovieGenerator(path, resolution, outputProperties));
	m_imp->m_useMarkers = useMarkers;
}

//-------------------------------------------------------------------

MovieGenerator::~MovieGenerator()
{
}

//-------------------------------------------------------------------

void MovieGenerator::setListener(Listener *listener)
{
	m_imp->m_listener = listener;
}

//-------------------------------------------------------------------

void MovieGenerator::close()
{
	m_imp->close();
}

//-------------------------------------------------------------------

void MovieGenerator::setSceneCount(int sceneCount)
{
	m_imp->m_sceneCount = sceneCount;
}

//-------------------------------------------------------------------

bool MovieGenerator::addSoundtrack(const ToonzScene &scene, int frameOffset, int sceneFrameCount)
{
	m_imp->addSoundtrack(scene, frameOffset, sceneFrameCount);
	return true;
}

//-------------------------------------------------------------------

bool MovieGenerator::addScene(ToonzScene &scene, int r0, int r1)
{
	TApp *app = TApp::instance();
	RasterMovieGenerator *imp = dynamic_cast<RasterMovieGenerator *>(m_imp.get());
	if (imp)
		imp->m_alphaEnabled = true;
	m_imp->m_renderRange = std::make_pair(r0, r1);
	m_imp->startScene(scene, r0);
	for (int r = r0; r <= r1; r++) {
		TColorStyle::m_currentFrame = r;
		if (!m_imp->addFrame(scene, r, r == r1)) {
			TColorStyle::m_currentFrame = app->getCurrentFrame()->getFrame();
			return false;
		}
		if (m_imp->m_listener) {
			bool ret = m_imp->m_listener->onFrameCompleted(r);
			if (!ret) {
				throw TException("Execution canceled by user");
				TColorStyle::m_currentFrame = app->getCurrentFrame()->getFrame();
				return false;
			}
		}
	}
	TColorStyle::m_currentFrame = app->getCurrentFrame()->getFrame();
	return true;
}

//-------------------------------------------------------------------

void MovieGenerator::setBackgroundColor(TPixel32 color)
{
	m_imp->m_bgColor = color;
}

//-------------------------------------------------------------------

TPixel32 MovieGenerator::getBackgroundColor()
{
	return m_imp->m_bgColor;
}

//-------------------------------------------------------------------

TFilePath MovieGenerator::getOutputPath()
{
	return m_imp->m_filepath;
}

//-------------------------------------------------------------------

void MovieGenerator::setOnionSkin(int columnIndex, const OnionSkinMask &mask)
{
	m_imp->m_columnIndex = columnIndex;
	m_imp->m_osMask = mask;
}

//-------------------------------------------------------------------
