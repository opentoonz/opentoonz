

#include "toonz/scriptbinding_files.h"
#include <QScriptEngine>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include "tsystem.h"

namespace TScriptBinding {

//===========================================================================

FilePath::FilePath(const QString &filePath) : m_filePath(filePath) {}

FilePath::FilePath(const TFilePath &filePath)
    : m_filePath(QString::fromStdWString(filePath.getWideString())) {}

FilePath::~FilePath() {}

QScriptValue FilePath::ctor(QScriptContext *context, QScriptEngine *engine) {
  FilePath *file = new FilePath();
  if (context->argumentCount() == 1) {
    file->m_filePath = context->argument(0).toString();
  }
  return file->create(engine, file);
}

QScriptValue FilePath::toString() const { return tr("\"%1\"").arg(m_filePath); }

QString FilePath::getExtension() const {
  return QString::fromStdString(getToonzFilePath().getType());
}

QScriptValue FilePath::setExtension(const QString &extension) {
  TFilePath fp = getToonzFilePath().withType(extension.toStdString());
  m_filePath   = QString::fromStdWString(fp.getWideString());
  return context()->thisObject();
}

QString FilePath::getName() const {
  return QString::fromStdString(getToonzFilePath().getName());
}

void FilePath::setName(const QString &name) {
  TFilePath fp = getToonzFilePath().withName(name.toStdString());
  m_filePath   = QString::fromStdWString(fp.getWideString());
}

QScriptValue FilePath::getParentDirectory() const {
  FilePath *result = new FilePath(getToonzFilePath().getParentDir());
  return create(engine(), result);
}

void FilePath::setParentDirectory(const QScriptValue &folder) {
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), folder, fp);
  if (!err.isError()) {
    m_filePath = QString::fromStdWString(
        getToonzFilePath().withParentDir(fp).getWideString());
  }
}

QScriptValue FilePath::withExtension(const QString &extension) {
  TFilePath fp = getToonzFilePath().withType(extension.toStdString());
  return create(engine(), new FilePath(fp));
}

QScriptValue FilePath::withName(const QString &extension) {
  TFilePath fp = getToonzFilePath().withName(extension.toStdString());
  return create(engine(), new FilePath(fp));
}

QScriptValue FilePath::withParentDirectory(
    const QScriptValue &parentDirectoryArg) {
  TFilePath parentDirectory;
  QScriptValue err =
      checkFilePath(context(), parentDirectoryArg, parentDirectory);
  if (err.isError())
    return err;
  else
    return create(
        engine(),
        new FilePath(getToonzFilePath().withParentDir(parentDirectory)));
}

bool FilePath::exists() const { return QFile(m_filePath).exists(); }

QDateTime FilePath::lastModified() const {
  return QFileInfo(m_filePath).lastModified();
}

TFilePath FilePath::getToonzFilePath() const {
  return TFilePath(m_filePath.toStdWString());
}

bool FilePath::isDirectory() const { return QFileInfo(m_filePath).isDir(); }

QScriptValue FilePath::concat(const QScriptValue &value) const {
  TFilePath fp;
  QScriptValue err;
  err = checkFilePath(context(), value, fp);
  if (err.isError()) return err;

  if (fp.isAbsolute())
    return context()->throwError(
        tr("can't concatenate an absolute path : %1").arg(value.toString()));
  fp = getToonzFilePath() + fp;
  return create(engine(), new FilePath(fp));
}

QScriptValue FilePath::files() const {
  if (!isDirectory()) {
    return context()->throwError(
        tr("%1 is not a directory").arg(toString().toString()));
  }
  TFilePathSet fpset;
  try {
    TSystem::readDirectory(fpset, getToonzFilePath());
    QScriptValue result = engine()->newArray();
    quint32 index       = 0;
    for (TFilePathSet::iterator it = fpset.begin(); it != fpset.end(); ++it) {
      FilePath *res = new FilePath(*it);
      result.setProperty(index++, res->create<FilePath>(engine()));
    }
    return result;
  } catch (...) {
    return context()->throwError(
        tr("can't read directory %1").arg(toString().toString()));
  }
}

QScriptValue checkFilePath(QScriptContext *context, const QScriptValue &value,
                           TFilePath &fp) {
  FilePath *filePath = qscriptvalue_cast<FilePath *>(value);
  if (filePath) {
    fp = filePath->getToonzFilePath();
  } else if (value.isString()) {
    fp = TFilePath(value.toString().toStdWString());
  } else {
    return context->throwError(
        QObject::tr("Argument doesn't look like a file path : %1")
            .arg(value.toString()));
  }
  return QScriptValue();
}

}  // namespace TScriptBinding
