#include "iwa_tangentflowfx.h"

#include <QThreadPool>

namespace {
inline double dotProduct(const double2 v1, const double2 v2) {
  return v1.x * v2.x + v1.y * v2.y;
}
inline double clamp01(double val) {
  return (val > 1.0) ? 1.0 : (val < 0.0) ? 0.0 : val;
}

const int MAX_OFFSET = 10000;
};  // namespace
//------------------------------------------------------------
// obtain source tile brightness, normalizing 0 to 1
template <typename RASTER, typename PIXEL>
void Iwa_TangentFlowFx::setSourceTileToBuffer(const RASTER srcRas,
                                              double* buf) {
  double* buf_p = buf;
  for (int j = 0; j < srcRas->getLy(); j++) {
    PIXEL* pix = srcRas->pixels(j);
    for (int i = 0; i < srcRas->getLx(); i++, pix++, buf_p++) {
      // Value = 0.3R 0.59G 0.11B
      (*buf_p) = (double(pix->r) * 0.3 + double(pix->g) * 0.59 +
                  double(pix->b) * 0.11) /
                 double(PIXEL::maxChannelValue);
    }
  }
}

//------------------------------------------------------------

void Iwa_TangentFlowFx::alignFlowDirection(double2* flow_buf,
                                           const TDimension dim,
                                           const double2& pivotVec) {
  double2* fbuf_p = flow_buf;
  for (int i = 0; i < dim.lx * dim.ly; i++, fbuf_p++) {
    // 基準ベクトルに揃える
    if ((*fbuf_p).x * pivotVec.x + (*fbuf_p).y * pivotVec.y < 0.0) {
      (*fbuf_p).x = -(*fbuf_p).x;
      (*fbuf_p).y = -(*fbuf_p).y;
    }
  }
}

//------------------------------------------------------------
// render vector field in red & green channels
template <typename RASTER, typename PIXEL>
void Iwa_TangentFlowFx::setOutputRaster(double2* flow_buf, double* grad_buf,
                                        const RASTER dstRas) {
  double2* fbuf_p = flow_buf;
  double* gbuf_p  = grad_buf;
  for (int j = 0; j < dstRas->getLy(); j++) {
    PIXEL* pix = dstRas->pixels(j);
    for (int i = 0; i < dstRas->getLx(); i++, pix++, fbuf_p++, gbuf_p++) {
      double val;
      val = clamp01((*fbuf_p).x * 0.5 + 0.5) * (double)PIXEL::maxChannelValue;
      pix->r = (typename PIXEL::Channel)(val);
      val = clamp01((*fbuf_p).y * 0.5 + 0.5) * (double)PIXEL::maxChannelValue;
      pix->g = (typename PIXEL::Channel)(val);
      val    = clamp01(*gbuf_p) * (double)PIXEL::maxChannelValue;
      pix->b = (typename PIXEL::Channel)(val);
      pix->m = (typename PIXEL::Channel)PIXEL::maxChannelValue;
    }
  }
}

//------------------------------------------------------------
// compute the initial vector field.
// apply sobel filter, rotate by 90 degrees and normalize.
// also obtaining gradient magnitude (length of the vector length)
// empty regions will be filled by using Fast Sweeping Method

