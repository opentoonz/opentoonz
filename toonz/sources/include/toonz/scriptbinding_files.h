#pragma once

#ifndef SCRIPTBINDING_FILES_H
#define SCRIPTBINDING_FILES_H

#include "toonz/scriptbinding.h"
#include "tfilepath.h"

#include <QDateTime>

namespace TScriptBinding {

class DVAPI FilePath final : public Wrapper {
  Q_OBJECT
  QString m_filePath;

public:
  FilePath(const QString &filePath = "");
  FilePath(const TFilePath &filePath);
  ~FilePath();

  WRAPPER_STD_METHODS(FilePath)
  Q_INVOKABLE QScriptValue toString() const;

  Q_PROPERTY(QString extension READ getExtension WRITE setExtension)
  QString getExtension() const;
  QScriptValue setExtension(const QString &extension);

  Q_PROPERTY(QString name READ getName WRITE setName)
  QString getName() const;
  void setName(const QString &name);

  Q_PROPERTY(QScriptValue parentDirectory READ getParentDirectory WRITE
                 setParentDirectory)
  QScriptValue getParentDirectory() const;
  void setParentDirectory(const QScriptValue &name);

  Q_PROPERTY(bool exists READ exists)
  bool exists() const;

  Q_PROPERTY(QDateTime lastModified READ lastModified)
  QDateTime lastModified() const;

  Q_PROPERTY(bool isDirectory READ isDirectory)
  bool isDirectory() const;

  Q_INVOKABLE QScriptValue withExtension(const QString &extension);
  Q_INVOKABLE QScriptValue withName(const QString &extension);
  Q_INVOKABLE QScriptValue
  withParentDirectory(const QScriptValue &parentDirectory);

  TFilePath getToonzFilePath() const;

  Q_INVOKABLE QScriptValue concat(const QScriptValue &value) const;

  // return a list of FilePath contained in the folder (assuming this FilePath
  // is a folder)
  Q_INVOKABLE QScriptValue files() const;
};

// helper functions

// convert a string or a FilePath object into a TFilePath
// if no conversion is possible it returns an error object, else it returns
// QScriptValue() and assign the conversion result to fp
QScriptValue checkFilePath(QScriptContext *context, const QScriptValue &value,
                           TFilePath &fp);

}  // namespace TScriptBinding

Q_DECLARE_METATYPE(TScriptBinding::FilePath *)

#endif
