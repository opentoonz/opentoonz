

#include "tfxparam.h"
#include "tfx.h"

#include "tparamset.h"
#include "stdfx.h"
#include "time.h"
#include "trop.h"
#include "trasterfx.h"
#include "tpixelutils.h"
#include "tparamuiconcept.h"

#include "timage_io.h"

#include "toonz/tdistort.h"

//==============================================================================

namespace {
inline bool myIsEmpty(const TRectD &r) { return r.x0 >= r.x1 || r.y0 >= r.y1; }
}

//==============================================================================

class FreeDistortBaseFx : public TStandardRasterFx {
  enum { PERSPECTIVE, BILINEAR };

public:
  FreeDistortBaseFx(bool isCastShadow);
  ~FreeDistortBaseFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &ri) override;
  void onPortConnected(TFxPort *port);
  bool canHandle(const TRenderSettings &info, double frame) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  void safeTransform(double frame, int port, const TRectD &rectOnOutput,
                     const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                     TRenderSettings &infoOnInput, TRectD &inBBox);

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

private:
  bool m_isCastShadow;
  TRasterFxPort m_input;

  TIntEnumParamP m_distortType;

  TPointParamP m_p00_a;
  TPointParamP m_p00_b;
  TPointParamP m_p01_a;
  TPointParamP m_p01_b;
  TPointParamP m_p11_a;
  TPointParamP m_p11_b;
  TPointParamP m_p10_a;
  TPointParamP m_p10_b;

  TBoolParamP m_deactivate;
  TPixelParamP m_color;
  TDoubleParamP m_fade;
  TDoubleParamP m_upTransp;
  TDoubleParamP m_downTransp;
  TDoubleParamP m_upBlur;
  TDoubleParamP m_downBlur;
};

class FreeDistortFx final : public FreeDistortBaseFx {
  FX_PLUGIN_DECLARATION(FreeDistortFx)

public:
  FreeDistortFx() : FreeDistortBaseFx(false) {}
};

class CastShadowFx final : public FreeDistortBaseFx {
  FX_PLUGIN_DECLARATION(CastShadowFx)
public:
  CastShadowFx() : FreeDistortBaseFx(true) {}
};

//==============================================================================

