#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "tparamset.h"  // TPointParamP
#include "stdfx.h"
#include "tparamuiconcept.h"  //add 20140130

#include "ino_common.h"
#include "igs_radial_blur.h"

//------------------------------------------------------------
namespace {
const double ino_param_range = 100.0;
}
class ino_radial_blur final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_radial_blur)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TPointParamP m_center;
  TDoubleParamP m_blur;
  TDoubleParamP m_radius;
  TDoubleParamP m_twist;
  // TDoubleParamP m_twist_radius;
  TBoolParamP m_alpha_rendering;
  TBoolParamP m_anti_alias;
  TIntEnumParamP m_ref_mode;

public:
  ino_radial_blur()
      : m_center(TPointD(0.0, 0.0))
      , m_blur(0.01 * ino_param_range)
      , m_radius(0.0)
      , m_twist(0.0)
      // ,m_twist_radius(0.0)
      , m_alpha_rendering(true)
      , m_anti_alias(false)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    this->m_center->getX()->setMeasureName("fxLength");
    this->m_center->getY()->setMeasureName("fxLength");
    this->m_radius->setMeasureName("fxLength");

    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "center", this->m_center);
    bindParam(this, "blur", this->m_blur);
    bindParam(this, "radius", this->m_radius);
    bindParam(this, "twist", this->m_twist);
    // bindParam(this,"twist_radius", this->m_twist_radius);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);
    bindParam(this, "anti_alias", this->m_anti_alias);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_radius->setValueRange(0, (std::numeric_limits<double>::max)());
    /* 拡大のしすぎを防ぐためにMaxを制限する */
    this->m_blur->setValueRange(0.0 * ino_param_range, 1.0 * ino_param_range);
    this->m_twist->setValueRange(-180.0, 180.0);
    // this->m_twist_radius->setValueRange(0.0,1000.0);
    this->m_ref_mode->addItem(1, "Green");
    this->m_ref_mode->addItem(2, "Blue");
    this->m_ref_mode->addItem(3, "Alpha");
    this->m_ref_mode->addItem(4, "Luminance");
    this->m_ref_mode->addItem(-1, "Nothing");
  }
  //------------------------------------------------------------
  TPointD get_render_center(const double frame, const TPointD &pos,
                            const TAffine affine) {
    /*--- 表示画像の中心をゼロとしたミリメートル単位の位置 ---*/
    TPointD center = this->m_center->getValue(frame);
    /*--- 単位変換(mm --> render用pixel) ---*/
    center = center * ino::pixel_per_mm();
    /*--- Affine変換(Geometry&Shrink&原点を中心から左下へ) ---*/
    center = affine * center;
    /*--- カメラ座標をrender画像座標の原点に ---*/
    return center - pos;
  }
  int get_render_int_margin(const double frame, const TRectD &bBox,
                            const TAffine affine, TPointD center) {
    /*--- 単位変換(mm-->render用pixel)と長さ(scale)変換 ---*/
    const double scale = ino::pixel_per_mm() * sqrt(fabs(affine.det()));
    /*--- margin計算...Twist時正確でない ---*/
    return igs::radial_blur::reference_margin(
        static_cast<int>(ceil(bBox.getLy())),
        static_cast<int>(ceil(bBox.getLx())), center.x, center.y,
        this->m_twist->getValue(frame) * 3.14159265358979 / 180.0,
        0.0  // this->m_twist_radius->getValue(frame) * scale
        ,
        this->m_blur->getValue(frame) / ino_param_range

        //,this->m_radius->getValue(frame) * scale
        ,
        0 /* debug:2012-02-01:ゼロ以上だとmarginが小さすぎになり画像が切れてしまう
             */

        ,
        (this->m_anti_alias->getValue() ? 4 : 1));
  }
  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox) {
    TPointD center(this->get_render_center(frame, bBox.getP00(), affine));
    int margin = this->get_render_int_margin(frame, bBox, affine, center);
    if (0 < margin) {
      /* 拡大のしすぎを防ぐテキトーな制限 */
      if (4096 < margin) {
        margin = 4096;
      }
      bBox = bBox.enlarge(margin);
    }
  }
  //------------------------------------------------------------
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (!this->m_input.isConnected()) {
      bBox = TRectD();
      return false;
    }
    const bool ret = this->m_input->doGetBBox(frame, bBox, info);
    this->get_render_enlarge(frame, info.m_affine, bBox);
    return ret;
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    TRectD bBox(rect);
    this->get_render_enlarge(frame, info.m_affine, bBox);
    return TRasterFx::memorySize(bBox, info.m_bpp);
  }
  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override {
    rectOnInput = rectOnOutput;
    infoOnInput = infoOnOutput;
    this->get_render_enlarge(frame, infoOnOutput.m_affine, rectOnInput);
  }
  bool canHandle(const TRenderSettings &info, double frame) override {
    // return false; // toonz has geometry control
    // return true;
    /* 2012-11-14:
     * trueのとき、Fx処理がジオメトリ変換の最後にまとめられてしまう。 */
    return m_blur->getValue(frame) == 0 ? true
                                        : isAlmostIsotropic(info.m_affine);
  }
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  // add 20140130
  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 2];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Center";
    concepts[0].m_params.push_back(m_center);

    concepts[1].m_type  = TParamUIConcept::RADIUS;
    concepts[1].m_label = "Radius";
    concepts[1].m_params.push_back(m_radius);
    concepts[1].m_params.push_back(m_center);
  }
  // add 20140130
};
FX_PLUGIN_IDENTIFIER(ino_radial_blur, "inoRadialBlurFx");
//--------------------------------------------------------------------
namespace {
void fx_(const TRasterP in_ras  // with margin
         ,
         const int margin

         /* ここではinとrefは同サイズで使用している */
         ,
         const TRasterP refer_ras  // no margin
         ,
         const int refer_mode

         ,
         TRasterP out_ras  // no margin

         ,
         const double xp, const double yp, const double twist,
         const double twist_radius, const double blur, const double radius,
         const bool alpha_rendering_sw, const bool anti_alias_sw) {
  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();

  igs::radial_blur::convert(
      in_ras->getRawData()  // BGRA
      ,
      0 /* margin機能は使っていない、のでinとref画像は同サイズ */

      ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? refer_ras->getRawData()
                                                     : nullptr)  // BGRA
      ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? ino::bits(refer_ras)
                                                     : 0),
      refer_mode

