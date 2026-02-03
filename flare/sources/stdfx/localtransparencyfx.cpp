

#include "stdfx.h"
#include "tfxparam.h"
// #include "trop.h"
#include "tdoubleparam.h"
// #include "tnotanimatableparam.h"
#include "tpixelgr.h"
#include "trasterfx.h"

//-------------------------------------------------------------------

namespace {

//==================================================================

enum Status {
  StatusGood        = 0,
  OutOfTime         = 1 << 1,  // Non utilizzato
  Port0NotConnected = 1 << 2,
  Port1NotConnected = 1 << 3,
  NoPortsConnected  = Port0NotConnected | Port1NotConnected
};
inline Status operator|(const Status &l, const Status &r) {
  return Status(((int)l | (int)r));
}
inline int operator&(const Status &l) { return int(l); }
Status getFxStatus(const TRasterFxPort &port0, const TRasterFxPort &port1) {
  Status status = StatusGood;
  if (!port0.isConnected()) status = status | Port0NotConnected;
  if (!port1.isConnected()) status = status | Port1NotConnected;
  return status;
}

/*inline*/
template <typename T, typename Q, typename P>
void doLocalTransparency(TRasterPT<T> out, TRasterPT<T> src, TRasterPT<T> ref,
                         double transp) {
  out->lock();
  ref->lock();
  src->lock();

  T *outRow   = out->pixels();
  T *refRow   = ref->pixels();
  T *srcRow   = src->pixels();
  int outWrap = out->getWrap();
  int refWrap = ref->getWrap();
  int srcWrap = src->getWrap();

  T *outPix  = outRow;
  T *srcPix  = srcRow;
  T *refPix  = refRow;
  T *lastPix = outRow + outWrap * out->getLy();

  const int cropVal = T::maxChannelValue;
  double factor     = transp / (double)cropVal;

  while (outPix < lastPix) {
    T *endPix = outPix + out->getLx();
    while (outPix < endPix) {
      double local_transp = 1 - (Q::from(*refPix).value) * factor;
      if (local_transp > 0.0) {
        int val   = (int)(local_transp * srcPix->r + 0.5);
        outPix->r = (P)((val < cropVal) ? val : cropVal);
        val       = (int)(local_transp * srcPix->g + 0.5);
        outPix->g = (P)((val < cropVal) ? val : cropVal);
        val       = (int)(local_transp * srcPix->b + 0.5);
        outPix->b = (P)((val < cropVal) ? val : cropVal);
        val       = (int)(local_transp * srcPix->m + 0.5);
        outPix->m = (P)((val < cropVal) ? val : cropVal);
      } else {
        outPix->r = outPix->g = outPix->b = outPix->m = 0;
      }
      ++outPix;
      ++refPix;
      ++srcPix;
    }
    srcRow += srcWrap;
    outRow += outWrap;
    refRow += refWrap;
    srcPix = srcRow;
    outPix = outRow;
    refPix = refRow;
  }
  out->unlock();
  ref->unlock();
  src->unlock();
}

template <>
void doLocalTransparency<TPixelF, TPixelGRF, float>(TRasterFP out,
                                                    TRasterFP src,
                                                    TRasterFP ref,
                                                    double transp) {
  out->lock();
  ref->lock();
  src->lock();

  TPixelF *outRow = out->pixels();
  TPixelF *refRow = ref->pixels();
  TPixelF *srcRow = src->pixels();
  int outWrap     = out->getWrap();
  int refWrap     = ref->getWrap();
  int srcWrap     = src->getWrap();

  TPixelF *outPix  = outRow;
  TPixelF *srcPix  = srcRow;
  TPixelF *refPix  = refRow;
  TPixelF *lastPix = outRow + outWrap * out->getLy();

  while (outPix < lastPix) {
    TPixelF *endPix = outPix + out->getLx();
    while (outPix < endPix) {
      // clamp 0.f to 1.f in case computing HDR
      float refv =
          std::min(1.f, std::max(0.f, (TPixelGRF::from(*refPix).value)));
      float local_transp = 1.f - refv * transp;
      if (local_transp > 0.f) {
        outPix->r = local_transp * srcPix->r;
        outPix->g = local_transp * srcPix->g;
        outPix->b = local_transp * srcPix->b;
        outPix->m = local_transp * srcPix->m;
      } else {
        outPix->r = outPix->g = outPix->b = outPix->m = 0.f;
      }
      ++outPix;
      ++refPix;
      ++srcPix;
    }
    srcRow += srcWrap;
    outRow += outWrap;
    refRow += refWrap;
    srcPix = srcRow;
    outPix = outRow;
    refPix = refRow;
  }
  out->unlock();
  ref->unlock();
  src->unlock();
}
//-------------------------------------------------------------------

inline double func(int x, int y, int lx, int ly) {
  return ((1 + sin(3.14 * x / (0.5 * lx))) * (1 + cos(3.14 * y / (0.5 * ly))));
}

void drawCheckboard(TRaster32P &raster) {
  int lx = raster->getLx();
  int ly = raster->getLy();
  // int chessSize = 4;
  int x, y;
  raster->lock();
  for (y = 0; y < ly; ++y) {
    TPixel32 *pix = raster->pixels(y);
    for (x = 0; x < lx; ++x, ++pix) {
      pix->r = pix->g = pix->b =
          troundp(255.0 * func(x, y, lx, ly) / func(lx - 1, ly - 1, lx, ly));
      pix->m = 255;
    }
  }
  raster->unlock();
}
};  // namespace

