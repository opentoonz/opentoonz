#include "texception.h"
#include "tfxparam.h"
#include "trop.h"
#include "stdfx.h"
#include "trasterfx.h"

//-------------------------------------------------------------------

class BlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(BlurFx)

  TRasterFxPort m_input;
  TDoubleParamP m_value;
  TBoolParamP m_useSSE;

public:
  BlurFx() : m_value(20), m_useSSE(true) {
    m_value->setMeasureName("fxLength");
    bindParam(this, "value", m_value);
    bindParam(this, "useSSE", m_useSSE, true);

    addInputPort("Source", m_input);
    m_value->setValueRange(0, std::numeric_limits<double>::max());
  }

  ~BlurFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);

      double blur = fabs(m_value->getValue(frame));
      int brad    = tceil(blur);
      bBox        = bBox.enlarge(brad);

      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void enlarge(const TRectD &bbox, TRectD &requestedRect, int blur);

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    if (m_value->getValue(frame) == 0) return true;
    return (isAlmostIsotropic(info.m_affine));
  }
};

FX_PLUGIN_IDENTIFIER(BlurFx, "blurFx")

//-------------------------------------------------------------------

//! Calculates the geometry we need for this node computation, given
//! the known input data (bbox), the requested output (requestedRect) and the
//! blur factor.
void BlurFx::enlarge(const TRectD &bbox, TRectD &requestedRect, int blur) {
  if (bbox.isEmpty() || requestedRect.isEmpty()) {
    requestedRect.empty();
    return;
  }

  // We are to find out the geometry that is useful for the fx computation.
  // There are some rules to follow:
  //  a) First, the interesting output we can generate is bounded by both
  //     the requestedRect and the blurred bbox (i.e. enlarged by the blur
  //     radius).
  //  b) Pixels contributing to any output are necessarily part of bbox - and
  //  only
  //     those which are blurrable into the requestedRect are useful to us
  //     (i.e. pixels contained in its enlargement by the blur radius).

  TRectD enlargedBBox(bbox.enlarge(blur));
  TRectD enlargedOut(requestedRect.enlarge(blur));

  TPointD originalP00(requestedRect.getP00());
  requestedRect = (enlargedOut * bbox) + (enlargedBBox * requestedRect);

  // Finally, make sure that the result is coherent with the original P00
  requestedRect -= originalP00;
  requestedRect.x0 = tfloor(requestedRect.x0);
  requestedRect.y0 = tfloor(requestedRect.y0);
  requestedRect.x1 = tceil(requestedRect.x1);
  requestedRect.y1 = tceil(requestedRect.y1);
  requestedRect += originalP00;
}

//-------------------------------------------------------------------

void BlurFx::transform(double frame, int port, const TRectD &rectOnOutput,
                       const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                       TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;
  rectOnInput = rectOnOutput;

  double blur =
      fabs(m_value->getValue(frame) * sqrt(fabs(infoOnOutput.m_affine.det())));

  if (blur == 0) {
    rectOnInput = rectOnOutput;
    return;
  }

  int brad = tceil(blur);
  TRectD bbox;
  m_input->getBBox(frame, bbox, infoOnInput);
  enlarge(bbox, rectOnInput, brad);
}

//-------------------------------------------------------------------

int BlurFx::getMemoryRequirement(const TRectD &rect, double frame,
                                 const TRenderSettings &info) {
  double blurValue =
      fabs(m_value->getValue(frame) * sqrt(fabs(info.m_affine.det())));

  if (blurValue == 0.0) return 0;

  int brad = tceil(blurValue);

  // Trop::blur is quite inefficient at the moment - it has to allocate a whole
  // raster of the same size of the input/output made of FLOAT QUADRUPLES...!
  return TRasterFx::memorySize(rect.enlarge(brad), sizeof(float) << 5);
}

//-------------------------------------------------------------------

void BlurFx::doCompute(TTile &tile, double frame,
                       const TRenderSettings &renderSettings) {
  if (!m_input.isConnected()) return;

  double shrink = 0.5 * (renderSettings.m_shrinkX + renderSettings.m_shrinkY);
  // Note: shrink is obsolete. It should be always = 1

  double blurValue = fabs(m_value->getValue(frame) *
                          sqrt(fabs(renderSettings.m_affine.det())) / shrink);

  if (blurValue == 0) {
    // No blur will be done. The underlying fx may pass by.
    m_input->compute(tile, frame, renderSettings);
    return;
  }

  int brad = tceil(blurValue);

  // Get the requested tile's geometry
  TRectD rectIn, rectOut;
  rectOut = TRectD(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                           tile.getRaster()->getLy()));

  // Retrieve the input interesting geometry - and ensure that something
  // actually has
  // to be computed
  if (!m_input->getBBox(frame, rectIn, renderSettings) || rectOut.isEmpty())
    return;

  enlarge(rectIn, rectOut, brad);

  if (rectOut.isEmpty()) return;

  // Finally, allocate and compute the blur argument
  TTile tileIn;
  m_input->allocateAndCompute(tileIn, rectOut.getP00(),
                              TDimension(rectOut.getLx(), rectOut.getLy()),
                              tile.getRaster(), frame, renderSettings);

  TPointD displacement(rectOut.getP00() - tile.m_pos);
  TRop::blur(tile.getRaster(), tileIn.getRaster(), blurValue, displacement.x,
             displacement.y, false);
}
