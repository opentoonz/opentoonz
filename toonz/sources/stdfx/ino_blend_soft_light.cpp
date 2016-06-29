//------------------------------------------------------------
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
/* tnzbase --> Source Files --> tfx --> binaryFx.cppを参照 */
class ino_blend_soft_light final : public TBlendForeBackRasterFx {
  FX_PLUGIN_DECLARATION(ino_blend_soft_light)
  TRasterFxPort m_up;
  TRasterFxPort m_down;
  TDoubleParamP m_opacity;
  TBoolParamP m_clipping_mask;

public:
  ino_blend_soft_light()
      : m_opacity(1.0 * ino::param_range()), m_clipping_mask(true) {
    addInputPort("Fore", this->m_up);
    addInputPort("Back", this->m_down);
    bindParam(this, "opacity", this->m_opacity);
    bindParam(this, "clipping_mask", this->m_clipping_mask);
    this->m_opacity->setValueRange(0, 1.0 * ino::param_range());
  }
  ~ino_blend_soft_light() {}
  bool canHandle(const TRenderSettings &rs, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &rs) override {
    TRectD up_bx;
    const bool up_sw =
        (m_up.isConnected() ? m_up->doGetBBox(frame, up_bx, rs) : false);
    TRectD dn_bx;
    const bool dn_sw =
        (m_down.isConnected() ? m_down->doGetBBox(frame, dn_bx, rs) : false);
    if (up_sw && dn_sw) {
      bBox = up_bx + dn_bx;
      return !bBox.isEmpty();
    } else if (up_sw) {
      bBox = up_bx;
      return true;
    } else if (dn_sw) {
      bBox = dn_bx;
      return true;
    } else {
      bBox = TRectD();
      return false;
    }
  }
  // TRect getInvalidRect(const TRect &max) {return max;}
  // void doSetParam(const std::string &name, const TParamP &param) {}
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &rs) override {
    return TRasterFx::memorySize(rect, rs.m_bpp);
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &rs) override {
    this->dryComputeUpAndDown(rect, frame, rs, false);
  }
  void doCompute(TTile &tile, double frame, const TRenderSettings &rs) override;
  void computeUpAndDown(TTile &tile, double frame, const TRenderSettings &rs,
                        TRasterP &dn_ras, TRasterP &up_ras,
                        bool upComputesWholeTile = false);
  void dryComputeUpAndDown(TRectD &rect, double frame,
                           const TRenderSettings &rs,
                           bool upComputesWholeTile = false
                           /*
           upComputesWholeTile は Screen, Min, Blendでtrueにして使用している。
           */
                           );
};
FX_PLUGIN_IDENTIFIER(ino_blend_soft_light, "inoSoftLightFx");
//------------------------------------------------------------
namespace {
/* より大きな四角エリアにPixel整数値で密着する */
void makeRectCoherent(TRectD &rect, const TPointD &pos) {
  rect -= pos;
  rect.x0 = tfloor(rect.x0); /* ((x)<(int)(x)? (int)(x)-1: (int)(x))*/
  rect.y0 = tfloor(rect.y0);
  rect.x1 = tceil(rect.x1); /* ((int)(x)<(x)? (int)(x)+1: (int)(x))*/
  rect.y1 = tceil(rect.y1);
  rect += pos;
}
}
void ino_blend_soft_light::computeUpAndDown(TTile &tile, double frame,
                                            const TRenderSettings &rs,
                                            TRasterP &dn_ras, TRasterP &up_ras,
                                            bool upComputesWholeTile) {
  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }
  /*
m_down,m_upは繋がっている方があればそれを表示する
両方とも接続していれば合成処理する
表示スイッチを切ってあるならm_upを表示する
fxをreplaceすると、
  m_source   --> m_up  (=port0)
  m_refernce --> m_down(=port1)
となる
*/
  const bool up_is = (this->m_up.isConnected() &&
                      this->m_up.getFx()->getTimeRegion().contains(frame));
  const bool down_is = (this->m_down.isConnected() &&
                        this->m_down.getFx()->getTimeRegion().contains(frame));
  /* ------ 両方とも切断の時処理しない ---------------------- */
  if (!up_is && !down_is) {
    tile.getRaster()->clear();
    return;
  }
  /* ------ up接続かつdown切断の時 -------------------------- */
  if (up_is && !down_is) {
    this->m_up->compute(tile, frame, rs);
    return;
  }
  /* ------ down接続時 downのみ描画して... ------------------ */
  if (down_is) {
    this->m_down->compute(tile, frame, rs);
  }
  /* ------ up切断時 ---------------------------------------- */
  if (!up_is) {
    return;
  }

  /* upと重なる部分を描画する */

  /* ------ tileの範囲 -------------------------------------- */
  const TDimension tsz(tile.getRaster()->getSize()); /* 整数 */
  const TRectD tileRect(tile.m_pos, TDimensionD(tsz.lx, tsz.ly));
  TRectD upBBox;
  if (upComputesWholeTile) {
    upBBox = tileRect;
  }      /* tile全体を得る */
  else { /* 厳密なエリア... */
    this->m_up->getBBox(frame, upBBox, rs);
    upBBox *= tileRect; /* upとtileの交差エリア */

    /* より大きな四角エリアにPixel整数値で密着する */
    makeRectCoherent(upBBox, tile.m_pos);  // double-->int grid
  }

  TDimensionI upSize(                        /* TRectDをTDimensionIに変換 */
                     tround(upBBox.getLx())  // getLx() = "x1>=x0?x1-x0:0"
                     ,
                     tround(upBBox.getLy())  // getLy() = "y1>=y0?y1-y0:0"
                     );
  if ((upSize.lx <= 0) || (upSize.ly <= 0)) {
    return;
  }

  /* ------ upのメモリ確保と描画 ---------------------------- */
  TTile upTile;
  this->m_up->allocateAndCompute(upTile, upBBox.getP00(), upSize,
                                 tile.getRaster() /* 32/64bitsの判定に使う */
                                 ,
                                 frame, rs);
  /* ------ upとdownのTRasterを得る ------------------------- */
  TRectI dnRect(upTile.getRaster()->getSize());  // TDimensionI(-)

  dnRect += convert(upTile.m_pos - tile.m_pos); /* uptile->tile原点 */
  /*
  ここで問題はdoubleの位置を、四捨五入して整数値にしていること
  移動してから四捨五入ではないの???
  dnRectの元位置が整数位置なので、問題ないか...
  */

  dn_ras = upComputesWholeTile ? tile.getRaster()
                               : tile.getRaster()->extract(dnRect);
  up_ras = upTile.getRaster();
  assert(dn_ras->getSize() == up_ras->getSize());
}
void ino_blend_soft_light::dryComputeUpAndDown(TRectD &rect, double frame,
                                               const TRenderSettings &rs,
                                               bool upComputesWholeTile) {
  const bool up_is = (this->m_up.isConnected() &&
                      this->m_up.getFx()->getTimeRegion().contains(frame));
  const bool down_is = (this->m_down.isConnected() &&
                        this->m_down.getFx()->getTimeRegion().contains(frame));
  /* ------ 両方とも切断の時処理しない ---------------------- */
  if (!up_is && !down_is) {
    return;
  }
  /* ------ up接続かつdown切断の時 -------------------------- */
  if (up_is && !down_is) {
    this->m_up->dryCompute(rect, frame, rs);
    return;
  }
  /* ------ down接続時 -------------------------------------- */
  if (down_is) {
    this->m_down->dryCompute(rect, frame, rs);
  }
  /* ------ up切断時 ---------------------------------------- */
  if (!up_is) {
    return;
  }

  /* ------ tileのgeometryを計算する ------------------------ */
  TRectD upBBox;

  if (upComputesWholeTile) {
    upBBox = rect;
  } else {
    this->m_up->getBBox(frame, upBBox, rs);
    upBBox *= rect;
    makeRectCoherent(upBBox, rect.getP00());
  }
  if ((upBBox.getLx() > 0.5) && (upBBox.getLy() > 0.5)) {
    this->m_up->dryCompute(upBBox, frame, rs);
  }
}
//------------------------------------------------------------
#include <sstream> /* std::ostringstream */
#include "igs_color_blend.h"
namespace {
template <class T, class Q>
void tmpl_(TRasterPT<T> dn_ras_out, const TRasterPT<T> &up_ras,
           const double up_opacity, const bool clipping_mask_sw) {
  double maxi = static_cast<double>(T::maxChannelValue);  // 255or65535

  assert(dn_ras_out->getSize() == up_ras->getSize());

  for (int yy = 0; yy < dn_ras_out->getLy(); ++yy) {
    T *out_pix             = dn_ras_out->pixels(yy);
    const T *const out_end = out_pix + dn_ras_out->getLx();
    const T *up_pix        = up_ras->pixels(yy);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      double upr = static_cast<double>(up_pix->r) / maxi;
      double upg = static_cast<double>(up_pix->g) / maxi;
      double upb = static_cast<double>(up_pix->b) / maxi;
      double upa = static_cast<double>(up_pix->m) / maxi;
      double dnr = static_cast<double>(out_pix->r) / maxi;
      double dng = static_cast<double>(out_pix->g) / maxi;
      double dnb = static_cast<double>(out_pix->b) / maxi;
      double dna = static_cast<double>(out_pix->m) / maxi;
      igs::color::soft_light(dnr, dng, dnb, dna, upr, upg, upb, upa,
                             clipping_mask_sw ? up_opacity * dna : up_opacity);
      out_pix->r = static_cast<Q>(dnr * (maxi + 0.999999));
      out_pix->g = static_cast<Q>(dng * (maxi + 0.999999));
      out_pix->b = static_cast<Q>(dnb * (maxi + 0.999999));
      out_pix->m = static_cast<Q>(dna * (maxi + 0.999999));
    }
  }
}
void fx_(TRasterP &dn_ras_out, const TRasterP &up_ras, const TPoint &pos,
         const double up_opacity, const bool clipping_mask_sw) {
  /* 交差したエリアを処理するようにする、いるのか??? */
  TRect outRect(dn_ras_out->getBounds());
  TRect upRect(up_ras->getBounds() + pos);
  TRect intersection = outRect * upRect;
  if (intersection.isEmpty()) return;

  TRasterP cRout = dn_ras_out->extract(intersection);
  TRect rr       = intersection - pos;
  TRasterP cRup  = up_ras->extract(rr);

  TRaster32P rout32 = cRout, rup32 = cRup;
  TRaster64P rout64 = cRout, rup64 = cRup;

  if (rout32 && rup32) {
    tmpl_<TPixel32, UCHAR>(rout32, rup32, up_opacity, clipping_mask_sw);
  } else if (rout64 && rup64) {
    tmpl_<TPixel64, USHORT>(rout64, rup64, up_opacity, clipping_mask_sw);
  } else {
    throw TRopException("unsupported pixel type");
  }
}
}
void ino_blend_soft_light::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &rs) {
  /* ------ 画像生成 ---------------------------------------- */
  TRasterP dn_ras, up_ras;
  this->computeUpAndDown(tile, frame, rs, dn_ras, up_ras);
  if (!dn_ras || !up_ras) {
    return;
  }
  /* ------ 動作パラメータを得る ---------------------------- */
  const double up_opacity =
      this->m_opacity->getValue(frame) / ino::param_range();
  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  up_opacity " << up_opacity << "   dn_tile w " << dn_ras->getLx()
       << "  wrap " << dn_ras->getWrap() << "  h " << dn_ras->getLy()
       << "  pixbits " << ino::pixel_bits(dn_ras) << "   up_tile w "
       << up_ras->getLx() << "  wrap " << up_ras->getWrap() << "  h "
       << up_ras->getLy() << "  pixbits " << ino::pixel_bits(up_ras)
       << "   frame " << frame;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    if (dn_ras) {
      dn_ras->lock();
    }
    if (up_ras) {
      up_ras->lock();
    }
    fx_(dn_ras, up_ras, TPoint(), up_opacity,
        this->m_clipping_mask->getValue());
    if (up_ras) {
      up_ras->unlock();
    }
    if (dn_ras) {
      dn_ras->unlock();
    }
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::exception &e) {
    if (up_ras) {
      up_ras->unlock();
    }
    if (dn_ras) {
      dn_ras->unlock();
    }
    if (log_sw) {
      std::string str("exception <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (...) {
    if (up_ras) {
      up_ras->unlock();
    }
    if (dn_ras) {
      dn_ras->unlock();
    }
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}
