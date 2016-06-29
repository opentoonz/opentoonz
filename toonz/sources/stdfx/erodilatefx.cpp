

// TnzCore includes
#include "trop.h"

// TnzBase includes
#include "tfxparam.h"
#include "tparamset.h"

// TnzStdfx includes
#include "stdfx.h"

//****************************************************************************
//    ErodeDilate Fx
//****************************************************************************

class ErodeDilateFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ErodeDilateFx)

  TRasterFxPort m_input;

  TIntEnumParamP m_type;
  TDoubleParamP m_radius;

public:
  ErodeDilateFx() : m_type(new TIntEnumParam(0, "Square")), m_radius(0.0) {
    addInputPort("Source", m_input);

    bindParam(this, "type", m_type);
    m_type->addItem(1, "Circular");

    m_radius->setMeasureName("fxLength");
    bindParam(this, "radius", m_radius);
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return isAlmostIsotropic(info.m_affine);
  }
};

//-------------------------------------------------------------------

bool ErodeDilateFx::doGetBBox(double frame, TRectD &bBox,
                              const TRenderSettings &info) {
  // Remember: info.m_affine MUST NOT BE CONSIDERED in doGetBBox's
  // implementation

  // Retrieve the input bbox without applied affines.

  if (!m_input.getFx()) return false;

  bool ret = m_input->doGetBBox(frame, bBox, info);

  bBox = bBox.enlarge(tceil(m_radius->getValue(frame)));
  return ret;
}

//-------------------------------------------------------------------

void ErodeDilateFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  double radius = m_radius->getValue(frame) * sqrt(info.m_affine.det());
  bool dilate   = (radius >= 0.0);

  TRop::ErodilateMaskType type = TRop::ErodilateMaskType(m_type->getValue());

  if (dilate) {
    // Quite easy, this time - just compute the input tile by forwarding the
    // request
    m_input->compute(tile, frame, info);

    // Then, apply TRop::erodilate directly with the scale-adjusted radius
    const TRasterP &ras = tile.getRaster();

    TRop::erodilate(ras, ras, radius, type);
  } else {
    // In the erosion case, we need to know the input enlarged by the radius
    // value
    int radI = tceil(fabs(radius));

    const TDimension &tileSize = tile.getRaster()->getSize();
    const TRectD &enlargedRect =
        TRectD(tile.m_pos, TDimensionD(tileSize.lx, tileSize.ly)).enlarge(radI);

    TDimension enlargedSize(tround(enlargedRect.getLx()),
                            tround(enlargedRect.getLy()));

    TTile inTile;
    m_input->allocateAndCompute(inTile, enlargedRect.getP00(), enlargedSize,
                                tile.getRaster(), frame, info);

    const TRasterP &inRas = inTile.getRaster();
    TRop::erodilate(inRas, inRas, radius, type);

    tile.getRaster()->copy(inRas, -TPoint(radI, radI));
  }
}

//-------------------------------------------------------------------

void ErodeDilateFx::doDryCompute(TRectD &rect, double frame,
                                 const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  double radius = m_radius->getValue(frame) * sqrt(info.m_affine.det());
  bool dilate   = (radius >= 0.0);

  if (dilate)
    m_input->dryCompute(rect, frame, info);
  else {
    int radI = tceil(fabs(radius));

    TRectD enlargedRect(rect.enlarge(radI));
    m_input->dryCompute(enlargedRect, frame, info);
  }
}

//------------------------------------------------------------------

int ErodeDilateFx::getMemoryRequirement(const TRectD &rect, double frame,
                                        const TRenderSettings &info) {
  return (m_type->getValue() == 0) ? TRasterFx::memorySize(rect, 8)
                                   :              // One additional greymaps
             2 * TRasterFx::memorySize(rect, 8);  // Two additional greymaps
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(ErodeDilateFx, "erodeDilateFx");
