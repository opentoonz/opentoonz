#pragma once

#ifndef TW_SPLASH_INCLUDED
#define TW_SPLASH_INCLUDED

//#include "tfilepath.h"
//#include "tthread.h"
#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFilePath;

class DVAPI TSplashWindow {
protected:
  class TSplashWindowImp;
  TSplashWindowImp *m_imp;
  // TThread::Executor  m_thrExecutor;
  // TRasterP					 m_raster;

protected:
  TSplashWindow(TRaster32P splash_image, unsigned int timeout_msec);
  TSplashWindow(const TFilePath &splash_image, unsigned int timeout_msec);

public:
  virtual ~TSplashWindow();
  void close();

  static TSplashWindow *create(TRaster32P splash_image,
                               unsigned int timeout_msec);
  static TSplashWindow *create(const TFilePath &splash_image,
                               unsigned int timeout_msec);

protected:
  void createWindow(TRaster32P splash_image, unsigned int timeout_msec);
  void createWindow(const TFilePath &splash_image, unsigned int timeout_msec);

private:
  // cloning is forbidden
  TSplashWindow(const TSplashWindow &);
  TSplashWindow &operator=(const TSplashWindow &);
};

#endif
