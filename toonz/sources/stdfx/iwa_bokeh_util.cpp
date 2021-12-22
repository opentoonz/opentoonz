
#include "iwa_bokeh_util.h"

#include "trop.h"
#include <array>

#include <QSet>
#include <QMap>
#include <QReadWriteLock>

namespace {
QReadWriteLock lock;
QMutex mutex;

// modify fft coordinate to normal
inline int getCoord(int index, int lx, int ly) {
  int i = index % lx;
  int j = index / lx;

  int cx = i - lx / 2;
  int cy = j - ly / 2;

  if (cx < 0) cx += lx;
  if (cy < 0) cy += ly;

  return cy * lx + cx;
}

// RGB value <--> Exposure
inline double valueToExposure(double value, double filmGamma) {
  double logVal = (value - 0.5) / filmGamma;
  return std::pow(10.0, logVal);
}
inline double exposureToValue(double exposure, double filmGamma) {
  return std::log10(exposure) * filmGamma + 0.5;
}

inline double clamp01(double val) {
  return (val < 0.0) ? 0.0 : ((val > 1.0) ? 1.0 : val);
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
}  // namespace

//--------------------------------------------
// thread for computing bokeh of each channel
//--------------------------------------------

BokehUtils::MyThread::MyThread(Channel channel, TRasterP layerTileRas,
                               double4* result, double* alpha_bokeh,
                               kiss_fft_cpx* kissfft_comp_iris,
                               double layerHardness, double masterHardness,
                               bool doLightenComp)
    : m_channel(channel)
    , m_layerTileRas(layerTileRas)
    , m_result(result)
    , m_alpha_bokeh(alpha_bokeh)
    , m_kissfft_comp_iris(kissfft_comp_iris)
    , m_layerHardness(layerHardness)
    , m_masterHardness(masterHardness)
    , m_finished(false)
    , m_kissfft_comp_in(0)
    , m_kissfft_comp_out(0)
    , m_isTerminated(false)
    , m_doLightenComp(doLightenComp) {
  if (m_masterHardness == 0.0) m_masterHardness = m_layerHardness;
}

bool BokehUtils::MyThread::init() {
  // obtain the input image size
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
// convert layer RGB to exposure
// multiply alpha
// set to kiss_fft_cpx

template <typename RASTER, typename PIXEL>
void BokehUtils::MyThread::setLayerRaster(const RASTER srcRas,
                                          kiss_fft_cpx* dstMem,
                                          TDimensionI dim) {
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++) {
      if (pix->m != 0) {
        double val = (m_channel == Red) ? (double)pix->r
                                        : (m_channel == Green) ? (double)pix->g
                                                               : (double)pix->b;
        // multiply the exposure by alpha channel value
        dstMem[j * dim.lx + i].r =
            valueToExposure(val / (double)PIXEL::maxChannelValue,
                            m_layerHardness) *
            ((double)pix->m / (double)PIXEL::maxChannelValue);
      }
    }
  }
}

//------------------------------------------------------------

void BokehUtils::MyThread::run() {
  // get the source image size
  TDimensionI dim = m_layerTileRas->getSize();
  // int lx,ly;
  int lx = m_layerTileRas->getSize().lx;
  int ly = m_layerTileRas->getSize().ly;

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
    for (int i = 0; i < lx * ly; i++) {
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

    double* alp_p  = m_alpha_bokeh;
    double4* res_p = m_result;
    for (int i = 0; i < dim.lx * dim.ly; i++, alp_p++, res_p++) {
      if ((*alp_p) < 0.00001) continue;

      double exposure =
          (double)m_kissfft_comp_in[getCoord(i, dim.lx, dim.ly)].r /
          (double)(dim.lx * dim.ly);

      // convert to layer hardness
      if (m_masterHardness != m_layerHardness)
        exposure =
            std::pow(exposure / (*alp_p), m_layerHardness / m_masterHardness) *
            (*alp_p);

      double* res = (m_channel == Red) ? (&((*res_p).x))
                                       : (m_channel == Green) ? (&((*res_p).y))
                                                              : (&((*res_p).z));

      // composite exposure
      if ((*alp_p) >= 1.0 || (*res) == 0.0)
        (*res) = exposure;
      else {
        (*res) *= 1.0 - (*alp_p);
        (*res) += exposure;
      }

      // over compoite alpha
      if (m_channel == Red) {
        if ((*res_p).w < 1.0) {
          if ((*alp_p) > 1.0)
            (*res_p).w = 1.0;
          else
            (*res_p).w = (*alp_p) + (*res_p).w * (1.0 - (*alp_p));
        }
      }
      //---
    }
  }

  m_kissfft_comp_in_ras->unlock();
  m_kissfft_comp_in = 0;

  m_finished = true;
}

bool BokehUtils::MyThread::checkTerminationAndCleanupThread() {
  if (!m_isTerminated) return false;

  if (m_kissfft_comp_in) m_kissfft_comp_in_ras->unlock();
  if (m_kissfft_comp_out) m_kissfft_comp_out_ras->unlock();

  if (m_kissfft_plan_fwd) kiss_fft_free(m_kissfft_plan_fwd);
  if (m_kissfft_plan_bkwd) kiss_fft_free(m_kissfft_plan_bkwd);

  m_finished = true;
  return true;
}

//------------------------------------
BokehUtils::BokehRefThread::BokehRefThread(
    int channel, kiss_fft_cpx* fftcpx_channel_before,
    kiss_fft_cpx* fftcpx_channel, kiss_fft_cpx* fftcpx_alpha,
    kiss_fft_cpx* fftcpx_iris, double4* result_buff,
    kiss_fftnd_cfg kissfft_plan_fwd, kiss_fftnd_cfg kissfft_plan_bkwd,
    TDimensionI& dim)
    : m_channel(channel)
    , m_fftcpx_channel_before(fftcpx_channel_before)
    , m_fftcpx_channel(fftcpx_channel)
    , m_fftcpx_alpha(fftcpx_alpha)
    , m_fftcpx_iris(fftcpx_iris)
    , m_result_buff(result_buff)
    , m_kissfft_plan_fwd(kissfft_plan_fwd)
    , m_kissfft_plan_bkwd(kissfft_plan_bkwd)
    , m_dim(dim)
    , m_finished(false)
    , m_isTerminated(false) {}

//------------------------------------