FreeDistortBaseFx::FreeDistortBaseFx(bool isCastShadow)
    : m_deactivate(false)
    , m_color(TPixel32::Black)
    , m_fade(0.0)
    , m_upTransp(0.0)
    , m_downTransp(0.0)
    , m_upBlur(0.0)
    , m_downBlur(0.0)
    , m_isCastShadow(isCastShadow)
    , m_distortType(new TIntEnumParam(PERSPECTIVE, "Perspective")) {
  m_upBlur->setMeasureName("fxLength");
  m_downBlur->setMeasureName("fxLength");
  double ext = 400.;
  double inn = 400.;
  m_p00_a    = TPointD(-ext, -ext);
  m_p00_b    = TPointD(-inn, -inn);

  m_p01_a = TPointD(-ext, ext);
  m_p01_b = TPointD(-inn, inn);

  m_p11_a = TPointD(ext, ext);
  m_p11_b = TPointD(inn, inn);

  m_p10_a = TPointD(ext, -ext);
  m_p10_b = TPointD(inn, -inn);

  m_p00_b->getX()->setMeasureName("fxLength");
  m_p00_b->getY()->setMeasureName("fxLength");
  m_p10_b->getX()->setMeasureName("fxLength");
  m_p10_b->getY()->setMeasureName("fxLength");
  m_p01_b->getX()->setMeasureName("fxLength");
  m_p01_b->getY()->setMeasureName("fxLength");
  m_p11_b->getX()->setMeasureName("fxLength");
  m_p11_b->getY()->setMeasureName("fxLength");

  m_p00_a->getX()->setMeasureName("fxLength");
  m_p00_a->getY()->setMeasureName("fxLength");
  m_p10_a->getX()->setMeasureName("fxLength");
  m_p10_a->getY()->setMeasureName("fxLength");
  m_p01_a->getX()->setMeasureName("fxLength");
  m_p01_a->getY()->setMeasureName("fxLength");
  m_p11_a->getX()->setMeasureName("fxLength");
  m_p11_a->getY()->setMeasureName("fxLength");

  bindParam(this, "distort_type", m_distortType);

  bindParam(this, "bottom_left_b", m_p00_b);
  bindParam(this, "bottom_right_b", m_p10_b);
  bindParam(this, "top_left_b", m_p01_b);
  bindParam(this, "top_right_b", m_p11_b);

  bindParam(this, "bottom_left_a", m_p00_a);
  bindParam(this, "bottom_right_a", m_p10_a);
  bindParam(this, "top_left_a", m_p01_a);
  bindParam(this, "top_right_a", m_p11_a);

  if (isCastShadow) {
    bindParam(this, "color", m_color);
    bindParam(this, "fade", m_fade);
    bindParam(this, "up_transp", m_upTransp);
    bindParam(this, "down_transp", m_downTransp);
    bindParam(this, "up_blur", m_upBlur);
    bindParam(this, "down_blur", m_downBlur);

    m_color->enableMatte(false);
    m_fade->setValueRange(0.0, 100.0);
    m_upTransp->setValueRange(0.0, 100.0);
    m_downTransp->setValueRange(0.0, 100.0);
    m_upBlur->setValueRange(0.0, (std::numeric_limits<double>::max)());
    m_downBlur->setValueRange(0.0, (std::numeric_limits<double>::max)());
  }

  bindParam(this, "deactivate", m_deactivate);
  addInputPort("source", m_input);

  m_p00_a->getX()->setValueRange(-1000, 1000);
  m_p00_b->getX()->setValueRange(-1000, 1000);

  m_p00_a->getY()->setValueRange(-1000, 1000);
  m_p00_b->getY()->setValueRange(-1000, 1000);

  m_p01_a->getX()->setValueRange(-1000, 1000);
  m_p01_b->getX()->setValueRange(-1000, 1000);

  m_p01_a->getY()->setValueRange(-1000, 1000);
  m_p01_b->getY()->setValueRange(-1000, 1000);

  m_p11_a->getX()->setValueRange(-1000, 1000);
  m_p11_b->getX()->setValueRange(-1000, 1000);

  m_p11_a->getY()->setValueRange(-1000, 1000);
  m_p11_b->getY()->setValueRange(-1000, 1000);

  m_p10_a->getX()->setValueRange(-1000, 1000);
  m_p10_b->getX()->setValueRange(-1000, 1000);

  m_p10_a->getY()->setValueRange(-1000, 1000);
  m_p10_b->getY()->setValueRange(-1000, 1000);

  m_distortType->addItem(BILINEAR, "Bilinear");
}

//------------------------------------------------------------------------

FreeDistortBaseFx::~FreeDistortBaseFx() {}

//------------------------------------------------------------------------

bool FreeDistortBaseFx::canHandle(const TRenderSettings &info, double frame) {
  // Blurs cant deal with anisotropic affines, so these are retained above
  return m_upBlur->getValue(frame) == 0 && m_downBlur->getValue(frame) == 0;
}

//------------------------------------------------------------------------

