// Glew include
#include <GL/glew.h>

#include "toonz/imagepainter.h"
#include "toonz/glrasterpainter.h"
#include "trastercm.h"
#include "tropcm.h"
#include "tpalette.h"
#include "tgl.h"
#include "tvectorimage.h"
#include "tenv.h"
#include "trop.h"
#include "tvectorrenderdata.h"
#include "tvectorgl.h"
#include "trasterimage.h"
#include "timagecache.h"
#include "toonz/sceneproperties.h"
#include "toonz/stage2.h"
#include "toonz/preferences.h"
#include "toonz/fill.h"

using namespace ImagePainter;

TEnv::IntVar FlipBookWhiteBgToggle("FlipBookWhiteBgToggle", 1);
TEnv::IntVar FlipBookBlackBgToggle("FlipBookBlackBgToggle", 0);
TEnv::IntVar FlipBookCheckBgToggle("FlipBookCheckBgToggle", 0);
namespace {

//-----------------------------------------------------------------------------

TRasterP keepChannels(const TRasterP &rin, TPalette *palette, UCHAR channel) {
  TRasterP rout;
  if ((TRasterFP)rin) {
    rout = rin->clone();

    TPixelF *pix = (TPixelF *)rout->getRawData();

    assert(channel & TRop::MChan);
    int i;
    for (i = 0; i < rout->getLx() * rout->getLy(); i++, pix++) {
      if (!(channel & TRop::RChan)) pix->r = 0.f;
      if (!(channel & TRop::GChan)) pix->g = 0.f;
      if (!(channel & TRop::BChan)) pix->b = 0.f;
    }
  } else {
    rout = TRaster32P(rin->getSize());

    if ((TRasterCM32P)rin)
      TRop::convert(rout, (TRasterCM32P)rin, TPaletteP(palette));
    else
      TRop::copy(rout, rin);

    TPixel32 *pix = (TPixel32 *)rout->getRawData();

    assert(channel & TRop::MChan);
    int i;

    for (i = 0; i < rout->getLx() * rout->getLy(); i++, pix++) {
      if (!(channel & TRop::RChan)) pix->r = 0;
      if (!(channel & TRop::GChan)) pix->g = 0;
      if (!(channel & TRop::BChan)) pix->b = 0;
    }
  }
  return rout;
}

//-----------------------------------------------------------------------------

inline void quickput(const TRasterP &rout, const TRasterP &rin,
                     const TPaletteP &palette, const TAffine &aff,
                     bool useChecks) {
  if (TRasterCM32P srcCM32 = rin) {
    if (useChecks) {
      bool inksOnly       = false;
      TPixel32 colorscale = TPixel32(0, 0, 0, 255);
      int tc              = ToonzCheck::instance()->getChecks();
      int index           = ToonzCheck::instance()->getColorIndex();

      if (tc & ToonzCheck::eGap) {
        srcCM32 = srcCM32->clone();
        AreaFiller(srcCM32).rectFill(srcCM32->getBounds(), 1, true, true,
                                     false);
      }

      if (tc == 0 || tc == ToonzCheck::eBlackBg)
        TRop::quickPut(rout, srcCM32, palette, aff, colorscale, inksOnly);
      else {
        TRop::CmappedQuickputSettings settings;
        settings.m_globalColorScale = colorscale;
        settings.m_inksOnly         = inksOnly;
        settings.m_transparencyCheck =
            tc & (ToonzCheck::eTransparency | ToonzCheck::eGap);
        settings.m_blackBgCheck = tc & ToonzCheck::eBlackBg;
        settings.m_inkIndex =
            tc & ToonzCheck::eInk ? index : (tc & ToonzCheck::eInk1 ? 1 : -1);
        settings.m_paintIndex = tc & ToonzCheck::ePaint ? index : -1;
        Preferences::instance()->getTranspCheckData(
            settings.m_transpCheckBg, settings.m_transpCheckInk,
            settings.m_transpCheckPaint);
        TRop::quickPut(rout, srcCM32, palette, aff, settings);
      }
      srcCM32 = TRasterCM32P();

    } else
      TRop::quickPut(rout, rin, palette, aff);
  } else
    TRop::quickPut(rout, rin, aff);
}

//-----------------------------------------------------------------------------

TPixel32 *getBuffer(const TRect &rect) {
  static std::vector<char> buffer;
  int size = rect.getLx() * rect.getLy() * 4;
  if (size > (int)buffer.size()) buffer.resize(size);
  return (TPixel32 *)&buffer[0];
}

//-----------------------------------------------------------------------------

TRaster32P getCheckBoard(const TRect &rect, const TAffine &aff,
                         TSceneProperties *sprop) {
  double scale = sqrt(fabs(aff.det()));
  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  TRaster32P ras(rect.getLx(), rect.getLy(), rect.getLx(), getBuffer(rect));
  TRop::checkBoard(ras, col1, col2, TDimensionD(50 * scale, 50 * scale),
                   TPointD(-aff.a13, -aff.a23));
  return ras;
}

//-----------------------------------------------------------------------------

void drawCompareLines(const TDimension &dim, double compareX, double compareY) {
  glPushMatrix();
  glLoadIdentity();

  glColor3d(0.0, 0.0, 0.0);

  glBegin(GL_LINES);
  glVertex2d(dim.lx * compareX - 1, 0);
  glVertex2d(dim.lx * compareX - 1, dim.ly - 1);
  glVertex2d(dim.lx * compareX + 1, 0);
  glVertex2d(dim.lx * compareX + 1, dim.ly - 1);

  glVertex2d(0, dim.ly * compareY - 1);
  glVertex2d(dim.lx - 1, dim.ly * compareY - 1);
  glVertex2d(0, dim.ly * compareY + 1);
  glVertex2d(dim.lx - 1, dim.ly * compareY + 1);
  glEnd();
  glColor3d(1.0, 0.0, 0.0);
  glBegin(GL_LINES);
  glVertex2d(dim.lx * compareX, 0);
  glVertex2d(dim.lx * compareX, dim.ly - 1);

  glVertex2d(0, dim.ly * compareY);
  glVertex2d(dim.lx - 1, dim.ly * compareY);
  glEnd();
  glPopMatrix();
}

//-----------------------------------------------------------------------------

class Painter {
  TDimension m_dim;
  TDimension m_imageSize;
  TAffine m_aff;
  TAffine m_finalAff;

