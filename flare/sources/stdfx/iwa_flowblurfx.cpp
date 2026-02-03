//--------------------------------------------------------------
// This Fx is based on & modified from the source code by Zhanping Liu, licensed
// with the terms as follows:

//----------------[Start of the License Notice]-----------------
////////////////////////////////////////////////////////////////////////////
///          Line Integral Convolution for Flow Visualization            ///
///                           Initial  Version                           ///
///                             May 15, 1999                             ///
///                           by  Zhanping Liu                           ///
///                      (zhanping@erc.msstate.edu)                      ///
///                    while with  Graphics  Laboratory,                 ///
///                    Peking  University, P. R. China                   ///
///----------------------------------------------------------------------///
///                           Later  Condensed                           ///
///                             May 4,  2002                             ///
///                                                                      ///
///          VAIL (Visualization Analysis & Imaging Laboratory)          ///
///                  ERC  (Engineering Research Center)                  ///
///                     Mississippi State University                     ///
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
/// This code was developed  based on  the original algorithm  proposed by///
/// Brian Cabral  and  Leith (Casey) Leedom  in the paper  "Imaging Vector///
/// Fields Using Line Integral Convolution", published  in  Proceedings of///
/// ACM  SigGraph 93,  Aug 2-6,  Anaheim,  California,  pp. 263-270, 1993.///
/// Permission to use, copy, modify, distribute and sell this code for any///
/// purpose is hereby granted without fee,  provided that the above notice///
/// appears in all copies  and  that both that notice and  this permission///
/// appear in supporting documentation. The  developer  of this code makes///
/// no  representations  about  the  suitability  of  this  code  for  any///
/// purpose.  It is provided "as is"  without express or implied warranty.///
////////////////////////////////////////////////////////////////////////////
//-----------------[End of the License Notice]------------------

//--------------------------------------------------------------
#include "iwa_flowblurfx.h"
#include <QThreadPool>

