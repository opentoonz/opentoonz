

// TnzCore includes
#include "tstream.h"
#include "trop.h"

// TnzBase includes
#include "tdoubleparam.h"
#include "tnotanimatableparam.h"
#include "tfxparam.h"
#include "trasterfx.h"
#include "tbasefx.h"

//******************************************************************************************
//    Local namespace
//******************************************************************************************

namespace {
void makeRectCoherent(TRectD &rect, const TPointD &pos) {
  rect -= pos;
  rect.x0 = tfloor(rect.x0);
  rect.y0 = tfloor(rect.y0);
  rect.x1 = tceil(rect.x1);
  rect.y1 = tceil(rect.y1);
  rect += pos;
}
}

//******************************************************************************************
//    TImageCombinationFx  declaration
//******************************************************************************************

class TImageCombinationFx : public TBaseRasterFx {
  TFxPortDG m_group;

public:
  TImageCombinationFx();
  virtual ~TImageCombinationFx() {}

public:
  // Virtual interface for heirs

  //! The raster processing function that must be reimplemented to perform the
  //! fx
  virtual void process(const TRasterP &up, const TRasterP &down,
                       double frame) = 0;

  //! Whether the 'up' rasters of process() invocations must be allocated to
  //! entirely cover the 'down' counterpart. Should be enabled if the process()
  //! function affects 'down' pixels when the 'up's are fully transparent.
  virtual bool requiresFullRect() { return false; }

public:
  // Low-level TRasterFx-related functions

