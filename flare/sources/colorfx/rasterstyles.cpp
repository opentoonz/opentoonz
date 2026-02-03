

#include "rasterstyles.h"
#include "trop.h"
#include "tvectorimage.h"
#include "traster.h"
#include "trastercm.h"
#include "tlevel_io.h"
#include "tenv.h"
//#include "tpixelutils.h"

//*************************************************************************************
//    TAirbrushRasterStyle  implementation
//*************************************************************************************

void TAirbrushRasterStyle::loadData(TInputStreamInterface &is) { is >> m_blur; }

//-----------------------------------------------------------------------------

void TAirbrushRasterStyle::saveData(TOutputStreamInterface &os) const {
  os << m_blur;
}

//-----------------------------------------------------------------------------

bool TAirbrushRasterStyle::compute(const Params &params) const {
  if (m_blur > 0.0) TRop::blur(params.m_r, params.m_r, m_blur, 0, 0, true);

  return true;
}

//-----------------------------------------------------------------------------

TColorStyle *TAirbrushRasterStyle::clone() const {
  TColorStyle *cs = new TAirbrushRasterStyle(*this);
  cs->assignNames(this);
  return cs;
}

//-----------------------------------------------------------------------------

void TAirbrushRasterStyle::makeIcon(const TDimension &d) {
  TFilePath dir = TEnv::getStuffDir() + "pixmaps";
  static TRasterP normalIc;
  if (normalIc == TRasterP())
    TImageReader::load(dir + "airbrush.bmp", normalIc);

  arrangeIcon(d, normalIc);
}

/*---------------------------------------------------------------------------*/

void TAirbrushRasterStyle::arrangeIcon(const TDimension &d,
                                       const TRasterP &normalIc) {
  if (normalIc == TRasterP()) {
    m_icon = TRaster32P(d);
    m_icon->fill(TPixel32::Red);
    return;
  }

  m_icon = TRasterP();

  if (d == TDimension(52, 52))
    m_icon = normalIc->clone();
  else {
    m_icon = TRaster32P(d);
    TRop::resample(m_icon, normalIc, TScale(d.lx / 52.0, d.ly / 52.0));
  }

  m_icon->lock();
  TPixel *buf  = (TPixel *)m_icon->getRawData();
  TPixel color = getMainColor();
  double rd    = (color.r - color.m) / 255.0;
  double gd    = (color.g - color.m) / 255.0;
  double bd    = (color.b - color.m) / 255.0;

  assert(m_icon->getWrap() == m_icon->getSize().lx);

  for (int i = 0; i < m_icon->getSize().lx * m_icon->getSize().ly; i++, buf++) {
    assert(buf->r == buf->g && buf->r == buf->b);

    if (buf->r < 255) {
      if (buf->r == 0)
        *buf = color;
      else {
        int val = 255 - buf->r;
        buf->r  = (UCHAR)(val * rd + 255);
        buf->g  = (UCHAR)(val * gd + 255);
        buf->b  = (UCHAR)(val * bd + 255);
      }
    }
  }
  m_icon->unlock();
}

//*************************************************************************************
//    TBlendRasterStyle  implementation
//*************************************************************************************

TColorStyle *TBlendRasterStyle::clone() const {
  return new TBlendRasterStyle(*this);
}

//-----------------------------------------------------------------------------

bool TBlendRasterStyle::compute(const Params &params) const {
  int i, j;
  TRasterGR8P r = (TRasterGR8P)params.m_r;
  assert(r);

  double factor = computeFactor(params);

  if (m_blur > 0.0) TRop::blur(params.m_r, params.m_r, m_blur, 0, 0, true);

  r->lock();
  for (i = 0; i < r->getLy(); i++) {
    TPixelGR8 *bufGr = r->pixels(i);
    for (j = 0; j < r->getLx(); j++, bufGr++)
      if (bufGr->value > 0) {
        double val   = bufGr->value * factor + 0.5;
        bufGr->value = (UCHAR)((val > 255) ? 255 : val);
      }
  }
  r->unlock();
  return true;
}

//-----------------------------------------------------------------------------
namespace {
bool inline doIsBorderPix(int index, TPixelCM32 *pix, TPixelGR8 *pixGr,
                          int paint) {
  return (pix->getPaint() != paint &&
          (pix->isPurePaint() || pixGr->value != 0));
}

//-----------------------------------------------------------------------------

bool inline isBorderPix(int index, TPixelCM32 *pix, int wrap, TPixelGR8 *pixGr,
                        int wrapGr, int color) {
  return doIsBorderPix(index, pix + 1, pixGr + 1, color) ||
         doIsBorderPix(index, pix - 1, pixGr - 1, color) ||
         doIsBorderPix(index, pix + wrap, pixGr + wrapGr, color) ||
         doIsBorderPix(index, pix - wrap, pixGr - wrapGr, color);
}
}

/*---------------------------------------------------------------------------*/

void TBlendRasterStyle::makeIcon(const TDimension &d) {
  TFilePath dir = TEnv::getStuffDir() + "pixmaps";
  static TRasterP normalIc;
  if (normalIc == TRasterP()) TImageReader::load(dir + "blend.bmp", normalIc);

  arrangeIcon(d, normalIc);
}
//-----------------------------------------------------------------------------

double TBlendRasterStyle::computeFactor(const Params &params) const {
  TRasterCM32P rCm = (TRasterCM32P)params.m_rOrig;
  TRasterGR8P rGr  = (TRasterGR8P)params.m_r;
  TPixelGR8 *bufGr;
  TPixelCM32 *bufCmap;
  int i, j;
  int wr         = params.m_rOrig->getWrap();
  int wrGr       = params.m_r->getWrap();
  int num_lines  = 0;
  double tot_sum = 0;

  // interactive!
  // if (Factor == 0)
  rGr->lock();
  rCm->lock();
  for (i = 1; i < params.m_r->getLy() - 1; i++) {
    int line_sum = 0, num_points = 0;
    bufGr   = rGr->pixels(i) + 1;
    bufCmap = rCm->pixels(i) + 1;
    for (j = 1; j < rGr->getLx() - 1; j++, bufGr++, bufCmap++) {
      if (bufGr->value > 0) {
        int paint = bufCmap->getPaint();
        if ((bufCmap->isPurePaint() || bufGr->value != 0) &&
            isBorderPix(params.m_colorIndex, bufCmap, wr, bufGr, wrGr, paint)) {
          line_sum += troundp(bufGr->value);
          num_points++;
        }
      }
    }
    rGr->unlock();
    rCm->unlock();
    if (num_points != 0) {
      tot_sum += ((float)line_sum / ((float)num_points * 255));
      num_lines++;
    }
  }
  if (num_lines == 0) return 0;

  return num_lines / tot_sum;
}

//*************************************************************************************
//    TNoColorRasterStyle  implementation
//*************************************************************************************

void TNoColorRasterStyle::makeIcon(const TDimension &d) {
  TFilePath dir = TEnv::getStuffDir() + "pixmaps";
  static TRasterP normalIc;
  if (normalIc == TRasterP()) TImageReader::load(dir + "markup.bmp", normalIc);

  if (normalIc == TRasterP()) {
    m_icon = TRaster32P(d);
    m_icon->fill(TPixel32::Red);
    return;
  }

  m_icon = TRasterP();
  if (d == TDimension(52, 52) && normalIc != TRasterP())
    m_icon = normalIc->clone();
  else {
    m_icon = TRaster32P(d);
    TRop::resample(m_icon, normalIc, TScale(d.lx / 52.0, d.ly / 52.0));
  }
}
