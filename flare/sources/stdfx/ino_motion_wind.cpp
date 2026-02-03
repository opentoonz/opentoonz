#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_motion_wind final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_motion_wind)

  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TIntEnumParamP m_direction;
  TBoolParamP m_dark;
  TBoolParamP m_alpha_rendering;

  TDoubleParamP m_length_min;
  TDoubleParamP m_length_max;
  TDoubleParamP m_length_bias;
  TDoubleParamP m_length_seed;
  TBoolParamP m_length_ref;

  TDoubleParamP m_force_min;
  TDoubleParamP m_force_max;
  TDoubleParamP m_force_bias;
  TDoubleParamP m_force_seed;
  TBoolParamP m_force_ref;

  TDoubleParamP m_density_min;
  TDoubleParamP m_density_max;
  TDoubleParamP m_density_bias;
  TDoubleParamP m_density_seed;
  TBoolParamP m_density_ref;

  // TBoolParamP m_spread;
  TIntEnumParamP m_ref_mode;

public:
  ino_motion_wind()
      : m_direction(new TIntEnumParam(0, "Right"))
      , m_dark(false)
      , m_alpha_rendering(true)

      , m_length_min(0.0)
      , m_length_max(18.0)
      , m_length_bias(1.0 * ino::param_range())
      , m_length_seed(1.0)
      , m_length_ref(false)

      , m_force_min(1.0 * ino::param_range())
      , m_force_max(1.0 * ino::param_range())
      , m_force_bias(1.0 * ino::param_range())
      , m_force_seed(1.0)
      , m_force_ref(false)

      , m_density_min(1.0 * ino::param_range())
      , m_density_max(1.0 * ino::param_range())
      , m_density_bias(1.0 * ino::param_range())
      , m_density_seed(1.0)
      , m_density_ref(false)

      // , m_spread(true)
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    this->m_length_min->setMeasureName("fxLength");
    this->m_length_max->setMeasureName("fxLength");

    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "direction", this->m_direction);
    bindParam(this, "dark", this->m_dark);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);

    bindParam(this, "length_min", this->m_length_min);
    bindParam(this, "length_max", this->m_length_max);
    bindParam(this, "length_bias", this->m_length_bias);
    bindParam(this, "length_seed", this->m_length_seed);
    bindParam(this, "length_ref", this->m_length_ref);

    bindParam(this, "force_min", this->m_force_min);
    bindParam(this, "force_max", this->m_force_max);
    bindParam(this, "force_bias", this->m_force_bias);
    bindParam(this, "force_seed", this->m_force_seed);
    bindParam(this, "force_ref", this->m_force_ref);

    bindParam(this, "density_min", this->m_density_min);
    bindParam(this, "density_max", this->m_density_max);
    bindParam(this, "density_bias", this->m_density_bias);
    bindParam(this, "density_seed", this->m_density_seed);
    bindParam(this, "density_ref", this->m_density_ref);
    // bindParam(this,"spread", this->m_spread);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_direction->addItem(1, "Up");
    this->m_direction->addItem(2, "Left");
    this->m_direction->addItem(3, "Down");

    this->m_length_min->setValueRange(0.0, 1000.0);
    this->m_length_max->setValueRange(0.0, 1000.0);
    this->m_length_bias->setValueRange(0.1 * ino::param_range(),
                                       10.0 * ino::param_range()); /* gamma値 */
    this->m_length_seed->setValueRange(
        0, std::numeric_limits<unsigned long>::max());

    this->m_force_min->setValueRange(0.1 * ino::param_range(),
                                     10.0 * ino::param_range()); /* gamma値 */
    this->m_force_max->setValueRange(0.1 * ino::param_range(),
                                     10.0 * ino::param_range()); /* gamma値 */
    this->m_force_bias->setValueRange(0.1 * ino::param_range(),
                                      10.0 * ino::param_range()); /* gamma値 */
    this->m_force_seed->setValueRange(
        0, std::numeric_limits<unsigned long>::max());

    this->m_density_min->setValueRange(0.0 * ino::param_range(),
                                       100.0 * ino::param_range());
    this->m_density_max->setValueRange(0.0 * ino::param_range(),
                                       100.0 * ino::param_range());
    this->m_density_bias->setValueRange(
        0.1 * ino::param_range(), 10.0 * ino::param_range()); /* gamma値 */
    this->m_density_seed->setValueRange(
        0, std::numeric_limits<unsigned long>::max());

    this->m_ref_mode->addItem(1, "Green");
    this->m_ref_mode->addItem(2, "Blue");
    this->m_ref_mode->addItem(3, "Alpha");
    this->m_ref_mode->addItem(4, "Luminance");
    this->m_ref_mode->addItem(-1, "Nothing");
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (this->m_input.isConnected()) {
      const bool ret = this->m_input->doGetBBox(frame, bBox, info);
      // if ( this->m_spread->getValue() ) {
      const double min = this->m_length_min->getValue(frame);
      const double max = this->m_length_max->getValue(frame);
      const double margin =
          ceil(ino::pixel_per_mm() * ((min < max) ? max : min));
      if (0.0 < margin) {
        bBox = bBox.enlarge(margin);
      }
      // }
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }
  bool canHandle(const TRenderSettings &info, double frame) override {
    // return true;
    return false;
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    const double mm2scale_shrink_pixel =
        ino::pixel_per_mm() * sqrt(fabs(info.m_affine.det())) /
        ((info.m_shrinkX + info.m_shrinkY) / 2.0);
    const double length_min =
        this->m_length_min->getValue(frame) * mm2scale_shrink_pixel;
    const double length_max =
        this->m_length_max->getValue(frame) * mm2scale_shrink_pixel;
    const int enlarge_pixel =
        (int)(ceil((length_min < length_max) ? length_max : length_min) + 0.5);
    return TRasterFx::memorySize(rect.enlarge(enlarge_pixel), info.m_bpp);
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_motion_wind, "inoMotionWindFx");
//------------------------------------------------------------
#include "igs_motion_wind.h"
namespace {
void fx_(const TRasterP in_ras  // with margin
         ,
         const TRasterP refer_ras  // with margin
         ,
         const int refer_mode, const int margin, TRasterP out_ras  // no margin

         ,
         const int direction, const bool dark_sw, const bool alpha_rendering_sw

         ,
         const double length_min, const double length_max,
         const double length_bias, const unsigned long length_seed,
         const bool length_ref_sw

         ,
         const double force_min, const double force_max,
         const double force_bias, const unsigned long force_seed,
         const bool force_ref_sw

         ,
         const double density_min, const double density_max,
         const double density_bias, const unsigned long density_seed,
         const bool density_ref_sw) {
  /***std::vector<unsigned char> in_vec;
  ino::ras_to_vec( in_ras, ino::channels(), in_vec );
  std::vector<unsigned char> refer_vec;
  ino::ras_to_vec( refer_ras, ino::channels(), refer_vec );***/

  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  if (refer_ras != nullptr) {
    TRasterGR8P refer_gr8(refer_ras->getLy(),
                          refer_ras->getLx() * ino::channels() *
                              ((TRaster64P)refer_ras ? sizeof(unsigned short)
                                                     : sizeof(unsigned char)));
    refer_gr8->lock();
    ino::ras_to_arr(refer_ras, ino::channels(), refer_gr8->getRawData());

    igs::motion_wind::change(
        // in_ras->getRawData() // BGRA
        //&in_vec.at(0) // RGBA
        in_gr8->getRawData()  // BGRA

        ,
        in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(in_ras)

                                                               ,
        refer_gr8->getRawData()

            ,
        refer_ras->getLy(), refer_ras->getLx(), ino::channels(),
        ino::bits(refer_ras)

            ,
        refer_mode

        ,
        direction, dark_sw, alpha_rendering_sw

        ,
        length_seed, length_min, length_max, length_bias, length_ref_sw,
        force_seed, force_min, force_max, force_bias, force_ref_sw,
        density_seed, density_min, density_max, density_bias, density_ref_sw);
    /***ino::vec_to_ras( refer_vec, 0, 0 );
ino::vec_to_ras( in_vec, ino::channels(), out_ras, margin );***/

    ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), out_ras, margin);
    refer_gr8->unlock();
  } else {
    igs::motion_wind::change(
        // in_ras->getRawData() // BGRA
        //&in_vec.at(0) // RGBA
        in_gr8->getRawData()  // BGRA

        ,
        in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(in_ras)

                                                               ,
        0

        ,
        0, 0, 0, 0

        ,
        0

        ,
        direction, dark_sw, alpha_rendering_sw

        ,
        length_seed, length_min, length_max, length_bias, length_ref_sw

        ,
        force_seed, force_min, force_max, force_bias, force_ref_sw

        ,
        density_seed, density_min, density_max, density_bias, density_ref_sw);
    /***ino::vec_to_ras( refer_vec, 0, 0 );
ino::vec_to_ras( in_vec, ino::channels(), out_ras, margin );***/

    ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), out_ras, margin);
  }
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_motion_wind::doCompute(TTile &tile, double frame,
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

  /* ------ Pixel単位で動作パラメータを得る ----------------- */
  /* 単位変換用スケーリング
  外積(面積)
          rend_sets.m_affine.det() = a11*a22-a12*a21 // double
  シュリンク値
          rend_sets.m_shrinkX // int
          rend_sets.m_shrinkY // int
  */
  const double mm2scale_shrink_pixel =
      ino::pixel_per_mm() * sqrt(fabs(rend_sets.m_affine.det())) /
      ((rend_sets.m_shrinkX + rend_sets.m_shrinkY) / 2.0);

  /* 動作パラメータを得る */
  const int direction    = this->m_direction->getValue();
  const bool dark_sw     = this->m_dark->getValue();
  const bool alp_rend_sw = this->m_alpha_rendering->getValue();

  const double length_min =
      this->m_length_min->getValue(frame) * mm2scale_shrink_pixel;
  const double length_max =
      this->m_length_max->getValue(frame) * mm2scale_shrink_pixel;
  const double length_bias =
      this->m_length_bias->getValue(frame) / ino::param_range();
  const unsigned long length_seed = this->m_length_seed->getValue(frame);
  const bool length_ref_sw        = this->m_length_ref->getValue();

  const double force_min =
      this->m_force_min->getValue(frame) / ino::param_range();
  const double force_max =
      this->m_force_max->getValue(frame) / ino::param_range();
  const double force_bias =
      this->m_force_bias->getValue(frame) / ino::param_range();
  const unsigned long force_seed = this->m_force_seed->getValue(frame);
  const bool force_ref_sw        = this->m_force_ref->getValue();

  const double density_min =
      this->m_density_min->getValue(frame) / ino::param_range();
  const double density_max =
      this->m_density_max->getValue(frame) / ino::param_range();
  const double density_bias =
      this->m_density_bias->getValue(frame) / ino::param_range();
  const unsigned long density_seed = this->m_density_seed->getValue(frame);
  const bool density_ref_sw        = this->m_density_ref->getValue();
  const int refer_mode             = this->m_ref_mode->getValue();

  /* ------ 参照マージン含めた画像生成 ---------------------- */
  /* Rendering画像のBBox値 --> Pixel単位のdouble値 */
  /*	typedef TRectT<double> TRectD;
          inline TRectT<double>::TRectT(
                  const TPointT<double> &bottomLeft
                  , const TDimensionT<double> &d
          )	: x0(bottomLeft.x)
                  , y0(bottomLeft.y)
                  , x1(bottomLeft.x+d.lx)
                  , y1(bottomLeft.y+d.ly)
          {}
  */
  TRectD enlarge_rect =
      TRectD(tile.m_pos /* TPointD */
             ,
             TDimensionD(/* int --> doubleにしてセット */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  /* BBoxの拡大 Pixel単位 */
  const int enlarge_pixel =
      (int)(ceil((length_min < length_max) ? length_max : length_min) + 0.5);
  enlarge_rect = enlarge_rect.enlarge(enlarge_pixel);

  /*	void TRasterFx::allocateAndCompute(
                  TTile &tile,
                  const TPointD &pos, const TDimension &size,
                  const TRasterP &templateRas, double frame,
                  const TRenderSettings &info
          ) {
                  if (templateRas) {
          tile.getRaster() = templateRas->create(size.lx, size.ly);
                  } else {
          tile.getRaster() = TRaster32P(size.lx, size.ly);
                  }
          tile.m_pos = pos;
                  compute(tile, frame, info);
          }
  */
  TTile enlarge_tile;
  this->m_input->allocateAndCompute(
      enlarge_tile, enlarge_rect.getP00(),
      TDimensionI(/* Pixel単位のdouble -> intにしてセット */
                  (int)(enlarge_rect.getLx() + 0.5),
                  (int)(enlarge_rect.getLy() + 0.5)),
      tile.getRaster(), frame, rend_sets);

  /* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
  tile.getRaster()->clear(); /* 塗りつぶしクリア */

  /* ------ 参照画像生成 ------------------------------------ */
  TTile refer_tile;
  const bool refer_sw = this->m_refer.isConnected();
  if (refer_sw) {
    this->m_refer->allocateAndCompute(
        refer_tile
        // tile.m_pos,
        // tile.getRaster()->getSize(),
        ,
        enlarge_rect.getP00(),
        TDimensionI(/* Pixel単位のdouble -> intにしてセット */
                    (int)(enlarge_rect.getLx() + 0.5),
                    (int)(enlarge_rect.getLy() + 0.5)),
        tile.getRaster(), frame, rend_sets);
  }

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  dir " << direction << "  dark_sw " << dark_sw << "  alp_rend_sw "
       << alp_rend_sw

       << "  len_min " << length_min << "  len_max " << length_max
       << "  len_bias " << length_bias << "  len_seed " << length_seed
       << "  len_ref_sw " << length_ref_sw

       << "  for_min " << force_min << "  for_max " << force_max
       << "  for_bias " << force_bias << "  for_seed " << force_seed
       << "  for_ref_sw " << force_ref_sw

       << "  den_min " << density_min << "  den_max " << density_max
       << "  den_bias " << density_bias << "  den_seed " << density_seed
       << "  den_ref_sw " << density_ref_sw << "  reference " << refer_mode

       << "   tile w " << tile.getRaster()->getLx() << "  h "
       << tile.getRaster()->getLy() << "  pixbits "
       << ino::pixel_bits(tile.getRaster());
    if (refer_sw) {
      os << "   rtile w " << refer_tile.getRaster()->getLx() << "  h "
         << refer_tile.getRaster()->getLy();
    }
    os << "   frame " << frame << "   rand_sets affine_det "
       << rend_sets.m_affine.det() << "  shrink x " << rend_sets.m_shrinkX
       << "  y " << rend_sets.m_shrinkY;
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
        refer_tile.getRaster()  // with margin
        ,
        refer_mode, enlarge_pixel  // margin
        ,
        tile.getRaster()  // out with no margin

        ,
        direction, dark_sw, alp_rend_sw

        ,
        length_min, length_max, length_bias, length_seed, length_ref_sw,
        force_min, force_max, force_bias, force_seed, force_ref_sw, density_min,
        density_max, density_bias, density_seed, density_ref_sw);
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
