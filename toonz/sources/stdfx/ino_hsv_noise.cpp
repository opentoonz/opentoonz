#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_hsv_noise final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_hsv_noise)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TDoubleParamP m_hue;
  TDoubleParamP m_sat;
  TDoubleParamP m_val;
  TDoubleParamP m_mat;
  TDoubleParamP m_random_seed;
  TDoubleParamP m_near_blur;
  TDoubleParamP m_term_effective;
  TDoubleParamP m_term_center;
  TIntEnumParamP m_term_type;

  TBoolParamP m_anti_alias;
  TIntEnumParamP m_ref_mode;

public:
  ino_hsv_noise()
      : m_hue(0.025 * ino::param_range())
      , m_sat(0.0 * ino::param_range())
      , m_val(0.035 * ino::param_range())
      , m_mat(0.0 * ino::param_range())
      , m_random_seed(1)
      , m_near_blur(1.0 * ino::param_range())
      , m_term_effective(0.0 * ino::param_range())
      , m_term_center(ino::param_range() / 2.0)
      , m_term_type(new TIntEnumParam(0, "Keep Noise"))

      , m_anti_alias(true)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "hue", this->m_hue);
    bindParam(this, "saturation", this->m_sat);
    bindParam(this, "value", this->m_val);
    bindParam(this, "alpha", this->m_mat);
    bindParam(this, "seed", this->m_random_seed);
    bindParam(this, "nblur", this->m_near_blur);
    bindParam(this, "effective", this->m_term_effective);
    bindParam(this, "center", this->m_term_center);
    bindParam(this, "type", this->m_term_type);

    bindParam(this, "anti_alias", this->m_anti_alias);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_hue->setValueRange(0.0 * ino::param_range(),
                               1.0 * ino::param_range());
    this->m_sat->setValueRange(0.0 * ino::param_range(),
                               1.0 * ino::param_range());
    this->m_val->setValueRange(0.0 * ino::param_range(),
                               1.0 * ino::param_range());
    this->m_mat->setValueRange(0.0 * ino::param_range(),
                               1.0 * ino::param_range());

    this->m_random_seed->setValueRange(
        0, std::numeric_limits<unsigned long>::max());
    this->m_near_blur->setValueRange(0.0 * ino::param_range(),
                                     1.0 * ino::param_range());
    this->m_term_effective->setValueRange(0.0 * ino::param_range(),
                                          1.0 * ino::param_range());
    this->m_term_center->setValueRange(0.0 * ino::param_range(),
                                       1.0 * ino::param_range());
    this->m_term_type->addItem(1, "Keep Contrast");

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
    // return true;
    /* trueだと素材がスライドして現れるときノイズパターンが
    変わってしまう 2013-4-5からtoonz上で変更した */
    /* でなく、
    ここでの指定にかかわらず、
    素材をカメラ範囲で切り取ってるため、
    スライドで現れるときのノイズパターンは変わってしまう。
    だたし、移動以外のGeometryがなく、
    連番の絵でなく一枚の絵を移動するだけのときは、
    Cashのため(?)ノイズパターンは変化しない。
    回転にノイズがついてくる。
    防ぐ方法はtileに素材の位置と大きさ情報があれば...
    2013-11-08 */
    return false;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_hsv_noise, "inohsvNoiseFx");
//------------------------------------------------------------
#include "igs_hsv_noise.h"
namespace {
void fx_(TRasterP in_ras, const TRasterP refer_ras, const int refer_mode

         ,
         double hue_range, double sat_range, double val_range, double alp_range,
         unsigned long random_seed, double near_blur, double effective,
         double center, int type, const int camera_x, const int camera_y,
         const int camera_w, const int camera_h, const bool anti_alias_sw) {
  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  /* igs::hsv_noise::change(-)は今後つかわない2011-07-15 */
  igs::hsv_noise::change(
      // in_ras->getRawData() // BGRA
      in_gr8->getRawData()

          ,
      in_ras->getLy(), in_ras->getLx()  // Must Not use in_ras->getWrap()
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
      camera_x, camera_y, camera_w, camera_h

      ,
      hue_range, sat_range, val_range, alp_range, random_seed, near_blur,
      effective, center, type, effective, center, type, effective, center, type,
      anti_alias_sw);

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_hsv_noise::doCompute(TTile &tile, double frame,
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
  const double hue_range = this->m_hue->getValue(frame) / ino::param_range();
  const double sat_range = this->m_sat->getValue(frame) / ino::param_range();
  const double val_range = this->m_val->getValue(frame) / ino::param_range();
  const double mat_range = this->m_mat->getValue(frame) / ino::param_range();
  const unsigned long random_seed =
      static_cast<unsigned long>(this->m_random_seed->getValue(frame));
  const double near_blur =
      this->m_near_blur->getValue(frame) / ino::param_range() / 2.0;
  int term_type = -1;
  switch (this->m_term_type->getValue()) {
  case 0:
    term_type = 0;
    break;
  case 1:
    term_type = 3;
    break;
  }
  const double term_center =
      this->m_term_center->getValue(frame) / ino::param_range();
  const double term_effective =
      this->m_term_effective->getValue(frame) / ino::param_range();
  const bool anti_alias_sw = this->m_anti_alias->getValue();
  const int refer_mode     = this->m_ref_mode->getValue();

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
       << "  h " << hue_range << "  s " << sat_range << "  v " << val_range
       << "  a " << mat_range << "  seed " << random_seed << "  nblur "
       << near_blur << "  effective " << term_effective << "  center "
       << term_center << "  type " << term_type << "  frame " << frame
       << "  anti_alias " << anti_alias_sw << "  reference " << refer_mode
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "  tile.m_pos "
       << tile.m_pos << "  tile_getLx " << tile.getRaster()->getLx() << "  y "
       << tile.getRaster()->getLy() << "  rend_sets.m_cameraBox "
       << rend_sets.m_cameraBox << "  rend_sets.m_affine " << rend_sets.m_affine
       << "  camera x " << camera_x << "  y " << camera_y << "  w " << camera_w
       << "  h " << camera_h;
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
    fx_(tile.getRaster(), refer_tile.getRaster(), refer_mode

        ,
        hue_range, sat_range, val_range, mat_range, random_seed, near_blur,
        term_effective, term_center, term_type, camera_x, camera_y, camera_w,
        camera_h, anti_alias_sw  // --> add_blend_sw, default is true
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
