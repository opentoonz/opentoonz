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
QMutex mutex, fx_mutex;

bool isFurtherLayer(const QPair<int, float> val1,
                    const QPair<int, float> val2) {
  return val1.second > val2.second;
}

// FFT coordinate -> Normal corrdinate
inline int getCoord(int i, int j, int lx, int ly) {
  int cx = i - lx / 2;
  int cy = j - ly / 2;

  if (cx < 0) cx += lx;
  if (cy < 0) cy += ly;

  return cy * lx + cx;
}

// RGB value <--> Exposure
inline float valueToExposure(float value, float filmGamma) {
  float logVal = (value - 0.5) / filmGamma;
  return pow(10, logVal);
}
inline float exposureToValue(float exposure, float filmGamma) {
  return log10(exposure) * filmGamma + 0.5;
}
};

//--------------------------------------------
// Threads used for FFT computation for each RGB channel
//--------------------------------------------

MyThread::MyThread(Channel channel, TRasterP layerTileRas, TRasterP outTileRas,
                   TRasterP tmpAlphaRas, kiss_fft_cpx* kissfft_comp_iris,
                   float filmGamma,
                   bool doLightenComp)  // not used for now
    : m_channel(channel),
      m_layerTileRas(layerTileRas),
      m_outTileRas(outTileRas),
      m_tmpAlphaRas(tmpAlphaRas),
      m_kissfft_comp_iris(kissfft_comp_iris),
      m_filmGamma(filmGamma),
      m_finished(false),
      m_kissfft_comp_in(0),
      m_kissfft_comp_out(0),
      m_isTerminated(false),
      m_doLightenComp(doLightenComp)  // not used for now
{}

bool MyThread::init() {
  // get the source size
  int lx, ly;
  lx = m_layerTileRas->getSize().lx;
  ly = m_layerTileRas->getSize().ly;

  // memory allocation for input
  m_kissfft_comp_in_ras = TRasterGR8P(lx * sizeof(kiss_fft_cpx), ly);
  m_kissfft_comp_in_ras->lock();
  m_kissfft_comp_in = (kiss_fft_cpx*)m_kissfft_comp_in_ras->getRawData();

  // allocation check
  if (m_kissfft_comp_in == 0) return false;

  // cancel check
  if (m_isTerminated) {
    m_kissfft_comp_in_ras->unlock();
    return false;
  }

  // memory allocation for output
  m_kissfft_comp_out_ras = TRasterGR8P(lx * sizeof(kiss_fft_cpx), ly);
  m_kissfft_comp_out_ras->lock();
  m_kissfft_comp_out = (kiss_fft_cpx*)m_kissfft_comp_out_ras->getRawData();

  // allocation check
  if (m_kissfft_comp_out == 0) {
    m_kissfft_comp_in_ras->unlock();
    m_kissfft_comp_in = 0;
    return false;
  }

  // cancel check
  if (m_isTerminated) {
    m_kissfft_comp_in_ras->unlock();
    m_kissfft_comp_in = 0;
    m_kissfft_comp_out_ras->unlock();
    m_kissfft_comp_out = 0;
    return false;
  }

  // create the forward FFT plan
  int dims[2]        = {ly, lx};
  int ndims          = 2;
  m_kissfft_plan_fwd = kiss_fftnd_alloc(dims, ndims, false, 0, 0);
  // allocation and cancel check
  if (m_kissfft_plan_fwd == NULL || m_isTerminated) {
    m_kissfft_comp_in_ras->unlock();
    m_kissfft_comp_in = 0;
    m_kissfft_comp_out_ras->unlock();
    m_kissfft_comp_out = 0;
    return false;
  }

  // create the backward FFT plan
  m_kissfft_plan_bkwd = kiss_fftnd_alloc(dims, ndims, true, 0, 0);
  // allocation and cancel check
  if (m_kissfft_plan_bkwd == NULL || m_isTerminated) {
    m_kissfft_comp_in_ras->unlock();
    m_kissfft_comp_in = 0;
    m_kissfft_comp_out_ras->unlock();
    m_kissfft_comp_out = 0;
    kiss_fft_free(m_kissfft_plan_fwd);
    m_kissfft_plan_fwd = NULL;
    return false;
  }

  // return true if all the initializations are done
  return true;
}