  int dynamicPortGroupsCount() const override { return 1; }
  const TFxPortDG *dynamicPortGroup(int g) const override {
    return (g == 0) ? &m_group : 0;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    // At max the memory of another tile with the same infos may be allocated
    // apart from
    // the externally supplied one.
    return TRasterFx::memorySize(rect, info.m_bpp);
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override;

  void compatibilityTranslatePort(int majorVersion, int minorVersion,
                                  std::string &portName) override;

  int getPreferredInputPort() override { return 1; }
};

//******************************************************************************************
//    TImageCombinationFx  implementation
//******************************************************************************************

TImageCombinationFx::TImageCombinationFx() : m_group("Source", 2) {
  addInputPort("Source1", new TRasterFxPort, 0);
  addInputPort("Source2", new TRasterFxPort, 0);
  setName(L"ImageCombinationFx");
}

//---------------------------------------------------------------------------

bool TImageCombinationFx::doGetBBox(double frame, TRectD &bBox,
                                    const TRenderSettings &info) {
  bBox = TRectD();

  int p, pCount = getInputPortCount();
  for (p = 0; p != pCount; ++p) {
    TRasterFxPort *port = static_cast<TRasterFxPort *>(getInputPort(p));
    TRectD inputBBox;

    bool hasInput = (port && port->isConnected())
                        ? (*port)->doGetBBox(frame, inputBBox, info)
                        : false;

    if (hasInput) bBox += inputBBox;
  }

  return (bBox.getLx() >= 0) && (bBox.getLy() >= 0);
}

//---------------------------------------------------------------------------

void TImageCombinationFx::doCompute(TTile &tile, double frame,
                                    const TRenderSettings &info) {
  int p, pCount = getInputPortCount();
  TRasterFxPort *port = 0;

  // Skip empty ports
  for (p = pCount - 1; p >= 0;
       --p)  // Reverse iteration - bottom ports have high indices
  {
    port = static_cast<TRasterFxPort *>(getInputPort(p));
    if (port && port->getFx()) break;
  }

  // If there is no input, clear and return
  if (p < 0) {
    tile.getRaster()
        ->clear();  // Probably not necessary unless externally invocation
    return;  // deliberately soiled tile - however, should be rare anyway.
  }

  // Calculate the tiles' geometries
  const TRect &tileRect(tile.getRaster()->getBounds());
  const TDimension &tileSize(tileRect.getSize());
  TRectD tileRectD(tile.m_pos, TDimensionD(tileSize.lx, tileSize.ly));

  // Render the first viable port directly on tile
  (*port)->compute(tile, frame,
                   info);  // Should we do it only if the bbox is not empty?

  // Then, render each subsequent port and process() it on top of tile
  bool canRestrict = !requiresFullRect();

  for (--p; p >= 0; --p) {
    port = static_cast<TRasterFxPort *>(getInputPort(p));
    if (!(port && port->getFx()))  // Skip empty ports
      continue;

    // Will allocate a new tile to calculate the input contribution - so, if
    // possible
    // we'll restrict allocation to the input port's bbox
    TRectD computeRect(tileRectD);

    if (canRestrict) {
      TRectD inBBox;
      (*port)->getBBox(frame, inBBox, info);

      computeRect *= inBBox;

      makeRectCoherent(
          computeRect,
          tile.m_pos);  // Make it coherent with tile's pixel geometry
    }

    // Calculate the input port and perform processing
    TDimension computeSize(tround(computeRect.getLx()),
                           tround(computeRect.getLy()));
    if ((computeSize.lx > 0) && (computeSize.ly > 0)) {
      TTile inTile;  // Observe its locality - not incidental
      (*port)->allocateAndCompute(inTile, computeRect.getP00(), computeSize,
                                  tile.getRaster(), frame, info);

      // Invoke process() to deal with the actual fx processing
      TRasterP up(inTile.getRaster()), down(tile.getRaster());

      if (canRestrict) {
        // Extract from tile the part corresponding to inTile
        TRect downRect(convert(computeRect.getP00() - tile.m_pos), computeSize);
        down = down->extract(downRect);
      }

      assert(up->getSize() == down->getSize());
      process(up, down, frame);  // This is the point with the max concentration
                                 // of allocated resources
    }
  }
}

//-------------------------------------------------------------------------------------

void TImageCombinationFx::doDryCompute(TRectD &rect, double frame,
                                       const TRenderSettings &info) {
  // Mere copy of doCompute(), stripped of the actual computations

  int p, pCount = getInputPortCount();
  TRasterFxPort *port = 0;

  for (p = pCount - 1; p >= 0; --p) {
    port = static_cast<TRasterFxPort *>(getInputPort(p));
    if (port && port->getFx()) break;
  }

  if (p < 0) return;

  (*port)->dryCompute(rect, frame, info);

  bool canRestrict = !requiresFullRect();

  for (--p; p >= 0; --p) {
    port = static_cast<TRasterFxPort *>(getInputPort(p));
    if (!(port && port->getFx())) continue;

    TRectD computeRect(rect);

    if (canRestrict) {
      TRectD inBBox;
      (*port)->getBBox(frame, inBBox, info);

      computeRect *= inBBox;
      makeRectCoherent(computeRect, rect.getP00());
    }

    TDimension computeSize(tround(computeRect.getLx()),
                           tround(computeRect.getLy()));
    if ((computeSize.lx > 0) && (computeSize.ly > 0))
      (*port)->dryCompute(computeRect, frame, info);
  }
}

//-------------------------------------------------------------------------------------

void TImageCombinationFx::compatibilityTranslatePort(int major, int minor,
                                                     std::string &portName) {
  if (VersionNumber(major, minor) < VersionNumber(1, 20)) {
    if (portName == "Up")
      portName = "Source1";
    else if (portName == "Down")
      portName = "Source2";
  }
}

//******************************************************************************************
//    TImageCombinationFx  heir classes
//******************************************************************************************

class OverFx final : public TImageCombinationFx {
  FX_DECLARATION(OverFx)

public:
  OverFx() { setName(L"OverFx"); }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::over(down, up);
  }
};

//==================================================================

class AddFx final : public TImageCombinationFx {
  FX_DECLARATION(AddFx)

  TDoubleParamP m_value;

public:
  AddFx() : m_value(100.0) { bindParam(this, "value", m_value); }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    double value = m_value->getValue(frame) / 100.0;

