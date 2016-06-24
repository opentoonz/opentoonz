

#include "stdfx.h"
#include "tfxparam.h"
#include "ttzpimagefx.h"

#include "timage_io.h"

//=============================================================================

class TileFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(TileFx)

  enum tilingMode { eTile = 1, eTileHorizontally = 2, eTileVertically = 3 };

  TRasterFxPort m_input;
  TIntEnumParamP m_mode;
  TBoolParamP m_xMirror;
  TBoolParamP m_yMirror;
  TDoubleParamP m_margin;

public:
  TileFx();
  ~TileFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

private:
  void makeTile(const TTile &inputTile, const TTile &tile) const;
};

//------------------------------------------------------------------------------

TileFx::TileFx()
    : m_mode(new TIntEnumParam(eTile, "Tile"))
    , m_xMirror(false)
    , m_yMirror(false)
    , m_margin(-1.0) {
  m_margin->setMeasureName("fxLength");
  addInputPort("Source", m_input);
  bindParam(this, "mode", m_mode);
  bindParam(this, "xMirror", m_xMirror);
  bindParam(this, "yMirror", m_yMirror);
  bindParam(this, "margin", m_margin);
  m_mode->addItem(eTileHorizontally, "Tile Horizontally");
  m_mode->addItem(eTileVertically, "Tile Vertically");
}

//------------------------------------------------------------------------------

TileFx::~TileFx() {}

//------------------------------------------------------------------------------

bool TileFx::canHandle(const TRenderSettings &info, double frame) {
  // Currently, only affines which transform the X and Y axis into themselves
  // may
  // be handled by this fx...
  return (fabs(info.m_affine.a12) < 0.0001 &&
          fabs(info.m_affine.a21) < 0.0001) ||
         (fabs(info.m_affine.a11) < 0.0001 && fabs(info.m_affine.a22) < 0.0001);
}

//------------------------------------------------------------------------------

bool TileFx::doGetBBox(double frame, TRectD &bBox,
                       const TRenderSettings &info) {
  if (m_input.isConnected()) {
    bBox = TConsts::infiniteRectD;
    return true;
  } else {
    bBox = TRectD();
    return false;
  }
}

//------------------------------------------------------------------------------

void TileFx::transform(double frame, int port, const TRectD &rectOnOutput,
                       const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                       TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;

  TRectD inputBox;
  m_input->getBBox(frame, inputBox, infoOnOutput);

  double scale = sqrt(fabs(infoOnOutput.m_affine.det()));
  int margin   = m_margin->getValue(frame) * scale;
  inputBox     = inputBox.enlarge(margin);

  if (inputBox.isEmpty()) {
    rectOnInput.empty();
    return;
  }
  if (inputBox == TConsts::infiniteRectD) {
    infoOnInput = infoOnOutput;
    return;
  }

  TDimensionD size(0, 0);
  size.lx     = tceil(inputBox.getLx());
  size.ly     = tceil(inputBox.getLy());
  rectOnInput = TRectD(inputBox.getP00(), size);
}

//------------------------------------------------------------------------------

int TileFx::getMemoryRequirement(const TRectD &rect, double frame,
                                 const TRenderSettings &info) {
  TRectD inputBox;
  m_input->getBBox(frame, inputBox, info);

  double scale = sqrt(fabs(info.m_affine.det()));
  int margin   = m_margin->getValue(frame) * scale;
  inputBox     = inputBox.enlarge(margin);

  return TRasterFx::memorySize(inputBox, info.m_bpp);
}

//------------------------------------------------------------------------------

void TileFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  TRectD inputBox;
  m_input->getBBox(frame, inputBox, ri);

  double scale = sqrt(fabs(ri.m_affine.det()));
  int margin   = m_margin->getValue(frame) * scale;
  inputBox     = inputBox.enlarge(margin);

  if (inputBox.isEmpty()) return;

  if (inputBox == TConsts::infiniteRectD) {
    m_input->compute(tile, frame, ri);
    return;
  }

  TDimension size(0, 0);
  size.lx = tceil(inputBox.getLx());
  size.ly = tceil(inputBox.getLy());
  TTile inputTile;
  m_input->allocateAndCompute(inputTile, inputBox.getP00(), size,
                              tile.getRaster(), frame, ri);
  makeTile(inputTile, tile);
}

//------------------------------------------------------------------------------
//! Make the tile of the image contained in \b inputTile in \b tile
/*
*/
void TileFx::makeTile(const TTile &inputTile, const TTile &tile) const {
  // Build the mirroring pattern. It obviously repeats itself out of 2x2 tile
  // blocks.
  std::map<std::pair<bool, bool>, TRasterP> mirrorRaster;
  mirrorRaster[std::pair<bool, bool>(false, false)] = inputTile.getRaster();
  mirrorRaster[std::pair<bool, bool>(false, true)] =
      inputTile.getRaster()->clone();
  mirrorRaster[std::pair<bool, bool>(false, true)]->yMirror();
  mirrorRaster[std::pair<bool, bool>(true, false)] =
      inputTile.getRaster()->clone();
  mirrorRaster[std::pair<bool, bool>(true, false)]->xMirror();
  mirrorRaster[std::pair<bool, bool>(true, true)] =
      mirrorRaster[std::pair<bool, bool>(true, false)]->clone();
  mirrorRaster[std::pair<bool, bool>(true, true)]->yMirror();

  TPoint animatedPos = convert(inputTile.m_pos - tile.m_pos);
  TDimension inSize  = inputTile.getRaster()->getSize();
  TDimension outSize = tile.getRaster()->getSize();

  bool mirrorX = false, mirrorY = false;
  int mode             = m_mode->getValue();
  bool tileOrizontally = mode == 1 || mode == 2;
  bool tileVertically  = mode == 1 || mode == 3;

  // Reach the lower left tiling position
  while (animatedPos.x > 0 && tileOrizontally) {
    animatedPos.x -= inSize.lx;
    mirrorX = !mirrorX;
  }
  while (animatedPos.x + inSize.lx < 0 && tileOrizontally) {
    animatedPos.x += inSize.lx;
    mirrorX = !mirrorX;
  }
  while (animatedPos.y > 0 && tileVertically) {
    animatedPos.y -= inSize.ly;
    mirrorY = !mirrorY;
  }
  while (animatedPos.y + inSize.ly < 0 && tileVertically) {
    animatedPos.y += inSize.ly;
    mirrorY = !mirrorY;
  }
  bool doMirroX = mirrorX, doMirroY = mirrorY;

  // Write the tiling blocks.
  tile.getRaster()->lock();
  inputTile.getRaster()->lock();
  TPoint startTilingPos = animatedPos;
  do {
    do {
      std::pair<bool, bool> doMirror(doMirroX && m_xMirror->getValue(),
                                     doMirroY && m_yMirror->getValue());
      tile.getRaster()->copy(mirrorRaster[doMirror], startTilingPos);
      startTilingPos.x += inSize.lx;
      doMirroX = !doMirroX;
    } while (startTilingPos.x < outSize.lx && tileOrizontally);
    startTilingPos.y += inSize.ly;
    startTilingPos.x = animatedPos.x;
    doMirroY         = !doMirroY;
    doMirroX         = mirrorX;
  } while (startTilingPos.y < outSize.ly && tileVertically);
  inputTile.getRaster()->unlock();
  tile.getRaster()->unlock();
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(TileFx, "tileFx");
