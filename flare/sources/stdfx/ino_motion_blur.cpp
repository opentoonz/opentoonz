#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"
#include "tfxattributes.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_motion_blur final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_motion_blur)
  TRasterFxPort m_input;

  TIntEnumParamP m_depend_move;
  TDoubleParamP m_x1;
  TDoubleParamP m_y1;
  TDoubleParamP m_x2;
  TDoubleParamP m_y2;
  TDoubleParamP m_scale;
  TDoubleParamP m_curve;
  TDoubleParamP m_zanzo_length;
  TDoubleParamP m_zanzo_power;
  TBoolParamP m_alpha_rendering;
  // TBoolParamP m_spread;
public:
  ino_motion_blur()
      : m_depend_move(new TIntEnumParam(0, "P1 -> P2"))
      , m_x1(0.0)
      , m_y1(0.0)
      , m_x2(1.0)
      , m_y2(1.0)
      , m_scale(1.0 * ino::param_range())
      , m_curve(1.0 * ino::param_range())
      , m_zanzo_length(0.0)
      , m_zanzo_power(1.0 * ino::param_range())
      , m_alpha_rendering(true)
  // , m_spread(true)
  {
    this->m_x1->setMeasureName("fxLength");
    this->m_y1->setMeasureName("fxLength");
    this->m_x2->setMeasureName("fxLength");
    this->m_y2->setMeasureName("fxLength");
    this->m_zanzo_length->setMeasureName("fxLength");

    addInputPort("Source", this->m_input);

    bindParam(this, "depend_move", this->m_depend_move);
    bindParam(this, "x1", this->m_x1);
    bindParam(this, "y1", this->m_y1);
    bindParam(this, "x2", this->m_x2);
    bindParam(this, "y2", this->m_y2);
    bindParam(this, "scale", this->m_scale);
    bindParam(this, "curve", this->m_curve);
    bindParam(this, "zanzo_length", this->m_zanzo_length);
    bindParam(this, "zanzo_power", this->m_zanzo_power);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);
    // bindParam(this,"spread", this->m_spread);

    this->m_depend_move->addItem(1, "Motion");

    this->m_curve->setValueRange(0.1 * ino::param_range(),
                                 10.0 * ino::param_range()); /* gamma値 */
    this->m_zanzo_length->setValueRange(0.0, 1000.0);
    this->m_zanzo_power->setValueRange(0.0 * ino::param_range(),
                                       1.0 * ino::param_range());

    this->getAttributes()->setIsSpeedAware(true);
  }
  //------------------------------------------------------------
  void get_render_vector(const double frame, const TAffine affine,
                         TPointD &rend_vect) {
    /*--- ベクトルを得て、render用単位にする ---*/
    if (0 == this->m_depend_move->getValue()) {
      rend_vect.x = this->m_x2->getValue(frame) - this->m_x1->getValue(frame);
      rend_vect.y = this->m_y2->getValue(frame) - this->m_y1->getValue(frame);

      rend_vect = rend_vect * ino::pixel_per_mm();
      /* 単位変換(mm --> render_pixel) */
    } else {
      rend_vect = -this->getAttributes()->getSpeed();
    }

    /*--- 回転と拡大縮小のGeometryを反映させる ---*/
    /*
    toonz/main/sources/stdfx/motionblurfx.cpp
    750-768行を参照して書いた
    */
    TAffine aff(affine);
    aff.a13 = aff.a23 = 0; /* 移動変換をしないで... */

    rend_vect = aff * rend_vect;

    /*--- ユーザー指定による、強弱を付加する ---*/
    rend_vect =
        rend_vect * (this->m_scale->getValue(frame) / ino::param_range());
    /*--- shrinkはGeometry(affine)に含まれる...らしい ---*/
    // rend_vect = rend_vect
    //	* (1.0 / ((info.m_shrinkX + info.m_shrinkY)/2.));
  }
  int get_render_margin(const double frame, const TAffine affine) {
    TPointD rend_vect;
    this->get_render_vector(frame, affine, rend_vect);
    return static_cast<int>(ceil((fabs(rend_vect.x) < fabs(rend_vect.y))
                                     ? fabs(rend_vect.y)
                                     : fabs(rend_vect.x)) +
                            0.5);
  }
  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox) {
    const int margin = this->get_render_margin(frame, affine);
    if (0 < margin) {
      bBox = bBox.enlarge(static_cast<double>(margin));
    }
  }
  //------------------------------------------------------------
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (false == this->m_input.isConnected()) {
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
    if (0 == this->m_depend_move->getValue()) {
      return true;
    } else {
      return isAlmostIsotropic(info.m_affine) || m_scale->getValue(frame) == 0;
    }
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_motion_blur, "inoMotionBlurFx");
//------------------------------------------------------------
#include "igs_motion_blur.h"
namespace {
template <class U, class T>
void arr_to_ras_(const U *arr, const int channels, TRasterPT<T> ras,
                 const int margin) {
  arr +=
      (ras->getLx() + margin + margin) * margin * channels + margin * channels;

  for (int yy = 0; yy < ras->getLy();
       ++yy, arr += (ras->getLx() + margin + margin) * channels) {
    const U *arrx = arr;
    T *ras_sl     = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arrx += channels) {
      ras_sl[xx].b = arrx[0];
      ras_sl[xx].g = arrx[1];
      ras_sl[xx].r = arrx[2];
      ras_sl[xx].m = arrx[3];
    }
  }
}
void vec_to_ras_(unsigned char *vec, const int channels, TRasterP ras,
                 const int margin) {
  if ((TRaster32P)ras) {
    arr_to_ras_<unsigned char, TPixel32>(vec, channels, ras, margin);
  } else if ((TRaster64P)ras) {
    arr_to_ras_<unsigned short, TPixel64>(
        reinterpret_cast<unsigned short *>(vec), channels, ras, margin);
  }
}
void fx_(const TRasterP in_ras  // with margin
         ,
         const int margin, TRasterP out_ras  // no margin
         ,
         const double x_vector, const double y_vector, const double scale,
         const double curve, const int zanzo_length, const double zanzo_power,
         const bool alp_rend_sw) {
  /******
  std::vector<unsigned char> in_vec;
  ino::ras_to_vec( in_ras, ino::channels(), in_vec );
  std::vector<unsigned char> out_vec(in_vec.size());
******/
  /***	int out_size = in_ras->getLy()*in_ras->getLx()*ino::channels();
  if ((TRaster64P)in_ras) { out_size *= sizeof(unsigned short) ; }
  TRasterGR8P out_byt(out_size, 1);
  //TRasterGR8P out_byt; out_byt->create(out_size, 1);
  //TRasterP out_byt = TRasterGR8P(out_size, 1);
  out_byt->lock();
  if (0 == out_byt->getRawData()) {
   throw TRopException("Can not get TRasterGR8P->getRawData()");
  }***/

  TRasterGR8P out_gr8(in_ras->getLy(),
                      in_ras->getLx() * ino::channels() *
                          ((TRaster64P)in_ras ? sizeof(unsigned short)
                                              : sizeof(unsigned char)));
  out_gr8->lock();

  igs::motion_blur::convert(
      in_ras->getRawData()  // BGRA
      //&in_vec.at(0) // RGBA

      //, out_byt->getRawData() // BGRA
      //, &out_vec.at(0)    // RGBA
      //, out_ras->getRawData() // BGRA
      ,
      out_gr8->getRawData()

          ,
      in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(in_ras)

                                                             ,
      x_vector, y_vector, scale, curve, zanzo_length, zanzo_power, alp_rend_sw);

  /******
  ino::vec_to_ras( out_vec, ino::channels(), out_ras, margin );
  in_vec.clear();
******/
  /***vec_to_ras_(out_byt->getRawData(),ino::channels(),out_ras,margin);
  out_byt->unlock();***/

  ino::arr_to_ras(out_gr8->getRawData(), ino::channels(), out_ras, margin);
  out_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_motion_blur::doCompute(TTile &tile, double frame,
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

  /* ------ Render(Pixel)単位で動作パラメータを得る --------- */
  const int depend_move  = this->m_depend_move->getValue();
  const double scale     = this->m_scale->getValue(frame) / ino::param_range();
  const double curve     = this->m_curve->getValue(frame) / ino::param_range();
  const int zanzo_length = this->m_zanzo_length->getValue(frame) *
                           ino::pixel_per_mm() *
                           sqrt(fabs(rend_sets.m_affine.det())) /
                           ((rend_sets.m_shrinkX + rend_sets.m_shrinkY) / 2.0);
  const double zanzo_power =
      this->m_zanzo_power->getValue(frame) / ino::param_range();
  const bool alp_rend_sw = this->m_alpha_rendering->getValue();

  /* ------ 実際に処理するぼかしの方向vectorを生成 ---------- */
  TPointD rend_vect;
  this->get_render_vector(frame, rend_sets.m_affine, rend_vect);
  /* ------ 参照マージンの大きさを得る ---------------------- */
  const int margin = static_cast<int>(
      ceil((fabs(rend_vect.x) < fabs(rend_vect.y)) ? fabs(rend_vect.y)
                                                   : fabs(rend_vect.x)) +
      0.5);
  /* ------ 参照マージン含めた画像を生成 -------------------- */
  TRectD bBox =
      TRectD(tile.m_pos /* Render画像上(Pixel単位)の位置 */
             ,
             TDimensionD(/* Render画像上(Pixel単位)のサイズ */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  if (0 < margin) {
    bBox = bBox.enlarge(static_cast<double>(margin));
  }

  TTile enlarge_tile;
  this->m_input->allocateAndCompute(
      enlarge_tile, bBox.getP00(),
      TDimensionI(/* Pixel単位のdouble -> intにしてセット */
                  static_cast<int>(bBox.getLx() + 0.5),
                  static_cast<int>(bBox.getLy() + 0.5)),
      tile.getRaster(), frame, rend_sets);

  /* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
  tile.getRaster()->clear(); /* 塗りつぶしクリア */

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  depend_move_num " << depend_move << "  render_vector x "
       << rend_vect.x << "  y " << rend_vect.y << "  margin " << margin
       << "  scale " << scale << "  curve " << curve << "  z_len "
       << zanzo_length << "  z_pow " << zanzo_power << "  alp_rend_sw "
       << alp_rend_sw << "   tile w " << tile.getRaster()->getLx() << "  h "
       << tile.getRaster()->getLy() << "  pixbits "
       << ino::pixel_bits(tile.getRaster()) << "   frame " << frame
       << "   rand_sets affine_det " << rend_sets.m_affine.det() << "  a11 "
       << rend_sets.m_affine.a11 << "  a12 " << rend_sets.m_affine.a12
       << "  a13 " << rend_sets.m_affine.a13 << "  a21 "
       << rend_sets.m_affine.a21 << "  a22 " << rend_sets.m_affine.a22
       << "  a23 " << rend_sets.m_affine.a23 << "  x " << rend_sets.m_shrinkX
       << "  y " << rend_sets.m_shrinkY;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    fx_(enlarge_tile.getRaster()  // in with margin
        ,
        margin  // margin
        ,
        tile.getRaster()  // out with no margin
        ,
        rend_vect.x, rend_vect.y, scale, curve, zanzo_length, zanzo_power,
        alp_rend_sw);
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
