#pragma once

#ifndef TCLEANUPPER_H
#define TCLEANUPPER_H

#include "trasterimage.h"
#include "ttoonzimage.h"
#include "toonz/cleanupparameters.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-----------------------------------------------------------------------------

//  Forward declarations

class CleanupPreprocessedImage;
class TargetColors;
class TCamera;

//-----------------------------------------------------------------------------

//****************************************************************************************
//    CleanupProcessedImage declaration
//****************************************************************************************

class DVAPI CleanupPreprocessedImage {
  std::string m_imgId;
  TDimension m_size;

public:
  bool m_wasFromGR8;
  bool m_autocentered;
  std::vector<TPixel32> m_pixelsLut;
  TAffine m_appliedAff;

public:
  CleanupPreprocessedImage(CleanupParameters *parameters,
                           TToonzImageP processed, bool fromGr8);
  ~CleanupPreprocessedImage();

  TDimension getSize() const { return m_size; }
  TToonzImageP getImg() const;
  TRasterImageP getPreviewImage() const;
};

//****************************************************************************************
//    TCleanupper singleton declaration
//****************************************************************************************

class DVAPI TCleanupper {
  CleanupParameters *m_parameters;
  TPointD m_sourceDpi;

private:
  TCleanupper()
      : m_parameters(0) {
  }  // singleton class - will not be externally constructed

public:
  static TCleanupper *instance();

  void setParameters(CleanupParameters *parameters);
  const CleanupParameters *getParameters() const { return m_parameters; }

  TPalette *createToonzPaletteFromCleanupPalette();

  /*---
   * 拡大／縮小をしていない場合（DPIが変わらない場合）、NearestNeighborでリサンプリングする。
   * ---*/
  bool getResampleValues(const TRasterImageP &image, TAffine &aff, double &blur,
                         TDimension &outDim, TPointD &outDpi, bool isCameraTest,
                         bool &isSameDpi);

  TRasterP processColors(const TRasterP &r);

  /*!
The main cleanup method.
\warning The input image reference is internally released at the most
appropriate
time, to unlock a possibly useful memory block.
*/
  CleanupPreprocessedImage *process(TRasterImageP &image, bool first_image,
                                    TRasterImageP &onlyResampledImage,
                                    bool isCameraTest    = false,
                                    bool returnResampled = false,
                                    bool onlyForSwatch   = false,
                                    TAffine *aff         = 0);

  void finalize(const TRaster32P &dst, CleanupPreprocessedImage *src);
  TToonzImageP finalize(CleanupPreprocessedImage *src,
                        bool isCleanupper = false);

  TRasterImageP autocenterOnly(const TRasterImageP &image, bool isCameraTest,
                               bool &autocentered);

  TPointD getSourceDpi() const { return m_sourceDpi; }
  void setSourceDpi(const TPointD &dpi) { m_sourceDpi = dpi; }

private:
  // process phase
  bool doAutocenter(double &angle, double &skew, double &cxin, double &cyin,
                    double &cqout, double &cpout, const double xdpi,
                    const double ydpi, const int raster_is_savebox,
                    const TRect saveBox, const TRasterImageP &image,
                    const double scalex);
  void preprocessColors(const TRasterCM32P &outRas, const TRaster32P &raster32,
                        const TargetColors &colors);
  void removeSinglePoint(const TRasterCM32P &outRas);

  // post-processing phase
  TToonzImageP doPostProcessingGR8(const CleanupPreprocessedImage *img);
  void doPostProcessingGR8(const TRaster32P &outRas,
                           CleanupPreprocessedImage *srcImg);

  TToonzImageP doPostProcessingColor(const TToonzImageP &imgToProcess,
                                     bool isCleanupper);
  void doPostProcessingColor(const TRaster32P &outRas,
                             CleanupPreprocessedImage *srcImg);
};

#endif
