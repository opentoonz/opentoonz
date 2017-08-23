#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_level_master final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_level_master)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TRangeParamP m_in;
  TRangeParamP m_out;
  TDoubleParamP m_gamma;
  TBoolParamP m_alpha_rendering;

  TBoolParamP m_anti_alias;
  TIntEnumParamP m_ref_mode;

public:
  ino_level_master()
      : m_in(DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_out(DoublePair(0.0 * ino::param_range(), 1.0 * ino::param_range()))
      , m_gamma(1.0 * ino::param_range())
      , m_alpha_rendering(false)

      , m_anti_alias(true)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "in", this->m_in);
    bindParam(this, "out", this->m_out);
    bindParam(this, "gamma", this->m_gamma);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);

    bindParam(this, "anti_alias", this->m_anti_alias);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_in->getMin()->setValueRange(0.0 * ino::param_range(),
                                        1.0 * ino::param_range());
    this->m_in->getMax()->setValueRange(0.0 * ino::param_range(),
                                        1.0 * ino::param_range());
    this->m_out->getMin()->setValueRange(0.0 * ino::param_range(),
                                         1.0 * ino::param_range());
    this->m_out->getMax()->setValueRange(0.0 * ino::param_range(),
                                         1.0 * ino::param_range());
    this->m_gamma->setValueRange(0.1 * ino::param_range(),
                                 10.0 * ino::param_range()); /* gamma値 */

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
FX_PLUGIN_IDENTIFIER(ino_level_master, "inoLevelMasterFx");
//------------------------------------------------------------
#include "igs_levels.h"
void ino_level_master::doCompute(TTile &tile, double frame,
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
  DoublePair v_in        = this->m_in->getValue(frame);
  DoublePair v_out       = this->m_out->getValue(frame);
  double gamma           = this->m_gamma->getValue(frame) / ino::param_range();
  const bool alp_rend_sw = this->m_alpha_rendering->getValue();
  const bool anti_alias_sw = this->m_anti_alias->getValue();
  const int refer_mode     = this->m_ref_mode->getValue();
  v_in.first /= ino::param_range();
  v_in.second /= ino::param_range();
  v_out.first /= ino::param_range();
  v_out.second /= ino::param_range();

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
       << "  in_min " << v_in.first << "  in_max " << v_in.second
       << "  out_min " << v_out.first << "  out_max " << v_out.second
       << "  gamma " << gamma << "  alp_rend_sw " << alp_rend_sw
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
        v_in.first, v_in.second, v_in.first, v_in.second, v_in.first,
        v_in.second, v_in.first, v_in.second, gamma, gamma, gamma, gamma,
        v_out.first, v_out.second, v_out.first, v_out.second, v_out.first,
        v_out.second, v_out.first, v_out.second

        ,
        true  // clamp_sw
        ,
        alp_rend_sw, anti_alias_sw  // --> add_blend_sw, default is true
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
