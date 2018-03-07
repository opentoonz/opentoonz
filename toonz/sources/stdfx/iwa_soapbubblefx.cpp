/*------------------------------------
Iwa_SoapBubbleFx
Generates thin film interference colors from two reference images;
one is for thickness and the other one is for shape or normal vector
distribution of the film.
Inherits Iwa_SpectrumFx.
------------------------------------*/

#include "iwa_soapbubblefx.h"
#include "iwa_cie_d65.h"
#include "iwa_xyz.h"

#include "trop.h"

#include <QList>
#include <QPoint>
#include <QSize>
#include <QRect>

namespace {
const float PI = 3.14159265f;

#define INF 1e20 /* less than FLT_MAX */

/* dt of 1d function using squared distance */
static float* dt(float* f, int n, float a = 1.0f) {
  float* d = new float[n];
  int* v   = new int[n];
  float* z = new float[n + 1];
  /* index of rightmost parabola in lower envelope */
  int k = 0;
  /* locations of parabolas in lower envelope */
  v[0] = 0;
  /* locations of boundaries between parabolas */
  z[0] = -INF;
  z[1] = +INF;
  /* compute lower envelope */
  for (int q = 1; q <= n - 1; q++) {
    /* compute intersection */
    float s =
        ((f[q] / a + q * q) - (f[v[k]] / a + v[k] * v[k])) / (2 * q - 2 * v[k]);
    while (s <= z[k]) {
      k--;
      s = ((f[q] / a + q * q) - (f[v[k]] / a + v[k] * v[k])) /
          (2 * q - 2 * v[k]);
    }
    k++;
    v[k]     = q;
    z[k]     = s;
    z[k + 1] = +INF;
  }
  k = 0;
  /* fill in values of distance transform */
  for (int q = 0; q <= n - 1; q++) {
    while (z[k + 1] < q) k++;
    d[q] = a * (q - v[k]) * (q - v[k]) + f[v[k]];
  }

  delete[] v;
  delete[] z;
  return d;
}
}

//------------------------------------

Iwa_SoapBubbleFx::Iwa_SoapBubbleFx()
    : Iwa_SpectrumFx()
    , m_renderMode(new TIntEnumParam(RENDER_MODE_BUBBLE, "Bubble"))
    , m_binarize_threshold(0.5)
    , m_shape_aspect_ratio(1.0)
    , m_blur_radius(5.0)
    , m_blur_power(0.5)
    , m_multi_source(false)
    , m_mask_center(false)  // obsolete
    , m_center_opacity(1.0)
    , m_fit_thickness(false)
    , m_normal_sample_distance(1)
    , m_noise_sub_depth(3)
    , m_noise_resolution_s(18.0)
    , m_noise_resolution_t(5.0)
    , m_noise_sub_composite_ratio(0.5)
    , m_noise_evolution(0.0)
    , m_noise_depth_mix_ratio(0.05)
    , m_noise_thickness_mix_ratio(0.05) {
  removeInputPort("Source");
  removeInputPort("Light"); /* not used */
  addInputPort("Thickness", m_input);
  addInputPort("Shape", m_shape);
  addInputPort("Depth", m_depth);

  bindParam(this, "renderMode", m_renderMode);
  m_renderMode->addItem(RENDER_MODE_THICKNESS, "Thickness");
  m_renderMode->addItem(RENDER_MODE_DEPTH, "Depth");

  bindParam(this, "binarizeThresold", m_binarize_threshold);
  bindParam(this, "shapeAspectRatio", m_shape_aspect_ratio);
  bindParam(this, "blurRadius", m_blur_radius);
  bindParam(this, "blurPower", m_blur_power);
  bindParam(this, "multiSource", m_multi_source);
  bindParam(this, "maskCenter", m_mask_center, false, true);  // obsolete
  bindParam(this, "centerOpacity", m_center_opacity);
  bindParam(this, "fitThickness", m_fit_thickness);
  bindParam(this, "normalSampleDistance", m_normal_sample_distance);
  bindParam(this, "noiseSubDepth", m_noise_sub_depth);
  bindParam(this, "noiseResolutionS", m_noise_resolution_s);
  bindParam(this, "noiseResolutionT", m_noise_resolution_t);
  bindParam(this, "noiseSubCompositeRatio", m_noise_sub_composite_ratio);
  bindParam(this, "noiseEvolution", m_noise_evolution);
  bindParam(this, "noiseDepthMixRatio", m_noise_depth_mix_ratio);
  bindParam(this, "noiseThicknessMixRatio", m_noise_thickness_mix_ratio);

  m_binarize_threshold->setValueRange(0.01, 0.99);
  m_shape_aspect_ratio->setValueRange(0.2, 5.0);
  m_blur_radius->setMeasureName("fxLength");
  m_blur_radius->setValueRange(0.0, 25.0);
  m_blur_power->setValueRange(0.01, 5.0);
  m_center_opacity->setValueRange(0.0, 1.0);

  m_normal_sample_distance->setValueRange(1, 20);
  m_noise_sub_depth->setValueRange(1, 5);
  m_noise_resolution_s->setValueRange(1.0, 40.0);
  m_noise_resolution_t->setValueRange(1.0, 20.0);
  m_noise_sub_composite_ratio->setValueRange(0.0, 5.0);
  m_noise_depth_mix_ratio->setValueRange(0.0, 1.0);
  m_noise_thickness_mix_ratio->setValueRange(0.0, 1.0);
}

