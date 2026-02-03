/*-----------------------------------------------------------------------------
 Iwa_TileFx based on TileFx by Digital Video
 カメラ枠を基準にマージンを加えて敷き詰めることができ、繰り返しの有無も指定できる
 -------------------------------------------------------------------------------*/

#include "stdfx.h"
#include "tfxparam.h"
#include "ttzpimagefx.h"

#include "timage_io.h"

//=============================================================================

class Iwa_TileFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_TileFx)

  enum tileQuantity { eNoTile = 1, eOneTile = 2, eMultipleTiles = 3 };

  enum inputSize { eBoundingBox = 1, eCameraBox = 2, eImageSize = 3 };

  TIntEnumParamP m_inputSizeMode;
  TRasterFxPort m_input;
  TIntEnumParamP m_leftQuantity;
  TIntEnumParamP m_rightQuantity;
  TIntEnumParamP m_topQuantity;
  TIntEnumParamP m_bottomQuantity;
  TBoolParamP m_xMirror;
  TBoolParamP m_yMirror;
  TDoubleParamP m_hmargin;
  TDoubleParamP m_vmargin;

public:
  Iwa_TileFx();
  ~Iwa_TileFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override;
  TRect getInvalidRect(const TRect &max) { return max; }
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  bool checkIfThisTileShouldBeComptedOrNot(int horizIndex, int vertIndex);
  bool isInRange(int quantityMode, int index);

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override {
    return tileIsLinear;
  }

private:
  void makeTile(const TTile &inputTile, const TTile &tile);
};

//------------------------------------------------------------------------------

Iwa_TileFx::Iwa_TileFx()
    : m_inputSizeMode(new TIntEnumParam(eBoundingBox, "Bounding Box"))
    , m_leftQuantity(new TIntEnumParam(eNoTile, "No Tile"))
    , m_rightQuantity(new TIntEnumParam(eNoTile, "No Tile"))
    , m_topQuantity(new TIntEnumParam(eNoTile, "No Tile"))
    , m_bottomQuantity(new TIntEnumParam(eNoTile, "No Tile"))
    , m_xMirror(false)
    , m_yMirror(false)
    , m_hmargin(
          5.24934) /*- スタンダードサイズのカメラとLevelのマージン（2.5mm）-*/
    , m_vmargin(12.4934) /*- 5.95mm -*/
{
  addInputPort("Source", m_input);

  bindParam(this, "inputSize", m_inputSizeMode);
  m_inputSizeMode->addItem(eCameraBox, "Camera Box");
  m_inputSizeMode->addItem(eImageSize, "Image Size");

  bindParam(this, "leftQuantity", m_leftQuantity);
  m_leftQuantity->addItem(eOneTile, "1 Tile");
  m_leftQuantity->addItem(eMultipleTiles, "Multiple Tiles");

  bindParam(this, "rightQuantity", m_rightQuantity);
  m_rightQuantity->addItem(eOneTile, "1 Tile");
  m_rightQuantity->addItem(eMultipleTiles, "Multiple Tiles");

  bindParam(this, "xMirror", m_xMirror);

  bindParam(this, "hMargin", m_hmargin);
  m_hmargin->setMeasureName("fxLength");

  bindParam(this, "topQuantity", m_topQuantity);
  m_topQuantity->addItem(eOneTile, "1 Tile");
  m_topQuantity->addItem(eMultipleTiles, "Multiple Tiles");

  bindParam(this, "bottomQuantity", m_bottomQuantity);
  m_bottomQuantity->addItem(eOneTile, "1 Tile");
  m_bottomQuantity->addItem(eMultipleTiles, "Multiple Tiles");

  bindParam(this, "yMirror", m_yMirror);

  bindParam(this, "vMargin", m_vmargin);
  m_vmargin->setMeasureName("fxLength");

  enableComputeInFloat(true);
}

//------------------------------------------------------------------------------

Iwa_TileFx::~Iwa_TileFx() {}

//------------------------------------------------------------------------------

bool Iwa_TileFx::canHandle(const TRenderSettings &info, double frame) {
  // Currently, only affines which transform the X and Y axis into themselves
  // may
  // be handled by this fx...
  return (fabs(info.m_affine.a12) < 0.0001 &&
          fabs(info.m_affine.a21) < 0.0001) ||
         (fabs(info.m_affine.a11) < 0.0001 && fabs(info.m_affine.a22) < 0.0001);
}

//------------------------------------------------------------------------------