void BokehUtils::BokehRefThread::run() {
  // execute channel fft
  kiss_fftnd(m_kissfft_plan_fwd, m_fftcpx_channel_before, m_fftcpx_channel);

  // cancel check
  if (m_isTerminated) {
    m_finished = true;
    return;
  }

  int size = m_dim.lx * m_dim.ly;

  // multiply filter
  for (int i = 0; i < size; i++) {
    float re, im;
    re = m_fftcpx_channel[i].r * m_fftcpx_iris[i].r -
         m_fftcpx_channel[i].i * m_fftcpx_iris[i].i;
    im = m_fftcpx_channel[i].r * m_fftcpx_iris[i].i +
         m_fftcpx_iris[i].r * m_fftcpx_channel[i].i;
    m_fftcpx_channel[i].r = re;
    m_fftcpx_channel[i].i = im;
  }
  // execute invert fft
  kiss_fftnd(m_kissfft_plan_bkwd, m_fftcpx_channel, m_fftcpx_channel_before);

  // cancel check
  if (m_isTerminated) {
    m_finished = true;
    return;
  }

  // pixels with smaller index : normal composite exposure value
  // pixels with the same or larger index : replace exposure value
  double4* result_p = m_result_buff;
  for (int i = 0; i < size; i++, result_p++) {
    // modify fft coordinate to normal
    int coord = getCoord(i, m_dim.lx, m_dim.ly);

    double alpha = (double)m_fftcpx_alpha[coord].r / (double)size;
    // ignore transpalent pixels
    if (alpha < 0.00001) continue;

    double exposure = (double)m_fftcpx_channel_before[coord].r / (double)size;

    // in case of using upper layer at all
    if (alpha >= 1.0 || (m_channel == 0 && (*result_p).x == 0.0) ||
        (m_channel == 1 && (*result_p).y == 0.0) ||
        (m_channel == 2 && (*result_p).z == 0.0)) {
      // set exposure
      if (m_channel == 0)  // R
        (*result_p).x = exposure;
      else if (m_channel == 1)  // G
        (*result_p).y = exposure;
      else  // B
        (*result_p).z = exposure;
    }
    // in case of compositing both layers
    else {
      if (m_channel == 0)  // R
      {
        (*result_p).x *= 1.0 - alpha;
        (*result_p).x += exposure;
      } else if (m_channel == 1)  // G
      {
        (*result_p).y *= 1.0 - alpha;
        (*result_p).y += exposure;
      } else  // B
      {
        (*result_p).z *= 1.0 - alpha;
        (*result_p).z += exposure;
      }
    }
  }
  m_finished = true;
}

//------------------------------------------------------------

//------------------------------------------------------------
// normalize the source raster image to 0-1 and set to dstMem
// returns true if the source is (seems to be) premultiplied
//------------------------------------------------------------
template void BokehUtils::setSourceRaster<TRaster32P, TPixel32>(
    const TRaster32P srcRas, double4* dstMem, TDimensionI dim);
template void BokehUtils::setSourceRaster<TRaster64P, TPixel64>(
    const TRaster64P srcRas, double4* dstMem, TDimensionI dim);

template <typename RASTER, typename PIXEL>
void BokehUtils::setSourceRaster(const RASTER srcRas, double4* dstMem,
                                 TDimensionI dim) {
  double4* chann_p = dstMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, chann_p++) {
      (*chann_p).x = (double)pix->r / (double)PIXEL::maxChannelValue;
      (*chann_p).y = (double)pix->g / (double)PIXEL::maxChannelValue;
      (*chann_p).z = (double)pix->b / (double)PIXEL::maxChannelValue;
      (*chann_p).w = (double)pix->m / (double)PIXEL::maxChannelValue;
    }
  }
}

//------------------------------------------------------------
// normalize brightness of the depth reference image to unsigned char
// and store into detMem
//------------------------------------------------------------
template void BokehUtils::setDepthRaster<TRaster32P, TPixel32>(
    const TRaster32P srcRas, unsigned char* dstMem, TDimensionI dim);
template void BokehUtils::setDepthRaster<TRaster64P, TPixel64>(
    const TRaster64P srcRas, unsigned char* dstMem, TDimensionI dim);

