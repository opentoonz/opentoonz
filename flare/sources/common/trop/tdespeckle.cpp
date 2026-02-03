

#include "tropcm.h"

// TnzCore includes
#include "tpalette.h"
#include "trop_borders.h"

#include "pixelselectors.h"
#include "runsmap.h"

#define INCLUDE_HPP
#include "raster_edge_iterator.h"
#include "borders_extractor.h"
#define INCLUDE_HPP

// STL includes
#include <deque>

/*============================================================================================

    Explanation

  Despeckling is a noise-removal procedure which aims at eliminating small blots
of color
  from an image.

  We will assume that speckles are recognized with uniform color (which means -
image
  discretization should be externally performed).

  We will currently assume that the despeckling procedure works only on the ink
or paint plane of a
  TRasterCM32 instance, not both (should be extended in the future).

  The image is traversed to isolate regions with the same color and, if their
area is
  below the specified threshold, they get removed or changed of color.

  Color change looks at the area's neighbouring pixels for the most used color
to use for
  replacement. If NO neighbouring color can be found, the area is made
transparent.

==============================================================================================*/

using namespace TRop::borders;

namespace {

//************************************************************************
//    Pixel Selectors
//************************************************************************

class InkSelectorCM32 {
public:
  typedef TPixelCM32 pixel_type;
  typedef TUINT32 value_type;

public:
  InkSelectorCM32() {}

  value_type transparent() const { return 0; }
  bool transparent(const pixel_type &pix) const { return value(pix) == 0; }

  value_type value(const pixel_type &pix) const { return pix.getInk(); }
  bool equal(const pixel_type &a, const pixel_type &b) const {
    return value(a) == value(b);
  }

  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return true;
  }
};

//------------------------------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL>
class InkSelectorRGBM {
  bool m_transparentIsWhite;

public:
  typedef PIXEL pixel_type;
  typedef CHANNEL value_type;

public:
  InkSelectorRGBM(bool transparentIsWhite)
      : m_transparentIsWhite(transparentIsWhite) {}

  value_type transparent() const { return 0; }
  bool transparent(const pixel_type &pix) const { return value(pix) == 0; }

  value_type value(const pixel_type &pix) const {
    if (m_transparentIsWhite)
      return (pix == PIXEL::White) ? 0 : 1;
    else
      return (pix.m == 0) ? 0 : 1;
  }
  bool equal(const pixel_type &a, const pixel_type &b) const {
    return value(a) == value(b);
  }

  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return true;
  }
};

//------------------------------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL>
class InkSelectorGR {
public:
  typedef PIXEL pixel_type;
  typedef CHANNEL value_type;

public:
  InkSelectorGR() {}

  value_type transparent() const { return PIXEL::maxChannelValue; }
  bool transparent(const pixel_type &pix) const { return value(pix) == 0; }

  value_type value(const pixel_type &pix) const {
    return (pix.value == PIXEL::maxChannelValue) ? 0 : 1;
  }
  bool equal(const pixel_type &a, const pixel_type &b) const {
    return value(a) == value(b);
  }

  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return true;
  }
};

//************************************************************************
//    Border class
//************************************************************************

struct Border {
  std::vector<TPoint> m_points;
  int m_x0, m_y0, m_x1, m_y1;

  Border()
      : m_x0((std::numeric_limits<int>::max)())
      , m_y0(m_x0)
      , m_x1(-m_x0)
      , m_y1(m_x1) {}

  void addPoint(const TPoint &p) {
    // Update the region box
    if (p.x < m_x0) m_x0 = p.x;
    if (p.x > m_x1) m_x1 = p.x;
    if (p.y < m_y0) m_y0 = p.y;
    if (p.y > m_y1) m_y1 = p.y;

    // Add the vertex
    m_points.push_back(p);
  }

  void clear() {
    m_points.clear();
    m_x0 = (std::numeric_limits<int>::max)(), m_y0 = m_x0, m_x1 = -m_x0,
    m_y1 = m_x1;
  }
};

//************************************************************************
//    Base classes
//************************************************************************

//========================================================================
//    Borders Painter
//========================================================================

