#pragma once

#ifndef TCOMPOSERRENDERER_INCLUDED
#define TCOMPOSERRENDERER_INCLUDED

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace TComposerRenderer {

inline QString executable(const QString &selectedPath) {
  if (!selectedPath.isEmpty()) return selectedPath;
#ifdef _WIN32
  const QString name = QStringLiteral("tcomposer.exe");
#else
  const QString name = QStringLiteral("tcomposer");
#endif
  return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

inline bool isExecutable(const QString &path) {
  const QFileInfo file(path);
  return file.isAbsolute() && file.isFile() && file.isExecutable();
}

inline QProcessEnvironment environment(const QString &path) {
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  for (const char *key : {"QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH",
                          "QML_IMPORT_PATH", "QML2_IMPORT_PATH"})
    env.remove(QString::fromLatin1(key));
  env.insert("PATH", QDir::toNativeSeparators(QFileInfo(path).absolutePath()) +
                         QDir::listSeparator() + env.value("PATH"));
  return env;
}

}  // namespace TComposerRenderer

#endif