template <typename RASTER, typename PIXEL>
void BokehUtils::setDepthRaster(const RASTER srcRas, unsigned char* dstMem,
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

//------------------------------------------------------------
// create the depth index map
//------------------------------------------------------------
void BokehUtils::defineSegemntDepth(
    const unsigned char* indexMap_main, const unsigned char* indexMap_sub,
    const double* mainSub_ratio, const unsigned char* depth_buff,
    const TDimensionI& dimOut, QVector<double>& segmentDepth_main,
    QVector<double>& segmentDepth_sub, const double focusDepth,
    int distancePrecision, double nearDepth, double farDepth) {
  QSet<int> segmentValues;

  // histogram parameters
  struct HISTO {
    int pix_amount;
    int belongingSegmentValue;  // value to which temporary segmented
    int segmentId;
    int segmentId_sub;
  };
  std::array<HISTO, 256> histo;

  // initialize
  for (int h = 0; h < 256; h++) {
    histo[h].pix_amount            = 0;
    histo[h].belongingSegmentValue = -1;
    histo[h].segmentId             = -1;
  }

  int size = dimOut.lx * dimOut.ly;

  // max and min
  int minHisto = (int)UCHAR_MAX;
  int maxHisto = 0;

  unsigned char* depth_p = (unsigned char*)depth_buff;
  for (int i = 0; i < size; i++, depth_p++) {
    histo[(int)*depth_p].pix_amount++;
    // update max and min
    if ((int)*depth_p < minHisto) minHisto = (int)*depth_p;
    if ((int)*depth_p > maxHisto) maxHisto = (int)*depth_p;
  }

  // the maximum and the minimum depth become the segment layers
  segmentValues.insert(minHisto);
  segmentValues.insert(maxHisto);

  // The nearest depth in the histogram
  double minDepth =
      (farDepth - nearDepth) * (double)minHisto / 255.0 + nearDepth;
  // The furthest depth in the histogram
  double maxDepth =
      (farDepth - nearDepth) * (double)maxHisto / 255.0 + nearDepth;
  // focus depth becomes the segment layer as well
  if (minDepth < focusDepth && focusDepth < maxDepth)
    segmentValues.insert(std::round(
        (double)UCHAR_MAX * (focusDepth - nearDepth) / (farDepth - nearDepth)));

  // set the initial segmentation for each depth value
  for (int h = 0; h < 256; h++) {
    for (int seg = 0; seg < segmentValues.size(); seg++) {
      // set the segment
      if (histo[h].belongingSegmentValue == -1) {
        histo[h].belongingSegmentValue = segmentValues.values().at(seg);
        continue;
      }
      // error amount at the current registered layers
      int tmpError = std::abs(h - histo[h].belongingSegmentValue);
      if (tmpError == 0) break;
      // new error amount
      int newError = std::abs(h - segmentValues.values().at(seg));
      // compare the two and update
      if (newError < tmpError)
        histo[h].belongingSegmentValue = segmentValues.values().at(seg);
    }
  }

  // add the segment layers to the distance precision value
  while (segmentValues.size() < distancePrecision) {
    // add a new segment at the value which will reduce the error amount in
    // maximum
    double tmpMaxErrorMod = 0;
    int tmpBestNewSegVal;
    bool newSegFound = false;
    for (int h = minHisto + 1; h < maxHisto; h++) {
      // if it is already set as the segment, continue
      if (histo[h].belongingSegmentValue == h) continue;

      double errorModAmount = 0;
      // estimate how much the error will be reduced if the current h becomes
      // segment
      for (int i = minHisto + 1; i < maxHisto; i++) {
        // compare the current segment value and h and take the nearest value
        // if h is near (from i), then accumulate the estimated error reduction
        // amount
        if (std::abs(i - histo[i].belongingSegmentValue) >
            std::abs(i - h))  // the current segment value has
                              // priority, if the distance is the same
          errorModAmount +=
              (std::abs(i - histo[i].belongingSegmentValue) - std::abs(i - h)) *
              histo[i].pix_amount;
      }

      // if h will reduce the error, update the candidate segment value
      if (errorModAmount > tmpMaxErrorMod) {
        tmpMaxErrorMod   = errorModAmount;
        tmpBestNewSegVal = h;
        newSegFound      = true;
      }
    }

    if (!newSegFound) break;

    // register tmpBestNewSegVal to the segment values list
    segmentValues.insert(tmpBestNewSegVal);

    // update belongingSegmentValue
    for (int h = minHisto + 1; h < maxHisto; h++) {
      // compare the current segment value and h and take the nearest value
      // if tmpBestNewSegVal is near (from h), then update the
      // belongingSegmentValue
      if (std::abs(h - histo[h].belongingSegmentValue) >
          std::abs(h - tmpBestNewSegVal))  // the current segment value has
        // priority, if the distance is the same
        histo[h].belongingSegmentValue = tmpBestNewSegVal;
    }
  }

  // set indices from the farthest and create the index table for each depth
  // value
  QVector<int> segValVec;
  int tmpSegVal = -1;
  int tmpSegId  = -1;
  for (int h = 255; h >= 0; h--) {
    if (histo[h].belongingSegmentValue != tmpSegVal) {
      segmentDepth_main.push_back((double)histo[h].belongingSegmentValue /
                                  (double)UCHAR_MAX);
      tmpSegVal = histo[h].belongingSegmentValue;
      tmpSegId++;
      segValVec.push_back(tmpSegVal);
    }
    histo[h].segmentId = tmpSegId;
  }

  // "sub" depth segment value list for interporation
  for (int d = 0; d < segmentDepth_main.size() - 1; d++)
    segmentDepth_sub.push_back(
        (segmentDepth_main.at(d) + segmentDepth_main.at(d + 1)) / 2.0);

  // create the "sub" index table for each depth value
  tmpSegId = 0;
  for (int seg = 0; seg < segValVec.size() - 1; seg++) {
    int hMax = (seg == 0) ? 255 : segValVec.at(seg);
    int hMin = (seg == segValVec.size() - 2) ? 0 : segValVec.at(seg + 1) + 1;
    for (int h = hMax; h >= hMin; h--) histo[h].segmentId_sub = tmpSegId;
    tmpSegId++;
  }

  // convert the depth value to the segment index by using the index table
  depth_p               = (unsigned char*)depth_buff;
  unsigned char* main_p = (unsigned char*)indexMap_main;
  unsigned char* sub_p  = (unsigned char*)indexMap_sub;
  // mainSub_ratio represents the composition ratio of the image with "main"
  // separation.
  double* ratio_p = (double*)mainSub_ratio;
  for (int i = 0; i < size; i++, depth_p++, main_p++, sub_p++, ratio_p++) {
    *main_p = (unsigned char)histo[(int)*depth_p].segmentId;
    *sub_p  = (unsigned char)histo[(int)*depth_p].segmentId_sub;

    double depth         = (double)*depth_p / (double)UCHAR_MAX;
    double main_segDepth = segmentDepth_main.at(*main_p);
    double sub_segDepth  = segmentDepth_sub.at(*sub_p);

    if (main_segDepth == sub_segDepth)
      *ratio_p = 1.0;
    else {
      *ratio_p = 1.0 - (main_segDepth - depth) / (main_segDepth - sub_segDepth);
      *ratio_p = clamp01(*ratio_p);
    }
  }
}

//--------------------------------------------
// convert source image value rgb -> exposure
//--------------------------------------------
void BokehUtils::convertRGBToExposure(const double4* source_buff, int size,
                                      double filmGamma) {
  double4* source_p = (double4*)source_buff;
  for (int i = 0; i < size; i++, source_p++) {
    // continue if alpha channel is 0
    if ((*source_p).w == 0.0) {
      (*source_p).x = 0.0;
      (*source_p).y = 0.0;
      (*source_p).z = 0.0;
      continue;
    }

    // RGB value -> exposure
    (*source_p).x = valueToExposure((*source_p).x, filmGamma);
    (*source_p).y = valueToExposure((*source_p).y, filmGamma);
    (*source_p).z = valueToExposure((*source_p).z, filmGamma);

    // multiply with alpha channel
    (*source_p).x *= (*source_p).w;
    (*source_p).y *= (*source_p).w;
    (*source_p).z *= (*source_p).w;
  }
}

//--------------------------------------------
// convert result image value exposure -> rgb
//--------------------------------------------
void BokehUtils::convertExposureToRGB(const double4* result_buff, int size,
                                      double filmGamma) {
  double4* res_p = (double4*)result_buff;
  for (int i = 0; i < size; i++, res_p++) {
    (*res_p).x = clamp01(exposureToValue((*res_p).x, filmGamma));
    (*res_p).y = clamp01(exposureToValue((*res_p).y, filmGamma));
    (*res_p).z = clamp01(exposureToValue((*res_p).z, filmGamma));
  }
}

//-----------------------------------------------------
// obtain iris size from the depth value
//-----------------------------------------------------
double BokehUtils::calcIrisSize(
    const double depth,  // relative depth where near depth is set to 0 and far
                         // depth is 1
    const double bokehPixelAmount, const double onFocusDistance,
    const double bokehAdjustment, double nearDepth,
    double farDepth)  // actual depth of black and white reference pixels
{
  double realDepth = nearDepth + (farDepth - nearDepth) * depth;
  return ((double)onFocusDistance - realDepth) * bokehPixelAmount *
         bokehAdjustment;
}

namespace {
//--------------------------------------------
// apply single median filter
//--------------------------------------------
void doSingleMedian(const double4* source_buff,
                    const double4* segment_layer_buff,
                    const unsigned char* indexMap_mainSub, int index, int lx,
                    int ly, const unsigned char* generation_buff, int curGen) {
  double4* source_p         = (double4*)source_buff;
  double4* layer_p          = (double4*)segment_layer_buff;
  unsigned char* indexMap_p = (unsigned char*)indexMap_mainSub;
  unsigned char* gen_p      = (unsigned char*)generation_buff;
  for (int posY = 0; posY < ly; posY++) {
    for (int posX = 0; posX < lx;
         posX++, source_p++, layer_p++, indexMap_p++, gen_p++) {
      // continue if the current pixel is at the same or far depth
      if ((int)(*indexMap_p) <= index) continue;
      // continue if the current pixel is already extended
      if ((*gen_p) > 0) continue;

      // check out the neighbor pixels. store brightness in neighbor[x].w
      double4 neighbor[8];
      int neighbor_amount = 0;
      for (int ky = posY - 1; ky <= posY + 1; ky++) {
        for (int kx = posX - 1; kx <= posX + 1; kx++) {
          // skip the current pixel itself
          if (kx == posX && ky == posY) continue;
          // border condition
          if (ky < 0 || ky >= ly || kx < 0 || kx >= lx) continue;
          // index in the buffer
          int neighborId = ky * lx + kx;

          if ((int)indexMap_mainSub[neighborId] !=
                  index &&  // pixels from the original image can be used as
                            // neighbors
              (generation_buff[neighborId] == 0 ||  // pixels which is not yet
                                                    // be extended cannot be
                                                    // used as neighbors
               generation_buff[neighborId] == curGen))  // pixels which is
                                                        // extended in the
                                                        // current median
                                                        // generation cannot be
                                                        // used as neighbors
            continue;
          // compute brightness (actually, it is "pseudo" brightness
          // since the source buffer is already converted to exposure)
          double brightness = source_buff[neighborId].x * 0.3 +
                              source_buff[neighborId].y * 0.59 +
                              source_buff[neighborId].z * 0.11;
          // insert with sorting
          int ins_index;
          for (ins_index = 0; ins_index < neighbor_amount; ins_index++) {
            if (neighbor[ins_index].w < brightness) break;
          }
          // displace neighbor values from neighbor_amount-1 to ins_index
          for (int k = neighbor_amount - 1; k >= ins_index; k--) {
            neighbor[k + 1].x = neighbor[k].x;
            neighbor[k + 1].y = neighbor[k].y;
            neighbor[k + 1].z = neighbor[k].z;
            neighbor[k + 1].w = neighbor[k].w;
          }
          // set the neighbor value
          neighbor[ins_index].x = source_buff[neighborId].x;
          neighbor[ins_index].y = source_buff[neighborId].y;
          neighbor[ins_index].z = source_buff[neighborId].z;
          neighbor[ins_index].w = brightness;

          // increment the count
          neighbor_amount++;
        }
      }

      // If there is no neighbor pixles available, continue
      if (neighbor_amount == 0) continue;

      // switch the behavior when there are even number of neighbors
      bool flag = ((posX + posY) % 2 == 0);
      // pick up the medium index
      int pickIndex = (flag)
                          ? (int)std::floor((double)(neighbor_amount - 1) / 2.0)
                          : (int)std::ceil((double)(neighbor_amount - 1) / 2.0);

      // set the medium pixel values
      (*layer_p).x = neighbor[pickIndex].x;
      (*layer_p).y = neighbor[pickIndex].y;
      (*layer_p).z = neighbor[pickIndex].z;
      (*layer_p).w = (*source_p).w;

      // set the generation
      (*gen_p) = (unsigned char)curGen;
    }
  }
}

void doSingleExtend(const double4* source_buff,
                    const double4* segment_layer_buff,
                    const unsigned char* indexMap_mainSub, int index, int lx,
                    int ly, const unsigned char* generation_buff, int curGen) {
  double4* source_p         = (double4*)source_buff;
  double4* layer_p          = (double4*)segment_layer_buff;
  unsigned char* indexMap_p = (unsigned char*)indexMap_mainSub;
  unsigned char* gen_p      = (unsigned char*)generation_buff;
  for (int posY = 0; posY < ly; posY++) {
    for (int posX = 0; posX < lx;
         posX++, source_p++, layer_p++, indexMap_p++, gen_p++) {
      // continue if the current pixel is at the same or far depth
      if ((int)(*indexMap_p) <= index) continue;
      // continue if the current pixel is already extended
      if ((*gen_p) > 0) continue;

      // check out the neighbor pixels. store brightness in neighbor[x].w
      double4 neighbor[8];
      bool neighbor_found = false;
      for (int ky = posY - 1; ky <= posY + 1; ky++) {
        for (int kx = posX - 1; kx <= posX + 1; kx++) {
          // skip the current pixel itself
          if (kx == posX && ky == posY) continue;
          // border condition
          if (ky < 0 || ky >= ly || kx < 0 || kx >= lx) continue;
          // index in the buffer
          int neighborId = ky * lx + kx;

          if ((int)indexMap_mainSub[neighborId] !=
                  index &&  // pixels from the original image can be used as
                            // neighbors
              (generation_buff[neighborId] == 0 ||  // pixels which is not yet
                                                    // be extended cannot be
                                                    // used as neighbors
               generation_buff[neighborId] == curGen))  // pixels which is
                                                        // extended in the
                                                        // current median
                                                        // generation cannot be
                                                        // used as neighbors
            continue;
          // increment the count
          neighbor_found = true;
          break;
        }
        if (neighbor_found) break;
      }

      // If there is no neighbor pixles available, continue
      if (!neighbor_found) continue;

      // set the medium pixel values
      (*layer_p).x = (*source_p).x;
      (*layer_p).y = (*source_p).y;
      (*layer_p).z = (*source_p).z;
      (*layer_p).w = (*source_p).w;

      // set the generation
      (*gen_p) = (unsigned char)curGen;
    }
  }
}
}  // namespace

//--------------------------------------------
// generate the segment layer source at the current depth
// considering fillGap and doMedian options
//--------------------------------------------
void BokehUtils::retrieveLayer(const double4* source_buff,
                               const double4* segment_layer_buff,
                               const unsigned char* indexMap_mainSub, int index,
                               int lx, int ly, bool fillGap, bool doMedian,
                               int margin) {
  // only when fillGap is ON and doMedian is OFF,
  // fill the region where will be behind of the near layers
  // bool fill = (fillGap && !doMedian);
  // retrieve the regions with the current depth
  double4* source_p         = (double4*)source_buff;
  double4* layer_p          = (double4*)segment_layer_buff;
  unsigned char* indexMap_p = (unsigned char*)indexMap_mainSub;
  for (int i = 0; i < lx * ly; i++, source_p++, layer_p++, indexMap_p++) {
    // continue if the current pixel is at the far layer
    // consider the fill flag if the current pixel is at the near layer
    // if ((int)(*indexMap_p) < index || (!fill && (int)(*indexMap_p) > index))
    if ((int)(*indexMap_p) != index) continue;

    // copy pixel values
    (*layer_p).x = (*source_p).x;
    (*layer_p).y = (*source_p).y;
    (*layer_p).z = (*source_p).z;
    (*layer_p).w = (*source_p).w;
  }

  if (!fillGap && !doMedian) return;
  if (margin == 0) return;

  // extend pixels by using median filter
  unsigned char* generation_buff;
  TRasterGR8P generation_buff_ras = allocateRasterAndLock<unsigned char>(
      &generation_buff, TDimensionI(lx, ly));

  // extend (margin * 2) pixels in order to enough cover when two adjacent
  // layers are both blurred in the maximum radius
  for (int gen = 0; gen < margin * 2; gen++) {
    if (doMedian)
      // apply single median filter
      doSingleMedian(source_buff, segment_layer_buff, indexMap_mainSub, index,
                     lx, ly, generation_buff, gen + 1);
    else
      // put the source pixel as-is
      doSingleExtend(source_buff, segment_layer_buff, indexMap_mainSub, index,
                     lx, ly, generation_buff, gen + 1);
  }

  generation_buff_ras->unlock();
}

//--------------------------------------------
// normal-composite the layer as is, without filtering
//--------------------------------------------
void BokehUtils::compositeAsIs(const double4* segment_layer_buff,
                               const double4* result_buff_mainSub, int size) {
  double4* layer_p  = (double4*)segment_layer_buff;
  double4* result_p = (double4*)result_buff_mainSub;
  for (int i = 0; i < size; i++, layer_p++, result_p++) {
    // in case the pixel is full opac
    if ((*layer_p).w == 1.0f) {
      (*result_p).x = (*layer_p).x;
      (*result_p).y = (*layer_p).y;
      (*result_p).z = (*layer_p).z;
      (*result_p).w = 1.0f;
      continue;
    }
    // in case the pixel is full transparent
    else if ((*layer_p).w == 0.0f)
      continue;
    // in case the pixel is semi-transparent, do normal composite
    else {
      (*result_p).x = (*layer_p).x + (*result_p).x * (1.0 - (*layer_p).w);
      (*result_p).y = (*layer_p).y + (*result_p).y * (1.0 - (*layer_p).w);
      (*result_p).z = (*layer_p).z + (*result_p).z * (1.0 - (*layer_p).w);
      (*result_p).w = (*layer_p).w + (*result_p).w * (1.0 - (*layer_p).w);
    }
  }
}

//------------------------------------------------
// Resize / flip the iris image according to the size ratio.
// Normalize the brightness of the iris image.
// Enlarge the iris to the output size.
void BokehUtils::convertIris(const double irisSize,
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
  // add 1 pixel margins to all sides
  int2 filterSize = {int(std::ceil(resizedIrisSize.x)) + 2,
                     int(std::ceil(resizedIrisSize.y)) + 2};

  TPointD resizeOffset((double)filterSize.x - resizedIrisSize.x,
                       (double)filterSize.y - resizedIrisSize.y);
  // Add some adjustment in order to absorb the difference of the cases when the
  // iris size is odd and even numbers.
  // Try to set the center of the iris to the center of the screen
  if ((dimOut.lx - filterSize.x) % 2 == 1) filterSize.x++;
  if ((dimOut.ly - filterSize.y) % 2 == 1) filterSize.y++;

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
  TPointD affOffset(0.5, 0.5);
  affOffset += TPointD((dimOut.lx % 2 == 1) ? 0.5 : 0.0,
                       (dimOut.ly % 2 == 1) ? 0.5 : 0.0);

  aff = TTranslation(resizedIris->getCenterD() + affOffset);
  aff *= TScale(irisSizeResampleRatio);
  aff *= TTranslation(-(irisTile.getRaster()->getCenterD() + affOffset));

  // resample the iris
  TRop::resample(resizedIris, irisTile.getRaster(), aff);

  // accumulated value
  float irisValAmount = 0.0f;

  int iris_j = 0;
  // Initialize
  for (int i = 0; i < dimOut.lx * dimOut.ly; i++) {
    kissfft_comp_iris_before[i].r = 0.0f;
    kissfft_comp_iris_before[i].i = 0.0f;
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

//--------------------------------------------
// retrieve segment layer image for each channel
//--------------------------------------------
void BokehUtils::retrieveChannel(const double4* segment_layer_buff,  // src
                                 kiss_fft_cpx* fftcpx_r_before,      // dst
                                 kiss_fft_cpx* fftcpx_g_before,      // dst
                                 kiss_fft_cpx* fftcpx_b_before,      // dst
                                 kiss_fft_cpx* fftcpx_a_before,      // dst
                                 int size) {
  double4* layer_p = (double4*)segment_layer_buff;
  for (int i = 0; i < size; i++, layer_p++) {
    fftcpx_r_before[i].r = (*layer_p).x;
    fftcpx_g_before[i].r = (*layer_p).y;
    fftcpx_b_before[i].r = (*layer_p).z;
    fftcpx_a_before[i].r = (*layer_p).w;
  }
}

//--------------------------------------------
// multiply filter on channel
//--------------------------------------------
void BokehUtils::multiplyFilter(kiss_fft_cpx* fftcpx_channel,  // dst
                                kiss_fft_cpx* fftcpx_iris,     // filter
                                int size) {
  for (int i = 0; i < size; i++) {
    float re, im;
    re = fftcpx_channel[i].r * fftcpx_iris[i].r -
         fftcpx_channel[i].i * fftcpx_iris[i].i;
    im = fftcpx_channel[i].r * fftcpx_iris[i].i +
         fftcpx_channel[i].i * fftcpx_iris[i].r;

    fftcpx_channel[i].r = re;
    fftcpx_channel[i].i = im;
  }
}

//--------------------------------------------
// normal comosite the alpha channel
//--------------------------------------------
void BokehUtils::compositeAlpha(const double4* result_buff,        // dst
                                const kiss_fft_cpx* fftcpx_alpha,  // alpha
                                int lx, int ly) {
  int size          = lx * ly;
  double4* result_p = (double4*)result_buff;
  for (int i = 0; i < size; i++, result_p++) {
    // modify fft coordinate to normal
    double alpha  = (double)fftcpx_alpha[getCoord(i, lx, ly)].r / (double)size;
    alpha         = clamp01(alpha);
    (*result_p).w = alpha + ((*result_p).w * (1.0 - alpha));
  }
}

//--------------------------------------------
// interpolate main and sub exposures
// set to result
//--------------------------------------------
void BokehUtils::interpolateExposureAndConvertToRGB(
    const double4* result_main_buff,  // result1
    const double4* result_sub_buff,   // result2
    const double* mainSub_ratio,      // ratio
    const double4* result_buff,       // dst
    int size, double layerHardnessRatio) {
  double4* resultMain_p = (double4*)result_main_buff;
  double4* resultSub_p  = (double4*)result_sub_buff;
  double* ratio_p       = (double*)mainSub_ratio;
  double4* out_p        = (double4*)result_buff;
  for (int i = 0; i < size;
       i++, resultMain_p++, resultSub_p++, ratio_p++, out_p++) {
    // interpolate main and sub exposures
    double4 result;

    result.x =
        (*resultMain_p).x * (*ratio_p) + (*resultSub_p).x * (1.0 - (*ratio_p));
    result.y =
        (*resultMain_p).y * (*ratio_p) + (*resultSub_p).y * (1.0 - (*ratio_p));
    result.z =
        (*resultMain_p).z * (*ratio_p) + (*resultSub_p).z * (1.0 - (*ratio_p));
    result.w =
        (*resultMain_p).w * (*ratio_p) + (*resultSub_p).w * (1.0 - (*ratio_p));

    // continue for transparent pixel
    if (result.w == 0.0) {
      continue;
    }

    // convert exposure by layer hardness
    if (layerHardnessRatio != 1.0) {
      result.x = std::pow(result.x / result.w, layerHardnessRatio) * result.w;
      result.y = std::pow(result.y / result.w, layerHardnessRatio) * result.w;
      result.z = std::pow(result.z / result.w, layerHardnessRatio) * result.w;
    }

    // in case the result is replaced by the upper layer pixel
    if (result.w >= 1.0f) {
      (*out_p).x = result.x;
      (*out_p).y = result.y;
      (*out_p).z = result.z;
    }
    // in case the layers will be composed
    else {
      (*out_p).x = (*out_p).x * (1.0 - result.w) + result.x;
      (*out_p).y = (*out_p).y * (1.0 - result.w) + result.y;
      (*out_p).z = (*out_p).z * (1.0 - result.w) + result.z;
    }

    (*out_p).w = (*out_p).w * (1.0 - result.w) + result.w;
  }
}

//--------------------------------------------
//"Over" composite the layer to the output exposure.
void BokehUtils::compositLayerAsIs(TTile& layerTile, double4* result,
                                   TDimensionI& dimOut, double filmGamma) {
  double4* layer_buff;
  TRasterGR8P layer_buff_ras(dimOut.lx * sizeof(double4), dimOut.ly);
  layer_buff_ras->lock();
  layer_buff = (double4*)layer_buff_ras->getRawData();

  TRaster32P ras32 = (TRaster32P)layerTile.getRaster();
  TRaster64P ras64 = (TRaster64P)layerTile.getRaster();
  lock.lockForRead();
  if (ras32)
    BokehUtils::setSourceRaster<TRaster32P, TPixel32>(ras32, layer_buff,
                                                      dimOut);
  else if (ras64)
    BokehUtils::setSourceRaster<TRaster64P, TPixel64>(ras64, layer_buff,
                                                      dimOut);
  lock.unlock();

  double4* lay_p = layer_buff;
  double4* res_p = result;
  for (int i = 0; i < dimOut.lx * dimOut.ly; i++, lay_p++, res_p++) {
    if ((*lay_p).w <= 0.0)
      continue;
    else if ((*lay_p).w < 1.0) {
      // composite exposure
      (*res_p).x = valueToExposure((*lay_p).x, filmGamma) * (*lay_p).w +
                   (*res_p).x * (1.0 - (*lay_p).w);
      (*res_p).y = valueToExposure((*lay_p).y, filmGamma) * (*lay_p).w +
                   (*res_p).y * (1.0 - (*lay_p).w);
      (*res_p).z = valueToExposure((*lay_p).z, filmGamma) * (*lay_p).w +
                   (*res_p).z * (1.0 - (*lay_p).w);
      // over composite alpha
      (*res_p).w = (*lay_p).w + ((*res_p).w * (1.0 - (*lay_p).w));
      (*res_p).w = clamp01((*res_p).w);
    } else  // replace by upper layer
    {
      (*res_p).x = valueToExposure((*lay_p).x, filmGamma);
      (*res_p).y = valueToExposure((*lay_p).y, filmGamma);
      (*res_p).z = valueToExposure((*lay_p).z, filmGamma);
      (*res_p).w = 1.0;
    }
  }

  layer_buff_ras->unlock();
}

//--------------------------------------------
// Do FFT the alpha channel.
// Forward FFT -> Multiply by the iris data -> Backward FFT
void BokehUtils::calcAlfaChannelBokeh(kiss_fft_cpx* kissfft_comp_iris,
                                      TTile& layerTile, double* alpha_bokeh) {
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
        kissfft_comp_in[j * lx + i].r = (double)pix->m / (double)UCHAR_MAX;
        pix++;
      }
    }
  } else if (ras64) {
    for (int j = 0; j < ly; j++) {
      TPixel64* pix = ras64->pixels(j);
      for (int i = 0; i < lx; i++) {
        kissfft_comp_in[j * lx + i].r = (double)pix->m / (double)USHRT_MAX;
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

  // store to the buffer
  double* alp_p = alpha_bokeh;
  for (int j = 0; j < ly; j++) {
    for (int i = 0; i < lx; i++, alp_p++) {
      (*alp_p) = (double)kissfft_comp_in[getCoord(j * lx + i, lx, ly)].r /
                 (double)(lx * ly);
      (*alp_p) = clamp01(*alp_p);
    }
  }

  kissfft_comp_in_ras->unlock();
}

//-----------------------------------------------------
// convert to channel value and set to output

template void BokehUtils::setOutputRaster<TRaster32P, TPixel32>(
    double4* src, const TRaster32P dstRas, TDimensionI& dim, int2 margin);
template void BokehUtils::setOutputRaster<TRaster64P, TPixel64>(
    double4* src, const TRaster64P dstRas, TDimensionI& dim, int2 margin);

template <typename RASTER, typename PIXEL>
void BokehUtils::setOutputRaster(double4* src, const RASTER dstRas,
                                 TDimensionI& dim, int2 margin) {
  double4* src_p = src + (margin.y * dim.lx);

  for (int j = 0; j < dstRas->getLy(); j++) {
    PIXEL* outPix = dstRas->pixels(j);
    src_p += margin.x;
    for (int i = 0; i < dstRas->getLx(); i++, outPix++, src_p++) {
      double val;
      val = (*src_p).x * (double)PIXEL::maxChannelValue + 0.5;
      outPix->r =
          (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                        ? (double)PIXEL::maxChannelValue
                                        : val);
      val = (*src_p).y * (double)PIXEL::maxChannelValue + 0.5;
      outPix->g =
          (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                        ? (double)PIXEL::maxChannelValue
                                        : val);
      val = (*src_p).z * (double)PIXEL::maxChannelValue + 0.5;
      outPix->b =
          (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                        ? (double)PIXEL::maxChannelValue
                                        : val);
      val = (*src_p).w * (double)PIXEL::maxChannelValue + 0.5;
      outPix->m =
          (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                        ? (double)PIXEL::maxChannelValue
                                        : val);
    }
    src_p += margin.x;
  }
}

//-----------------------------------------------------
// Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
double BokehUtils::getBokehPixelAmount(const double bokehAmount,
                                       const TAffine affine) {
  /*--- Convert to vector --- */
  TPointD vect(bokehAmount, 0.0);
  /*--- Apply geometrical transformation ---*/
  // For the following lines I referred to lines 586-592 of
  // sources/stdfx/motionblurfx.cpp
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0; /* ignore translation */
  vect              = aff * vect;
  /*--- return the length of the vector ---*/
  return std::sqrt(vect.x * vect.x + vect.y * vect.y);
}

//-----------------------------------------------------

Iwa_BokehCommonFx::Iwa_BokehCommonFx()
    : m_onFocusDistance(0.5), m_bokehAmount(30.0), m_hardness(0.3) {
  addInputPort("Iris", m_iris);

  // Set the ranges of common parameters
  m_onFocusDistance->setValueRange(0.0, 10.);
  m_bokehAmount->setValueRange(0.0, 300.0);
  m_bokehAmount->setMeasureName("fxLength");
  m_hardness->setValueRange(0.05, 3.0);
}

//--------------------------------------------
bool Iwa_BokehCommonFx::doGetBBox(double frame, TRectD& bBox,
                                  const TRenderSettings& info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//--------------------------------------------
bool Iwa_BokehCommonFx::canHandle(const TRenderSettings& info, double frame) {
  return false;
}

//--------------------------------------------
void Iwa_BokehCommonFx::doFx(TTile& tile, double frame,
                             const TRenderSettings& settings,
                             double bokehPixelAmount, int margin,
                             TDimensionI& dimOut, TRectD& irisBBox,
                             TTile& irisTile, QList<LayerValue>& layerValues,
                             QMap<int, unsigned char*>& ctrls) {
  // This fx is relatively heavy so the multi thread computation is introduced.
  // Lock the mutex here in order to prevent multiple rendering tasks run at the
  // same time.
  // QMutexLocker fx_locker(&fx_mutex);

  QList<TRasterGR8P> rasterList;
  QList<kiss_fftnd_cfg> planList;

  kiss_fft_cpx* kissfft_comp_iris;
  double* alpha_bokeh = nullptr;
  double4* result     = nullptr;
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&kissfft_comp_iris, dimOut));
  rasterList.append(allocateRasterAndLock<double>(&alpha_bokeh, dimOut));
  rasterList.append(allocateRasterAndLock<double4>(&result, dimOut));

  double4 zero = {0.0, 0.0, 0.0, 0.0};
  // initialize
  std::fill_n(result, dimOut.lx * dimOut.ly, zero);

  double masterHardness = m_hardness->getValue(frame);

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRastersAndPlans(rasterList, planList);
    return;
  }

  // compute layers from further to nearer
  for (int i = 0; i < layerValues.size(); i++) {
    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      releaseAllRastersAndPlans(rasterList, planList);
      return;
    }

    LayerValue layer = layerValues.at(i);

    //-------------------

    // separate layer by the reference image
    int ctrlIndex = layer.depth_ref;
    if (ctrlIndex > 0 && ctrls.contains(ctrlIndex) && ctrls[ctrlIndex] != 0) {
      TTile* layerTile = layer.sourceTile;
      if (!layer.premultiply) TRop::depremultiply(layerTile->getRaster());

      doBokehRef(result, frame, settings, bokehPixelAmount, margin, dimOut,
                 irisBBox, irisTile, kissfft_comp_iris, layer,
                 ctrls[ctrlIndex]);

      continue;
    }

    //-------------------

    double layerHardness = layer.layerHardness;
    // The iris size of the current layer
    double irisSize = layer.irisSize;

    // in case the layer is at the focus
    if (-1.0 <= irisSize && 1.0 >= irisSize) {
      TTile* layerTile = layer.sourceTile;
      if (!layer.premultiply) TRop::depremultiply(layerTile->getRaster());
      BokehUtils::compositLayerAsIs(*layerTile, result, dimOut, masterHardness);
      // Continue to the next layer
      continue;
    }

    {
      // prepare for iris FFT
      kiss_fft_cpx* kissfft_comp_iris_before;
      rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(
          &kissfft_comp_iris_before, dimOut));
      // Resize / flip the iris image according to the size ratio.
      // Normalize the brightness of the iris image.
      // Enlarge the iris to the output size.
      BokehUtils::convertIris(irisSize, kissfft_comp_iris_before, dimOut,
                              irisBBox, irisTile);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
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
      // release the iris buffer
      rasterList.takeLast()->unlock();
    }

    // Up to here, FFT-ed iris data is stored in kissfft_comp_iris

    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      releaseAllRastersAndPlans(rasterList, planList);
      return;
    }

    //- - - - - - - - - - - - -
    // Prepare the layer rasters
    TTile* layerTile = layer.sourceTile;

    if (!layer.premultiply) TRop::depremultiply(layerTile->getRaster());

    //- - - - - - - - - - - - -
    // Do FFT the alpha channel.
    // Forward FFT -> Multiply by the iris data -> Backward FFT
    BokehUtils::calcAlfaChannelBokeh(kissfft_comp_iris, *layerTile,
                                     alpha_bokeh);

    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      releaseAllRastersAndPlans(rasterList, planList);
      return;
    }

    BokehUtils::MyThread threadR(
        BokehUtils::MyThread::Red, layerTile->getRaster(), result, alpha_bokeh,
        kissfft_comp_iris, layerHardness, masterHardness);
    BokehUtils::MyThread threadG(
        BokehUtils::MyThread::Green, layerTile->getRaster(), result,
        alpha_bokeh, kissfft_comp_iris, layerHardness, masterHardness);
    BokehUtils::MyThread threadB(
        BokehUtils::MyThread::Blue, layerTile->getRaster(), result, alpha_bokeh,
        kissfft_comp_iris, layerHardness, masterHardness);

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
      // cancel check
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 20) {
        releaseAllRastersAndPlans(rasterList, planList);
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

    waitCount = 0;
    while (1) {
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 20)  // 10 seconds
      {
        if (!threadR.isFinished()) threadR.terminateThread();
        while (!threadR.isFinished()) {
        }
        releaseAllRastersAndPlans(rasterList, planList);
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

    waitCount = 0;
    while (1) {
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 20)  // 10 seconds
      {
        if (!threadR.isFinished()) threadR.terminateThread();
        if (!threadG.isFinished()) threadG.terminateThread();
        while (!threadR.isFinished() || !threadG.isFinished()) {
        }
        releaseAllRastersAndPlans(rasterList, planList);
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
      if ((settings.m_isCanceled && *settings.m_isCanceled) ||
          waitCount >= 2000)  // 100 second timeout
      {
        if (!threadR.isFinished()) threadR.terminateThread();
        if (!threadG.isFinished()) threadG.terminateThread();
        if (!threadB.isFinished()) threadB.terminateThread();
        while (!threadR.isFinished() || !threadG.isFinished() ||
               !threadB.isFinished()) {
        }
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }
      if (threadR.isFinished() && threadG.isFinished() && threadB.isFinished())
        break;
      QThread::msleep(50);
      waitCount++;
    }
  }

  // convert result image value exposure -> rgb
  BokehUtils::convertExposureToRGB(result, dimOut.lx * dimOut.ly,
                                   masterHardness);

  // clear result raster
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();

  int2 outMargin = {(dimOut.lx - tile.getRaster()->getSize().lx) / 2,
                    (dimOut.ly - tile.getRaster()->getSize().ly) / 2};

  lock.lockForWrite();
  if (outRas32)
    BokehUtils::setOutputRaster<TRaster32P, TPixel32>(result, outRas32, dimOut,
                                                      outMargin);
  else if (outRas64)
    BokehUtils::setOutputRaster<TRaster64P, TPixel64>(result, outRas64, dimOut,
                                                      outMargin);
  lock.unlock();

  releaseAllRastersAndPlans(rasterList, planList);
}

