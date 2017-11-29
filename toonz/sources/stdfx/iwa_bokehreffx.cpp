#include "iwa_bokehreffx.h"

#include "trop.h"

#include <QReadWriteLock>
#include <QSet>
#include <math.h>

namespace {
QMutex fx_mutex;
QReadWriteLock lock;

template <typename T>
TRasterGR8P allocateRasterAndLock(T** buf, TDimensionI dim) {
  TRasterGR8P ras(dim.lx * sizeof(T), dim.ly);
  ras->lock();
  *buf = (T*)ras->getRawData();
  return ras;
}

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
};

//------------------------------------
BokehRefThread::BokehRefThread(int channel, kiss_fft_cpx* fftcpx_channel_before,
                               kiss_fft_cpx* fftcpx_channel,
                               kiss_fft_cpx* fftcpx_alpha,
                               kiss_fft_cpx* fftcpx_iris, float4* result_buff,
                               kiss_fftnd_cfg kissfft_plan_fwd,
                               kiss_fftnd_cfg kissfft_plan_bkwd,
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

void BokehRefThread::run() {
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

  // normal composite exposure value
  float4* result_p = m_result_buff;
  for (int i = 0; i < size; i++, result_p++) {
    // modify fft coordinate to normal
    int coord = getCoord(i, m_dim.lx, m_dim.ly);

    float alpha = m_fftcpx_alpha[coord].r / (float)size;
    // ignore transpalent pixels
    if (alpha == 0.0f) continue;

    float exposure = m_fftcpx_channel_before[coord].r / (float)size;

    // in case of using upper layer at all
    if (alpha >= 1.0f || (m_channel == 0 && (*result_p).x == 0.0f) ||
        (m_channel == 1 && (*result_p).y == 0.0f) ||
        (m_channel == 2 && (*result_p).z == 0.0f)) {
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
        (*result_p).x *= 1.0f - alpha;
        (*result_p).x += exposure;
      } else if (m_channel == 1)  // G
      {
        (*result_p).y *= 1.0f - alpha;
        (*result_p).y += exposure;
      } else  // B
      {
        (*result_p).z *= 1.0f - alpha;
        (*result_p).z += exposure;
      }
    }
  }
  m_finished = true;
}

//============================================================

//------------------------------------------------------------
// Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
// DONE
float Iwa_BokehRefFx::getBokehPixelAmount(const double frame,
                                          const TAffine affine) {
  // Convert to vector
  TPointD vect;
  vect.x = m_bokehAmount->getValue(frame);
  vect.y = 0.0;
  // Apply geometrical transformation
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0;  // ignore translation
  vect              = aff * vect;
  // return the length of the vector
  return sqrtf((float)(vect.x * vect.x + vect.y * vect.y));
}

//------------------------------------------------------------
// normalize the source raster image to 0-1 and set to dstMem
// returns true if the source is (seems to be) premultiplied
//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
bool Iwa_BokehRefFx::setSourceRaster(const RASTER srcRas, float4* dstMem,
                                     TDimensionI dim) {
  bool isPremultiplied = true;

  float4* chann_p = dstMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, chann_p++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;

      // if there is at least one pixel of which any of RGB channels has
      // higher value than alpha channel, the source image can be jusged
      // as NON-premultiplied.
      if (isPremultiplied &&
          ((*chann_p).x > (*chann_p).w || (*chann_p).y > (*chann_p).w ||
           (*chann_p).z > (*chann_p).w))
        isPremultiplied = false;
    }
  }
  return isPremultiplied;
}

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
      float val = ((float)pix->r * 0.3f + (float)pix->g * 0.59f +
                   (float)pix->b * 0.11f) /
                  (float)PIXEL::maxChannelValue;
      // convert to unsigned char
      (*depth_p) = (unsigned char)(val * (float)UCHAR_MAX + 0.5f);
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
      float val = (float)pix->value / (float)PIXEL::maxChannelValue;
      // convert to unsigned char
      (*depth_p) = (unsigned char)(val * (float)UCHAR_MAX + 0.5f);
    }
  }
}