//------------------------------------

void Iwa_SoapBubbleFx::doCompute(TTile& tile, double frame,
                                 const TRenderSettings& settings) {
  if (!m_input.isConnected()) return;
  if (!m_shape.isConnected() && !m_depth.isConnected()) return;

  TDimensionI dim(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TRectD bBox(tile.m_pos, TPointD(dim.lx, dim.ly));
  QList<TRasterGR8P> allocatedRasList;

  if (m_renderMode->getValue() == RENDER_MODE_DEPTH && m_depth.isConnected()) {
    m_depth->allocateAndCompute(tile, bBox.getP00(), dim, tile.getRaster(),
                                frame, settings);
    return;
  }

  /* soap bubble color map */
  TRasterGR8P bubbleColor_ras(sizeof(float3) * 256 * 256, 1);
  bubbleColor_ras->lock();
  allocatedRasList.append(bubbleColor_ras);
  float3* bubbleColor_p = (float3*)bubbleColor_ras->getRawData();
  if (m_renderMode->getValue() == RENDER_MODE_BUBBLE)
    calcBubbleMap(bubbleColor_p, frame, true);

  if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

  /* depth map */
  TRasterGR8P depth_map_ras(sizeof(float) * dim.lx * dim.ly, 1);
  depth_map_ras->lock();
  allocatedRasList.append(depth_map_ras);
  float* depth_map_p = (float*)depth_map_ras->getRawData();

  /* alpha map */
  TRasterGR8P alpha_map_ras(sizeof(float) * dim.lx * dim.ly, 1);
  alpha_map_ras->lock();
  allocatedRasList.append(alpha_map_ras);
  float* alpha_map_p = (float*)alpha_map_ras->getRawData();

  /* region indices */
  TRasterGR8P regionIds_ras(sizeof(USHORT) * dim.lx * dim.ly, 1);
  regionIds_ras->lock();
  regionIds_ras->clear();
  allocatedRasList.append(regionIds_ras);
  USHORT* regionIds_p = (USHORT*)regionIds_ras->getRawData();
  QList<QRect> regionBoundingRects;

  /* if the depth image is connected, use it */
  if (m_depth.isConnected()) {
    TTile depth_tile;
    m_depth->allocateAndCompute(depth_tile, bBox.getP00(), dim,
                                tile.getRaster(), frame, settings);

    if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

    TRasterP depthRas = depth_tile.getRaster();
    depthRas->lock();

    TRaster32P depthRas32 = (TRaster32P)depthRas;
    TRaster64P depthRas64 = (TRaster64P)depthRas;
    {
      if (depthRas32)
        convertToBrightness<TRaster32P, TPixel32>(depthRas32, depth_map_p,
                                                  alpha_map_p, dim);
      else if (depthRas64)
        convertToBrightness<TRaster64P, TPixel64>(depthRas64, depth_map_p,
                                                  alpha_map_p, dim);
    }
    depthRas->unlock();

    // set one region covering whole camera rect
    regionBoundingRects.append(QRect(0, 0, dim.lx, dim.ly));
  }
  /* or, use the shape image to obtain pseudo depth */
  else { /* m_shape.isConnected */
    /* obtain shape image */
    TTile shape_tile;
    {
      TRaster32P tmp(1, 1);
      m_shape->allocateAndCompute(shape_tile, bBox.getP00(), dim, tmp, frame,
                                  settings);
    }

    if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

    processShape(frame, shape_tile, depth_map_p, alpha_map_p, regionIds_p,
                 regionBoundingRects, dim, settings);
  }

  if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

  // conpute thickness
  TRasterGR8P thickness_map_ras(sizeof(float) * dim.lx * dim.ly, 1);
  thickness_map_ras->lock();
  allocatedRasList.append(thickness_map_ras);
  float* thickness_map_p = (float*)thickness_map_ras->getRawData();

  TRasterP tileRas = tile.getRaster();
  TRaster32P ras32 = (TRaster32P)tileRas;
  TRaster64P ras64 = (TRaster64P)tileRas;

  if (m_fit_thickness->getValue()) {
    // Get the original bbox of thickness image
    TRectD thickBBox;
    m_input->getBBox(frame, thickBBox, settings);
    if (thickBBox == TConsts::infiniteRectD)
      thickBBox = TRectD(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                                 tile.getRaster()->getLy()));
    // Compute the thickenss tile.
    TTile thicknessTile;
    TDimension thickDim(static_cast<int>(thickBBox.getLx() + 0.5),
                        static_cast<int>(thickBBox.getLy() + 0.5));
    m_input->allocateAndCompute(thicknessTile, thickBBox.getP00(), thickDim,
                                tile.getRaster(), frame, settings);

    if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

    TRasterP thickRas = thicknessTile.getRaster();

    fitThicknessPatches(thickRas, thickDim, thickness_map_p, dim, regionIds_p,
                        regionBoundingRects);
  } else {
    /* compute the thickness input and temporarily store to the tile */
    m_input->compute(tile, frame, settings);

    if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

    if (ras32)
      convertToBrightness<TRaster32P, TPixel32>(ras32, thickness_map_p, nullptr,
                                                dim);
    else if (ras64)
      convertToBrightness<TRaster64P, TPixel64>(ras64, thickness_map_p, nullptr,
                                                dim);
  }

  if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

  /* process noise */
  processNoise(thickness_map_p, depth_map_p, dim, frame, settings);

  if (checkCancelAndReleaseRaster(allocatedRasList, tile, settings)) return;

  if (ras32)
    convertToRaster<TRaster32P, TPixel32>(ras32, thickness_map_p, depth_map_p,
                                          alpha_map_p, dim, bubbleColor_p);
  else if (ras64)
    convertToRaster<TRaster64P, TPixel64>(ras64, thickness_map_p, depth_map_p,
                                          alpha_map_p, dim, bubbleColor_p);

  for (int i = 0; i < allocatedRasList.size(); i++)
    allocatedRasList.at(i)->unlock();
}

