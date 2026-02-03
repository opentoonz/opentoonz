#pragma once

#ifndef SCRIPTBINDING_SCENE_H
#define SCRIPTBINDING_SCENE_H

#include "toonz/scriptbinding.h"

namespace TScriptBinding {

class DVAPI Scene final : public Wrapper {
  Q_OBJECT
  ToonzScene *m_scene;

public:
  Scene();
  ~Scene();

  WRAPPER_STD_METHODS(Scene)
  Q_INVOKABLE QScriptValue toString();

  Q_PROPERTY(int frameCount READ getFrameCount)
  Q_PROPERTY(int columnCount READ getColumnCount)
  int getFrameCount();
  int getColumnCount();

  Q_INVOKABLE QScriptValue load(const QScriptValue &fpArg);
  Q_INVOKABLE QScriptValue save(const QScriptValue &fpArg);

  QString doSetCell(int row, int col, const QScriptValue &level,
                    const QScriptValue &fid);
  Q_INVOKABLE QScriptValue setCell(int row, int col, const QScriptValue &level,
                                   const QScriptValue &fid);
  Q_INVOKABLE QScriptValue setCell(int row, int col, const QScriptValue &cell);
  Q_INVOKABLE QScriptValue getCell(int row, int col);

  Q_INVOKABLE QScriptValue insertColumn(int col);
  Q_INVOKABLE QScriptValue deleteColumn(int col);

  Q_INVOKABLE QScriptValue getLevels() const;
  Q_INVOKABLE QScriptValue getLevel(const QString &name) const;
  Q_INVOKABLE QScriptValue newLevel(const QString &type,
                                    const QString &name) const;
  Q_INVOKABLE QScriptValue loadLevel(const QString &name,
                                     const QScriptValue &path) const;

  ToonzScene *getToonzScene() const { return m_scene; }
};

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::Scene *)

#endif