//------------------------------------------------------------
// create the depth index map based on the histogram
//------------------------------------------------------------
void Iwa_BokehRefFx::defineSegemntDepth(
    const unsigned char* indexMap_main, const unsigned char* indexMap_sub,
    const float* mainSub_ratio, const unsigned char* depth_buff,
    const TDimensionI& dimOut, const double frame,
    QVector<float>& segmentDepth_main, QVector<float>& segmentDepth_sub) {
  QSet<int> segmentValues;

  // histogram parameters
  struct HISTO {
    int pix_amount;
    int belongingSegmentValue;  // value to which temporary segmented
    int segmentId;
    int segmentId_sub;
  } histo[256];

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

  // focus depth becomes the segment layer as well
  int focusVal = (int)(m_onFocusDistance->getValue(frame) * (double)UCHAR_MAX);
  if (minHisto < focusVal && focusVal < maxHisto)
    segmentValues.insert(focusVal);

  // set the initial segmentation for each depth value
  for (int h = 0; h < 256; h++) {
    for (int seg = 0; seg < segmentValues.size(); seg++) {
      // set the segment
      if (histo[h].belongingSegmentValue == -1) {
        histo[h].belongingSegmentValue = segmentValues.values().at(seg);
        continue;
      }
      // error amount at the current registered layers
      int tmpError = abs(h - histo[h].belongingSegmentValue);
      if (tmpError == 0) break;
      // new error amount
      int newError = abs(h - segmentValues.values().at(seg));
      // compare the two and update
      if (newError < tmpError)
        histo[h].belongingSegmentValue = segmentValues.values().at(seg);
    }
  }

  // add the segment layers to the distance precision value
  while (segmentValues.size() < m_distancePrecision->getValue()) {
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
        if (abs(i - histo[i].belongingSegmentValue) >
            abs(i - h))  // the current segment value has
                         // proirity, if the distance is the same
          errorModAmount +=
              (abs(i - histo[i].belongingSegmentValue) - abs(i - h)) *
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
    // std::cout << "insert  " << tmpBestNewSegVal << std::endl;

    // update belongingSegmentValue
    for (int h = minHisto + 1; h < maxHisto; h++) {
      // compare the current segment value and h and take the nearest value
      // if tmpBestNewSegVal is near (from h), then update the
      // belongingSegmentValue
      if (abs(h - histo[h].belongingSegmentValue) >
          abs(h - tmpBestNewSegVal))  // the current segment value has
                                      // proirity, if the distance is the same
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
      segmentDepth_main.push_back((float)histo[h].belongingSegmentValue /
                                  (float)UCHAR_MAX);
      tmpSegVal = histo[h].belongingSegmentValue;
      tmpSegId++;
      segValVec.push_back(tmpSegVal);
    }
    histo[h].segmentId = tmpSegId;
  }

  // "sub" depth segment value list for interporation
  for (int d = 0; d < segmentDepth_main.size() - 1; d++)
    segmentDepth_sub.push_back(
        (segmentDepth_main.at(d) + segmentDepth_main.at(d + 1)) / 2.0f);

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
  float* ratio_p = (float*)mainSub_ratio;
  for (int i = 0; i < size; i++, depth_p++, main_p++, sub_p++, ratio_p++) {
    *main_p = (unsigned char)histo[(int)*depth_p].segmentId;
    *sub_p  = (unsigned char)histo[(int)*depth_p].segmentId_sub;

    float depth         = (float)*depth_p / (float)UCHAR_MAX;
    float main_segDepth = segmentDepth_main.at(*main_p);
    float sub_segDepth  = segmentDepth_sub.at(*sub_p);

    if (main_segDepth == sub_segDepth)
      *ratio_p = 1.0f;
    else {
      *ratio_p =
          1.0f - (main_segDepth - depth) / (main_segDepth - sub_segDepth);
      if (*ratio_p > 1.0f) *ratio_p = 1.0f;
      if (*ratio_p < 0.0f) *ratio_p = 0.0f;
    }
  }
}

//------------------------------------------------------------
// set the result
//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_BokehRefFx::setOutputRaster(float4* srcMem, const RASTER dstRas,
                                     TDimensionI dim, TDimensionI margin) {
  int out_j = 0;
  for (int j = margin.ly; j < dstRas->getLy() + margin.ly; j++, out_j++) {
    PIXEL* pix     = dstRas->pixels(out_j);
    float4* chan_p = srcMem;
    chan_p += j * dim.lx + margin.lx;
    for (int i = 0; i < dstRas->getLx(); i++, pix++, chan_p++) {
      float val;
      val    = (*chan_p).x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).w * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
    }
  }
}

//------------------------------------------------------------
// obtain iris size from the depth value
//------------------------------------------------------------
inline float Iwa_BokehRefFx::calcIrisSize(const float depth,
                                          const float bokehPixelAmount,
                                          const double onFocusDistance) {
  return ((float)onFocusDistance - depth) * bokehPixelAmount;
}

