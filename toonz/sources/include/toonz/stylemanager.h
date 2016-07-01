#pragma once

#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include "tfilepath.h"
#include "tthread.h"

#include <QSize>
#include <QList>
#include <QString>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------------

//  Forward declarations
class QImage;

//-------------------------------------------------------------------------

//********************************************************************************
//    CustomStyleManager declaration
//********************************************************************************

class DVAPI CustomStyleManager final : public QObject {
  Q_OBJECT

public:
  struct DVAPI PatternData {
    QImage *m_image;
    std::string m_patternName;
    bool m_isVector;

    PatternData() : m_image(0), m_patternName(""), m_isVector(false) {}
  };

  class StyleLoaderTask;
  friend class CustomStyleManager::StyleLoaderTask;

private:
  QList<PatternData> m_patterns;
  TFilePath m_rootPath;
  TFilePath m_stylesFolder;
  QString m_filters;
  QSize m_chipSize;

  TThread::Executor m_executor;
  bool m_started;

public:
  CustomStyleManager(const TFilePath &stylesFolder, QString filters = QString(),
                     QSize chipSize = QSize(30, 30));

  const TFilePath &stylesFolder() const { return m_stylesFolder; }
  QSize getChipSize() const { return m_chipSize; }

  int getPatternCount();
  PatternData getPattern(int index);

  static TFilePath getRootPath();
  static void setRootPath(const TFilePath &rootPath);

  void loadItems();

private:
  void addPattern(const TFilePath &path);

signals:

  void patternAdded();
};

#endif  // STYLEMANAGER_H
