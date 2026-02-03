#include "iwa_bokeh_advancedfx.h"

#include "trop.h"
#include "trasterfx.h"
#include "trasterimage.h"

#include <QReadWriteLock>
#include <QMutexLocker>
#include <QVector>
#include <QMap>
#include <QSet>

namespace {
QReadWriteLock lock;
QMutex fx_mutex;
bool isFurtherLayer(const QPair<int, double> val1,
                    const QPair<int, double> val2) {
  // if the layers are at the same depth, then put the layer with smaller index
  // above
  if (val1.second == val2.second) return val1.first > val2.first;
  return val1.second > val2.second;
}

template <typename T>
TRasterGR8P allocateRasterAndLock(T** buf, TDimensionI dim) {
  TRasterGR8P ras(dim.lx * sizeof(T), dim.ly);
  ras->lock();
  *buf = (T*)ras->getRawData();
  return ras;
}

// release all registered raster memories and free all fft plans
void releaseAllRastersAndPlans(QList<TRasterGR8P>& rasterList,
                               QList<kiss_fftnd_cfg>& planList) {
  for (int r = 0; r < rasterList.size(); r++) rasterList.at(r)->unlock();
  for (int p = 0; p < planList.size(); p++) kiss_fft_free(planList.at(p));
}

};  // namespace

//--------------------------------------------
// sort source images
QList<int> Iwa_BokehAdvancedFx::getSortedSourceIndices(double frame) {
  QList<QPair<int, double>> usedSourceList;

  // gather connected sources
  for (int i = 0; i < LAYER_NUM; i++) {
    if (m_layerParams[i].m_source.isConnected())
      usedSourceList.push_back(
          QPair<int, double>(i, m_layerParams[i].m_distance->getValue(frame)));
  }

  if (usedSourceList.empty()) return QList<int>();

  // sort by distance
  std::sort(usedSourceList.begin(), usedSourceList.end(), isFurtherLayer);

  QList<int> indicesList;
  for (int i = 0; i < usedSourceList.size(); i++) {
    indicesList.push_back(usedSourceList.at(i).first);
  }

  return indicesList;
}

//--------------------------------------------
// Compute the bokeh size for each layer. The source tile will be enlarged by
// the largest size of them.
QMap<int, double> Iwa_BokehAdvancedFx::getIrisSizes(
    const double frame, const QList<int> sourceIndices,
    const double bokehPixelAmount, double& maxIrisSize) {
  // create a list of available reference image ports
  QList<int> availableCtrPorts;
  for (int i = 0; i < getInputPortCount(); ++i) {
    QString portName = QString::fromStdString(getInputPortName(i));
    if (portName.startsWith("Depth") && getInputPort(i)->isConnected())
      availableCtrPorts.push_back(portName.remove(0, 5).toInt());
  }

  double focus = m_onFocusDistance->getValue(frame);
  double max   = 0.0;
  QMap<int, double> irisSizes;
  for (int s = 0; s < sourceIndices.size(); s++) {
    int index = sourceIndices.at(s);

    double layer  = m_layerParams[index].m_distance->getValue(frame);
    double adjust = m_layerParams[index].m_bokehAdjustment->getValue(frame);

    double irisSize;
    // In case there is no available reference image
    if (m_layerParams[index].m_depth_ref->getValue() == 0 ||
        !availableCtrPorts.contains(
            m_layerParams[index].m_depth_ref->getValue())) {
      irisSize = (focus - layer) * bokehPixelAmount * adjust;
    }
    // in case using the reference image
    else {
      double refRangeHalf =
          m_layerParams[index].m_depthRange->getValue(frame) * 0.5;
      double nearToFocus = focus - layer + refRangeHalf;
      double farToFocus  = focus - layer - refRangeHalf;
      // take further point from the focus distance
      if (std::abs(nearToFocus) > std::abs(farToFocus))
        irisSize = nearToFocus * bokehPixelAmount * adjust;
      else
        irisSize = farToFocus * bokehPixelAmount * adjust;
    }

    irisSizes[index] = irisSize;

    // update the maximum
    if (max < std::abs(irisSize)) max = std::abs(irisSize);
  }
  maxIrisSize = max;

  return irisSizes;
}

//-----------------------------------------------------
// return true if the control image is available and used
bool Iwa_BokehAdvancedFx::portIsUsed(int portIndex) {
  for (int layer = 0; layer < LAYER_NUM; layer++) {
    if (m_layerParams[layer].m_source.isConnected() &&
        m_layerParams[layer].m_depth_ref->getValue() == portIndex)
      return true;
  }
  return false;
}

