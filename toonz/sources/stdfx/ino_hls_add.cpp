#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_hls_add final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_hls_add)
  TRasterFxPort m_input;
  TRasterFxPort m_noise;
  TRasterFxPort m_refer;

  TIntEnumParamP m_from_rgba;
  TDoubleParamP m_offset;
  TDoubleParamP m_hue;
  TDoubleParamP m_lig;
  TDoubleParamP m_sat;
  TDoubleParamP m_alp;

  TBoolParamP m_anti_alias;
  TIntEnumParamP m_ref_mode;

public:
  ino_hls_add()
      : m_from_rgba(new TIntEnumParam(0, "Red"))
      , m_offset(0.5 * ino::param_range())
      , m_hue(0.0 * ino::param_range())
      , m_lig(0.25 * ino::param_range())
      , m_sat(0.0 * ino::param_range())
      , m_alp(0.0 * ino::param_range())

      , m_anti_alias(true)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Noise", this->m_noise);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "from_rgba", this->m_from_rgba);
    bindParam(this, "offset", this->m_offset);
    bindParam(this, "hue", this->m_hue);
    bindParam(this, "lightness", this->m_lig);
    bindParam(this, "saturation", this->m_sat);
    bindParam(this, "alpha", this->m_alp);

    bindParam(this, "anti_alias", this->m_anti_alias);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_from_rgba->addItem(1, "Green");
    this->m_from_rgba->addItem(2, "Blue");
    this->m_from_rgba->addItem(3, "Alpha");

    this->m_offset->setValueRange(-1.0 * ino::param_range(),
                                  1.0 * ino::param_range());
    this->m_hue->setValueRange(-1.0 * ino::param_range(),
                               1.0 * ino::param_range());
    this->m_lig->setValueRange(-1.0 * ino::param_range(),
                               1.0 * ino::param_range());
    this->m_sat->setValueRange(-1.0 * ino::param_range(),
                               1.0 * ino::param_range());
    this->m_alp->setValueRange(-1.0 * ino::param_range(),
                               1.0 * ino::param_range());

    this->m_ref_mode->addItem(1, "Green");
    this->m_ref_mode->addItem(2, "Blue");
    this->m_ref_mode->addItem(3, "Alpha");
    this->m_ref_mode->addItem(4, "Luminance");
    this->m_ref_mode->addItem(-1, "Nothing");
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (this->m_input.isConnected()) {
      return this->m_input->doGetBBox(frame, bBox, info);
    } else {
      bBox = TRectD();
      return false;
    }
  }
  bool canHandle(const TRenderSettings &rend_sets, double frame) override {
    return true;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_hls_add, "inohlsAddFx");
//------------------------------------------------------------
#include "igs_hls_add.h"
namespace {
void fx_(TRasterP in_ras, const TRasterP noise_ras, const TRasterP refer_ras,
         const int refer_mode, const int xoffset, const int yoffset,
         const int from_rgba, const double offset, const double hue_scale,
         const double lig_scale, const double sat_scale, const double alp_scale,
         const bool anti_alias_sw) {
  /***std::vector<unsigned char> in_vec;
  ino::ras_to_vec( in_ras, ino::channels(), in_vec );
  std::vector<unsigned char> refer_vec;
  ino::ras_to_vec( noise_ras, ino::channels(), refer_vec );***/

  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  TRasterGR8P noise_gr8(noise_ras->getLy(),
                        noise_ras->getLx() * ino::channels() *
                            ((TRaster64P)noise_ras ? sizeof(unsigned short)
                                                   : sizeof(unsigned char)));
  noise_gr8->lock();
  ino::ras_to_arr(noise_ras, ino::channels(), noise_gr8->getRawData());

  igs::hls_add::change(
      // in_ras->getRawData() // BGRA
      //&in_vec.at(0) // RGBA
      in_gr8->getRawData()

          ,
      in_ras->getLy(), in_ras->getLx()  // Not use in_ras->getWrap()
      ,
      ino::channels(), ino::bits(in_ras)

      //,noise_ras->getRawData() // BGRA
      //,&refer_vec.at(0) // RGBA
      ,
      noise_gr8->getRawData()

          ,
      noise_ras->getLy(), noise_ras->getLx(), ino::channels(),
      ino::bits(noise_ras)

          ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? refer_ras->getRawData()
                                                     : nullptr)  // BGRA
      ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? ino::bits(refer_ras)
                                                     : 0),
      refer_mode

