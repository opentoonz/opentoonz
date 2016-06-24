

#include "texception.h"
#include "tfxparam.h"
#include "trop.h"
#include "stdfx.h"
#include "trasterfx.h"
#include "tparamset.h"
#include "tparamuiconcept.h"

//-------------------------------------------------------------------

class BaseRaylitFx : public TStandardRasterFx {
protected:
  TRasterFxPort m_input;
  TPointParamP m_p;
  TDoubleParamP m_z;
  TDoubleParamP m_intensity;
  TDoubleParamP m_decay;
  TDoubleParamP m_smoothness;
  TBoolParamP m_includeInput;

public:
  BaseRaylitFx()
      : m_p(TPointD(0, 0))
      , m_z(300.0)
      , m_intensity(60)
      , m_decay(1.0)
      , m_smoothness(100)
      , m_includeInput(false) {
    m_p->getX()->setMeasureName("fxLength");
    m_p->getY()->setMeasureName("fxLength");

    bindParam(this, "p", m_p);
    bindParam(this, "z", m_z);
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "decay", m_decay);
    bindParam(this, "smoothness", m_smoothness);
    bindParam(this, "includeInput", m_includeInput);

    addInputPort("Source", m_input);
  }

  ~BaseRaylitFx() {}

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Center";
    concepts[0].m_params.push_back(m_p);
  }
};

//-------------------------------------------------------------------

bool BaseRaylitFx::doGetBBox(double frame, TRectD &bBox,
                             const TRenderSettings &info) {
  if (m_input.isConnected()) {
    bool ret      = m_input->doGetBBox(frame, bBox, info);
    if (ret) bBox = TConsts::infiniteRectD;
    return ret;
  } else {
    bBox = TRectD();
    return false;
  }
};

//-------------------------------------------------------------------

void BaseRaylitFx::doDryCompute(TRectD &rect, double frame,
                                const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  TRectD bboxIn;
  m_input->getBBox(frame, bboxIn, ri);
  if (bboxIn == TConsts::infiniteRectD) bboxIn = rect;

  TDimension sizeIn(std::max(tceil(bboxIn.getLx()), 1),
                    std::max(tceil(bboxIn.getLy()), 1));
  bboxIn = TRectD(bboxIn.getP00(), TDimensionD(sizeIn.lx, sizeIn.ly));
  m_input->dryCompute(bboxIn, frame, ri);
}

//---------------------------------------------------------------------------

int BaseRaylitFx::getMemoryRequirement(const TRectD &rect, double frame,
                                       const TRenderSettings &info) {
  TRectD bboxIn;
  m_input->getBBox(frame, bboxIn, info);
  if (bboxIn == TConsts::infiniteRectD) return -1;

  if (bboxIn.isEmpty()) return 0;

  return TRasterFx::memorySize(bboxIn, info.m_bpp);
}

//========================================================================================