//------------------------------------------------------------
// Convert the pixels from RGB values to exposures and multiply it by alpha
// channel value.
// Store the results in the real part of kiss_fft_cpx.
//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void MyThread::setLayerRaster(const RASTER srcRas, kiss_fft_cpx* dstMem,
                              TDimensionI dim) {
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++) {
      if (pix->m != 0) {
        float val = (m_channel == Red)
                        ? (float)pix->r
                        : (m_channel == Green) ? (float)pix->g : (float)pix->b;
        // multiply the exposure by alpha channel value
        dstMem[j * dim.lx + i].r =
            valueToExposure(val / (float)PIXEL::maxChannelValue) *
            ((float)pix->m / (float)PIXEL::maxChannelValue);
      }
    }
  }
}

//------------------------------------------------------------
// Composite the bokeh layer to the result
//------------------------------------------------------------
template <typename RASTER, typename PIXEL, typename A_RASTER, typename A_PIXEL>
void MyThread::compositLayerToTile(const RASTER layerRas,
                                   const RASTER outTileRas,
                                   const A_RASTER alphaRas, TDimensionI dim,
                                   int2 margin) {
  int j = margin.y;
  for (int out_j = 0; out_j < outTileRas->getLy(); j++, out_j++) {
    PIXEL* outPix     = outTileRas->pixels(out_j);
    A_PIXEL* alphaPix = alphaRas->pixels(j);

    alphaPix += margin.x;

    int i = margin.x;
    for (int out_i = 0; out_i < outTileRas->getLx(); i++, out_i++) {
      // If the layer pixel is transparent, keep the result pizel as-is.
      float alpha = (float)alphaPix->value / (float)PIXEL::maxChannelValue;
      if (alpha == 0.0f) {
        alphaPix++;
        outPix++;
        continue;
      }
      // Composite the upper layer exposure with the bottom layers. Then,
      // convert the exposure to RGB values.
      typename PIXEL::Channel dnVal =
          (m_channel == Red) ? outPix->r
                             : (m_channel == Green) ? outPix->g : outPix->b;

      float exposure;
      double val;
      if (alpha == 1.0 || dnVal == 0.0) {
        exposure = (m_kissfft_comp_in[getCoord(i, j, dim.lx, dim.ly)].r /
                    (dim.lx * dim.ly));
        val = exposureToValue(exposure) * (float)PIXEL::maxChannelValue + 0.5f;
      } else {
        exposure =
            (m_kissfft_comp_in[getCoord(i, j, dim.lx, dim.ly)].r /
             (dim.lx * dim.ly)) +
            valueToExposure((float)dnVal / (float)PIXEL::maxChannelValue) *
                (1 - alpha);
        val = exposureToValue(exposure) * (float)PIXEL::maxChannelValue + 0.5f;
        // not used for now
        if (m_doLightenComp) val = std::max(val, (double)dnVal);
      }

      // clamp
      if (val < 0.0)
        val = 0.0;
      else if (val > (float)PIXEL::maxChannelValue)
        val = (float)PIXEL::maxChannelValue;

      switch (m_channel) {
      case Red:
        outPix->r = (typename PIXEL::Channel)val;
        //"over" composite the alpha channel here
        if (outPix->m != A_PIXEL::maxChannelValue) {
          if (alphaPix->value == A_PIXEL::maxChannelValue)
            outPix->m = A_PIXEL::maxChannelValue;
          else
            outPix->m =
                alphaPix->value +
                (typename A_PIXEL::Channel)(
                    (float)outPix->m *
                    (float)(A_PIXEL::maxChannelValue - alphaPix->value) /
                    (float)A_PIXEL::maxChannelValue);
        }
        break;
      case Green:
        outPix->g = (typename PIXEL::Channel)val;
        break;
      case Blue:
        outPix->b = (typename PIXEL::Channel)val;
        break;
      }

      alphaPix++;
      outPix++;
    }
  }
}

//------------------------------------------------------------

