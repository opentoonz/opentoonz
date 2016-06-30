

#include "stdfx.h"
#include "ttzpimagefx.h"

#include "toonz/tcolumnfx.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"

//===================================================================

class ExternalPaletteFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ExternalPaletteFx)

  TRasterFxPort m_input;
  TRasterFxPort m_expalette;

public:
  ExternalPaletteFx()

  {
    addInputPort("Source", m_input);
    addInputPort("Palette", m_expalette);
  }

  ~ExternalPaletteFx(){};

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

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool allowUserCacheOnPort(int port) override { return false; }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//-------------------------------------------------------------------

namespace {
TPalette *getPalette(TXshColumn *column, int frame) {
  TXshCellColumn *cellColumn = dynamic_cast<TXshCellColumn *>(column);
  if (!cellColumn) return 0;

  TXshCell cell = cellColumn->getCell(frame);
  if (cell.isEmpty()) return 0;

  TXshPaletteLevel *pl = cell.m_level->getPaletteLevel();
  if (pl) return pl->getPalette();

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (sl) return sl->getPalette();

  return 0;
}

TPalette *getPalette(TFx *fx, double frame) {
  if (!fx) return 0;
  int branches;
  branches = fx->getInputPortCount();

  if (branches == 1) return getPalette(fx->getInputPort(0)->getFx(), frame);

  if (branches > 1) return 0;

  if (TColumnFx *columnFx = dynamic_cast<TColumnFx *>(fx))
    return getPalette(columnFx->getXshColumn(), (int)frame);

  return 0;
}
}

//-------------------------------------------------------------------

std::string ExternalPaletteFx::getAlias(double frame,
                                        const TRenderSettings &info) const {
  std::string alias(TRasterFx::getAlias(frame, info));

  if (m_expalette.isConnected()) {
    TFx *fx = m_expalette.getFx();
    TPaletteP plt(getPalette(fx, frame));
    if (plt && plt->isAnimated()) alias += std::to_string(frame);
  }

  return alias;
}

//-------------------------------------------------------------------

void ExternalPaletteFx::doDryCompute(TRectD &rect, double frame,
                                     const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  if (m_expalette.isConnected()) {
    TFx *fx              = m_expalette.getFx();
    std::string pltAlias = m_expalette->getAlias(frame, ri);

    TPaletteP palette(getPalette(fx, frame));
    if (palette && palette->isAnimated()) pltAlias += std::to_string(frame);

    TRenderSettings ri2(ri);
    ExternalPaletteFxRenderData *data =
        new ExternalPaletteFxRenderData(palette, pltAlias);
    ri2.m_data.push_back(data);
    ri2.m_userCachable = false;

    m_input->dryCompute(rect, frame, ri2);
  } else
    m_input->dryCompute(rect, frame, ri);
}

//-------------------------------------------------------------------

void ExternalPaletteFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  if (m_expalette.isConnected()) {
    TFx *fx              = m_expalette.getFx();
    std::string pltAlias = m_expalette->getAlias(frame, ri);

    TPaletteP palette(getPalette(fx, frame));
    if (palette && palette->isAnimated()) pltAlias += std::to_string(frame);

    TRenderSettings ri2(ri);
    ExternalPaletteFxRenderData *data =
        new ExternalPaletteFxRenderData(palette, pltAlias);
    ri2.m_data.push_back(data);
    m_input->compute(tile, frame, ri2);
  } else
    m_input->compute(tile, frame, ri);
}

FX_PLUGIN_IDENTIFIER(ExternalPaletteFx, "externalPaletteFx");