      ,
      in_gr8->getRawData()  // BGRA

      ,
      in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(out_ras)

                                                             ,
      xp + margin, yp + margin, twist, twist_radius, blur, radius

      ,
      (anti_alias_sw ? 4 : 1), alpha_rendering_sw);

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), out_ras, margin);
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_radial_blur::doCompute(TTile &tile, double frame,
                                const TRenderSettings &ri) {
  /*------ 接続していなければ処理しない ----------------------*/
  if (!this->m_input.isConnected()) {
    tile.getRaster()->clear(); /* 塗りつぶしクリア */
    return;
  }
  /*------ サポートしていないPixelタイプはエラーを投げる -----*/
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }
  /*------ パラメータを得る ----------------------------------*/
  const double scale  = ino::pixel_per_mm() * sqrt(fabs(ri.m_affine.det()));
  const double blur   = this->m_blur->getValue(frame) / ino_param_range;
  const double radius = this->m_radius->getValue(frame) * scale;
  const double twist =
      this->m_twist->getValue(frame) * 3.14159265358979 / 180.0;
  // const double twist_radius = this->m_twist_radius->getValue(frame)*scale;
  const bool alpha_rend_sw = this->m_alpha_rendering->getValue();
  const bool anti_alias_sw = this->m_anti_alias->getValue();
  const int refer_mode     = this->m_ref_mode->getValue();

  TPointD center = this->m_center->getValue(frame);
  TPointD render_center(
      this->get_render_center(frame, tile.m_pos, ri.m_affine));
  /*------ 入力画像の範囲を得る ------------------------------*/
  TRectD tile_with_margin =
      TRectD(tile.m_pos /* Render画像上(Pixel単位)の位置 */
             ,
             TDimensionD(/* Render画像上(Pixel単位)のサイズ */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  /*------ 入力画像のエリアの拡大 ----------------------------*/
  int margin = this->get_render_int_margin(frame, tile_with_margin, ri.m_affine,
                                           render_center);
  if (0 < margin) {
    /* 拡大のしすぎを防ぐテキトーな制限 */
    if (4096 < margin) {
      margin = 4096;
    }
    tile_with_margin = tile_with_margin.enlarge(margin);
  }
  /*------ 入力画像生成 --------------------------------------*/
  TTile enlarge_tile;
  this->m_input->allocateAndCompute(
      enlarge_tile, tile_with_margin.getP00(),
      TDimensionI(/* Pixel単位で四捨五入 */
                  static_cast<int>(tile_with_margin.getLx() + 0.5),
                  static_cast<int>(tile_with_margin.getLy() + 0.5)),
      tile.getRaster(), frame, ri);
  /*------ 参照画像生成 --------------------------------------*/
  TTile refer_tile;
  bool refer_sw = false;
  if (this->m_refer.isConnected()) {
    refer_sw = true;
    this->m_refer->allocateAndCompute(
        refer_tile, tile_with_margin.getP00(),
        TDimensionI(/* Pixel単位で四捨五入 */
                    static_cast<int>(tile_with_margin.getLx() + 0.5),
                    static_cast<int>(tile_with_margin.getLy() + 0.5)),
        tile.getRaster(), frame, ri);
  }
  /* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
  tile.getRaster()->clear(); /* 塗りつぶしクリア */
  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  cx " << center.x << "  cy " << center.y << "  blur " << blur
       << "  radius " << radius << "  twist " << twist
       // << "  twist_radius " << twist_radius
       << "  reference " << refer_mode << "  alpha_rendering " << alpha_rend_sw
       << "  anti_alias " << anti_alias_sw << "  render_center "
       << render_center << "  frame " << frame << "  pixbits "
       << ino::pixel_bits(tile.getRaster()) << "  tile.m_pos " << tile.m_pos
       << "  tile_getLx " << tile.getRaster()->getLx() << "  y "
       << tile.getRaster()->getLy() << "  ri.m_cameraBox " << ri.m_cameraBox
       << "  ri.m_affine " << ri.m_affine << "  ri_m_affine_det "
       << ri.m_affine.det() << "  shrink_x " << ri.m_shrinkX << "  shrink_y "
       << ri.m_shrinkY << "  tile_with_margin " << tile_with_margin
       << "  enlarge_tile.m_pos " << enlarge_tile.m_pos
       << "  tile_allocated_getLx " << enlarge_tile.getRaster()->getLx()
       << "  y " << enlarge_tile.getRaster()->getLy();
    if (refer_sw) {
      os << "  refer_tile.m_pos " << refer_tile.m_pos << "  refer_tile_getLx "
         << refer_tile.getRaster()->getLx() << "  y "
         << refer_tile.getRaster()->getLy();
    }
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    enlarge_tile.getRaster()->lock();
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->lock();
    }
    fx_(enlarge_tile.getRaster(), margin

        ,
        refer_tile.getRaster(), refer_mode

        ,
        tile.getRaster()

            ,
        render_center.x, render_center.y, twist, 0.0  // twist_radius
        ,
        blur, radius, alpha_rend_sw, anti_alias_sw);
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    enlarge_tile.getRaster()->unlock();
    tile.getRaster()->unlock();
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::bad_alloc &e) {
    if (refer_tile.getRaster() != nullptr) {
      refer_tile.getRaster()->unlock();
    }
    enlarge_tile.getRaster()->unlock();
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
    enlarge_tile.getRaster()->unlock();
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
    enlarge_tile.getRaster()->unlock();
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}
