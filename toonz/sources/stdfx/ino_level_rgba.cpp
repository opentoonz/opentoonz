#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_level_rgba final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_level_rgba)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TRangeParamP m_red_in;
  TRangeParamP m_red_out;
  TDoubleParamP m_red_gamma;
  TRangeParamP m_gre_in;
  TRangeParamP m_gre_out;
  TDoubleParamP m_gre_gamma;
  TRangeParamP m_blu_in;
  TRangeParamP m_blu_out;
  TDoubleParamP m_blu_gamma;
  TRangeParamP m_alp_in;
  TRangeParamP m_alp_out;
  TDoubleParamP m_alp_gamma;

  TBoolParamP m_anti_alias;
  TIntEnumParamP m_ref_mode;

public:
  ino_level_rgba()
      : m_red_in(DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_red_out(
            DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_red_gamma(1.0 * ino::param_range())
      , m_gre_in(DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_gre_out(
            DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_gre_gamma(1.0 * ino::param_range())
      , m_blu_in(DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_blu_out(
            DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_blu_gamma(1.0 * ino::param_range())
      , m_alp_in(DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_alp_out(
            DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_alp_gamma(1.0 * ino::param_range())

      , m_anti_alias(true)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "red_in", this->m_red_in);
    bindParam(this, "red_out", this->m_red_out);
    bindParam(this, "red_gamma", this->m_red_gamma);
    bindParam(this, "gre_in", this->m_gre_in);
    bindParam(this, "gre_out", this->m_gre_out);
    bindParam(this, "gre_gamma", this->m_gre_gamma);
    bindParam(this, "blu_in", this->m_blu_in);
    bindParam(this, "blu_out", this->m_blu_out);
    bindParam(this, "blu_gamma", this->m_blu_gamma);
    bindParam(this, "alp_in", this->m_alp_in);
    bindParam(this, "alp_out", this->m_alp_out);
    bindParam(this, "alp_gamma", this->m_alp_gamma);

    bindParam(this, "anti_alias", this->m_anti_alias);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_red_in->getMin()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_red_in->getMax()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_red_out->getMin()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_red_out->getMax()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_red_gamma->setValueRange(
        0.1 * ino::param_range(), 10.0 * ino::param_range()); /* red_gamma値 */
    this->m_gre_in->getMin()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_gre_in->getMax()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_gre_out->getMin()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_gre_out->getMax()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_gre_gamma->setValueRange(
        0.1 * ino::param_range(), 10.0 * ino::param_range()); /* gre_gamma値 */
    this->m_blu_in->getMin()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_blu_in->getMax()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_blu_out->getMin()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_blu_out->getMax()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_blu_gamma->setValueRange(
        0.1 * ino::param_range(), 10.0 * ino::param_range()); /* blu_gamma値 */
    this->m_alp_in->getMin()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_alp_in->getMax()->setValueRange(0.0 * ino::param_range(),
                                            1.0 * ino::param_range());
    this->m_alp_out->getMin()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_alp_out->getMax()->setValueRange(0.0 * ino::param_range(),
                                             1.0 * ino::param_range());
    this->m_alp_gamma->setValueRange(
        0.1 * ino::param_range(), 10.0 * ino::param_range()); /* alp_gamma値 */

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
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_level_rgba, "inoLevelrgbaFx");
//------------------------------------------------------------
#include "igs_levels.h"
void ino_level_rgba::doCompute(TTile &tile, double frame,
                               const TRenderSettings &rend_sets) {
  /* ------ 接続していなければ処理しない -------------------- */
  if (!this->m_input.isConnected()) {
    tile.getRaster()->clear(); /* 塗りつぶしクリア */
    return;
  }

  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  /* ------ 動作パラメータを得る ---------------------------- */
  DoublePair v_red_in  = this->m_red_in->getValue(frame);
  DoublePair v_red_out = this->m_red_out->getValue(frame);
  double red_gamma = this->m_red_gamma->getValue(frame) / ino::param_range();
  v_red_in.first /= ino::param_range();
  v_red_in.second /= ino::param_range();
  v_red_out.first /= ino::param_range();
  v_red_out.second /= ino::param_range();

  DoublePair v_gre_in  = this->m_gre_in->getValue(frame);
  DoublePair v_gre_out = this->m_gre_out->getValue(frame);
  double gre_gamma = this->m_gre_gamma->getValue(frame) / ino::param_range();
  v_gre_in.first /= ino::param_range();
  v_gre_in.second /= ino::param_range();
  v_gre_out.first /= ino::param_range();
  v_gre_out.second /= ino::param_range();

  DoublePair v_blu_in  = this->m_blu_in->getValue(frame);
  DoublePair v_blu_out = this->m_blu_out->getValue(frame);
  double blu_gamma = this->m_blu_gamma->getValue(frame) / ino::param_range();
  v_blu_in.first /= ino::param_range();
  v_blu_in.second /= ino::param_range();
  v_blu_out.first /= ino::param_range();
  v_blu_out.second /= ino::param_range();

  DoublePair v_alp_in  = this->m_alp_in->getValue(frame);
  DoublePair v_alp_out = this->m_alp_out->getValue(frame);
  double alp_gamma = this->m_alp_gamma->getValue(frame) / ino::param_range();
  v_alp_in.first /= ino::param_range();
  v_alp_in.second /= ino::param_range();
  v_alp_out.first /= ino::param_range();
  v_alp_out.second /= ino::param_range();

  const bool anti_alias_sw = this->m_anti_alias->getValue();
  const int refer_mode     = this->m_ref_mode->getValue();

  /* ------ 画像生成 ---------------------------------------- */
  this->m_input->compute(tile, frame, rend_sets);

  /*------ 参照画像生成 --------------------------------------*/
  TTile refer_tile;
  bool refer_sw = false;
  if (this->m_refer.isConnected()) {
    refer_sw = true;
    this->m_refer->allocateAndCompute(
        refer_tile, tile.m_pos,
        TDimensionI(/* Pixel単位 */
                    tile.getRaster()->getLx(), tile.getRaster()->getLy()),
        tile.getRaster(), frame, rend_sets);
  }

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  red_in_min " << v_red_in.first << "  red_in_max "
       << v_red_in.second << "  red_out_min " << v_red_out.first
       << "  red_out_max " << v_red_out.second << "  red_gamma " << red_gamma
       << "  gre_in_min " << v_gre_in.first << "  gre_in_max "
       << v_gre_in.second << "  gre_out_min " << v_gre_out.first
       << "  gre_out_max " << v_gre_out.second << "  gre_gamma " << gre_gamma
       << "  blu_in_min " << v_blu_in.first << "  blu_in_max "
       << v_blu_in.second << "  blu_out_min " << v_blu_out.first
       << "  blu_out_max " << v_blu_out.second << "  blu_gamma " << blu_gamma
       << "  alp_in_min " << v_alp_in.first << "  alp_in_max "
       << v_alp_in.second << "  alp_out_min " << v_alp_out.first
       << "  alp_out_max " << v_alp_out.second << "  alp_gamma " << alp_gamma
       << "  anti_alias " << anti_alias_sw << "  reference " << refer_mode
       << "   tile w " << tile.getRaster()->getLx() << "  h "
       << tile.getRaster()->getLy() << "  pixbits "
       << ino::pixel_bits(tile.getRaster()) << "   frame " << frame;
    if (refer_sw) {
      os << "  refer_tile.m_pos " << refer_tile.m_pos << "  refer_tile_getLx "
         << refer_tile.getRaster()->getLx() << "  y "
         << refer_tile.getRaster()->getLy();
    }
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    TRasterP in_ras = tile.getRaster();

    TRasterGR8P in_gr8(in_ras->getLy(),
                       in_ras->getLx() * ino::channels() *
                           ((TRaster64P)in_ras ? sizeof(unsigned short)
                                               : sizeof(unsigned char)));

    in_ras->lock();
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->lock();
    }
    in_gr8->lock();

    ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

    const TRasterP refer_ras = (refer_sw ? refer_tile.getRaster() : nullptr);
    igs::levels::change(
        in_gr8->getRawData()

            ,
        in_ras->getLy(), in_ras->getLx()  // Not use in_ras->getWrap()
        ,
        ino::channels(), ino::bits(in_ras)

                             ,
        (((refer_ras != nullptr) && (0 <= refer_mode)) ? refer_ras->getRawData()
                                                       : nullptr)  // BGRA
        ,
        (((refer_ras != nullptr) && (0 <= refer_mode)) ? ino::bits(refer_ras)
                                                       : 0),
        refer_mode

        ,
        v_red_in.first, v_red_in.second, v_gre_in.first, v_gre_in.second,
        v_blu_in.first, v_blu_in.second, v_alp_in.first, v_alp_in.second,
        red_gamma, gre_gamma, blu_gamma, alp_gamma, v_red_out.first,
        v_red_out.second, v_gre_out.first, v_gre_out.second, v_blu_out.first,
        v_blu_out.second, v_alp_out.first, v_alp_out.second

        ,
        true  // clamp_sw
        ,
        true  // alpha_rendering_sw
        ,
        anti_alias_sw  // --> add_blend_sw, default is true
        );

    ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);

    in_gr8->unlock();
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    in_ras->unlock();
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::bad_alloc &e) {
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
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
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}
