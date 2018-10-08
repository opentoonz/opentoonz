

#include "trop.h"
#include "tconvert.h"
//#include "trastercm.h"
#ifndef TNZCORE_LIGHT
#include "timagecache.h"
#include "ttile.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#endif

TString TRopException::getMessage() const { return ::to_wstring(message); }

namespace {

bool isOpaque32(TRaster32P &ras) {
  ras->lock();
  UCHAR *m0 = &(ras->pixels()->m);
  if (0 < m0[0] && m0[0] < 255) return false;

  int wrap4      = ras->getWrap() * 4;
  int lx4        = ras->getLx() * 4;
  const UCHAR cm = *m0;
  int nrows      = ras->getLy();
  while (nrows-- > 0) {
    UCHAR *m1 = m0 + lx4;
    UCHAR *m  = m0;
    while (m < m1 && *m == cm) m += 4;
    if (m < m1) break;
    m0 += wrap4;
  }
  ras->unlock();
  return (nrows <= 0);

  // m_image->setOpaqueFlag(true);
}

}  // namespace

bool TRop::isOpaque(TRasterP ras) {
  TRaster32P ras32 = ras;
  if (ras32)
    return isOpaque32(ras32);
  else if (TRasterGR8P(ras))
    return true;
  else
    throw TRopException("isOpaque: unsupported pixel type");
}

#ifdef TNZ_MACHINE_CHANNEL_ORDER_MRGB
void TRop::swapRBChannels(const TRaster32P &r) {
  int lx = r->getLx();
  int y  = r->getLy();
  r->lock();
  while (--y >= 0) {
    TPixel32 *pix    = r->pixels(y);
    TPixel32 *endPix = pix + lx;
    while (pix < endPix) {
      std::swap(pix->r, pix->b);
      ++pix;
    }
  }
  r->unlock();
}
#endif

TRaster32P TRop::copyAndSwapRBChannels(const TRaster32P &srcRaster) {
  TRaster32P newRaster(srcRaster->getSize());
  int lx = srcRaster->getLx();
  int y  = srcRaster->getLy();
  srcRaster->lock();
  newRaster->lock();
  while (--y >= 0) {
    TPixel32 *pix    = srcRaster->pixels(y);
    TPixel32 *newpix = newRaster->pixels(y);
    TPixel32 *endPix = pix + lx;
    while (pix < endPix) {
      newpix->r = pix->b;
      newpix->g = pix->g;
      newpix->b = pix->r;
      newpix->m = pix->m;
      ++pix;
      ++newpix;
    }
  }
  srcRaster->unlock();
  newRaster->unlock();

  return newRaster;
}

void TRop::copy(TRasterP dst, const TRasterP &src) {
  assert(!((TRasterCM32P)src) || (TRasterCM32P)dst);
  if (dst->getPixelSize() == src->getPixelSize())
    dst->copy(src);
  else {
    if (dst->getBounds() != src->getBounds()) {
      TRect rect = dst->getBounds() * src->getBounds();
      if (rect.isEmpty()) return;
      TRop::convert(dst->extract(rect), src->extract(rect));
    } else
      TRop::convert(dst, src);
  }
}

//-------------------------------------------------------------------

