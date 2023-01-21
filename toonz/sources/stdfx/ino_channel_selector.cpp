#include <sstream> /* std::ostringstream */
/* Not use boost at toonz-6.1 */
// #include <boost/shared_array.hpp> /* boost::shared_array<> */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_channel_selector final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_channel_selector)

  TRasterFxPort m_source1;
  TRasterFxPort m_source2;
  TRasterFxPort m_source3;
  TRasterFxPort m_source4;

  TIntParamP m_red_source;
  TIntParamP m_gre_source;
  TIntParamP m_blu_source;
  TIntParamP m_alp_source;

  TIntEnumParamP m_red_channel;
  TIntEnumParamP m_gre_channel;
  TIntEnumParamP m_blu_channel;
  TIntEnumParamP m_alp_channel;

public:
  ino_channel_selector()
      : m_red_source(1)
      , m_gre_source(1)
      , m_blu_source(1)
      , m_alp_source(1)

      , m_red_channel(new TIntEnumParam(0, "Red"))
      , m_gre_channel(new TIntEnumParam(1, "Green"))
      , m_blu_channel(new TIntEnumParam(2, "Blue"))
      , m_alp_channel(new TIntEnumParam(3, "Alpha")) {
    addInputPort("Source1", this->m_source1);
    addInputPort("Source2", this->m_source2);
    addInputPort("Source3", this->m_source3);
    addInputPort("Source4", this->m_source4);

    bindParam(this, "red_source", this->m_red_source);
    bindParam(this, "green_source", this->m_gre_source);
    bindParam(this, "blue_source", this->m_blu_source);
    bindParam(this, "alpha_source", this->m_alp_source);

    bindParam(this, "red_channel", this->m_red_channel);
    bindParam(this, "green_channel", this->m_gre_channel);
    bindParam(this, "blue_channel", this->m_blu_channel);
    bindParam(this, "alpha_channel", this->m_alp_channel);

    this->m_red_channel->addItem(1, "Green");
    this->m_red_channel->addItem(2, "Blue");
    this->m_red_channel->addItem(3, "Alpha");

    this->m_gre_channel->addItem(0, "Red");
    this->m_gre_channel->addItem(2, "Blue");
    this->m_gre_channel->addItem(3, "Alpha");

    this->m_blu_channel->addItem(0, "Red");
    this->m_blu_channel->addItem(1, "Green");
    this->m_blu_channel->addItem(3, "Alpha");

    this->m_alp_channel->addItem(0, "Red");
    this->m_alp_channel->addItem(1, "Green");
    this->m_alp_channel->addItem(2, "Blue");

    enableComputeInFloat(true);
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    for (int ii = 0; ii < this->getInputPortCount(); ++ii) {
      std::string nm          = this->getInputPortName(ii);
      TRasterFxPort *tmp_port = (TRasterFxPort *)this->getInputPort(nm);
      if (tmp_port->isConnected()) {
        return (*tmp_port)->doGetBBox(frame, bBox, info);
      }
    }
    bBox = TRectD();
    return false;
  }
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
};
FX_PLUGIN_IDENTIFIER(ino_channel_selector, "inoChannelSelectorFx");
//--------------------------------------------------------------------
// #include "igs_channel_selector.h"
namespace {
template <typename IN_PIXEL, typename OUT_PIXEL>
void fx_template(const TRasterPT<IN_PIXEL> in_ras, const int in_sel,
                 const TRasterPT<OUT_PIXEL> out_ras, const int out_sel) {
  for (int yy = 0; yy < out_ras->getLy(); ++yy) {
    IN_PIXEL *sl_in   = in_ras->pixels(yy);
    OUT_PIXEL *sl_out = out_ras->pixels(yy);
    for (int xx = 0; xx < out_ras->getLx(); ++xx, ++sl_in, ++sl_out) {
      typename IN_PIXEL::Channel val = 0;
      switch (in_sel) {
      case 0:
        val = sl_in->r;
        break;
      case 1:
        val = sl_in->g;
        break;
      case 2:
        val = sl_in->b;
        break;
      case 3:
        val = sl_in->m;
        break;
      }
      switch (out_sel) {
      case 0:
        sl_out->r = val;
        break;
      case 1:
        sl_out->g = val;
        break;
      case 2:
        sl_out->b = val;
        break;
      case 3:
        // clamp the alpha channel in case computing TPixelF
        sl_out->m = (val < 0) ? 0
                    : (val > OUT_PIXEL::maxChannelValue)
                        ? OUT_PIXEL::maxChannelValue
                        : val;
        break;
      }
    }
  }
}
void fx_(const TRasterP in_ras, const int in_sel, TRasterP out_ras,
         const int out_sel) {
  if ((TRaster32P)in_ras && (TRaster32P)out_ras) {
    fx_template<TPixel32, TPixel32>(in_ras, in_sel, out_ras, out_sel);
  } else if ((TRaster64P)in_ras && (TRaster64P)out_ras) {
    fx_template<TPixel64, TPixel64>(in_ras, in_sel, out_ras, out_sel);
  } else if ((TRasterFP)in_ras && (TRasterFP)out_ras) {
    fx_template<TPixelF, TPixelF>(in_ras, in_sel, out_ras, out_sel);
  }
}
}  // namespace
//------------------------------------------------------------
void ino_channel_selector::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &ri) {
  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster()) &&
      !((TRasterFP)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  /* ------ 動作パラメータを得る ---------------------------- */
  const int red_source  = this->m_red_source->getValue() - 1;
  const int gre_source  = this->m_gre_source->getValue() - 1;
  const int blu_source  = this->m_blu_source->getValue() - 1;
  const int alp_source  = this->m_alp_source->getValue() - 1;
  const int red_channel = this->m_red_channel->getValue();
  const int gre_channel = this->m_gre_channel->getValue();
  const int blu_channel = this->m_blu_channel->getValue();
  const int alp_channel = this->m_alp_channel->getValue();

  /* ------ 画像位置とサイズ -------------------------------- */
  /***const TRectD rect = TRectD( tile.m_pos, TDimensionD(
            tile.getRaster()->getLx()
          , tile.getRaster()->getLy()
  ));
  const TPointD poin = TPointD(rect.getP00());
  const TDimension dime = TDimension(
           (int)(rect.getLx()+0.5)
          ,(int)(rect.getLy()+0.5)
  );***/

  /* ------ 塗りつぶしクリア -------------------------------- */
  tile.getRaster()->clear();

  /* ------ 入力画像を接続していなければ処理しない ---------- */
  if (this->getInputPortCount() <= 0) {
    return;
  }

  /* ------ 入力画像の参照を確保 ---------------------------- */
  // TTile *source_tiles = new TTile[this->getInputPortCount()];
  // int   *source_sw    = new int[this->getInputPortCount()];
  // TRasterP *ras_a     = new TRasterP[this->getInputPortCount()];

  /* Not use boost at toonz-6.1 */
  /******
  boost::shared_array<TTile>
          source_tiles(new TTile[this->getInputPortCount()]);
  boost::shared_array<int>
          source_sw(new int[this->getInputPortCount()]);
  boost::shared_array<TRasterP>
          ras_a(new TRasterP[this->getInputPortCount()]);
******/
  /* Array item(TRasterFxPort) number is 4(fix) in this code */
  TTile source_tiles[4];
  int source_sw[4];
  TRasterP ras_a[4];

  int ras_s = 0;

  /* ------ 画像生成 ---------------------------------------- */
  for (int ii = 0; ii < this->getInputPortCount(); ++ii) {
    std::string nm          = this->getInputPortName(ii);
    TRasterFxPort *tmp_port = (TRasterFxPort *)this->getInputPort(nm);
    if (tmp_port->isConnected() && ((ii == red_source) || (ii == gre_source) ||
                                    (ii == blu_source) || (ii == alp_source))) {
      (*tmp_port)->allocateAndCompute(
          source_tiles[ii]
          //,poin,dime
          ,
          tile.m_pos /* 位置 */
          ,
          TDimension(/* サイズ */
                     tile.getRaster()->getLx(), tile.getRaster()->getLy()),
          tile.getRaster() /* sampling */
          ,
          frame, ri);
      source_sw[ii]  = 1;
      ras_a[ras_s++] = source_tiles[ii].getRaster();
    } else
      source_sw[ii] = 0;
  }

  TRasterP red_ras = 0;
  TRasterP gre_ras = 0;
  TRasterP blu_ras = 0;
  TRasterP alp_ras = 0;

  if ((0 <= red_source) && (red_source < this->getInputPortCount()) &&
      source_sw[red_source]) {
    red_ras = source_tiles[red_source].getRaster();
  }
  if ((0 <= gre_source) && (gre_source < this->getInputPortCount()) &&
      source_sw[gre_source]) {
    gre_ras = source_tiles[gre_source].getRaster();
  }
  if ((0 <= blu_source) && (blu_source < this->getInputPortCount()) &&
      source_sw[blu_source]) {
    blu_ras = source_tiles[blu_source].getRaster();
  }
  if ((0 <= alp_source) && (alp_source < this->getInputPortCount()) &&
      source_sw[alp_source]) {
    alp_ras = source_tiles[alp_source].getRaster();
  }

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "red"
       << "  s " << red_source << "  c " << red_channel << "   green"
       << "  s " << gre_source << "  c " << gre_channel << "   blue"
       << "  s " << blu_source << "  c " << blu_channel << "   alpha"
       << "  s " << alp_source << "  c " << alp_channel << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  b " << ino::pixel_bits(tile.getRaster());
    os << "   s_count " << this->getInputPortCount();
    for (int ii = 0; ii < this->getInputPortCount(); ++ii) {
      if (source_sw[ii]) {
        os << "   tile" << ii << "  w " << source_tiles[ii].getRaster()->getLx()
           << "  h " << source_tiles[ii].getRaster()->getLy() << "  b "
           << ino::pixel_bits(source_tiles[ii].getRaster());
      }
    }
    os << "   frame " << frame;
  }
  /* ------ 入力画像の参照開放 ------------------------------ */
  // delete [] source_sw;
  // delete [] source_tiles;
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    for (int ii = 0; ii < ras_s; ++ii) {
      ras_a[ii]->lock();
    }
    if (red_ras) {
      fx_(red_ras, red_channel, tile.getRaster(), 0);
    }
    if (gre_ras) {
      fx_(gre_ras, gre_channel, tile.getRaster(), 1);
    }
    if (blu_ras) {
      fx_(blu_ras, blu_channel, tile.getRaster(), 2);
    }
    if (alp_ras) {
      fx_(alp_ras, alp_channel, tile.getRaster(), 3);
    }
    for (int ii = ras_s - 1; 0 <= ii; --ii) {
      ras_a[ii]->unlock();
    }
    tile.getRaster()->unlock();
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::bad_alloc &e) {
    for (int ii = ras_s - 1; 0 <= ii; --ii) {
      ras_a[ii]->unlock();
    }
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("std::bad_alloc <");
      str += e.what();
      str += '>';
    }
    // delete [] ras_a;
    throw;
  } catch (std::exception &e) {
    for (int ii = ras_s - 1; 0 <= ii; --ii) {
      ras_a[ii]->unlock();
    }
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("exception <");
      str += e.what();
      str += '>';
    }
    // delete [] ras_a;
    throw;
  } catch (...) {
    for (int ii = ras_s - 1; 0 <= ii; --ii) {
      ras_a[ii]->unlock();
    }
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("other exception");
    }
    // delete [] ras_a;
    throw;
  }
  // delete [] ras_a;
}