//-------------------------------------------------------------------

class LocalTransparencyFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(LocalTransparencyFx)
protected:
  TRasterFxPort m_src, m_ref;
  TDoubleParamP m_value;

public:
  LocalTransparencyFx() : m_value(100) {
    addInputPort("Source", m_src);
    addInputPort("Reference", m_ref);
    bindParam(this, "value", m_value);
    m_value->setValueRange(0, 100);
    enableComputeInFloat(true);
  }
  virtual ~LocalTransparencyFx() {}

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_src.isConnected())
      return m_src->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!checkBeforeCompute(tile, frame, ri)) return;
    TTile srcTile;
    m_src->allocateAndCompute(srcTile, tile.m_pos, tile.getRaster()->getSize(),
                              tile.getRaster(), frame, ri);
    // TTile refTile = tile;
    m_ref->compute(tile, frame, ri);

    // TRaster32P ref32 (refTile.getRaster());
    TRaster32P out32(tile.getRaster());
    TRaster32P src32(srcTile.getRaster());
    if (out32 && src32) {
      doLocalTransparency<TPixelRGBM32, TPixelGR8, UCHAR>(
          out32, src32, out32, m_value->getValue(frame) / 100.);
      return;
    }
    // TRaster64P ref64(refTile.getRaster());
    TRaster64P out64(tile.getRaster());
    TRaster64P src64(srcTile.getRaster());
    if (out64 && src64) {
      doLocalTransparency<TPixelRGBM64, TPixelGR16, USHORT>(
          out64, src64, out64, m_value->getValue(frame) / 100.);
      return;
    }
    TRasterFP outF(tile.getRaster());
    TRasterFP srcF(srcTile.getRaster());
    if (outF && srcF) {
      doLocalTransparency<TPixelF, TPixelGRF, float>(
          outF, srcF, outF, m_value->getValue(frame) / 100.);
      return;
    }

    throw TException("LocalTransparencyFx: unsupported raster type");
  }

  bool checkBeforeCompute(TTile &tile, double frame,
                          const TRenderSettings &info) {
    Status status = getFxStatus(m_src, m_ref);
    if ((status & NoPortsConnected) == NoPortsConnected) return false;
    if ((status & OutOfTime) == OutOfTime) return false;
    if ((status & Port0NotConnected) == Port0NotConnected) return false;
    if ((status & Port1NotConnected) == Port1NotConnected) {
      m_src->compute(tile, frame, info);
      return false;
    }
    assert(status == StatusGood);
    return true;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return TRasterFx::memorySize(rect, info.m_bpp);
  }
};

//-------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(LocalTransparencyFx, "localTransparencyFx")