template <typename Pix>
class BordersPainter {
public:
  typedef Pix pixel_type;

protected:
  TRasterPT<pixel_type> m_ras;
  RunsMapP m_runsMap;

public:
  BordersPainter(const TRasterPT<pixel_type> &ras) : m_ras(ras) {}

  const TRasterPT<pixel_type> &ras() { return m_ras; }
  RunsMapP &runsMap() { return m_runsMap; }

  void paintLine(int x, int y0, int y1) const;
  void paintBorder(const Border &border) const;
  void paintBorders(const std::deque<Border *> &borders) const {
    size_t i, size = borders.size();
    for (i = 0; i < size; ++i) paintBorder(*borders[i]);
  }

protected:
  virtual void paintPixel(pixel_type *pix) const = 0;
};

//---------------------------------------------------------------------------------------------

template <typename Pix>
void BordersPainter<Pix>::paintBorder(const Border &border) const {
  const std::vector<TPoint> &points = border.m_points;

  size_t i, size_1 = points.size() - 1;
  for (i = 0; i < size_1; ++i) {
    const TPoint &p = points[i];
    paintLine(p.x, p.y, points[i + 1].y);
  }
  paintLine(points[size_1].x, points[size_1].y, points[0].y);
}

//---------------------------------------------------------------------------------------------

template <typename Pix>
void BordersPainter<Pix>::paintLine(int x, int y0, int y1) const {
  for (int j = y0; j < y1; ++j) {
    TPixelGR8 *runPix = m_runsMap->pixels(j) + x;
    int l, runLength = 0, hierarchy = 0;

    do {
      if (runPix->value & TRop::borders::_HIERARCHY_INCREASE) ++hierarchy;

      // Update vars
      runLength += l = m_runsMap->runLength(runPix);
      runPix += l;

      if ((runPix - 1)->value & TRop::borders::_HIERARCHY_DECREASE) --hierarchy;
    } while (hierarchy > 0);

    pixel_type *pix = m_ras->pixels(j) + x, *pixEnd = pix + runLength;

    for (; pix < pixEnd; ++pix) paintPixel(pix);
  }
}

//========================================================================
//    Despeckling Reader
//========================================================================

class DespecklingReader {
protected:
  std::deque<Border *> m_borders;
  Border m_border;

  int m_sizeTol;

public:
  DespecklingReader(int sizeTol) : m_sizeTol(sizeTol) {}
  ~DespecklingReader();

  int sizeTol() const { return m_sizeTol; }
  bool isSpeckle(const Border &border) {
    return border.m_x1 - border.m_x0 <= m_sizeTol &&
           border.m_y1 - border.m_y0 <= m_sizeTol;
  }

  void openContainer(const TPoint &pos);
  void addElement(const TPoint &pos);
  virtual void closeContainer();

  const std::deque<Border *> &borders() const { return m_borders; }
  std::deque<Border *> &borders() { return m_borders; }
};

//---------------------------------------------------------------------------------------------

DespecklingReader::~DespecklingReader() {
  std::for_each(m_borders.begin(), m_borders.end(), std::default_delete<Border>());
}

//---------------------------------------------------------------------------------------------

void DespecklingReader::openContainer(const TPoint &pos) {
  m_border.clear();
  m_border.addPoint(pos);
}

//---------------------------------------------------------------------------------------------

void DespecklingReader::addElement(const TPoint &pos) {
  m_border.addPoint(pos);
}

//---------------------------------------------------------------------------------------------

void DespecklingReader::closeContainer() {
  if (isSpeckle(m_border)) m_borders.push_back(new Border(m_border));
}

//************************************************************************
//    Specialized classes
//************************************************************************

//========================================================================
//    Replace Painters
//========================================================================

template <typename Pix>
class ReplacePainter final : public BordersPainter<Pix> {
  typename ReplacePainter::pixel_type m_color;

public:
  ReplacePainter(const TRasterPT<typename ReplacePainter::pixel_type> &ras)
      : BordersPainter<Pix>(ras) {}
  ReplacePainter(const TRasterPT<typename ReplacePainter::pixel_type> &ras,
                 const typename ReplacePainter::pixel_type &color)
      : BordersPainter<Pix>(ras), m_color(color) {}

