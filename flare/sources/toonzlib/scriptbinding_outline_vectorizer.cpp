

#include "toonz/scriptbinding_outline_vectorizer.h"
#include "toonz/scriptbinding_level.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/stage.h"
#include "ttoonzimage.h"
#include "tpalette.h"

namespace TScriptBinding {

OutlineVectorizer::OutlineVectorizer() {
  m_parameters = new NewOutlineConfiguration();
}

OutlineVectorizer::~OutlineVectorizer() { delete m_parameters; }

QScriptValue OutlineVectorizer::toString() { return "Outline Vectorizer"; }

QScriptValue OutlineVectorizer::ctor(QScriptContext *context,
                                     QScriptEngine *engine) {
  return create(engine, new OutlineVectorizer());
}

static QScriptValue vectorizeImage(QScriptContext *context,
                                   QScriptEngine *engine, const TImageP &src,
                                   TPalette *palette,
                                   NewOutlineConfiguration *parameters) {
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
    return context->throwError(QObject::tr("Vectorization failed"));
  }
  if (dpix != 0.0 && dpiy != 0.0) dpiAff = TScale(factor / dpix, factor / dpiy);
  factor                                 = norm(dpiAff * TPointD(1, 0));

  parameters->m_affine     = dpiAff * TTranslation(-center);
  parameters->m_thickScale = factor;

  TVectorImageP vi = vc.vectorize(src, *parameters, palette);
  vi->setPalette(palette);
  return engine->newQObject(new Image(vi), QScriptEngine::AutoOwnership);
}

QScriptValue OutlineVectorizer::vectorize(QScriptValue arg) {
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
    return vectorizeImage(context(), engine(), img->getImg(), palette,
                          m_parameters);
  } else if (level) {
    QScriptValue newLevel = create(engine(), new Level());
    QList<TFrameId> fids;
    level->getFrameIds(fids);
    for (const TFrameId &fid : fids) {
      TImageP srcImg = level->getImg(fid);
      if (srcImg && (srcImg->getType() == TImage::RASTER ||
                     srcImg->getType() == TImage::TOONZ_RASTER)) {
        QScriptValue newFrame =
            vectorizeImage(context(), engine(), srcImg, palette, m_parameters);
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

int OutlineVectorizer::getAccuracy() const {
  return (5.0 - m_parameters->m_mergeTol) * 2.0;
}

void OutlineVectorizer::setAccuracy(int v) {
  m_parameters->m_mergeTol = 5.0 - v * 0.5;
}

int OutlineVectorizer::getDespeckling() const {
  return m_parameters->m_despeckling;
}

void OutlineVectorizer::setDespeckling(int v) {
  m_parameters->m_despeckling = v;
}

bool OutlineVectorizer::getPreservePaintedAreas() const {
  return !m_parameters->m_leaveUnpainted;
}

void OutlineVectorizer::setPreservePaintedAreas(bool v) {
  m_parameters->m_leaveUnpainted = !v;
}

double OutlineVectorizer::getCornerAdherence() const {
  return m_parameters->m_adherenceTol * 100.0;
}

void OutlineVectorizer::setCornerAdherence(double v) {
  m_parameters->m_adherenceTol = 0.01 * v;
}

double OutlineVectorizer::getCornerAngle() const {
  return m_parameters->m_angleTol * 180.0;
}

void OutlineVectorizer::setCornerAngle(double v) {
  m_parameters->m_angleTol = v / 180.0;
}

double OutlineVectorizer::getCornerCurveRadius() const {
  return m_parameters->m_relativeTol * 100;
}

void OutlineVectorizer::setCornerCurveRadius(double v) {
  m_parameters->m_relativeTol = 0.01 * v;
}

int OutlineVectorizer::getMaxColors() const {
  return m_parameters->m_maxColors;
}

void OutlineVectorizer::setMaxColors(int v) { m_parameters->m_maxColors = v; }

QString OutlineVectorizer::getTransparentColor() const {
  TPixel32 c = m_parameters->m_transparentColor;
  QColor color(c.r, c.g, c.b, c.m);
  return color.name();
}

void OutlineVectorizer::setTransparentColor(const QString &colorName) {
  QColor color;
  color.setNamedColor(colorName);
  if (color.isValid()) {
    m_parameters->m_transparentColor =
        TPixel32(color.red(), color.green(), color.blue(), color.alpha());
  } else {
    context()->throwError(tr("Invalid color : ").arg(colorName));
  }
}

int OutlineVectorizer::getToneThreshold() const {
  return m_parameters->m_toneTol;
}

void OutlineVectorizer::setToneThreshold(int v) { m_parameters->m_toneTol = v; }

}  // namespace TScriptBinding