bool Iwa_TileFx::doGetBBox(double frame, TRectD &bBox,
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

void Iwa_TileFx::transform(double frame, int port, const TRectD &rectOnOutput,
                           const TRenderSettings &infoOnOutput,
                           TRectD &rectOnInput, TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;

  if (!m_input.isConnected()) {
    rectOnInput.empty();
    return;
  }

  TRectD inputBox;
  m_input->getBBox(frame, inputBox, infoOnOutput);

  double scale = sqrt(fabs(infoOnOutput.m_affine.det()));
  int hmargin  = m_hmargin->getValue(frame) * scale;
  int vmargin  = m_vmargin->getValue(frame) * scale;
  inputBox     = inputBox.enlarge(hmargin, vmargin);

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

int Iwa_TileFx::getMemoryRequirement(const TRectD &rect, double frame,
                                     const TRenderSettings &info) {
  if (!m_input.isConnected()) return 0;

  TRectD inputBox;
  m_input->getBBox(frame, inputBox, info);

  double scale = sqrt(fabs(info.m_affine.det()));
  int hmargin  = m_hmargin->getValue(frame) * scale;
  int vmargin  = m_vmargin->getValue(frame) * scale;
  inputBox     = inputBox.enlarge(hmargin, vmargin);

  return TRasterFx::memorySize(inputBox, info.m_bpp);
}

//------------------------------------------------------------------------------

void Iwa_TileFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  if (!m_input.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  TRectD inputBox;

  int inputSizeMode =
      m_inputSizeMode
          ->getValue();  //	eBoundingBox = 1, eCameraBox = 2, eImageSize = 3
  if (inputSizeMode == eBoundingBox)
    m_input->getBBox(frame, inputBox, ri);
  else if (inputSizeMode == eCameraBox) {
    TPointD offset = tile.m_pos + tile.getRaster()->getCenterD();

    inputBox = TRectD(
        TPointD(ri.m_cameraBox.x0 + offset.x, ri.m_cameraBox.y0 + offset.y),
        TDimensionD(ri.m_cameraBox.getLx(), ri.m_cameraBox.getLy()));
  } else if (inputSizeMode == eImageSize) {
    TRenderSettings riAux(ri);
    // set a tentative flag to obtain full image size. (see
    // TLevelColumnFx::doGetBBox)
    riAux.m_getFullSizeBBox = true;
    m_input->getBBox(frame, inputBox, riAux);
  }

  double scale = sqrt(fabs(ri.m_affine.det()));
  int hmargin  = m_hmargin->getValue(frame) * scale;
  int vmargin  = m_vmargin->getValue(frame) * scale;
  inputBox     = inputBox.enlarge(hmargin, vmargin);

  if (inputBox.isEmpty()) {
    tile.getRaster()->clear();
    return;
  }
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

bool Iwa_TileFx::isInRange(int quantityMode, int index) {
  switch (quantityMode) {
  case eNoTile:
    return false;
    break;
  case eOneTile:
    return (abs(index) < 2);
    break;
  case eMultipleTiles:
    return true;
    break;
  default:
    return false;
    break;
  }
}

//------------------------------------------------------------------------------

bool Iwa_TileFx::checkIfThisTileShouldBeComptedOrNot(int horizIndex,
                                                     int vertIndex) {
  int leftQuantity   = m_leftQuantity->getValue();
  int rightQuantity  = m_rightQuantity->getValue();
  int topQuantity    = m_topQuantity->getValue();
  int bottomQuantity = m_bottomQuantity->getValue();

  bool horizInRange, vertInRange;

  if (horizIndex == 0)
    horizInRange = true;
  else if (horizIndex < 0)
    horizInRange = isInRange(leftQuantity, horizIndex);
  else  // horizIndex > 0
    horizInRange = isInRange(rightQuantity, horizIndex);

  if (vertIndex == 0)
    vertInRange = true;
  else if (vertIndex < 0)
    vertInRange = isInRange(bottomQuantity, vertIndex);
  else  // horizIndex > 0
    vertInRange = isInRange(topQuantity, vertIndex);

  return horizInRange & vertInRange;
}

//------------------------------------------------------------------------------
//! Make the tile of the image contained in \b inputTile in \b tile
/*
 */
void Iwa_TileFx::makeTile(const TTile &inputTile, const TTile &tile) {
  tile.getRaster()->clear();

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

  int leftQuantity   = m_leftQuantity->getValue();
  int rightQuantity  = m_rightQuantity->getValue();
  int topQuantity    = m_topQuantity->getValue();
  int bottomQuantity = m_bottomQuantity->getValue();

  bool tileHorizontally = leftQuantity != eNoTile || rightQuantity != eNoTile;
  bool tileVertically   = topQuantity != eNoTile || bottomQuantity != eNoTile;

  /*- 現在のタイルのインデックス -*/
  int horizIndex = 0;
  int vertIndex  = 0;

  /*- 左下のタイルのインデックスを計算 -*/
  // Reach the lower left tiling position
  while (animatedPos.x > 0 && tileHorizontally) {
    animatedPos.x -= inSize.lx;
    mirrorX = !mirrorX;

    horizIndex--;
  }
  while (animatedPos.x + inSize.lx < 0 && tileHorizontally) {
    animatedPos.x += inSize.lx;
    mirrorX = !mirrorX;

    horizIndex++;
  }
  while (animatedPos.y > 0 && tileVertically) {
    animatedPos.y -= inSize.ly;
    mirrorY = !mirrorY;

    vertIndex--;
  }
  while (animatedPos.y + inSize.ly < 0 && tileVertically) {
    animatedPos.y += inSize.ly;
    mirrorY = !mirrorY;

    vertIndex++;
  }

  bool doMirroX = mirrorX, doMirroY = mirrorY;

  // Write the tiling blocks.
  tile.getRaster()->lock();
  inputTile.getRaster()->lock();
  TPoint startTilingPos = animatedPos;

  int horizStartIndex = horizIndex;

  do {
    do {
      if (checkIfThisTileShouldBeComptedOrNot(horizIndex, vertIndex)) {
        std::pair<bool, bool> doMirror(doMirroX && m_xMirror->getValue(),
                                       doMirroY && m_yMirror->getValue());
        tile.getRaster()->copy(mirrorRaster[doMirror], startTilingPos);
      }

      startTilingPos.x += inSize.lx;
      doMirroX = !doMirroX;

      horizIndex++;
    } while (startTilingPos.x < outSize.lx && tileHorizontally);

    startTilingPos.y += inSize.ly;
    startTilingPos.x = animatedPos.x;
    doMirroY         = !doMirroY;
    doMirroX         = mirrorX;

    horizIndex = horizStartIndex;
    vertIndex++;
  } while (startTilingPos.y < outSize.ly && tileVertically);

  inputTile.getRaster()->unlock();
  tile.getRaster()->unlock();
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_TileFx, "iwa_TileFx");