//--------------------------------------------
// resize/invert the iris according to the size ratio
// normalize the brightness
// resize to the output size
//--------------------------------------------
void Iwa_BokehRefFx::convertIris(const float irisSize, const TRectD& irisBBox,
                                 const TTile& irisTile,
                                 const TDimensionI& dimOut,
                                 kiss_fft_cpx* fftcpx_iris_before) {
  // original size of the iris image
  TDimensionD irisOrgSize = irisBBox.getSize();

  // obtain size ratio based on width
  double irisSizeResampleRatio = irisSize / irisOrgSize.lx;

  // create the raster for resizing
  TDimensionD resizedIrisSize(abs(irisSizeResampleRatio) * irisOrgSize.lx,
                              abs(irisSizeResampleRatio) * irisOrgSize.ly);
  TDimensionI filterSize((int)std::ceil(resizedIrisSize.lx),
                         (int)std::ceil(resizedIrisSize.ly));
  TPointD resizeOffset((double)filterSize.lx - resizedIrisSize.lx,
                       (double)filterSize.ly - resizedIrisSize.ly);

  bool isIrisOffset[2] = {false, false};
  // iris shape must be exactly at the center of the image
  if ((dimOut.lx - filterSize.lx) % 2 == 1) {
    filterSize.lx++;
    isIrisOffset[0] = true;
  }
  if ((dimOut.ly - filterSize.ly) % 2 == 1) {
    filterSize.ly++;
    isIrisOffset[1] = true;
  }

  // if the filter size becomes larger than the output size, return
  if (filterSize.lx > dimOut.lx || filterSize.ly > dimOut.ly) {
    std::cout
        << "Error: The iris filter size becomes larger than the source size!"
        << std::endl;
    return;
  }

  TRaster64P resizedIris(filterSize);

  // offset
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

  // sum of the value
  float irisValAmount = 0.0;

  int iris_j = 0;
  // initialize
  for (int i = 0; i < dimOut.lx * dimOut.ly; i++) {
    fftcpx_iris_before[i].r = 0.0;
    fftcpx_iris_before[i].i = 0.0;
  }
  for (int j = (dimOut.ly - filterSize.ly) / 2; iris_j < filterSize.ly;
       j++, iris_j++) {
    TPixel64* pix = resizedIris->pixels(iris_j);
    int iris_i    = 0;
    for (int i = (dimOut.lx - filterSize.lx) / 2; iris_i < filterSize.lx;
         i++, iris_i++) {
      // Value = 0.3R 0.59G 0.11B
      fftcpx_iris_before[j * dimOut.lx + i].r =
          ((float)pix->r * 0.3f + (float)pix->g * 0.59f +
           (float)pix->b * 0.11f) /
          (float)USHRT_MAX;
      irisValAmount += fftcpx_iris_before[j * dimOut.lx + i].r;
      pix++;
    }
  }

  // Normalize value
  for (int i = 0; i < dimOut.lx * dimOut.ly; i++)
    fftcpx_iris_before[i].r /= irisValAmount;
}

//--------------------------------------------
// convert source image value rgb -> exposure
//--------------------------------------------
void Iwa_BokehRefFx::convertRGBToExposure(const float4* source_buff, int size,
                                          float filmGamma,
                                          bool sourceIsPremultiplied) {
  float4* source_p = (float4*)source_buff;
  for (int i = 0; i < size; i++, source_p++) {
    // continue if alpha channel is 0
    if ((*source_p).w == 0.0f) {
      (*source_p).x = 0.0f;
      (*source_p).y = 0.0f;
      (*source_p).z = 0.0f;
      continue;
    }

    // unmultiply for premultiplied source
    // this will not be applied to non-premultiplied image such as "DigiBook"
    // (digital overlay)
    if (sourceIsPremultiplied) {
      // unpremultiply
      (*source_p).x /= (*source_p).w;
      (*source_p).y /= (*source_p).w;
      (*source_p).z /= (*source_p).w;
    }

    // RGB value -> exposure
    (*source_p).x = pow(10, ((*source_p).x - 0.5f) / filmGamma);
    (*source_p).y = pow(10, ((*source_p).y - 0.5f) / filmGamma);
    (*source_p).z = pow(10, ((*source_p).z - 0.5f) / filmGamma);

    // multiply with alpha channel
    (*source_p).x *= (*source_p).w;
    (*source_p).y *= (*source_p).w;
    (*source_p).z *= (*source_p).w;
  }
}

