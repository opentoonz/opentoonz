

#include "trastercm.h"
#include "toonz/fill.h"
#include "tregion.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/toonzimageutils.h"
#include "skeletonlut.h"
#include "tpixelutils.h"

#include <stack>

using namespace SkeletonLut;

//-----------------------------------------------------------------------------
namespace {  // Utility Function
//-----------------------------------------------------------------------------

void computeSeeds(const TRasterCM32P &r, TStroke *stroke,
                  std::vector<std::pair<TPoint, int>> &seeds) {
  int length = (int)stroke->getLength();
  TRect bbox = r->getBounds();

  TPoint oldP;

  for (int i = 0; i < length; i++) {
    TPoint p = convert(stroke->getPointAtLength(i));
    if (p == oldP || !bbox.contains(p)) continue;
    seeds.push_back(
        std::pair<TPoint, int>(p, (r->pixels(p.y) + p.x)->getPaint()));
    oldP = p;
  }
}

//-----------------------------------------------------------------------------

void fillArea(const TRasterCM32P &ras, TRegion *r, int colorId,
              bool onlyUnfilled, bool fillPaints, bool fillInks) {
  TRect bbox = convert(r->getBBox());
  bbox *= ras->getBounds();
  ras->lock();

  for (int i = bbox.y0; i <= bbox.y1; i++) {
    TPixelCM32 *line = ras->pixels(i);
    std::vector<double> intersections;
    r->computeScanlineIntersections(i, intersections);
    assert(!(intersections.size() & 0x1));

    for (UINT j = 0; j < intersections.size(); j += 2) {
      if (intersections[j] == intersections[j + 1]) continue;
      int from        = std::max(tfloor(intersections[j]), bbox.x0);
      int to          = std::min(tceil(intersections[j + 1]), bbox.x1);
      TPixelCM32 *pix = line + from;
      for (int k = from; k < to; k++, pix++) {
        if (fillPaints && (!onlyUnfilled || pix->getPaint() == 0))
          pix->setPaint(colorId);
        if (fillInks) pix->setInk(colorId);
      }
    }
  }
  ras->unlock();
}

//-----------------------------------------------------------------------------

void restoreColors(const TRasterCM32P &r,
                   const std::vector<std::pair<TPoint, int>> &seeds) {
  FillParameters params;
  // in order to make the paint to protlude behind the line
  params.m_prevailing = false;
  for (UINT i = 0; i < seeds.size(); i++) {
    params.m_p       = seeds[i].first;
    params.m_styleId = seeds[i].second;
    fill(r, params);
  }
}

//-----------------------------------------------------------------------------

/*!
  Return true if all \b pixels in \b rect are pure paint; otherwise return
  false.
*/
bool areRectPixelsPurePaint(TPixelCM32 *pixels, TRect rect, int wrap) {
  int dx          = rect.x1 - rect.x0;
  TPixelCM32 *pix = pixels + rect.y0 * wrap + rect.x0;
  int x, y;
  for (y = rect.y0; y <= rect.y1; y++, pix += wrap - dx - 1)
    for (x = rect.x0; x <= rect.x1; x++, pix++)
      if (!pix->isPurePaint()) return false;
  return true;
}

//-----------------------------------------------------------------------------

/*!
  Return true if all \b pixels in \b rect are transparent; otherwise return
  false.
*/
bool areRectPixelsTransparent(TPixel32 *pixels, TRect rect, int wrap) {
  int dx        = rect.x1 - rect.x0;
  TPixel32 *pix = pixels + rect.y0 * wrap + rect.x0;
  int x, y;
  for (y = rect.y0; y <= rect.y1; y++, pix += wrap - dx - 1)
    for (x = rect.x0; x <= rect.x1; x++, pix++)
      if (pix->m <= 0) return false;
  return true;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// AreaFiller

AreaFiller::AreaFiller(const TRasterCM32P &ras)
    : m_ras(ras)
    , m_bounds(ras->getBounds())
    , m_pixels(ras->pixels())
    , m_wrap(ras->getWrap())
    , m_color(0) {
  m_ras->lock();
}

//-----------------------------------------------------------------------------

AreaFiller::~AreaFiller() { m_ras->unlock(); }

//-----------------------------------------------------------------------------
// questa funzione viene chiamata dopo il fill rect delle aree, e colora gli
// inchiostri di tipo "autoink"
// che confinano con le aree appena fillate con il rect. rbefore e' il rect del
// raster prima del rectfill.
void fillautoInks(TRasterCM32P &rin, TRect &rect, const TRasterCM32P &rbefore,
                  TPalette *plt) {
  assert(plt);
  TRasterCM32P r = rin->extract(rect);
  assert(r->getSize() == rbefore->getSize());
  int i, j;

  for (i = 0; i < r->getLy(); i++) {
    TPixelCM32 *pix  = r->pixels(i);
    TPixelCM32 *pixb = rbefore->pixels(i);
    for (j = 0; j < r->getLx(); j++, pix++, pixb++) {
      int paint = pix->getPaint();
      int tone  = pix->getTone();
      int ink   = pix->getInk();
      if (paint != pixb->getPaint() && tone > 0 && tone < 255 && ink != paint &&
          plt->getStyle(ink)->getFlags() != 0)
        inkFill(rin, TPoint(j, i) + rect.getP00(), paint, 0, NULL, &rect);
    }
  }
}

//-----------------------------------------------------------------------------

void AreaFiller::rectFill(const TRect &rect, int color, bool onlyUnfilled,
                          bool fillPaints, bool fillInks) {
  // Viene trattato il caso fillInks
  /*- FillInkのみの場合 -*/
  if (!fillPaints) {
    assert(fillInks);
    assert(m_ras->getBounds().contains(rect));
    for (int y = rect.y0; y <= rect.y1; y++) {
      TPixelCM32 *pix = m_ras->pixels(y) + rect.x0;
      for (int x = rect.x0; x <= rect.x1; x++, pix++) pix->setInk(color);
    }
    return;
  }

  TRect r = m_bounds * rect;

  int dx = r.x1 - r.x0;
  int dy = (r.y1 - r.y0) * m_wrap;
  if (dx < 2 || dy < 2)  // rect degenere(area contenuta nulla), skippo.
    return;

  std::vector<int> frameSeed(2 * (r.getLx() + r.getLy() - 2));

  int x, y, count1, count2;
  /*- ptrをRect範囲のスタート地点に移動 -*/
  Pixel *ptr = m_pixels + r.y0 * m_wrap + r.x0;
  count1     = 0;
  count2     = r.y1 - r.y0 + 1;

  // Se il rettangolo non contiene il bordo del raster e se tutti i pixels
  // contenuti nel rettangolo sono pure paint non deve fare nulla!
  if (!rect.contains(m_bounds) && areRectPixelsPurePaint(m_pixels, r, m_wrap))
    return;

  // Viene riempito frameSeed con tutti i paint delle varie aree del rettangolo
  // di contorno.
  // Viene verificato se i pixels del rettangolo sono tutti pure paint.
  /*- 輪郭のPaintのIDをframeseed内に格納 -*/
  for (y = r.y0; y <= r.y1; y++, ptr += m_wrap, count1++, count2++) {
    if (r.x0 > 0) frameSeed[count1]                  = ptr->getPaint();
    if (r.x1 < m_ras->getLx() - 1) frameSeed[count2] = (ptr + dx)->getPaint();
  }
  ptr    = m_pixels + r.y0 * m_wrap + r.x0 + 1;
  count1 = count2;
  count2 = count1 + r.x1 - r.x0 - 1;
  for (x = r.x0 + 1; x < r.x1; x++, ptr++, count1++, count2++) {
    if (r.y0 > 0) frameSeed[count1]                  = ptr->getPaint();
    if (r.y1 < m_ras->getLy() - 1) frameSeed[count2] = (ptr + dy)->getPaint();
  }
  assert(count2 == 2 * (r.getLx() + r.getLy() - 2));

  // Viene fillato l'interno e il bordo del rettangolo rect con color
  Pixel *pix = m_pixels + r.y0 * m_wrap + r.x0;
  if (onlyUnfilled)
    for (y = r.y0; y <= r.y1; y++, pix += m_wrap - dx - 1) {
      for (x = r.x0; x <= r.x1; x++, pix++) {
        if (pix->getPaint() == 0)  // BackgroundStyle
          pix->setPaint(color);
        if (fillInks) pix->setInk(color);
      }
    }
  else
    for (y = r.y0; y <= r.y1; y++, pix += m_wrap - dx - 1) {
      for (x = r.x0; x <= r.x1; x++, pix++) {
        pix->setPaint(color);
        if (fillInks) pix->setInk(color);
      }
    }

  // Vengono fillati i pixel del rettangolo con i paint (mantenuti in frameSeed)
  // che
  // c'erano prima di fillare l'intero rettangolo, in questo modo si riportano
  // al colore originale le aree che non sono chiuse e non dovevano essere
  // fillate.
  count1 = 0;
  FillParameters params;
  // in order to make the paint to protlude behind the line
  params.m_prevailing = false;
  if (r.x0 > 0)
    for (y = r.y0; y <= r.y1; y++) {
      params.m_p       = TPoint(r.x0, y);
      params.m_styleId = frameSeed[count1++];
      fill(m_ras, params);
    }
  else
    count1 += r.y1 - r.y0 + 1;

  if (r.x1 < m_ras->getLx() - 1)
    for (y = r.y0; y <= r.y1; y++) {
      params.m_p       = TPoint(r.x1, y);
      params.m_styleId = frameSeed[count1++];
      fill(m_ras, params);
    }
  else
    count1 += r.y1 - r.y0 + 1;

  if (r.y0 > 0)
    for (x = r.x0 + 1; x < r.x1; x++) {
      params.m_p       = TPoint(x, r.y0);
      params.m_styleId = frameSeed[count1++];
      fill(m_ras, params);
    }
  else
    count1 += r.x1 - r.x0 - 1;

  if (r.y1 < m_ras->getLy() - 1)
    for (x = r.x0 + 1; x < r.x1; x++) {
      params.m_p       = TPoint(x, r.y1);
      params.m_styleId = frameSeed[count1++];
      fill(m_ras, params);
    }
}

//-----------------------------------------------------------------------------

void AreaFiller::strokeFill(TStroke *stroke, int colorId, bool onlyUnfilled,
                            bool fillPaints, bool fillInks) {
  stroke->transform(TTranslation(convert(m_ras->getCenter())));
  m_ras->lock();

  std::vector<std::pair<TPoint, int>> seeds;
  computeSeeds(m_ras, stroke, seeds);

  TVectorImage app;
  app.addStroke(stroke);
  app.findRegions();
  for (UINT i = 0; i < app.getRegionCount(); i++)
    fillArea(m_ras, app.getRegion(i), colorId, onlyUnfilled, fillPaints,
             fillInks);
  app.removeStroke(0);

  stroke->transform(TTranslation(convert(-m_ras->getCenter())));
  restoreColors(m_ras, seeds);
  m_ras->unlock();
}

//=============================================================================
// FullColorAreaFiller

FullColorAreaFiller::FullColorAreaFiller(const TRaster32P &ras)
    : m_ras(ras)
    , m_bounds(ras->getBounds())
    , m_pixels(ras->pixels())
    , m_wrap(ras->getWrap())
    , m_color(0) {
  m_ras->lock();
}

//-----------------------------------------------------------------------------

FullColorAreaFiller::~FullColorAreaFiller() { m_ras->unlock(); }

//-----------------------------------------------------------------------------

void FullColorAreaFiller::rectFill(const TRect &rect,
                                   const FillParameters &params,
                                   bool onlyUnfilled) {
  TRect bbox = m_ras->getBounds();
  TRect r    = rect * bbox;
  if (r.isEmpty()) return;

  TRaster32P workRas = m_ras->extract(r);
  TRaster32P copy    = workRas->clone();

  TPixel32 color = params.m_palette->getStyle(params.m_styleId)->getMainColor();

  // Fillo tutto il quadaratino con color
  int x, y;
  for (y = 0; y < workRas->getLy(); y++) {
    TPixel32 *line = workRas->pixels(y);
    for (x        = 0; x < workRas->getLx(); x++)
      *(line + x) = overPix(color, workRas->pixels(y)[x]);
  }

  FillParameters paramsApp = params;
  TPixel32 refColor;
  for (y = 0; y < workRas->getLy(); y++) {
    paramsApp.m_p = TPoint(0, y);
    if (y == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }
  for (y = 0; y < workRas->getLy(); y++) {
    paramsApp.m_p = TPoint(workRas->getLx() - 1, y);
    if (y == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }

  for (x = 0; x < workRas->getLx(); x++) {
    paramsApp.m_p = TPoint(x, 0);
    if (x == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }
  for (x = 0; x < workRas->getLx(); x++) {
    paramsApp.m_p = TPoint(x, workRas->getLy() - 1);
    if (x == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }
}

//=============================================================================
// InkSegmenter

const int damInk = 3;

//-----------------------------------------------------------------------------

#define GROW_FACTOR 2.51

//-----------------------------------------------------------------------------

class InkSegmenter {
  int m_lx, m_ly, m_wrap;
  int m_displaceVector[8];
  TPixelCM32 *m_buf;
  TRect m_bBox;
  TRasterCM32P m_r;
  TTileSaverCM32 *m_saver;
  float m_growFactor;

public:
  InkSegmenter(const TRasterCM32P &r, float growFactor, TTileSaverCM32 *saver)
      : m_r(r)
      , m_lx(r->getLx())
      , m_ly(r->getLy())
      , m_wrap(r->getWrap())
      , m_buf((TPixelCM32 *)r->getRawData())
      , m_bBox(r->getBounds())
      , m_saver(saver)
      , m_growFactor(growFactor) {
    m_displaceVector[0] = -m_wrap - 1;
    m_displaceVector[1] = -m_wrap;
    m_displaceVector[2] = -m_wrap + 1;
    m_displaceVector[3] = -1;
    m_displaceVector[4] = +1;
    m_displaceVector[5] = m_wrap - 1;
    m_displaceVector[6] = m_wrap;
    m_displaceVector[7] = m_wrap + 1;
  }

  //-----------------------------------------------------------------------------

  bool compute(const TPoint &pin, int ink, bool isSelective) {
    TPixelCM32 *pix;
    int distance;
    TPixelCM32 *master;
    TPoint mp, sp;
    TPixelCM32 *slave;
    TPixelCM32 *d11, *d12, *d21, *d22;
    TPoint d1p1, d1p2, d2p1, d2p2;
    TPoint p = pin;

    if (!m_bBox.contains(p)) return false;

    if ((m_buf + p.y * m_wrap + p.x)->isPurePaint() &&
        ((p = nearestInk(p, 2)) == TPoint(-1, -1)))
      return false;

    pix = m_buf + p.y * m_wrap + p.x;

    /*-- 同じインクの場合はreturn --*/
    if (pix->getInk() == ink) return false;

    if (!ConnectionTable[neighboursCode(pix, p)]) {
      master = slave = pix;
      mp = sp  = p;
      distance = 0;
    } else
      distance = findTwinPoints(pix, p, master, mp, slave, sp);

    if (distance == -1) return false;

    if (!findDam(master, mp, slave, sp, distance, d11, d1p1, d12, d1p2))
      d11 = d12 = d21 = d22 = 0;
    else
      findDamRev(master, mp, slave, sp, distance, d21, d2p1, d22, d2p2);

    // vector<pair<TPixelCM32*, int> > oldInks;

    drawSegment(d1p1, d1p2, damInk, m_saver);
    drawSegment(d2p1, d2p2, damInk, m_saver);

    inkSegmentFill(p, ink, isSelective, m_saver);

    // UINT i;

    drawSegment(d1p1, d1p2, ink, m_saver);
    drawSegment(d2p1, d2p2, ink, m_saver);

    /*	for (i=0; i<oldInks.size(); i++)
    (oldInks[i].first)->setInk(ink);*/

    return true;
  }

private:
  void drawSegment(
      const TPoint &p0, const TPoint &p1, int ink,
      /*vector<pair<TPixelCM32*, int> >& oldInks,*/ TTileSaverCM32 *saver);

  int findTwinPoints(TPixelCM32 *pix, const TPoint &p, TPixelCM32 *&master,
                     TPoint &mp, TPixelCM32 *&slave, TPoint &sp);
  int searchForNearestSlave(TPixelCM32 *pix1, TPixelCM32 *pix2,
                            const TPoint &p1, TPoint &p2, TPixelCM32 *&slave,
                            TPoint &sp);
  int rearrangePoints(TPixelCM32 *&master, TPoint &mp, TPixelCM32 *&slave,
                      int s_prewalker, TPoint &sp, int walk);
  int rearrangePointsRev(TPixelCM32 *&master, TPoint &mp, TPixelCM32 *&slave,
                         int s_prewalker, TPoint &sp, int walk);
  int dragSlave(TPoint mp, TPixelCM32 *&slave, int &s_prewalker, TPoint &sp);
  int dragSlaveRev(TPoint mp, TPixelCM32 *&slave, int &s_prewalker, TPoint &sp,
                   TPixelCM32 *first_slave);
  bool findDam(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave, TPoint sp,
               int distance, TPixelCM32 *&d11, TPoint &d1p1, TPixelCM32 *&d12,
               TPoint &d1p2);
  void findDamRev(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave, TPoint sp,
                  int distance, TPixelCM32 *&d11, TPoint &d1p1,
                  TPixelCM32 *&d12, TPoint &d1p2);
  int nextPointIsGoodRev(TPoint mp, TPoint sp, TPixelCM32 *slave,
                         int s_prewalker, int distance);
  int nextPointIsGood(TPoint mp, TPoint sp, TPixelCM32 *slave, int s_prewalker,
                      int distance);
  void inkSegmentFill(const TPoint &p, int ink, bool isSelective,
                      TTileSaverCM32 *saver);
  TPoint nearestInk(const TPoint &p, int ray);
  inline int stepReversed(TPixelCM32 *walker, int prewalker, int &distance,
                          const TPoint &p1, TPoint &p2);
  inline int stepForward(TPixelCM32 *walker, int prewalker, int &distance,
                         const TPoint &p1, TPoint &p2);

  TPixelCM32 *ePix(TPixelCM32 *br) { return (br + 1); }
  TPixelCM32 *wPix(TPixelCM32 *br) { return (br - 1); }
  TPixelCM32 *nPix(TPixelCM32 *br) { return (br + m_wrap); }
  TPixelCM32 *sPix(TPixelCM32 *br) { return (br - m_wrap); }
  TPixelCM32 *swPix(TPixelCM32 *br) { return (br - m_wrap - 1); }
  TPixelCM32 *nwPix(TPixelCM32 *br) { return (br + m_wrap - 1); }
  TPixelCM32 *nePix(TPixelCM32 *br) { return (br + m_wrap + 1); }
  TPixelCM32 *sePix(TPixelCM32 *br) { return (br - m_wrap + 1); }

  UCHAR neighboursCode(TPixelCM32 *seed, const TPoint &p) {
    // assert(p == TPoint((seed-m_buf)%m_wrap, (seed-m_buf)/m_wrap));
    bool w = (p.x > 0), e = (p.x < m_lx - 1), s = (p.y > 0),
         n = (p.y < m_ly - 1);

    return (((s && w) ? ((!swPix(seed)->isPurePaint())) : 0) |
            ((s) ? ((!sPix(seed)->isPurePaint()) << 1) : 0) |
            ((s && e) ? ((!sePix(seed)->isPurePaint()) << 2) : 0) |
            ((w) ? ((!wPix(seed)->isPurePaint()) << 3) : 0) |
            ((e) ? ((!ePix(seed)->isPurePaint()) << 4) : 0) |
            ((n && w) ? ((!nwPix(seed)->isPurePaint()) << 5) : 0) |
            ((n) ? ((!nPix(seed)->isPurePaint()) << 6) : 0) |
            ((n && e) ? ((!nePix(seed)->isPurePaint()) << 7) : 0));
  }
};

//-----------------------------------------------------------------------------

#define DRAW_SEGMENT(a, b, da, db, istr1, istr2, block)                        \
  {                                                                            \
    d      = 2 * db - da;                                                      \
    incr_1 = 2 * db;                                                           \
    incr_2 = 2 * (db - da);                                                    \
    while (a < da) {                                                           \
      if (d <= 0) {                                                            \
        d += incr_1;                                                           \
        a++;                                                                   \
        istr1;                                                                 \
      } else {                                                                 \
        d += incr_2;                                                           \
        a++;                                                                   \
        b++;                                                                   \
        istr2;                                                                 \
      }                                                                        \
      block;                                                                   \
    }                                                                          \
  }
#define SET_INK                                                                \
  {                                                                            \
    if (saver) saver->save(TPoint(x1 + x, y1 + y));                            \
    /*if (buf->getInk()!=damInk)*/                                             \
    /*  oldInks.push_back(pair<TPixelCM32*, int>(buf, buf->getInk()));*/       \
    buf->setInk(ink);                                                          \
  }

//-----------------------------------------------------------------------------

void InkSegmenter::drawSegment(
    const TPoint &p0, const TPoint &p1, int ink,
    /*vector<pair<TPixelCM32*, int> >& oldInks,*/ TTileSaverCM32 *saver) {
  int x, y, dx, dy, d, incr_1, incr_2;

  int x1 = p0.x;
  int y1 = p0.y;
  int x2 = p1.x;
  int y2 = p1.y;

  if (x1 > x2) {
    tswap(x1, x2);
    tswap(y1, y2);
  }

  TPixelCM32 *buf = m_r->pixels() + y1 * m_wrap + x1;
  /*if (buf->getInk()!=damInk)
          oldInks.push_back(pair<TPixelCM32*, int>(buf, buf->getInk()));
  if ((m_r->pixels() + y2*m_wrap + x2)->getInk()!=damInk)
          oldInks.push_back(pair<TPixelCM32*, int>(m_r->pixels() + y2*m_wrap +
  x2, (m_r->pixels() + y2*m_wrap + x2)->getInk()));*/

  if (saver) {
    saver->save(p0);
    saver->save(p1);
  }

  buf->setInk(ink);
  (m_r->pixels() + y2 * m_wrap + x2)->setInk(ink);

  dx = x2 - x1;
  dy = y2 - y1;

  x = y = 0;

  if (dy >= 0) {
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (buf++), (buf += m_wrap + 1), SET_INK)
    else
      DRAW_SEGMENT(y, x, dy, dx, (buf += m_wrap), (buf += m_wrap + 1), SET_INK)
  } else {
    dy = -dy;
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (buf++), (buf -= (m_wrap - 1)), SET_INK)
    else
      DRAW_SEGMENT(y, x, dy, dx, (buf -= m_wrap), (buf -= (m_wrap - 1)),
                   SET_INK)
  }
}

//-----------------------------------------------------------------------------

void InkSegmenter::inkSegmentFill(const TPoint &p, int ink, bool isSelective,
                                  TTileSaverCM32 *saver) {
  int x = p.x, y = p.y;
  int lx             = m_r->getLx();
  int ly             = m_r->getLy();
  TPixelCM32 *pixels = (TPixelCM32 *)m_r->getRawData();
  TPixelCM32 *pix    = pixels + p.y * m_wrap + x;
  int oldInk;

  if (pix->isPurePaint() || pix->getInk() == ink) return;

  if (isSelective) oldInk = pix->getInk();

  std::stack<TPoint> seeds;
  seeds.push(p);

  while (!seeds.empty()) {
    TPoint seed = seeds.top();
    seeds.pop();
    // if(!m_r->getBounds().contains(seed)) continue;
    x               = seed.x;
    y               = seed.y;
    TPixelCM32 *pix = pixels + (y * m_wrap + x);
    if (pix->isPurePaint() || pix->getInk() == ink || pix->getInk() == damInk ||
        (isSelective && pix->getInk() != oldInk))
      continue;

    if (saver) saver->save(seed);

    pix->setInk(ink);

    if (x > 0) seeds.push(TPoint(x - 1, y));
    if (y > 0) seeds.push(TPoint(x, y - 1));
    if (y < ly - 1) seeds.push(TPoint(x, y + 1));
    if (x < lx - 1) seeds.push(TPoint(x + 1, y));

    if (x == lx - 1 || x == 0 || y == ly - 1 || y == 0) continue;

    if (ePix(pix)->getInk() == damInk || wPix(pix)->getInk() == damInk ||
        sPix(pix)->getInk() == damInk || nPix(pix)->getInk() == damInk ||
        nePix(pix)->getInk() == damInk || sePix(pix)->getInk() == damInk ||
        swPix(pix)->getInk() == damInk || nwPix(pix)->getInk() == damInk)
      continue;

    seeds.push(TPoint(x - 1, y - 1));
    seeds.push(TPoint(x - 1, y + 1));
    seeds.push(TPoint(x + 1, y - 1));
    seeds.push(TPoint(x + 1, y + 1));
  }
}

//-----------------------------------------------------------------------------

TPoint InkSegmenter::nearestInk(const TPoint &p, int ray) {
  int i, j;

  for (j = std::max(p.y - ray, 0); j <= std::min(p.y + ray, m_ly - 1); j++)
    for (i = std::max(p.x - ray, 0); i <= std::min(p.x + ray, m_lx - 1); i++)
      if (!(m_buf + j * m_wrap + i)->isPurePaint()) return TPoint(i, j);

  return TPoint(-1, -1);
}

//-----------------------------------------------------------------------------

int InkSegmenter::findTwinPoints(TPixelCM32 *pix, const TPoint &p,
                                 TPixelCM32 *&master, TPoint &mp,
                                 TPixelCM32 *&slave, TPoint &sp) {
  TPixelCM32 *row_p1, *col_p1, *row_p2, *col_p2;
  int distance;
  int row_x1, row_x2, col_y1, col_y2;

  row_p1 = pix - 1;
  row_x1 = p.x - 1;

  while (row_x1 + 1 < m_lx && !(row_p1 + 1)->isPurePaint()) {
    row_p1++;
    row_x1++;
  }

  row_p2 = pix + 1;
  row_x2 = p.x + 1;

  while (row_x2 - 1 > 0 && !(row_p2 - 1)->isPurePaint()) {
    row_p2--;
    row_x2--;
  }

  master = row_p1;
  mp.x   = row_x1;
  mp.y   = p.y;

  col_p1 = pix - m_wrap;
  col_y1 = p.y - 1;

  while (col_y1 + 1 < m_ly && !(col_p1 + m_wrap)->isPurePaint()) {
    col_p1 += m_wrap;
    col_y1++;
  }

  col_p2 = pix + m_wrap;
  col_y2 = p.y + 1;

  while (col_y2 - 1 > 0 && !(col_p2 - 1)->isPurePaint()) {
    col_p2 -= m_wrap;
    col_y2--;
  }

  if (row_x1 - row_x2 <= col_y1 - col_y2) {
    master = row_p1;
    mp     = TPoint(row_x1, p.y);

    TPoint auxp(row_x2, p.y);
    if ((distance =
             searchForNearestSlave(row_p1, row_p2, mp, auxp, slave, sp)) != 0)
      return distance;

    master = col_p1;
    mp     = TPoint(p.x, col_y1);

    auxp = TPoint(p.x, col_y2);
    if ((distance = searchForNearestSlave(col_p1, col_p2, mp, auxp, slave,
                                          sp)) == 0 /*&& !is_connecting(p1)*/)
      return -1;
  } else {
    master = col_p1;
    mp     = TPoint(p.x, col_y1);

    TPoint auxp(p.x, col_y2);
    if ((distance =
             searchForNearestSlave(col_p1, col_p2, mp, auxp, slave, sp)) != 0)
      return distance;

    master = row_p1;
    mp     = TPoint(row_x1, p.y);
    auxp   = TPoint(row_x2, p.y);
    if ((distance = searchForNearestSlave(row_p1, row_p2, mp, auxp, slave,
                                          sp)) == 0 /*&& !is_connecting(p1)*/)
      return -1;
  }

  return distance;
}

//-----------------------------------------------------------------------------

inline void newP(int next, TPoint &p) {
  switch (next) {
  case 0:
  case 3:
  case 5:
    p.x -= 1;
    break;
  case 2:
  case 4:
  case 7:
    p.x += 1;
    break;
  }

  switch (next) {
  case 0:
  case 1:
  case 2:
    p.y -= 1;
    break;
  case 5:
  case 6:
  case 7:
    p.y += 1;
    break;
  }
}

//-----------------------------------------------------------------------------

inline int InkSegmenter::stepReversed(TPixelCM32 *walker, int prewalker,
                                      int &distance, const TPoint &p1,
                                      TPoint &p2) {
  int next = NextPointTableRev[(neighboursCode(walker, p2) << 3) | prewalker];
  newP(next, p2);
  distance = norm2(p1 - p2);
  return next;
}

/*------------------------------------------------------------------------*/

inline int InkSegmenter::stepForward(TPixelCM32 *walker, int prewalker,
                                     int &distance, const TPoint &p1,
                                     TPoint &p2) {
  int next = NextPointTable[(neighboursCode(walker, p2) << 3) | prewalker];
  newP(next, p2);
  distance = norm2(p1 - p2);
  return next;
}

//-----------------------------------------------------------------------------

int InkSegmenter::searchForNearestSlave(TPixelCM32 *pix1, TPixelCM32 *pix2,
                                        const TPoint &p1, TPoint &p2,
                                        TPixelCM32 *&slave, TPoint &sp) {
  int curr_distance, new_distance;
  UCHAR prewalker, next;
  TPixelCM32 *walker;
  TPoint currp2;

  currp2 = p2;

  curr_distance = norm2(p1 - p2);
  walker        = pix2;
  slave         = pix2;
  sp            = p2;

  prewalker = FirstPreseedTable[neighboursCode(walker, p2)];

  next = stepForward(walker, prewalker, new_distance, p1, p2);

  if (curr_distance != 0 && new_distance < curr_distance) {
    while (p2.x > 0 && p2.x < m_lx - 1 && p2.y > 0 && p2.y < m_ly - 1 &&
           new_distance < curr_distance && new_distance != 0) {
      curr_distance = new_distance;
      sp.x          = p2.x;
      sp.y          = p2.y;
      walker        = walker + m_displaceVector[next];
      slave         = walker;
      prewalker     = (~next) & 0x7;
      next          = stepForward(walker, prewalker, new_distance, p1, p2);
    }
    if (new_distance != 0) return curr_distance;
  }

  curr_distance = norm2(p1 - p2);
  walker        = pix2;
  p2            = currp2;

  UCHAR code = neighboursCode(walker, p2);
  next       = FirstPreseedTable[code];
  next       = NextPointTable[(code << 3) | next];
  prewalker  = next;

  next = stepReversed(walker, prewalker, new_distance, p1, p2);

  if (p2.x > 0 && p2.x < m_lx - 1 && p2.y > 0 && p2.y < m_ly - 1 &&
      curr_distance != 0 && new_distance < curr_distance) {
    while (new_distance < curr_distance && new_distance > 0) {
      curr_distance = new_distance;
      sp            = p2;

      walker = walker + m_displaceVector[next];
      slave  = walker;

      prewalker = (~next) & 0x7;
      next      = stepReversed(walker, prewalker, new_distance, p1, p2);
    }
    if (new_distance != 0) return curr_distance;
  } else if (new_distance != 0)
    return curr_distance;
  return 0;
}

//-----------------------------------------------------------------------------

int InkSegmenter::rearrangePoints(TPixelCM32 *&master, TPoint &mp,
                                  TPixelCM32 *&slave, int s_prewalker,
                                  TPoint &sp, int walk) {
  int s_next;

  while (walk-- && sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1) {
    s_next =
        NextPointTableRev[((neighboursCode(slave, sp)) << 3) | s_prewalker];

    newP(s_next, sp);

    slave       = slave + m_displaceVector[s_next];
    s_prewalker = (~s_next) & 0x7;
  }
  return 1;
}

//-----------------------------------------------------------------------------

int InkSegmenter::rearrangePointsRev(TPixelCM32 *&master, TPoint &mp,
                                     TPixelCM32 *&slave, int s_prewalker,
                                     TPoint &sp, int walk) {
  int s_next;

  while (walk-- && sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1) {
    s_next = NextPointTable[((neighboursCode(slave, sp)) << 3) | s_prewalker];
    // s_next = NEXT_POINT_24(*slave, s_prewalker);

    newP(s_next, sp);

    slave       = slave + m_displaceVector[s_next];
    s_prewalker = (~s_next) & 0x7;
  }
  return 1;
}

//-----------------------------------------------------------------------------

int InkSegmenter::dragSlave(TPoint mp, TPixelCM32 *&slave, int &s_prewalker,
                            TPoint &sp) {
  int distance, s_next, new_distance;
  int ret = 0;

  distance = norm2(mp - sp);

  s_next = stepForward(slave, s_prewalker, new_distance, mp, sp);

  while (sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1 &&
         (new_distance < distance ||
          nextPointIsGood(mp, sp, slave + m_displaceVector[s_next],
                          (~s_next) & 0x7, distance))) {
    if (!ret) ret = 1;
    distance      = new_distance;
    slave         = slave + m_displaceVector[s_next];
    s_prewalker   = (~s_next) & 0x7;
    s_next        = stepForward(slave, s_prewalker, new_distance, mp, sp);
  }

  newP(((~s_next) & 0x7), sp);
  return ret;
}

//-----------------------------------------------------------------------------

int InkSegmenter::dragSlaveRev(TPoint mp, TPixelCM32 *&slave, int &s_prewalker,
                               TPoint &sp, TPixelCM32 *first_slave) {
  int distance, new_distance, s_next;
  int ret = 0;

  distance = norm2(mp - sp);

  s_next = stepReversed(slave, s_prewalker, new_distance, mp, sp);

  while (sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1 &&
         (new_distance < distance ||
          nextPointIsGoodRev(mp, sp, slave + m_displaceVector[s_next],
                             (~s_next) & 0x7, distance))) {
    if (!ret) ret = 1;
    distance      = new_distance;
    slave         = slave + m_displaceVector[s_next];
    if (slave == first_slave) return -1;

    s_prewalker = (~s_next) & 0x7;
    s_next      = stepReversed(slave, s_prewalker, new_distance, mp, sp);
  }

  newP(((~s_next) & 0x7), sp);
  return ret;
}

//-----------------------------------------------------------------------------

bool InkSegmenter::findDam(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave,
                           TPoint sp, int distance, TPixelCM32 *&d11,
                           TPoint &d1p1, TPixelCM32 *&d12, TPoint &d1p2)

{
  int ref_distance, m_prewalker, s_prewalker, m_next, next, ret;
  unsigned int walkalone = 0;
  TPixelCM32 *first_slave, *first_master;

  first_slave  = slave;
  first_master = master;

  ref_distance = tround(m_growFactor * ((float)distance + 1));

  m_prewalker = FirstPreseedTable[neighboursCode(master, mp)];

  if (!ConnectionTable[neighboursCode(master, mp)])
    s_prewalker = FirstPreseedTableRev[neighboursCode(slave, sp)];
  else {
    UCHAR code  = neighboursCode(slave, sp);
    next        = FirstPreseedTable[code];
    next        = NextPointTable[(code << 3) | next];
    s_prewalker = next;
  }

  while (mp.x > 0 && mp.x < m_lx - 1 && mp.y > 0 && mp.y < m_ly - 1 &&
         distance < ref_distance &&
         !(((m_next = NextPointTable[((neighboursCode(master, mp)) << 3) |
                                     m_prewalker]) == s_prewalker) &&
           master == slave)) {
    newP(m_next, mp);
    master      = master + m_displaceVector[m_next];
    m_prewalker = (~m_next) & 0x7;

    ret = dragSlaveRev(mp, slave, s_prewalker, sp, first_slave);

    if (ret == -1) return false;

    if (ret == 0)
      walkalone++;
    else
      walkalone = 0;

    if (master == first_master) break;
    distance = norm2(mp - sp);
  }

  if (walkalone > 0)
    rearrangePoints(master, mp, slave, s_prewalker, sp, walkalone);

  d11  = master;
  d1p1 = mp;
  d12  = slave;
  d1p2 = sp;
  return 1;
}

//-----------------------------------------------------------------------------

void InkSegmenter::findDamRev(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave,
                              TPoint sp, int distance, TPixelCM32 *&d11,
                              TPoint &d1p1, TPixelCM32 *&d12, TPoint &d1p2)

{
  int ref_distance, m_prewalker, s_prewalker, m_next, next;
  unsigned int walkalone = 0;
  TPixelCM32 *first_master;

  first_master = master;

  ref_distance = tround(GROW_FACTOR * ((float)distance + 1));

  m_prewalker = FirstPreseedTableRev[neighboursCode(master, mp)];

  if (!ConnectionTable[neighboursCode(master, mp)]) {
    UCHAR code  = neighboursCode(slave, sp);
    next        = FirstPreseedTableRev[code];
    next        = NextPointTableRev[(code << 3) | next];
    s_prewalker = next;
  } else
    s_prewalker = FirstPreseedTable[neighboursCode(slave, sp)];

  while (mp.x > 0 && mp.x < m_lx - 1 && mp.y > 0 && mp.y < m_ly - 1 &&
         distance < ref_distance &&
         !(((m_next = NextPointTableRev[((neighboursCode(master, mp)) << 3) |
                                        m_prewalker]) == s_prewalker) &&
           master == slave)) {
    newP(m_next, mp);
    master      = master + m_displaceVector[m_next];
    m_prewalker = (~m_next) & 0x7;
    if (!dragSlave(mp, slave, s_prewalker, sp))
      walkalone++;
    else
      walkalone = 0;
    if (master == first_master) break;
    distance = norm2(mp - sp);
  }

  if (walkalone > 0)
    rearrangePointsRev(master, mp, slave, s_prewalker, sp, walkalone);

  d11  = master;
  d1p1 = mp;
  d12  = slave;
  d1p2 = sp;
}

//-----------------------------------------------------------------------------

int InkSegmenter::nextPointIsGood(TPoint mp, TPoint sp, TPixelCM32 *slave,
                                  int s_prewalker, int distance) {
  int s_next;

  s_next = NextPointTable[((neighboursCode(slave, sp)) << 3) | s_prewalker];

  newP(s_next, sp);

  return (norm2(mp - sp) <= distance);
}

//-----------------------------------------------------------------------------

int InkSegmenter::nextPointIsGoodRev(TPoint mp, TPoint sp, TPixelCM32 *slave,
                                     int s_prewalker, int distance) {
  int s_next;

  s_next = NextPointTableRev[((neighboursCode(slave, sp)) << 3) | s_prewalker];

  newP(s_next, sp);

  return (norm2(mp - sp) <= distance);
}

//-----------------------------------------------------------------------------

bool inkSegment(const TRasterCM32P &r, const TPoint &p, int ink,
                float growFactor, bool isSelective, TTileSaverCM32 *saver) {
  r->lock();
  InkSegmenter is(r, growFactor, saver);
  bool ret = is.compute(p, ink, isSelective);
  r->unlock();
  return ret;
}