void SobelFilterWorker::run() {
  // the 5x5 Sobel filter is already rotated by 90 degrees.
  // ( i.e. converted as X = -y, Y = x )
  const double kernel_x[5][5] = {{0.25, 0.4, 0.5, 0.4, 0.25},
                                 {0.2, 0.5, 1., 0.5, 0.2},
                                 {0., 0., 0., 0., 0.},
                                 {-0.2, -0.5, -1., -0.5, -0.2},
                                 {-0.25, -0.4, -0.5, -0.4, -0.25}};
  const double kernel_y[5][5] = {{-0.25, -0.2, 0., 0.2, 0.25},
                                 {-0.4, -0.5, 0., 0.5, 0.4},
                                 {-0.5, -1., 0., 1., 0.5},
                                 {-0.4, -0.5, 0., 0.5, 0.4},
                                 {-0.25, -0.2, 0., 0.2, 0.25}};
  /*
  const double kernel_x[5][5] = {
  { 0.0297619, 0.047619, 0.0595238, 0.047619, 0.0297619 },
  { 0.0238,  0.0595238, 0.119, 0.0595238, 0.0238 },
  { 0.,  0.,  0.,  0.,  0. },
  { -0.0238, -0.0595238, -0.119,  -0.0595238, -0.0238 },
  { -0.0297619, -0.047619, -0.0595238, -0.047619, -0.0297619 }
  };
  const double kernel_y[5][5] = {
  { -0.0297619, -0.0238,  0.,  0.0238,  0.0297619 },
  { -0.047619, -0.0595238,  0.,  0.0595238,  0.047619 },
  { -0.0595238, -0.119,  0.,  0.119, 0.0595238 },
  { -0.047619, -0.0595238, 0.,  0.0595238, 0.047619 },
  { -0.0297619, -0.0238, 0.,  0.0238, 0.0297619 }
  };
  */

  auto source = [&](int xPos, int yPos) {
    return m_source_buf[yPos * m_dim.lx + xPos];
  };

  double2* flow_p    = &m_flow_buf[m_yFrom * m_dim.lx];
  double* grad_mag_p = &m_grad_mag_buf[m_yFrom * m_dim.lx];
  int2* offset_p     = &m_offset_buf[m_yFrom * m_dim.lx];

  for (int y = m_yFrom; y < m_yTo; y++) {
    for (int x = 0; x < m_dim.lx; x++, flow_p++, grad_mag_p++, offset_p++) {
      double x_accum = 0.;
      double y_accum = 0.;
      for (int ky = 0; ky < 5; ky++) {
        int yPos = y + ky - 2;  // y coordinate of sample pos
        if (yPos < 0) continue;
        if (yPos >= m_dim.ly) break;
        for (int kx = 0; kx < 5; kx++) {
          int xPos = x + kx - 2;  // x coordinate of sample pos
          if (xPos < 0) continue;
          if (xPos >= m_dim.lx) break;
          if (kx == 2 && ky == 2) continue;

          double sourceVal = source(xPos, yPos);
          x_accum += kernel_x[ky][kx] * sourceVal;
          y_accum += kernel_y[ky][kx] * sourceVal;
        }
      }
      // storing Gradient Magnitude
      *grad_mag_p = std::sqrt(x_accum * x_accum + y_accum * y_accum);
      (*flow_p).x = (*grad_mag_p == 0.) ? 0.0 : (x_accum / (*grad_mag_p));
      (*flow_p).y = (*grad_mag_p == 0.) ? 0.0 : (y_accum / (*grad_mag_p));

      // offset will be used later in Fast Sweeping Method
      if (*grad_mag_p < m_mag_threshold) {
        (*offset_p).x    = MAX_OFFSET;
        (*offset_p).y    = MAX_OFFSET;
        m_hasEmptyVector = true;
      } else {
        (*offset_p).x = 0;
        (*offset_p).y = 0;
      }
    }
  }
}

