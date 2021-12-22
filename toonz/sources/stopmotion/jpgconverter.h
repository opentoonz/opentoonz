#pragma once

#ifndef JPGCONVERTER_H
#define JPGCONVERTER_H

//#include "opencv2/opencv.hpp"
#include "turbojpeg.h"

#ifdef WITH_CANON
// Canon Includes
#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"
#endif

// Toonz Includes
#include "traster.h"
#include "tfilepath.h"

#include <QObject>
#include <QThread>

class QCamera;
class QCameraInfo;

//=============================================================================
// JpgConverter
//-----------------------------------------------------------------------------

class JpgConverter : public QThread {
  Q_OBJECT
#ifdef WITH_CANON
  EdsStreamRef m_stream;
#endif
  TRaster32P m_finalImage;
  bool m_scale     = false;
  int m_scaleWidth = 0;

public:
  JpgConverter();
  ~JpgConverter();
  static void saveJpg(TRaster32P, TFilePath path);
  static bool loadJpg(TFilePath path, TRaster32P& image);
#ifdef WITH_CANON
  void setStream(EdsStreamRef stream);
  void setScale(bool scale) { m_scale = scale; }
  void setScaleWidth(bool scaleWidth) { m_scaleWidth = scaleWidth; }
  TRaster32P getImage() { return m_finalImage; }
  void convertFromJpg();

protected:
  void run() override;
#endif

signals:
  void imageReady(bool);
};

//#endif

#endif  // JPGCONVERTER_H