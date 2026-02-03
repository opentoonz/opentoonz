#pragma once

#ifndef TCACHERESOURCEPOOL_INCLUDED
#define TCACHERESOURCEPOOL_INCLUDED

#include "tcommon.h"
#include "tfilepath.h"

#include "tcacheresource.h"

#include <QString>
#include <QSettings>
#include <QMutex>

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//============================================================================

class THDCacheResourcePool;
class TCacheResource;
class TFilePath;

//============================================================================

class DVAPI TCacheResourcePool final : public QObject {
  Q_OBJECT

  THDCacheResourcePool *m_hdPool;
  TFilePath m_path;

  typedef std::map<std::string, TCacheResource *> MemResources;
  MemResources m_memResources;
  QMutex m_memMutex;

  unsigned int m_searchCount;
  bool m_foundIterator;
  MemResources::iterator m_searchIterator;

  TCacheResourcePool();
  ~TCacheResourcePool();

public:
  // General functions

  static TCacheResourcePool *instance();

  void setPath(QString cacheRoot, QString projectName, QString sceneName);
  const TFilePath &getPath() const;
  void reset();

  void beginCachedSearch();
  void endCachedSearch();

public:
  // Pool management functions

  void flushResources();

  unsigned int getCurrentSize() const;
  unsigned int getSizeAccessedAfterDays(int days) const;

  void clear();
  void clear(QString cacheRoot, QString projectName, QString sceneName);

  void clearKeyword(const std::string &keyword);

  void addReference(TCacheResourceP resource, QString flag);
  void releaseReference(TCacheResourceP resource, QString flag);
  void releaseReferences(QString flag);

  void clearFxResources(std::wstring fxId);

  void clearAccessedUpToSize(int MB);
  void clearAccessedAfterDays(int days);
  void clearAccessedAfterSessions(int sessionsCount);

  // Automatic management functions

  void setMaximumSize(int MB);
  int getMaximumSize() const;

  void setResourcesAccessTimeOut(int days);
  int getResourcesAccessTimeOut() const;

  // void setAutomaticCleanupInterval(int days, int minutes);

private:
  // Resources interaction functions

  friend class TCacheResource;
  friend class TCacheResourceP;

  TCacheResource *getResource(const std::string &name, bool createIfNone);
  void releaseResource(TCacheResource *resource);

  void invalidateAll();

private:
  // HD Pool functions

  bool isHDActive();
  void startBacking(TCacheResource *resource);
  void saveResourceInfos(TCacheResource *resource);
  void touchBackingPath(TCacheResource *resource);
  QString getPoolRoot(QString cacheRoot, QString projectName,
                      QString sceneName);
  void clearResource(QString path);
};

#endif  // TCACHERESOURCEPOOL_INCLUDED