bool FreeDistortBaseFx::doGetBBox(double frame, TRectD &bBox,
                                  const TRenderSettings &info) {
  if (m_input.isConnected()) {
    // We'll treat this fx like a zerary one. The idea is, since
    // the task of taking images and pre-images of rects through
    // these kind of distortions is definitely non-trivial, we'll
    // simply avoid doing so...
    bBox = TConsts::infiniteRectD;
    return true;

    /*
bool ret = m_input->doGetBBox(frame, bBox);

if(m_deactivate->getValue())
return ret;

TPointD p00_a = m_p00_a->getValue(frame);
TPointD p00_b = m_p00_b->getValue(frame);
TPointD p10_a = m_p10_a->getValue(frame);
TPointD p10_b = m_p10_b->getValue(frame);
TPointD p01_a = m_p01_a->getValue(frame);
TPointD p01_b = m_p01_b->getValue(frame);
TPointD p11_a = m_p11_a->getValue(frame);
TPointD p11_b = m_p11_b->getValue(frame);

    if (m_isCastShadow)
{
            std::swap(p00_a, p01_a);
std::swap(p10_a, p11_a);
    }*/

    /*TRectD source;
source.x0 = std::min(p00_b.x, p10_b.x, p01_b.x, p11_b.x);
source.y0 = std::min(p00_b.y, p10_b.y, p01_b.y, p11_b.y);
source.x1 = std::max(p00_b.x, p10_b.x, p01_b.x, p11_b.x);
source.y1 = std::max(p00_b.y, p10_b.y, p01_b.y, p11_b.y);
bBox *= source;

int distortType = m_distortType->getValue();
if(distortType == PERSPECTIVE)
{
PerspectiveDistorter distorter(
p00_b, p10_b, p01_b, p11_b,
p00_a, p10_a, p01_a, p11_a
);
bBox = distorter.map(bBox);
}
else if(distortType == BILINEAR)
{
BilinearDistorter distorter(
p00_b, p10_b, p01_b, p11_b,
p00_a, p10_a, p01_a, p11_a
);
bBox = distorter.map(bBox);
}
else assert(0);*/

    // Further, we will also avoid the assumption that the bbox
    // is defined by that of destination points...
    /*TRectD result;
result.x0 = std::min(p00_a.x, p10_a.x, p01_a.x, p11_a.x);
result.y0 = std::min(p00_a.y, p10_a.y, p01_a.y, p11_a.y);
result.x1 = std::max(p00_a.x, p10_a.x, p01_a.x, p11_a.x);
result.y1 = std::max(p00_a.y, p10_a.y, p01_a.y, p11_a.y);
bBox = result;    //bBox *= result;*/
  } else {
    bBox = TRectD();
    return false;
  }

  if (m_upBlur->getValue(frame) > 0 || m_downBlur->getValue(frame) > 0) {
    int brad = std::max((int)m_upBlur->getValue(frame),
                        (int)m_downBlur->getValue(frame));
    bBox.x0 -= brad;
    bBox.x1 += brad;
    bBox.y0 -= brad;
    bBox.y1 += brad;
  }

  return true;
}

// ------------------------------------------------------------------------

