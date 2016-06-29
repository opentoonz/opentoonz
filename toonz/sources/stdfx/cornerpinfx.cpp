

#include "tfxparam.h"
#include "tparamset.h"
#include "stdfx.h"
#include "time.h"
#include "trop.h"
#include "trasterfx.h"
#include "ttzpimagefx.h"
#include "toonz/tdistort.h"
#include "texturefxP.h"

//==============================================================================

namespace {
inline bool myIsEmpty(const TRectD &r) { return r.x0 >= r.x1 || r.y0 >= r.y1; }
}

//==============================================================================

class CornerPinFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CornerPinFx)

  enum { PERSPECTIVE, BILINEAR };

public:
  CornerPinFx();
  ~CornerPinFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  void onPortConnected(TFxPort *port);

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool allowUserCacheOnPort(int port) override { return port != 0; }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  void safeTransform(double frame, int port, const TRectD &rectOnOutput,
                     const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                     TRenderSettings &infoOnInput, TRectD &inBBox);

private:
  TRasterFxPort m_input;
  TRasterFxPort m_texture;

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

  TStringParamP m_string;
  TIntEnumParamP m_mode;
  TIntEnumParamP m_keep;
  TDoubleParamP m_value;

  TAffine getPort1Affine(double frame) const;
};

//==============================================================================

