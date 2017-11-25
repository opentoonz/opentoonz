

#include "tbasefx.h"
#include "trop.h"
#include "tdoubleparam.h"
#include "tnotanimatableparam.h"
#include "trasterfx.h"
#include "tflash.h"
#include "tfxparam.h"
#include "tparamset.h"

//#define ALLOW_SHEAR

//==============================================================================

TGeometryFx::TGeometryFx() { setName(L"Geometry"); }

//--------------------------------------------------

void TGeometryFx::compute(TFlash &flash, int frame) {
  flash.multMatrix(getPlacement(frame));
  TRasterFx::compute(flash, frame);
}

//---------------------------------------------------------------

void TGeometryFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  TRasterFxPort *input = dynamic_cast<TRasterFxPort *>(getInputPort(0));
  assert(input);

  if (!input->isConnected()) return;

  if (!getActiveTimeRegion().contains(frame)) {
    TRasterFxP(input->getFx())->compute(tile, frame, ri);
    return;
  }

  if (!TRaster32P(tile.getRaster()) && !TRaster64P(tile.getRaster()))
    throw TException("AffineFx unsupported pixel type");

  TAffine aff1 = getPlacement(frame);
  TRenderSettings ri2(ri);
  ri2.m_affine = ri2.m_affine * aff1;

  TRasterFxP src = getInputPort("source")->getFx();
  src->compute(tile, frame, ri2);
  return;
}

//--------------------------------------------------

bool TGeometryFx::doGetBBox(double frame, TRectD &bBox,
                            const TRenderSettings &info) {
  TRasterFxPort *input = dynamic_cast<TRasterFxPort *>(getInputPort(0));
  assert(input);

  if (input->isConnected()) {
    TRasterFxP fx = input->getFx();
    assert(fx);
    bool ret = fx->doGetBBox(frame, bBox, info);
    if (getActiveTimeRegion().contains(frame))
      bBox = getPlacement(frame) * bBox;
    return ret;
  } else {
    bBox = TRectD();
    return false;
  }
  return true;
};

//--------------------------------------------------

std::string TGeometryFx::getAlias(double frame,
                                  const TRenderSettings &info) const {
  TGeometryFx *tthis = const_cast<TGeometryFx *>(this);
  TAffine affine     = tthis->getPlacement(frame);

  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)

  for (int i = 0; i < getInputPortCount(); ++i) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  return alias +
         (areAlmostEqual(affine.a11, 0) ? "0" : ::to_string(affine.a11, 5)) +
         "," +
         (areAlmostEqual(affine.a12, 0) ? "0" : ::to_string(affine.a12, 5)) +
         "," +
         (areAlmostEqual(affine.a13, 0) ? "0" : ::to_string(affine.a13, 5)) +
         "," +
         (areAlmostEqual(affine.a21, 0) ? "0" : ::to_string(affine.a21, 5)) +
         "," +
         (areAlmostEqual(affine.a22, 0) ? "0" : ::to_string(affine.a22, 5)) +
         "," +
         (areAlmostEqual(affine.a23, 0) ? "0" : ::to_string(affine.a23, 5)) +
         "]";
}

//--------------------------------------------------

void TGeometryFx::transform(double frame, int port, const TRectD &rectOnOutput,
                            const TRenderSettings &infoOnOutput,
                            TRectD &rectOnInput, TRenderSettings &infoOnInput) {
  rectOnInput = rectOnOutput;
  TAffine aff = getPlacement(frame);

  infoOnInput          = infoOnOutput;
  infoOnInput.m_affine = infoOnInput.m_affine * aff;
}

//==================================================

NaAffineFx::NaAffineFx(bool isDpiAffine)
    : m_aff(TAffine()), m_isDpiAffine(isDpiAffine) {
  addInputPort("source", m_port);
  setName(L"Geometry-NaAffineFx");
}

//--------------------------------------------------

TFx *NaAffineFx::clone(bool recursive) const {
  NaAffineFx *clonedFx = dynamic_cast<NaAffineFx *>(TFx::clone(recursive));
  assert(clonedFx);
  clonedFx->m_aff         = m_aff;
  clonedFx->m_isDpiAffine = m_isDpiAffine;
  return clonedFx;
}

//--------------------------------------------------