void MyThread::run() {
  // get the source image size
  TDimensionI dim = m_layerTileRas->getSize();

  int2 margin = {(dim.lx - m_outTileRas->getSize().lx) / 2,
                 (dim.ly - m_outTileRas->getSize().ly) / 2};

  // initialize
  for (int i = 0; i < dim.lx * dim.ly; i++) {
    m_kissfft_comp_in[i].r = 0.0;  // real part
    m_kissfft_comp_in[i].i = 0.0;  // imaginary part
  }

  TRaster32P ras32 = (TRaster32P)m_layerTileRas;
  TRaster64P ras64 = (TRaster64P)m_layerTileRas;
  // Prepare data for FFT.
  // Convert the RGB values to the exposure, then multiply it by the alpha
  // channel value
  {
    lock.lockForRead();
    if (ras32)
      setLayerRaster<TRaster32P, TPixel32>(ras32, m_kissfft_comp_in, dim);
    else if (ras64)
      setLayerRaster<TRaster64P, TPixel64>(ras64, m_kissfft_comp_in, dim);
    else {
      lock.unlock();
      return;
    }

    lock.unlock();
  }

  if (checkTerminationAndCleanupThread()) return;

  kiss_fftnd(m_kissfft_plan_fwd, m_kissfft_comp_in, m_kissfft_comp_out);
  kiss_fft_free(m_kissfft_plan_fwd);  // we don't need this plan anymore
  m_kissfft_plan_fwd = NULL;

  if (checkTerminationAndCleanupThread()) return;

  // Filtering. Multiply by the iris FFT data
  {
    for (int i = 0; i < dim.lx * dim.ly; i++) {
      float re, im;
      re = m_kissfft_comp_out[i].r * m_kissfft_comp_iris[i].r -
           m_kissfft_comp_out[i].i * m_kissfft_comp_iris[i].i;
      im = m_kissfft_comp_out[i].r * m_kissfft_comp_iris[i].i +
           m_kissfft_comp_iris[i].r * m_kissfft_comp_out[i].i;
      m_kissfft_comp_out[i].r = re;
      m_kissfft_comp_out[i].i = im;
    }
  }

  if (checkTerminationAndCleanupThread()) return;

  kiss_fftnd(m_kissfft_plan_bkwd, m_kissfft_comp_out,
             m_kissfft_comp_in);       // Backward FFT
  kiss_fft_free(m_kissfft_plan_bkwd);  // we don't need this plan anymore
  m_kissfft_plan_bkwd = NULL;

  // In the backward FFT above, "m_kissfft_comp_out" is used as input and
  // "m_kissfft_comp_in" as output.
  // So we don't need "m_kissfft_comp_out" anymore.
  m_kissfft_comp_out_ras->unlock();
  m_kissfft_comp_out = 0;

  if (checkTerminationAndCleanupThread()) return;

  {
    QMutexLocker locker(&mutex);

    TRaster32P ras32 = (TRaster32P)m_layerTileRas;
    TRaster64P ras64 = (TRaster64P)m_layerTileRas;

    if (ras32) {
      compositLayerToTile<TRaster32P, TPixel32, TRasterGR8P, TPixelGR8>(
          ras32, (TRaster32P)m_outTileRas, (TRasterGR8P)m_tmpAlphaRas, dim,
          margin);
    } else if (ras64) {
      compositLayerToTile<TRaster64P, TPixel64, TRasterGR16P, TPixelGR16>(
          ras64, (TRaster64P)m_outTileRas, (TRasterGR16P)m_tmpAlphaRas, dim,
          margin);
    } else {
      lock.unlock();
      return;
    }
  }

  // Now we don't need "m_kissfft_comp_in" anymore.
  m_kissfft_comp_in_ras->unlock();
  m_kissfft_comp_in = 0;

  m_finished = true;
}

// RGB value <--> Exposure
inline float MyThread::valueToExposure(float value) {
  float logVal = (value - 0.5) / m_filmGamma;
  return pow(10, logVal);
}
inline float MyThread::exposureToValue(float exposure) {
  return log10(exposure) * m_filmGamma + 0.5;
}

// Release the raster memory and FFT plans on cancel rendering.
bool MyThread::checkTerminationAndCleanupThread() {
  if (!m_isTerminated) return false;

  if (m_kissfft_comp_in) m_kissfft_comp_in_ras->unlock();
  if (m_kissfft_comp_out) m_kissfft_comp_out_ras->unlock();
  if (m_kissfft_plan_fwd) kiss_fft_free(m_kissfft_plan_fwd);
  if (m_kissfft_plan_bkwd) kiss_fft_free(m_kissfft_plan_bkwd);

  m_finished = true;
  return true;
}

//--------------------------------------------
// Iwa_BokehFx
//--------------------------------------------