void Iwa_TangentFlowFx::computeInitialFlow(double* source_buf,
                                           double2* flow_buf,
                                           double* grad_mag_buf,
                                           const TDimension dim,
                                           double mag_threshold) {
  // empty regions will be filled by using Fast Sweeping Method
  // allocate offset buffer
  int2* offset_buf;
  TRasterGR8P offset_buf_ras(dim.lx * dim.ly * sizeof(int2), 1);
  offset_buf_ras->lock();
  offset_buf = (int2*)offset_buf_ras->getRawData();

  int activeThreadCount = QThreadPool::globalInstance()->activeThreadCount();
  // use half of the available threads
  int threadAmount = std::max(1, activeThreadCount / 2);
  QList<SobelFilterWorker*> threadList;

  int tmpStart = 0;
  for (int t = 0; t < threadAmount; t++) {
    int tmpEnd =
        (int)std::round((float)(dim.ly * (t + 1)) / (float)threadAmount);

    SobelFilterWorker* worker =
        new SobelFilterWorker(source_buf, flow_buf, grad_mag_buf, offset_buf,
                              mag_threshold, dim, tmpStart, tmpEnd);
    worker->start();
    threadList.append(worker);
    tmpStart = tmpEnd;
  }

  bool hasEmptyVector = false;
  for (auto worker : threadList) {
    worker->wait();
    hasEmptyVector = hasEmptyVector | worker->hasEmptyVector();
    delete worker;
  }

  // return if there is no empty region
  if (!hasEmptyVector) {
    offset_buf_ras->unlock();
    return;
  }

  auto getFlow    = [&](int2 xy) { return &flow_buf[xy.y * dim.lx + xy.x]; };
  auto getOffset  = [&](int2 xy) { return &offset_buf[xy.y * dim.lx + xy.x]; };
  auto getGradMag = [&](int2 xy) {
    return &grad_mag_buf[xy.y * dim.lx + xy.x];
  };

  // update vector field by using Fast Sweeping Method
  double2* neighbor_flow[2];
  int2* neighbor_offset[2];
  double* neighbor_gradMag[2];
  double2* flow_p;
  int2* offset_p;
  double* gradMag_p;
  int2 offset_incr[2];
  // positive/negative in y axis
  for (int ny = -1; ny <= 1; ny += 2) {
    offset_incr[0] = {0, ny};
    int y_start    = (ny == 1) ? 1 : dim.ly - 2;
    int y_end      = (ny == 1) ? dim.ly : 0;

    // positive/negative in x axis
    for (int nx = -1; nx <= 1; nx += 2) {
      offset_incr[1] = {nx, 0};
      int x_start    = (nx == 1) ? 1 : dim.lx - 2;
      int x_end      = (nx == 1) ? dim.lx : 0;

      for (int y = y_start; y != y_end; y += ny) {
        int2 startPos(x_start, y);
        for (int n = 0; n < 2; n++) {
          neighbor_flow[n]    = getFlow(startPos - offset_incr[n]);
          neighbor_offset[n]  = getOffset(startPos - offset_incr[n]);
          neighbor_gradMag[n] = getGradMag(startPos - offset_incr[n]);
        }
        flow_p    = getFlow(startPos);
        offset_p  = getOffset(startPos);
        gradMag_p = getGradMag(startPos);

        for (int x = x_start; x != x_end; x += nx, neighbor_flow[0] += nx,
                 neighbor_flow[1] += nx, flow_p += nx, neighbor_offset[0] += nx,
                 neighbor_offset[1] += nx, offset_p += nx,
                 neighbor_gradMag[0] += nx, neighbor_gradMag[1] += nx,
                 gradMag_p += nx) {
          // continue if this pixel is already filled
          if ((*offset_p).x == 0 && (*offset_p).y == 0) continue;
          // for each neighbor pixel
          for (int n = 0; n < 2; n++) {
            // continue if the neighbor is still empty
            if ((*neighbor_offset[n]).x == MAX_OFFSET ||
                (*neighbor_offset[n]).y == MAX_OFFSET)
              continue;
            int2 tmpOffset = (*neighbor_offset[n]) + offset_incr[n];
            if (tmpOffset.len2() < (*offset_p).len2()) {
              (*offset_p)  = tmpOffset;
              (*flow_p)    = (*neighbor_flow[n]);
              (*gradMag_p) = (*neighbor_gradMag[n]);
            }
          }
        }
      }
    }
  }

  offset_buf_ras->unlock();
}

//------------------------------------------------------------

Iwa_TangentFlowFx::Iwa_TangentFlowFx()
    : m_iteration(4)
    , m_kernelRadius(2.5)
    , m_threshold(0.15)
    , m_alignDirection(false)
    , m_pivotAngle(45.0) {
  addInputPort("Source", m_source);

  bindParam(this, "iteration", m_iteration);
  bindParam(this, "kernelRadius", m_kernelRadius);
  bindParam(this, "threshold", m_threshold);
  bindParam(this, "alignDirection", m_alignDirection);
  bindParam(this, "pivotAngle", m_pivotAngle);

  m_iteration->setValueRange(0, 10);
  m_kernelRadius->setMeasureName("fxLength");
  m_kernelRadius->setValueRange(0.5, 10);
  m_threshold->setValueRange(0.0, 1.0);

  m_pivotAngle->setValueRange(-180.0, 180.0);
}

