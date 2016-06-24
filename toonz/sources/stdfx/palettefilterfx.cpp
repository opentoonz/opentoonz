

#include "stdfx.h"
#include "tfxparam.h"
#include "ttzpimagefx.h"

//===================================================================

class PaletteFilterFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(PaletteFilterFx)

  TRasterFxPort m_input;
  TStringParamP m_string;
  TIntEnumParamP m_keep;
  TIntEnumParamP m_type;

public:
  PaletteFilterFx()
      : m_string(L"1,2,3")
      , m_type(new TIntEnumParam(0, "Lines & Areas"))
      , m_keep(new TIntEnumParam(0, "Delete"))

  {
    addInputPort("Source", m_input);
    bindParam(this, "indexes", m_string);
    bindParam(this, "keep", m_keep);
    bindParam(this, "type", m_type);
    m_type->addItem(1, "Lines");
    m_type->addItem(2, "Areas");
    m_type->addItem(3, "Lines & Areas (No Gap)");
    m_type->addItem(4, "Lines (Delete All Areas)");
    m_type->addItem(5, "Areas (Delete All Lines)");

    m_keep->addItem(1, "Keep");
  }

  ~PaletteFilterFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool allowUserCacheOnPort(int port) override { return false; }
};

//-------------------------------------------------------------------

void PaletteFilterFx::doDryCompute(TRectD &rect, double frame,
                                   const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  std::vector<std::string> items;
  std::string indexes = ::to_string(m_string->getValue());
  parseIndexes(indexes, items);
  TRenderSettings ri2(info);
  PaletteFilterFxRenderData *PaletteFilterData = new PaletteFilterFxRenderData;
  insertIndexes(items, PaletteFilterData);
  PaletteFilterData->m_keep = (bool)(m_keep->getValue() == 1);
  switch (m_type->getValue()) {
  case 0:
    PaletteFilterData->m_type = eApplyToInksAndPaints;
    break;
  case 1:
    PaletteFilterData->m_type = eApplyToInksKeepingAllPaints;
    break;
  case 2:
    PaletteFilterData->m_type = eApplyToPaintsKeepingAllInks;
    break;
  case 3:
    PaletteFilterData->m_type = eApplyToInksAndPaints_NoGap;
    break;
  case 4:
    PaletteFilterData->m_type = eApplyToInksDeletingAllPaints;
    break;
  case 5:
    PaletteFilterData->m_type = eApplyToPaintsDeletingAllInks;
    break;
  default:
    assert(false);
  }

  ri2.m_data.push_back(PaletteFilterData);
  ri2.m_userCachable = false;
  m_input->dryCompute(rect, frame, ri2);
}

//-------------------------------------------------------------------

void PaletteFilterFx::doCompute(TTile &tile, double frame,
                                const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  std::vector<std::string> items;
  std::string indexes = ::to_string(m_string->getValue());
  parseIndexes(indexes, items);
  TRenderSettings ri2(ri);
  PaletteFilterFxRenderData *PaletteFilterData = new PaletteFilterFxRenderData;
  insertIndexes(items, PaletteFilterData);
  PaletteFilterData->m_keep = (bool)(m_keep->getValue() == 1);
  switch (m_type->getValue()) {
  case 0:
    PaletteFilterData->m_type = eApplyToInksAndPaints;
    break;
  case 1:
    PaletteFilterData->m_type = eApplyToInksKeepingAllPaints;
    break;
  case 2:
    PaletteFilterData->m_type = eApplyToPaintsKeepingAllInks;
    break;
  case 3:
    PaletteFilterData->m_type = eApplyToInksAndPaints_NoGap;
    break;
  case 4:
    PaletteFilterData->m_type = eApplyToInksDeletingAllPaints;
    break;
  case 5:
    PaletteFilterData->m_type = eApplyToPaintsDeletingAllInks;
    break;
  default:
    assert(false);
  }

  ri2.m_data.push_back(PaletteFilterData);
  ri2.m_userCachable = false;
  m_input->compute(tile, frame, ri2);
}

FX_PLUGIN_IDENTIFIER(PaletteFilterFx, "paletteFilterFx");