//--------------------------------------------
// generate the segment layer source at the current depth
// considering fillGap and doMedian options
//--------------------------------------------
void Iwa_BokehRefFx::retrieveLayer(const float4* source_buff,
                                   const float4* segment_layer_buff,
                                   const unsigned char* indexMap_mainSub,
                                   int index, int lx, int ly, bool fillGap,
                                   bool doMedian, int margin) {
  // only when fillGap is ON and doMedian is OFF,
  // fill the region where will be behind of the near layers
  bool fill = (fillGap && !doMedian);
  // retrieve the regions with the current depth
  float4* source_p          = (float4*)source_buff;
  float4* layer_p           = (float4*)segment_layer_buff;
  unsigned char* indexMap_p = (unsigned char*)indexMap_mainSub;
  for (int i = 0; i < lx * ly; i++, source_p++, layer_p++, indexMap_p++) {
    // continue if the current pixel is at the far layer
    // consider the fill flag if the current pixel is at the near layer
    if ((int)(*indexMap_p) < index || (!fill && (int)(*indexMap_p) > index))
      continue;

    // copy pixel values
    (*layer_p).x = (*source_p).x;
    (*layer_p).y = (*source_p).y;
    (*layer_p).z = (*source_p).z;
    (*layer_p).w = (*source_p).w;
  }

  if (!fillGap || !doMedian) return;
  if (margin == 0) return;

  // extend pixels by using median filter
  unsigned char* generation_buff;
  TRasterGR8P generation_buff_ras = allocateRasterAndLock<unsigned char>(
      &generation_buff, TDimensionI(lx, ly));

  for (int gen = 0; gen < margin; gen++) {
    // apply single median filter
    doSingleMedian(source_buff, segment_layer_buff, indexMap_mainSub, index, lx,
                   ly, generation_buff, gen + 1);
  }

  generation_buff_ras->unlock();
}

//--------------------------------------------
// apply single median filter
//--------------------------------------------
void Iwa_BokehRefFx::doSingleMedian(const float4* source_buff,
                                    const float4* segment_layer_buff,
                                    const unsigned char* indexMap_mainSub,
                                    int index, int lx, int ly,
                                    const unsigned char* generation_buff,
                                    int curGen) {
  float4* source_p          = (float4*)source_buff;
  float4* layer_p           = (float4*)segment_layer_buff;
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
      float4 neighbor[8];
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
          float brightness = source_buff[neighborId].x * 0.3f +
                             source_buff[neighborId].y * 0.59f +
                             source_buff[neighborId].z * 0.11f;
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
                          ? (int)std::floor((float)(neighbor_amount - 1) / 2.0f)
                          : (int)std::ceil((float)(neighbor_amount - 1) / 2.0f);

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

//--------------------------------------------
// normal-composite the layer as is, without filtering
//--------------------------------------------
void Iwa_BokehRefFx::compositeAsIs(const float4* segment_layer_buff,
                                   const float4* result_buff_mainSub,
                                   int size) {
  float4* layer_p  = (float4*)segment_layer_buff;
  float4* result_p = (float4*)result_buff_mainSub;
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
      (*result_p).x = (*layer_p).x + (*result_p).x * (1.0f - (*layer_p).w);
      (*result_p).y = (*layer_p).y + (*result_p).y * (1.0f - (*layer_p).w);
      (*result_p).z = (*layer_p).z + (*result_p).z * (1.0f - (*layer_p).w);
      (*result_p).w = (*layer_p).w + (*result_p).w * (1.0f - (*layer_p).w);
    }
  }
}