//------------------------------------------------------------

void TangentFlowWorker::run() {
  // flow to be computed
  double2* flow_new_p = &m_flow_new_buf[m_yFrom * m_dim.lx];
  // current flow
  double2* flow_cur_p = &m_flow_cur_buf[m_yFrom * m_dim.lx];
  // Gradient Magnitude of the current pos
  double* grad_mag_p = &m_grad_mag_buf[m_yFrom * m_dim.lx];
  int kr2            = m_kernelRadius * m_kernelRadius;

  // loop for Y
  for (int y = m_yFrom; y < m_yTo; y++) {
    // loop for X
    for (int x = 0; x < m_dim.lx;
         x++, flow_new_p++, grad_mag_p++, flow_cur_p++) {
      // accumulation vector
      double2 flow_new_accum;

      // loop for kernel y
      for (int ky = -m_kernelRadius; ky <= m_kernelRadius; ky++) {
        int yPos = y + ky;
        // boundary condition
        if (yPos < 0) continue;
        if (yPos >= m_dim.ly) break;

        // loop for kernel x
        for (int kx = -m_kernelRadius; kx <= m_kernelRadius; kx++) {
          int xPos = x + kx;
          // boundary condition
          if (xPos < 0) continue;
          if (xPos >= m_dim.lx) break;

          // Eq.(2) continue if the sample pos is outside of the kernel
          if (kx * kx + ky * ky > kr2) continue;
          double2 flow_k = m_flow_cur_buf[yPos * m_dim.lx + xPos];
          if (flow_k.x == 0.0 && flow_k.y == 0.0) continue;

          // Eq.(3) calculate the magnitude weight function (wm)
          double grad_mag_k = m_grad_mag_buf[yPos * m_dim.lx + xPos];
          double w_m = (1.0 + std::tanh(grad_mag_k - (*grad_mag_p))) / 2.0;
          if (w_m == 0.0) continue;

          // Eq.(4) calculate the direction weight function (wd)
          double flowDot = dotProduct((*flow_cur_p), flow_k);
          double w_d     = std::abs(flowDot);

          // Eq.(5) calculate phi
          double phi = (flowDot > 0.) ? 1.0 : -1.0;

          // accumulate vector
          flow_new_accum.x += phi * w_m * w_d * flow_k.x;
          flow_new_accum.y += phi * w_m * w_d * flow_k.y;
        }
      }

      // normalize vector
      double len = std::sqrt(flow_new_accum.x * flow_new_accum.x +
                             flow_new_accum.y * flow_new_accum.y);
      if (len != 0.0) {
        flow_new_accum.x /= len;
        flow_new_accum.y /= len;
      }

      // update vector
      (*flow_new_p).x = flow_new_accum.x;
      (*flow_new_p).y = flow_new_accum.y;
    }
  }
}

