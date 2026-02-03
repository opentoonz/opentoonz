

#include "stdfx.h"
#include "tfxparam.h"
#include "ttzpimagefx.h"
#include "trop.h"
#include "texturefxP.h"

//===================================================================

class TextureFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(TextureFx)

  TRasterFxPort m_input;
  TRasterFxPort m_texture;
  TStringParamP m_string;
  TIntEnumParamP m_keep;
  // TIntEnumParamP m_type;
  TIntEnumParamP m_mode;
  TDoubleParamP m_value;

public:
  TextureFx()
      : m_string(L"1,2,3")
      , m_keep(new TIntEnumParam(0, "Delete"))
      //, m_type(new TIntEnumParam(0, "Lines & Areas"))
      , m_mode(new TIntEnumParam(SUBSTITUTE, "Texture"))
      , m_value(100) {
    addInputPort("Source", m_input);
    addInputPort("Texture", m_texture);
    bindParam(this, "indexes", m_string);
    bindParam(this, "keep", m_keep);
    bindParam(this, "mode", m_mode);
    // bindParam(this,"type",  m_type);
    bindParam(this, "value", m_value);
    m_value->setValueRange(0, 100);
    m_keep->addItem(1, "Keep");
    // m_type->addItem(1, "Lines");
    // m_type->addItem(2, "Areas");
    m_mode->addItem(PATTERNTYPE, "Pattern");
    m_mode->addItem(ADD, "Add");
    m_mode->addItem(SUBTRACT, "Subtract");
    m_mode->addItem(MULTIPLY, "Multiply");
    m_mode->addItem(LIGHTEN, "Lighten");
    m_mode->addItem(DARKEN, "Darken");
  }

  ~TextureFx(){};

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

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;

  bool allowUserCacheOnPort(int port) override { return port != 0; }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//-------------------------------------------------------------------

void TextureFx::doDryCompute(TRectD &rect, double frame,
                             const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  std::vector<std::string> items;
  std::string indexes = ::to_string(m_string->getValue());
  parseIndexes(indexes, items);
  TRenderSettings ri2(info);
  PaletteFilterFxRenderData *PaletteFilterData = new PaletteFilterFxRenderData;
  insertIndexes(items, PaletteFilterData);
  PaletteFilterData->m_keep = (bool)(m_keep->getValue() == 1);

  ri2.m_data.push_back(PaletteFilterData);
  ri2.m_userCachable = false;

  // First child compute: part of output that IS NOT texturized
  m_input->dryCompute(rect, frame, ri2);

  if (!m_texture.isConnected()) return;

  bool isSwatch                = ri2.m_isSwatch;
  if (isSwatch) ri2.m_isSwatch = false;
  PaletteFilterData->m_keep    = !(m_keep->getValue());

  // Second child compute: part of output that IS to be texturized
  m_input->dryCompute(rect, frame, ri2);

  // Third child compute: texture
  m_texture->dryCompute(rect, frame, info);
}

//-------------------------------------------------------------------

void TextureFx::doCompute(TTile &tile, double frame,
                          const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

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
  PaletteFilterData->m_keep                    = !!(m_keep->getValue());
  insertIndexes(items, PaletteFilterData);

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
  PaletteFilterData->m_keep    = !(m_keep->getValue());
  m_input->compute(tile, frame, ri2);
  if (isSwatch) ri2.m_isSwatch = true;

  // controllo se ho ottenuto quaclosa su cui si possa lavorare.
  TRect box;
  TRop::computeBBox(tile.getRaster(), box);
  if (box.isEmpty()) {
    m_input->compute(tile, frame, ri);  // Could the invertMask be copied??
    return;
  }

  // Then, generate the texture tile
  TTile textureTile;
  TDimension size = tile.getRaster()->getSize();
  TPointD pos     = tile.m_pos;
  m_texture->allocateAndCompute(textureTile, pos, size, tile.getRaster(), frame,
                                ri);

  // And copy the part corresponding to mask tile
  /*TDimension dim = tile.getRaster()->getSize();
TRasterP appRas = tile.getRaster()->create(dim.lx,dim.ly);
  appRas->clear();
  appRas->copy(textureTile.getRaster(),convert(textureTile.m_pos-tile.m_pos));
  textureTile.setRaster(appRas);*/

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
    }
  }

  TRop::over(tile.getRaster(), invertMaskTile.getRaster());
}

//------------------------------------------------------------------

int TextureFx::getMemoryRequirement(const TRectD &rect, double frame,
                                    const TRenderSettings &info) {
  return TRasterFx::memorySize(rect, info.m_bpp);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(TextureFx, "textureFx");
