

#include "toonz/scriptbinding_image_builder.h"
#include "toonz/scriptbinding_image.h"
#include "ttoonzimage.h"
#include "trop.h"
#include <cmath>

namespace TScriptBinding {

Transform::Transform() {}

Transform::Transform(const TAffine &aff) : m_affine(aff) {}

Transform::~Transform() {}

QScriptValue Transform::ctor(QScriptContext *context, QScriptEngine *engine) {
  return create(engine, new Transform());
}

QScriptValue Transform::toString() {
  if (m_affine.isIdentity())
    return tr("Identity");
  else if (m_affine.isTranslation())
    return tr("Translation(%1,%2)").arg(m_affine.a13).arg(m_affine.a23);
  else {
    QString translationPart = "";
    if (m_affine.a13 != 0.0 || m_affine.a23 != 0.0)
      translationPart =
          tr("Translation(%1,%2)").arg(m_affine.a13).arg(m_affine.a23);

    if (fabs(m_affine.det() - 1.0) < 1.0e-8) {
      double phi           = atan2(m_affine.a12, m_affine.a11) * 180.0 / M_PI;
      phi                  = -(0.001 * floor(1000 * phi + 0.5));
      QString rotationPart = tr("Rotation(%1)").arg(phi);
      if (translationPart == "")
        return rotationPart;
      else
        return translationPart + "*" + rotationPart;
    } else if (m_affine.a12 == 0.0 && m_affine.a21 == 0.0) {
      QString scalePart;
      if (m_affine.a11 == m_affine.a22)
        scalePart = tr("Scale(%1%)").arg(m_affine.a11 * 100);
      else
        scalePart = tr("Scale(%1%, %2%)")
                        .arg(m_affine.a11 * 100)
                        .arg(m_affine.a22 * 100);
      if (translationPart == "")
        return scalePart;
      else
        return translationPart + "*" + scalePart;
    } else {
      return tr("Transform(%1, %2, %3;  %4, %5, %6)")
          .arg(m_affine.a11)
          .arg(m_affine.a12)
          .arg(m_affine.a13)
          .arg(m_affine.a21)
          .arg(m_affine.a22)
          .arg(m_affine.a23);
    }
  }
}

QScriptValue Transform::translate(double x, double y) {
  return create(engine(), new Transform(TTranslation(x, y) * m_affine));
}

QScriptValue Transform::rotate(double degrees) {
  return create(engine(), new Transform(TRotation(degrees) * m_affine));
}

QScriptValue Transform::scale(double s) {
  return create(engine(), new Transform(TScale(s) * m_affine));
}

QScriptValue Transform::scale(double sx, double sy) {
  return create(engine(), new Transform(TScale(sx, sy) * m_affine));
}

//=============================================================================

ImageBuilder::ImageBuilder() : m_width(0), m_height(0) {}

ImageBuilder::~ImageBuilder() {}

QScriptValue ImageBuilder::ctor(QScriptContext *context,
                                QScriptEngine *engine) {
  ImageBuilder *imageBuilder = 0;
  if (context->argumentCount() == 2 || context->argumentCount() == 3) {
    if (!context->argument(0).isNumber() || !context->argument(1).isNumber())
      return context->throwError("Bad arguments: expected width,height[,type]");
    int width  = (int)(context->argument(0).toNumber());
    int height = (int)(context->argument(1).toNumber());
    if (width <= 0 || height <= 0) return context->throwError("Bad size");
    QString type;
    if (context->argumentCount() == 3) {
      if (context->argument(2).isString())
        type = context->argument(2).toString();
      if (type != "Raster" && type != "ToonzRaster")
        return context->throwError(
            tr("Bad argument (%1): should be 'Raster' or ToonzRaster'")
                .arg(context->argument(2).toString()));
    }
    imageBuilder           = new ImageBuilder();
    imageBuilder->m_width  = width;
    imageBuilder->m_height = height;
    if (type == "Raster")
      imageBuilder->m_img = new TRasterImage(TRaster32P(width, height));
    else if (type == "ToonzRaster") {
      imageBuilder->m_img = new TToonzImage(TRasterCM32P(width, height),
                                            TRect(0, 0, width, height));
    }
  } else {
    if (context->argumentCount() != 0)
      return context->throwError(
          "Bad argument count. expected: width,height[,type]");
    imageBuilder = new ImageBuilder();
  }
  QScriptValue obj =
      engine->newQObject(imageBuilder, QScriptEngine::AutoOwnership,
                         QScriptEngine::ExcludeSuperClassContents |
                             QScriptEngine::ExcludeSuperClassMethods);
  return obj;
}

QScriptValue ImageBuilder::toString() {
  QString type = "Empty";
  if (m_img) {
    if (m_img->getType() == TImage::RASTER)
      type = "Raster";
    else if (m_img->getType() == TImage::TOONZ_RASTER)
      type = "ToonzRaster";
    else if (m_img->getType() == TImage::VECTOR)
      type = "Vector";
    else
      type = "Bad";
  }
  return tr("ImageBuilder(%1 image)").arg(type);
}

QScriptValue ImageBuilder::getImage() {
  return create(engine(), new Image(m_img));
}

QScriptValue ImageBuilder::fill(const QString &colorName) {
  QColor color;
  QScriptValue err = checkColor(context(), colorName, color);
  if (err.isError()) return err;
  TPixel32 pix(color.red(), color.green(), color.blue(), color.alpha());
  if (m_img) {
    if (m_img->getType() != TImage::RASTER)
      context()->throwError("Can't fill a non-'Raster' image");
    TRaster32P ras = m_img->raster();
    if (ras) ras->fill(pix);
  } else if (m_width > 0 && m_height > 0) {
    TRaster32P ras(m_width, m_height);
    ras->fill(pix);
    m_img = TRasterImageP(ras);
  }
  return context()->thisObject();
}

void ImageBuilder::clear() { m_img = TImageP(); }

QString ImageBuilder::add(const TImageP &img, const TAffine &aff) {
  if (fabs(aff.det()) < 0.001) return "";
  if (m_img && (m_img->getType() != img->getType()))
    return "Image type mismatch";

  if (!m_img && img->getType() != TImage::VECTOR && m_width > 0 &&
      m_height > 0) {
    TRasterP ras = img->raster()->create(m_width, m_height);
    if (img->getType() == TImage::RASTER)
      m_img = TRasterImageP(ras);
    else if (img->getType() == TImage::TOONZ_RASTER) {
      m_img = TToonzImageP(ras, ras->getBounds());
      m_img->setPalette(img->getPalette());
    } else
      return "Bad image type";
  }

  if (!m_img && aff.isIdentity()) {
    m_img = img->cloneImage();
  } else if (img->getType() == TImage::VECTOR) {
    // vector image
    if (!m_img) {
      // transform
      TVectorImageP vi = img->cloneImage();
      vi->transform(aff);
      m_img = vi;
    } else {
      // merge
      TVectorImageP up = img;
      TVectorImageP dn = m_img;
      dn->mergeImage(up, aff);
    }
  } else {
    if (img->getType() != TImage::TOONZ_RASTER &&
        img->getType() != TImage::RASTER) {
      // this should not ever happen
      return "Bad image type";
    }
    TRasterP up = img->raster();
    if (!m_img) {
      // create an empty bg
      TRasterP ras = up->create();
      ras->clear();
      if (img->getType() == TImage::TOONZ_RASTER) {
        TRasterCM32P rasCm = ras;
        m_img              = TToonzImageP(rasCm,
                             TRect(0, 0, ras->getLx() - 1, ras->getLy() - 1));
        m_img->setPalette(img->getPalette());
      } else {
        m_img = TRasterImageP(ras);
      }
    }
    TRasterP dn = m_img->raster();
    if (aff.isTranslation() && aff.a13 == floor(aff.a13) &&
        aff.a23 == floor(aff.a23)) {
      // just a integer coord translation
      TPoint delta = -up->getCenter() + dn->getCenter() +
                     TPoint((int)aff.a13, (int)aff.a23);
      TRop::over(dn, up, delta);
    } else {
      TAffine aff1 = TTranslation(dn->getCenterD()) * aff *
                     TTranslation(-up->getCenterD());
      TRop::over(dn, up, aff1, TRop::Mitchell);
    }
  }
  return "";
}

/*
  TImageP srcImg = img->getImg();
  if(srcImg->getType()==TImage::RASTER ||
  srcImg->getType()==TImage::TOONZ_RASTER)
  {
    TRasterP in = srcImg->raster();
    TRasterP out = in->create();
    TPointD center = in->getCenterD();
    TAffine aff1 = TTranslation(center) * aff * TTranslation(-center);
    TRop::resample(out,in,aff1,TRop::Mitchell);
    m_img = TRasterImageP(out);
  }
  else if(srcImg->getType()==TImage::VECTOR)
  {
    TVectorImageP vi = srcImg->cloneImage();
    vi->transform(aff);
    m_img = vi;
  }
  else
  {
    return context()->throwError(tr("Bad image type"));
  }
  return context()->thisObject();
  */

QScriptValue ImageBuilder::add(QScriptValue imgArg) {
  Image *simg      = 0;
  QScriptValue err = checkImage(context(), imgArg, simg);
  if (err.isError()) return err;
  QString errStr = add(simg->getImg(), TAffine());
  if (errStr != "")
    return context()->throwError(
        tr("%1 : %2").arg(errStr).arg(imgArg.toString()));
  else
    return context()->thisObject();
}

QScriptValue ImageBuilder::add(QScriptValue imgArg,
                               QScriptValue transformationArg) {
  Image *simg      = 0;
  QScriptValue err = checkImage(context(), imgArg, simg);
  if (err.isError()) return err;
  Transform *transformation = qscriptvalue_cast<Transform *>(transformationArg);
  if (!transformation) {
    return context()->throwError(
        tr("Bad argument (%1): should be a Transformation")
            .arg(transformationArg.toString()));
  }
  TAffine aff    = transformation->getAffine();
  QString errStr = add(simg->getImg(), aff);
  if (errStr != "")
    return context()->throwError(
        tr("%1 : %2").arg(errStr).arg(imgArg.toString()));
  else
    return context()->thisObject();
}

}  // namespace TScriptBinding