  const typename ReplacePainter::pixel_type &color() const { return m_color; }
  typename ReplacePainter::pixel_type &color() { return m_color; }

  void paintPixel(typename ReplacePainter::pixel_type *pix) const override {
    *pix = m_color;
  }
};

//---------------------------------------------------------------------------------------------

template <>
class ReplacePainter<TPixelCM32> final : public BordersPainter<TPixelCM32> {
  TUINT32 m_value;
  TUINT32 m_keepMask;

public:
  ReplacePainter(const TRasterPT<pixel_type> &ras)
      : BordersPainter<TPixelCM32>(ras), m_value(0), m_keepMask(0) {}
  ReplacePainter(const TRasterPT<pixel_type> &ras, TUINT32 value,
                 TUINT32 keepMask)
      : BordersPainter<TPixelCM32>(ras), m_value(value), m_keepMask(keepMask) {}

  const TUINT32 &value() const { return m_value; }
  TUINT32 &value() { return m_value; }

  const TUINT32 &keepMask() const { return m_keepMask; }
  TUINT32 &keepMask() { return m_keepMask; }

  void paintPixel(pixel_type *pix) const override {
    *pix = TPixelCM32(m_value | (pix->getValue() & m_keepMask));
  }
};

//========================================================================
//    Isolated Despeckling
//========================================================================

template <typename PixelSelector>
class IsolatedReader final : public DespecklingReader {
public:
  typedef typename PixelSelector::pixel_type pixel_type;
  typedef typename PixelSelector::value_type value_type;

private:
  const PixelSelector &m_selector;
  bool m_ok;

public:
  IsolatedReader(const PixelSelector &selector, int sizeTol);

