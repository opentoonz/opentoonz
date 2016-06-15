

#include "texception.h"
#include "stdfx.h"
#include "tpixelutils.h"

//-------------------------------------------------------------------

class BacklitFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(BacklitFx)

  TRasterFxPort m_input;
  // TDoubleParamP m_value;

public:
  BacklitFx()
  //    , m_value (args, "Blur")
  {
    // m_value->setDefaultValue(100);
    addInputPort("Source", m_input);
  }

  ~BacklitFx(){};

  bool doGetBBox(double frame, TRectD &bBox);
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri);
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info);

  TRect getInvalidRect(const TRect &max);
};

//-------------------------------------------------------------------

bool BacklitFx::doGetBBox(double frame, TRectD &bBox) {
  if (m_input.isConnected()) {
    bool ret = m_input->doGetBBox(frame, bBox);
    // devo scurire bgColor
    return ret;
  } else {
    bBox = TRectD();
    return false;
  }
}

//===================================================================

namespace {

template <class T>
void computeBacklit(TRasterPT<T> dst, TRasterPT<T> src, const TPoint &srcPos,
                    // cordinate di src[0,0] rispetto a dst;
                    // in genere srcPos.x,.y <= 0
                    const TPoint &center
                    // coordinate della sorgente di luce rispetto a dst;
                    ) {
  // TRect srcRect = src->getBounds() + srcPos;
  // assert(srcRect.contains(dst->getBounds()));
  // assert(srcRect.contains(center));
  UINT max = T::maxChannelValue;
  dst->lock();
  for (int x = 0; x < dst->getLx(); x++)
    for (int y = 0; y < dst->getLy(); y++) {
      int dx   = x - center.x;
      int dy   = y - center.y;
      int dist = (int)sqrt((float)(dx * dx + dy * dy));

      int d        = tmin(100, dist) + 1;
      double value = 0.0;
      for (int i = 0; i <= d; i++) {
        int xx = center.x + (x - center.x) * i / d - srcPos.x;
        int yy = center.y + (y - center.y) * i / d - srcPos.y;
        assert(src->getBounds().contains(TPoint(xx, yy)));
        value += (max - src->pixels(yy)[xx].m) * 2;
      }
      value = value / dist;
      //    double value = sin(dist)<0?0:255;
      int v = tcrop((int)value, 0, (int)max);

      int srcX = -srcPos.x + x;
      int srcY = -srcPos.y + y;
      T srcPix(0, 0, 0, 0);
      if (src->getBounds().contains(TPoint(srcX, srcY)))
        srcPix = src->pixels(srcY)[srcX];
      srcPix.r = srcPix.r * (max - v) / max;
      srcPix.g = srcPix.g * (max - v) / max;
      srcPix.b = srcPix.b * (max - v) / max;

      dst->pixels(y)[x] = overPix(srcPix, T(v, v, v, v / 2));
    }
  dst->unlock();
}

}  // namespace

//---------------------------------------------------------------------

void BacklitFx::doCompute(TTile &tile, double frame,
                          const TRenderSettings &ri) {
  if (!m_input.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  TTile srcTile;
  srcTile.m_pos = tile.m_pos;
  TPoint srcPos(0, 0);
  TDimension srcSize = tile.getRaster()->getSize();

  TPoint center(-(int)tile.m_pos.x, -(int)tile.m_pos.y);
  center += TPoint(10, 10);
  if (center.x < 0) {
    srcSize.lx += -center.x;
    srcPos.x += center.x;
    srcTile.m_pos.x += center.x;
  } else if (center.x >= srcSize.lx) {
    srcSize.lx += center.x - srcSize.lx + 1;
  }

  if (center.y < 0) {
    srcSize.ly += -center.y;
    srcPos.y += center.y;
    srcTile.m_pos.y += center.y;
  } else if (center.y >= srcSize.ly) {
    srcSize.ly += center.y - srcSize.ly + 1;
  }

  if ((TRaster32P)tile.getRaster()) {
    TRaster32P src = TRaster32P(srcSize);
    src->clear();
    srcTile.getRaster() = src;

    m_input->compute(srcTile, frame, ri);

    TRaster32P dst = tile.getRaster();
    assert(dst);
    computeBacklit<TPixel32>(dst, src, srcPos, center);
  } else if ((TRaster64P)tile.getRaster()) {
    TRaster64P src = TRaster64P(srcSize);
    src->clear();
    srcTile.getRaster() = src;

    m_input->compute(srcTile, frame, ri);

    TRaster64P dst = tile.getRaster();
    assert(dst);
    computeBacklit<TPixel64>(dst, src, srcPos, center);
  }

  // dst->copy(src,srcPos);
}

//---------------------------------------------------------------------

int BacklitFx::getMemoryRequirement(const TRectD &rect, double frame,
                                    const TRenderSettings &info) {
  return TRasterFx::memorySize(rect, frame, info);
}

//---------------------------------------------------------------------

TRect BacklitFx::getInvalidRect(const TRect &max) {
  if (!m_input.isConnected()) return TRect();
  TRect rect = m_input->getInvalidRect(max);
  TRect ris  = max;

  if (rect.x0 >= 0)
    ris.x0 = tmin(rect.x0, max.x1);
  else if (rect.x1 <= 0)
    ris.x1 = tmax(rect.x1, max.x0);
  if (rect.y0 >= 0)
    ris.y0 = tmin(rect.y0, max.y1);
  else if (rect.y1 <= 0)
    ris.y1 = tmax(rect.y1, max.y0);

  return ris;
}

// FX_PLUGIN_IDENTIFIER(BacklitFx        , "backlitFx")