void FreeDistortBaseFx::transform(double frame, int port,
                                  const TRectD &rectOnOutput,
                                  const TRenderSettings &infoOnOutput,
                                  TRectD &rectOnInput,
                                  TRenderSettings &infoOnInput) {
  TPointD p00_b = m_p00_b->getValue(frame);
  TPointD p10_b = m_p10_b->getValue(frame);
  TPointD p01_b = m_p01_b->getValue(frame);
  TPointD p11_b = m_p11_b->getValue(frame);

  TPointD p00_a = m_p00_a->getValue(frame);
  TPointD p10_a = m_p10_a->getValue(frame);
  TPointD p01_a = m_p01_a->getValue(frame);
  TPointD p11_a = m_p11_a->getValue(frame);

  if (m_isCastShadow) {
    // Shadows are mirrored
    std::swap(p00_a, p01_a);
    std::swap(p10_a, p11_a);
  }

  // Build the input affine
  infoOnInput = infoOnOutput;

  double scale = 0;
  scale        = std::max(scale, norm(p10_a - p00_a) / norm(p10_b - p00_b));
  scale        = std::max(scale, norm(p01_a - p00_a) / norm(p01_b - p00_b));
  scale        = std::max(scale, norm(p11_a - p10_a) / norm(p11_b - p10_b));
  scale        = std::max(scale, norm(p11_a - p01_a) / norm(p11_b - p01_b));

  scale *= sqrt(fabs(infoOnInput.m_affine.det()));

  if (infoOnOutput.m_isSwatch) {
    // Swatch viewers work out a lite version of the distort: only contractions
    // are passed below in the schematic - so that input memory load is always
    // at minimum.
    // This is allowed in order to make fx adjusts the less frustrating as
    // possible.
    if (scale > 1) scale = 1;
  }

  infoOnInput.m_affine = TScale(scale);

  // Build rectOnInput
  p00_a = infoOnOutput.m_affine * p00_a;
  p10_a = infoOnOutput.m_affine * p10_a;
  p01_a = infoOnOutput.m_affine * p01_a;
  p11_a = infoOnOutput.m_affine * p11_a;

  p00_b = infoOnInput.m_affine * p00_b;
  p10_b = infoOnInput.m_affine * p10_b;
  p01_b = infoOnInput.m_affine * p01_b;
  p11_b = infoOnInput.m_affine * p11_b;

  if (m_distortType->getValue() == PERSPECTIVE) {
    PerspectiveDistorter pd(p00_b, p10_b, p01_b, p11_b, p00_a, p10_a, p01_a,
                            p11_a);

    rectOnInput = ((TQuadDistorter *)&pd)->invMap(rectOnOutput);
  } else {
    BilinearDistorter bd(p00_b, p10_b, p01_b, p11_b, p00_a, p10_a, p01_a,
                         p11_a);

    rectOnInput = ((TQuadDistorter *)&bd)->invMap(rectOnOutput);
  }

  if (rectOnInput.x0 != TConsts::infiniteRectD.x0)
    rectOnInput.x0 = tfloor(rectOnInput.x0);
  if (rectOnInput.y0 != TConsts::infiniteRectD.y0)
    rectOnInput.y0 = tfloor(rectOnInput.y0);
  if (rectOnInput.x1 != TConsts::infiniteRectD.x1)
    rectOnInput.x1 = tceil(rectOnInput.x1);
  if (rectOnInput.y1 != TConsts::infiniteRectD.y1)
    rectOnInput.y1 = tceil(rectOnInput.y1);
}

// ------------------------------------------------------------------------

void FreeDistortBaseFx::safeTransform(double frame, int port,
                                      const TRectD &rectOnOutput,
                                      const TRenderSettings &infoOnOutput,
                                      TRectD &rectOnInput,
                                      TRenderSettings &infoOnInput,
                                      TRectD &inBBox) {
  assert(port == 0 && m_input.isConnected());

  if (m_deactivate->getValue()) {
    infoOnInput = infoOnOutput;
    rectOnInput = rectOnOutput;
    m_input->getBBox(frame, inBBox, infoOnInput);
    return;
  }

  // Avoid the singular matrix cases
  if (fabs(infoOnOutput.m_affine.det()) < 1.e-3) {
    infoOnInput = infoOnOutput;
    rectOnInput.empty();
    m_input->getBBox(frame, inBBox, infoOnInput);
    return;
  }

  transform(frame, port, rectOnOutput, infoOnOutput, rectOnInput, infoOnInput);

  m_input->getBBox(frame, inBBox, infoOnInput);

  // In case inBBox is infinite, such as with gradients, also intersect with the
  // input quadrilateral's bbox
  if (inBBox == TConsts::infiniteRectD) {
    TPointD affP00_b(infoOnInput.m_affine * m_p00_b->getValue(frame));
    TPointD affP10_b(infoOnInput.m_affine * m_p10_b->getValue(frame));
    TPointD affP01_b(infoOnInput.m_affine * m_p01_b->getValue(frame));
    TPointD affP11_b(infoOnInput.m_affine * m_p11_b->getValue(frame));

    TRectD source;
    source.x0 = std::min({affP00_b.x, affP10_b.x, affP01_b.x, affP11_b.x});
    source.y0 = std::min({affP00_b.y, affP10_b.y, affP01_b.y, affP11_b.y});
    source.x1 = std::max({affP00_b.x, affP10_b.x, affP01_b.x, affP11_b.x});
    source.y1 = std::max({affP00_b.y, affP10_b.y, affP01_b.y, affP11_b.y});

    rectOnInput *= source;
  }
}

// ------------------------------------------------------------------------

void FreeDistortBaseFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 14];

  concepts[0].m_type = TParamUIConcept::QUAD;
  concepts[0].m_params.push_back(m_p00_b);
  concepts[0].m_params.push_back(m_p10_b);
  concepts[0].m_params.push_back(m_p11_b);
  concepts[0].m_params.push_back(m_p01_b);

  concepts[1].m_type = TParamUIConcept::QUAD;
  concepts[1].m_params.push_back(m_p00_a);
  concepts[1].m_params.push_back(m_p10_a);
  concepts[1].m_params.push_back(m_p11_a);
  concepts[1].m_params.push_back(m_p01_a);

  concepts[2].m_type = TParamUIConcept::VECTOR;
  concepts[2].m_params.push_back(m_p00_b);
  concepts[2].m_params.push_back(m_p00_a);

  concepts[3].m_type = TParamUIConcept::VECTOR;
  concepts[3].m_params.push_back(m_p10_b);
  concepts[3].m_params.push_back(m_p10_a);

  concepts[4].m_type = TParamUIConcept::VECTOR;
  concepts[4].m_params.push_back(m_p01_b);
  concepts[4].m_params.push_back(m_p01_a);

  concepts[5].m_type = TParamUIConcept::VECTOR;
  concepts[5].m_params.push_back(m_p11_b);
  concepts[5].m_params.push_back(m_p11_a);

  concepts[6].m_type  = TParamUIConcept::POINT;
  concepts[6].m_label = "Bottom Left Src";
  concepts[6].m_params.push_back(m_p00_b);

  concepts[7].m_type  = TParamUIConcept::POINT;
  concepts[7].m_label = "Bottom Right Src";
  concepts[7].m_params.push_back(m_p10_b);

  concepts[8].m_type  = TParamUIConcept::POINT;
  concepts[8].m_label = "Top Left Src";
  concepts[8].m_params.push_back(m_p01_b);

  concepts[9].m_type  = TParamUIConcept::POINT;
  concepts[9].m_label = "Top Right Src";
  concepts[9].m_params.push_back(m_p11_b);

  concepts[10].m_type  = TParamUIConcept::POINT;
  concepts[10].m_label = "Bottom Left Dst";
  concepts[10].m_params.push_back(m_p00_a);

  concepts[11].m_type  = TParamUIConcept::POINT;
  concepts[11].m_label = "Bottom Right Dst";
  concepts[11].m_params.push_back(m_p10_a);

  concepts[12].m_type  = TParamUIConcept::POINT;
  concepts[12].m_label = "Top Left Dst";
  concepts[12].m_params.push_back(m_p01_a);

  concepts[13].m_type  = TParamUIConcept::POINT;
  concepts[13].m_label = "Top Right Dst";
  concepts[13].m_params.push_back(m_p11_a);
}

// ------------------------------------------------------------------------