  void openContainer(const RasterEdgeIterator<PixelSelector> &it);
  void addElement(const RasterEdgeIterator<PixelSelector> &it);
  void closeContainer() override;
};

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
IsolatedReader<PixelSelector>::IsolatedReader(const PixelSelector &selector,
                                              int sizeTol)
    : DespecklingReader(sizeTol), m_selector(selector) {}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void IsolatedReader<PixelSelector>::openContainer(
    const RasterEdgeIterator<PixelSelector> &it) {
  m_ok = (it.leftColor() == m_selector.transparent());
  if (!m_ok) return;

  DespecklingReader::openContainer(it.pos());
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void IsolatedReader<PixelSelector>::addElement(
    const RasterEdgeIterator<PixelSelector> &it) {
  if (!m_ok) return;
  m_ok = (it.leftColor() == m_selector.transparent());
  if (!m_ok) return;

  DespecklingReader::addElement(it.pos());
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void IsolatedReader<PixelSelector>::closeContainer() {
  if (m_ok) DespecklingReader::closeContainer();
}

}  // namespace

//*********************************************************************************************************
//    Despeckling Mains
//*********************************************************************************************************

/*!
  Applies despeckling (paint or removal of small blots of uniform color) to the
  image.
*/

template <typename PIXEL, typename CHANNEL>
void doDespeckleRGBM(const TRasterPT<PIXEL> &ras, int sizeThreshold,
                     bool transparentIsWhite) {
  InkSelectorRGBM<PIXEL, CHANNEL> selector(transparentIsWhite);
  IsolatedReader<InkSelectorRGBM<PIXEL, CHANNEL>> reader(selector,
                                                         sizeThreshold);
  ReplacePainter<PIXEL> painter(
      ras, transparentIsWhite ? PIXEL::White : PIXEL::Transparent);

  TRop::borders::readBorders(ras, selector, reader, &painter.runsMap());
  painter.paintBorders(reader.borders());
}

//----------------------------------------------------

template <typename PIXEL, typename CHANNEL>
void doDespeckleGR(const TRasterPT<PIXEL> &ras, int sizeThreshold) {
  InkSelectorGR<PIXEL, CHANNEL> selector;
  IsolatedReader<InkSelectorGR<PIXEL, CHANNEL>> reader(selector, sizeThreshold);
  ReplacePainter<PIXEL> painter(ras, PIXEL::maxChannelValue);

  TRop::borders::readBorders(ras, selector, reader, &painter.runsMap());
  painter.paintBorders(reader.borders());
}

//----------------------------------------------------

static void doDespeckleCM32(const TRasterPT<TPixelCM32> &ras, int sizeThreshold,
                            bool check) {
  TRasterCM32P rasCM(ras);
  rasCM->lock();

  InkSelectorCM32 selector;
  IsolatedReader<InkSelectorCM32> reader(selector, sizeThreshold);
  ReplacePainter<TPixelCM32> painter(
      rasCM, check ? 0xffffff00 : 0x000000ff,
      0);  // 0xffffff00 is a special non-mapped full ink pixel
           // 0x000000ff is a full transparent paint pixel
  TRop::borders::readBorders(rasCM, selector, reader, &painter.runsMap());
  painter.paintBorders(reader.borders());

  rasCM->unlock();
}

//---------------------------------------------------------------------------------------------

void TRop::despeckle(const TRasterP &ras, int sizeThreshold, bool check,
                     bool transparentIsWhite) {
  ras->lock();

  if (TRasterCM32P(ras)) {
    doDespeckleCM32(ras, sizeThreshold, check);
    return;
  }
  if (TRaster32P(ras)) {
    doDespeckleRGBM<TPixel32, UCHAR>(ras, sizeThreshold, transparentIsWhite);
    return;
  }
  if (TRaster64P(ras)) {
    doDespeckleRGBM<TPixel64, USHORT>(ras, sizeThreshold, transparentIsWhite);
    return;
  }
  if (TRasterGR8P(ras)) {
    doDespeckleGR<TPixelGR8, UCHAR>(ras, sizeThreshold);
    return;
  }
  if (TRasterGR16P(ras)) {
    doDespeckleGR<TPixelGR16, USHORT>(ras, sizeThreshold);
    return;
  }

  ras->unlock();
}

//---------------------------------------------------------------------------------------------

/*!
  Performs a copy of rin and then applies despeckling.
*/
void TRop::despeckle(const TRasterP &rout, const TRasterP &rin,
                     int sizeThreshold, bool check) {
  TRop::copy(rout, rin);
  TRop::despeckle(rout, sizeThreshold, check);
}

//*********************************************************************************************************
//    Majority Despeckling
//*********************************************************************************************************

namespace {

template <typename PixelSelector>
class FillingReader final : public DespecklingReader {
public:
  typedef typename PixelSelector::pixel_type pixel_type;
  typedef typename PixelSelector::value_type value_type;

private:
  ReplacePainter<TPixelGR8> m_painter;

public:
  FillingReader(const TRasterGR8P &rasGR, int sizeTol)
      : DespecklingReader(sizeTol), m_painter(rasGR, TPixelGR8::Black) {}

  void openContainer(const RasterEdgeIterator<PixelSelector> &it);
  void addElement(const RasterEdgeIterator<PixelSelector> &it);
  void closeContainer() override;

  RunsMapP &runsMap() { return m_painter.runsMap(); }
};

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void FillingReader<PixelSelector>::openContainer(
    const RasterEdgeIterator<PixelSelector> &it) {
  DespecklingReader::openContainer(it.pos());
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void FillingReader<PixelSelector>::addElement(
    const RasterEdgeIterator<PixelSelector> &it) {
  DespecklingReader::addElement(it.pos());
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void FillingReader<PixelSelector>::closeContainer() {
  if (isSpeckle(m_border)) m_painter.paintBorder(m_border);

  DespecklingReader::closeContainer();
}

//=============================================================================================

inline TPoint direction(const TPoint &a, const TPoint &b) {
  return TPoint((b.x > a.x) ? 1 : (b.x < a.x) ? -1 : 0,
                (b.y > a.y) ? 1 : (b.y < a.y) ? -1 : 0);
}

//---------------------------------------------------------------------------------------------

template <typename Pixel, typename PixelSelector>
bool majority(const TRasterPT<Pixel> ras, const TRasterGR8P &rasGR,
              const PixelSelector &selector, const Border &border,
              typename PixelSelector::value_type &color) {
  typedef typename PixelSelector::value_type value_type;

  // Build a histogram of all found colors around the border
  std::map<value_type, int> histogram;

  Pixel *pix, *basePix = ras->pixels(0);
  TPixelGR8 *grPix;

  int diff, x, y, lx = ras->getLx(), ly = ras->getLy(), wrap = ras->getWrap();

  assert(border.m_points[1].y > border.m_points[0].y);

  // Iterate the raster along the border
  const std::vector<TPoint> &points = border.m_points;

  RasterEdgeIterator<PixelSelector> start(ras, selector, points[0],
                                          direction(points[0], points[1])),
      it(start);

  size_t next = 1, size = points.size();
  do {
    while (it.pos() != points[next]) {
      pix  = it.leftPix();
      diff = pix - basePix;
      x    = diff % wrap;
      y    = diff / wrap;

      if (x >= 0 && y >= 0 && x < lx && y < ly) {
        grPix = rasGR->pixels(y) + x;
        if (grPix->value) ++histogram[it.leftColor()];
      }

      it.setEdge(it.pos() + it.dir(), it.dir());
    }

    next = (next + 1) % size;
    it.setEdge(it.pos(), direction(it.pos(), points[next]));
  } while (it != start);

  if (!histogram.empty()) {
    // Return the most found color
    color = histogram.begin()->first;
    return true;
  }

  return false;
}

//---------------------------------------------------------------------------------------------

template <typename Pix>
void majorityDespeckle(const TRasterPT<Pix> &ras, int sizeThreshold) {
  typedef typename TRop::borders::PixelSelector<Pix> pixel_selector;
  typedef typename pixel_selector::pixel_type pixel_type;
  typedef typename pixel_selector::value_type value_type;

  ras->lock();

  // Use a temporary bitmap (well, a bytemap - for now?) to store the found
  // speckles
  TRasterGR8P rasGR(ras->getSize());
  rasGR->fill(TPixelGR8::White);

  // Find the speckles and draw them on the bitmap
  pixel_selector selector;
  FillingReader<pixel_selector> reader(rasGR, sizeThreshold);

  TRop::borders::readBorders(ras, selector, reader, &reader.runsMap());

  // Now, operate each speckle. Try to apply a majority color to the speckle
  ReplacePainter<pixel_type> painter(ras);
  painter.runsMap() = reader.runsMap();

  ReplacePainter<TPixelGR8> painterGR(rasGR, TPixelGR8::White);
  painterGR.runsMap() = reader.runsMap();

  std::deque<Border *> borders =
      reader.borders();  // Note that the DEEP copy is NEEDED

  int processedCount = 1;
  while (processedCount > 0 && !borders.empty()) {
    processedCount = 0;

    // Traverse the speckles list. Try to apply majority.
    Border *current, *last = borders.back();

    do {
      current = borders.front();
      borders.pop_front();

      value_type color;
      if (majority(ras, rasGR, selector, *current, color)) {
        ++processedCount;
        painter.color() = color;

        painter.paintBorder(*current);
        painterGR.paintBorder(*current);
      } else
        borders.push_back(current);

    } while (current != last);
  }

  // Speckles may remain. In this case, fill with transparent.
  painter.color() = selector.transparent();
  while (!borders.empty()) {
    Border *current = borders.front();
    painter.paintBorder(*current);
    borders.pop_front();
  }

  ras->unlock();
}

}  // namespace

//================================================================================================

void TRop::majorityDespeckle(const TRasterP &ras, int sizeThreshold) {
  TRaster32P ras32(ras);
  if (ras32) {
    ::majorityDespeckle(ras32, sizeThreshold);
    return;
  }

  TRaster64P ras64(ras);
  if (ras64) {
    ::majorityDespeckle(ras64, sizeThreshold);
    return;
  }

  TRasterGR8P rasGR8(ras);
  if (rasGR8) {
    ::majorityDespeckle(rasGR8, sizeThreshold);
    return;
  }

  TRasterGR16P rasGR16(ras);
  if (rasGR16) {
    ::majorityDespeckle(rasGR16, sizeThreshold);
    return;
  }

  TRasterCM32P rasCM32(ras);
  if (rasCM32) {
    // Not yet implemented
    assert(false);

    //::majorityDespeckleCM(rasCM32, sizeThreshold, toneTol);
    return;
  }
}