//------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_SoapBubbleFx::convertToBrightness(const RASTER srcRas, float* dst,
                                           float* alpha, TDimensionI dim) {
  float* dst_p   = dst;
  float* alpha_p = alpha;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, dst_p++, pix++) {
      float r = (float)pix->r / (float)PIXEL::maxChannelValue;
      float g = (float)pix->g / (float)PIXEL::maxChannelValue;
      float b = (float)pix->b / (float)PIXEL::maxChannelValue;
      /* brightness */
      *dst_p = 0.298912f * r + 0.586611f * g + 0.114478f * b;
      if (alpha) {
        *alpha_p = (float)pix->m / (float)PIXEL::maxChannelValue;
        alpha_p++;
      }
    }
  }
}

//------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_SoapBubbleFx::convertToRaster(const RASTER ras, float* thickness_map_p,
                                       float* depth_map_p, float* alpha_map_p,
                                       TDimensionI dim, float3* bubbleColor_p) {
  int renderMode     = m_renderMode->getValue();
  float* depth_p     = depth_map_p;
  float* thickness_p = thickness_map_p;
  float* alpha_p     = alpha_map_p;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = ras->pixels(j);
    for (int i = 0; i < dim.lx;
         i++, depth_p++, thickness_p++, alpha_p++, pix++) {
      float alpha = (*alpha_p);
      if (!m_fit_thickness->getValue())
        alpha *= (float)pix->m / (float)PIXEL::maxChannelValue;
      if (alpha == 0.0f) { /* no change for the transparent pixels */
        pix->m = (typename PIXEL::Channel)0;
        continue;
      }

      // thickness and depth render mode
      if (renderMode != RENDER_MODE_BUBBLE) {
        float val = alpha * (float)PIXEL::maxChannelValue + 0.5f;
        pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
        float mapVal =
            (renderMode == RENDER_MODE_THICKNESS) ? (*thickness_p) : (*depth_p);
        val = alpha * mapVal * (float)PIXEL::maxChannelValue + 0.5f;
        typename PIXEL::Channel chanVal =
            (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                          ? (float)PIXEL::maxChannelValue
                                          : val);
        pix->r = chanVal;
        pix->g = chanVal;
        pix->b = chanVal;
        continue;
      }

      float coordinate[2];
      coordinate[0] = 256.0f * std::min(1.0f, *depth_p);
      coordinate[1] = 256.0f * std::min(1.0f, *thickness_p);

      int neighbors[2][2];

      /* interpolate sampling */
      if (coordinate[0] <= 0.5f)
        neighbors[0][0] = 0;
      else
        neighbors[0][0] = (int)std::floor(coordinate[0] - 0.5f);
      if (coordinate[0] >= 255.5f)
        neighbors[0][1] = 255;
      else
        neighbors[0][1] = (int)std::floor(coordinate[0] + 0.5f);
      if (coordinate[1] <= 0.5f)
        neighbors[1][0] = 0;
      else
        neighbors[1][0] = (int)std::floor(coordinate[1] - 0.5f);
      if (coordinate[1] >= 255.5f)
        neighbors[1][1] = 255;
      else
        neighbors[1][1] = (int)std::floor(coordinate[1] + 0.5f);

      float interp_ratio[2];
      interp_ratio[0] = coordinate[0] - 0.5f - std::floor(coordinate[0] - 0.5f);
      interp_ratio[1] = coordinate[1] - 0.5f - std::floor(coordinate[1] - 0.5f);

      float3 nColors[4] = {
          bubbleColor_p[neighbors[0][0] * 256 + neighbors[1][0]],
          bubbleColor_p[neighbors[0][1] * 256 + neighbors[1][0]],
          bubbleColor_p[neighbors[0][0] * 256 + neighbors[1][1]],
          bubbleColor_p[neighbors[0][1] * 256 + neighbors[1][1]]};

      float3 color =
          nColors[0] * (1.0f - interp_ratio[0]) * (1.0f - interp_ratio[1]) +
          nColors[1] * interp_ratio[0] * (1.0f - interp_ratio[1]) +
          nColors[2] * (1.0f - interp_ratio[0]) * interp_ratio[1] +
          nColors[3] * interp_ratio[0] * interp_ratio[1];

      /* clamp */
      float val = alpha * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = alpha * color.x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = alpha * color.y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = alpha * color.z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
    }
  }
}

