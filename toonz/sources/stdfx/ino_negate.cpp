#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_negate final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ino_negate)
  TRasterFxPort m_input;
  TBoolParamP m_red;
  TBoolParamP m_green;
  TBoolParamP m_blue;
  TBoolParamP m_alpha;

public:
  ino_negate() : m_red(true), m_green(true), m_blue(true), m_alpha(false) {
    addInputPort("Source", this->m_input);
    bindParam(this, "red", this->m_red);
    bindParam(this, "green", this->m_green);
    bindParam(this, "blue", this->m_blue);
    bindParam(this, "alpha", this->m_alpha);
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
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
};
FX_PLUGIN_IDENTIFIER(ino_negate, "inoNegateFx");
//------------------------------------------------------------
#include "igs_negate.h"
namespace {
void fx_(TRasterP in_ras, const bool sw_array[4]) {
  /***std::vector<unsigned char> in_vec;
  ino::ras_to_vec( in_ras, ino::channels(), in_vec );***/

  TRasterGR8P in_gr8(in_ras->getLy(),
                     in_ras->getLx() * ino::channels() *
                         ((TRaster64P)in_ras ? sizeof(unsigned short)
                                             : sizeof(unsigned char)));
  in_gr8->lock();
  ino::ras_to_arr(in_ras, ino::channels(), in_gr8->getRawData());

  igs::negate::change(
      // in_ras->getRawData() // BGRA
      //&in_vec.at(0) // RGBA
      in_gr8->getRawData()

          ,
      in_ras->getLy(), in_ras->getLx()  // Not use in_ras->getWrap()
      ,
      ino::channels(), ino::bits(in_ras)

                           ,
      sw_array);

  /***ino::vec_to_ras( in_vec, ino::channels(), in_ras, 0 );***/

  ino::arr_to_ras(in_gr8->getRawData(), ino::channels(), in_ras, 0);
  in_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_negate::doCompute(TTile &tile, double frame,
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
  bool sw_array[4];
  sw_array[0] = this->m_red->getValue();
  sw_array[1] = this->m_green->getValue();
  sw_array[2] = this->m_blue->getValue();
  sw_array[3] = this->m_alpha->getValue();

  /* ------ 画像生成 ---------------------------------------- */
  this->m_input->compute(tile, frame, rend_sets);

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  r_sw " << sw_array[0] << "  g_sw " << sw_array[1] << "  b_sw "
       << sw_array[2] << "  a_sw " << sw_array[3] << "   tile w "
       << tile.getRaster()->getLx() << "  h " << tile.getRaster()->getLy()
       << "  pixbits " << ino::pixel_bits(tile.getRaster()) << "   frame "
       << frame;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    tile.getRaster()->lock();
    fx_(tile.getRaster(), sw_array);
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
