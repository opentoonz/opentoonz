#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_hls_adjust final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_hls_adjust)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TDoubleParamP m_hue_pivot;
  TDoubleParamP m_hue_scale;
  TDoubleParamP m_hue_shift;
  TDoubleParamP m_lig_pivot;
  TDoubleParamP m_lig_scale;
  TDoubleParamP m_lig_shift;
  TDoubleParamP m_sat_pivot;
  TDoubleParamP m_sat_scale;
  TDoubleParamP m_sat_shift;

  TBoolParamP m_anti_alias;
  TIntEnumParamP m_ref_mode;

public:
  ino_hls_adjust()
      : m_hue_pivot(0.0)
      , m_hue_scale(1.0 * ino::param_range())
      , m_hue_shift(0.0)
      , m_lig_pivot(0.0 * ino::param_range())
      , m_lig_scale(1.0 * ino::param_range())
      , m_lig_shift(0.0 * ino::param_range())
      , m_sat_pivot(0.0 * ino::param_range())
      , m_sat_scale(1.0 * ino::param_range())
      , m_sat_shift(0.0 * ino::param_range())

      , m_anti_alias(true)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "pivot_hue", this->m_hue_pivot);
    bindParam(this, "pivot_lightness", this->m_lig_pivot);
    bindParam(this, "pivot_saturation", this->m_sat_pivot);
    bindParam(this, "scale_hue", this->m_hue_scale);
    bindParam(this, "scale_lightness", this->m_lig_scale);
    bindParam(this, "scale_saturation", this->m_sat_scale);
    bindParam(this, "shift_hue", this->m_hue_shift);
    bindParam(this, "shift_lightness", this->m_lig_shift);
    bindParam(this, "shift_saturation", this->m_sat_shift);

    bindParam(this, "anti_alias", this->m_anti_alias);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_hue_pivot->setValueRange(0.0, 360.0);
    this->m_hue_scale->setValueRange(0.0 * ino::param_range(),
                                     std::numeric_limits<double>::max());
    this->m_lig_pivot->setValueRange(0.0 * ino::param_range(),
                                     1.0 * ino::param_range());
    this->m_lig_scale->setValueRange(0.0 * ino::param_range(),
                                     std::numeric_limits<double>::max());
    this->m_sat_pivot->setValueRange(0.0 * ino::param_range(),
                                     1.0 * ino::param_range());
    this->m_sat_scale->setValueRange(0.0 * ino::param_range(),
                                     std::numeric_limits<double>::max());

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
FX_PLUGIN_IDENTIFIER(ino_hls_adjust, "inohlsAdjustFx");
//------------------------------------------------------------
#include "igs_hls_adjust.h"
namespace {
void fx_(TRasterP in_ras, const TRasterP refer_ras, const int refer_mode,
         const double hue_pivot, const double hue_scale, const double hue_shift,
         const double lig_pivot, const double lig_scale, const double lig_shift,
         const double sat_pivot, const double sat_scale, const double sat_shift,
         const bool anti_alias_sw) {
  /***std::vector<unsigned char> in_vec;
  ino::ras_to_vec( in_ras, ino::channels(), in_vec );***/

  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  igs::hls_adjust::change(
      // in_ras->getRawData() // BGRA
      //&in_vec.at(0) // RGBA
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
      hue_pivot, hue_scale, hue_shift, lig_pivot, lig_scale, lig_shift,
      sat_pivot, sat_scale, sat_shift

      //,true	/* add_blend_sw */
      ,
      anti_alias_sw);

  /***ino::vec_to_ras( in_vec, ino::channels(), in_ras, 0 );***/

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_hls_adjust::doCompute(TTile &tile, double frame,
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
  const double hue_pivot = this->m_hue_pivot->getValue(frame);
  const double hue_scale =
      this->m_hue_scale->getValue(frame) / ino::param_range();
  const double hue_shift = this->m_hue_shift->getValue(frame);
  const double lig_pivot =
      this->m_lig_pivot->getValue(frame) / ino::param_range();
  const double lig_scale =
      this->m_lig_scale->getValue(frame) / ino::param_range();
  const double lig_shift =
      this->m_lig_shift->getValue(frame) / ino::param_range();
  const double sat_pivot =
      this->m_sat_pivot->getValue(frame) / ino::param_range();
  const double sat_scale =
      this->m_sat_scale->getValue(frame) / ino::param_range();
  const double sat_shift =
      this->m_sat_shift->getValue(frame) / ino::param_range();
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
       << "  h_pvt " << hue_pivot << "  h_scl " << hue_scale << "  h_sft "
       << hue_shift << "  l_pvt " << lig_pivot << "  l_scl " << lig_scale
       << "  l_sft " << lig_shift << "  s_pvt " << sat_pivot << "  s_scl "
       << sat_scale << "  s_sft " << sat_shift << "  anti_alias "
       << anti_alias_sw << "  reference " << refer_mode << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "   frame "
       << frame;
    if (refer_sw) {
      os << "  refer_tile.m_pos " << refer_tile.m_pos << "  refer_tile_getLx "
         << refer_tile.getRaster()->getLx() << "  y "
         << refer_tile.getRaster()->getLy();
    }
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->lock();
    }
    fx_(tile.getRaster(), refer_tile.getRaster(), refer_mode, hue_pivot,
        hue_scale, hue_shift, lig_pivot, lig_scale, lig_shift, sat_pivot,
        sat_scale, sat_shift,
        anti_alias_sw  // --> add_blend_sw, default is true
        );
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    tile.getRaster()->unlock();
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