//------------------------------------

void Iwa_SoapBubbleFx::processShape(double frame, TTile& shape_tile,
                                    float* depth_map_p, float* alpha_map_p,
                                    USHORT* regionIds_p,
                                    QList<QRect>& regionBoundingRects,
                                    TDimensionI dim,
                                    const TRenderSettings& settings) {
  TRaster32P shapeRas = shape_tile.getRaster();
  shapeRas->lock();

  TRasterGR8P distance_ras(sizeof(float) * dim.lx * dim.ly, 1);
  distance_ras->lock();
  float* distance_p = (float*)distance_ras->getRawData();

  float binarize_thres = (float)m_binarize_threshold->getValue(frame);

  int regionCount =
      do_binarize(shapeRas, regionIds_p, binarize_thres, distance_p,
                  alpha_map_p, regionBoundingRects, dim);

  shapeRas->unlock();

  if (settings.m_isCanceled && *settings.m_isCanceled) {
    distance_ras->unlock();
    return;
  }

  do_distance_transform(distance_p, regionIds_p, regionCount, dim, frame);

  if (settings.m_isCanceled && *settings.m_isCanceled) {
    distance_ras->unlock();
    return;
  }

  float center_opacity = (float)m_center_opacity->getValue(frame);
  if (center_opacity != 1.0f)
    applyDistanceToAlpha(distance_p, alpha_map_p, dim, center_opacity);

  /* create blur filter */
  float blur_radius = (float)m_blur_radius->getValue(frame) *
                      std::sqrt(std::abs((float)settings.m_affine.det()));

  /* if blur radius is 0, set the distance image to the depth image as-is */
  if (blur_radius == 0.0f) {
    float power      = (float)m_blur_power->getValue(frame);
    float* tmp_depth = depth_map_p;
    float* tmp_dist  = distance_p;
    USHORT* rid_p    = regionIds_p;
    for (int i = 0; i < dim.lx * dim.ly;
         i++, tmp_depth++, tmp_dist++, rid_p++) {
      if (*rid_p == 0)
        *tmp_depth = 0.0f;
      else
        *tmp_depth = 1.0f - std::pow(*tmp_dist, power);
    }
    distance_ras->unlock();
    return;
  }

  int blur_filter_size = (int)std::floor(blur_radius) * 2 + 1;
  TRasterGR8P blur_filter_ras(
      sizeof(float) * blur_filter_size * blur_filter_size, 1);
  blur_filter_ras->lock();
  float* blur_filter_p = (float*)blur_filter_ras->getRawData();

  do_createBlurFilter(blur_filter_p, blur_filter_size, blur_radius);

  if (settings.m_isCanceled && *settings.m_isCanceled) {
    blur_filter_ras->unlock();
    distance_ras->unlock();
    return;
  }

  /* blur filtering, normarize & power */
  do_applyFilter(depth_map_p, dim, distance_p, regionIds_p, blur_filter_p,
                 blur_filter_size, frame, settings);

  distance_ras->unlock();
  blur_filter_ras->unlock();
}

//------------------------------------

