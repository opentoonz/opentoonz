#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
#include "igs_warp.h"
//------------------------------------------------------------
class ino_warp_hv final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_warp_hv)

  TRasterFxPort m_input;
  TRasterFxPort m_hori;
  TRasterFxPort m_vert;

  TDoubleParamP m_h_maxlen;
  TDoubleParamP m_v_maxlen;
  TIntEnumParamP m_h_ref_mode;
  TIntEnumParamP m_v_ref_mode;
  TBoolParamP m_alpha_rendering;
  TBoolParamP m_anti_aliasing;

public:
  ino_warp_hv()
      : m_h_maxlen(0.0 * ino::param_range())
      , m_v_maxlen(0.0 * ino::param_range())
      , m_h_ref_mode(new TIntEnumParam(2, "Red"))
      , m_v_ref_mode(new TIntEnumParam(2, "Red"))
      , m_alpha_rendering(true)
      , m_anti_aliasing(true) {
    this->m_h_maxlen->setMeasureName("fxLength");
    this->m_v_maxlen->setMeasureName("fxLength");

    addInputPort("Source", this->m_input);
    addInputPort("Hori", this->m_hori);
    addInputPort("Vert", this->m_vert);

    bindParam(this, "h_maxlen", this->m_h_maxlen);
    bindParam(this, "v_maxlen", this->m_v_maxlen);
    bindParam(this, "h_ref_mode", this->m_h_ref_mode);
    bindParam(this, "v_ref_mode", this->m_v_ref_mode);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);
    bindParam(this, "anti_aliasing", this->m_anti_aliasing);

    this->m_h_maxlen->setValueRange(0.0 * ino::param_range(),
                                    100.0 * ino::param_range());
    this->m_v_maxlen->setValueRange(0.0 * ino::param_range(),
                                    100.0 * ino::param_range());
    this->m_h_ref_mode->addItem(1, "Green");
    this->m_h_ref_mode->addItem(0, "Blue");
    this->m_h_ref_mode->addItem(3, "Alpha");
    this->m_v_ref_mode->addItem(1, "Green");
    this->m_v_ref_mode->addItem(0, "Blue");
    this->m_v_ref_mode->addItem(3, "Alpha");
  }
  void get_render_real_hv(const double frame, const TAffine affine,
                          double &h_maxlen, double &v_maxlen) {
    /*--- ベクトルにする(プラス値) --- */
    TPointD rend_vect;
    rend_vect.x = this->m_h_maxlen->getValue(frame);
    rend_vect.y = this->m_v_maxlen->getValue(frame);
    /*--- 単位変換(mm --> render_pixel)render用単位にする ---*/
    rend_vect = rend_vect * ino::pixel_per_mm();
    /*--- 拡大縮小(移動回転しないで)のGeometryを反映させる ---*/
    rend_vect = rend_vect * sqrt(fabs(affine.det()));
    /*--- 方向は無視して長さを返す(プラス値) ---*/
    h_maxlen = rend_vect.x;
    v_maxlen = rend_vect.y;
  }
  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox) {
    double h_maxlen = 0.0;
    double v_maxlen = 0.0;
    this->get_render_real_hv(frame, affine, h_maxlen, v_maxlen);
    const int margin =
        static_cast<int>(ceil((h_maxlen < v_maxlen) ? v_maxlen : h_maxlen));
    if (0 < margin) {
      bBox = bBox.enlarge(static_cast<double>(margin));
    }
  }
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
    // return true;
    return false;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_warp_hv, "inoWarphvFx");
