#include "iwa_bokehreffx.h"

#include "trop.h"

#include <QReadWriteLock>
#include <QSet>
#include <QMap>
#include <math.h>

namespace {
QReadWriteLock lock;
QMutex fx_mutex;

template <typename T>
TRasterGR8P allocateRasterAndLock(T** buf, TDimensionI dim) {
  TRasterGR8P ras(dim.lx * sizeof(T), dim.ly);
  ras->lock();
  *buf = (T*)ras->getRawData();
  return ras;
}

// release all registered raster memories
void releaseAllRasters(QList<TRasterGR8P>& rasterList) {
  for (int r = 0; r < rasterList.size(); r++) rasterList.at(r)->unlock();
}

// release all registered raster memories and free all fft plans
void releaseAllRastersAndPlans(QList<TRasterGR8P>& rasterList,
                               QList<kiss_fftnd_cfg>& planList) {
  releaseAllRasters(rasterList);
  for (int p = 0; p < planList.size(); p++) kiss_fft_free(planList.at(p));
}
};  // namespace

//============================================================

//------------------------------------------------------------
// normalize brightness of the depth reference image to unsigned char
// and store into detMem
//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_BokehRefFx::setDepthRaster(const RASTER srcRas, unsigned char* dstMem,
                                    TDimensionI dim) {
  unsigned char* depth_p = dstMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, depth_p++) {
      // normalize brightness to 0-1
      double val = ((double)pix->r * 0.3 + (double)pix->g * 0.59 +
                    (double)pix->b * 0.11) /
                   (double)PIXEL::maxChannelValue;
      // convert to unsigned char
      (*depth_p) = (unsigned char)(val * (double)UCHAR_MAX + 0.5);
    }
  }
}

template <typename RASTER, typename PIXEL>
void Iwa_BokehRefFx::setDepthRasterGray(const RASTER srcRas,
                                        unsigned char* dstMem,
                                        TDimensionI dim) {
  unsigned char* depth_p = dstMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, depth_p++) {
      // normalize brightness to 0-1
      double val = (double)pix->value / (double)PIXEL::maxChannelValue;
      // convert to unsigned char
      (*depth_p) = (unsigned char)(val * (double)UCHAR_MAX + 0.5);
    }
  }
}

//--------------------------------------------

Iwa_BokehRefFx::Iwa_BokehRefFx()
    : m_distancePrecision(10), m_fillGap(true), m_doMedian(true) {
  // Bind parameters
  addInputPort("Source", m_source);
  addInputPort("Depth", m_depth);

  bindParam(this, "on_focus_distance", m_onFocusDistance, false);
  bindParam(this, "bokeh_amount", m_bokehAmount, false);
  bindParam(this, "hardness", m_hardness, false);
  bindParam(this, "gamma", m_gamma, false);
  bindParam(this, "gammaAdjust", m_gammaAdjust, false);
  bindParam(this, "distance_precision", m_distancePrecision, false);
  bindParam(this, "fill_gap", m_fillGap, false);
  bindParam(this, "fill_gap_with_median_filter", m_doMedian, false);
  bindParam(this, "linearizeMode", m_linearizeMode, false);

  m_distancePrecision->setValueRange(3, 128);

  enableComputeInFloat(true);

  // Version 1: Exposure is computed by using Hardness
  //            E = std::pow(10.0, (value - 0.5) / hardness)
  // Version 2: Exposure can also be computed by using Gamma, for easier
  // combination with the linear color space
  //            E = std::pow(value, gamma)
  // this must be called after binding the parameters (see onFxVersionSet())
  // Version 3: Gamma is computed by rs.m_colorSpaceGamma + gammaAdjust
  setFxVersion(3);
}

//--------------------------------------------

void Iwa_BokehRefFx::onFxVersionSet() {
  bool useGamma = getFxVersion() == 2;
  if (getFxVersion() == 1) {
    m_linearizeMode->setValue(Hardness);
    setFxVersion(3);
  } else if (getFxVersion() == 2) {
    // Automatically update version
    if (m_linearizeMode->getValue() == Hardness ||
        (m_gamma->getKeyframeCount() == 0 &&
         areAlmostEqual(m_gamma->getDefaultValue(), 2.2))) {
      useGamma = false;
      setFxVersion(3);
    }
  }
  getParams()->getParamVar("gamma")->setIsHidden(!useGamma);
  getParams()->getParamVar("gammaAdjust")->setIsHidden(useGamma);
}

//--------------------------------------------