  TPalette *m_palette;
  TRectD m_bbox;
  TRasterP m_raster;
  bool m_useTexture;
  bool m_drawExternalBG;
  VisualSettings m_vSettings;

public:
  Painter(const TDimension &dim, const TDimension &imageSize,
          const TAffine &viewAff, TPalette *palette,
          const VisualSettings &visualSettings);

  void flushRasterImages(const TRect &loadbox, double compareX, double compareY,
                         bool swapCompared);
  void doFlushRasterImages(const TRasterP &rin, int bg,
                           const TPointD &offs = TPointD());
  void onVectorImage(TVectorImage *vi);
  void onRasterImage(TRasterImage *ri);
  void onToonzImage(TToonzImage *ti);
  void drawBlank();
  TRasterP buildCheckboard(int bg, const TDimension &dim,
                           TRasterP templateRas = TRaster32P());
};

//-----------------------------------------------------------------------------

Painter::Painter(const TDimension &dim, const TDimension &imageSize,
                 const TAffine &viewAff, TPalette *palette,
                 const VisualSettings &visualSettings)
    : m_dim(dim)
    , m_imageSize(imageSize)
    , m_aff(viewAff)
    , m_palette(palette)
    , m_bbox()
    , m_raster()
    , m_vSettings(visualSettings)
//, m_channel(visualSettings.m_colorMask)
//, m_greytones(visualSettings.m_greytones)
//, m_sceneProperties(visualSettings.m_sceneProperties)
//, m_useTexture(visualSettings.m_useTexture)
//, m_drawExternalBG(visualSettings.m_drawExternalBG)
{}

//-----------------------------------------------------------------------------

void Painter::flushRasterImages(const TRect &loadbox, double compareX,
                                double compareY, bool swapCompare) {
  if (!m_raster) return;
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DITHER);
  glDisable(GL_LOGIC_OP);
  glDisable(GL_STENCIL_TEST);

#ifdef GL_EXT_convolution
  if (GLEW_EXT_convolution) {
    glDisable(GL_CONVOLUTION_1D_EXT);
    glDisable(GL_CONVOLUTION_2D_EXT);
    glDisable(GL_SEPARABLE_2D_EXT);
  }
#endif

#ifdef GL_EXT_histogram
  if (GLEW_EXT_histogram) {
    glDisable(GL_HISTOGRAM_EXT);
    glDisable(GL_MINMAX_EXT);
  }
#endif

#ifdef GL_EXT_texture3D
  if (GL_EXT_texture3D) {
    glDisable(GL_TEXTURE_3D_EXT);
  }
#endif