namespace {
template <class Q>
class Gamma_Lut {
public:
  std::vector<Q> m_table;
  Gamma_Lut(int insteps, int outsteps, double gamma) {
    double inspace = (double)(insteps);
    for (int i = 0; i <= insteps; i++)
      m_table.push_back(
          (Q)((outsteps) * (pow(i / inspace, 1.0 / gamma)) + 0.5));
  }
};

template <class T, class Q>
void doGammaCorrect(TRasterPT<T> raster, double gamma) {
  Gamma_Lut<Q> lut(T::maxChannelValue, T::maxChannelValue, gamma);

  int j;
  for (j = 0; j < raster->getLy(); j++) {
    T *pix    = raster->pixels(j);
    T *endPix = pix + raster->getLx();
    while (pix < endPix) {
      pix->r = lut.m_table[pix->r];
      pix->b = lut.m_table[pix->b];
      pix->g = lut.m_table[pix->g];
      /*if(pix->m != T::maxChannelValue)
{
pix->r= pix->r*pix->m/T::maxChannelValue;
pix->g= pix->g*pix->m/T::maxChannelValue;
pix->b= pix->b*pix->m/T::maxChannelValue;
}*/
      *pix++;
    }
  }
}
template <class T, class Q>
void doGammaCorrectRGBM(TRasterPT<T> raster, double gammar, double gammag,
                        double gammab, double gammam) {
  Gamma_Lut<Q> lutr(T::maxChannelValue, T::maxChannelValue, gammar);
  Gamma_Lut<Q> lutg(T::maxChannelValue, T::maxChannelValue, gammag);
  Gamma_Lut<Q> lutb(T::maxChannelValue, T::maxChannelValue, gammab);
  Gamma_Lut<Q> lutm(T::maxChannelValue, T::maxChannelValue, gammam);
  int j;
  for (j = 0; j < raster->getLy(); j++) {
    T *pix    = raster->pixels(j);
    T *endPix = pix + raster->getLx();
    while (pix < endPix) {
      pix->r = lutr.m_table[pix->r];
      pix->g = lutg.m_table[pix->g];
      pix->b = lutb.m_table[pix->b];
      pix->m = lutm.m_table[pix->m];
      /*if(pix->m != T::maxChannelValue)
{
pix->r= pix->r*pix->m/T::maxChannelValue;
pix->g= pix->g*pix->m/T::maxChannelValue;
pix->b= pix->b*pix->m/T::maxChannelValue;
}*/
      *pix++;
    }
  }
}
}
//-------------------------------------------------------------------

void TRop::gammaCorrect(TRasterP raster, double gamma) {
  if (gamma <= 0) gamma = 0.01;
  raster->lock();

  if ((TRaster32P)raster)
    doGammaCorrect<TPixel32, UCHAR>(raster, gamma);
  else if ((TRaster64P)raster)
    doGammaCorrect<TPixel64, USHORT>(raster, gamma);
  else {
    raster->unlock();
    throw TRopException("isOpaque: unsupported pixel type");
  }
  raster->unlock();
}
//-------------------------------------------------------------------

void TRop::gammaCorrectRGBM(TRasterP raster, double gammar, double gammag,
                            double gammab, double gammam) {
  if (gammar <= 0) gammar = 0.01;
  if (gammag <= 0) gammag = 0.01;
  if (gammab <= 0) gammab = 0.01;
  if (gammam <= 0) gammam = 0.01;

  raster->lock();

  if ((TRaster32P)raster)
    doGammaCorrectRGBM<TPixel32, UCHAR>(raster, gammar, gammag, gammab, gammam);
  else if ((TRaster64P)raster)
    doGammaCorrectRGBM<TPixel64, USHORT>(raster, gammar, gammag, gammab,
                                         gammam);
  else {
    raster->unlock();
    throw TRopException("isOpaque: unsupported pixel type");
  }
  raster->unlock();
}
//-------------------------------------------------------------------

template <class T>
void doSetChannel(const TRasterPT<T> &rin, const TRasterPT<T> &rout,
                  UCHAR channel, bool greytones) {
  int lx = rin->getLx();
  int ly = rout->getLy();

  int i, j;
  for (i = 0; i < ly; i++) {
    T *pixin  = rin->pixels(i);
    T *pixout = rout->pixels(i);
    if (greytones || channel == TRop::MChan) {
      switch (channel) {
      case TRop::RChan:
        for (j = 0; j < lx; j++, pixin++, pixout++)
          pixout->r = pixout->g = pixout->b = pixout->m = pixin->r;
        break;
      case TRop::GChan:
        for (j = 0; j < lx; j++, pixin++, pixout++)
          pixout->r = pixout->g = pixout->b = pixout->m = pixin->g;
        break;
      case TRop::BChan:
        for (j = 0; j < lx; j++, pixin++, pixout++)
          pixout->r = pixout->g = pixout->b = pixout->m = pixin->b;
        break;
      case TRop::MChan:
        for (j = 0; j < lx; j++, pixin++, pixout++)
          pixout->r = pixout->g = pixout->b = pixout->m = pixin->m;
        break;
      default:
        assert(false);
      }
    } else {
      for (j = 0; j < lx; j++, pixin++, pixout++) {
        pixout->r = channel & TRop::RChan ? pixin->r : 0;
        pixout->b = channel & TRop::BChan ? pixin->b : 0;
        pixout->g = channel & TRop::GChan ? pixin->g : 0;
      }
    }
  }
}

//-------------------------------------------------------------------