      ,
      xoffset, yoffset, from_rgba, offset, hue_scale, lig_scale, sat_scale,
      alp_scale

      //,true	/* add_blend_sw */
      ,
      anti_alias_sw);

  /***ino::vec_to_ras( refer_vec, 0, 0 );
  ino::vec_to_ras( in_vec, ino::channels(), in_ras, 0 );***/

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);
  noise_gr8->unlock();
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_hls_add::doCompute(TTile &tile, double frame,
                            const TRenderSettings &rend_sets) {
  /* ------ 両方とも接続していなければ処理しない ------------ */
  const bool in_cn_is    = this->m_input.isConnected();
  const bool noise_cn_is = this->m_noise.isConnected();
  if (!in_cn_is || !noise_cn_is) {
    tile.getRaster()->clear(); /* 塗りつぶしクリア */
    return;
  }

  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  /* ------ 動作パラメータを得る ---------------------------- */
  const int xoffset      = 0.0;
  const int yoffset      = 0.0;
  const int from_rgba    = this->m_from_rgba->getValue();
  const double offset    = this->m_offset->getValue(frame) / ino::param_range();
  const double hue_scale = this->m_hue->getValue(frame) / ino::param_range();
  const double lig_scale = this->m_lig->getValue(frame) / ino::param_range();
  const double sat_scale = this->m_sat->getValue(frame) / ino::param_range();
  const double alp_scale = this->m_alp->getValue(frame) / ino::param_range();
  const bool anti_alias_sw = this->m_anti_alias->getValue();
  const int refer_mode     = this->m_ref_mode->getValue();

  /* ------ 画像生成 ---------------------------------------- */
  this->m_input->compute(tile, frame, rend_sets);

  /* ------ noise画像生成 ------------------------------------ */
  TTile noise_tile;
  this->m_noise->allocateAndCompute(noise_tile, tile.m_pos,
                                    tile.getRaster()->getSize(),
                                    tile.getRaster(), frame, rend_sets);
  /*------ 参照画像生成 --------------------------------------*/
  TTile refer_tile;
  bool refer_sw = false;
  if (this->m_refer.isConnected()) {
    refer_sw = true;
    this->m_refer->allocateAndCompute(
        refer_tile, tile.m_pos,
        TDimensionI(/* Pixel単位 */
                    tile.getRaster()->getLx(),
                    tile.getRaster()
                        ->getLy()) /* ここtile.getRaster()->getSize()と同じ、将来修正する
                                      */
        ,
        tile.getRaster(), frame, rend_sets);
  }

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  xo " << xoffset << "  yo " << yoffset << "  rgba " << from_rgba
       << "  offs " << offset << "  h " << hue_scale << "  l " << lig_scale
       << "  s " << sat_scale << "  a " << alp_scale << "  anti_alias "
       << anti_alias_sw << "  reference " << refer_mode << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  pixbits " << ino::pixel_bits(tile.getRaster())
       << "   noise_tile w " << noise_tile.getRaster()->getLx() << "  h "
       << noise_tile.getRaster()->getLy() << "   frame " << frame;
    if (refer_sw) {
      os << "  refer_tile.m_pos " << refer_tile.m_pos << "  refer_tile_getLx "
         << refer_tile.getRaster()->getLx() << "  y "
         << refer_tile.getRaster()->getLy();
    }
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    noise_tile.getRaster()->lock();
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->lock();
    }
    fx_(tile.getRaster(), noise_tile.getRaster(), refer_tile.getRaster(),
        refer_mode

        ,
        xoffset, yoffset, from_rgba, offset, hue_scale, lig_scale, sat_scale,
        alp_scale, anti_alias_sw  // --> add_blend_sw, default is true
        );
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    noise_tile.getRaster()->unlock();
    tile.getRaster()->unlock();
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::bad_alloc &e) {
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    noise_tile.getRaster()->unlock();
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("std::bad_alloc <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (std::exception &e) {
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    noise_tile.getRaster()->unlock();
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("exception <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (...) {
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    noise_tile.getRaster()->unlock();
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}