Iwa_BokehFx::Iwa_BokehFx()
    : m_onFocusDistance(0.5), m_bokehAmount(30.0), m_hardness(0.3) {
  // Bind the common parameters
  addInputPort("Iris", m_iris);
  bindParam(this, "on_focus_distance", m_onFocusDistance, false);
  bindParam(this, "bokeh_amount", m_bokehAmount, false);
  bindParam(this, "hardness", m_hardness, false);

  // Set the ranges of common parameters
  m_onFocusDistance->setValueRange(0, 1);
  m_bokehAmount->setValueRange(0, 300);
  m_bokehAmount->setMeasureName("fxLength");
  m_hardness->setValueRange(0.05, 3.0);

  // Bind the layer parameters
  for (int layer = 0; layer < LAYER_NUM; layer++) {
    m_layerParams[layer].m_premultiply     = TBoolParamP(false);
    m_layerParams[layer].m_distance        = TDoubleParamP(0.5);
    m_layerParams[layer].m_bokehAdjustment = TDoubleParamP(1);

    std::string str = QString("Source%1").arg(layer + 1).toStdString();
    addInputPort(str, m_layerParams[layer].m_source);
    bindParam(this, QString("premultiply%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_premultiply, false);
    bindParam(this, QString("distance%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_distance, false);
    bindParam(this, QString("bokeh_adjustment%1").arg(layer + 1).toStdString(),
              m_layerParams[layer].m_bokehAdjustment, false);

    m_layerParams[layer].m_distance->setValueRange(0, 1);
    m_layerParams[layer].m_bokehAdjustment->setValueRange(0, 2);
  }
}

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
  float bokehPixelAmount = getBokehPixelAmount(frame, settings.m_affine);

  // Compute the bokeh size for each layer. The source tile will be enlarged by
  // the largest size of them.
  float maxIrisSize;
  QVector<float> irisSizes =
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

    _rectOut = _rectOut.enlarge(static_cast<double>(new_x - dimOut.lx) / 2.0,
                                static_cast<double>(new_y - dimOut.ly) / 2.0);

    dimOut.lx = new_x;
    dimOut.ly = new_y;
  }

  //----------------------------
  // Compute the input tiles first
  QMap<int, TTile*> sourceTiles;
  for (int i = 0; i < sourceIndices.size(); i++) {
    int index      = sourceIndices.at(i);
    float irisSize = irisSizes.at(i);

    TTile* layerTile = new TTile();
    // Layer to be composited as-is
    if (-1.0 <= irisSize && 1.0 >= irisSize) {
      m_layerParams[index].m_source->allocateAndCompute(
          *layerTile, tile.m_pos,
          TDimension(tile.getRaster()->getLx(), tile.getRaster()->getLy()),
          tile.getRaster(), frame, settings);
    }
    // Layer to be off-focused
    else {
      m_layerParams[index].m_source->allocateAndCompute(
          *layerTile, _rectOut.getP00(), dimOut, tile.getRaster(), frame,
          settings);
    }
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

  // This fx is relatively heavy so the multi thread computation is introduced.
  // Lock the mutex here in order to prevent multiple rendering tasks run at the
  // same time.
  QMutexLocker fx_locker(&fx_mutex);

  kiss_fft_cpx* kissfft_comp_iris;
  // create the iris data for FFT (in the same size as the source tile)
  TRasterGR8P kissfft_comp_iris_ras(dimOut.lx * sizeof(kiss_fft_cpx),
                                    dimOut.ly);
  kissfft_comp_iris_ras->lock();
  kissfft_comp_iris = (kiss_fft_cpx*)kissfft_comp_iris_ras->getRawData();

  // obtain the film gamma
  double filmGamma = m_hardness->getValue(frame);

  // clear the raster memory
  tile.getRaster()->clear();
  TRaster32P raster32 = tile.getRaster();
  if (raster32)
    raster32->fill(TPixel32::Transparent);
  else {
    TRaster64P ras64 = tile.getRaster();
    if (ras64) ras64->fill(TPixel64::Transparent);
  }

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    kissfft_comp_iris_ras->unlock();
    tile.getRaster()->clear();
    return;
  }

  // Compute from from the most distant layer
  for (int i = 0; i < sourceIndices.size(); i++) {
    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      kissfft_comp_iris_ras->unlock();
      tile.getRaster()->clear();
      return;
    }

    int index = sourceIndices.at(i);

    // The iris size of the current layer
    float irisSize = irisSizes.at(i);

    // If the size of iris is less than 1 (i.e. the layer is at focal position),
    // composite the layer as-is.
    if (-1.0 <= irisSize && 1.0 >= irisSize) {
      //"Over" composite the layer to the output raster.
      TTile* layerTile = sourceTiles.value(index);
      compositLayerAsIs(tile, *layerTile, frame, settings, index);
      sourceTiles.remove(index);
      // Continue to the next layer
      continue;
    }

    {
      // Create the Iris image for FFT
      kiss_fft_cpx* kissfft_comp_iris_before;
      TRasterGR8P kissfft_comp_iris_before_ras(dimOut.lx * sizeof(kiss_fft_cpx),
                                               dimOut.ly);
      kissfft_comp_iris_before_ras->lock();
      kissfft_comp_iris_before =
          (kiss_fft_cpx*)kissfft_comp_iris_before_ras->getRawData();
      // Resize / flip the iris image according to the size ratio.
      // Normalize the brightness of the iris image.
      // Enlarge the iris to the output size.
      convertIris(irisSize, kissfft_comp_iris_before, dimOut, irisBBox,
                  irisTile);

      if (settings.m_isCanceled && *settings.m_isCanceled) {
        kissfft_comp_iris_ras->unlock();
        kissfft_comp_iris_before_ras->unlock();
        tile.getRaster()->clear();
        return;
      }

      // Create the FFT plan for the iris image.
      kiss_fftnd_cfg iris_kissfft_plan;
      while (1) {
        int dims[2]       = {dimOut.ly, dimOut.lx};
        int ndims         = 2;
        iris_kissfft_plan = kiss_fftnd_alloc(dims, ndims, false, 0, 0);
        if (iris_kissfft_plan != NULL) break;
      }
      // Do FFT the iris image.
      kiss_fftnd(iris_kissfft_plan, kissfft_comp_iris_before,
                 kissfft_comp_iris);
      kiss_fft_free(iris_kissfft_plan);
      kissfft_comp_iris_before_ras->unlock();
    }

    // Up to here, FFT-ed iris data is stored in kissfft_comp_iris

    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      kissfft_comp_iris_ras->unlock();
      tile.getRaster()->clear();
      return;
    }
    // Prepare the layer rasters
    TTile* layerTile = sourceTiles.value(index);
    // Unpremultiply the source if needed
    if (!m_layerParams[index].m_premultiply->getValue())
      TRop::depremultiply(layerTile->getRaster());
    // Create the raster memory for storing alpha channel
    TRasterP tmpAlphaRas;
    {
      TRaster32P ras32(tile.getRaster());
      TRaster64P ras64(tile.getRaster());
      if (ras32)
        tmpAlphaRas = TRasterGR8P(dimOut);
      else if (ras64)
        tmpAlphaRas = TRasterGR16P(dimOut);
    }
    tmpAlphaRas->lock();

    // Do FFT the alpha channel.
    // Forward FFT -> Multiply by the iris data -> Backward FFT
    calcAlfaChannelBokeh(kissfft_comp_iris, *layerTile, tmpAlphaRas);

    if (settings.m_isCanceled && *settings.m_isCanceled) {
      kissfft_comp_iris_ras->unlock();
      tile.getRaster()->clear();
      tmpAlphaRas->unlock();
      return;
    }

    // Create the threads for RGB channels
    MyThread threadR(MyThread::Red, layerTile->getRaster(), tile.getRaster(),
                     tmpAlphaRas, kissfft_comp_iris, filmGamma);
    MyThread threadG(MyThread::Green, layerTile->getRaster(), tile.getRaster(),
                     tmpAlphaRas, kissfft_comp_iris, filmGamma);
    MyThread threadB(MyThread::Blue, layerTile->getRaster(), tile.getRaster(),
                     tmpAlphaRas, kissfft_comp_iris, filmGamma);

    // If you set this flag to true, the fx will be forced to compute in single
    // thread.
    // Under some specific condition (such as calling from single-threaded
    // tcomposer)
    // we may need to use this flag... For now, I'll keep this option unused.
    // TODO: investigate this.
    bool renderInSingleThread = false;

    // Start the thread when the initialization is done.
    // Red channel
    int waitCount = 0;
    while (1) {
      // cancel & timeout check
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 20)  // 10 second timeout
      {
        kissfft_comp_iris_ras->unlock();
        tile.getRaster()->clear();
        tmpAlphaRas->unlock();
        return;
      }
      if (threadR.init()) {
        if (renderInSingleThread)
          threadR.run();
        else
          threadR.start();
        break;
      }
      QThread::msleep(500);
      waitCount++;
    }
    // Green channel
    waitCount = 0;
    while (1) {
      // cancel & timeout check
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 20)  // 10 second timeout
      {
        if (!threadR.isFinished()) threadR.terminateThread();
        while (!threadR.isFinished()) {
        }
        kissfft_comp_iris_ras->unlock();
        tile.getRaster()->clear();
        tmpAlphaRas->unlock();
        return;
      }
      if (threadG.init()) {
        if (renderInSingleThread)
          threadG.run();
        else
          threadG.start();
        break;
      }
      QThread::msleep(500);
      waitCount++;
    }
    // Blue channel
    waitCount = 0;
    while (1) {
      // cancel & timeout check
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 20)  // 10 second timeout
      {
        if (!threadR.isFinished()) threadR.terminateThread();
        if (!threadG.isFinished()) threadG.terminateThread();
        while (!threadR.isFinished() || !threadG.isFinished()) {
        }
        kissfft_comp_iris_ras->unlock();
        tile.getRaster()->clear();
        tmpAlphaRas->unlock();
        return;
      }
      if (threadB.init()) {
        if (renderInSingleThread)
          threadB.run();
        else
          threadB.start();
        break;
      }
      QThread::msleep(500);
      waitCount++;
    }

    /*
    * What is done in the thread for each RGB channel:
    * - Convert channel value -> Exposure
    * - Multiply by alpha channel
    * - Forward FFT
    * - Multiply by the iris FFT data
    * - Backward FFT
    * - Convert Exposure -> channel value
    */

    waitCount = 0;
    while (1) {
      // cancel & timeout check
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 2000)  // 100 second timeout
      {
        if (!threadR.isFinished()) threadR.terminateThread();
        if (!threadG.isFinished()) threadG.terminateThread();
        if (!threadB.isFinished()) threadB.terminateThread();
        while (!threadR.isFinished() || !threadG.isFinished() ||
               !threadB.isFinished()) {
        }
        kissfft_comp_iris_ras->unlock();
        tile.getRaster()->clear();
        tmpAlphaRas->unlock();
        return;
      }
      if (threadR.isFinished() && threadG.isFinished() && threadB.isFinished())
        break;
      QThread::msleep(50);
      waitCount++;
    }
    tmpAlphaRas->unlock();
    sourceTiles.remove(index);
  }

  kissfft_comp_iris_ras->unlock();
}