void TRop::setChannel(const TRasterP &rin, TRasterP rout, UCHAR chan,
                      bool greytones) {
  assert(rin->getSize() == rout->getSize());

  rout->lock();

  if ((TRaster32P)rin && (TRaster32P)rout)
    doSetChannel<TPixel32>(rin, rout, chan, greytones);
  else if ((TRaster64P)rin && (TRaster64P)rout)
    doSetChannel<TPixel64>(rin, rout, chan, greytones);
  else {
    rout->unlock();
    throw TRopException("setChannel: unsupported pixel type");
  }

  rout->unlock();
}

//-------------------------------------------------------------------

TRasterP TRop::shrink(TRasterP rin, int shrink) {
  int pixelSize = rin->getPixelSize();

  int lx = (rin->getLx() - 1) / shrink + 1;
  int ly = (rin->getLy() - 1) / shrink + 1;

  TRasterP rout;

  if ((TRaster32P)rin)
    rout = TRaster32P(lx, ly);
  else if ((TRaster64P)rin)
    rout                      = TRaster64P(lx, ly);
  if ((TRasterCM32P)rin) rout = TRasterCM32P(lx, ly);
  if ((TRasterGR8P)rin) rout  = TRasterGR8P(lx, ly);

  int i, j;

  for (i = 0; i < ly; i++) {
    UCHAR *bufin =
        (UCHAR *)rin->getRawData() + (i * shrink) * rin->getWrap() * pixelSize;
    UCHAR *bufout =
        (UCHAR *)rout->getRawData() + i * rout->getWrap() * pixelSize;
    for (j = 0; j < lx; j++) {
      memcpy(bufout, bufin, pixelSize);
      bufin += shrink * pixelSize;
      bufout += pixelSize;
    }
  }
  return rout;
}

//-------------------------------------------------------------------

template <class T>
void doMakeStereoRaster(const TRasterPT<T> &rleft, const TRasterPT<T> &rright) {
  int lx = rleft->getLx();
  int ly = rright->getLy();

  for (int i = 0; i < ly; i++) {
    T *pixl = rleft->pixels(i);
    T *pixr = rright->pixels(i);

    for (int j = 0; j < lx; j++, pixl++, pixr++) {
      pixl->g = pixr->g;
      pixl->b = pixr->b;
    }
  }
}

//---------------------------------------

void TRop::makeStereoRaster(const TRasterP &left, const TRasterP &right) {
  assert(left->getSize() == right->getSize());

  left->lock();

  if ((TRaster32P)left && (TRaster32P)right)
    doMakeStereoRaster<TPixel32>(left, right);
  else if ((TRaster64P)left && (TRaster64P)right)
    doMakeStereoRaster<TPixel64>(left, right);
  else {
    left->unlock();
    throw TRopException("setChannel: unsupported pixel type");
  }

  left->unlock();
}

//-------------------------------------------------------------------
#ifndef TNZCORE_LIGHT

void TTile::addInCache(const TRasterP &raster) {
  if (!raster) {
    m_rasterId = "";
    return;
  }
  TRasterP rin;

  m_rasterId = TImageCache::instance()->getUniqueId();
  if (raster->getParent()) {
    rin = raster->getParent();
    unsigned long offs =
        (raster->getRawData() - raster->getParent()->getRawData()) /
        raster->getPixelSize();
    m_subRect =
        TRect(TPoint(offs % raster->getWrap(), offs / raster->getWrap()),
              raster->getSize());
  } else {
    m_subRect = raster->getBounds();
    rin       = raster;
  }

  if ((TRasterCM32P)rin)
    TImageCache::instance()->add(m_rasterId,
                                 TToonzImageP(rin, rin->getBounds()));
  else if ((TRaster32P)rin || (TRaster64P)rin)
    TImageCache::instance()->add(m_rasterId, TRasterImageP(rin));
  else if ((TRasterGR8P)rin || (TRasterGR16P)rin)
    TImageCache::instance()->add(m_rasterId, TRasterImageP(rin));
  else
    assert(false);
}

TTile::TTile(const TRasterP &raster) : m_pos(), m_subRect() {
  addInCache(raster);
}

TTile::TTile(const TRasterP &raster, TPointD pos) : m_pos(pos), m_subRect() {
  addInCache(raster);
}

void TTile::setRaster(const TRasterP &raster) {
  if (m_rasterId != "") TImageCache::instance()->remove(m_rasterId);
  m_subRect = TRect();
  addInCache(raster);
}

TTile::~TTile() {
  if (!m_rasterId.empty()) TImageCache::instance()->remove(m_rasterId);
}

#endif