void NaAffineFx::compute(TFlash &flash, int frame) {
  TGeometryFx::compute(flash, frame);
}

//==================================================

void TRasterFx::compute(TFlash &flash, int frame) {
  for (int i = getInputPortCount() - 1; i >= 0; i--) {
    TFxPort *port = getInputPort(i);

    if (port->isConnected() && !port->isaControlPort()) {
      flash.pushMatrix();
      ((TRasterFxP)(port->getFx()))->compute(flash, frame);
      flash.popMatrix();
    }
  }
}

//--------------------------------------------------

FX_IDENTIFIER_IS_HIDDEN(NaAffineFx, "naAffineFx")

//==================================================================
//  ColumnColorFilterFx
//==================================================================

ColumnColorFilterFx::ColumnColorFilterFx() : m_colorFilter(TPixel::Black) {
  setName(L"ColumnColorFilterFx");
  addInputPort("source", m_port);
}

bool ColumnColorFilterFx::doGetBBox(double frame, TRectD &bBox,
                                    const TRenderSettings &info) {
  if (!m_port.isConnected()) return false;
  TRasterFxP fx = m_port.getFx();
  assert(fx);
  bool ret = fx->doGetBBox(frame, bBox, info);
  return ret;
}

void ColumnColorFilterFx::doCompute(TTile &tile, double frame,
                                    const TRenderSettings &ri) {
  if (!m_port.isConnected()) return;

  if (!TRaster32P(tile.getRaster()) && !TRaster64P(tile.getRaster()))
    throw TException("AffineFx unsupported pixel type");

  TRasterFxP src = m_port.getFx();
  src->compute(tile, frame, ri);

  TRop::applyColorScale(tile.getRaster(), m_colorFilter);
}

std::string ColumnColorFilterFx::getAlias(double frame,
                                          const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";
  if (m_port.isConnected()) {
    TRasterFxP ifx = m_port.getFx();
    assert(ifx);
    alias += ifx->getAlias(frame, info);
  }
  alias += ",";

  return alias + std::to_string(m_colorFilter.r) + "," +
         std::to_string(m_colorFilter.g) + "," +
         std::to_string(m_colorFilter.b) + "," +
         std::to_string(m_colorFilter.m) + "]";
}

//--------------------------------------------------

FX_IDENTIFIER_IS_HIDDEN(ColumnColorFilterFx, "columnColorFilterFx")

//==================================================================
//  Geometric Fx
//==================================================================

//==================================================================

//==================================================================

//------------------------------------------------------------------------------

class InvertFx final : public TBaseRasterFx {
  FX_DECLARATION(InvertFx)
  TRasterFxPort m_input;
  TBoolParamP m_redChan, m_greenChan, m_blueChan, m_alphaChan;

public:
  InvertFx()
      : m_redChan(true)
      , m_greenChan(true)
      , m_blueChan(true)
      , m_alphaChan(false) {
    addInputPort("source", m_input);
    //   bindParam(this, "red_channel"  , m_redChan);
    //   bindParam(this, "green_channel", m_greenChan);
    //   bindParam(this, "blue_channel" , m_blueChan);
    //   bindParam(this, "alpha_channel", m_alphaChan);
    setName(L"InvertFx");
  };

  ~InvertFx(){};

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  };

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_input.isConnected()) return;

    m_input->compute(tile, frame, ri);

    TRop::invert(tile.getRaster(), m_redChan->getValue(),
                 m_greenChan->getValue(), m_blueChan->getValue(),
                 m_alphaChan->getValue());
  }
};

//==================================================================
//  Video Fx
//==================================================================

//==================================================================

//===

// Geometric
// FX_IDENTIFIER(ScaleFx,       "scaleFx")
// FX_IDENTIFIER(MoveFx,        "moveFx")
// FX_IDENTIFIER(AffineFx,      "affineFx")
// FX_IDENTIFIER(CropFx,        "cropFx")

// Color
FX_IDENTIFIER(InvertFx, "invertFx")

// Video
// FX_IDENTIFIER(FieldFx,       "fieldFx")
// FX_IDENTIFIER(SwapFieldsFx,  "swapFieldsFx")
// FX_IDENTIFIER(DeInterlaceFx, "deInterlaceFx")
// FX_IDENTIFIER(InterlaceFx,   "interlaceFx")