bool Iwa_BokehFx::doGetBBox(double frame, TRectD& bBox,
                            const TRenderSettings& info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

bool Iwa_BokehFx::canHandle(const TRenderSettings& info, double frame) {
  return false;
}

// Sort the layers by distances
QList<int> Iwa_BokehFx::getSortedSourceIndices(double frame) {
  QList<QPair<int, float>> usedSourceList;

  // Gather the source layers connected to the ports
  for (int i = 0; i < LAYER_NUM; i++) {
    if (m_layerParams[i].m_source.isConnected())
      usedSourceList.push_back(
          QPair<int, float>(i, m_layerParams[i].m_distance->getValue(frame)));
  }

  if (usedSourceList.empty()) return QList<int>();

  // Sort the layers in descending distance order
  qSort(usedSourceList.begin(), usedSourceList.end(), isFurtherLayer);

  QList<int> indicesList;
  for (int i = 0; i < usedSourceList.size(); i++) {
    indicesList.push_back(usedSourceList.at(i).first);
  }

  return indicesList;
}

// Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
float Iwa_BokehFx::getBokehPixelAmount(const double frame,
                                       const TAffine affine) {
  /*--- Convert to vector --- */
  TPointD vect;
  vect.x = m_bokehAmount->getValue(frame);
  vect.y = 0.0;
  /*--- Apply geometrical transformation ---*/
  // For the following lines I referred to lines 586-592 of
  // sources/stdfx/motionblurfx.cpp
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0; /* ignore translation */
  vect              = aff * vect;
  /*--- return the length of the vector ---*/
  return sqrt(vect.x * vect.x + vect.y * vect.y);
}

// Compute the bokeh size for each layer. The source tile will be enlarged by
// the largest size of them.
QVector<float> Iwa_BokehFx::getIrisSizes(const double frame,
                                         const QList<int> sourceIndices,
                                         const float bokehPixelAmount,
                                         float& maxIrisSize) {
  float max = 0.0;
  QVector<float> irisSizes;
  for (int s = 0; s < sourceIndices.size(); s++) {
    int index      = sourceIndices.at(s);
    float irisSize = (m_onFocusDistance->getValue(frame) -
                      m_layerParams[index].m_distance->getValue(frame)) *
                     bokehPixelAmount *
                     m_layerParams[index].m_bokehAdjustment->getValue(frame);
    irisSizes.push_back(irisSize);

    // Update the maximum size
    if (max < fabs(irisSize)) max = fabs(irisSize);
  }
  maxIrisSize = max;

  return irisSizes;
}

//"Over" composite the layer to the output raster.
void Iwa_BokehFx::compositLayerAsIs(TTile& tile, TTile& layerTile,
                                    const double frame,
                                    const TRenderSettings& settings,
                                    const int index) {
  // Premultiply the source if needed
  if (m_layerParams[index].m_premultiply->getValue())
    TRop::premultiply(layerTile.getRaster());

  TRop::over(tile.getRaster(), layerTile.getRaster());
}

//------------------------------------------------
// Resize / flip the iris image according to the size ratio.
// Normalize the brightness of the iris image.
// Enlarge the iris to the output size.
void Iwa_BokehFx::convertIris(const float irisSize,
                              kiss_fft_cpx* kissfft_comp_iris_before,
                              const TDimensionI& dimOut, const TRectD& irisBBox,
                              const TTile& irisTile) {
  // the original size of iris image
  double2 irisOrgSize = {irisBBox.getLx(), irisBBox.getLy()};

  // Get the size ratio of iris based on width. The ratio can be negative value.
  double irisSizeResampleRatio = irisSize / irisOrgSize.x;

  // Create the raster for resized iris
  double2 resizedIrisSize = {std::abs(irisSizeResampleRatio) * irisOrgSize.x,
                             std::abs(irisSizeResampleRatio) * irisOrgSize.y};
  int2 filterSize = {tceil(resizedIrisSize.x), tceil(resizedIrisSize.y)};
  TPointD resizeOffset((double)filterSize.x - resizedIrisSize.x,
                       (double)filterSize.y - resizedIrisSize.y);

  // Add some adjustment in order to absorb the difference of the cases when the
  // iris size is odd and even numbers.
  bool isIrisOffset[2] = {false, false};
  // Try to set the center of the iris to the center of the screen
  if ((dimOut.lx - filterSize.x) % 2 == 1) {
    filterSize.x++;
    isIrisOffset[0] = true;
  }
  if ((dimOut.ly - filterSize.y) % 2 == 1) {
    filterSize.y++;
    isIrisOffset[1] = true;
  }

  // Terminate if the filter size becomes bigger than the output size.
  if (filterSize.x > dimOut.lx || filterSize.y > dimOut.ly) {
    std::cout
        << "Error: The iris filter size becomes larger than the source size!"
        << std::endl;
    return;
  }

  TRaster64P resizedIris(TDimension(filterSize.x, filterSize.y));

  // Add some adjustment in order to absorb the 0.5 translation to be done in
  // resample()
  TAffine aff;
  TPointD affOffset((isIrisOffset[0]) ? 0.5 : 1.0,
                    (isIrisOffset[1]) ? 0.5 : 1.0);
  if (!isIrisOffset[0]) affOffset.x -= resizeOffset.x / 2;
  if (!isIrisOffset[1]) affOffset.y -= resizeOffset.y / 2;

  aff = TTranslation(resizedIris->getCenterD() + affOffset);
  aff *= TScale(irisSizeResampleRatio);
  aff *= TTranslation(-(irisTile.getRaster()->getCenterD() + affOffset));

  // resample the iris
  TRop::resample(resizedIris, irisTile.getRaster(), aff);

  // accumulated value
  float irisValAmount = 0.0;

  int iris_j = 0;
  // Initialize
  for (int i = 0; i < dimOut.lx * dimOut.ly; i++) {
    kissfft_comp_iris_before[i].r = 0.0;
    kissfft_comp_iris_before[i].i = 0.0;
  }
  for (int j = (dimOut.ly - filterSize.y) / 2; iris_j < filterSize.y;
       j++, iris_j++) {
    TPixel64* pix = resizedIris->pixels(iris_j);
    int iris_i    = 0;
    for (int i = (dimOut.lx - filterSize.x) / 2; iris_i < filterSize.x;
         i++, iris_i++) {
      // Value = 0.3R 0.59G 0.11B
      kissfft_comp_iris_before[j * dimOut.lx + i].r =
          ((float)pix->r * 0.3f + (float)pix->g * 0.59f +
           (float)pix->b * 0.11f) /
          (float)USHRT_MAX;
      irisValAmount += kissfft_comp_iris_before[j * dimOut.lx + i].r;
      pix++;
    }
  }

  // Normalize value
  for (int i = 0; i < dimOut.lx * dimOut.ly; i++) {
    kissfft_comp_iris_before[i].r /= irisValAmount;
  }
}

// Do FFT the alpha channel.
// Forward FFT -> Multiply by the iris data -> Backward FFT
void Iwa_BokehFx::calcAlfaChannelBokeh(kiss_fft_cpx* kissfft_comp_iris,
                                       TTile& layerTile, TRasterP tmpAlphaRas) {
  // Obtain the source size
  int lx, ly;
  lx = layerTile.getRaster()->getSize().lx;
  ly = layerTile.getRaster()->getSize().ly;

  // Allocate the FFT data
  kiss_fft_cpx *kissfft_comp_in, *kissfft_comp_out;

  TRasterGR8P kissfft_comp_in_ras(lx * sizeof(kiss_fft_cpx), ly);
  kissfft_comp_in_ras->lock();
  kissfft_comp_in = (kiss_fft_cpx*)kissfft_comp_in_ras->getRawData();
  TRasterGR8P kissfft_comp_out_ras(lx * sizeof(kiss_fft_cpx), ly);
  kissfft_comp_out_ras->lock();
  kissfft_comp_out = (kiss_fft_cpx*)kissfft_comp_out_ras->getRawData();

  // Initialize the FFT data
  for (int i = 0; i < lx * ly; i++) {
    kissfft_comp_in[i].r = 0.0;  // real part
    kissfft_comp_in[i].i = 0.0;  // imaginary part
  }

  TRaster32P ras32 = (TRaster32P)layerTile.getRaster();
  TRaster64P ras64 = (TRaster64P)layerTile.getRaster();
  if (ras32) {
    for (int j = 0; j < ly; j++) {
      TPixel32* pix = ras32->pixels(j);
      for (int i = 0; i < lx; i++) {
        kissfft_comp_in[j * lx + i].r = (float)pix->m / (float)UCHAR_MAX;
        pix++;
      }
    }
  } else if (ras64) {
    for (int j = 0; j < ly; j++) {
      TPixel64* pix = ras64->pixels(j);
      for (int i = 0; i < lx; i++) {
        kissfft_comp_in[j * lx + i].r = (float)pix->m / (float)USHRT_MAX;
        pix++;
      }
    }
  } else
    return;

  int dims[2]             = {ly, lx};
  int ndims               = 2;
  kiss_fftnd_cfg plan_fwd = kiss_fftnd_alloc(dims, ndims, false, 0, 0);
  kiss_fftnd(plan_fwd, kissfft_comp_in, kissfft_comp_out);
  kiss_fft_free(plan_fwd);  // we don't need this plan anymore

  // Filtering. Multiply by the iris FFT data
  for (int i = 0; i < lx * ly; i++) {
    float re, im;
    re = kissfft_comp_out[i].r * kissfft_comp_iris[i].r -
         kissfft_comp_out[i].i * kissfft_comp_iris[i].i;
    im = kissfft_comp_out[i].r * kissfft_comp_iris[i].i +
         kissfft_comp_iris[i].r * kissfft_comp_out[i].i;
    kissfft_comp_out[i].r = re;
    kissfft_comp_out[i].i = im;
  }

  kiss_fftnd_cfg plan_bkwd = kiss_fftnd_alloc(dims, ndims, true, 0, 0);
  kiss_fftnd(plan_bkwd, kissfft_comp_out, kissfft_comp_in);  // Backward FFT
  kiss_fft_free(plan_bkwd);  // we don't need this plan anymore

  // In the backward FFT above, "kissfft_comp_out" is used as input and
  // "kissfft_comp_in" as output.
  // So we don't need "kissfft_comp_out" anymore.
  kissfft_comp_out_ras->unlock();

  // Store the result into the alpha channel of layer tile
  if (ras32) {
    TRasterGR8P alphaRas8(tmpAlphaRas);
    for (int j = 0; j < ly; j++) {
      TPixelGR8* pix = alphaRas8->pixels(j);
      for (int i = 0; i < lx; i++) {
        float val =
            kissfft_comp_in[getCoord(i, j, lx, ly)].r / (lx * ly) * 256.0;
        if (val < 0.0)
          val = 0.0;
        else if (val > 255.0)
          val = 255.0;

        pix->value = (unsigned char)val;

        pix++;
      }
    }
  } else if (ras64) {
    TRasterGR16P alphaRas16(tmpAlphaRas);
    for (int j = 0; j < ly; j++) {
      TPixelGR16* pix = alphaRas16->pixels(j);
      for (int i = 0; i < lx; i++) {
        float val =
            kissfft_comp_in[getCoord(i, j, lx, ly)].r / (lx * ly) * 65536.0;
        if (val < 0.0)
          val = 0.0;
        else if (val > 65535.0)
          val = 65535.0;

        pix->value = (unsigned short)val;

        pix++;
      }
    }
  } else
    return;

  kissfft_comp_in_ras->unlock();
}

FX_PLUGIN_IDENTIFIER(Iwa_BokehFx, "iwa_BokehFx")