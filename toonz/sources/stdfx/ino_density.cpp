#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_density final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_density)
  TRasterFxPort m_input;
  TRasterFxPort m_refer;

  TDoubleParamP m_density;

  TIntEnumParamP m_ref_mode;

public:
  ino_density()
      : m_density(1.0 * ino::param_range())
      , m_ref_mode(new TIntEnumParam(0, "Red")) {
    addInputPort("Source", this->m_input);
    addInputPort("Reference", this->m_refer);

    bindParam(this, "density", this->m_density);
    bindParam(this, "reference", this->m_ref_mode);

    this->m_density->setValueRange(0.0 * ino::param_range(),
                                   10.0 * ino::param_range());

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
  bool canHandle(const TRenderSettings &rend_sets, double frame) override {
    return true;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_density, "inoDensityFx");
//------------------------------------------------------------
#include "igs_density.h"

namespace {
void fx_(TRasterP in_ras, TRasterP refer_ras, const int refer_mode,
         const double density) {
  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  igs::density::change(
      in_gr8->getRawData()  // BGRA
      ,
      in_ras->getLy(), in_ras->getLx()  // Not use in_ras->getWrap()
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
      density);

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_density::doCompute(TTile &tile, double frame,
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
  const double density = this->m_density->getValue(frame) / ino::param_range();
  const int refer_mode = this->m_ref_mode->getValue();

  /* ------ 画像生成 ---------------------------------------- */
  this->m_input->compute(tile, frame, rend_sets);

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
       << "  den " << density << "  refer_mode " << refer_mode << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "   frame "
       << frame;
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
    fx_(tile.getRaster(), refer_tile.getRaster(), refer_mode, density);
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
