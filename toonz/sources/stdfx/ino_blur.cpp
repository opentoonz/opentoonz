#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"
#include "tfxattributes.h"

#include "ino_common.h"
#include "igs_gaussian_blur.h"
//------------------------------------------------------------
class ino_blur final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_blur)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TDoubleParamP m_radius;
  TIntEnumParamP m_ref_mode;

public:
  ino_blur() : m_radius(1.0), m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "radius", this->m_radius);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_radius->setMeasureName("fxLength");
    this->m_radius->setValueRange(0.0, 1000.0);

    this->m_ref_mode->addItem(1, "Green");
    this->m_ref_mode->addItem(2, "Blue");
    this->m_ref_mode->addItem(3, "Alpha");
    this->m_ref_mode->addItem(4, "Luminance");
    this->m_ref_mode->addItem(-1, "Nothing");
  }
  //------------------------------------------------------------
  double get_render_real_radius(const double frame, const TAffine affine) {
    /*--- ベクトルにする --- */
    TPointD rend_vect;
    rend_vect.x = this->m_radius->getValue(frame);
    rend_vect.y = 0.0;
    /*--- render用単位にする ---*/
    rend_vect = rend_vect * ino::pixel_per_mm();
    /* 単位変換(mm --> render_pixel) */
    /*--- 回転と拡大縮小のGeometryを反映させる ---*/
    /* 以下は、
toonz/main/sources/stdfx/motionblurfx.cpp
586-592行を参照して書いた
*/
    TAffine aff(affine);
    aff.a13 = aff.a23 = 0; /* 移動変換をしないで... */
    rend_vect         = aff * rend_vect;
    /*--- 方向は無視して長さを返す ---*/
    return sqrt(rend_vect.x * rend_vect.x + rend_vect.y * rend_vect.y);
  }
  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox) {
    const int margin = igs::gaussian_blur_hv::int_radius(
        this->get_render_real_radius(frame, affine));
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
    if (0 == this->m_radius->getValue(frame)) {
      return true;
    } else {
      return isAlmostIsotropic(info.m_affine);
    }
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_blur, "inoBlurFx");
//------------------------------------------------------------
namespace {
void fx_(const TRasterP in_ras  // with margin
         ,
         TRasterP out_ras  // no margin
         ,
         const TRasterP refer_ras, const int refer_mode, const int int_radius,
         const double real_radius) {
  TRasterGR8P out_buffer(out_ras->getLy(),
                         out_ras->getLx() * ino::channels() *
                             ((TRaster64P)in_ras ? sizeof(unsigned short)
                                                 : sizeof(unsigned char)));
  const int buffer_bytes = igs::gaussian_blur_hv::buffer_bytes(
      in_ras->getLy(), in_ras->getLx(), int_radius);
  TRasterGR8P cvt_buffer(buffer_bytes, 1);
  out_buffer->lock();
  cvt_buffer->lock();
  igs::gaussian_blur_hv::convert(
      in_ras->getRawData()  // const void *in_with_margin (BGRA)
      ,
      out_buffer->getRawData()  // void *out_no_margin (BGRA)

      ,
      in_ras->getLy()  // const int height_with_margin
      ,
      in_ras->getLx()  // const int width_with_margin
      ,
      ino::channels()  // const int channels
      ,
      ino::bits(in_ras)  // const int bits

      ,
      (((refer_ras != nullptr) && (0 <= refer_mode))
           ? refer_ras->getRawData()
           : nullptr)  // BGRA // const unsigned char *ref
      ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? ino::bits(refer_ras)
                                                     : 0)  // const int ref_bits
      ,
      refer_mode  // const int refer_mode

      ,
      cvt_buffer->getRawData()  // void *buffer
      ,
      buffer_bytes  // int buffer_bytes

      ,
      int_radius  // const int int_radius
      ,
      real_radius  // const double real_radius // , 0.25
      );
  ino::arr_to_ras(out_buffer->getRawData(), ino::channels(), out_ras, 0);
  cvt_buffer->unlock();
  out_buffer->unlock();
}
}
//------------------------------------------------------------
void ino_blur::doCompute(TTile &tile, double frame,
                         const TRenderSettings &rend_sets) {
  /*------ 接続していなければ処理しない ----------------------*/
  if (!this->m_input.isConnected()) {
    tile.getRaster()->clear(); /* 塗りつぶしクリア */
    return;
  }
  /*------ サポートしていないPixelタイプはエラーを投げる -----*/
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }
  /*------ ボケ足長さの実際の長さをえる ----------------------*/
  const double real_radius =
      this->get_render_real_radius(frame, rend_sets.m_affine);
  const int int_radius = igs::gaussian_blur_hv::int_radius(real_radius);
  /*------ ボケ足長さがゼロのときはblur処理しない ------------*/
  if (0 == int_radius) {
    this->m_input->compute(tile, frame, rend_sets);
    return;
  }
  /*------- 動作パラメータを得る -----------------------------*/
  const int refer_mode = this->m_ref_mode->getValue();

  /*------ 表示の範囲を得る ----------------------------------*/
  TRectD bBox =
      TRectD(tile.m_pos /* Render画像上(Pixel単位)の位置 */
             ,
             TDimensionD(/* Render画像上(Pixel単位)のサイズ */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  /*------ ぼけ半径(=マージン)分表示範囲を広げる -------------*/
  if (0 < int_radius) {
    bBox = bBox.enlarge(static_cast<double>(int_radius));
  }

  /* ------ margin付き画像生成 ------------------------------ */
  /*
void allocateAndCompute(
TTile &tile
,const TPointD &pos
,const TDimension &size // TDimension = TDimensionI = TDimensionT<int>
,TRasterP templateRas
,double frame
,const TRenderSettings &info
);
*/
  TTile enlarge_tile;
  this->m_input->allocateAndCompute(
      enlarge_tile, bBox.getP00(),
      TDimensionI(/* Pixel単位に四捨五入 */
                  static_cast<int>(bBox.getLx() + 0.5),
                  static_cast<int>(bBox.getLy() + 0.5)),
      tile.getRaster(), frame, rend_sets);

  /*------ 参照画像生成 --------------------------------------*/
  TTile refer_tile;
  bool refer_sw = false;
  if (this->m_refer.isConnected()) {
    refer_sw = true;
    this->m_refer->allocateAndCompute(
        refer_tile, tile.m_pos /* TPointD */
        ,
        tile.getRaster()->getSize() /* Pixel単位 */
        ,
        tile.getRaster(), frame, rend_sets);
  }

  /* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
  tile.getRaster()->clear(); /* 塗りつぶしクリア */

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  usr_radius " << this->m_radius->getValue(frame) << "  real_radius "
       << real_radius << "  int_radius " << int_radius << "  refer_mode "
       << refer_mode << "  tile"
       << " pos " << tile.m_pos << " w " << tile.getRaster()->getLx() << " h "
       << tile.getRaster()->getLy() << "  enl_tile"
       << " w " << enlarge_tile.getRaster()->getLx() << " h "
       << enlarge_tile.getRaster()->getLy() << "  pixbits "
       << ino::pixel_bits(tile.getRaster()) << "  frame " << frame
       << "  affine a11 " << rend_sets.m_affine.a11 << "  a12 "
       << rend_sets.m_affine.a12 << "  a21 " << rend_sets.m_affine.a21
       << "  a22 " << rend_sets.m_affine.a22;
    if (refer_sw) {
      os << "  refer_tile"
         << " pos " << refer_tile.m_pos << " x "
         << refer_tile.getRaster()->getLx() << " y "
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
    fx_(enlarge_tile.getRaster()  // in with margin
        ,
        tile.getRaster()  // out with no margin

        ,
        refer_tile.getRaster(), refer_mode

        ,
        int_radius  // margin
        ,
        real_radius);
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