void Iwa_TangentFlowFx::doCompute(TTile& tile, double frame,
                                  const TRenderSettings& settings) {
  if (!m_source.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  TDimension dim = tile.getRaster()->getSize();

  // std::cout << "Iwa_TangentFlowFx dim = (" << dim.lx << ", " << dim.ly <<
  // ")";

  double fac       = sqrt(fabs(settings.m_affine.det()));
  int kernelRadius = std::round(fac * m_kernelRadius->getValue(frame));
  if (kernelRadius == 0) kernelRadius = 1;
  int iterationCount = m_iteration->getValue();

  double mag_threshold = m_threshold->getValue(frame) / fac;

  TTile sourceTile;
  m_source->allocateAndCompute(sourceTile, tile.m_pos, dim, tile.getRaster(),
                               frame, settings);

  // allocate source image buffer
  double* source_buf;
  TRasterGR8P source_buf_ras(dim.lx * dim.ly * sizeof(double), 1);
  source_buf_ras->lock();
  source_buf = (double*)source_buf_ras->getRawData();

  // obtain source tile brightness, normalizing 0 to 1
  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  if (ras32)
    setSourceTileToBuffer<TRaster32P, TPixel32>(sourceTile.getRaster(),
                                                source_buf);
  else if (ras64)
    setSourceTileToBuffer<TRaster64P, TPixel64>(sourceTile.getRaster(),
                                                source_buf);

  // allocate flow buffers (for both current and new)
  double2 *flow_cur_buf, *flow_new_buf;
  TRasterGR8P flow_cur_buf_ras(dim.lx * dim.ly * sizeof(double2), 1);
  TRasterGR8P flow_new_buf_ras(dim.lx * dim.ly * sizeof(double2), 1);
  flow_cur_buf_ras->lock();
  flow_new_buf_ras->lock();
  flow_cur_buf = (double2*)flow_cur_buf_ras->getRawData();
  flow_new_buf = (double2*)flow_new_buf_ras->getRawData();

  // allocate Gradient Magnitude image buffer
  double* grad_mag_buf;
  TRasterGR8P grad_mag_buf_ras(dim.lx * dim.ly * sizeof(double), 1);
  grad_mag_buf_ras->lock();
  grad_mag_buf = (double*)grad_mag_buf_ras->getRawData();

  // compute the initial vector field.
  // apply sobel filter, rotate by 90 degrees and normalize.
  // also obtaining gradient magnitude (length of the vector length)
  // empty regions will be filled by using Fast Sweeping Method
  computeInitialFlow(source_buf, flow_cur_buf, grad_mag_buf, dim,
                     mag_threshold);

  source_buf_ras->unlock();

  int activeThreadCount = QThreadPool::globalInstance()->activeThreadCount();
  // use half of the available threads
  int threadAmount = std::max(1, activeThreadCount / 2);

  // start iteration
  for (int i = 0; i < iterationCount; i++) {
    QList<QThread*> threadList;
    int tmpStart = 0;
    for (int t = 0; t < threadAmount; t++) {
      int tmpEnd =
          (int)std::round((float)(dim.ly * (t + 1)) / (float)threadAmount);

      TangentFlowWorker* worker =
          new TangentFlowWorker(flow_cur_buf, flow_new_buf, grad_mag_buf, dim,
                                kernelRadius, tmpStart, tmpEnd);
      worker->start();
      threadList.append(worker);
      tmpStart = tmpEnd;
    }

    for (auto worker : threadList) {
      worker->wait();
      delete worker;
    }

    // swap buffer pointers
    double2* tmp = flow_cur_buf;
    flow_cur_buf = flow_new_buf;
    flow_new_buf = tmp;
  }

  flow_new_buf_ras->unlock();

  // 基準の角度に向きを合わせる
  if (m_alignDirection->getValue()) {
    double pivotAngle =
        m_pivotAngle->getValue(frame) * M_PI_180;  // convert to radian
    double2 pivotVec = {std::cos(pivotAngle), std::sin(pivotAngle)};
    alignFlowDirection(flow_cur_buf, dim, pivotVec);
  }

  // render vector field in red & green channels
  if (ras32)
    setOutputRaster<TRaster32P, TPixel32>(flow_cur_buf, grad_mag_buf, ras32);
  else if (ras64)
    setOutputRaster<TRaster64P, TPixel64>(flow_cur_buf, grad_mag_buf, ras64);

  flow_cur_buf_ras->unlock();
  grad_mag_buf_ras->unlock();
}

//------------------------------------------------------------

bool Iwa_TangentFlowFx::doGetBBox(double frame, TRectD& bBox,
                                  const TRenderSettings& info) {
  if (m_source.isConnected()) {
    bBox = TConsts::infiniteRectD;
    return true;
    // return m_source->doGetBBox(frame, bBox, info);
  }
  return false;
}

//------------------------------------------------------------

bool Iwa_TangentFlowFx::canHandle(const TRenderSettings& info, double frame) {
  return false;
}

FX_PLUGIN_IDENTIFIER(Iwa_TangentFlowFx, "iwa_TangentFlowFx")
