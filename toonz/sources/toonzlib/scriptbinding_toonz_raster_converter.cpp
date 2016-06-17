

#include "toonz/scriptbinding_toonz_raster_converter.h"
#include "toonz/scriptbinding_level.h"
#include "tropcm.h"
#include "convert2tlv.h"

namespace TScriptBinding {

ToonzRasterConverter::ToonzRasterConverter() : m_flatSource(false) {}

ToonzRasterConverter::~ToonzRasterConverter() {}

QScriptValue ToonzRasterConverter::ctor(QScriptContext *context,
                                        QScriptEngine *engine) {
  return engine->newQObject(new ToonzRasterConverter(),
                            QScriptEngine::AutoOwnership);
}

QScriptValue ToonzRasterConverter::toString() { return "ToonzRasterConverter"; }

QScriptValue ToonzRasterConverter::convert(QScriptContext *context,
                                           QScriptEngine *engine) {
  if (context->argumentCount() != 1)
    return context->throwError(
        "Expected one argument (a raster Level or a raster Image)");
  QScriptValue arg = context->argument(0);
  Level *level     = qscriptvalue_cast<Level *>(arg);
  Image *img       = qscriptvalue_cast<Image *>(arg);
  QString type;
  if (level) {
    type = level->getType();
    if (type != "Raster")
      return context->throwError(tr("Can't convert a %1 level").arg(type));
    if (level->getFrameCount() <= 0)
      return context->throwError(tr("Can't convert a level with no frames"));
  } else if (img) {
    type = img->getType();
    if (type != "Raster")
      return context->throwError(tr("Can't convert a %1 image").arg(type));
  } else {
    return context->throwError(
        tr("Bad argument (%1): should be a raster Level or a raster Image")
            .arg(arg.toString()));
  }
  RasterToToonzRasterConverter converter;
  if (img) {
    TRasterImageP ri = img->getImg();
    TToonzImageP ti  = converter.convert(ri);
    ti->setPalette(converter.getPalette().getPointer());
    return create(engine, new Image(ti));
  } else
    return QScriptValue();
}

}  // namespace TScriptBinding