int Iwa_SoapBubbleFx::do_binarize(TRaster32P srcRas, USHORT* dst_p, float thres,
                                  float* distance_p, float* alpha_map_p,
                                  QList<QRect>& regionBoundingRects,
                                  TDimensionI dim) {
  TPixel32::Channel channelThres =
      (TPixel32::Channel)(thres * (float)TPixel32::maxChannelValue);
  USHORT* tmp_p   = dst_p;
  float* tmp_dist = distance_p;
  float* alpha_p  = alpha_map_p;
  for (int j = 0; j < dim.ly; j++) {
    TPixel32* pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, tmp_p++, tmp_dist++, alpha_p++) {
      (*tmp_p)    = (pix->m > channelThres) ? 1 : 0;
      (*tmp_dist) = (*tmp_p == 1) ? INF : 0.0f;
      *alpha_p    = (float)pix->m / (float)TPixel32::maxChannelValue;
    }
  }

  // label regions when multi bubble option is on
  if (!m_multi_source->getValue()) {
    if (m_fit_thickness->getValue()) {
      regionBoundingRects.append(QRect());
      // calc boundingRect of the bubble
      QPoint topLeft(dim.lx, dim.ly);
      QPoint bottomRight(0, 0);
      USHORT* tmp_p = dst_p;
      for (int j = 0; j < dim.ly; j++) {
        for (int i = 0; i < dim.lx; i++, tmp_p++) {
          if ((*tmp_p) == 0) continue;
          if (topLeft.x() > i) topLeft.setX(i);
          if (bottomRight.x() < i) bottomRight.setX(i);
          if (topLeft.y() > j) topLeft.setY(j);
          if (bottomRight.y() < j) bottomRight.setY(j);
        }
      }
      regionBoundingRects.append(QRect(topLeft, bottomRight));
    }

    return 1;
  }

  QList<int> lut;
  for (int i      = 0; i < 65536; i++) lut.append(i);
  tmp_p           = dst_p;
  int regionCount = 0;
  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx; i++, tmp_p++) {
      if ((*tmp_p) == 1) {
        int up   = (j == 0) ? 0 : *(tmp_p - dim.lx);
        int left = (i == 0) ? 0 : *(tmp_p - 1);
        assert(up >= 0 && left >= 0);
        if (!up && !left) {
          if (regionCount < 65535) regionCount++;
          (*tmp_p) = regionCount;
        } else if (up && !left)
          (*tmp_p) = up;
        else if (!up && left)
          (*tmp_p) = left;
        else if (up == left)
          (*tmp_p) = up;
        else if (up > left) {
          (*tmp_p) = left;
          lut[up]  = left;
        } else {
          (*tmp_p)  = up;
          lut[left] = up;
        }
      }
    }
  }

  // organize lut
  QList<int> convIndex;
  int currentIndex = 0;
  for (int i = 0; i < 65536; i++) {
    if (lut.at(i) == i) {
      lut[i] = currentIndex;
      currentIndex++;
    } else
      convIndex.append(i);
  }
  for (int i             = 0; i < convIndex.count(); i++)
    lut[convIndex.at(i)] = lut.at(lut.at(convIndex.at(i)));

  // apply lut
  int maxRegionIndex = 0;
  tmp_p              = dst_p;
  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx; i++, tmp_p++) {
      (*tmp_p)                                      = lut[*tmp_p];
      if (maxRegionIndex < (*tmp_p)) maxRegionIndex = (*tmp_p);
    }
  }

  // compute bounding boxes of each bubble
  if (m_fit_thickness->getValue()) {
    regionBoundingRects.append(QRect());
    USHORT* tmp_p = dst_p;
    for (int j = 0; j < dim.ly; j++) {
      for (int i = 0; i < dim.lx; i++, tmp_p++) {
        int rId = (*tmp_p);
        if (rId == 0) continue;
        while (regionBoundingRects.size() <= rId)
          regionBoundingRects.append(QRect());

        if (regionBoundingRects.at(rId).isNull())
          regionBoundingRects[rId].setRect(i, j, 1, 1);
        else {
          if (regionBoundingRects[rId].left() > i)
            regionBoundingRects[rId].setLeft(i);
          if (regionBoundingRects[rId].right() < i)
            regionBoundingRects[rId].setRight(i);
          if (regionBoundingRects[rId].top() > j)
            regionBoundingRects[rId].setTop(j);
          if (regionBoundingRects[rId].bottom() < j)
            regionBoundingRects[rId].setBottom(j);
        }
      }
    }
  }

  return maxRegionIndex;
}

//------------------------------------

void Iwa_SoapBubbleFx::do_createBlurFilter(float* dst_p, int size,
                                           float radius) {
  float radius2 = radius * radius;
  float* tmp_p  = dst_p;
  float sum     = 0.0f;
  int rad       = (size - 1) / 2;
  for (int j = -rad; j <= rad; j++) {
    for (int i = -rad; i <= rad; i++, tmp_p++) {
      float length2 = (float)i * (float)i + (float)j * (float)j;
      /* out of range */
      if (length2 >= radius2)
        *tmp_p = 0.0f;
      else {
        /* normalize distace from the filter center, to 0-1 */
        *tmp_p = 1.0f - std::sqrt(length2) / radius;
        sum += *tmp_p;
      }
    }
  }
  /* normalize */
  tmp_p = dst_p;
  for (int i = 0; i < size * size; i++, tmp_p++) {
    *tmp_p /= sum;
  }
}
//------------------------------------

void Iwa_SoapBubbleFx::do_applyFilter(float* depth_map_p, TDimensionI dim,
                                      float* distance_p, USHORT* binarized_p,
                                      float* blur_filter_p,
                                      int blur_filter_size, double frame,
                                      const TRenderSettings& settings) {
  float power = (float)m_blur_power->getValue(frame);

  memset(depth_map_p, 0, sizeof(float) * dim.lx * dim.ly);

  int fil_margin = (blur_filter_size - 1) / 2;
  float* dst_p   = depth_map_p;
  USHORT* bin_p  = binarized_p;
  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx; i++, dst_p++, bin_p++) {
      if (*bin_p == 0) continue;

      float* fil_p = blur_filter_p;
      for (int fy = j - fil_margin; fy <= j + fil_margin; fy++) {
        if (fy < 0 || fy >= dim.ly) {
          fil_p += blur_filter_size;
          continue;
        }
        for (int fx = i - fil_margin; fx <= i + fil_margin; fx++, fil_p++) {
          if (fx < 0 || fx >= dim.lx) continue;

          *dst_p += *fil_p * distance_p[fy * dim.lx + fx];
        }
      }
      /* power the value */
      *dst_p = 1.0f - std::pow(*dst_p, power);
    }
    if (settings.m_isCanceled && *settings.m_isCanceled) return;
  }
}

//------------------------------------