  UCHAR m = m_vSettings.m_colorMask;
  bool showBg =
      (m == 0 || (m & TRop::MChan && !m_vSettings.m_greytones &&
                  (m & TRop::RChan || m & TRop::GChan || m & TRop::BChan)));

  int bg = showBg ? m_vSettings.m_bg : 0x40000;

  if (m_vSettings.m_drawExternalBG) {
    if (m_vSettings.m_colorMask != 0) {
      glClearColor(0, 0, 0, 1.0);
      glClear(GL_COLOR_BUFFER_BIT);
    } else {
      TPixel bgColor = Preferences::instance()->getPreviewBgColor();
      glClearColor(bgColor.r / 255.0f, bgColor.g / 255.0f, bgColor.b / 255.0f,
                   1.0);
      glClear(GL_COLOR_BUFFER_BIT);
    }
  }
  UCHAR chan = m_vSettings.m_colorMask;
  if (chan != 0 && !(chan & TRop::MChan) &&
      !(chan != TRop::MChan && chan & TRop::MChan) && !m_vSettings.m_greytones)
    glColorMask((chan & TRop::RChan) ? GL_TRUE : GL_FALSE,
                (chan & TRop::GChan) ? GL_TRUE : GL_FALSE,
                (chan & TRop::BChan) ? GL_TRUE : GL_FALSE, GL_TRUE);

  if (compareX != DefaultCompareValue || compareY != DefaultCompareValue) {
    glEnable(GL_SCISSOR_TEST);
    // draw right/down part of the screen...
    if (swapCompare)
      glScissor(0, 0,
                m_dim.lx * (compareX == DefaultCompareValue ? 1 : compareX),
                m_dim.ly * (compareY == DefaultCompareValue ? 1 : compareY));
    else
      glScissor(m_dim.lx * (compareX == DefaultCompareValue ? 0 : compareX),
                m_dim.ly * (compareY == DefaultCompareValue ? 0 : compareY),
                m_dim.lx, m_dim.ly);
    doFlushRasterImages(m_raster, bg, convert(loadbox.getP00()));

    TImageP refimg =
        TImageCache::instance()->get(QString("TnzCompareImg"), false);
    if ((TToonzImageP)refimg || (TRasterImageP)refimg) {
      // draw left/up part of the screen...
      TRasterP raux, rref;
      if ((TToonzImageP)refimg) {
        rref      = ((TToonzImageP)refimg)->getCMapped();
        m_palette = refimg->getPalette();
      } else
        rref = ((TRasterImageP)refimg)->getRaster();

      TRect rect = loadbox;
      raux       = loadbox != TRect() ? rref->extract(rect) : rref;

      TPointD cmpOffs = TPointD(
          0.5 * (((m_imageSize.lx == 0) ? m_raster->getLx() : m_imageSize.lx) -
                 rref->getLx()),
          0.5 * (((m_imageSize.ly == 0) ? m_raster->getLy() : m_imageSize.ly) -
                 rref->getLy()));

      if (swapCompare)
        glScissor(m_dim.lx * (compareX == DefaultCompareValue ? 0 : compareX),
                  m_dim.ly * (compareY == DefaultCompareValue ? 0 : compareY),
                  m_dim.lx, m_dim.ly);
      else
        glScissor(0, 0,
                  m_dim.lx * (compareX == DefaultCompareValue ? 1 : compareX),
                  m_dim.ly * (compareY == DefaultCompareValue ? 1 : compareY));
      doFlushRasterImages(raux, bg, cmpOffs + convert(loadbox.getP00()));
    }
    glDisable(GL_SCISSOR_TEST);
  } else
    doFlushRasterImages(m_raster, bg, convert(loadbox.getP00()));

