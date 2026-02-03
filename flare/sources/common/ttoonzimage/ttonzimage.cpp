

#include "ttoonzimage.h"
#include "tpalette.h"
//#include "ttile.h"
//#include "transparencyCheck.h"

//---------------------------------------------------------

TToonzImage::TToonzImage()
    : m_ras()
    , m_dpix(0)
    , m_dpiy(0)
    , m_subsampling(1)
    , m_name("")
    , m_savebox()
    //, m_hPos(0.0)
    , m_offset(0, 0)
    , m_size() {}

//---------------------------------------------------------

TToonzImage::TToonzImage(const TRasterCM32P &ras, const TRect &saveBox)
    : m_ras(ras)
    , m_size(ras->getSize())
    , m_dpix(0)
    , m_dpiy(0)
    , m_subsampling(1)
    , m_name("")
    , m_savebox(saveBox)
    //, m_hPos(0.0)
    , m_offset(0, 0) {
  assert(ras->getBounds().contains(saveBox));
}

//---------------------------------------------------------

TToonzImage::TToonzImage(const TToonzImage &src)
    : m_ras(TRasterCM32P())
    , m_dpix(src.m_dpix)
    , m_dpiy(src.m_dpiy)
    , m_subsampling(src.m_subsampling)
    , m_name(src.m_name)
    , m_savebox(src.m_savebox)
    //, m_hPos(src.m_hPos)
    , m_offset(src.m_offset)
    , m_size(src.m_size) {
  if (src.m_ras)  // src non e' compressa
  {
    m_ras = src.m_ras->clone();
  } else {
    assert(false);
    assert(!src.m_ras);
  }

  TPalette *palette = src.getPalette();
  if (palette) {
    m_palette = palette->clone();
    m_palette->addRef();
  } else
    m_palette = 0;
}

//---------------------------------------------------------

TToonzImageP TToonzImage::clone() const { return TToonzImageP(cloneImage()); }

//---------------------------------------------------------

TToonzImage::~TToonzImage() {}

//---------------------------------------------------------

TImage *TToonzImage::cloneImage() const { return new TToonzImage(*this); }

//=========================================================

void TToonzImage::setSubsampling(int s) { m_subsampling = s; }

//---------------------------------------------------------

void TToonzImage::setSavebox(const TRect &rect) {
  QMutexLocker sl(&m_mutex);

  // assert(m_lockCount>0);
  assert(m_ras);
  m_savebox = TRect(m_size) * rect;
  assert(TRect(m_size).contains(m_savebox));
}

//---------------------------------------------------------

namespace {

//---------------------------------------------------------
// il clipRect fa riferimento alle coordinate di rasIn
// se clipRect.isEmpty() allora aggiorna tutto

/*
void doUpdateRas32(const TRaster32P   &rasOut,
                   const TRasterCM32P &rasIn,
                   TPaletteP palette,
                   const TRect &theClipRect,
                                                                         bool
transparencyCheck,
                                                                         bool
applyFx)
{
  assert(palette);
  assert(rasIn && rasOut);

  applyFx = true;

  TRect clipRect(theClipRect);
  if(clipRect.isEmpty())
    clipRect = rasIn->getBounds();
  else
    {
     if(!clipRect.overlaps(rasIn->getBounds())) return;
     clipRect = clipRect * rasIn->getBounds();
    }
  if (clipRect.isEmpty())
    return;

        TRect clipRectIn, clipRectOut;

        if (applyFx && palette->getFxRects(clipRect, clipRectIn, clipRectOut))
          {
                TRect rAux = clipRectIn;
                TRasterP rAuxIn = rasIn->extract(clipRectIn);
                if (rAux!=clipRectIn && rAux!=rasIn->getBounds())
                  {
                        TRasterCM32P rNew(rAux.getSize());
                        TRect tmpRect = clipRectIn - rAux.getP00();
      rNew->extract(tmpRect)->copy(rAuxIn);
      rAuxIn = rNew;
                        clipRectIn = rAux;
                        }
    TTile tileIn(rAuxIn, convert(clipRectIn.getP00()));

                rAux = clipRectOut;
    TRasterP rAuxOut = rasOut->extract(clipRectOut);
    TTile tileOut(rAuxOut, convert(clipRectOut.getP00()));
    TRop::convert(tileOut, tileIn, palette, transparencyCheck, true);
    }
        else
          {
                TRect clipRectIn = clipRect;
                TRect clipRectOut = clipRect;
                TRasterP _rasOut = rasOut->extract(clipRectOut);
                TRasterP _rasIn;

                _rasIn = rasIn->extract(clipRectIn);

    TRop::convert(TTile(_rasOut, convert(clipRectOut.getP00())),
                              TTile(_rasIn,  convert(clipRectIn.getP00())),
                                                                        palette,
transparencyCheck, false);
                }
}

*/

}  // anonymous namespace

//---------------------------------------------------------
/*
TRaster32P TToonzImage::getRGBM(bool readOnly, bool computeFx)
{

  if (readOnly)
  {
    QMutexLocker sl(m_mutex);

    if (m_ras32)
      return m_ras32->clone();

    TRasterCM32P rasCM32;
    if (m_ras)
      rasCM32 = m_ras;
    else
      rasCM32 = getCMapped(true);

    assert(rasCM32);

    TPalette *palette = getPalette();
    assert(palette);

    TRaster32P ras32(rasCM32->getSize());
    const TPixel32 bgColor(255,255,255,0);
    ras32->fillOutside(m_savebox, bgColor);

    doUpdateRas32(ras32, rasCM32, palette, m_savebox, false, computeFx);
    return ras32;
  }
  else
  {
    QMutexLocker sl(m_mutex);
    return m_ras32;
  }
}
*/
//---------------------------------------------------------

TRasterCM32P TToonzImage::getCMapped(/*bool readOnly*/) const {
  /*
if (readOnly)
{
{
QMutexLocker sl(m_mutex);
if (m_ras)
  return m_ras->clone();
}

return decompress();
}
else
*/
  {
    // QMutexLocker sl(&m_mutex);
    return m_ras;
  }
}

//---------------------------------------------------------

void TToonzImage::setCMapped(const TRasterCM32P &ras) {
  QMutexLocker sl(&m_mutex);
  m_ras     = ras;
  m_size    = ras->getSize();
  m_savebox = ras->getBounds();
  // delete [] m_compressedBuffer;
  // m_compressedBuffer = 0;
  // m_compressedBufferSize = 0;
}

//---------------------------------------------------------

void TToonzImage::getCMapped(const TRasterCM32P &ras) {
  assert(ras && ras->getSize() == m_size);

  QMutexLocker sl(&m_mutex);
  if (m_ras) ras->copy(m_ras);
  /*
else
decompress(ras);
*/
}