void Iwa_SoapBubbleFx::processNoise(float* thickness_map_p, float* depth_map_p,
                                    TDimensionI dim, double frame,
                                    const TRenderSettings& settings) {
  float noise_depth_mix_ratio = (float)m_noise_depth_mix_ratio->getValue(frame);
  float noise_thickness_mix_ratio =
      (float)m_noise_thickness_mix_ratio->getValue(frame);

  /* If the noise ratio is 0, do nothing and return */
  if (noise_depth_mix_ratio == 0.0f && noise_thickness_mix_ratio == 0.0f)
    return;

  int noise_sub_depth    = m_noise_sub_depth->getValue();
  int noise_resolution_s = (int)m_noise_resolution_s->getValue(frame);
  int noise_resolution_t = (int)m_noise_resolution_t->getValue(frame);
  float noise_composite_ratio =
      (float)m_noise_sub_composite_ratio->getValue(frame);
  float noise_evolution = (float)m_noise_evolution->getValue(frame);

  /* initialize the phase map */
  QList<int> noise_amount;
  QList<QSize> noise_base_resolution;
  int whole_noise_amount = 0;

  for (int layer = 0; layer < noise_sub_depth; layer++) {
    /* noise resolution */
    /* width: circumferential direction   height:distal direction */
    QSize size;
    size.setWidth(std::pow(2, layer) * noise_resolution_s);
    size.setHeight(std::pow(2, layer) * noise_resolution_t + 1);
    noise_base_resolution.append(size);
    int amount = size.width() * size.height();
    noise_amount.append(amount);
    whole_noise_amount += amount;
  }

  float* noise_phases = new float[whole_noise_amount];
  float* ph_p         = noise_phases;

  srand(0);
  /* Set the phase differences (0-2) */
  for (int i = 0; i < whole_noise_amount; i++, ph_p++) {
    *ph_p = (float)rand() / (float)RAND_MAX * 2.0f * PI;
  }

  /* make noise base */
  /* compute composite ratio of each layer */
  QList<float> comp_ratios;
  comp_ratios.append(10.0f);
  float ratio_sum = 10.0f;
  for (int i = 1; i < noise_sub_depth; i++) {
    comp_ratios.append(comp_ratios.last() * noise_composite_ratio);
    ratio_sum += comp_ratios.last();
  }
  /* normalize */
  for (int i = 0; i < noise_sub_depth; i++) comp_ratios[i] /= ratio_sum;

  float* noise_base = new float[whole_noise_amount];

  float* nb_p = noise_base;
  ph_p        = noise_phases;

  /* for each sub-noise layer */
  for (int layer = 0; layer < noise_sub_depth; layer++) {
    float tmp_evolution = noise_evolution * (float)(layer + 1);
    for (int i = 0; i < noise_amount[layer]; i++, nb_p++, ph_p++) {
      *nb_p = comp_ratios[layer] * (cosf(tmp_evolution + *ph_p) / 2.0f + 0.5f);
    }
  }
  delete[] noise_phases;

  TRasterGR8P norm_angle_ras(sizeof(float) * dim.lx * dim.ly, 1);
  norm_angle_ras->lock();
  float* norm_angle_p = (float*)norm_angle_ras->getRawData();

  calc_norm_angle(norm_angle_p, depth_map_p, dim, settings.m_shrinkX);

  TRasterGR8P noise_map_ras(sizeof(float) * dim.lx * dim.ly, 1);
  noise_map_ras->lock();
  float* noise_map_p = (float*)noise_map_ras->getRawData();

  make_noise_map(noise_map_p, depth_map_p, norm_angle_p, dim, noise_amount,
                 noise_base_resolution, noise_sub_depth, noise_base);

  norm_angle_ras->unlock();
  delete[] noise_base;

  /* composite with perlin noise */
  add_noise(thickness_map_p, depth_map_p, dim, noise_map_p,
            noise_thickness_mix_ratio, noise_depth_mix_ratio);

  noise_map_ras->unlock();
}

//------------------------------------

void Iwa_SoapBubbleFx::calc_norm_angle(float* norm_angle_p, float* depth_map_p,
                                       TDimensionI dim, int shrink) {
  struct Locals {
    TDimensionI _dim;
    const float* _depth_p;
    float data(int x, int y) {
      if (x < 0 || _dim.lx <= x || y < 0 || _dim.ly <= y) return 0.0f;
      return _depth_p[y * _dim.lx + x];
    }
  } locals = {dim, depth_map_p};

  int sampleDistance =
      std::max(1, m_normal_sample_distance->getValue() / shrink);
  float* dst_p = norm_angle_p;

  for (int j = 0; j < dim.ly; j++) {
    int sample_y[2]                  = {j - sampleDistance, j + sampleDistance};
    if (sample_y[0] < 0) sample_y[0] = 0;
    if (sample_y[1] >= dim.ly) sample_y[1] = dim.ly - 1;

    for (int i = 0; i < dim.lx; i++, norm_angle_p++) {
      int sample_x[2] = {i - sampleDistance, i + sampleDistance};
      if (sample_x[1] >= dim.lx) sample_x[1] = dim.lx - 1;
      if (sample_x[0] < 0) sample_x[0]       = 0;

      float gradient[2];
      gradient[0] =
          (locals.data(sample_x[0], j) - locals.data(sample_x[1], j)) /
          (float)(sample_x[0] - sample_x[1]);
      gradient[1] =
          (locals.data(i, sample_y[0]) - locals.data(i, sample_y[1])) /
          (float)(sample_y[0] - sample_y[1]);

      if (gradient[0] == 0.0f && gradient[1] == 0.0f)
        *norm_angle_p = 0.0f;
      else /* normalize value range to 0-1 */
        *norm_angle_p =
            0.5f + std::atan2(gradient[0], gradient[1]) / (2.0f * PI);
    }
  }
}