//--------------------------------------------------------------------
namespace {
template <class T>
void data_set_template_(const TRasterPT<T> in_ras  // with margin
                        ,
                        const int margin, TRasterPT<T> out_ras  // no margin
                        ) {
  for (int yy = 0; yy < out_ras->getLy(); ++yy) {
    const T *in_ras_sl = in_ras->pixels(yy + margin);
    T *out_ras_sl      = out_ras->pixels(yy);
    for (int xx = 0; xx < out_ras->getLx(); ++xx) {
      out_ras_sl[xx].r = in_ras_sl[xx + margin].r;
      out_ras_sl[xx].g = in_ras_sl[xx + margin].g;
      out_ras_sl[xx].b = in_ras_sl[xx + margin].b;
      out_ras_sl[xx].m = in_ras_sl[xx + margin].m;
    }
  }
}
void fx_(TRasterP in_ras  // with margin
         ,
         const TRasterP hori_ras  // with margin
         ,
         const TRasterP vert_ras  // with margin
         ,
         const int margin, TRasterP out_ras  // no margin

         ,
         const double h_maxlen, const double v_maxlen, const int h_ref_mode,
         const int v_ref_mode, const bool alpha_rendering_sw,
         const bool anti_aliasing_sw) {
  if (0 != hori_ras) {
    const int bits = ino::bits(hori_ras);
    igs::warp::hori_change(in_ras->getRawData()  // BGRA
                           ,
                           in_ras->getLy(), in_ras->getLx(), ino::channels(),
                           ino::bits(in_ras)

                               ,
                           hori_ras->getRawData()  // BGRA
                           ,
                           ino::channels()
                           //, 2 // order is "bgra", then r is 2.
                           ,
                           h_ref_mode, bits

                           ,
                           (double)(1 << (bits - 1)) / ((1 << bits) - 1)
                           // , h_maxlen
                           ,
                           -h_maxlen /* 移動方向と参照方向は逆 */
                               * (double)((1 << bits) - 1) / (1 << (bits - 1)),
                           alpha_rendering_sw, anti_aliasing_sw);
  }
  if (0 != vert_ras) {
    const int bits = ino::bits(vert_ras);
    igs::warp::vert_change(in_ras->getRawData()  // BGRA
                           ,
                           in_ras->getLy(), in_ras->getLx(), ino::channels(),
                           ino::bits(in_ras)

                               ,
                           vert_ras->getRawData()  // BGRA
                           ,
                           ino::channels()
                           //, 2 // order is "bgra", then r is 2.
                           ,
                           v_ref_mode, bits

                           ,
                           (double)(1 << (bits - 1)) / ((1 << bits) - 1)
                           // , v_maxlen
                           ,
                           -v_maxlen /* 移動方向と参照方向は逆 */
                               * (double)((1 << bits) - 1) / (1 << (bits - 1)),
                           alpha_rendering_sw, anti_aliasing_sw);
  }

  if ((TRaster32P)in_ras) {
    data_set_template_<TPixel32>(in_ras, margin, out_ras);
  } else if ((TRaster64P)in_ras) {
    data_set_template_<TPixel64>(in_ras, margin, out_ras);
  }
}
}
//------------------------------------------------------------
void ino_warp_hv::doCompute(TTile &tile, double frame,
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

  /*------ 動作パラメータを得る ------------------------------*/
  const bool alp_rend_sw = this->m_alpha_rendering->getValue();
  const bool anti_ali_sw = this->m_anti_aliasing->getValue();
  double h_maxlen        = 0;
  double v_maxlen        = 0;
  this->get_render_real_hv(frame, rend_sets.m_affine, h_maxlen, v_maxlen);
  /*------ 表示の範囲を得る ----------------------------------*/
  TRectD bBox =
      TRectD(tile.m_pos /* Render画像上(Pixel単位)の位置 */
             ,
             TDimensionD(/* Render画像上(Pixel単位)のサイズ */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  /*------ 歪み量分(=マージン)分表示範囲を広げる -------------*/
  const int margin =
      static_cast<int>(ceil((h_maxlen < v_maxlen) ? v_maxlen : h_maxlen));
  if (0 < margin) {
    bBox = bBox.enlarge(static_cast<double>(margin));
  }
  /*------ 広げた範囲の画像生成 ------------------------------*/
  TTile enlarge_tile;
  this->m_input->allocateAndCompute(
      enlarge_tile, bBox.getP00(),
      TDimensionI(/* Pixel単位のdouble -> intにしてセット */
                  static_cast<int>(bBox.getLx() + 0.5),
                  static_cast<int>(bBox.getLy() + 0.5)),
      tile.getRaster(), frame, rend_sets);
  /*------ 参照画像生成 --------------------------------------*/
  TTile hori_tile;
  TTile vert_tile;
  const bool hori_cn_is = this->m_hori.isConnected();
  const bool vert_cn_is = this->m_vert.isConnected();
  if (hori_cn_is) {
    this->m_hori->allocateAndCompute(
        hori_tile, bBox.getP00(),
        TDimensionI(/* Pixel単位のdouble -> intにしてセット */
                    (int)(bBox.getLx() + 0.5), (int)(bBox.getLy() + 0.5)),
        tile.getRaster(), frame, rend_sets);
  }
  if (vert_cn_is) {
    this->m_vert->allocateAndCompute(
        vert_tile, bBox.getP00(),
        TDimensionI(/* Pixel単位のdouble -> intにしてセット */
                    (int)(bBox.getLx() + 0.5), (int)(bBox.getLy() + 0.5)),
        tile.getRaster(), frame, rend_sets);
  }
  /*------ 保存すべき画像メモリを塗りつぶしクリア ------------*/
  tile.getRaster()->clear(); /* 塗りつぶしクリア */

  /*------ (app_begin)log記憶 --------------------------------*/
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  h_maxlen " << h_maxlen << "  v_maxlen " << v_maxlen << "  margin "
       << margin << "  alp_rend_sw " << alp_rend_sw << "  anti_ali_sw "
       << anti_ali_sw

       << "   tile w " << tile.getRaster()->getLx() << "  h "
       << tile.getRaster()->getLy() << "  pixbits "
       << ino::pixel_bits(tile.getRaster());
    if (hori_cn_is) {
      os << "   htile w " << hori_tile.getRaster()->getLx() << "  h "
         << hori_tile.getRaster()->getLy() << "  pixbits "
         << ino::pixel_bits(hori_tile.getRaster());
    }
    if (vert_cn_is) {
      os << "   vtile w " << vert_tile.getRaster()->getLx() << "  h "
         << vert_tile.getRaster()->getLy() << "  pixbits "
         << ino::pixel_bits(vert_tile.getRaster());
    }
    os << "   frame " << frame << "   rand_sets affine_det "
       << rend_sets.m_affine.det() << "  shrink x " << rend_sets.m_shrinkX
       << "  y " << rend_sets.m_shrinkY;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    if (hori_cn_is) {
      hori_tile.getRaster()->lock();
    }
    if (vert_cn_is) {
      vert_tile.getRaster()->lock();
    }
    fx_(enlarge_tile.getRaster()  // in with margin
        ,
        (hori_cn_is ? hori_tile.getRaster() : nullptr)  // with margin
        ,
        (vert_cn_is ? vert_tile.getRaster() : nullptr)  // with margin
        ,
        margin  // margin
        ,
        tile.getRaster()  // out with no margin
        ,
        h_maxlen, v_maxlen, this->m_h_ref_mode->getValue(),
        this->m_v_ref_mode->getValue(), alp_rend_sw, anti_ali_sw);
    if (vert_cn_is) {
      vert_tile.getRaster()->unlock();
    }
    if (hori_cn_is) {
      hori_tile.getRaster()->unlock();
    }
    tile.getRaster()->unlock();
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::bad_alloc &e) {
    if (vert_cn_is) {
      vert_tile.getRaster()->unlock();
    }
    if (hori_cn_is) {
      hori_tile.getRaster()->unlock();
    }
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("std::bad_alloc <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (std::exception &e) {
    if (vert_cn_is) {
      vert_tile.getRaster()->unlock();
    }
    if (hori_cn_is) {
      hori_tile.getRaster()->unlock();
    }
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("exception <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (...) {
    if (vert_cn_is) {
      vert_tile.getRaster()->unlock();
    }
    if (hori_cn_is) {
      hori_tile.getRaster()->unlock();
    }
    tile.getRaster()->unlock();
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}