//--------------------------------------------
// retrieve segment layer image for each channel
//--------------------------------------------
void Iwa_BokehRefFx::retrieveChannel(const float4* segment_layer_buff,  // src
                                     kiss_fft_cpx* fftcpx_r_before,     // dst
                                     kiss_fft_cpx* fftcpx_g_before,     // dst
                                     kiss_fft_cpx* fftcpx_b_before,     // dst
                                     kiss_fft_cpx* fftcpx_a_before,     // dst
                                     int size) {
  float4* layer_p = (float4*)segment_layer_buff;
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
void Iwa_BokehRefFx::multiplyFilter(kiss_fft_cpx* fftcpx_channel,  // dst
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
void Iwa_BokehRefFx::compositeAlpha(const float4* result_buff,         // dst
                                    const kiss_fft_cpx* fftcpx_alpha,  // alpha
                                    int lx, int ly) {
  int size         = lx * ly;
  float4* result_p = (float4*)result_buff;
  for (int i = 0; i < size; i++, result_p++) {
    // modify fft coordinate to normal
    float alpha = fftcpx_alpha[getCoord(i, lx, ly)].r / (float)size;

    if ((*result_p).w < 1.0f) {
      if (alpha >= 1.0f)
        (*result_p).w = 1.0f;
      else
        (*result_p).w = alpha + ((*result_p).w * (1.0f - alpha));
    }
  }
}

//--------------------------------------------
// interpolate main and sub exposures
// convert exposure -> value (0-1)
// set to the result
//--------------------------------------------
void Iwa_BokehRefFx::interpolateExposureAndConvertToRGB(
    const float4* result_main_buff,  // result1
    const float4* result_sub_buff,   // result2
    const float* mainSub_ratio,      // ratio
    float filmGamma,
    const float4* source_buff,  // dst
    int size) {
  float4* resultMain_p = (float4*)result_main_buff;
  float4* resultSub_p  = (float4*)result_sub_buff;
  float* ratio_p       = (float*)mainSub_ratio;
  float4* out_p        = (float4*)source_buff;
  for (int i = 0; i < size;
       i++, resultMain_p++, resultSub_p++, ratio_p++, out_p++) {
    // interpolate main and sub exposures
    float4 result;

    result.x =
        (*resultMain_p).x * (*ratio_p) + (*resultSub_p).x * (1.0f - (*ratio_p));
    result.y =
        (*resultMain_p).y * (*ratio_p) + (*resultSub_p).y * (1.0f - (*ratio_p));
    result.z =
        (*resultMain_p).z * (*ratio_p) + (*resultSub_p).z * (1.0f - (*ratio_p));
    result.w =
        (*resultMain_p).w * (*ratio_p) + (*resultSub_p).w * (1.0f - (*ratio_p));

    (*out_p).w = result.w;

    // convert exposure -> value (0-1)

    // continue for transparent pixel
    if (result.w == 0.0f) {
      (*out_p).x = 0.0f;
      (*out_p).y = 0.0f;
      (*out_p).z = 0.0f;
      continue;
    }

    // convert Exposure to value
    result.x = log10(result.x) * filmGamma + 0.5f;
    result.y = log10(result.y) * filmGamma + 0.5f;
    result.z = log10(result.z) * filmGamma + 0.5f;

    (*out_p).x =
        (result.x > 1.0f) ? 1.0f : ((result.x < 0.0f) ? 0.0f : result.x);
    (*out_p).y =
        (result.y > 1.0f) ? 1.0f : ((result.y < 0.0f) ? 0.0f : result.y);
    (*out_p).z =
        (result.z > 1.0f) ? 1.0f : ((result.z < 0.0f) ? 0.0f : result.z);
  }
}

//--------------------------------------------

Iwa_BokehRefFx::Iwa_BokehRefFx()
    : m_onFocusDistance(0.5)
    , m_bokehAmount(30.0)
    , m_hardness(0.3)
    , m_distancePrecision(10)
    , m_fillGap(true)
    , m_doMedian(true) {
  // Bind parameters
  addInputPort("Iris", m_iris);
  addInputPort("Source", m_source);
  addInputPort("Depth", m_depth);

  bindParam(this, "on_focus_distance", m_onFocusDistance, false);
  bindParam(this, "bokeh_amount", m_bokehAmount, false);
  bindParam(this, "hardness", m_hardness, false);
  bindParam(this, "distance_precision", m_distancePrecision, false);
  bindParam(this, "fill_gap", m_fillGap, false);
  bindParam(this, "fill_gap_with_median_filter", m_doMedian, false);

  // Set the ranges of parameters
  m_onFocusDistance->setValueRange(0, 1);
  m_bokehAmount->setValueRange(0, 300);
  m_bokehAmount->setMeasureName("fxLength");
  m_hardness->setValueRange(0.05, 20.0);
  m_distancePrecision->setValueRange(3, 128);
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
  float bokehPixelAmount = getBokehPixelAmount(frame, settings.m_affine);

  // Obtain the larger size of bokeh between the nearest (black) point and
  // the farthest (white) point, based on the focus distance.
  double onFocusDistance = m_onFocusDistance->getValue(frame);
  float maxIrisSize =
      bokehPixelAmount * std::max((1.0 - onFocusDistance), onFocusDistance);

  int margin =
      (maxIrisSize > 1.0f) ? (int)(std::ceil((maxIrisSize - 1.0f) / 2.0f)) : 0;

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

    rectOut = rectOut.enlarge(static_cast<double>(new_x - dimOut.lx) / 2.0,
                              static_cast<double>(new_y - dimOut.ly) / 2.0);

    dimOut.lx = new_x;
    dimOut.ly = new_y;
  }

  // - - - Compute the input tiles - - -

  // Automatically judge whether the source image is premultiplied or not
  bool isPremultiplied;

  // source image buffer
  float4* source_buff;
  rasterList.append(allocateRasterAndLock<float4>(&source_buff, dimOut));

  {
    // source tile is used only in this focus.
    // normalized source image data is stored in source_buff.
    TTile sourceTile;
    m_source->allocateAndCompute(sourceTile, rectOut.getP00(), dimOut,
                                 tile.getRaster(), frame, settings);
    // normalize the tile image to 0-1 and set to source_buff
    TRaster32P ras32 = (TRaster32P)sourceTile.getRaster();
    TRaster64P ras64 = (TRaster64P)sourceTile.getRaster();
    lock.lockForRead();
    if (ras32)
      isPremultiplied =
          setSourceRaster<TRaster32P, TPixel32>(ras32, source_buff, dimOut);
    else if (ras64)
      isPremultiplied =
          setSourceRaster<TRaster64P, TPixel64>(ras64, source_buff, dimOut);
    lock.unlock();
  }

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRasters(rasterList);
    tile.getRaster()->clear();
    return;
  }

  // create the index map, which indicates which layer each pixel belongs to
  // make two separations and interporate the results in order to avoid
  // artifacts appear at the layer border
  unsigned char* indexMap_main;
  unsigned char* indexMap_sub;
  rasterList.append(
      allocateRasterAndLock<unsigned char>(&indexMap_main, dimOut));
  rasterList.append(
      allocateRasterAndLock<unsigned char>(&indexMap_sub, dimOut));

  // interporation ratio between two results
  float* mainSub_ratio;
  rasterList.append(allocateRasterAndLock<float>(&mainSub_ratio, dimOut));

  // - - - depth segmentation - - -
  QVector<float> segmentDepth_main;
  QVector<float> segmentDepth_sub;
  {
    // depth image stored in 256 levels
    unsigned char* depth_buff;
    TRasterGR8P depth_buff_ras =
        allocateRasterAndLock<unsigned char>(&depth_buff, dimOut);
    {
      TTile depthTile;
      m_depth->allocateAndCompute(depthTile, rectOut.getP00(), dimOut,
                                  tile.getRaster(), frame, settings);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRasters(rasterList);
        depth_buff_ras->unlock();
        tile.getRaster()->clear();
        return;
      }

      // normalize brightness of the depth reference image to unsigned char
      // and store into depth_buff
      TRasterGR8P rasGR8   = (TRasterGR8P)depthTile.getRaster();
      TRasterGR16P rasGR16 = (TRasterGR16P)depthTile.getRaster();
      TRaster32P ras32     = (TRaster32P)depthTile.getRaster();
      TRaster64P ras64     = (TRaster64P)depthTile.getRaster();
      lock.lockForRead();
      if (rasGR8)
        setDepthRasterGray<TRasterGR8P, TPixelGR8>(rasGR8, depth_buff, dimOut);
      else if (rasGR16)
        setDepthRasterGray<TRasterGR16P, TPixelGR16>(rasGR16, depth_buff,
                                                     dimOut);
      else if (ras32)
        setDepthRaster<TRaster32P, TPixel32>(ras32, depth_buff, dimOut);
      else if (ras64)
        setDepthRaster<TRaster64P, TPixel64>(ras64, depth_buff, dimOut);
      lock.unlock();
    }
    // create the depth index map
    defineSegemntDepth(indexMap_main, indexMap_sub, mainSub_ratio, depth_buff,
                       dimOut, frame, segmentDepth_main, segmentDepth_sub);
    // depth image is not needed anymore. release it
    depth_buff_ras->unlock();
  }

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRasters(rasterList);
    tile.getRaster()->clear();
    return;
  }

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

  // for now only CPU computation is supported
  // when introduced GPGPU capability, computation will be separated here
  doCompute_CPU(frame, settings, bokehPixelAmount, maxIrisSize, margin, dimOut,
                source_buff, indexMap_main, indexMap_sub, mainSub_ratio,
                segmentDepth_main, segmentDepth_sub, irisTile, irisBBox,
                isPremultiplied);

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRasters(rasterList);
    tile.getRaster()->clear();
    return;
  }

  TDimensionI actualMargin((dimOut.lx - tile.getRaster()->getSize().lx) / 2,
                           (dimOut.ly - tile.getRaster()->getSize().ly) / 2);
  // clear the tile raster
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  lock.lockForWrite();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(source_buff, outRas32, dimOut,
                                          actualMargin);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(source_buff, outRas64, dimOut,
                                          actualMargin);
  lock.unlock();

  // release all rasters
  releaseAllRasters(rasterList);
}