void Iwa_BokehCommonFx::doBokehRef(double4* result, double frame,
                                   const TRenderSettings& settings,
                                   double bokehPixelAmount, int margin,
                                   TDimensionI& dimOut, TRectD& irisBBox,
                                   TTile& irisTile,
                                   kiss_fft_cpx* kissfft_comp_iris,
                                   LayerValue layer, unsigned char* ctrl) {
  QList<TRasterGR8P> rasterList;
  QList<kiss_fftnd_cfg> planList;
  // source image
  double4* source_buff;
  rasterList.append(allocateRasterAndLock<double4>(&source_buff, dimOut));

  TRaster32P ras32 = (TRaster32P)layer.sourceTile->getRaster();
  TRaster64P ras64 = (TRaster64P)layer.sourceTile->getRaster();
  lock.lockForRead();
  if (ras32)
    BokehUtils::setSourceRaster<TRaster32P, TPixel32>(ras32, source_buff,
                                                      dimOut);
  else if (ras64)
    BokehUtils::setSourceRaster<TRaster64P, TPixel64>(ras64, source_buff,
                                                      dimOut);
  lock.unlock();

  // create the index map, which indicates which layer each pixel belongs to
  // make two separations and interporate the results in order to avoid
  // artifacts appear at the layer border
  unsigned char* indexMap_main_buff;
  unsigned char* indexMap_sub_buff;
  double* mainSub_ratio_buff;

  rasterList.append(
      allocateRasterAndLock<unsigned char>(&indexMap_main_buff, dimOut));
  rasterList.append(
      allocateRasterAndLock<unsigned char>(&indexMap_sub_buff, dimOut));
  rasterList.append(allocateRasterAndLock<double>(&mainSub_ratio_buff, dimOut));

  QVector<double> segmentDepth_main;
  QVector<double> segmentDepth_sub;

  double layerDitance = layer.distance;
  double nearDepth    = layerDitance - layer.depthRange * layerDitance;
  double farDepth     = nearDepth + layer.depthRange;

  int distancePrecision = layer.distancePrecision;

  // create the depth index map
  BokehUtils::defineSegemntDepth(
      indexMap_main_buff, indexMap_sub_buff, mainSub_ratio_buff, ctrl, dimOut,
      segmentDepth_main, segmentDepth_sub, m_onFocusDistance->getValue(frame),
      distancePrecision, nearDepth, farDepth);

  int size = dimOut.lx * dimOut.ly;

  double4* layer_buff;
  rasterList.append(allocateRasterAndLock<double4>(&layer_buff, dimOut));

  kiss_fft_cpx* kissfft_comp_iris_before;
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&kissfft_comp_iris_before, dimOut));

  // alpha channel
  kiss_fft_cpx* fftcpx_alpha_before;
  kiss_fft_cpx* fftcpx_alpha;
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_alpha_before, dimOut));
  rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_alpha, dimOut));

  // RGB channels
  kiss_fft_cpx* fftcpx_r_before;
  kiss_fft_cpx* fftcpx_g_before;
  kiss_fft_cpx* fftcpx_b_before;
  kiss_fft_cpx* fftcpx_r;
  kiss_fft_cpx* fftcpx_g;
  kiss_fft_cpx* fftcpx_b;
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_r_before, dimOut));
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_g_before, dimOut));
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_b_before, dimOut));
  rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_r, dimOut));
  rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_g, dimOut));
  rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_b, dimOut));

  // for accumulating result image
  double4* result_main_buff;
  double4* result_sub_buff;
  rasterList.append(allocateRasterAndLock<double4>(&result_main_buff, dimOut));
  rasterList.append(allocateRasterAndLock<double4>(&result_sub_buff, dimOut));

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRastersAndPlans(rasterList, planList);
    return;
  }

  // fft plans
  int dims[2]                      = {dimOut.ly, dimOut.lx};
  kiss_fftnd_cfg kissfft_plan_fwd  = kiss_fftnd_alloc(dims, 2, false, 0, 0);
  kiss_fftnd_cfg kissfft_plan_bkwd = kiss_fftnd_alloc(dims, 2, true, 0, 0);
  planList.append(kissfft_plan_fwd);
  planList.append(kissfft_plan_bkwd);

  kiss_fftnd_cfg kissfft_plan_r_fwd  = kiss_fftnd_alloc(dims, 2, false, 0, 0);
  kiss_fftnd_cfg kissfft_plan_r_bkwd = kiss_fftnd_alloc(dims, 2, true, 0, 0);
  kiss_fftnd_cfg kissfft_plan_g_fwd  = kiss_fftnd_alloc(dims, 2, false, 0, 0);
  kiss_fftnd_cfg kissfft_plan_g_bkwd = kiss_fftnd_alloc(dims, 2, true, 0, 0);
  kiss_fftnd_cfg kissfft_plan_b_fwd  = kiss_fftnd_alloc(dims, 2, false, 0, 0);
  kiss_fftnd_cfg kissfft_plan_b_bkwd = kiss_fftnd_alloc(dims, 2, true, 0, 0);
  planList.append(kissfft_plan_r_fwd);
  planList.append(kissfft_plan_r_bkwd);
  planList.append(kissfft_plan_g_fwd);
  planList.append(kissfft_plan_g_bkwd);
  planList.append(kissfft_plan_b_fwd);
  planList.append(kissfft_plan_b_bkwd);

  // initialize result memory
  memset(result_main_buff, 0, sizeof(double4) * size);
  memset(result_sub_buff, 0, sizeof(double4) * size);

  double masterHardness = (double)m_hardness->getValue(frame);
  double layerHardness  = layer.layerHardness;

  // convert source image value rgb -> exposure
  // note that premultiplied source image is already unpremultiplied before this
  // function
  BokehUtils::convertRGBToExposure(source_buff, size, layerHardness);

  double focus  = m_onFocusDistance->getValue(frame);
  double adjust = layer.bokehAdjustment;

  for (int mainSub = 0; mainSub < 2; mainSub++) {
    double4* result_buff_mainSub;
    QVector<double> segmentDepth_mainSub;
    unsigned char* indexMap_mainSub;
    if (mainSub == 0) {
      result_buff_mainSub  = result_main_buff;
      segmentDepth_mainSub = segmentDepth_main;
      indexMap_mainSub     = indexMap_main_buff;
    } else {
      result_buff_mainSub  = result_sub_buff;
      segmentDepth_mainSub = segmentDepth_sub;
      indexMap_mainSub     = indexMap_sub_buff;
    }

    // compute from further to nearer
    for (int index = 0; index < segmentDepth_mainSub.size(); index++) {
      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // compute iris size
      double irisSize = BokehUtils::calcIrisSize(segmentDepth_mainSub.at(index),
                                                 bokehPixelAmount, focus,
                                                 adjust, nearDepth, farDepth);

      memset(layer_buff, 0, sizeof(double4) * size);
      BokehUtils::retrieveLayer(
          source_buff, layer_buff, indexMap_mainSub, index, dimOut.lx,
          dimOut.ly, layer.fillGap, layer.doMedian,
          (index == segmentDepth_mainSub.size() - 1) ? 0 : margin);

      // in case the current segment is at the focus
      if (-1.0 <= irisSize && 1.0 >= irisSize) {
        BokehUtils::compositeAsIs(layer_buff, result_buff_mainSub, size);
        continue;
      }

      // Resize / flip the iris image according to the size ratio.
      // Normalize the brightness of the iris image.
      // Enlarge the iris to the output size.
      BokehUtils::convertIris(irisSize, kissfft_comp_iris_before, dimOut,
                              irisBBox, irisTile);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }
      // Do FFT the iris image.
      kiss_fftnd(kissfft_plan_fwd, kissfft_comp_iris_before, kissfft_comp_iris);

      // initialize alpha
      memset(fftcpx_alpha_before, 0, sizeof(kiss_fft_cpx) * size);
      // initialize channels
      memset(fftcpx_r_before, 0, sizeof(kiss_fft_cpx) * size);
      memset(fftcpx_g_before, 0, sizeof(kiss_fft_cpx) * size);
      memset(fftcpx_b_before, 0, sizeof(kiss_fft_cpx) * size);

      // retrieve segment layer image for each channel
      BokehUtils::retrieveChannel(layer_buff,           // src
                                  fftcpx_r_before,      // dst
                                  fftcpx_g_before,      // dst
                                  fftcpx_b_before,      // dst
                                  fftcpx_alpha_before,  // dst
                                  size);

      // forward fft of alpha channel
      kiss_fftnd(kissfft_plan_fwd, fftcpx_alpha_before, fftcpx_alpha);

      // multiply filter on alpha
      BokehUtils::multiplyFilter(fftcpx_alpha,       // dst
                                 kissfft_comp_iris,  // filter
                                 size);

      // inverse fft the alpha channel
      // note that the result is multiplied by the image size
      kiss_fftnd(kissfft_plan_bkwd, fftcpx_alpha, fftcpx_alpha_before);

      // over composite the alpha channel
      BokehUtils::compositeAlpha(result_buff_mainSub,  // dst
                                 fftcpx_alpha_before,  // alpha
                                 dimOut.lx, dimOut.ly);

      // create worker threads
      BokehUtils::BokehRefThread threadR(
          0, fftcpx_r_before, fftcpx_r, fftcpx_alpha_before, kissfft_comp_iris,
          result_buff_mainSub, kissfft_plan_r_fwd, kissfft_plan_r_bkwd, dimOut);
      BokehUtils::BokehRefThread threadG(
          1, fftcpx_g_before, fftcpx_g, fftcpx_alpha_before, kissfft_comp_iris,
          result_buff_mainSub, kissfft_plan_g_fwd, kissfft_plan_g_bkwd, dimOut);
      BokehUtils::BokehRefThread threadB(
          2, fftcpx_b_before, fftcpx_b, fftcpx_alpha_before, kissfft_comp_iris,
          result_buff_mainSub, kissfft_plan_b_fwd, kissfft_plan_b_bkwd, dimOut);

      // If you set this flag to true, the fx will be forced to compute in
      // single thread.
      // Under some specific condition (such as calling from single-threaded
      // tcomposer)
      // we may need to use this flag... For now, I'll keep this option unused.
      // TODO: investigate this.
      bool renderInSingleThread = false;

      if (renderInSingleThread) {
        threadR.run();
        threadG.run();
        threadB.run();
      } else {
        threadR.start();
        threadG.start();
        threadB.start();
        int waitCount = 0;
        while (1) {
          if ((settings.m_isCanceled && *settings.m_isCanceled) ||
              waitCount >= 2000)  // 100 second timeout
          {
            if (!threadR.isFinished()) threadR.terminateThread();
            if (!threadG.isFinished()) threadG.terminateThread();
            if (!threadB.isFinished()) threadB.terminateThread();
            while (!threadR.isFinished() || !threadG.isFinished() ||
                   !threadB.isFinished()) {
            }
            releaseAllRastersAndPlans(rasterList, planList);
            return;
          }
          if (threadR.isFinished() && threadG.isFinished() &&
              threadB.isFinished())
            break;
          QThread::msleep(50);
          waitCount++;
        }
      }

    }  // for each segment
  }    // main and sub

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRastersAndPlans(rasterList, planList);
    return;
  }

  BokehUtils::interpolateExposureAndConvertToRGB(
      result_main_buff,    // result1
      result_sub_buff,     // result2
      mainSub_ratio_buff,  // ratio
      result,              // dst
      size, layerHardness / masterHardness);

  // release rasters and plans
  releaseAllRastersAndPlans(rasterList, planList);
}