  m_raster  = TRasterP();
  m_palette = 0;
}

//-----------------------------------------------------------------------------

TRasterP Painter::buildCheckboard(int bg, const TDimension &dim,
                                  TRasterP templateRas) {
  TRaster32P ras32(templateRas);
  TRaster64P ras64(templateRas);
  TRasterFP rasF(templateRas);
  templateRas = 0;
  TRasterP checkBoard;
  if (ras32)
    checkBoard = TRaster32P(100, 100);
  else if (ras64)
    checkBoard = TRaster64P(100, 100);
  else if (rasF)
    checkBoard = TRasterFP(100, 100);

  TPixel col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  TPointD p = TPointD(0, 0);
  if (m_vSettings.m_useTexture)
    p = TPointD(m_bbox.x0 > 0 ? 0 : -m_bbox.x0, m_bbox.y0 > 0 ? 0 : -m_bbox.y0);

  assert(checkBoard.getPointer());
  TRop::checkBoard(checkBoard, col1, col2, TDimensionD(50, 50), p);

  assert(checkBoard.getPointer());
  int lx = (m_imageSize.lx == 0 ? dim.lx : m_imageSize.lx);
  int ly = (m_imageSize.ly == 0 ? dim.ly : m_imageSize.ly);
  int x, y;

  TRasterP checkBoardRas;

  if (ras32)
    checkBoardRas = TRaster32P(lx, ly);
  else if (ras64)
    checkBoardRas = TRaster64P(lx, ly);
  else if (rasF)
    checkBoardRas = TRasterFP(lx, ly);

  for (y = 0; y < ly; y += 100) {
    for (x = 0; x < lx; x += 100) {
      // TAffine checkTrans = TTranslation(x,y);
      // if(m_vSettings.m_useTexture)
      //  quickput(textureBackGround, checkBoard, m_palette, checkTrans);
      // else
      quickput(checkBoardRas, checkBoard, m_palette, TTranslation(x, y), false);
    }
  }

  return checkBoardRas;
}

//---------------------------------------------------------------------

