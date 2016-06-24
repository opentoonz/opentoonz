#pragma once

#ifndef SCRIPTBINDING_H
#define SCRIPTBINDING_H

#include <QObject>
#include <QScriptable>
#include <QScriptValue>
#include <QScriptEngine>
#include <QMetaType>
#include <QColor>

#include "timage.h"
#include "toonz/vectorizerparameters.h"
#include "toonz/txshsimplelevel.h"
#include "tlevel.h"
#include "tgeometry.h"

#include "tcommon.h"
#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#define WRAPPER_STD_METHODS(T)                                                 \
  static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);    \
  static QScriptValue toScriptValue(QScriptEngine *engine, T *const &in) {     \
    return engine->newQObject(in);                                             \
  }                                                                            \
  static void fromScriptValue(const QScriptValue &object, T *&out) {           \
    out = qobject_cast<T *>(object.toQObject());                               \
  }

#define WRAPPER_STD_CTOR_IMPL(T)                                               \
  QScriptValue T::ctor(QScriptContext *context, QScriptEngine *engine) {       \
    return engine->newQObject(new T(), QScriptEngine::AutoOwnership);          \
  }

class TAffine;
class TLevelReader;
class ToonzScene;
class TFilePath;

namespace TScriptBinding {

class DVAPI Void final : public QObject {
  Q_OBJECT
public:
  WRAPPER_STD_METHODS(Void)
};

class DVAPI Wrapper : public QObject, protected QScriptable {
  Q_OBJECT

  static int m_count;
  int m_id;

public:
  Wrapper();
  virtual ~Wrapper();

  Q_PROPERTY(int id READ getId())
  int getId() const { return m_id; }

  void print(const QString &msg);
  void print(const QScriptValueList &lst);

  void warning(const QString &msg);

  template <class T>
  QScriptValue create(QScriptEngine *engine) {
    return create(engine, static_cast<T *>(this));
  }

  template <class T>
  static QScriptValue create(QScriptEngine *engine, T *obj) {
    return engine->newQObject(obj, QScriptEngine::AutoOwnership,
                              QScriptEngine::ExcludeSuperClassContents |
                                  QScriptEngine::ExcludeChildObjects);
  }

protected:
  template <class T>
  QScriptValue create(T *obj) {
    return create(engine(), obj);
  }
};

void bindAll(QScriptEngine &engine);

// helper functions

// check the number of arguments: if it is out of range returns an error object
QScriptValue checkArgumentCount(QScriptContext *context, const QString &name,
                                int minCount, int maxCount);
QScriptValue checkArgumentCount(QScriptContext *context, const QString &name,
                                int count);

// check the color. if colorName is not valid then an error is returned
QScriptValue checkColor(QScriptContext *context, const QString &colorName,
                        QColor &color);

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::Void *)

#endif