//--------------------------------------------
Iwa_BokehAdvancedFx::Iwa_BokehAdvancedFx()
    : m_hardnessPerSource(false), m_control("Depth") {
  // Bind common parameters
  bindParam(this, "on_focus_distance", m_onFocusDistance, false);
  bindParam(this, "bokeh_amount", m_bokehAmount, false);
  bindParam(this, "masterHardness", m_hardness, false);
  bindParam(this, "masterGamma", m_gamma, false);
  bindParam(this, "masterGammaAdjust", m_gammaAdjust, false);
  bindParam(this, "hardnessPerSource", m_hardnessPerSource, false);
  bindParam(this, "linearizeMode", m_linearizeMode, false);

  // Bind layer parameters
  for (int layer = 0; layer < LAYER_NUM; layer++) {
    m_layerParams[layer].m_distance        = TDoubleParamP(0.5);
    m_layerParams[layer].m_bokehAdjustment = TDoubleParamP(1);

    m_layerParams[layer].m_hardness    = TDoubleParamP(0.3);
    m_layerParams[layer].m_gamma       = TDoubleParamP(2.2);
    m_layerParams[layer].m_gammaAdjust = TDoubleParamP(0.);
    m_layerParams[layer].m_depth_ref   = TIntParamP(0);
    m_layerParams[layer].m_depthRange  = TDoubleParamP(1.0);
    m_layerParams[layer].m_fillGap     = TBoolParamP(true);
    m_layerParams[layer].m_doMedian    = TBoolParamP(false);

    std::string str = QString("Source%1").arg(layer + 1).toStdString();
    addInputPort(str, m_layerParams[layer].m_source);
    bindParam(this, QString("distance%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_distance);
    bindParam(this, QString("bokeh_adjustment%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_bokehAdjustment);

    bindParam(this, QString("gamma%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_gamma);
    bindParam(this, QString("gammaAdjust%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_gammaAdjust);
    bindParam(this, QString("hardness%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_hardness);
    bindParam(this, QString("depth_ref%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_depth_ref);
    bindParam(this, QString("depthRange%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_depthRange);
    bindParam(this, QString("fillGap%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_fillGap);
    bindParam(this, QString("doMedian%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_doMedian);

    m_layerParams[layer].m_distance->setValueRange(0.0, 1.0);
    m_layerParams[layer].m_bokehAdjustment->setValueRange(0.0, 2.0);

    m_layerParams[layer].m_hardness->setValueRange(0.05, 3.0);
    m_layerParams[layer].m_gamma->setValueRange(1.0, 10.0);
    m_layerParams[layer].m_gammaAdjust->setValueRange(-5., 5.);
    m_layerParams[layer].m_depthRange->setValueRange(0.0, 1.0);
  }

  addInputPort("Depth1", new TRasterFxPort, 0);

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

void Iwa_BokehAdvancedFx::onFxVersionSet() {
  bool useGamma = getFxVersion() == 2;
  if (getFxVersion() == 1) {
    m_linearizeMode->setValue(Hardness);
    setFxVersion(3);
  } else if (getFxVersion() == 2) {
    if (m_linearizeMode->getValue() == Hardness) {
      useGamma = false;
      setFxVersion(3);
    }
  }
  getParams()->getParamVar("masterGamma")->setIsHidden(!useGamma);
  getParams()->getParamVar("masterGammaAdjust")->setIsHidden(useGamma);
  for (int layer = 0; layer < LAYER_NUM; layer++) {
    getParams()
        ->getParamVar(QString("gamma%1").arg(layer + 1).toStdString())
        ->setIsHidden(!useGamma);
    getParams()
        ->getParamVar(QString("gammaAdjust%1").arg(layer + 1).toStdString())
        ->setIsHidden(useGamma);
  }
}

//--------------------------------------------
void Iwa_BokehAdvancedFx::doCompute(TTile& tile, double frame,
                                    const TRenderSettings& settings) {
  // If the iris is not connected, then do nothing
  if (!m_iris.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  // If none of the source ports is connected, then do nothing
  bool sourceIsConnected = false;
  for (int i = 0; i < LAYER_NUM; i++) {
    if (m_layerParams[i].m_source.isConnected()) {
      sourceIsConnected = true;
      break;
    }
  }
  if (!sourceIsConnected) {
    tile.getRaster()->clear();
    return;
  }

  // Sort source layers by distance
  QList<int> sourceIndices = getSortedSourceIndices(frame);

  // Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
  double bokehPixelAmount = BokehUtils::getBokehPixelAmount(
      m_bokehAmount->getValue(frame), settings.m_affine);

  // Compute the bokeh size for each layer. The source tile will be enlarged by
  // the largest size of them.
  double maxIrisSize;
  QMap<int, double> irisSizes =
      getIrisSizes(frame, sourceIndices, bokehPixelAmount, maxIrisSize);

  int margin = (int)std::ceil(maxIrisSize * 0.5);

  TRectD _rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                          tile.getRaster()->getLy()));
  _rectOut = _rectOut.enlarge(static_cast<double>(margin));

  TDimensionI dimOut(static_cast<int>(_rectOut.getLx() + 0.5),
                     static_cast<int>(_rectOut.getLy() + 0.5));

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

    _rectOut = _rectOut.enlarge(static_cast<double>(new_x - dimOut.lx) / 2.0,
                                static_cast<double>(new_y - dimOut.ly) / 2.0);

    dimOut.lx = new_x;
    dimOut.ly = new_y;
  }

  // compute the input tiles
  QMap<int, TTile*> sourceTiles;
  for (auto index : sourceIndices) {
    TTile* layerTile = new TTile();
    m_layerParams[index].m_source->allocateAndCompute(
        *layerTile, _rectOut.getP00(), dimOut, tile.getRaster(), frame,
        settings);
    sourceTiles[index] = layerTile;
  }

  // obtain pixel size of original iris image
  TRectD irisBBox;
  m_iris->getBBox(frame, irisBBox, settings);
  // compute the iris tile
  TTile irisTile;
  m_iris->allocateAndCompute(
      irisTile, irisBBox.getP00(),
      TDimension(static_cast<int>(irisBBox.getLx() + 0.5),
                 static_cast<int>(irisBBox.getLy() + 0.5)),
      tile.getRaster(), frame, settings);

  // compute the reference image
  std::vector<TRasterGR8P> ctrl_rasters;  // to be stored in uchar
  QMap<int, unsigned char*>
      ctrls;  // container of [port number, reference image buffer in uchar]
  for (int i = 0; i < getInputPortCount(); ++i) {
    QString portName = QString::fromStdString(getInputPortName(i));
    if (portName.startsWith("Depth") && getInputPort(i)->isConnected()) {
      int portIndex = portName.remove(0, 5).toInt();
      if (portIsUsed(portIndex)) {
        TTile tmpTile;
        TRasterFxPort* tmpCtrl = dynamic_cast<TRasterFxPort*>(getInputPort(i));
        (*tmpCtrl)->allocateAndCompute(tmpTile, _rectOut.getP00(), dimOut,
                                       tile.getRaster(), frame, settings);
        TRasterGR8P ctrlRas(dimOut.lx, dimOut.ly);
        ctrlRas->lock();
        unsigned char* ctrl_mem = (unsigned char*)ctrlRas->getRawData();
        TRaster32P ras32        = (TRaster32P)tmpTile.getRaster();
        TRaster64P ras64        = (TRaster64P)tmpTile.getRaster();
        TRasterFP rasF          = (TRasterFP)tmpTile.getRaster();
        lock.lockForRead();
        if (ras32)
          BokehUtils::setDepthRaster<TRaster32P, TPixel32>(ras32, ctrl_mem,
                                                           dimOut);
        else if (ras64)
          BokehUtils::setDepthRaster<TRaster64P, TPixel64>(ras64, ctrl_mem,
                                                           dimOut);
        else if (rasF)
          BokehUtils::setDepthRaster<TRasterFP, TPixelF>(rasF, ctrl_mem,
                                                         dimOut);
        lock.unlock();
        ctrl_rasters.push_back(ctrlRas);
        ctrls[portIndex] = ctrl_mem;
      }
    }
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

  QList<LayerValue> layerValues;
  for (auto index : sourceIndices) {
    LayerValue layerValue;
    layerValue.sourceTile = sourceTiles[index];
    layerValue.premultiply =
        false;  // assuming input images are always premultiplied

    if (m_hardnessPerSource->getValue()) {
      if (m_linearizeMode->getValue() == Hardness)
        layerValue.layerGamma =
            m_layerParams[index].m_hardness->getValue(frame);
      else {  // Gamma
        if (getFxVersion() == 2)
          layerValue.layerGamma = m_layerParams[index].m_gamma->getValue(frame);
        else
          layerValue.layerGamma = std::max(
              1., settings.m_colorSpaceGamma +
                      m_layerParams[index].m_gammaAdjust->getValue(frame));
        if (tile.getRaster()->isLinear())
          layerValue.layerGamma /= settings.m_colorSpaceGamma;
      }
    } else
      layerValue.layerGamma = masterGamma;

    layerValue.depth_ref = m_layerParams[index].m_depth_ref->getValue();
    layerValue.irisSize  = irisSizes.value(index);
    layerValue.distance  = m_layerParams[index].m_distance->getValue(frame);
    layerValue.bokehAdjustment =
        m_layerParams[index].m_bokehAdjustment->getValue(frame);
    layerValue.depthRange = m_layerParams[index].m_depthRange->getValue(frame);
    layerValue.distancePrecision = 10;
    layerValue.fillGap           = m_layerParams[index].m_fillGap->getValue();
    layerValue.doMedian          = m_layerParams[index].m_doMedian->getValue();

    layerValues.append(layerValue);
  }

  Iwa_BokehCommonFx::doFx(tile, frame, settings, bokehPixelAmount, margin,
                          dimOut, irisBBox, irisTile, layerValues, ctrls);

  // release control image buffers
  for (int r = 0; r < ctrl_rasters.size(); r++) ctrl_rasters[r]->unlock();

  qDeleteAll(sourceTiles);
}

//--------------------------------------------
FX_PLUGIN_IDENTIFIER(Iwa_BokehAdvancedFx, "iwa_BokehAdvancedFx")