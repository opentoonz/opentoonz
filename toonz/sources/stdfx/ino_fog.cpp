#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
#include "igs_fog.h"
//------------------------------------------------------------
class ino_fog final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_fog)
  TRasterFxPort m_input;
  TDoubleParamP m_radius;
  TDoubleParamP m_curve;
  TDoubleParamP m_power;
  TDoubleParamP m_threshold_min;
  TDoubleParamP m_threshold_max;
  TBoolParamP m_alpha_rendering;

public:
  ino_fog()
      : m_radius(1.0)
      , m_curve(1.0 * ino::param_range())
      , m_power(1.0 * ino::param_range())
      , m_threshold_min(0.0 * ino::param_range())
      , m_threshold_max(0.0 * ino::param_range())
      , m_alpha_rendering(false) {
    this->m_radius->setMeasureName("fxLength");

    addInputPort("Source", this->m_input);

    bindParam(this, "radius", this->m_radius);
    bindParam(this, "curve", this->m_curve);
    bindParam(this, "power", this->m_power);
    bindParam(this, "threshold_min", this->m_threshold_min);
    bindParam(this, "threshold_max", this->m_threshold_max);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);

    this->m_radius->setValueRange(0.0, 100.0);
    this->m_curve->setValueRange(0.1 * ino::param_range(),
                                 10.0 * ino::param_range()); /* gammaカーブ */
    this->m_power->setValueRange(-2.0 * ino::param_range(),
                                 2.0 * ino::param_range());
    this->m_threshold_min->setValueRange(0.0 * ino::param_range(),
                                         1.01 * ino::param_range());
    this->m_threshold_max->setValueRange(0.0 * ino::param_range(),
                                         1.01 * ino::param_range());
  }
  //------------------------------------------------------------
  double get_render_real_radius(const double frame, const TAffine affine) {
    return this->m_radius->getValue(frame) *
           ino::pixel_per_mm()        /* render用単位にする */
           * sqrt(fabs(affine.det())) /* 拡大縮小のGeometryを反映 */
        ;
  }
  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox) {
    const int margin =
        static_cast<int>(ceil(this->get_render_real_radius(frame, affine)));
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
    return true;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_fog, "inoFogFx");
//------------------------------------------------------------
namespace {
void fx_(const TRasterP in_ras  // with margin
         ,
         const int margin, TRasterP out_ras  // no margin
         ,
         const int nthread, const double radius, const double curve,
         const double power, const double threshold_min,
         const double threshold_max, const bool alp_rend_sw) {
  TRasterGR8P out_gr8(in_ras->getLy(),
                      in_ras->getLx() * ino::channels() *
                          ((TRaster64P)in_ras ? sizeof(unsigned short)
                                              : sizeof(unsigned char)));
  out_gr8->lock();

  TRasterGR8P ref_gr8(in_ras->getLy(), in_ras->getLx() * sizeof(double));
  ref_gr8->lock();

  igs::fog::convert(
      in_ras->getRawData()  // BGRA
      ,
      out_gr8->getRawData(), reinterpret_cast<double *>(ref_gr8->getRawData())

                                 ,
      in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(in_ras)

                                                             ,
      nthread

      ,
      radius, curve, 2, 0.0, power, threshold_min, threshold_max, alp_rend_sw);
  ref_gr8->unlock();

  ino::arr_to_ras(out_gr8->getRawData(), ino::channels(), out_ras, margin);
  out_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_fog::doCompute(TTile &tile, double frame,
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
  const double radius = this->get_render_real_radius(frame, rend_sets.m_affine);
  const double curve  = this->m_curve->getValue(frame) / ino::param_range();
  const double power  = this->m_power->getValue(frame) / ino::param_range();
  const double threshold_min =
      this->m_threshold_min->getValue(frame) / ino::param_range();
  const double threshold_max =
      this->m_threshold_max->getValue(frame) / ino::param_range();
  const bool alp_rend_sw = this->m_alpha_rendering->getValue();
  const int nthread      = 1;
  /*------ fogがからないパラメータ値のときはfog処理しない ----*/
  if (!igs::fog::have_change(radius, power, threshold_min)) {
    this->m_input->compute(tile, frame, rend_sets);
    return;
  }
  /*------ 表示の範囲を得る ----------------------------------*/
  TRectD bBox =
      TRectD(tile.m_pos /* Render画像上(Pixel単位)の位置 */
             ,
             TDimensionD(/* Render画像上(Pixel単位)のサイズ */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  /*------ fog半径(=マージン)分表示範囲を広げる --------------*/
  const int int_radius = static_cast<int>(ceil(radius));
  if (0 < int_radius) {
    bBox = bBox.enlarge(static_cast<double>(int_radius));
  }
  TTile enlarge_tile;
  this->m_input->allocateAndCompute(
      enlarge_tile, bBox.getP00(),
      TDimensionI(/* Pixel単位に四捨五入 */
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
       << "  nthread " << nthread << "  usr_radius "
       << this->m_radius->getValue(frame) << "  real_radius " << radius
       << "  int_radius " << int_radius << "  curve " << curve << "  power "
       << power << "  threshold_min " << threshold_min << "  threshold_max "
       << threshold_max << "  alp_rend_sw " << alp_rend_sw << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "   frame "
       << frame << "   rand_sets affine_det " << rend_sets.m_affine.det()
       << "  shrink x " << rend_sets.m_shrinkX << "  y " << rend_sets.m_shrinkY;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    fx_(enlarge_tile.getRaster()  // with margin
        ,
        int_radius  // margin
        ,
        tile.getRaster()  // no margin
        ,
        nthread, radius, curve, power, threshold_min, threshold_max,
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
