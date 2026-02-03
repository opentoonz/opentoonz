#pragma once

#ifndef IWA_FLOWBLURFX
#define IWA_FLOWBLURFX

#include "stdfx.h"
#include "tfxparam.h"

#include <QThread>

struct double2 {
  double x = 0., y = 0.;
};

struct double4 {
  double x = 0., y = 0., z = 0, w = 0.;
};

enum FILTER_TYPE { Linear = 0, Gaussian, Flat };

class FlowBlurWorker : public QThread {
  double4 *m_source_buf;
  double2 *m_flow_buf;
  double4 *m_out_buf;
  double *m_reference_buf;
  TDimension m_dim;
  double m_krnlen;
  int m_yFrom, m_yTo;
  FILTER_TYPE m_filterType;

public:
  FlowBlurWorker(double4 *source_buf, double2 *flow_buf, double4 *out_buf,
                 double *ref_buf, TDimension dim, double krnlen, int yFrom,
                 int yTo, FILTER_TYPE filterType)
      : m_source_buf(source_buf)
      , m_flow_buf(flow_buf)
      , m_out_buf(out_buf)
      , m_reference_buf(ref_buf)
      , m_dim(dim)
      , m_krnlen(krnlen)
      , m_yFrom(yFrom)
      , m_yTo(yTo)
      , m_filterType(filterType) {}

  void run();
};

class Iwa_FlowBlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_FlowBlurFx)

protected:
  TRasterFxPort m_source;
  TRasterFxPort m_flow;
  TRasterFxPort m_reference;

  TDoubleParamP m_length;

  TBoolParamP m_linear;
  TDoubleParamP m_gamma;

  /*- リニア／ガウシアン／平均化 -*/
  TIntEnumParamP m_filterType;

  // Reference, Blue Channel of Flow Image
  TIntEnumParamP m_referenceMode;

  enum ReferenceMode { REFERENCE = 0, FLOW_BLUE_CHANNEL };

  template <typename RASTER, typename PIXEL>
  void setSourceTileToBuffer(const RASTER srcRas, double4 *buf, bool isLinear,
                             double gamma);
  template <typename RASTER, typename PIXEL>
  void setFlowTileToBuffer(const RASTER flowRas, double2 *buf, double *refBuf);
  template <typename RASTER, typename PIXEL>
  void setReferenceTileToBuffer(const RASTER srcRas, double *buf);

  template <typename RASTER, typename PIXEL>
  void setOutputRaster(double4 *out_buf, const RASTER dstRas, bool isLinear,
                       double gamma);

public:
  Iwa_FlowBlurFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;
};
#endif