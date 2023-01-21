#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
#include "globalcontrollablefx.h"
//------------------------------------------------------------
class ino_level_auto final : public GlobalControllableFx {
  FX_PLUGIN_DECLARATION(ino_level_auto)
  TRasterFxPort m_input;
  TDoubleParamP m_in_min_shift;
  TDoubleParamP m_in_max_shift;
  TDoubleParamP m_out_min;
  TDoubleParamP m_out_max;
  TDoubleParamP m_gamma;

public:
  ino_level_auto()
      : m_in_min_shift(0.0 * ino::param_range())
      , m_in_max_shift(0.0 * ino::param_range())
      , m_out_min(0.0 * ino::param_range())
      , m_out_max(1.0 * ino::param_range())
      , m_gamma(1.0 * ino::param_range()) {
    addInputPort("Source", this->m_input);

    bindParam(this, "in_min_shift", this->m_in_min_shift);
    bindParam(this, "in_max_shift", this->m_in_max_shift);
    bindParam(this, "out_min", this->m_out_min);
    bindParam(this, "out_max", this->m_out_max);
    bindParam(this, "gamma", this->m_gamma);

    this->m_in_min_shift->setValueRange(-1.0 * ino::param_range(),
                                        1.0 * ino::param_range());
    this->m_in_max_shift->setValueRange(-1.0 * ino::param_range(),
                                        1.0 * ino::param_range());
    this->m_out_min->setValueRange(0.0 * ino::param_range(),
                                   1.0 * ino::param_range());
    this->m_out_max->setValueRange(0.0 * ino::param_range(),
                                   1.0 * ino::param_range());
    this->m_gamma->setValueRange(0.1 * ino::param_range(),
                                 10.0 * ino::param_range()); /* gamma値 */
    enableComputeInFloat(true);
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
FX_PLUGIN_IDENTIFIER(ino_level_auto, "inoLevelAutoFx");
//------------------------------------------------------------
#include "igs_level_auto_in_camera.h"
namespace {
void fx_(TRasterP in_ras, bool *act_sw, double *in_min_shift,
         double *in_max_shift, double *out_min, double *out_max, double *gamma,
         const int camera_x, const int camera_y, const int camera_w,
         const int camera_h) {
  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras   ? sizeof(unsigned short)
                          : (TRaster32P)in_ras ? sizeof(unsigned char)
                                               : sizeof(float)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  /* igs::level_auto::change(-)は今後つかわない2011-07-15 */
  igs::level_auto_in_camera::change(
      // in_ras->getRawData() // BGRA
      in_gr8->getRawData()

          ,
      in_ras->getLy(), in_ras->getLx()  // Must Not use in_ras->getWrap()
      ,
      ino::channels(), ino::bits(in_ras)

                           ,
      act_sw, in_min_shift, in_max_shift, out_min, out_max, gamma, camera_x,
      camera_y, camera_w, camera_h);

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);
  in_gr8->unlock();
}
}  // namespace
//------------------------------------------------------------
void ino_level_auto::doCompute(TTile &tile, double frame,
                               const TRenderSettings &rend_sets) {
  /* ------ 接続していなければ処理しない -------------------- */
  if (!this->m_input.isConnected()) {
    tile.getRaster()->clear(); /* 塗りつぶしクリア */
    return;
  }

  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster()) &&
      !((TRasterFP)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  /* ------ 動作パラメータを得る ---------------------------- */
  bool act_sw[4];
  double in_min_sft[4];
  double in_max_sft[4];
  double out_min[4];
  double out_max[4];
  double gamma[4];

  act_sw[0] = act_sw[1] = act_sw[2] = act_sw[3] = true;
  in_min_sft[0] = in_min_sft[1] = in_min_sft[2] = in_min_sft[3] =
      this->m_in_min_shift->getValue(frame) / ino::param_range();
  in_max_sft[0] = in_max_sft[1] = in_max_sft[2] = in_max_sft[3] =
      this->m_in_max_shift->getValue(frame) / ino::param_range();
  out_min[0] = out_min[1] = out_min[2] = out_min[3] =
      this->m_out_min->getValue(frame) / ino::param_range();
  out_max[0] = out_max[1] = out_max[2] = out_max[3] =
      this->m_out_max->getValue(frame) / ino::param_range();
  gamma[0] = gamma[1] = gamma[2] = gamma[3] =
      static_cast<double>(this->m_gamma->getValue(frame) / ino::param_range());
  /* ------ 画像生成 ---------------------------------------- */
  this->m_input->compute(tile, frame, rend_sets);
  /*--- カメラの範囲をノイズを掛ける範囲としておく(余白含ず) -*/
  int camera_x = 0;
  int camera_y = 0;
  int camera_w = static_cast<int>(rend_sets.m_cameraBox.getLx() + 0.5);
  int camera_h = static_cast<int>(rend_sets.m_cameraBox.getLy() + 0.5);
  /*--- カメラ範囲外へのmargin付き(たぶん)はノイズ範囲から外す -*/
  const int margin_w = tile.getRaster()->getLx() - camera_w;
  const int margin_h = tile.getRaster()->getLy() - camera_h;
  if ((0 <= margin_h && 0 < margin_w)    /* 横方向のみ余白あり */
      || (0 < margin_h && 0 <= margin_w) /* 縦方向のみ余白あり */
      || (0 < margin_h && 0 < margin_w)  /* 縦横両方に余白あり */
  ) {
    /*camera_x = static_cast<int>(ceil((double)margin_w / 2.));
    camera_y = static_cast<int>(ceil((double)margin_h / 2.));*/
    camera_x = margin_w / 2;
    camera_y = margin_h / 2;
    camera_w = rend_sets.m_cameraBox.getLx();
    camera_h = rend_sets.m_cameraBox.getLy();
  }
  /*--- 入力画像がカメラより一部でも小さい
          (ノイズ範囲指定対応できない-->懸案事項) ------------*/
  else {
    camera_w = tile.getRaster()->getLx();
    camera_h = tile.getRaster()->getLy();
  }
  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params  act true"
       << "  in_min_shift " << in_min_sft[0] << "  in_max_shift "
       << in_max_sft[0] << "  out_min " << out_min[0] << "  out_max "
       << out_max[0] << "  gamma " << gamma[0] << "  frame " << frame
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "  tile.m_pos "
       << tile.m_pos << "  tile_getLx " << tile.getRaster()->getLx() << "  y "
       << tile.getRaster()->getLy() << "  rend_sets.m_cameraBox "
       << rend_sets.m_cameraBox << "  rend_sets.m_affine " << rend_sets.m_affine
       << "  camera x " << camera_x << "  y " << camera_y << "  w " << camera_w
       << "  h " << camera_h;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    fx_(tile.getRaster(), act_sw, in_min_sft, in_max_sft, out_min, out_max,
        gamma, camera_x, camera_y, camera_w, camera_h);
    tile.getRaster()->unlock();
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::bad_alloc &e) {
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("std::bad_alloc <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (std::exception &e) {
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("exception <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (...) {
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}