class RaylitFx final : public BaseRaylitFx {
  FX_PLUGIN_DECLARATION(RaylitFx)

protected:
  TPixelParamP m_color;
  TBoolParamP m_invert;

public:
  RaylitFx() : m_color(TPixel(255, 80, 0)), m_invert(false) {
    bindParam(this, "color", m_color);
    bindParam(this, "invert", m_invert);
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;
};

FX_PLUGIN_IDENTIFIER(RaylitFx, "raylitFx")

//-------------------------------------------------------------------

void RaylitFx::doCompute(TTile &tileOut, double frame,
                         const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double scale = sqrt(fabs(ri.m_affine.det()));
  int shrink   = (ri.m_shrinkX + ri.m_shrinkY) / 2;

  TPointD p(ri.m_affine * m_p->getValue(frame));

  TRectD rectIn(tileOut.m_pos, TDimensionD(tileOut.getRaster()->getLx(),
                                           tileOut.getRaster()->getLy()));
  TRectD bboxIn;
  m_input->getBBox(frame, bboxIn, ri);
  if (bboxIn == TConsts::infiniteRectD) bboxIn = rectIn;

  if (!bboxIn.isEmpty()) {
    TPoint posIn, posOut;

    TTile tileIn;
    TDimension sizeIn(std::max(tceil(bboxIn.getLx()), 1),
                      std::max(tceil(bboxIn.getLy()), 1));
    m_input->allocateAndCompute(tileIn, bboxIn.getP00(), sizeIn,
                                tileOut.getRaster(), frame, ri);

    TRop::RaylitParams params;

    params.m_scale = scale;

    params.m_lightOriginSrc.x = params.m_lightOriginDst.x = p.x;
    params.m_lightOriginSrc.y = params.m_lightOriginDst.y = p.y;
    params.m_lightOriginSrc.z = params.m_lightOriginDst.z =
        (int)m_z->getValue(frame);
    params.m_color        = m_color->getValue(frame);
    params.m_intensity    = m_intensity->getValue(frame);
    params.m_decay        = m_decay->getValue(frame);
    params.m_smoothness   = m_smoothness->getValue(frame);
    params.m_invert       = m_invert->getValue();
    params.m_includeInput = m_includeInput->getValue();
    params.m_lightOriginSrc.x -= (int)tileIn.m_pos.x;
    params.m_lightOriginSrc.y -= (int)tileIn.m_pos.y;
    params.m_lightOriginDst.x -= (int)tileOut.m_pos.x;
    params.m_lightOriginDst.y -= (int)tileOut.m_pos.y;

    TRop::raylit(tileOut.getRaster(), tileIn.getRaster(), params);
  }
}

//========================================================================================

class ColorRaylitFx final : public BaseRaylitFx {
  FX_PLUGIN_DECLARATION(ColorRaylitFx)

public:
  ColorRaylitFx() : BaseRaylitFx() {}

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;
};

//-------------------------------------------------------------------

void ColorRaylitFx::doCompute(TTile &tileOut, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double scale = sqrt(fabs(ri.m_affine.det()));
  int shrink   = (ri.m_shrinkX + ri.m_shrinkY) / 2;

  TPointD p(ri.m_affine * m_p->getValue(frame));

  TRectD rectIn(tileOut.m_pos, TDimensionD(tileOut.getRaster()->getLx(),
                                           tileOut.getRaster()->getLy()));
  TRectD bboxIn;
  m_input->getBBox(frame, bboxIn, ri);
  if (bboxIn == TConsts::infiniteRectD) bboxIn = rectIn;

  if (!bboxIn.isEmpty()) {
    TPoint posIn, posOut;

    TTile tileIn;
    TDimension sizeIn(std::max(tceil(bboxIn.getLx()), 1),
                      std::max(tceil(bboxIn.getLy()), 1));
    m_input->allocateAndCompute(tileIn, bboxIn.getP00(), sizeIn,
                                tileOut.getRaster(), frame, ri);

    TRop::RaylitParams params;

    params.m_scale = scale;

    params.m_lightOriginSrc.x = params.m_lightOriginDst.x = p.x;
    params.m_lightOriginSrc.y = params.m_lightOriginDst.y = p.y;
    params.m_lightOriginSrc.z = params.m_lightOriginDst.z =
        (int)m_z->getValue(frame);
    params.m_intensity    = m_intensity->getValue(frame);
    params.m_decay        = m_decay->getValue(frame);
    params.m_smoothness   = m_smoothness->getValue(frame);
    params.m_includeInput = m_includeInput->getValue();
    params.m_lightOriginSrc.x -= (int)tileIn.m_pos.x;
    params.m_lightOriginSrc.y -= (int)tileIn.m_pos.y;
    params.m_lightOriginDst.x -= (int)tileOut.m_pos.x;
    params.m_lightOriginDst.y -= (int)tileOut.m_pos.y;

    TRop::glassRaylit(tileOut.getRaster(), tileIn.getRaster(), params);
  }
}

FX_PLUGIN_IDENTIFIER(ColorRaylitFx, "colorRaylitFx")
