#pragma once

#ifndef SCRIPT_ENGINE_H
#define SCRIPT_ENGINE_H

#include <QObject>
#include <QString>

class TXshSimpleLevel;

namespace ScriptWrapper {

class Cell : public QObject {
  Q_OBJECT
public:
  Cell();
};

class Xsheet : public QObject {
  Q_OBJECT
public:
  Xsheet();
};

class Level : public QObject {
  Q_OBJECT
  QString m_name;

public:
  Level();
  Level(TXshSimpleLevel *);
  ~Level();

  QString getName() const { return m_name; }
  void setName(const QString &name) { m_name = name; }

  TXshSimpleLevel *getLevel() const;

  Q_PROPERTY(QString name READ getName WRITE setName)

private:
};

class Scene : public QObject {
  Q_OBJECT
public:
  Scene();
};

}  // namespace ScriptWrapper

#endif
