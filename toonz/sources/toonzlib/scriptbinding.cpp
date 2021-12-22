

#include "toonz/scriptbinding.h"
#include "toonz/scriptbinding_scene.h"
#include "toonz/scriptbinding_image.h"
#include "toonz/scriptbinding_level.h"
#include "toonz/scriptbinding_files.h"
#include "toonz/scriptbinding_renderer.h"
#include "toonz/scriptbinding_outline_vectorizer.h"
#include "toonz/scriptbinding_centerline_vectorizer.h"
#include "toonz/scriptbinding_rasterizer.h"
#include "toonz/scriptbinding_toonz_raster_converter.h"
#include "toonz/scriptbinding_image_builder.h"
#include <QScriptEngine>
#include "tlevel_io.h"
#include "tlevel.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/tcamera.h"
#include "trop.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "tpalette.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tgeometry.h"
#include "toonz/stage.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzscene.h"
#include "tfiletype.h"
#include <QRegExp>
#include <QColor>

namespace TScriptBinding {

//===========================================================================

WRAPPER_STD_CTOR_IMPL(Void)

//===========================================================================

int Wrapper::m_count = 0;

Wrapper::Wrapper() { m_id = ++m_count; }

Wrapper::~Wrapper() {}

void Wrapper::print(const QString &msg) {
  QScriptValueList lst;
  lst << msg;
  print(lst);
}

void Wrapper::print(const QScriptValueList &lst) {
  QScriptValue print = engine()->globalObject().property("print");
  print.call(print, lst);
}

void Wrapper::warning(const QString &msg) {
  QScriptValueList lst;
  lst << msg;
  QScriptValue f = engine()->globalObject().property("warning");
  f.call(f, lst);
}

//===========================================================================

QScriptValue checkArgumentCount(QScriptContext *context, const QString &name,
                                int minCount, int maxCount) {
  int count = context->argumentCount();
  if (minCount <= count && count <= maxCount) return QScriptValue();
  QString range;
  if (minCount != maxCount)
    range = QObject::tr("%1-%2").arg(minCount).arg(maxCount);
  else
    range = QObject::tr("%1").arg(minCount);
  return context->throwError(
      QObject::tr("Expected %1 argument(s) in %2, got %3")
          .arg(range)
          .arg(name)
          .arg(count));
}

QScriptValue checkArgumentCount(QScriptContext *context, const QString &name,
                                int count) {
  return checkArgumentCount(context, name, count, count);
}

QScriptValue checkColor(QScriptContext *context, const QString &colorName,
                        QColor &color) {
  color.setNamedColor(colorName);
  if (!color.isValid())
    return context->throwError(
        QObject::tr("%1 is not a valid color (valid color names are 'red', "
                    "'transparent', '#FF8800', ecc.)")
            .arg(colorName));
  else
    return QScriptValue();
}

//=============================================================================

template <class T>
void bindClass(QScriptEngine &engine, const QString &name) {
  const QMetaObject *metaObj = &T::staticMetaObject;
  QScriptValue ctor          = engine.newFunction(T::ctor);
  QScriptValue proto         = engine.newQMetaObject(metaObj, ctor);
  engine.globalObject().setProperty(name, proto);
  engine.setDefaultPrototype(qMetaTypeId<T *>(), proto);
}

template <typename Tp>
QScriptValue qScriptValueFromQObject(QScriptEngine *engine, Tp const &qobject) {
  return engine->newQObject(qobject);
}

template <typename Tp>
void qScriptValueToQObject(const QScriptValue &value, Tp &qobject) {
  qobject = qobject_cast<Tp>(value.toQObject());
}

template <typename Tp>
int qScriptRegisterQObjectMetaType(
    QScriptEngine *engine, const QScriptValue &prototype = QScriptValue()) {
  return qScriptRegisterMetaType<Tp>(engine, qScriptValueFromQObject,
                                     qScriptValueToQObject, prototype);
}

template <class T, QScriptValue (T::*Method)(QScriptContext *, QScriptEngine *)>
class Dummy {
public:
  static QScriptValue dummy(QScriptContext *context, QScriptEngine *engine) {
    T *obj = qscriptvalue_cast<T *>(context->thisObject());
    return (obj->*Method)(context, engine);
  }
};

static void deffoo(QScriptEngine &engine) {
  QScriptValue f = engine.newFunction(
      Dummy<ToonzRasterConverter, &ToonzRasterConverter::convert>::dummy);
  engine.globalObject()
      .property("ToonzRasterConverter")
      .setProperty("convert", f);
}

void bindAll(QScriptEngine &engine) {
  bindClass<Image>(engine, "Image");
  bindClass<Level>(engine, "Level");
  bindClass<Scene>(engine, "Scene");
  bindClass<Transform>(engine, "Transform");
  bindClass<ImageBuilder>(engine, "ImageBuilder");
  bindClass<OutlineVectorizer>(engine, "OutlineVectorizer");
  bindClass<CenterlineVectorizer>(engine, "CenterlineVectorizer");
  bindClass<Rasterizer>(engine, "Rasterizer");
  bindClass<ToonzRasterConverter>(engine, "ToonzRasterConverter");
  deffoo(engine);
  bindClass<FilePath>(engine, "FilePath");
  bindClass<Renderer>(engine, "Renderer");

  qScriptRegisterQObjectMetaType<TScriptBinding::Image *>(&engine);

  engine.evaluate("ToonzVersion='7.1'");
  // engine.evaluate("toonz={version:'7.0', }; script={version:'1.0'};");
  // engine.globalObject().setProperty("dir","C:\\Users\\gmt\\GMT to
  // vectorize\\");
  // QScriptValue test = engine.evaluate("function() { print('ok');
  // run(dir+'bu.js');}");
  // engine.globalObject().setProperty("test",test);
}

}  // namespace TScriptBinding
