#include "iwa_bokehfx.h"

#include "trop.h"
#include "tdoubleparam.h"
#include "trasterfx.h"
#include "trasterimage.h"

#include "kiss_fft.h"

#include <QPair>
#include <QVector>
#include <QReadWriteLock>
#include <QMutexLocker>
#include <QMap>

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
// Iwa_BokehFx
//--------------------------------------------

Iwa_BokehFx::Iwa_BokehFx() {
  // Bind the common parameters
  bindParam(this, "on_focus_distance", m_onFocusDistance, false);
  bindParam(this, "bokeh_amount", m_bokehAmount, false);
  bindParam(this, "hardness", m_hardness, false);
  bindParam(this, "gamma", m_gamma, false);
  bindParam(this, "gammaAdjust", m_gammaAdjust, false);
  bindParam(this, "linearizeMode", m_linearizeMode, false);

  // Bind the layer parameters
  for (int layer = 0; layer < LAYER_NUM; layer++) {
    m_layerParams[layer].m_distance        = TDoubleParamP(0.5);
    m_layerParams[layer].m_bokehAdjustment = TDoubleParamP(1.0);
    // The premultiply option is not displayed anymore for simplicity
    m_layerParams[layer].m_premultiply = TBoolParamP(false);

    std::string str = QString("Source%1").arg(layer + 1).toStdString();
    addInputPort(str, m_layerParams[layer].m_source);
    bindParam(this, QString("distance%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_distance, false);
    bindParam(this, QString("bokeh_adjustment%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_bokehAdjustment, false);
    bindParam(this, QString("premultiply%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_premultiply, false);

    m_layerParams[layer].m_distance->setValueRange(0.0, 1.0);
    m_layerParams[layer].m_bokehAdjustment->setValueRange(0.0, 2.0);
  }

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

void Iwa_BokehFx::onFxVersionSet() {
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

void Iwa_BokehFx::doCompute(TTile& tile, double frame,
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

  int margin = tceil(maxIrisSize / 2.0);

  // Range of computation
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

  //----------------------------
  // Compute the input tiles first
  QMap<int, TTile*> sourceTiles;
  for (auto index : sourceIndices) {
    TTile* layerTile = new TTile();
    m_layerParams[index].m_source->allocateAndCompute(
        *layerTile, _rectOut.getP00(), dimOut, tile.getRaster(), frame,
        settings);
    sourceTiles[index] = layerTile;
  }

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

  QMap<int, unsigned char*> ctrls;

  QList<LayerValue> layerValues;
  for (auto index : sourceIndices) {
    LayerValue layerValue;
    layerValue.sourceTile  = sourceTiles[index];
    layerValue.premultiply = m_layerParams[index].m_premultiply->getValue();
    layerValue.layerGamma  = masterGamma;
    layerValue.depth_ref   = 0;
    layerValue.irisSize    = irisSizes.value(index);
    layerValue.distance    = m_layerParams[index].m_distance->getValue(frame);
    layerValue.bokehAdjustment =
        m_layerParams[index].m_bokehAdjustment->getValue(frame);
    layerValues.append(layerValue);
  }

  Iwa_BokehCommonFx::doFx(tile, frame, settings, bokehPixelAmount, margin,
                          dimOut, irisBBox, irisTile, layerValues, ctrls);
  qDeleteAll(sourceTiles);
}

// Sort the layers by distances
QList<int> Iwa_BokehFx::getSortedSourceIndices(double frame) {
  QList<QPair<int, double>> usedSourceList;

  // Gather the source layers connected to the ports
  for (int i = 0; i < LAYER_NUM; i++) {
    if (m_layerParams[i].m_source.isConnected())
      usedSourceList.push_back(
          QPair<int, double>(i, m_layerParams[i].m_distance->getValue(frame)));
  }

  if (usedSourceList.empty()) return QList<int>();

  // Sort the layers in descending distance order
  std::sort(usedSourceList.begin(), usedSourceList.end(), isFurtherLayer);

  QList<int> indicesList;
  for (int i = 0; i < usedSourceList.size(); i++) {
    indicesList.push_back(usedSourceList.at(i).first);
  }

  return indicesList;
}

// Compute the bokeh size for each layer. The source tile will be enlarged by
// the largest size of them.
QMap<int, double> Iwa_BokehFx::getIrisSizes(const double frame,
                                            const QList<int> sourceIndices,
                                            const double bokehPixelAmount,
                                            double& maxIrisSize) {
  double max = 0.0;
  QMap<int, double> irisSizes;
  for (int s = 0; s < sourceIndices.size(); s++) {
    int index       = sourceIndices.at(s);
    double irisSize = (m_onFocusDistance->getValue(frame) -
                       m_layerParams[index].m_distance->getValue(frame)) *
                      bokehPixelAmount *
                      m_layerParams[index].m_bokehAdjustment->getValue(frame);
    irisSizes[index] = irisSize;

    // Update the maximum size
    if (max < std::abs(irisSize)) max = std::abs(irisSize);
  }
  maxIrisSize = max;

  return irisSizes;
}

FX_PLUGIN_IDENTIFIER(Iwa_BokehFx, "iwa_BokehFx")