#pragma once

#ifndef SCRIPTBINDING_LEVEL_H
#define SCRIPTBINDING_LEVEL_H

#include "toonz/scriptbinding.h"
#include "toonz/scriptbinding_image.h"

class ToonzScene;
class TXshSimpleLevel;

namespace TScriptBinding {

class DVAPI Level final : public Wrapper {
  Q_OBJECT
  TXshSimpleLevel *m_sl;
  ToonzScene *m_scene;
  bool m_sceneOwner;
  int m_type;

public:
  Level();
  Level(TXshSimpleLevel *);
  ~Level();

  WRAPPER_STD_METHODS(Image)
  Q_INVOKABLE QScriptValue toString();

  // QScriptValue toScriptValue(QScriptEngine *engine) { return create(engine,
  // this); }
  // static QScriptValue toScriptValue(QScriptEngine *engine, TXshSimpleLevel
  // *sl) { return (new Level(sl))->toScriptValue(engine); }

  Q_PROPERTY(QString type READ getType)
  QString getType() const;

  Q_PROPERTY(int frameCount READ getFrameCount)
  int getFrameCount() const;

  Q_PROPERTY(QString name READ getName WRITE setName)
  QString getName() const;
  void setName(const QString &name);

  Q_PROPERTY(QScriptValue path READ getPath WRITE setPath)
  QScriptValue getPath() const;
  void setPath(const QScriptValue &name);

  Q_INVOKABLE QScriptValue getFrame(const QScriptValue &fid);
  Q_INVOKABLE QScriptValue getFrameByIndex(const QScriptValue &index);
  Q_INVOKABLE QScriptValue setFrame(const QScriptValue &fid,
                                    const QScriptValue &image);

  Q_INVOKABLE QScriptValue getFrameIds();

  Q_INVOKABLE QScriptValue load(const QScriptValue &fp);
  Q_INVOKABLE QScriptValue save(const QScriptValue &fp);

  void getFrameIds(QList<TFrameId> &fids);
  TImageP getImg(const TFrameId &fid);

  int setFrame(const TFrameId &fid, const TImageP &img);

  TXshSimpleLevel *getSimpleLevel() const { return m_sl; }

  static TFrameId getFid(const QScriptValue &arg, QString &err);
};

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::Level *)

#endif
