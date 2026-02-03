

// TnzCore includes
#include "trop.h"
#include "tpixelutils.h"

// TnzBase includes
#include "tparamset.h"
#include "tdoubleparam.h"
#include "tfxparam.h"
#include "tparamuiconcept.h"

#include "tbasefx.h"
#include "tzeraryfx.h"

//==================================================================

class ColorCardFx final : public TBaseZeraryFx {
  FX_DECLARATION(ColorCardFx)

  TPixelParamP m_color;

public:
  ColorCardFx() : m_color(TPixel32::Green) {
    bindParam(this, "color", m_color);
    m_color->setDefaultValue(TPixel32::Green);
    setName(L"ColorCardFx");
    enableComputeInFloat(true);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    TRaster32P ras32 = tile.getRaster();
    TRaster64P ras64 = tile.getRaster();
    TRasterFP rasF   = tile.getRaster();
    // currently the tile should always be nonlinear
    assert(!tile.getRaster()->isLinear());
    if (!tile.getRaster()->isLinear()) {
      if (ras32)
        ras32->fill(m_color->getPremultipliedValue(frame));
      else if (ras64)
        ras64->fill(toPixel64(m_color->getPremultipliedValue(frame)));
      else if (rasF)
        rasF->fill(toPixelF(m_color->getPremultipliedValue(frame)));
      else
        throw TException("ColorCardFx unsupported pixel type");
    } else {  // linear color space
      if (ras32)
        ras32->fill(toLinear(m_color->getPremultipliedValue(frame),
                             ri.m_colorSpaceGamma));
      else if (ras64)
        ras64->fill(toLinear(toPixel64(m_color->getPremultipliedValue(frame)),
                             ri.m_colorSpaceGamma));
      else if (rasF)
        rasF->fill(toLinear(toPixelF(m_color->getPremultipliedValue(frame)),
                            ri.m_colorSpaceGamma));
      else
        throw TException("ColorCardFx unsupported pixel type");
    }
  }
};

//==================================================================

class CheckBoardFx final : public TBaseZeraryFx {
  FX_DECLARATION(CheckBoardFx)

  TPixelParamP m_color1, m_color2;
  TDoubleParamP m_size;

public:
  CheckBoardFx()
      : m_color1(TPixel32::Black), m_color2(TPixel32::White), m_size(50) {
    m_size->setMeasureName("fxLength");
    bindParam(this, "color1", m_color1);
    bindParam(this, "color2", m_color2);
    bindParam(this, "size", m_size);
    m_color1->setDefaultValue(TPixel32::Black);
    m_color2->setDefaultValue(TPixel32::White);
    m_size->setValueRange(1, 1000);
    m_size->setDefaultValue(50);
    setName(L"CheckBoardFx");
    enableComputeInFloat(true);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override {
    bool isLinear = tile.getRaster()->isLinear();
    // currently the tile should always be nonlinear
    assert(!isLinear);
    const TPixel32 &c1 =
        m_color1->getValue(frame, isLinear, info.m_colorSpaceGamma);
    const TPixel32 &c2 =
        m_color2->getValue(frame, isLinear, info.m_colorSpaceGamma);

    double size = m_size->getValue(frame);

    assert(info.m_shrinkX == info.m_shrinkY);
    size *= info.m_affine.a11 / info.m_shrinkX;

    TDimensionD dim(size, size);
    TRop::checkBoard(tile.getRaster(), c1, c2, dim, tile.m_pos);
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::SIZE;
    concepts[0].m_label = "Size";
    concepts[0].m_params.push_back(m_size);
  }
};

//==================================================================

FX_IDENTIFIER(ColorCardFx, "colorCardFx")
FX_IDENTIFIER(CheckBoardFx, "checkBoardFx")