//------------------------------------

void Iwa_SoapBubbleFx::make_noise_map(float* noise_map_p, float* depth_map_p,
                                      float* norm_angle_p, TDimensionI dim,
                                      const QList<int>& noise_amount,
                                      const QList<QSize>& noise_base_resolution,
                                      int noise_sub_depth, float* noise_base) {
  float* dst_p   = noise_map_p;
  float* depth_p = depth_map_p;
  float* norm_p  = norm_angle_p;

  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx; i++, dst_p++, depth_p++, norm_p++) {
      /* Obtain coordinate */
      /* circumferential direction */
      float tmp_s = (*norm_p);
      /* distal direction */
      float tmp_t = std::min(1.0f, *depth_p);

      /* accumulate noise values */
      *dst_p                  = 0.0f;
      float* noise_layer_base = noise_base;
      for (int layer = 0; layer < noise_sub_depth; layer++) {
        /* obtain pseudo polar coords */
        QSize reso = noise_base_resolution.at(layer);
        float polar_s =
            tmp_s * (float)(reso.width()); /* because it is circumferential */
        float polar_t = tmp_t * (float)(reso.height() - 1);

        /* first, compute circumferential position and ratio */
        int neighbor_s[2];
        neighbor_s[0] = (int)std::floor(polar_s);
        neighbor_s[1] = neighbor_s[0] + 1;
        if (neighbor_s[0] == reso.width()) neighbor_s[0] = 0;
        if (neighbor_s[1] >= reso.width()) neighbor_s[1] = 0;
        float ratio_s = polar_s - std::floor(polar_s);

        /* second, compute distal position and ratio */
        int neighbor_t[2];
        neighbor_t[0] = (int)std::floor(polar_t);
        neighbor_t[1] = neighbor_t[0] + 1;
        if (neighbor_t[1] == reso.height()) neighbor_t[1] -= 1;
        float ratio_t = polar_t - std::floor(polar_t);

        *dst_p += noise_interp(neighbor_s[0], neighbor_s[1], neighbor_t[0],
                               neighbor_t[1], ratio_s, ratio_t,
                               noise_layer_base, reso.width());

        /* offset noise pointer */
        noise_layer_base += noise_amount[layer];
      }
    }
  }
}

//------------------------------------

float Iwa_SoapBubbleFx::noise_interp(int left, int right, int bottom, int top,
                                     float ratio_s, float ratio_t,
                                     float* noise_layer_base, int noise_dim_x) {
  struct Locals {
    int _dim_x;
    const float* _noise_p;
    float data(int x, int y) { return _noise_p[y * _dim_x + x]; }
  } locals = {noise_dim_x, noise_layer_base};

  float c_ratio_s = (1.0f - cosf(ratio_s * PI)) * 0.5f;
  float c_ratio_t = (1.0f - cosf(ratio_t * PI)) * 0.5f;

  return locals.data(left, bottom) * (1.0f - c_ratio_s) * (1.0f - c_ratio_t) +
         locals.data(right, bottom) * c_ratio_s * (1.0f - c_ratio_t) +
         locals.data(left, top) * (1.0f - c_ratio_s) * c_ratio_t +
         locals.data(right, top) * c_ratio_s * c_ratio_t;
}

//------------------------------------

void Iwa_SoapBubbleFx::add_noise(float* thickness_map_p, float* depth_map_p,
                                 TDimensionI dim, float* noise_map_p,
                                 float noise_thickness_mix_ratio,
                                 float noise_depth_mix_ratio) {
  float one_minus_thickness_ratio = 1.0f - noise_thickness_mix_ratio;
  float one_minus_depth_ratio     = 1.0f - noise_depth_mix_ratio;
  float* tmp_thickness            = thickness_map_p;
  float* tmp_depth                = depth_map_p;
  float* tmp_noise                = noise_map_p;

  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx;
         i++, tmp_thickness++, tmp_depth++, tmp_noise++) {
      *tmp_thickness = *tmp_noise * noise_thickness_mix_ratio +
                       *tmp_thickness * one_minus_thickness_ratio;
      *tmp_depth = *tmp_noise * noise_depth_mix_ratio +
                   *tmp_depth * one_minus_depth_ratio;
    }
  }
}
//------------------------------------

