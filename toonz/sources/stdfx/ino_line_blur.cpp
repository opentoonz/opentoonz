#ifdef NDEBUG

/*------------------------------------------------------------------*/
#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"
#include "tfxattributes.h"
#include "ino_common.h"
#include "igs_line_blur.h"

class ino_line_blur final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_line_blur)

  TRasterFxPort m_input;
  TIntEnumParamP m_b_action_mode;
  TDoubleParamP m_b_blur_count;
  TDoubleParamP m_b_blur_power;
  TIntEnumParamP m_b_blur_subpixel;
  TDoubleParamP m_b_blur_near_ref;
  TDoubleParamP m_b_blur_near_len;
  TDoubleParamP m_v_smooth_retry;
  TDoubleParamP m_v_near_ref;
  TDoubleParamP m_v_near_len;
  TDoubleParamP m_b_smudge_thick;
  TDoubleParamP m_b_smudge_remain;

public:
  ino_line_blur()
      : m_b_action_mode(new TIntEnumParam(0, "Blur"))
      , m_b_blur_count(51)
      , m_b_blur_power(1.0)
      , m_b_blur_subpixel(new TIntEnumParam())
      , m_b_blur_near_ref(5)
      , m_b_blur_near_len(160)
      , m_v_smooth_retry(100)
      , m_v_near_ref(4)
      , m_v_near_len(160)
      , m_b_smudge_thick(7)
      , m_b_smudge_remain(0.85) {
    addInputPort("Source", this->m_input);
    bindParam(this, "action_mode", this->m_b_action_mode);
    bindParam(this, "blur_count", this->m_b_blur_count);
    bindParam(this, "blur_power", this->m_b_blur_power);
    bindParam(this, "blur_subpixel", this->m_b_blur_subpixel);
    bindParam(this, "blur_near_ref", this->m_b_blur_near_ref);
    bindParam(this, "blur_near_len", this->m_b_blur_near_len);
    bindParam(this, "vector_smooth_retry", this->m_v_smooth_retry);
    bindParam(this, "vector_near_ref", this->m_v_near_ref);
    bindParam(this, "vector_near_len", this->m_v_near_len);
    bindParam(this, "smudge_thick", this->m_b_smudge_thick);
    bindParam(this, "smudge_remain", this->m_b_smudge_remain);

    this->m_b_action_mode->addItem(1, "Smudge");
    this->m_b_blur_count->setValueRange(1, 100);
    this->m_b_blur_power->setValueRange(0.1, 10.0);
    this->m_b_blur_subpixel->addItem(1, "1");
    this->m_b_blur_subpixel->addItem(2, "4");
    this->m_b_blur_subpixel->addItem(3, "9");
    this->m_b_blur_subpixel->setDefaultValue(2);
    this->m_b_blur_subpixel->setValue(2);
    this->m_b_blur_near_ref->setValueRange(1, 100);
    this->m_b_blur_near_len->setValueRange(1, 1000);
    this->m_v_smooth_retry->setValueRange(1, 1000);
    this->m_v_near_ref->setValueRange(1, 100);
    this->m_v_near_len->setValueRange(1, 1000);
    this->m_b_smudge_thick->setValueRange(1, 100);
    this->m_b_smudge_remain->setValueRange(0.0, 1.0);
  }
  double get_render_real_radius(const double frame, const TAffine affine) {
    double rad = this->m_b_blur_count->getValue(frame);
    /*--- 単位について考察必要 ---*/
    // rad *= ino::pixel_per_mm();
    /*--- Geometryを反映させる ---*/
    // TAffine aff(affine);
    return rad;
  }
  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox) {
    const int margin =
        static_cast<int>(ceil(this->get_render_real_radius(frame, affine)));
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
    // return true;/* geometry処理済の画像に加工することになる */
    return false; /* ここでの処理後にgeometryがかかる */
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};

FX_PLUGIN_IDENTIFIER(ino_line_blur, "inoLineBlurFx");