namespace {

template <typename PIX>
void doFade(TRasterPT<PIX> &ras, PIX col, double intensity) {
  double maxChannelValueD = PIX::maxChannelValue;

  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIX *pix    = ras->pixels(j);
    PIX *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (pix->m > 0) {
        double factor = pix->m / maxChannelValueD;
        int val;
        val    = troundp(pix->r + intensity * (col.r * factor - pix->r));
        pix->r = (val > PIX::maxChannelValue) ? PIX::maxChannelValue : val;
        val    = troundp(pix->g + intensity * (col.g * factor - pix->g));
        pix->g = (val > PIX::maxChannelValue) ? PIX::maxChannelValue : val;
        val    = troundp(pix->b + intensity * (col.b * factor - pix->b));
        pix->b = (val > PIX::maxChannelValue) ? PIX::maxChannelValue : val;
      }
      ++pix;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------

template <typename PIX>
void doTransparency(TRasterPT<PIX> &ras, double downTransp, double upTransp,
                    double y0, double y1) {
  assert(y0 <= 0.0 && y1 >= ras->getLy() - 1);

  double den = y1 - y0;

  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    double transp = (1 - downTransp) + (downTransp - upTransp) * (j - y0) / den;
    PIX *pix      = ras->pixels(j);
    PIX *endPix   = pix + ras->getLx();
    while (pix < endPix) {
      if (pix->m > 0) {
        pix->r = troundp(pix->r * transp);
        pix->g = troundp(pix->g * transp);
        pix->b = troundp(pix->b * transp);
        pix->m = troundp(pix->m * transp);
      }
      ++pix;
    }
  }
  ras->unlock();
}

// ------------------------------------------------------------------------

template <typename PIX>
inline void filterPixel(PIX *bufin, int wrap, PIX *bufout, double blur,
                        int coord, int maxVal) {
  int rad, k, index;
  double coeff, weight;
  double valr = 0.0, valg = 0.0, valb = 0.0, valm = 0.0;

  rad   = tceil(blur);
  coeff = 1.0 / rad;

  valr = bufin[0].r;
  valg = bufin[0].g;
  valb = bufin[0].b;
  valm = bufin[0].m;

  for (k = 1, index = wrap, weight = (1 - coeff); k < rad;
       k++, index += wrap, weight -= coeff) {
    if (k + coord < maxVal) {
      if (-k + coord >= 0) {
        valr += (bufin[index].r + bufin[-index].r) * weight;
        valg += (bufin[index].g + bufin[-index].g) * weight;
        valb += (bufin[index].b + bufin[-index].b) * weight;
        valm += (bufin[index].m + bufin[-index].m) * weight;
      } else {
        valr += bufin[index].r * weight;
        valg += bufin[index].g * weight;
        valb += bufin[index].b * weight;
        valm += bufin[index].m * weight;
      }
    } else if (-k + coord >= 0) {
      if (k + coord < maxVal) {
        valr += (bufin[index].r + bufin[-index].r) * weight;
        valg += (bufin[index].g + bufin[-index].g) * weight;
        valb += (bufin[index].b + bufin[-index].b) * weight;
        valm += (bufin[index].m + bufin[-index].m) * weight;
      } else {
        valr += bufin[-index].r * weight;
        valg += bufin[-index].g * weight;
        valb += bufin[-index].b * weight;
        valm += bufin[-index].m * weight;
      }
    }
  }

  valr *= coeff;
  valg *= coeff;
  valb *= coeff;
  valm *= coeff;

  bufout->r = troundp(valr);
  bufout->g = troundp(valg);
  bufout->b = troundp(valb);
  bufout->m = troundp(valm);
}

/*----------------------------------------------------------------------------*/

template <typename PIX>
void doBlur(TRasterPT<PIX> &r, double blur0, double blur1, double transp0,
            double transp1, double y0, double y1) {
  int lx, ly, wrap;
  int i, j, brad, blur_max;
  PIX *bufin, *bufout, *pixin, *pixout, *row;
  double cur_blur, actual_blur, blur_incr, cur_transp, actual_transp,
      transp_incr;

  lx       = r->getLx();
  ly       = r->getLy();
  wrap     = r->getWrap();
  blur_max = (int)std::max(blur0, blur1);
  brad     = tceil(blur_max);

  // assert(y0 <= brad && y1 >= r->getLy()-1 - 2 * brad + y0);

  double den = y1 - y0;

  blur_incr   = (blur1 - blur0) / den;
  transp_incr = (transp0 - transp1) / den;

  row = new PIX[std::max(lx, ly)];
  r->lock();
  bufin = r->pixels(0);
  for (i = 0, cur_blur = blur0 + (blur1 - blur0) * (i - y0) / den, 0.0,
      actual_blur = std::max(cur_blur, 0.0);
       i < ly; i++, bufin += wrap, cur_blur += blur_incr,
      actual_blur = std::max(cur_blur, 0.0)) {
    pixin = bufin;
    for (j = 0; j < lx; j++, pixin++) {
      if (cur_blur > 1.0)
        filterPixel(pixin, 1, row + j, actual_blur, j, lx);
      else
        row[j] = *pixin;
    }
    for (j = 0; j < lx; j++) bufin[j] = row[j];
  }

  bufin  = r->pixels(0);
  bufout = r->pixels(0);
  for (i = 0; i < lx; i++, bufin++, bufout++) {
    pixin  = bufin;
    pixout = bufout;

    for (j = 0, cur_blur = blur0 + (blur1 - blur0) * (j - y0) / den,
        actual_blur = std::max(cur_blur, 0.0);
         j < ly; j++, pixin += wrap, cur_blur += blur_incr,
        actual_blur = std::max(cur_blur, 0.0)) {
      if (cur_blur > 1.0)
        filterPixel(pixin, wrap, row + j, actual_blur, j, ly);
      else
        row[j] = *pixin;
    }

    if (transp0 > 0 || transp1 > 0)
      for (j            = 0,
          cur_transp    = 1 - transp0 + (transp0 - transp1) * (j - y0) / den,
          actual_transp = std::max(cur_transp, 0.0);
           j < ly; j++, pixout += wrap, cur_transp += transp_incr,
          actual_transp = std::max(cur_transp, 0.0)) {
        pixout->r = troundp(row[j].r * actual_transp);
        pixout->g = troundp(row[j].g * actual_transp);
        pixout->b = troundp(row[j].b * actual_transp);
        pixout->m = troundp(row[j].m * actual_transp);
      }
    else
      for (j = 0; j < ly; j++, pixout += wrap) {
        pixout->r = row[j].r;
        pixout->g = row[j].g;
        pixout->b = row[j].b;
        pixout->m = row[j].m;
      }
  }
  r->unlock();
  delete[] row;
}
}