CornerPinFx::CornerPinFx()
    : m_distortType(new TIntEnumParam(PERSPECTIVE, "Perspective"))
    , m_deactivate(false)
    , m_string(L"1,2,3")
    , m_mode(new TIntEnumParam(SUBSTITUTE, "Texture"))
    , m_keep(new TIntEnumParam(0, "Delete"))
    , m_value(100) {
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

  bindParam(this, "deactivate", m_deactivate);

  bindParam(this, "indexes", m_string);
  bindParam(this, "mode", m_mode);
  bindParam(this, "keep", m_keep);
  bindParam(this, "value", m_value);

  addInputPort("source", m_input);
  addInputPort("Texture", m_texture);

  m_value->setValueRange(0, 100);
  m_keep->addItem(1, "Keep");

  m_mode->addItem(PATTERNTYPE, "Pattern");
  m_mode->addItem(ADD, "Add");
  m_mode->addItem(SUBTRACT, "Subtract");
  m_mode->addItem(MULTIPLY, "Multiply");
  m_mode->addItem(LIGHTEN, "Lighten");
  m_mode->addItem(DARKEN, "Darken");

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

// ------------------------------------------------------------------------

CornerPinFx::~CornerPinFx() {}

// ------------------------------------------------------------------------

bool CornerPinFx::doGetBBox(double frame, TRectD &bBox,
                            const TRenderSettings &info) {
  if (m_input.isConnected()) {
    bool ret = m_input->doGetBBox(frame, bBox, info);
    return ret;
  } else {
    bBox = TRectD();
    return false;
  }
}

// ------------------------------------------------------------------------

TAffine CornerPinFx::getPort1Affine(double frame) const {
  // Explanation: Fxs tree decomposition on n-ary fxs (n>1) works the following
  // way.
  // Fxs on port 0 are considered 'column representants' - so affines found on
  // them
  // must be passed throughout the tree hierarchy. Therefore, their inverses are
  // multiplied
  // just below the other input ports in order to push the former affine above.
  // In other words:
  //  port  0:  A -|                  ---|
  //               +--   =>              + A --
  //  port >0:  B -|         B - (Ainv) -|

  // Retrieve the affine that lies just below texture's port.
  TGeometryFx *AinvFx = dynamic_cast<TGeometryFx *>(m_texture.getFx());
  if (!AinvFx) return TAffine();
  TGeometryFx *BFx =
      dynamic_cast<TGeometryFx *>(AinvFx->getInputPort(0)->getFx());
  if (!BFx) return AinvFx->getPlacement(frame);
  return AinvFx->getPlacement(frame) * BFx->getPlacement(frame);
}

// ------------------------------------------------------------------------

void CornerPinFx::transform(double frame, int port, const TRectD &rectOnOutput,
                            const TRenderSettings &infoOnOutput,
                            TRectD &rectOnInput, TRenderSettings &infoOnInput) {
  // Build the input affine
  infoOnInput = infoOnOutput;

  TPointD p00_b = m_p00_b->getValue(frame);
  TPointD p10_b = m_p10_b->getValue(frame);
  TPointD p01_b = m_p01_b->getValue(frame);
  TPointD p11_b = m_p11_b->getValue(frame);

  TPointD p00_a = m_p00_a->getValue(frame);
  TPointD p10_a = m_p10_a->getValue(frame);
  TPointD p01_a = m_p01_a->getValue(frame);
  TPointD p11_a = m_p11_a->getValue(frame);

  // NOTE 1: An appropriate scale factor is sent below in the schematic,
  // depending
  // on the image distortion. For example, if the distortion brings a 2 scale
  // factor,
  // we want the same factor applied on image levels, so that their detail is
  // appropriate
  //(especially for vector images).

  double scale = 0;
  scale        = std::max(scale, norm(p10_a - p00_a) / norm(p10_b - p00_b));
  scale        = std::max(scale, norm(p01_a - p00_a) / norm(p01_b - p00_b));
  scale        = std::max(scale, norm(p11_a - p10_a) / norm(p11_b - p10_b));
  scale        = std::max(scale, norm(p11_a - p01_a) / norm(p11_b - p01_b));

  TAffine A_1B(getPort1Affine(frame));
  TAffine B(infoOnOutput.m_affine * A_1B);

  scale *= sqrt(fabs(B.det()));

  if (infoOnOutput.m_isSwatch) {
    // Swatch viewers work out a lite version of the distort: only contractions
    // are passed below in the schematic - so that input memory load is always
    // at minimum.
    // This is allowed in order to make fx adjusts the less frustrating as
    // possible.
    if (scale > 1) scale = 1;
  }

  // NOTE 2: We further include A_1B.inv() to be applied on the level. This is
  // done to
  // counter the presence of A_1B just below this node - since it is preferable
  // to place A_1B
  // directly in the distort.

  infoOnInput.m_affine = TScale(scale) * A_1B.inv();
  // rectOnInput = (TScale(scale) * B.inv()) * rectOnOutput;

  // NOTE 3: The original distortion points are intended on the source port
  // *before* applying A.
  // But we want to have them on texture port *before* applying B; so, the
  // following ref change arises:
  //  p_b = (TScale(scale) * A_1B.inv()) * p_b == infoOnInput.m_aff * p_b;
  //  p_a = (B * A_1B.inv()) * p_a == infoOnOutput.m_aff * p_a;
  // Observe that scale and B are here due to NOTES 1 and 2.

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

void CornerPinFx::safeTransform(double frame, int port,
                                const TRectD &rectOnOutput,
                                const TRenderSettings &infoOnOutput,
                                TRectD &rectOnInput,
                                TRenderSettings &infoOnInput, TRectD &inBBox) {
  assert(port == 1 && m_texture.isConnected());

  if (m_deactivate->getValue()) {
    infoOnInput = infoOnOutput;
    rectOnInput = rectOnOutput;
    m_texture->getBBox(frame, inBBox, infoOnInput);
    return;
  }

  // Avoid the singular matrix cases
  if (fabs(infoOnOutput.m_affine.det()) < 1.e-3) {
    infoOnInput = infoOnOutput;
    rectOnInput.empty();
    m_texture->getBBox(frame, inBBox, infoOnInput);
    return;
  }

  transform(frame, port, rectOnOutput, infoOnOutput, rectOnInput, infoOnInput);

  m_texture->getBBox(frame, inBBox, infoOnInput);

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

void CornerPinFx::doDryCompute(TRectD &rect, double frame,
                               const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  std::vector<std::string> items;
  std::string indexes = ::to_string(m_string->getValue());
  parseIndexes(indexes, items);
  TRenderSettings ri2(ri);
  PaletteFilterFxRenderData *PaletteFilterData = new PaletteFilterFxRenderData;
  TRasterFxRenderDataP smartP(PaletteFilterData);
  insertIndexes(items, PaletteFilterData);
  PaletteFilterData->m_keep = (m_keep->getValue() == 1);
  ri2.m_data.push_back(PaletteFilterData);
  ri2.m_userCachable = false;

  // First child compute: part of output that IS NOT texturized
  m_input->dryCompute(rect, frame, ri2);

  if (!m_texture.isConnected()) return;

  bool isSwatch                = ri2.m_isSwatch;
  if (isSwatch) ri2.m_isSwatch = false;
  PaletteFilterData->m_keep    = !(m_keep->getValue() == 1);

  // Second child compute: part of output that IS to be texturized
  m_input->dryCompute(rect, frame, ri2);

  // Deal with the texture
  if (m_deactivate->getValue())
    m_texture->dryCompute(rect, frame, ri);
  else {
    TRectD inRect;
    TRenderSettings riNew;
    TRectD inBBox;

    safeTransform(frame, 1, rect, ri, inRect, riNew, inBBox);

    inRect *= inBBox;

    if (myIsEmpty(inRect)) return;

    m_texture->dryCompute(inRect, frame, riNew);
  }
}

// ------------------------------------------------------------------------

void CornerPinFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  TTile invertMaskTile;

  // carico il vettore items con gli indici dei colori
  std::vector<std::string> items;
  std::string indexes = ::to_string(m_string->getValue());
  parseIndexes(indexes, items);

  // genero il tile il cui raster contiene l'immagine in input a cui sono stati
  // tolti i pixel
  // colorati con gli indici contenuti nel vettore items
  TRenderSettings ri2(ri);
  PaletteFilterFxRenderData *PaletteFilterData = new PaletteFilterFxRenderData;
  TRasterFxRenderDataP smartP(PaletteFilterData);
  insertIndexes(items, PaletteFilterData);
  PaletteFilterData->m_keep = (m_keep->getValue() == 1);
  ri2.m_data.push_back(PaletteFilterData);
  ri2.m_userCachable = false;
  m_input->allocateAndCompute(invertMaskTile, tile.m_pos,
                              tile.getRaster()->getSize(), tile.getRaster(),
                              frame, ri2);

  if (!m_texture.isConnected()) {
    tile.getRaster()->copy(invertMaskTile.getRaster());
    return;
  }

  // genero il tile il cui raster contiene l'immagine in input a cui sono stati
  // tolti i pixel
  // colorati con indici diversi da quelli contenuti nel vettore items
  bool isSwatch                = ri2.m_isSwatch;
  if (isSwatch) ri2.m_isSwatch = false;
  PaletteFilterData->m_keep    = !(m_keep->getValue() == 1);
  m_input->compute(tile, frame, ri2);
  if (isSwatch) ri2.m_isSwatch = true;

  // controllo se ho ottenuto quaclosa su cui si possa lavorare.
  TRect saveBox;
  TRop::computeBBox(tile.getRaster(), saveBox);
  if (saveBox.isEmpty()) {
    // I guess that this case should be wiped out...
    m_input->compute(tile, frame, ri);  // Could the invertMask be copied??
    return;
  }

  // Then, generate the texture tile and make its distortion
  TTile textureTile;
  if (m_deactivate->getValue()) {
    TDimension size = tile.getRaster()->getSize();
    TPointD pos     = tile.m_pos;
    m_texture->allocateAndCompute(textureTile, pos, size, tile.getRaster(),
                                  frame, ri);
  } else {
    // Build the interesting texture region
    TRasterP tileRas(tile.getRaster());
    TRectD tileRect(convert(tileRas->getBounds()) + tile.m_pos);

    TRectD inRect;
    TRenderSettings riNew;
    TRectD inBBox;

    safeTransform(frame, 1, tileRect, ri, inRect, riNew, inBBox);

    inRect *= inBBox;

    if (myIsEmpty(inRect)) return;

    TDimension inRectSize(tceil(inRect.getLx()), tceil(inRect.getLy()));

    TTile inTile;
    m_texture->allocateAndCompute(inTile, inRect.getP00(), inRectSize, tileRas,
                                  frame, riNew);

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

    double scale = 0;
    scale        = std::max(scale, norm(p10_a - p00_a) / norm(p10_b - p00_b));
    scale        = std::max(scale, norm(p01_a - p00_a) / norm(p01_b - p00_b));
    scale        = std::max(scale, norm(p11_a - p10_a) / norm(p11_b - p10_b));
    scale        = std::max(scale, norm(p11_a - p01_a) / norm(p11_b - p01_b));

    TAffine A_1B(getPort1Affine(frame));
    TAffine B(ri.m_affine * A_1B);
    scale *= sqrt(fabs(B.det()));

    if (ri.m_isSwatch)
      if (scale > 1) scale = 1;

    p00_b = riNew.m_affine * p00_b;
    p10_b = riNew.m_affine * p10_b;
    p01_b = riNew.m_affine * p01_b;
    p11_b = riNew.m_affine * p11_b;

    p00_a = ri.m_affine * p00_a;
    p10_a = ri.m_affine * p10_a;
    p01_a = ri.m_affine * p01_a;
    p11_a = ri.m_affine * p11_a;

    PerspectiveDistorter perpDistorter(
        p00_b - inTile.m_pos, p10_b - inTile.m_pos, p01_b - inTile.m_pos,
        p11_b - inTile.m_pos, p00_a, p10_a, p01_a, p11_a);

    BilinearDistorter bilDistorter(p00_b - inTile.m_pos, p10_b - inTile.m_pos,
                                   p01_b - inTile.m_pos, p11_b - inTile.m_pos,
                                   p00_a, p10_a, p01_a, p11_a);

    TDistorter *distorter;
    if (m_distortType->getValue() == PERSPECTIVE)
      distorter = &perpDistorter;
    else if (m_distortType->getValue() == BILINEAR)
      distorter = &bilDistorter;
    else
      assert(0);

    TRasterP textureRas(tileRas->create(tileRas->getLx(), tileRas->getLy()));
    distort(textureRas, inTile.getRaster(), *distorter, convert(tile.m_pos),
            TRop::Bilinear);
    textureTile.m_pos = tile.m_pos;
    textureTile.setRaster(textureRas);
  }

  // Then, apply the texture
  double v = m_value->getValue(frame);
  if (ri.m_bpp == 32) {
    TRaster32P witheCard;

    myOver32(tile.getRaster(), invertMaskTile.getRaster(),
             &makeOpaque<TPixel32>, v);

    switch (m_mode->getValue()) {
    case SUBSTITUTE:
      myOver32(tile.getRaster(), textureTile.getRaster(), &substitute<TPixel32>,
               v);
      break;
    case PATTERNTYPE:
      witheCard = TRaster32P(textureTile.getRaster()->getSize());
      witheCard->fill(TPixel32::White);
      TRop::over(textureTile.getRaster(), witheCard, textureTile.getRaster());
      myOver32(tile.getRaster(), textureTile.getRaster(), &pattern32, v);
      break;
    case ADD:
      myOver32(tile.getRaster(), textureTile.getRaster(), &textureAdd<TPixel32>,
               v / 100.0);
      break;
    case SUBTRACT:
      myOver32(tile.getRaster(), textureTile.getRaster(), &textureSub<TPixel32>,
               v / 100.0);
      break;
    case MULTIPLY:
      myOver32(tile.getRaster(), textureTile.getRaster(),
               &textureMult<TPixel32>, v);
      break;
    case DARKEN:
      myOver32(tile.getRaster(), textureTile.getRaster(),
               &textureDarken<TPixel32>, v);
      break;
    case LIGHTEN:
      myOver32(tile.getRaster(), textureTile.getRaster(),
               &textureLighten<TPixel32>, v);
      break;
    default:
      assert(0);
      break;
    }
  } else {
    TRaster64P witheCard;

    myOver64(tile.getRaster(), invertMaskTile.getRaster(),
             &makeOpaque<TPixel64>, v);

    switch (m_mode->getValue()) {
    case SUBSTITUTE:
      myOver64(tile.getRaster(), textureTile.getRaster(), &substitute<TPixel64>,
               v);
      break;
    case PATTERNTYPE:
      witheCard = TRaster64P(textureTile.getRaster()->getSize());
      witheCard->fill(TPixel64::White);
      TRop::over(textureTile.getRaster(), witheCard, textureTile.getRaster());
      myOver64(tile.getRaster(), textureTile.getRaster(), &pattern64, v);
      break;
    case ADD:
      myOver64(tile.getRaster(), textureTile.getRaster(), &textureAdd<TPixel64>,
               v / 100.0);
      break;
    case SUBTRACT:
      myOver64(tile.getRaster(), textureTile.getRaster(), &textureSub<TPixel64>,
               v / 100.0);
      break;
    case MULTIPLY:
      myOver64(tile.getRaster(), textureTile.getRaster(),
               &textureMult<TPixel64>, v);
      break;
    case DARKEN:
      myOver64(tile.getRaster(), textureTile.getRaster(),
               &textureDarken<TPixel64>, v);
      break;
    case LIGHTEN:
      myOver64(tile.getRaster(), textureTile.getRaster(),
               &textureLighten<TPixel64>, v);
      break;
    default:
      assert(0);
      break;
    }
  }

  TRop::over(tile.getRaster(), invertMaskTile.getRaster());
}

//-------------------------------------------------------------------------

int CornerPinFx::getMemoryRequirement(const TRectD &rect, double frame,
                                      const TRenderSettings &info) {
  if (!m_texture.isConnected()) return TRasterFx::memorySize(rect, info.m_bpp);

  TRectD inRect;
  TRenderSettings riNew;
  TRectD inBBox;
  safeTransform(frame, 1, rect, info, inRect, riNew, inBBox);
  inRect *= inBBox;

  return std::max(TRasterFx::memorySize(rect, info.m_bpp),
                  TRasterFx::memorySize(inRect, riNew.m_bpp));
}

//-------------------------------------------------------------------------
/*
TRectD CornerPinFx::getContainingBox(const FourPoints &points)
{
        if(points.m_p00==points.m_p01 && points.m_p01==points.m_p10 &&
points.m_p10==points.m_p11)
                return TRectD(points.m_p00,points.m_p00);
        double
xMax=std::max(points.m_p00.x,points.m_p01.x,points.m_p10.x,points.m_p11.x);
        double
yMax=std::max(points.m_p00.y,points.m_p01.y,points.m_p10.y,points.m_p11.y);
        double
xMin=std::min(points.m_p00.x,points.m_p01.x,points.m_p10.x,points.m_p11.x);
        double
yMin=std::min(points.m_p00.y,points.m_p01.y,points.m_p10.y,points.m_p11.y);
        return TRectD(xMin,yMin,xMax, yMax);
}
*/
// ------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(CornerPinFx, "cornerPinFx");
