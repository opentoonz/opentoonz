

#include "toonz/scriptbinding_rasterizer.h"
#include "toonz/scriptbinding_image.h"
#include "toonz/scriptbinding_level.h"
#include "toonz/tcamera.h"
#include "toonz/stage.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "ttoonzimage.h"
#include "toonz/toonzimageutils.h"

namespace TScriptBinding {

Rasterizer::Rasterizer()
    : m_colorMapped(false)
    , m_xres(720)
    , m_yres(576)
    , m_dpi(72)
    , m_antialiasing(true) {}

Rasterizer::~Rasterizer() {}

QScriptValue Rasterizer::toString() { return "Rasterizer"; }

QScriptValue Rasterizer::ctor(QScriptContext *context, QScriptEngine *engine) {
  return create(engine, new Rasterizer());
}

static TToonzImageP vectorToToonzRaster(const TVectorImageP &vi,
                                        const TDimension &size,
                                        const TAffine &aff,
                                        const TPointD &dpi) {
  /*
TScale sc(dpi.x/Stage::inch, dpi.y/Stage::inch);
TRectD bbox = sc*vi->getBBox();
bbox.x0 = tfloor(bbox.x0);
bbox.y0 = tfloor(bbox.y0);
bbox.x1 = tceil(bbox.x1);
bbox.y1 = tceil(bbox.y1);
TDimension size(bbox.getLx(), bbox.getLy());

*/

  TToonzImageP ti = ToonzImageUtils::vectorToToonzImage(
      vi, aff, vi->getPalette(), TPointD(0, 0), size, 0, true);
  ti->setPalette(vi->getPalette());  // e' necessario?
  return ti;
}

static TImageP renderVectorImage(TOfflineGL *glContext,
                                 const TVectorRenderData &rd,
                                 const TPointD &dpi, const TImageP &img,
                                 const TPixel32 &color) {
  glContext->clear(color);
  glContext->draw(img, rd);
  TRasterImageP rimg(glContext->getRaster());
  rimg->setDpi(dpi.x, dpi.y);
  return rimg;
}

static void setFrame(QScriptEngine *engine, QScriptValue &level,
                     const TFrameId &fid, const TImageP &drawing) {
  QScriptValueList args;
  args << QString::fromStdString(fid.expand())
       << Wrapper::create(engine, new Image(drawing.getPointer()));
  level.property("setFrame").call(level, args);
}

Q_INVOKABLE QScriptValue Rasterizer::rasterize(QScriptValue arg) {
  Image *img        = qscriptvalue_cast<Image *>(arg);
  Level *level      = qscriptvalue_cast<Level *>(arg);
  TPalette *palette = 0;
  if (img) {
    if (img->getType() != "Vector")
      return context()->throwError(
          tr("Expected a vector image: %1").arg(arg.toString()));
    palette = !!img->getImg() ? img->getImg()->getPalette() : 0;
  } else if (level) {
    if (level->getType() != "Vector")
      return context()->throwError(
          tr("Expected a vector level: %1").arg(arg.toString()));
    palette =
        level->getSimpleLevel() ? level->getSimpleLevel()->getPalette() : 0;
  } else {
    return context()->throwError(
        tr("Argument must be a vector level or image : ").arg(arg.toString()));
  }
  if (!palette) {
    return context()->throwError(tr("%1 has no palette").arg(arg.toString()));
  }

  TDimension res(m_xres, m_yres);
  TCamera camera;
  camera.setRes(res);
  camera.setSize(TDimensionD(m_xres / m_dpi, m_yres / m_dpi));
  TPointD dpi = camera.getDpi();
  TAffine aff = camera.getStageToCameraRef();

  QScriptValue result;
  int n               = 0;
  TXshSimpleLevel *sl = 0;
  if (level) {
    result = create(engine(), new Level());
    n      = level->getFrameCount();
    sl     = level->getSimpleLevel();
  }

  if (m_colorMapped) {
    // vector -> toonz image
    if (img) {
      TImageP outImg = vectorToToonzRaster(img->getImg(), res, aff, dpi);
      result         = create(engine(), new Image(outImg));
    } else {
      for (int i = 0; i < n; i++) {
        TFrameId fid    = sl->index2fid(i);
        TImageP drawing = sl->getFrame(fid, false);
        TImageP outImg  = vectorToToonzRaster(drawing, res, aff, dpi);
        setFrame(engine(), result, fid, outImg);
      }
    }
  } else {
    // vector -> full color
    TPixel32 bgColor = TPixel32::White;

    TOfflineGL *glContext = new TOfflineGL(res);
    glContext->makeCurrent();

    TVectorRenderData rd(TVectorRenderData::ProductionSettings(), aff, TRect(),
                         palette);
    rd.m_antiAliasing = m_antialiasing;

    if (img) {
      TImageP outImg =
          renderVectorImage(glContext, rd, dpi, img->getImg(), bgColor);
      result = create(engine(), new Image(outImg));
    } else {
      for (int i = 0; i < n; i++) {
        TFrameId fid    = sl->index2fid(i);
        TImageP drawing = sl->getFrame(fid, false);
        glContext->clear(TPixel32::White);
        TImageP outImg =
            renderVectorImage(glContext, rd, dpi, drawing, bgColor);
        setFrame(engine(), result, fid, outImg);
      }
    }
    delete glContext;
  }

  return result;
}

bool Rasterizer::getColorMapped() const { return m_colorMapped; }

void Rasterizer::setColorMapped(bool v) { m_colorMapped = v; }

int Rasterizer::getXRes() const { return m_xres; }

void Rasterizer::setXRes(int v) { m_xres = v; }

int Rasterizer::getYRes() const { return m_yres; }

void Rasterizer::setYRes(int v) { m_yres = v; }

double Rasterizer::getDpi() const { return m_dpi; }

void Rasterizer::setDpi(double v) { m_dpi = v; }

}  // namespace TScriptBinding