namespace {
void fx_(const TRasterP in_ras  // no margin
         ,
         TRasterP out_ras  // no margin
         ,
         const int action_mode, const int blur_count, const double blur_power,
         const int blur_subpixel, const int blur_near_ref,
         const int blur_near_len, const int vector_smooth_retry,
         const int vector_near_ref, const int vector_near_len,
         const int smudge_thick, const double smudge_remain) {
  igs::line_blur::convert(
      in_ras->getRawData(),   // const void *in_no_margin (BGRA)
      out_ras->getRawData(),  // void *out_no_margin (BGRA)
      in_ras->getLy(),        // const int height_no_margin
      in_ras->getLx(),        // const int width_no_margin
      in_ras->getWrap(), out_ras->getWrap(),
      ino::channels(),    // const int channels
      ino::bits(in_ras),  // const int bits
      blur_count, blur_power, blur_subpixel, blur_near_ref, blur_near_len,
      smudge_thick, smudge_remain, vector_smooth_retry, vector_near_ref,
      vector_near_len, false, /* bool mv_sw false=OFF */
      false,                  /* bool pv_sw false=OFF */
      false,                  /* bool cv_sw false=OFF */
      3,     /* long reference_channel 3=Red:RGBA or Blue:BGRA */
      false, /* bool debug_save_sw false=OFF */
      action_mode);
}
}  // namespace

void ino_line_blur::doCompute(
    TTile &tile, /* 注意:doGetBBox(-)が返す範囲の画像 */
    double frame, const TRenderSettings &rend_sets) {
  /*--- 接続していなければ処理しない -------------------------*/
  if (!this->m_input.isConnected()) {
    tile.getRaster()->clear(); /* 塗りつぶしクリア */
    return;
  }
  /*--- サポートしていないPixelタイプはエラーを投げる --------*/
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }
  /*--- パラメータを得る -------------------------------------*/
  const int action_mode         = this->m_b_action_mode->getValue();
  const int blur_count          = this->m_b_blur_count->getValue(frame);
  const double blur_power       = this->m_b_blur_power->getValue(frame);
  const int blur_subpixel       = this->m_b_blur_subpixel->getValue();
  const int blur_near_ref       = this->m_b_blur_near_ref->getValue(frame);
  const int blur_near_len       = this->m_b_blur_near_len->getValue(frame);
  const int vector_smooth_retry = this->m_v_smooth_retry->getValue(frame);
  const int vector_near_ref     = this->m_v_near_ref->getValue(frame);
  const int vector_near_len     = this->m_v_near_len->getValue(frame);
  const int smudge_thick        = this->m_b_smudge_thick->getValue(frame);
  const double smudge_remain    = this->m_b_smudge_remain->getValue(frame);
  /*--- 表示の範囲を得る -------------------------------------*/
  TRectD bBox =
      TRectD(tile.m_pos, /* Render画像上(Pixel単位)の位置 */
             TDimensionD(/* Render画像上(Pixel単位)のサイズ */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  /*--- doGetBBox(-)が返す範囲の画像を生成 -------------------*/
  TTile in_tile;
  this->m_input->allocateAndCompute(
      in_tile, bBox.getP00(),
      TDimensionI(/* Pixel単位に四捨五入 */
                  static_cast<int>(bBox.getLx() + 0.5),
                  static_cast<int>(bBox.getLy() + 0.5)),
      tile.getRaster(), frame, rend_sets);
  /*--- 保存すべき画像メモリを塗りつぶしクリア ---------------*/
  tile.getRaster()->clear();
  /*--- 画像処理 ---------------------------------------------*/
  try {
    tile.getRaster()->lock();
    fx_(in_tile.getRaster()  // in no margin
        ,
        tile.getRaster()  // out no margin
        ,
        action_mode, blur_count, blur_power, blur_subpixel, blur_near_ref,
        blur_near_len, vector_smooth_retry, vector_near_ref, vector_near_len,
        smudge_thick, smudge_remain);
    tile.getRaster()->unlock();
  }
  /*--- error処理 --------------------------------------------*/
  catch (std::bad_alloc &) {
    tile.getRaster()->unlock();
    throw;
  } catch (std::exception &) {
    tile.getRaster()->unlock();
    throw;
  } catch (...) {
    tile.getRaster()->unlock();
    throw;
  }
}
#endif