void Iwa_BokehRefFx::doCompute(TTile& tile, double frame,
                               const TRenderSettings& settings) {
  // If any of input is not connected, then do nothing
  if (!m_iris.isConnected() || !m_source.isConnected() ||
      !m_depth.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  QList<TRasterGR8P> rasterList;

  // Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
  double bokehPixelAmount = BokehUtils::getBokehPixelAmount(
      m_bokehAmount->getValue(frame), settings.m_affine);

  // Obtain the larger size of bokeh between the nearest (black) point and
  // the farthest (white) point, based on the focus distance.
  double onFocusDistance = m_onFocusDistance->getValue(frame);
  double maxIrisSize =
      bokehPixelAmount * std::max((1.0 - onFocusDistance), onFocusDistance);

  int margin =
      (maxIrisSize > 1.0f) ? (int)(std::ceil((maxIrisSize - 1.0) / 2.0)) : 0;

  // Range of computation
  TRectD rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                         tile.getRaster()->getLy()));
  rectOut = rectOut.enlarge(static_cast<double>(margin));

  TDimensionI dimOut(static_cast<int>(rectOut.getLx() + 0.5),
                     static_cast<int>(rectOut.getLy() + 0.5));

  // Enlarge the size to the "fast size" for kissfft which has no factors other
  // than 2,3, or 5.
  if (dimOut.lx < 10000 && dimOut.ly < 10000) {
    int new_x = kiss_fft_next_fast_size(dimOut.lx);
    int new_y = kiss_fft_next_fast_size(dimOut.ly);
    // margin should be integer
    while ((new_x - dimOut.lx) % 2 != 0)
      new_x = kiss_fft_next_fast_size(new_x + 1);
    while ((new_y - dimOut.ly) % 2 != 0)
      new_y = kiss_fft_next_fast_size(new_y + 1);

    rectOut = rectOut.enlarge(static_cast<double>(new_x - dimOut.lx) / 2.0,
                              static_cast<double>(new_y - dimOut.ly) / 2.0);

    dimOut.lx = new_x;
    dimOut.ly = new_y;
  }

  // - - - Compute the input tiles - - -

  // source image buffer
  // double4* source_buff;
  // rasterList.append(allocateRasterAndLock<double4>(&source_buff, dimOut));

  LayerValue layerValue;
  ;
  // source tile is used only in this focus.
  // normalized source image data is stored in source_buff.
  layerValue.sourceTile = new TTile();
  m_source->allocateAndCompute(*layerValue.sourceTile, rectOut.getP00(), dimOut,
                               tile.getRaster(), frame, settings);

  // - - - iris image - - -
  // Get the original size of Iris image
  TRectD irisBBox;
  m_iris->getBBox(frame, irisBBox, settings);
  // Compute the iris tile.
  TTile irisTile;
  m_iris->allocateAndCompute(
      irisTile, irisBBox.getP00(),
      TDimension(static_cast<int>(irisBBox.getLx() + 0.5),
                 static_cast<int>(irisBBox.getLy() + 0.5)),
      tile.getRaster(), frame, settings);

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRasters(rasterList);
    tile.getRaster()->clear();
    return;
  }

  double masterGamma;
  if (m_linearizeMode->getValue() == Hardness)
    masterGamma = m_hardness->getValue(frame);
  else {  // Gamma
    if (getFxVersion() == 2)
      masterGamma = m_gamma->getValue(frame);
    else
      masterGamma = std::max(
          1., settings.m_colorSpaceGamma + m_gammaAdjust->getValue(frame));
    if (tile.getRaster()->isLinear()) masterGamma /= settings.m_colorSpaceGamma;
  }

  // compute the reference image
  std::vector<TRasterGR8P> ctrl_rasters;  // to be stored in uchar
  QMap<int, unsigned char*>
      ctrls;  // container of [port number, reference image buffer in uchar]
  unsigned char* depth_buff;
  rasterList.append(allocateRasterAndLock<unsigned char>(&depth_buff, dimOut));
  {
    TTile depthTile;
    m_depth->allocateAndCompute(depthTile, rectOut.getP00(), dimOut,
                                tile.getRaster(), frame, settings);

    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      releaseAllRasters(rasterList);
      tile.getRaster()->clear();
      return;
    }

    // normalize brightness of the depth reference image to unsigned char
    // and store into depth_buff
    TRasterGR8P rasGR8   = (TRasterGR8P)depthTile.getRaster();
    TRasterGR16P rasGR16 = (TRasterGR16P)depthTile.getRaster();
    TRaster32P ras32     = (TRaster32P)depthTile.getRaster();
    TRaster64P ras64     = (TRaster64P)depthTile.getRaster();
    TRasterFP rasF       = (TRasterFP)depthTile.getRaster();
    lock.lockForRead();
    if (rasGR8)
      setDepthRasterGray<TRasterGR8P, TPixelGR8>(rasGR8, depth_buff, dimOut);
    else if (rasGR16)
      setDepthRasterGray<TRasterGR16P, TPixelGR16>(rasGR16, depth_buff, dimOut);
    else if (ras32)
      BokehUtils::setDepthRaster<TRaster32P, TPixel32>(ras32, depth_buff,
                                                       dimOut);
    else if (ras64)
      BokehUtils::setDepthRaster<TRaster64P, TPixel64>(ras64, depth_buff,
                                                       dimOut);
    else if (rasF)
      BokehUtils::setDepthRaster<TRasterFP, TPixelF>(rasF, depth_buff, dimOut);
    lock.unlock();
  }
  ctrls[1] = depth_buff;

  layerValue.premultiply       = false;
  layerValue.layerGamma        = masterGamma;
  layerValue.depth_ref         = 1;
  layerValue.distance          = 0.5;
  layerValue.bokehAdjustment   = 1.0;
  layerValue.depthRange        = 1.0;
  layerValue.distancePrecision = m_distancePrecision->getValue();
  layerValue.fillGap           = m_fillGap->getValue();
  layerValue.doMedian          = m_doMedian->getValue();
  QList<LayerValue> layerValues;
  layerValues.append(layerValue);

  Iwa_BokehCommonFx::doFx(tile, frame, settings, bokehPixelAmount, margin,
                          dimOut, irisBBox, irisTile, layerValues, ctrls);

  releaseAllRasters(rasterList);
  delete layerValue.sourceTile;
}

FX_PLUGIN_IDENTIFIER(Iwa_BokehRefFx, "iwa_BokehRefFx")