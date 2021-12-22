

#include "toonz/scriptbinding_centerline_vectorizer.h"
#include "toonz/scriptbinding_level.h"
#include <QScriptEngine>
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/stage.h"
#include "toonz/Naa2TlvConverter.h"
#include "tpalette.h"
#include "ttoonzimage.h"

namespace TScriptBinding {

CenterlineVectorizer::CenterlineVectorizer() {
  m_parameters = new CenterlineConfiguration();
}

CenterlineVectorizer::~CenterlineVectorizer() { delete m_parameters; }

QScriptValue CenterlineVectorizer::toString() {
  return "Centerline Vectorizer";
}

QScriptValue CenterlineVectorizer::ctor(QScriptContext *context,
                                        QScriptEngine *engine) {
  return create(engine, new CenterlineVectorizer());
}

QScriptValue CenterlineVectorizer::vectorizeImage(const TImageP &src,
                                                  TPalette *palette) {
  VectorizerCore vc;
  TAffine dpiAff;
  double factor = Stage::inch;
  double dpix = factor / 72, dpiy = factor / 72;
  TPointD center;
  if (TRasterImageP ri = src) {
    ri->getDpi(dpix, dpiy);
    center = ri->getRaster()->getCenterD();
  } else if (TToonzImageP ti = src) {
    ti->getDpi(dpix, dpiy);
    center = ti->getRaster()->getCenterD();
  } else {
    return context()->throwError(QObject::tr("Vectorization failed"));
  }
  if (dpix != 0.0 && dpiy != 0.0) dpiAff = TScale(factor / dpix, factor / dpiy);
  factor                                 = norm(dpiAff * TPointD(1, 0));

  m_parameters->m_affine     = dpiAff * TTranslation(-center);
  m_parameters->m_thickScale = factor;

  palette->addRef();  // if there are no other references the vectorize() method
                      // below can destroy the palette
                      // BEFORE assigning it to the vector image
  TVectorImageP vi = vc.vectorize(src, *m_parameters, palette);
  vi->setPalette(palette);
  palette->release();

  return engine()->newQObject(new Image(vi), QScriptEngine::AutoOwnership);
}

QScriptValue CenterlineVectorizer::vectorize(QScriptValue arg) {
  Level *level = qscriptvalue_cast<Level *>(arg);
  Image *img   = qscriptvalue_cast<Image *>(arg);
  QString type;
  TPalette *palette = 0;
  if (level) {
    type = level->getType();
    if (type != "Raster" && type != "ToonzRaster")
      return context()->throwError(tr("Can't vectorize a %1 level").arg(type));
    if (level->getFrameCount() <= 0)
      return context()->throwError(
          tr("Can't vectorize a level with no frames"));
    palette = level->getSimpleLevel()->getPalette();
  } else if (img) {
    type = img->getType();
    if (type != "Raster" && type != "ToonzRaster")
      return context()->throwError(tr("Can't vectorize a %1 image").arg(type));
    if (TToonzImageP ti = img->getImg()) {
      palette = ti->getPalette();
    }
  } else {
    return context()->throwError(
        tr("Bad argument (%1): should be an Image or a Level")
            .arg(arg.toString()));
  }
  if (palette == 0) palette = new TPalette();
  if (img) {
    return vectorizeImage(img->getImg(), palette);
  } else if (level) {
    QScriptValue newLevel = create(engine(), new Level());
    QList<TFrameId> fids;
    level->getFrameIds(fids);
    for (const TFrameId &fid : fids) {
      TImageP srcImg = level->getImg(fid);
      if (srcImg && (srcImg->getType() == TImage::RASTER ||
                     srcImg->getType() == TImage::TOONZ_RASTER)) {
        QScriptValue newFrame = vectorizeImage(srcImg, palette);
        if (newFrame.isError()) {
          return newFrame;
        }
        QScriptValueList args;
        args << QString::fromStdString(fid.expand()) << newFrame;
        newLevel.property("setFrame").call(newLevel, args);
      }
    }
    return newLevel;
  } else {
    // should never happen
    return QScriptValue();
  }
}

int CenterlineVectorizer::getThreshold() const {
  return m_parameters->m_threshold / 25;
}

void CenterlineVectorizer::setThreshold(int v) {
  m_parameters->m_threshold = v * 25;
}

int CenterlineVectorizer::getAccuracy() const {
  return 10 - m_parameters->m_penalty;
}

void CenterlineVectorizer::setAccuracy(int v) {
  m_parameters->m_penalty = 10 - v;
}

int CenterlineVectorizer::getDespeckling() const {
  return m_parameters->m_despeckling / 2;
}

void CenterlineVectorizer::setDespeckling(int v) {
  m_parameters->m_despeckling = v * 2;
}

double CenterlineVectorizer::getMaxThickness() const {
  return m_parameters->m_maxThickness * 2.0;
}

void CenterlineVectorizer::setMaxThickness(double v) {
  m_parameters->m_maxThickness = v / 2.0;
}

double CenterlineVectorizer::getThicknessCalibration() const {
  return m_parameters->m_thicknessRatio;
}

void CenterlineVectorizer::setThicknessCalibration(double v) {
  m_parameters->m_thicknessRatio = v;
}

bool CenterlineVectorizer::getPreservePaintedAreas() const {
  return !m_parameters->m_leaveUnpainted;
}

void CenterlineVectorizer::setPreservePaintedAreas(bool v) {
  m_parameters->m_leaveUnpainted = !v;
}

bool CenterlineVectorizer::getAddBorder() const {
  return m_parameters->m_makeFrame;
}

void CenterlineVectorizer::setAddBorder(bool v) {
  m_parameters->m_makeFrame = v;
}

bool CenterlineVectorizer::getEir() const { return m_parameters->m_naaSource; }

void CenterlineVectorizer::setEir(bool v) { m_parameters->m_naaSource = v; }

}  // namespace TScriptBinding