//--------------------------------------------

void Iwa_BokehRefFx::doCompute_CPU(
    const double frame, const TRenderSettings& settings, float bokehPixelAmount,
    float maxIrisSize, int margin, TDimensionI& dimOut, float4* source_buff,
    unsigned char* indexMap_main, unsigned char* indexMap_sub,
    float* mainSub_ratio, QVector<float>& segmentDepth_main,
    QVector<float>& segmentDepth_sub, TTile& irisTile, TRectD& irisBBox,
    bool sourceIsPremultiplied) {
  QList<TRasterGR8P> rasterList;
  QList<kiss_fftnd_cfg> planList;

  // This fx is relatively heavy so the multi thread computation is introduced.
  // Lock the mutex here in order to prevent multiple rendering tasks run at the
  // same time.
  QMutexLocker fx_locker(&fx_mutex);

  // - - - memory allocation for FFT - - -

  // iris image
  kiss_fft_cpx* fftcpx_iris_before;
  kiss_fft_cpx* fftcpx_iris;
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_iris_before, dimOut));
  rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_iris, dimOut));

  // segment layers
  float4* segment_layer_buff;
  rasterList.append(allocateRasterAndLock<float4>(&segment_layer_buff, dimOut));

  // alpha channel
  kiss_fft_cpx* fftcpx_alpha_before;
  kiss_fft_cpx* fftcpx_alpha;
  rasterList.append(
      allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_alpha_before, dimOut));
  rasterList.append(allocateRasterAndLock<kiss_fft_cpx>(&fftcpx_alpha, dimOut));

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRasters(rasterList);
    return;
  }

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
  float4* result_main_buff;
  float4* result_sub_buff;
  rasterList.append(allocateRasterAndLock<float4>(&result_main_buff, dimOut));
  rasterList.append(allocateRasterAndLock<float4>(&result_sub_buff, dimOut));

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRasters(rasterList);
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

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRastersAndPlans(rasterList, planList);
    return;
  }

  int size = dimOut.lx * dimOut.ly;

  // initialize result memory
  memset(result_main_buff, 0, sizeof(float4) * size);
  memset(result_sub_buff, 0, sizeof(float4) * size);

  // obtain parameters
  float filmGamma = (float)m_hardness->getValue(frame);
  bool fillGap    = m_fillGap->getValue();
  bool doMedian   = m_doMedian->getValue();

  // convert source image value rgb -> exposure
  convertRGBToExposure(source_buff, size, filmGamma, sourceIsPremultiplied);

  // compute twice (main and sub) for interpolation
  for (int mainSub = 0; mainSub < 2; mainSub++) {
    // cancel check
    if (settings.m_isCanceled && *settings.m_isCanceled) {
      releaseAllRastersAndPlans(rasterList, planList);
      return;
    }

    float4* result_buff_mainSub;
    QVector<float> segmentDepth_mainSub;
    unsigned char* indexMap_mainSub;
    if (mainSub == 0)  // main process
    {
      result_buff_mainSub  = result_main_buff;
      segmentDepth_mainSub = segmentDepth_main;
      indexMap_mainSub     = indexMap_main;
    } else  // sub process
    {
      result_buff_mainSub  = result_sub_buff;
      segmentDepth_mainSub = segmentDepth_sub;
      indexMap_mainSub     = indexMap_sub;
    }

    // Compute from the most distant segment layer
    for (int index = 0; index < segmentDepth_mainSub.size(); index++) {
      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // obtain iris size from the depth value
      float irisSize =
          calcIrisSize(segmentDepth_mainSub.at(index), bokehPixelAmount,
                       m_onFocusDistance->getValue(frame));

      // initialize the layer memory
      memset(segment_layer_buff, 0, sizeof(float4) * size);
      // generate the segment layer source at the current depth
      // considering fillGap and doMedian options
      retrieveLayer(source_buff, segment_layer_buff, indexMap_mainSub, index,
                    dimOut.lx, dimOut.ly, fillGap, doMedian,
                    (index == segmentDepth_mainSub.size() - 1) ? 0 : margin);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // if the layer is at (almost) the focus position
      if (-1.0 <= irisSize && 1.0 >= irisSize) {
        // normal-composite the layer as is, without filtering
        compositeAsIs(segment_layer_buff, result_buff_mainSub, size);
        // continue to next layer
        continue;
      }

      // resize the iris image

      // resize/invert the iris according to the size ratio
      // normalize the brightness
      // resize to the output size
      convertIris(irisSize, irisBBox, irisTile, dimOut, fftcpx_iris_before);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // Do FFT the iris image.
      kiss_fftnd(kissfft_plan_fwd, fftcpx_iris_before, fftcpx_iris);
      // fftwf_execute_dft(fftw_plan_fwd_r, iris_host, iris_host);

      // initialize alpha
      memset(fftcpx_alpha_before, 0, sizeof(kiss_fft_cpx) * size);
      // initialize channels
      memset(fftcpx_r_before, 0, sizeof(kiss_fft_cpx) * size);
      memset(fftcpx_g_before, 0, sizeof(kiss_fft_cpx) * size);
      memset(fftcpx_b_before, 0, sizeof(kiss_fft_cpx) * size);

      // retrieve segment layer image for each channel
      retrieveChannel(segment_layer_buff,   // src
                      fftcpx_r_before,      // dst
                      fftcpx_g_before,      // dst
                      fftcpx_b_before,      // dst
                      fftcpx_alpha_before,  // dst
                      size);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // forward fft of alpha channel
      kiss_fftnd(kissfft_plan_fwd, fftcpx_alpha_before, fftcpx_alpha);
      // fftwf_execute_dft(fftw_plan_fwd_r, alphaBokeh_host, alphaBokeh_host);

      // multiply filter on alpha
      multiplyFilter(fftcpx_alpha,  // dst
                     fftcpx_iris,   // filter
                     size);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // inverse fft the alpha channel
      // note that the result is multiplied by the image size
      kiss_fftnd(kissfft_plan_bkwd, fftcpx_alpha, fftcpx_alpha_before);
      // fftwf_execute_dft(fftw_plan_bkwd_r, alphaBokeh_host, alphaBokeh_host);

      // normal composite the alpha channel
      compositeAlpha(result_buff_mainSub,  // dst
                     fftcpx_alpha_before,  // alpha
                     dimOut.lx, dimOut.ly);

      // cancel check
      if (settings.m_isCanceled && *settings.m_isCanceled) {
        releaseAllRastersAndPlans(rasterList, planList);
        return;
      }

      // create worker threads
      BokehRefThread threadR(0, fftcpx_r_before, fftcpx_r, fftcpx_alpha_before,
                             fftcpx_iris, result_buff_mainSub,
                             kissfft_plan_r_fwd, kissfft_plan_r_bkwd, dimOut);
      BokehRefThread threadG(1, fftcpx_g_before, fftcpx_g, fftcpx_alpha_before,
                             fftcpx_iris, result_buff_mainSub,
                             kissfft_plan_g_fwd, kissfft_plan_g_bkwd, dimOut);
      BokehRefThread threadB(2, fftcpx_b_before, fftcpx_b, fftcpx_alpha_before,
                             fftcpx_iris, result_buff_mainSub,
                             kissfft_plan_b_fwd, kissfft_plan_b_bkwd, dimOut);

      // If you set this flag to true, the fx will be forced to compute in
      // single
      // thread.
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

    }  // for each layer
  }    // for main and sub

  // cancel check
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    releaseAllRastersAndPlans(rasterList, planList);
    return;
  }

  // interpolate main and sub exposures
  // convert exposure -> RGB (0-1)
  // set to the result
  interpolateExposureAndConvertToRGB(result_main_buff,  // result1
                                     result_sub_buff,   // result2
                                     mainSub_ratio,     // ratio
                                     filmGamma,
                                     source_buff,  // dst
                                     size);

  // release rasters and plans
  releaseAllRastersAndPlans(rasterList, planList);
}

//--------------------------------------------

bool Iwa_BokehRefFx::doGetBBox(double frame, TRectD& bBox,
                               const TRenderSettings& info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//--------------------------------------------

bool Iwa_BokehRefFx::canHandle(const TRenderSettings& info, double frame) {
  return false;
}

FX_PLUGIN_IDENTIFIER(Iwa_BokehRefFx, "iwa_BokehRefFx")