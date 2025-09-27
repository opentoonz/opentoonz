/*-----------------------------------------------------------------------------
 Iwa_SmootherFx
 二値画像にスムージングをかける
 -------------------------------------------------------------------------------*/

#include "stdfx.h"
#include "tfxparam.h"

#include "timage_io.h"
#include "trop.h"

//=============================================================================

class Iwa_SmootherFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_SmootherFx)

  TRasterFxPort m_input;
  TIntParamP m_threshold;
  TDoubleParamP m_softness;

  TAffine m_levelColumnAff;

public:
  Iwa_SmootherFx() : m_threshold(10), m_softness(50) {
    bindParam(this, "threshold", m_threshold);
    bindParam(this, "softness", m_softness);
    addInputPort("Source", m_input);
    m_threshold->setValueRange(0, 256);
    m_softness->setValueRange(0., 100.);
  }

  ~Iwa_SmootherFx() {};

  bool doGetBBox(double frame, TRectD& bBox,
                 const TRenderSettings& info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  }

  void doCompute(TTile& tile, double frame, const TRenderSettings& ri) override;
  bool canHandle(const TRenderSettings& info, double frame) override {
    return false;
  }
  void doDryCompute(TRectD& rect, double frame,
                    const TRenderSettings& info) override;
};

//------------------------------------------------------------------------------

void Iwa_SmootherFx::doCompute(TTile& tile, double frame,
                               const TRenderSettings& ri) {
  if (!m_input.isConnected()) return;

  int threshold   = m_threshold->getValue();
  double softness = m_softness->getValue(frame);

  TTile tileIn;
  TRenderSettings new_sets(ri);

  new_sets.m_affine = m_levelColumnAff.inv();

  TAffine rev_aff     = ri.m_affine * new_sets.m_affine.inv();
  TAffine rev_aff_inv = rev_aff.inv();
  TRasterP tileRas(tile.getRaster());
  TRectD tileRect(convert(tileRas->getBounds()) + tile.m_pos);

  TPointD p00 = rev_aff_inv * tileRect.getP00();
  TPointD p01 = rev_aff_inv * tileRect.getP01();
  TPointD p10 = rev_aff_inv * tileRect.getP10();
  TPointD p11 = rev_aff_inv * tileRect.getP11();
  TRect in_rect;
  in_rect.x0 = tfloor(std::min({p00.x, p10.x, p01.x, p11.x}));
  in_rect.y0 = tfloor(std::min({p00.y, p10.y, p01.y, p11.y}));
  in_rect.x1 = tceil(std::max({p00.x, p10.x, p01.x, p11.x}));
  in_rect.y1 = tceil(std::max({p00.y, p10.y, p01.y, p11.y}));

  m_input->allocateAndCompute(
      tileIn, TPointD(in_rect.getP00().x, in_rect.getP00().y),
      in_rect.getSize(), tile.getRaster(), frame, new_sets);

  TRasterP inRas = tileIn.getRaster();
  TRasterP aaRas = inRas->clone();
  //! Inserts antialias around jaggy lines. Threshold is a pixel distance
  //! intended from 0 to 256. Softness may vary from 0 to 100.
  TRop::antialias(aaRas, inRas, threshold, softness);

  TRenderSettings rev_sets(ri);
  rev_sets.m_affine = rev_aff;
  TRasterFx::applyAffine(tile, tileIn, rev_sets);
}

//------------------------------------------------------------------------------

void Iwa_SmootherFx::doDryCompute(TRectD& rect, double frame,
                                  const TRenderSettings& info) {
  TRenderSettings ri(info);

  LevelColumnAffineFxRenderData* levelColumnData;
  bool found = false;
  for (auto data : ri.m_data) {
    levelColumnData =
        dynamic_cast<LevelColumnAffineFxRenderData*>(data.getPointer());
    if (levelColumnData) {
      levelColumnData->m_isSet = false;
      found                    = true;
      break;
    }
  }
  if (!found) {
    levelColumnData = new LevelColumnAffineFxRenderData();
    ri.m_data.push_back(levelColumnData);
  }

  TRasterFx::doDryCompute(rect, frame, ri);

  if (levelColumnData->m_isSet) m_levelColumnAff = levelColumnData->m_aff;
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_SmootherFx, "iwa_SmootherFx");