// ------------------------------------------------------------------------

void FreeDistortBaseFx::doDryCompute(TRectD &rect, double frame,
                                     const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  if (m_deactivate->getValue()) {
    m_input->dryCompute(rect, frame, info);
    return;
  }

  TRectD rectOnInput;
  TRenderSettings infoOnInput;
  TRectD inBBox;

  safeTransform(frame, 0, rect, info, rectOnInput, infoOnInput, inBBox);

  rectOnInput *= inBBox;

  if (!myIsEmpty(rectOnInput))
    m_input->dryCompute(rectOnInput, frame, infoOnInput);
}

// ------------------------------------------------------------------------

void FreeDistortBaseFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  // Upon deactivation, this fx does nothing.
  if (m_deactivate->getValue()) {
    m_input->compute(tile, frame, ri);
    return;
  }

  // Get the source quad
  TPointD p00_b = m_p00_b->getValue(frame);
  TPointD p10_b = m_p10_b->getValue(frame);
  TPointD p01_b = m_p01_b->getValue(frame);
  TPointD p11_b = m_p11_b->getValue(frame);

  // Get destination quad
  TPointD p00_a = m_p00_a->getValue(frame);
  TPointD p10_a = m_p10_a->getValue(frame);
  TPointD p01_a = m_p01_a->getValue(frame);
  TPointD p11_a = m_p11_a->getValue(frame);

  if (m_isCastShadow) {
    // Shadows are mirrored
    std::swap(p00_a, p01_a);
    std::swap(p10_a, p11_a);
  }

  // Get requested tile's geometry
  TRasterP tileRas(tile.getRaster());
  TRectD tileRect(convert(tileRas->getBounds()) + tile.m_pos);

  // Call transform to get the minimal rectOnInput
  TRectD inRect;
  TRenderSettings riNew;
  TRectD inBBox;

  safeTransform(frame, 0, tileRect, ri, inRect, riNew, inBBox);

  // Intersect with the bbox
  inRect *= inBBox;

  if (myIsEmpty(inRect)) return;

  double scale = ri.m_affine.a11;

  double downBlur = m_downBlur->getValue(frame) * scale;
  double upBlur   = m_upBlur->getValue(frame) * scale;
  int brad        = tceil(std::max(downBlur, upBlur));

  inRect = inRect.enlarge(brad);

  TDimension inRectSize(tceil(inRect.getLx()), tceil(inRect.getLy()));

  TTile inTile;
  m_input->allocateAndCompute(inTile, inRect.getP00(), inRectSize, tileRas,
                              frame, riNew);

  TPointD inTilePosRi = inTile.m_pos;

  // Update quads by the scale factors
  p00_b = riNew.m_affine * p00_b;
  p10_b = riNew.m_affine * p10_b;
  p01_b = riNew.m_affine * p01_b;
  p11_b = riNew.m_affine * p11_b;

  p00_a = ri.m_affine * p00_a;
  p10_a = ri.m_affine * p10_a;
  p01_a = ri.m_affine * p01_a;
  p11_a = ri.m_affine * p11_a;

  PerspectiveDistorter perpDistorter(p00_b - inTile.m_pos, p10_b - inTile.m_pos,
                                     p01_b - inTile.m_pos, p11_b - inTile.m_pos,
                                     p00_a, p10_a, p01_a, p11_a);

  BilinearDistorter bilDistorter(p00_b - inTile.m_pos, p10_b - inTile.m_pos,
                                 p01_b - inTile.m_pos, p11_b - inTile.m_pos,
                                 p00_a, p10_a, p01_a, p11_a);

  TQuadDistorter *distorter;
  if (m_distortType->getValue() == PERSPECTIVE)
    distorter = &perpDistorter;
  else if (m_distortType->getValue() == BILINEAR)
    distorter = &bilDistorter;
  else
    assert(0);

  if (m_isCastShadow) {
    TRaster32P ras32 = inTile.getRaster();
    TRaster64P ras64 = inTile.getRaster();

    if (ras32) {
      if (m_fade->getValue(frame) > 0)
        doFade(ras32, m_color->getValue(frame),
               m_fade->getValue(frame) / 100.0);
      if (brad > 0)
        doBlur(ras32, upBlur, downBlur, m_upTransp->getValue(frame) / 100.0,
               m_downTransp->getValue(frame) / 100.0,
               inBBox.y0 - inTile.m_pos.y, inBBox.y1 - inTile.m_pos.y);
      else if (m_upTransp->getValue(frame) > 0 ||
               m_downTransp->getValue(frame) > 0)
        doTransparency(ras32, m_upTransp->getValue(frame) / 100.0,
                       m_downTransp->getValue(frame) / 100.0,
                       inBBox.y0 - inTile.m_pos.y, inBBox.y1 - inTile.m_pos.y);
    } else if (ras64) {
      if (m_fade->getValue(frame) > 0)
        doFade(ras64, toPixel64(m_color->getValue(frame)),
               m_fade->getValue(frame) / 100.0);
      if (brad > 0)
        doBlur(ras64, upBlur, downBlur, m_upTransp->getValue(frame) / 100.0,
               m_downTransp->getValue(frame) / 100.0,
               inBBox.y0 - inTile.m_pos.y, inBBox.y1 - inTile.m_pos.y);
      else if (m_upTransp->getValue(frame) > 0 ||
               m_downTransp->getValue(frame) > 0)
        doTransparency(ras64, m_upTransp->getValue(frame) / 100.0,
                       m_downTransp->getValue(frame) / 100.0,
                       inBBox.y0 - inTile.m_pos.y, inBBox.y1 - inTile.m_pos.y);
    } else
      assert(false);
  }

  distort(tileRas, inTile.getRaster(), *distorter, convert(tile.m_pos),
          TRop::Bilinear);
}

//-------------------------------------------------------------------------

int FreeDistortBaseFx::getMemoryRequirement(const TRectD &rect, double frame,
                                            const TRenderSettings &info) {
  if (!m_input.isConnected()) return 0;

  TRectD inRect;
  TRenderSettings riNew;
  TRectD inBBox;
  safeTransform(frame, 0, rect, info, inRect, riNew, inBBox);
  inRect *= inBBox;

  return TRasterFx::memorySize(inRect, riNew.m_bpp);
}

FX_PLUGIN_IDENTIFIER(FreeDistortFx, "freeDistortFx");
FX_PLUGIN_IDENTIFIER(CastShadowFx, "castShadowFx");

/*----------------------------------------------------------------------------*/