void Painter::doFlushRasterImages(const TRasterP &rin, int bg,
                                  const TPointD &offs) {
  if (!rin) return;
  TRectD viewRect(0.0, 0.0, m_dim.lx - 1, m_dim.ly - 1);
  TRectD bbox;
  if (m_vSettings.m_useTexture)
    bbox = m_bbox;
  else {
    // double delta = sqrt(fabs(m_finalAff.det()));
    // bbox = m_bbox.enlarge(delta) * viewRect;
    bbox = m_bbox * viewRect;
  }

  UCHAR chan = m_vSettings.m_colorMask;
  TRect rect(tfloor(bbox.x0), tfloor(bbox.y0), tceil(bbox.x1), tceil(bbox.y1));
  if (rect.isEmpty()) return;

  TRasterP ras;
  // TRaster32P ras;
  TRasterP _rin = rin;
  TAffine aff;
  GLenum bpcType = TGL_TYPE;
  // is16bpc = false;
  if (m_vSettings.m_useTexture) {
    ras = _rin;
    aff = m_aff;
    // ras->clear();
  } else {
    int lx = rect.getLx(), ly = rect.getLy();
    // when the "30bit display" preference option is enabled,
    // image previewed in 16bpc is not dithered & converted to 8bpc,
    // but is kept the channel depth as 16bpc.
    if (_rin->getPixelSize() == 8) {
      ras     = TRaster64P(lx, ly);
      bpcType = TGL_TYPE16;
    } else if (_rin->getPixelSize() == 16) {
      ras     = TRasterFP(lx, ly);
      bpcType = TGL_TYPE32F;
    } else
      ras = TRaster32P(lx, ly);

    /*
    // Following lines are used to solve a problem that occurs with some
    // graphics cards!
    // It seems that the glReadPixels() function is very slow if the lx length
    // isn't a multiple of 8!
    TDimension backgroundDim(lx, ly);
    backgroundDim.lx = (lx & 0x7) == 0 ? lx : lx + 8 - (lx & 0x7);

    static std::vector<char> buffer;
    int size = backgroundDim.lx * backgroundDim.ly * 4;
    if (size > (int)buffer.size()) buffer.resize(size);

    TRaster32P backgroundRas(backgroundDim.lx, backgroundDim.ly,
                             backgroundDim.lx, (TPixel32 *)&buffer[0]);
    glReadPixels(rect.x0, rect.y0, backgroundDim.lx, backgroundDim.ly, TGL_FMT,
                 TGL_TYPE, backgroundRas->getRawData());
    TRect r = rect - rect.getP00();
    ras     = backgroundRas->extract(r);
    */
    aff = TTranslation(-rect.x0, -rect.y0) * m_finalAff;
    aff *= TTranslation(TPointD(0.5, 0.5));  // very quick and very dirty fix:
                                             // in camerastand the images seems
                                             // shifted of an half pixel...it's
                                             // a quickput approximation?
    if (bg == 0x100000)
      quickput(ras, buildCheckboard(bg, _rin->getSize(), ras), m_palette, aff,
               false);
    else {
      if (bpcType == TGL_TYPE16)
        ((TRaster64P)ras)
            ->fill(bg == 0x40000 ? TPixel64::Black : TPixel64::White);
      else if (bpcType == TGL_TYPE32F)
        ((TRasterFP)ras)->fill(bg == 0x40000 ? TPixelF::Black : TPixelF::White);
      else
        ((TRaster32P)ras)->fill(bg == 0x40000 ? TPixel::Black : TPixel::White);
    }
  }

  ras->lock();

  bool showChannelsOnMatte =
      (chan != TRop::MChan && chan != 0 && !m_vSettings.m_greytones);

  if (m_vSettings.m_useTexture) {
    TRaster32P checkBoardRas = buildCheckboard(bg, _rin->getSize());
    GLRasterPainter::drawRaster(aff, checkBoardRas->getRawData(),
                                checkBoardRas->getWrap(), 4,
                                checkBoardRas->getSize(), true);
    if (showChannelsOnMatte)
      ras = keepChannels(_rin, m_palette, chan);
    else if (chan == TRop::MChan || m_vSettings.m_greytones) {
      TRasterP app = ras->create(ras->getLx(), ras->getLy());
      try {
        TRop::setChannel(ras, app, chan, m_vSettings.m_greytones);
        ras = app;
      } catch (...) {
      }
    }
    GLRasterPainter::drawRaster(aff, ras->getRawData(), ras->getWrap(), 4,
                                ras->getSize(), true);
    ras->unlock();
  } else {
    if (showChannelsOnMatte)
      quickput(ras, keepChannels(_rin, m_palette, chan), m_palette,
               m_vSettings.m_useTexture ? TAffine() : aff * TTranslation(offs),
               m_vSettings.m_useChecks);
    else if (chan != 0) {
      TRasterP app = ras->create(_rin->getLx(), _rin->getLy());
      quickput(app, _rin, m_palette, TAffine(), m_vSettings.m_useChecks);
      try {
        TRop::setChannel(app, app, chan, m_vSettings.m_greytones);
      } catch (...) {
      }
      quickput(ras, app, m_palette, aff * TTranslation(offs),
               m_vSettings.m_useChecks);
    } else {
      quickput(ras, _rin, m_palette, aff * TTranslation(offs),
               m_vSettings.m_useChecks);
    }

    glDisable(GL_BLEND);

    glPushMatrix();
    glLoadIdentity();

    glRasterPos2d(rect.x0, rect.y0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glDrawPixels(ras->getWrap(), ras->getLy(), TGL_FMT, bpcType,
                 (GLvoid *)ras->getRawData());

    CHECK_ERRORS_BY_GL

    ras->unlock();
    glPopMatrix();
  }
}

//-----------------------------------------------------------------------------

void Painter::onVectorImage(TVectorImage *vi) {
  TRect clipRect = m_dim;
  clipRect -= TPoint(m_dim.lx * 0.5, m_dim.ly * 0.5);

  bool thereIsColorFilter =
      (m_vSettings.m_colorMask != 0 && m_vSettings.m_colorMask != TRop::MChan);

  if (m_vSettings.m_bg == 0x100000 && !thereIsColorFilter) {
    TRaster32P check =
        getCheckBoard(clipRect, m_aff, m_vSettings.m_sceneProperties);

    glDisable(GL_BLEND);

    glPushMatrix();
    glLoadIdentity();
    glRasterPos2d(0, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glDrawPixels(check->getLx(), check->getLy(), TGL_FMT, TGL_TYPE,
                 check->getRawData());
    glPopMatrix();
  } else {
    if (m_vSettings.m_bg == 0x40000 || thereIsColorFilter)
      glClearColor(0.0, 0.0, 0.0, 1.0);
    else if (m_vSettings.m_bg == 0x80000)
      glClearColor(1.0, 1.0, 1.0, 1.0);

    glClear(GL_COLOR_BUFFER_BIT);
  }

  TVectorRenderData rd(m_aff, clipRect, vi->getPalette(), 0,
                       true  // alfa enabled
  );
  if (m_vSettings.m_useChecks) {
    ToonzCheck *tc         = ToonzCheck::instance();
    int checks             = tc->getChecks();  // &ToonzCheck::eBlackBg
    rd.m_inkCheckEnabled   = checks & ToonzCheck::eInk;
    rd.m_paintCheckEnabled = checks & ToonzCheck::ePaint;
    rd.m_colorCheckIndex   = tc->getColorIndex();
  }
  tglDraw(rd, vi);
}

//-----------------------------------------------------------------------------

void Painter::onRasterImage(TRasterImage *ri) {
  TDimension imageSize =
      (m_imageSize.lx == 0 ? ri->getRaster()->getSize() : m_imageSize);

  m_finalAff =
      m_vSettings.m_useTexture
          ? m_aff
          : TTranslation(m_dim.lx * 0.5, m_dim.ly * 0.5) * m_aff *
                TTranslation(-TPointD(0.5 * imageSize.lx, 0.5 * imageSize.ly));

  TRectD bbox = TRectD(0, 0, imageSize.lx - 1, imageSize.ly - 1);
  m_bbox      = m_vSettings.m_useTexture ? bbox : m_finalAff * bbox;

  if (!m_bbox.overlaps(TRectD(0, 0, m_dim.lx - 1, m_dim.ly - 1))) return;
  m_raster = ri->getRaster();
}

//-----------------------------------------------------------------------------

void Painter::onToonzImage(TToonzImage *ti) {
  TDimension imageSize =
      (m_imageSize.lx == 0 ? ti->getRaster()->getSize() : m_imageSize);

  m_finalAff =
      m_vSettings.m_useTexture
          ? m_aff
          : TTranslation(m_dim.lx * 0.5, m_dim.ly * 0.5) * m_aff *
                TTranslation(-TPointD(0.5 * imageSize.lx, 0.5 * imageSize.ly));

  TRectD bbox = TRectD(0, 0, imageSize.lx - 1, imageSize.ly - 1);
  m_bbox      = m_vSettings.m_useTexture ? bbox : m_finalAff * bbox;
  if (!m_bbox.overlaps(TRectD(0, 0, m_dim.lx - 1, m_dim.ly - 1))) return;
  m_raster = ti->getRaster();
}

//-----------------------------------------------------------------------------

void Painter::drawBlank() {
  TPixel bgColor = Preferences::instance()->getPreviewBgColor();
  glClearColor(bgColor.r / 255.0f, bgColor.g / 255.0f, bgColor.b / 255.0f, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glPushMatrix();
  glLoadIdentity();
  tglColor(m_vSettings.m_blankColor);
  tglFillRect(m_bbox);
  glPopMatrix();
}

}  // namespace

//=============================================================================

ImagePainter::VisualSettings::VisualSettings()
    : m_colorMask(0)
    , m_greytones(false)
    , m_doCompare(false)
    , m_defineLoadbox(false)
    , m_useLoadbox(false)
    , m_blankColor(TPixel::Transparent)
    , m_useTexture(false)
    , m_drawExternalBG(false)
    , m_showBBox(false)
    , m_sceneProperties(0)
    , m_recomputeIfNeeded(true)
    , m_drawBlankFrame(false)
    , m_useChecks(false)
    , m_gainStep(0) {
  if (FlipBookBlackBgToggle) m_bg = 0x40000;
  if (FlipBookWhiteBgToggle) m_bg = 0x80000;
  if (FlipBookCheckBgToggle) m_bg = 0x100000;
}

//-----------------------------------------------------------------------------

bool ImagePainter::VisualSettings::needRepaint(const VisualSettings &vs) const {
  return !(m_colorMask == vs.m_colorMask && m_greytones == vs.m_greytones &&
           m_bg == vs.m_bg && m_doCompare == vs.m_doCompare &&
           m_defineLoadbox == vs.m_defineLoadbox &&
           m_useLoadbox == vs.m_useLoadbox && m_useTexture == vs.m_useTexture &&
           m_drawExternalBG == vs.m_drawExternalBG &&
           m_gainStep == vs.m_gainStep);
}

//=============================================================================

ImagePainter::CompareSettings::CompareSettings()
    : m_compareX(0.5)
    , m_compareY(DefaultCompareValue)
    , m_dragCompareX(false)
    , m_dragCompareY(false)
    , m_swapCompared(false) {}

//=============================================================================

void ImagePainter::paintImage(const TImageP &image, const TDimension &imageSize,
                              const TDimension &viewerSize, const TAffine &aff,
                              const VisualSettings &visualSettings,
                              const CompareSettings &compareSettings,
                              const TRect &loadbox) {
  glDisable(GL_DEPTH_TEST);

  if (visualSettings.m_drawExternalBG) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  GLenum error = glGetError();
  // assert(error==GL_NO_ERROR);
  if (error != GL_NO_ERROR) {
    printf("ImagePainter::paintImage() gl_error:%d\n", error);
  }

  if (!image) return;

  TRasterImageP rimg = (TRasterImageP)image;
  TVectorImageP vimg = (TVectorImageP)image;
  TToonzImageP timg  = (TToonzImageP)image;

  TRect clipRect(viewerSize);
  clipRect -= TPoint(viewerSize.lx * 0.5, viewerSize.ly * 0.5);
  Painter painter(viewerSize, imageSize, aff, image->getPalette(),
                  visualSettings);

  if (rimg)
    painter.onRasterImage(rimg.getPointer());
  else if (vimg)
    painter.onVectorImage(vimg.getPointer());
  else if (timg)
    painter.onToonzImage(timg.getPointer());

  if (visualSettings.m_blankColor != TPixel::Transparent) {
    painter.drawBlank();
    return;
  }

  // if I have a color filter applied using a glmask, , drawing of images must
  // be done on black bg!
  if (!vimg)
    painter.flushRasterImages(
        loadbox,
        visualSettings.m_doCompare ? compareSettings.m_compareX
                                   : DefaultCompareValue,
        visualSettings.m_doCompare ? compareSettings.m_compareY
                                   : DefaultCompareValue,
        compareSettings.m_swapCompared);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  if (visualSettings.m_doCompare)
    drawCompareLines(viewerSize, compareSettings.m_compareX,
                     compareSettings.m_compareY);
}