namespace {
const double LINE_SQUARE_CLIP_MAX = 100000.0;
const double VECTOR_COMPONENT_MIN = 0.05;

inline double clamp01(double val) {
  return (val > 1.0) ? 1.0 : (val < 0.0) ? 0.0 : val;
}

// convert sRGB color space to power space
template <typename T = double>
inline T to_linear_color_space(T nonlinear_color, T exposure, T gamma) {
  if (nonlinear_color <= T(0)) return T(0);
  return std::pow(nonlinear_color, gamma) / exposure;
}
// convert power space to sRGB color space
template <typename T = double>
inline T to_nonlinear_color_space(T linear_color, T exposure, T gamma) {
  if (linear_color <= T(0)) return T(0);
  return std::pow(linear_color * exposure, T(1) / gamma);
}

}  // namespace
//------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_FlowBlurFx::setSourceTileToBuffer(const RASTER srcRas, double4 *buf,
                                           bool isLinear, double gamma) {
  double4 *buf_p = buf;
  for (int j = 0; j < srcRas->getLy(); j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < srcRas->getLx(); i++, pix++, buf_p++) {
      (*buf_p).x = double(pix->r) / double(PIXEL::maxChannelValue);
      (*buf_p).y = double(pix->g) / double(PIXEL::maxChannelValue);
      (*buf_p).z = double(pix->b) / double(PIXEL::maxChannelValue);
      (*buf_p).w = double(pix->m) / double(PIXEL::maxChannelValue);
      if (isLinear && (*buf_p).w > 0.0) {
        (*buf_p).x =
            to_linear_color_space((*buf_p).x / (*buf_p).w, 1.0, gamma) *
            (*buf_p).w;
        (*buf_p).y =
            to_linear_color_space((*buf_p).y / (*buf_p).w, 1.0, gamma) *
            (*buf_p).w;
        (*buf_p).z =
            to_linear_color_space((*buf_p).z / (*buf_p).w, 1.0, gamma) *
            (*buf_p).w;
      }
    }
  }
}
//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FlowBlurFx::setFlowTileToBuffer(const RASTER flowRas, double2 *buf,
                                         double *refBuf) {
  double2 *buf_p   = buf;
  double *refBuf_p = refBuf;
  for (int j = 0; j < flowRas->getLy(); j++) {
    PIXEL *pix = flowRas->pixels(j);
    for (int i = 0; i < flowRas->getLx(); i++, pix++, buf_p++) {
      double val = double(pix->r) / double(PIXEL::maxChannelValue);
      (*buf_p).x = val * 2.0 - 1.0;
      val        = double(pix->g) / double(PIXEL::maxChannelValue);
      (*buf_p).y = val * 2.0 - 1.0;
      if (refBuf != nullptr) {
        *refBuf_p = double(pix->b) / double(PIXEL::maxChannelValue);
        refBuf_p++;
      }
    }
  }
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FlowBlurFx::setReferenceTileToBuffer(const RASTER refRas,
                                              double *buf) {
  double *buf_p = buf;
  for (int j = 0; j < refRas->getLy(); j++) {
    PIXEL *pix = refRas->pixels(j);
    for (int i = 0; i < refRas->getLx(); i++, pix++, buf_p++) {
      // Value = 0.3R 0.59G 0.11B
      (*buf_p) = (double(pix->r) * 0.3 + double(pix->g) * 0.59 +
                  double(pix->b) * 0.11) /
                 double(PIXEL::maxChannelValue);
    }
  }
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_FlowBlurFx::setOutputRaster(double4 *out_buf, const RASTER dstRas,
                                     bool isLinear, double gamma) {
  double4 *buf_p = out_buf;
  for (int j = 0; j < dstRas->getLy(); j++) {
    PIXEL *pix = dstRas->pixels(j);
    for (int i = 0; i < dstRas->getLx(); i++, pix++, buf_p++) {
      if (!isLinear || (*buf_p).w == 0.0) {
        pix->r = (typename PIXEL::Channel)(clamp01((*buf_p).x) *
                                           (double)PIXEL::maxChannelValue);
        pix->g = (typename PIXEL::Channel)(clamp01((*buf_p).y) *
                                           (double)PIXEL::maxChannelValue);
        pix->b = (typename PIXEL::Channel)(clamp01((*buf_p).z) *
                                           (double)PIXEL::maxChannelValue);
      } else {
        double val;
        val = to_nonlinear_color_space((*buf_p).x / (*buf_p).w, 1.0, gamma) *
              (*buf_p).w;
        pix->r = (typename PIXEL::Channel)(clamp01(val) *
                                           (double)PIXEL::maxChannelValue);
        val    = to_nonlinear_color_space((*buf_p).y / (*buf_p).w, 1.0, gamma) *
              (*buf_p).w;
        pix->g = (typename PIXEL::Channel)(clamp01(val) *
                                           (double)PIXEL::maxChannelValue);
        val    = to_nonlinear_color_space((*buf_p).z / (*buf_p).w, 1.0, gamma) *
              (*buf_p).w;
        pix->b = (typename PIXEL::Channel)(clamp01(val) *
                                           (double)PIXEL::maxChannelValue);
      }

      pix->m = (typename PIXEL::Channel)(clamp01((*buf_p).w) *
                                         (double)PIXEL::maxChannelValue);
    }
  }
}
//------------------------------------------------------------

void FlowBlurWorker::run() {
  auto sourceAt = [&](int x, int y) {
    if (x < 0 || x >= m_dim.lx || y < 0 || y >= m_dim.ly) return double4();
    return m_source_buf[y * m_dim.lx + x];
  };
  auto flowAt = [&](int x, int y) {
    if (x < 0 || x >= m_dim.lx || y < 0 || y >= m_dim.ly) return double2();
    return m_flow_buf[y * m_dim.lx + x];
  };

  // ロジスティック分布の確率密度関数を格納する(s = 1/3、0-1の範囲で100分割)
  double logDist[101];
  if (m_filterType == Gaussian) {
    double scale = 1.0 / 3.0;
    for (int i = 0; i <= 101; i++) {
      double x   = (double)i / 100.0;
      logDist[i] = std::tanh(x / (2.0 * scale));
    }
  }
  // 0-1に正規化した変数の累積値を返す
  auto getCumulative = [&](double pos) {
    if (pos > 1.0) return 1.0;
    if (m_filterType == Linear)
      return 2.0 * pos - pos * pos;
    else if (m_filterType == Gaussian)
      return logDist[(int)(std::ceil(pos * 100.0))];
    else  // Flat
      return pos;
  };

  int max_ADVCTS = int(m_krnlen * 3);  // MAXIMUM number of advection steps per
                                       // direction to break dead loops

  double4 *out_p = &m_out_buf[m_yFrom * m_dim.lx];

  double *ref_p =
      (m_reference_buf) ? &m_reference_buf[m_yFrom * m_dim.lx] : nullptr;

  // for each pixel in the 2D output LIC image
  for (int j = m_yFrom; j < m_yTo; j++) {
    for (int i = 0; i < m_dim.lx; i++, out_p++) {
      double4 t_acum[2];            // texture accumulators zero-initialized
      double w_acum[2] = {0., 0.};  // weight accumulators zero-initialized

      double blurLength = (ref_p) ? (*ref_p) * m_krnlen : m_krnlen;

      if (blurLength == 0.0) {
        (*out_p) = sourceAt(i, j);
        if (ref_p) ref_p++;
        continue;
      }

      // for either advection direction
      for (int advDir = 0; advDir < 2; advDir++) {
        // init the step counter, curve-length measurer, and streamline seed///
        int advcts =
            0;  // number of ADVeCTion stepS per direction (a step counter)
        double curLen = 0.0;  // CURrent   LENgth of the streamline
        double clp0_x =
            (double)i + 0.5;  // x-coordinate of CLiP point 0 (current)
        double clp0_y =
            (double)j + 0.5;  // y-coordinate of CLiP point 0	(current)

        // until the streamline is advected long enough or a tightly  spiralling
        // center / focus is encountered///

        double2 pre_vctr = flowAt(int(clp0_x), int(clp0_y));
        if (advDir == 1) {
          pre_vctr.x = -pre_vctr.x;
          pre_vctr.y = -pre_vctr.y;
        }

        while (curLen < blurLength && advcts < max_ADVCTS) {
          // access the vector at the sample
          double2 vctr = flowAt(int(clp0_x), int(clp0_y));

          // in case of a critical point
          if (vctr.x == 0.0 && vctr.y == 0.0) {
            w_acum[advDir] = (advcts == 0) ? 1.0 : w_acum[advDir];
            break;
          }

          // negate the vector for the backward-advection case
          if (advDir == 1) {
            vctr.x = -vctr.x;
            vctr.y = -vctr.y;
          }

          // assuming that the flow field vector is bi-directional
          // if the vector is at an acute angle to the previous vector, negate
          // it.
          if (vctr.x * pre_vctr.x + vctr.y * pre_vctr.y < 0) {
            vctr.x = -vctr.x;
            vctr.y = -vctr.y;
          }

          // clip the segment against the pixel boundaries --- find the shorter
          // from the two clipped segments replace  all  if-statements  whenever
          // possible  as  they  might  affect the computational speed
          double tmpLen;
          double segLen = LINE_SQUARE_CLIP_MAX;  // SEGment   LENgth
          segLen        = (vctr.x < -VECTOR_COMPONENT_MIN)
                              ? (std::floor(clp0_x) - clp0_x) / vctr.x
                              : segLen;
          segLen =
              (vctr.x > VECTOR_COMPONENT_MIN)
                  ? (std::floor(std::floor(clp0_x) + 1.5) - clp0_x) / vctr.x
                  : segLen;
          segLen = (vctr.y < -VECTOR_COMPONENT_MIN)
                       ? (((tmpLen = (std::floor(clp0_y) - clp0_y) / vctr.y) <
                           segLen)
                              ? tmpLen
                              : segLen)
                       : segLen;
          segLen = (vctr.y > VECTOR_COMPONENT_MIN)
                       ? (((tmpLen = (std::floor(std::floor(clp0_y) + 1.5) -
                                      clp0_y) /
                                     vctr.y) < segLen)
                              ? tmpLen
                              : segLen)
                       : segLen;

          // update the curve-length measurers
          double prvLen = curLen;
          curLen += segLen;
          segLen += 0.0004;

          // check if the filter has reached either end
          segLen =
              (curLen > m_krnlen) ? ((curLen = m_krnlen) - prvLen) : segLen;

          // obtain the next clip point
          double clp1_x = clp0_x + vctr.x * segLen;
          double clp1_y = clp0_y + vctr.y * segLen;

          // obtain the middle point of the segment as the texture-contributing
          // sample
          double2 samp;
          samp.x = (clp0_x + clp1_x) * 0.5;
          samp.y = (clp0_y + clp1_y) * 0.5;

          // obtain the texture value of the sample
          double4 texVal = sourceAt(int(samp.x), int(samp.y));

          // update the accumulated weight and the accumulated composite texture
          // (texture x weight)
          double W_ACUM = getCumulative(curLen / blurLength);
          // double W_ACUM = curLen;
          // W_ACUM = wgtLUT[int(curLen * len2ID)];

          double smpWgt  = W_ACUM - w_acum[advDir];
          w_acum[advDir] = W_ACUM;
          t_acum[advDir].x += texVal.x * smpWgt;
          t_acum[advDir].y += texVal.y * smpWgt;
          t_acum[advDir].z += texVal.z * smpWgt;
          t_acum[advDir].w += texVal.w * smpWgt;

          // update the step counter and the "current" clip point
          advcts++;
          clp0_x = clp1_x;
          clp0_y = clp1_y;

          pre_vctr = vctr;
          // check if the streamline has gone beyond the flow field///
          if (clp0_x < 0.0 || clp0_x >= double(m_dim.lx) || clp0_y < 0.0 ||
              clp0_y >= double(m_dim.ly))
            break;
        }
      }

      // normalize the accumulated composite texture
      (*out_p).x = (t_acum[0].x + t_acum[1].x) / (w_acum[0] + w_acum[1]);
      (*out_p).y = (t_acum[0].y + t_acum[1].y) / (w_acum[0] + w_acum[1]);
      (*out_p).z = (t_acum[0].z + t_acum[1].z) / (w_acum[0] + w_acum[1]);
      (*out_p).w = (t_acum[0].w + t_acum[1].w) / (w_acum[0] + w_acum[1]);

      if (ref_p) ref_p++;
    }
  }
}

Iwa_FlowBlurFx::Iwa_FlowBlurFx()
    : m_length(5.0)
    , m_linear(false)
    , m_gamma(2.2)
    , m_filterType(new TIntEnumParam(Linear, "Linear"))
    , m_referenceMode(new TIntEnumParam(REFERENCE, "Reference Image")) {
  addInputPort("Source", m_source);
  addInputPort("Flow", m_flow);
  addInputPort("Reference", m_reference);

  bindParam(this, "length", m_length);
  bindParam(this, "linear", m_linear);
  bindParam(this, "gamma", m_gamma);
  bindParam(this, "filterType", m_filterType);
  bindParam(this, "referenceMode", m_referenceMode);

  m_length->setMeasureName("fxLength");
  m_length->setValueRange(0., 100.0);
  m_gamma->setValueRange(0.2, 5.0);

  m_filterType->addItem(Gaussian, "Gaussian");
  m_filterType->addItem(Flat, "Flat");

  m_referenceMode->addItem(FLOW_BLUE_CHANNEL, "Blue Channel of Flow Image");
}

void Iwa_FlowBlurFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &settings) {
  // do nothing if Source or Flow port is not connected
  if (!m_source.isConnected() || !m_flow.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  // output size
  TDimension dim = tile.getRaster()->getSize();

  double fac     = sqrt(fabs(settings.m_affine.det()));
  double krnlen  = fac * m_length->getValue(frame);
  int max_ADVCTS = int(krnlen * 3);  // MAXIMUM number of advection steps per
                                     // direction to break dead loops

  if (max_ADVCTS == 0) {
    // No blur will be done. The underlying fx may pass by.
    m_source->compute(tile, frame, settings);
    return;
  }

  bool isLinear = m_linear->getValue();
  double gamma  = m_gamma->getValue(frame);

  // obtain Source memory buffer (RGBA)
  TTile sourceTile;
  m_source->allocateAndCompute(sourceTile, tile.m_pos, dim, tile.getRaster(),
                               frame, settings);
  // allocate buffer
  double4 *source_buf;
  TRasterGR8P source_buf_ras(dim.lx * dim.ly * sizeof(double4), 1);
  source_buf_ras->lock();
  source_buf       = (double4 *)source_buf_ras->getRawData();
  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  if (ras32)
    setSourceTileToBuffer<TRaster32P, TPixel32>(sourceTile.getRaster(),
                                                source_buf, isLinear, gamma);
  else if (ras64)
    setSourceTileToBuffer<TRaster64P, TPixel64>(sourceTile.getRaster(),
                                                source_buf, isLinear, gamma);

  // obtain Flow memory buffer (XY)
  TTile flowTile;
  m_flow->allocateAndCompute(flowTile, tile.m_pos, dim, tile.getRaster(), frame,
                             settings);
  // allocate buffer
  double2 *flow_buf;
  TRasterGR8P flow_buf_ras(dim.lx * dim.ly * sizeof(double2), 1);
  flow_buf_ras->lock();
  flow_buf = (double2 *)flow_buf_ras->getRawData();

  double *reference_buf = nullptr;
  TRasterGR8P reference_buf_ras;
  if (m_referenceMode->getValue() == FLOW_BLUE_CHANNEL ||
      m_reference.isConnected()) {
    reference_buf_ras = TRasterGR8P(dim.lx * dim.ly * sizeof(double), 1);
    reference_buf_ras->lock();
    reference_buf = (double *)reference_buf_ras->getRawData();
  }

  if (ras32)
    setFlowTileToBuffer<TRaster32P, TPixel32>(flowTile.getRaster(), flow_buf,
                                              reference_buf);
  else if (ras64)
    setFlowTileToBuffer<TRaster64P, TPixel64>(flowTile.getRaster(), flow_buf,
                                              reference_buf);

  if (m_referenceMode->getValue() == REFERENCE && m_reference.isConnected()) {
    TTile referenceTile;
    m_reference->allocateAndCompute(referenceTile, tile.m_pos, dim,
                                    tile.getRaster(), frame, settings);
    if (ras32)
      setReferenceTileToBuffer<TRaster32P, TPixel32>(referenceTile.getRaster(),
                                                     reference_buf);
    else if (ras64)
      setReferenceTileToBuffer<TRaster64P, TPixel64>(referenceTile.getRaster(),
                                                     reference_buf);
  }

  // buffer for output raster
  double4 *out_buf;
  TRasterGR8P out_buf_ras(dim.lx * dim.ly * sizeof(double4), 1);
  out_buf_ras->lock();
  out_buf = (double4 *)out_buf_ras->getRawData();

  int activeThreadCount = QThreadPool::globalInstance()->activeThreadCount();
  // use half of the available threads
  int threadAmount       = std::max(1, activeThreadCount / 2);
  FILTER_TYPE filterType = (FILTER_TYPE)m_filterType->getValue();
  QList<QThread *> threadList;
  int tmpStart = 0;
  for (int t = 0; t < threadAmount; t++) {
    int tmpEnd =
        (int)std::round((float)(dim.ly * (t + 1)) / (float)threadAmount);

    FlowBlurWorker *worker =
        new FlowBlurWorker(source_buf, flow_buf, out_buf, reference_buf, dim,
                           krnlen, tmpStart, tmpEnd, filterType);
    worker->start();
    threadList.append(worker);
    tmpStart = tmpEnd;
  }

  for (auto worker : threadList) {
    worker->wait();
    delete worker;
  }

  source_buf_ras->unlock();
  flow_buf_ras->unlock();
  if (reference_buf) reference_buf_ras->unlock();

  // render vector field with red & green channels
  if (ras32)
    setOutputRaster<TRaster32P, TPixel32>(out_buf, ras32, isLinear, gamma);
  else if (ras64)
    setOutputRaster<TRaster64P, TPixel64>(out_buf, ras64, isLinear, gamma);

  out_buf_ras->unlock();
}

bool Iwa_FlowBlurFx::doGetBBox(double frame, TRectD &bBox,
                               const TRenderSettings &info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

bool Iwa_FlowBlurFx::canHandle(const TRenderSettings &info, double frame) {
  return true;
}

FX_PLUGIN_IDENTIFIER(Iwa_FlowBlurFx, "iwa_FlowBlurFx")