void Iwa_SoapBubbleFx::do_distance_transform(float* dst_p, USHORT* binarized_p,
                                             int regionCount, TDimensionI dim,
                                             double frame) {
  float ar = (float)m_shape_aspect_ratio->getValue(frame);

  float* f = new float[std::max(dim.lx, dim.ly)];

  QList<float> max_val;
  for (int r = 0; r <= regionCount; r++) max_val.append(0.0f);

  float* tmp_dst = dst_p;
  /* transform along rows */
  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx; i++, *tmp_dst++) {
      f[i] = *tmp_dst;
    }

    tmp_dst -= dim.lx;

    float* d = dt(f, dim.lx);
    for (int i = 0; i < dim.lx; i++, tmp_dst++) {
      *tmp_dst = d[i];
    }
    delete[] d;
  }
  /* transform along columns */
  for (int i = 0; i < dim.lx; i++) {
    for (int j = 0; j < dim.ly; j++) {
      f[j] = dst_p[j * dim.lx + i];
    }
    float* d =
        dt(f, dim.ly,
           ar); /* ar : taking account of the aspect ratio of the shape */
    for (int j = 0; j < dim.ly; j++) {
      dst_p[j * dim.lx + i] = d[j];
      int regionId          = binarized_p[j * dim.lx + i];
      if (d[j] > max_val[regionId]) max_val[regionId] = d[j];
    }
    delete[] d;
  }

  tmp_dst = dst_p;
  for (int r = 0; r <= regionCount; r++) max_val[r] = std::sqrt(max_val[r]);

  /* square root and normalize */
  USHORT* region_p = binarized_p;
  for (int i = 0; i < dim.lx * dim.ly; i++, *tmp_dst++, region_p++) {
    if (max_val[*region_p] > 0)
      *tmp_dst = std::sqrt(*tmp_dst) / max_val[*region_p];
  }
}
//------------------------------------

bool Iwa_SoapBubbleFx::checkCancelAndReleaseRaster(
    const QList<TRasterGR8P>& allocatedRasList, TTile& tile,
    const TRenderSettings& settings) {
  if (settings.m_isCanceled && *settings.m_isCanceled) {
    for (int i = 0; i < allocatedRasList.size(); i++)
      allocatedRasList.at(i)->unlock();
    tile.getRaster()->clear();
    return true;
  } else
    return false;
}
//------------------------------------

void Iwa_SoapBubbleFx::applyDistanceToAlpha(float* distance_p,
                                            float* alpha_map_p, TDimensionI dim,
                                            float center_opacity) {
  float da   = 1.0f - center_opacity;
  float* d_p = distance_p;
  float* a_p = alpha_map_p;
  for (int i = 0; i < dim.lx * dim.ly; i++, d_p++, a_p++) {
    (*a_p) *= 1.0f - (*d_p) * da;
  }
}

//------------------------------------
// This will be called in TFx::loadData when obsolete "mask center" value is
// loaded
void Iwa_SoapBubbleFx::onObsoleteParamLoaded(const std::string& paramName) {
  if (paramName != "maskCenter") return;
  // if "mask center" was ON, set a key frame to the center opacity in order to
  // get the same result.
  if (m_mask_center->getValue()) m_center_opacity->setValue(0.0, 0.0);
}

//------------------------------------
// patch the thickness images to each bounding box of the bubble
void Iwa_SoapBubbleFx::fitThicknessPatches(TRasterP thickRas,
                                           TDimensionI thickDim,
                                           float* thickness_map_p,
                                           TDimensionI dim, USHORT* regionIds_p,
                                           QList<QRect>& regionBoundingRects) {
  int regionCount = regionBoundingRects.size() - 1;

  // compute resized thickness rasters
  QList<TRasterGR16P> resizedThicks;
  resizedThicks.append(TRasterGR16P());
  for (int r = 1; r <= regionCount; r++) {
    QRect regionRect = regionBoundingRects.at(r);
    TRaster64P resizedThickness(
        TDimension(regionRect.width(), regionRect.height()));
    resizedThickness->lock();

    TAffine aff = TScale((double)regionRect.width() / (double)thickDim.lx,
                         (double)regionRect.height() / (double)thickDim.ly);

    // resample the thickenss
    TRop::resample(resizedThickness, thickRas, aff);

    for (int ry = 0; ry < regionRect.height(); ry++) {
      TPixel64* p = resizedThickness->pixels(ry);
      for (int rx = 0; rx < regionRect.width(); rx++, p++) {
        double val = (double)((*p).r) / (double)(TPixel64::maxChannelValue);
      }
    }

    TRasterGR16P thickRas_gray(
        TDimension(regionRect.width(), regionRect.height()));
    thickRas_gray->lock();
    TRop::convert(thickRas_gray, resizedThickness);

    resizedThickness->unlock();
    resizedThicks.append(thickRas_gray);
  }

  float* out_p  = thickness_map_p;
  USHORT* rId_p = regionIds_p;
  for (int j = 0; j < dim.ly; j++) {
    for (int i = 0; i < dim.lx; i++, out_p++, rId_p++) {
      if ((*rId_p) == 0) {
        (*out_p) = 0.0f;
        continue;
      }
      QRect regionBBox = regionBoundingRects.at((int)(*rId_p));
      QPoint coordInRegion(i - regionBBox.left(), j - regionBBox.top());
      TPixelGR16 pix = resizedThicks.at((int)(*rId_p))
                           ->pixels(coordInRegion.y())[coordInRegion.x()];
      (*out_p) = (float)pix.value / (float)TPixelGR16::maxChannelValue;
    }
  }

  for (int r = 1; r <= regionCount; r++) resizedThicks.at(r)->unlock();
}

//==============================================================================

FX_PLUGIN_IDENTIFIER(Iwa_SoapBubbleFx, "iwa_SoapBubbleFx");