    if (value != 1.0)
      TRop::add(up, down, down, value);
    else
      TRop::add(up, down, down);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class ColorDodgeFx final : public TImageCombinationFx {
  FX_DECLARATION(AddFx)

public:
  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::colordodge(up, down, down);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class ColorBurnFx final : public TImageCombinationFx {
  FX_DECLARATION(AddFx)

public:
  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::colorburn(up, down, down);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class ScreenFx final : public TImageCombinationFx {
  FX_DECLARATION(AddFx)

public:
  bool requiresFullRect() override { return true; }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::screen(up, down, down);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class SubFx final : public TImageCombinationFx {
  FX_DECLARATION(SubFx)

  TBoolParamP m_matte;

public:
  SubFx() : m_matte(false) { bindParam(this, "matte", m_matte); }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::sub(up, down, down, m_matte->getValue());
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class MultFx final : public TImageCombinationFx {
  FX_DECLARATION(MultFx)

  TDoubleParamP m_value;
  TBoolParamP m_matte;

public:
  MultFx() : m_value(0.0), m_matte(false) {
    bindParam(this, "value", m_value);
    bindParam(this, "matte", m_matte);
  }

  bool requiresFullRect() override { return m_matte->getValue(); }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::mult(up, down, down, m_value->getValue(frame), m_matte->getValue());
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class MinFx final : public TImageCombinationFx {
  FX_DECLARATION(MinFx)

  TBoolParamP m_matte;

public:
  MinFx() : m_matte(true) { bindParam(this, "matte", m_matte); }

  bool requiresFullRect() override { return true; }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::ropmin(up, down, down, m_matte->getValue());
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class MaxFx final : public TImageCombinationFx {
  FX_DECLARATION(MaxFx)

public:
  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::ropmax(up, down, down);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

class LinearBurnFx final : public TImageCombinationFx {
  FX_DECLARATION(LinearBurnFx)

public:
  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::linearburn(up, down, down);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

// This Fx is probably unused...!
class OverlayFx final : public TImageCombinationFx {
  FX_DECLARATION(OverlayFx)

public:
  OverlayFx() {}
  ~OverlayFx() {}

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    TRop::overlay(up, down, down);
  }
};

//==================================================================

class BlendFx final : public TImageCombinationFx {
  FX_DECLARATION(BlendFx)

  TDoubleParamP m_value;

public:
  BlendFx() : m_value(0.0) {
    bindParam(this, "value", m_value);
    m_value->setValueRange(0.0, 100.0);
  }

  bool requiresFullRect() override { return true; }

  void process(const TRasterP &up, const TRasterP &down,
               double frame) override {
    double value     = 0.01 * m_value->getValue(frame);
    UCHAR matteValue = (UCHAR)(value * 255.0 + 0.5);

    TRop::crossDissolve(up, down, down, matteValue);
  }

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//******************************************************************************************
//    Matte Fxs  definition
//******************************************************************************************

class InFx final : public TBaseRasterFx {
  FX_DECLARATION(InFx)

  TRasterFxPort m_source, m_matte;

public:
  InFx() {
    addInputPort("Source", m_source);
    addInputPort("Matte", m_matte);
    setName(L"InFx");
  }

  ~InFx() {}

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override {
    if (m_matte.isConnected() && m_source.isConnected()) {
      bool ret = m_matte->doGetBBox(frame, bbox, info);

      if (bbox == TConsts::infiniteRectD)
        return m_source->doGetBBox(frame, bbox, info);
      else
        return ret;
    }
    bbox = TRectD();
    return false;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    // This fx is not visible if either the source or the matte tiles are empty.
    // It's because only source is visible, and only where matte is opaque.
    if (!(m_source.isConnected() && m_matte.isConnected())) return;

    TTile srcTile;
    m_source->allocateAndCompute(srcTile, tile.m_pos,
                                 tile.getRaster()->getSize(), tile.getRaster(),
                                 frame, ri);

    m_matte->compute(tile, frame, ri);

    TRop::ropin(srcTile.getRaster(), tile.getRaster(), tile.getRaster());
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!(m_source.isConnected() && m_matte.isConnected())) return;

    m_source->dryCompute(rect, frame, info);
    m_matte->dryCompute(rect, frame, info);
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return TRasterFx::memorySize(rect, info.m_bpp);
  }
};

//==================================================================

class OutFx final : public TBaseRasterFx {
  FX_DECLARATION(OutFx)

  TRasterFxPort m_source, m_matte;

public:
  OutFx() {
    addInputPort("Source", m_source);
    addInputPort("Matte", m_matte);
    setName(L"OutFx");
  }

  ~OutFx() {}

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override {
    if (m_source.isConnected()) return m_source->doGetBBox(frame, bbox, info);

    return false;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    // If there is no source, do nothing
    if (!m_source.isConnected()) return;

    // Here, source is visible where matte is transparent. So if there is
    // no matte, just build source.
    if (!m_matte.isConnected()) {
      m_source->compute(tile, frame, ri);
      return;
    }

    TTile srcTile;
    m_source->allocateAndCompute(srcTile, tile.m_pos,
                                 tile.getRaster()->getSize(), tile.getRaster(),
                                 frame, ri);

    m_matte->compute(tile, frame, ri);

    TRop::ropout(srcTile.getRaster(), tile.getRaster(), tile.getRaster());
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_source.isConnected()) return;

    if (!m_matte.isConnected()) {
      m_source->dryCompute(rect, frame, info);
      return;
    }

    m_source->dryCompute(rect, frame, info);
    m_matte->dryCompute(rect, frame, info);
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return TRasterFx::memorySize(rect, info.m_bpp);
  }
};

//==================================================================

class AtopFx final : public TBaseRasterFx {
  FX_DECLARATION(AtopFx)

  TRasterFxPort m_up, m_dn;

public:
  AtopFx() {
    addInputPort("Up", m_up);
    addInputPort("Down", m_dn);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    bBox = TRectD();

    {
      TRectD inputBBox;

      bool hasInput =
          m_up.isConnected() ? m_up->doGetBBox(frame, inputBBox, info) : false;
      if (hasInput) bBox += inputBBox;
    }

    {
      TRectD inputBBox;

      bool hasInput =
          m_dn.isConnected() ? m_dn->doGetBBox(frame, inputBBox, info) : false;
      if (hasInput) bBox += inputBBox;
    }

    return (bBox.getLx() >= 0) && (bBox.getLy() >= 0);
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    // Here it's just like matte in, but the matte is visible under up.

    if (!m_dn.isConnected()) return;

    if (!m_up.isConnected()) {
      m_dn->compute(tile, frame, ri);
      return;
    }

    TTile upTile;
    m_up->allocateAndCompute(upTile, tile.m_pos, tile.getRaster()->getSize(),
                             tile.getRaster(), frame, ri);

    m_dn->compute(tile, frame, ri);

    TRop::atop(upTile.getRaster(), tile.getRaster(), tile.getRaster());
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_dn.isConnected()) return;

    if (!m_up.isConnected()) {
      m_dn->dryCompute(rect, frame, info);
      return;
    }

    m_up->dryCompute(rect, frame, info);
    m_dn->dryCompute(rect, frame, info);
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return TRasterFx::memorySize(rect, info.m_bpp);
  }
};

//==================================================================

//=======================
//    Fx identifiers
//-----------------------

FX_IDENTIFIER(OverFx, "overFx")
FX_IDENTIFIER(AddFx, "addFx")
FX_IDENTIFIER(SubFx, "subFx")
FX_IDENTIFIER(MultFx, "multFx")
FX_IDENTIFIER(InFx, "inFx")
FX_IDENTIFIER(OutFx, "outFx")
FX_IDENTIFIER(AtopFx, "atopFx")
// FX_IDENTIFIER(XorFx,       "xorFx")
FX_IDENTIFIER(MinFx, "minFx")
FX_IDENTIFIER(MaxFx, "maxFx")
FX_IDENTIFIER(LinearBurnFx, "linearBurnFx")
FX_IDENTIFIER(OverlayFx, "overlayFx")
FX_IDENTIFIER(BlendFx, "blendFx")
FX_IDENTIFIER(ColorDodgeFx, "colorDodgeFx")
FX_IDENTIFIER(ColorBurnFx, "colorBurnFx")
FX_IDENTIFIER(ScreenFx, "screenFx")
