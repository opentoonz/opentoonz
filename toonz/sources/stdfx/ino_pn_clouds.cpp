#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
/*
toonz6.4sg ではTStandardRasterFx
toonz7.0   ではTStandardZeraryFx
*/
class ino_pn_clouds final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(ino_pn_clouds)

  TDoubleParamP m_size;
  TDoubleParamP m_z;
  TIntEnumParamP m_octaves;
  TDoubleParamP m_persistance;
  TBoolParamP m_alpha_rendering;

public:
  ino_pn_clouds()
      : m_size(10.0)
      , m_z(0.0)
      , m_octaves(new TIntEnumParam(0, "1"))
      , m_persistance(0.5)
      , m_alpha_rendering(true) {
    this->m_size->setMeasureName("fxLength");

    bindParam(this, "size", this->m_size);
    bindParam(this, "z", this->m_z);

    bindParam(this, "octaves", this->m_octaves);
    this->m_octaves->addItem(1, "2");
    this->m_octaves->addItem(2, "3");
    this->m_octaves->addItem(3, "4");
    this->m_octaves->addItem(4, "5");
    this->m_octaves->addItem(5, "6");
    this->m_octaves->addItem(6, "7");
    this->m_octaves->addItem(7, "8");
    this->m_octaves->addItem(8, "9");
    this->m_octaves->addItem(9, "10");

    bindParam(this, "persistance", this->m_persistance);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);

    this->m_size->setValueRange(0.0, 1000.0);
    this->m_persistance->setValueRange(0.1 * ino::param_range(),
                                       2.0 * ino::param_range());

    enableComputeInFloat(true);
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
  }
  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_pn_clouds, "inopnCloudsFx");
// ------------------------------------------------------------------

#include "igs_perlin_noise.h"
namespace {
void fx_(TRasterP in_ras, const double zz, const int octaves,
         const double persistance, const bool alpha_rendering_sw,
         const double a11, const double a12, const double a13, const double a21,
         const double a22, const double a23) {
  igs::perlin_noise::change(in_ras->getRawData()  // BGRA
                            ,
                            in_ras->getLy(), in_ras->getLx(), in_ras->getWrap(),
                            ino::channels(), ino::bits(in_ras),
                            alpha_rendering_sw, a11, a12, a13, a21, a22, a23,
                            zz, 0, octaves, persistance);
}
}  // namespace
//------------------------------------------------------------
void ino_pn_clouds::doCompute(TTile &tile, double frame,
                              const TRenderSettings &rend_sets) {
  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster()) &&
      !((TRasterFP)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  /* ------ Pixel単位で動作パラメータを得る ----------------- */
  /* 単位変換用スケーリング
  行列値(=外積?=面積?)
          rend_sets.m_affine.det() = a11*a22-a12*a21 // double
  シュリンク値
          rend_sets.m_shrinkX // int
          rend_sets.m_shrinkY // int
  2009-10-09
          必ず1が返るようになり、
          シュリンク値はscale値と中心の移動値として
          m_affineに含まれるようになった、
          いつからか分からない、、、。
          詳細不明。??????????????????
  */

  /* 動作パラメータを得る */
  const double size        = this->m_size->getValue(frame);
  const double zz          = this->m_z->getValue(frame);
  const int octaves        = this->m_octaves->getValue();
  const double persistance = this->m_persistance->getValue(frame);
  const bool alp_rend_sw   = this->m_alpha_rendering->getValue();

  TAffine aff        = rend_sets.m_affine * TScale(ino::pixel_per_mm());
  const double scale = 1.0 / (size * sqrt(fabs(aff.det())));
  TAffine aff_pn     = TScale(scale) * TTranslation(tile.m_pos);

  /* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
  tile.getRaster()->clear(); /* 塗りつぶしクリア */

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  size " << size << "  z " << zz << "  octaves " << octaves
       << "  persistance " << persistance << "  alp_rend_sw " << alp_rend_sw

       << "   tile w " << tile.getRaster()->getLx() << "  h "
       << tile.getRaster()->getLy() << "  pixbits "
       << ino::pixel_bits(tile.getRaster());
    os << "   frame " << frame << "   aff_pn scale " << scale << "  pos x "
       << tile.m_pos.x << "  y " << tile.m_pos.y;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    fx_(tile.getRaster(), zz, octaves, persistance, alp_rend_sw, aff_pn.a11,
        aff_pn.a12, aff_pn.a13, aff_pn.a21, aff_pn.a22, aff_pn.a23);
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
