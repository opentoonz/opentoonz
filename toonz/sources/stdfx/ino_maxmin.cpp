#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
namespace {
const double smoothing_edge_ = 1.0;
}
//------------------------------------------------------------
class ino_maxmin final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_maxmin)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TIntEnumParamP m_max_min_select;
  TDoubleParamP m_radius;
  TDoubleParamP m_polygon_number;
  TDoubleParamP m_degree;
  TBoolParamP m_alpha_rendering;

  TIntEnumParamP m_ref_mode;

public:
  ino_maxmin()
      : m_max_min_select(new TIntEnumParam(0, "Max"))
      , m_radius(1.0)
      /*	1mmにしたければ 1.0 * 640. / 12. / 25.4 とする
            0.35    = mm = ユーザ指定の初期値
            640/12  = 53.33333 = toonz独自の係数 =tunit.cpp参照
            25.4    = mm/inch
    */
      , m_polygon_number(2.0)
      , m_degree(0.0)
      , m_alpha_rendering(true)

      , m_ref_mode(new TIntEnumParam()) {
    this->m_radius->setMeasureName("fxLength");

    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "max_min_select", this->m_max_min_select);
    bindParam(this, "radius", this->m_radius);
    bindParam(this, "polygon_number", this->m_polygon_number);
    bindParam(this, "degree", this->m_degree);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);

    bindParam(this, "reference", this->m_ref_mode);

    this->m_max_min_select->addItem(1, "Min");
    this->m_radius->setValueRange(0.0, 100.0);
    this->m_polygon_number->setValueRange(2.0, 16.0);
    this->m_degree->setValueRange(0.0, std::numeric_limits<double>::max());

    this->m_ref_mode->addItem(0, "Red");
    this->m_ref_mode->addItem(1, "Green");
    this->m_ref_mode->addItem(2, "Blue");
    this->m_ref_mode->addItem(3, "Alpha");
    this->m_ref_mode->addItem(4, "Luminance");
    this->m_ref_mode->addItem(-1, "Nothing");
    this->m_ref_mode->setDefaultValue(0);
    this->m_ref_mode->setValue(0);
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (this->m_input.isConnected()) {
      const bool ret = this->m_input->doGetBBox(frame, bBox, info);
      const double margin =
          ceil(ino::pixel_per_mm() *
               (this->m_radius->getValue(frame) + smoothing_edge_));
      if (0.0 < margin) {
        bBox = bBox.enlarge(margin);
      }
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    const double radius = (this->m_radius->getValue(frame) + smoothing_edge_) *
                          ino::pixel_per_mm() *
                          sqrt(fabs(info.m_affine.det())) /
                          ((info.m_shrinkX + info.m_shrinkY) / 2.0);
    return TRasterFx::memorySize(rect.enlarge(ceil(radius) + 0.5), info.m_bpp);
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_maxmin, "inoMaxMinFx");
//------------------------------------------------------------
#include "igs_maxmin.h"
namespace {
void fx_(const TRasterP in_ras  // with margin
         ,
         const int margin, TRasterP out_ras  // no margin

         ,
         const TRasterP refer_ras, const int refer_mode

         ,
         const int min_sw, const double radius, const double smoothing_edge,
         const int npolygon, const double degree, const bool alp_rend_sw

         ,
         const int nthread) {
  TRasterGR8P out_gr8(in_ras->getLy(),
                      in_ras->getLx() * ino::channels() *
                          ((TRaster64P)in_ras ? sizeof(unsigned short)
                                              : sizeof(unsigned char)));
  out_gr8->lock();

  igs::maxmin::convert(
      in_ras->getRawData()  // BGRA
      ,
      out_gr8->getRawData()

          ,
      in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(in_ras)

                                                             ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? refer_ras->getRawData()
                                                     : nullptr)  // BGRA
      ,
      (((refer_ras != nullptr) && (0 <= refer_mode)) ? ino::bits(refer_ras) : 0)

          ,
      refer_mode, radius, smoothing_edge  // smooth_outer_range
      ,
      npolygon, degree + 180.0

      ,
      min_sw, alp_rend_sw, false

      ,
      nthread);

  ino::arr_to_ras(out_gr8->getRawData(), ino::channels(), out_ras, margin);
  out_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_maxmin::doCompute(TTile &tile, double frame,
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
  const double mm2scale_shrink_pixel =
      ino::pixel_per_mm() * sqrt(fabs(rend_sets.m_affine.det())) /
      ((rend_sets.m_shrinkX + rend_sets.m_shrinkY) / 2.0);

  /* 動作パラメータを得る */
  const int min_sw    = this->m_max_min_select->getValue();  // 0,1
  const double radius = this->m_radius->getValue(frame) * mm2scale_shrink_pixel;
  const int npolygon  = this->m_polygon_number->getValue(frame);
  const double degree = this->m_degree->getValue(frame);
  const bool alp_rend_sw = this->m_alpha_rendering->getValue();

  const int refer_mode = this->m_ref_mode->getValue();

  /* 	tcomposer(RenderManager)でRenderingするときはthreadは1つ、
          toonz(Desktop)でPreview,Outputするなら-1を指定
          -1は自動でthread数を決めCPUを最大に使い高速化 */
  const int nthread = -1;

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
  const int enlarge_pixel = (int)(ceil(radius + smoothing_edge_) + 0.5);
  enlarge_rect            = enlarge_rect.enlarge(enlarge_pixel);

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

  /*------ 参照画像生成 --------------------------------------*/
  TTile refer_tile;
  bool refer_sw = false;
  if (this->m_refer.isConnected()) {
    refer_sw = true;
    this->m_refer->allocateAndCompute(
        refer_tile, enlarge_tile.m_pos,
        TDimensionI(/* Pixel単位 */
                    enlarge_tile.getRaster()->getLx(),
                    enlarge_tile.getRaster()->getLy()),
        enlarge_tile.getRaster(), frame, rend_sets);
  }
  /* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
  tile.getRaster()->clear(); /* 塗りつぶしクリア */

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  min_sw " << min_sw << "  radius " << radius << "  npolygon "
       << npolygon << "  degree " << degree << "  alp_rend_sw " << alp_rend_sw
       << "  smoothing_edge " << smoothing_edge_ << "  nthread " << nthread
       << "  refer_mode " << refer_mode << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "   frame "
       << frame << "   rand_sets affine_det " << rend_sets.m_affine.det()
       << "  shrink x " << rend_sets.m_shrinkX << "  y " << rend_sets.m_shrinkY;
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
    fx_(enlarge_tile.getRaster()  // in with margin
        ,
        enlarge_pixel  // margin
        ,
        tile.getRaster()  // out with no margin

        ,
        refer_tile.getRaster(), refer_mode

        ,
        min_sw, radius, smoothing_edge_, npolygon, degree, alp_rend_sw

        ,
        nthread);
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
