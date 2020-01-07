

#include "toonz/Naa2TlvConverter.h"

#include "toonz/tcenterlinevectorizer.h"
#include "toonz/stage.h"
#include "tpixelutils.h"
#include "tpalette.h"

#include <QSet>
#include <QTime>
#include <QDebug>
#include <QMultiMap>

QString RegionInfo::getTypeString() const {
  switch (type) {
  case RegionInfo::Background:
    return "Background";
  case RegionInfo::Ink:
    return "Ink";
  case RegionInfo::ThinInk:
    return "ThinInk";
  case RegionInfo::MainInk:
    return "Main ink";
  case RegionInfo::SyntheticInk:
    return "SyntheticInk";
  case RegionInfo::Paint:
    return "Paint";
  case RegionInfo::LargePaint:
    return "LargePaint";
  case RegionInfo::SmallPaint:
    return "SmallPaint";
  case RegionInfo::Unused:
    return "Unused";
  case RegionInfo::Unknown:
    return "Unknown";
  default:
    return "??????";
  }
}

Naa2TlvConverter::Naa2TlvConverter()
    : MaxColorCount(1000)
    , m_regionRas(0)
    , m_borderRas(0)
    , m_dotRas(0)
    , m_syntheticInkRas(0)
    , m_inkThickness(0)
    , m_palette(0)
    , m_valid(false)

{}

Naa2TlvConverter::~Naa2TlvConverter() {
  delete m_regionRas;
  delete m_borderRas;
  delete m_dotRas;
  delete m_syntheticInkRas;
  if (m_palette) m_palette->release();
}

void Naa2TlvConverter::setPalette(TPalette *palette) {
  if (m_palette != palette) {
    if (palette) palette->addRef();
    if (m_palette) m_palette->release();
    m_palette = palette;
  }
}

void Naa2TlvConverter::process(const TRaster32P &srcRas) {
  m_valid = false;
  setSourceImage(srcRas);
  if (!m_valid) return;

  separateRegions();

  computeLinks();

  findRegionBorders();
  erodeRegions();

  findLargePaints();

  findBackgroundRegions();
  findMainInks();

  // computeMainInkThickness();
  findThinInks();

  measureThickness();

  // findPaints();
  findPaints2();

  findThinPaints();

  findSuspectInks();

  addBorderInks();
}

//-----------------------------------------------------------------------------

// Input: srcRas (full color image)
// Output:
//   m_regionRas : pixel => region index
//   m_regions   : region index => RegionInfo
//
// Note: delete m_borderRas

void Naa2TlvConverter::setSourceImage(const TRaster32P &srcRas) {
  int lx = srcRas->getSize().lx;
  int ly = srcRas->getSize().ly;

  m_colors.clear();
  m_regions.clear();
  delete m_regionRas;
  m_regionRas = new WorkRaster<unsigned short>(lx, ly);
  delete m_borderRas;
  m_borderRas = 0;

  delete m_dotRas;
  m_dotRas = 0;

  delete m_syntheticInkRas;
  m_syntheticInkRas = 0;

  QMap<TPixel32, int> colorTable;
  for (int y = 0; y < ly; y++) {
    TPixel32 *srcScanLine          = srcRas->pixels(y);
    unsigned short *regionScanLine = m_regionRas->pixels(y);
    for (int x = 0; x < lx; x++) {
      TPixel32 srcPix = overPixOnWhite(srcScanLine[x]);
      QMap<TPixel32, int>::ConstIterator it = colorTable.find(srcPix);
      if (it == colorTable.end()) {
        // found new color (and therefore new region)
        RegionInfo r;
        // add new color
        r.colorIndex = m_colors.count();
        m_colors.append(srcPix);
        r.pixelCount = 1;
        // add new region
        int regionIndex = m_regions.count();
        m_regions.append(r);
        // update raster and colorTable
        regionScanLine[x] = regionIndex;
        colorTable.insert(srcPix, regionIndex);
        if (m_colors.count() > MaxColorCount) {
          return;
        }
      } else {
        // already defined color
        int regionIndex   = it.value();
        regionScanLine[x] = regionIndex;
        m_regions[regionIndex].pixelCount++;
      }
    }
  }
  m_valid = true;
}

//-----------------------------------------------------------------------------

// color-based regions => connection-based regions  (i.e. split unconnected
// region parts)
// note: rebuild m_regions

void Naa2TlvConverter::separateRegions() {
  if (!m_regionRas) return;
  const int lx   = m_regionRas->getLx();
  const int ly   = m_regionRas->getLy();
  const int wrap = lx;

  // we assume that m_regions contains almost no information (except:
  // m_regions[i].colorIndex == i)
  m_regions.clear();

  WorkRaster<int> wb(lx, ly);  // work buffer

  QVector<int> regionMap;  // wb pixels => region indices
  QList<int> freeRegions;

  for (int y = 0; y < ly; y++) {
    unsigned short *waScanLine = m_regionRas->pixels(y);
    int *wbScanLine            = wb.pixels(y);
    for (int x = 0; x < lx; x++) {
      int c       = waScanLine[x];
      int cUp     = y > 0 ? waScanLine[x - wrap] : -1;
      int cLeft   = x > 0 ? waScanLine[x - 1] : -1;
      int cUpLeft = x > 0 && y > 0 ? waScanLine[x - wrap - 1] : -1;

      if (c != cUp && c != cLeft && c != cUpLeft) {
        // no connection: create a new region
        Q_ASSERT(0 <= c && c < m_colors.count());
        RegionInfo region;
        region.colorIndex = c;
        int regionIndex;
        if (!freeRegions.empty()) {
          // use a previously discarded region
          regionIndex = freeRegions.back();
          freeRegions.pop_back();
          m_regions[regionIndex] = region;
        } else {
          // create a new region
          regionIndex = m_regions.count();
          m_regions.append(region);
        }
        wbScanLine[x] = regionMap.count();
        regionMap.append(regionIndex);
      } else {
        // at least one connection
        if (c == cUpLeft)
          wbScanLine[x] = wbScanLine[x - wrap - 1];
        else if (c == cUp)
          wbScanLine[x] = wbScanLine[x - wrap];
        else {
          Q_ASSERT(c == cLeft);
          wbScanLine[x] = wbScanLine[x - 1];
        }
      }

      // merge if needed
      if (cUp == cLeft && cUp >= 0 &&
          regionMap[wbScanLine[x - 1]] != regionMap[wbScanLine[x - wrap]]) {
        // merge
        int pixelToDiscard = wbScanLine[x - 1];
        int pixelToKeep    = wbScanLine[x - wrap];
        Q_ASSERT(pixelToDiscard != pixelToKeep);
        int regionToDiscard = regionMap[pixelToDiscard];
        int regionToKeep    = regionMap[pixelToKeep];
        Q_ASSERT(regionToDiscard != regionToKeep);
        Q_ASSERT(m_regions[regionToDiscard].type != RegionInfo::Unused);
        Q_ASSERT(m_regions[regionToKeep].type != RegionInfo::Unused);
        for (int i = 0; i < regionMap.count(); i++)
          if (regionMap[i] == regionToDiscard) regionMap[i] = regionToKeep;
        // wbScanLine[x] = pixelToKeep;
        m_regions[regionToDiscard].type = RegionInfo::Unused;
        freeRegions.append(regionToDiscard);
      }
    }
  }

  // fill gaps
  for (;;) {
    // just remove topmost Unused regions
    if (!m_regions.empty() && m_regions.back().type == RegionInfo::Unused) {
      m_regions.pop_back();
      int k = m_regions.count();
      freeRegions.removeAll(k);
    } else if (!freeRegions.empty()) {
      // last region is used, but there is at least a free region with lower
      // index: move it there
      int regionToDiscard = m_regions.count() - 1;
      int regionToKeep    = freeRegions.back();
      freeRegions.pop_back();
      Q_ASSERT(m_regions[regionToKeep].type == RegionInfo::Unused);
      Q_ASSERT(m_regions[regionToDiscard].type != RegionInfo::Unused);
      Q_ASSERT(regionToKeep < regionToDiscard);
      m_regions[regionToKeep] = m_regions[regionToDiscard];
      // pop the region to discard
      m_regions.pop_back();
      // update map
      for (int i = 0; i < regionMap.count(); i++)
        if (regionMap[i] == regionToDiscard) regionMap[i] = regionToKeep;
    } else
      break;
  }

  Q_ASSERT(freeRegions.empty());

  for (int i = 0; i < m_regions.count(); i++) m_regions[i].pixelCount = 0;

  // wb => m_regionRas
  for (int y = 0; y < ly; y++) {
    unsigned short *regionScanLine = m_regionRas->pixels(y);
    int *wbScanLine                = wb.pixels(y);
    for (int x = 0; x < lx; x++) {
      int regionIndex   = regionMap[wbScanLine[x]];
      regionScanLine[x] = (unsigned short)regionIndex;
      m_regions[regionIndex].pixelCount++;
    }
  }
}

//-----------------------------------------------------------------------------

// compute adjacency relationships
// update: m_regions[].links
// outer regions are connected to the "pseudo-region" -1
// note: check orthogonal directions only
// compute region.perimeter

void Naa2TlvConverter::computeLinks() {
  if (!m_regionRas || m_regions.empty()) return;
  const int lx   = m_regionRas->getLx();
  const int ly   = m_regionRas->getLy();
  const int wrap = lx;
  unsigned short *waScanLine;
  int x;
  for (int i = 0; i < m_regions.count(); i++) m_regions[i].perimeter = 0;
  for (int y = 0; y < ly; y++) {
    waScanLine = m_regionRas->pixels(y);
    for (x = 0; x < lx; x++) {
      int c     = waScanLine[x];
      int cUp   = y > 0 ? waScanLine[x - wrap] : -1;
      int cLeft = x > 0 ? waScanLine[x - 1] : -1;
      if (c != cUp) {
        m_regions[c].touchRegion(cUp);
        m_regions[c].perimeter++;
        if (cUp >= 0) {
          m_regions[cUp].touchRegion(c);
          m_regions[cUp].perimeter++;
        }
      }
      if (c != cLeft) {
        m_regions[c].touchRegion(cLeft);
        m_regions[c].perimeter++;
        if (cLeft >= 0) {
          m_regions[cLeft].touchRegion(c);
          m_regions[cLeft].perimeter++;
        }
      }
    }
    m_regions[waScanLine[lx - 1]].touchRegion(-1);
  }
  waScanLine = m_regionRas->pixels(ly - 1);
  for (x = 0; x < lx; x++) {
    m_regions[waScanLine[x]].touchRegion(-1);
  }
}

//-----------------------------------------------------------------------------

// find background regions : (almost) white
//
void Naa2TlvConverter::findBackgroundRegions() {
  if (!m_regionRas || m_regions.empty()) return;
  int lx = m_regionRas->getLx();
  int ly = m_regionRas->getLy();

  // find the whitest color
  int bgColorIndex = -1;
  int maxV         = 0;
  for (int i = 0; i < m_colors.count(); i++) {
    TPixel color = m_colors.at(i);
    int v        = color.r + color.g + color.b;
    int a        = std::min({color.r, color.g, color.b});
    if (a < 230) continue;
    if (v > maxV) {
      bgColorIndex = i;
      maxV         = v;
    }
  }
  if (bgColorIndex < 0) {
    // no white found. no bg region
    return;
  }

  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region                                 = m_regions[i];
    if (region.colorIndex == bgColorIndex) region.type = RegionInfo::Background;
  }
}

//-----------------------------------------------------------------------------

// start computing m_borderRas : 1 for boundary pixels, 0 for internal pixels

void Naa2TlvConverter::findRegionBorders() {
  if (!m_regionRas) return;
  int lx = m_regionRas->getLx();
  int ly = m_regionRas->getLy();
  delete m_borderRas;
  m_borderRas = new WorkRaster<unsigned char>(lx, ly);

  static const int dd[][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                              {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

  // find region-region boundaries
  for (int y = 0; y < ly; y++) {
    unsigned short *workScanLine  = m_regionRas->pixels(y);
    unsigned char *borderScanLine = m_borderRas->pixels(y);

    for (int x = 0; x < lx; x++) {
      int k = workScanLine[x];
      Q_ASSERT(0 <= k && k < m_regions.count());
      int isBoundary = 0;
      for (int j = 0; j < 8; j++) {
        int x1 = x + dd[j][0], y1 = y + dd[j][1];
        if (0 <= x1 && x1 < lx && 0 <= y1 && y1 < ly) {
          int k1 = m_regionRas->pixels(y1)[x1];
          if (k1 != k) {
            isBoundary = 1;
            break;
          }
        }
      }
      borderScanLine[x] = isBoundary;
    }
  }
}

//-----------------------------------------------------------------------------

// update m_borders, adding 2,3... for internal pixels close to boundary
// update m_regions[i].boundaries (histogram : boundaries[k] is the number of
// pixels belonging to that region with m_border[pix] == k)

void Naa2TlvConverter::erodeRegions() {
  QTime clock;
  clock.start();
  if (!m_regionRas || !m_borderRas) return;
  int lx = m_regionRas->getLx();
  int ly = m_regionRas->getLy();

  static const int dd[][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                              {1, 0},   {-1, 1}, {0, 1},  {1, 1}};

  for (int iter = 0; iter < 10; iter++) {
    for (int y = 0; y < ly; y++) {
      unsigned short *workScanLine  = m_regionRas->pixels(y);
      unsigned char *borderScanLine = m_borderRas->pixels(y);
      for (int x = 0; x < lx; x++) {
        if (borderScanLine[x] != iter + 1) continue;
        int c = workScanLine[x];
        for (int j = 0; j < 8; j++) {
          int x1 = x + dd[j][0], y1 = y + dd[j][1];
          if (!(0 <= x1 && x1 < lx && 0 <= y1 && y1 < ly)) continue;
          int c1 = m_regionRas->pixels(y1)[x1];
          int b1 = m_borderRas->pixels(y1)[x1];
          if (c1 == c && b1 == 0) m_borderRas->pixels(y1)[x1] = iter + 2;
        }
      }
    }
  }

  for (int i = 0; i < m_regions.count(); i++) m_regions[i].boundaries.clear();
  for (int y = 0; y < ly; y++) {
    unsigned short *workScanLine  = m_regionRas->pixels(y);
    unsigned char *borderScanLine = m_borderRas->pixels(y);
    for (int x = 0; x < lx; x++) {
      int c          = workScanLine[x];
      int b          = borderScanLine[x];
      RegionInfo &r  = m_regions[c];
      QList<int> &bs = r.boundaries;
      while (bs.count() <= b) bs.append(0);
      bs[b]++;
      if (b == bs.count() - 1) r.pos = QPoint(x, y);
      if (r.x0 > r.x1) {
        r.x0 = r.x1 = x;
        r.y0 = r.y1 = y;
      } else {
        if (x < r.x0)
          r.x0 = x;
        else if (x > r.x1)
          r.x1 = x;
        if (y < r.y0)
          r.y0 = y;
        else if (y > r.y1)
          r.y1 = y;
      }
    }
  }
  qDebug() << "Erode regions. time = " << clock.elapsed();
}

//-----------------------------------------------------------------------------

// main inks touch background regions and don't have large interior
void Naa2TlvConverter::findMainInks() {
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;
    if (region.isBackground() || region.type == RegionInfo::LargePaint ||
        region.boundaries[0] > 0)
      continue;
    double ap2 =
        100000.0 * (double)region.pixelCount / pow((double)region.perimeter, 2);
    if (ap2 > 100) continue;
    for (int c : region.links.keys()) {
      if (c >= 0 && (m_regions[c].isBackground() ||
                     m_regions[c].type == RegionInfo::LargePaint)) {
        int strength = region.links[c];
        if (strength > 50) {
          m_regions[i].type = RegionInfo::MainInk;
          break;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

// compute the average thickness of the main ink region

void Naa2TlvConverter::computeMainInkThickness() {
  m_inkThickness  = 0.0;
  int largestArea = 0;
  for (int i = 0; i < m_regions.count(); i++) {
    if (m_regions[i].type != RegionInfo::MainInk) continue;
    if (m_regions[i].pixelCount < largestArea) continue;
    largestArea = m_regions[i].pixelCount;
    if (i == 55) {
      int x = 123;
    }
    QList<int> &bs = m_regions[i].boundaries;
    Q_ASSERT(bs[1] > 0);
    int perimeter = m_regions[i].perimeter;
    int area      = bs[1];
    for (int j = 2; j < bs.count() && bs[j] * 2 > bs[1]; j++) area += bs[j];
    if (perimeter > 0) {
      m_inkThickness = 2 * (double)area / (double)perimeter;
    }
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::findLargePaints() {
  if (!m_regionRas || !m_borderRas || m_regions.empty()) return;
  QSet<int> largePaintColors;
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;
    QList<int> &bs = region.boundaries;
    if (bs[0] > 0) {
      region.type = RegionInfo::LargePaint;
      largePaintColors.insert(region.colorIndex);
    }
  }
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;
    if (largePaintColors.contains(region.colorIndex))
      region.type = RegionInfo::LargePaint;
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::findThinInks() {
  if (!m_regionRas || !m_borderRas || m_regions.empty()) return;
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;
    QList<int> &bs = region.boundaries;
    if (bs.count() == 2)
      region.type = RegionInfo::ThinInk;
    else if (bs.count() == 3) {
      continue;
      if (bs[2] * 5 < bs[1]) region.type = RegionInfo::ThinInk;
    } else {
      continue;
      int k = 1;
      int s = 0;
      while (k < bs.count() && s * 100 < region.pixelCount * 90) s += bs[k++];
      if (region.pixelCount > 100 && k <= 3)
        region.type = RegionInfo::ThinInk;  // era Ink per qualche ragione
    }
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::findPaints() {
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;
    for (int c : m_regions[i].links.keys()) {
      if (c >= 0 && m_regions[c].isInk()) {
        m_regions[i].type = RegionInfo::Paint;
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::findPaints2() {
  double avThickness = 0.0;
  int m              = 0;
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type == RegionInfo::MainInk) {
      avThickness += region.thickness * region.pixelCount;
      m += region.pixelCount;
    }
  }
  if (m > 0) {
    avThickness = avThickness / m;
  } else {
    avThickness = 1.5;  // this should never happen
  }

  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;

    if (region.thickness > 0.0) {
      if (region.thickness < 1.2 * avThickness)
        region.type = RegionInfo::Ink;
      else
        region.type = RegionInfo::Paint;
    }
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::findThinPaints() {
  int lx = m_regionRas->getLx();
  int ly = m_regionRas->getLy();

  QList<int> regions;
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (!region.isInk()) continue;
    if (region.type == RegionInfo::MainInk) continue;

    int inkBoundary = 0;
    QMap<int, int>::ConstIterator it;
    for (it = region.links.begin(); it != region.links.end(); ++it) {
      int c            = it.key();
      int linkStrength = it.value();  // number of contact points
      if (c >= 0 && m_regions[c].isInk()) inkBoundary += linkStrength;
    }
    region.inkBoundary = inkBoundary;
    if (inkBoundary * 100 > region.perimeter * 80) regions.append(i);
  }

  for (int c : regions)
    m_regions[c].type = RegionInfo::SmallPaint;
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::findSuspectInks() {
  QMultiMap<TUINT32, int> paintColorTable;
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (0 == (region.type & RegionInfo::Paint)) continue;
    TPixel32 color = m_colors[region.colorIndex];
    if (color == TPixel32(0, 0, 0)) {
      int x = 1234;
      continue;
    }
    TUINT32 rawColor = *(TUINT32 *)&color;
    paintColorTable.insert(rawColor, i);
  }

  int count = 0;
  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.isInk() && region.links.size() == 2) {
      int ra = region.links.keys().at(0), rb = region.links.keys().at(1);
      if (ra >= 0 && rb >= 0) {
        if (!m_regions[ra].isInk()) qSwap(ra, rb);
        if (m_regions[ra].isInk() && !m_regions[rb].isInk()) {
          int sa = region.links[ra];
          int sb = region.links[rb];
          if (sa > sb) {
            region.type = RegionInfo::Paint;
            count++;
            continue;
          }
        }
      }
    }

    if (region.type != RegionInfo::ThinInk) continue;
    TUINT32 rawColor = *(TUINT32 *)&m_colors[region.colorIndex];
    if (paintColorTable.contains(rawColor)) {
      region.type = RegionInfo::Unknown;
      count++;
    }
  }

  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.isInk() && 10 <= region.pixelCount && region.pixelCount < 100) {
      int lx = region.x1 - region.x0 + 1;
      int ly = region.y1 - region.y0 + 1;
      int d  = std::max(lx, ly);
      if (std::min(lx, ly) * 2 > std::max(lx, ly) && region.pixelCount > d * d / 2) {
        region.type = RegionInfo::Paint;
      }
    }

    if (region.type == RegionInfo::Paint ||
        region.type == RegionInfo::Unknown) {
      bool isInk = false;
      if (region.boundaries.at(0) == 0) {
        if (region.boundaries.count() == 2)
          isInk = true;
        else if (region.boundaries.count() == 3) {
          int b1                 = region.boundaries.at(1);
          int b2                 = region.boundaries.at(2);
          if (b1 * 2 < b2) isInk = true;
        }
      }
      if (isInk) region.type = RegionInfo::Ink;
    }
  }
}

//-----------------------------------------------------------------------------

//

void Naa2TlvConverter::assignColorTypes() {
  if (!m_regionRas || !m_borderRas || m_regions.empty()) return;
  int lx = m_regionRas->getLx();
  int ly = m_regionRas->getLy();

  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    if (region.type != RegionInfo::Unknown) continue;
    QList<int> &bs = region.boundaries;
    if (bs[0] > 0) {
      region.type = RegionInfo::LargePaint;
    } else {
      int b = 0;
      for (int j = 1; j <= 2 && j < bs.count(); j++) b += bs[j];
      if (region.pixelCount > 200 &&
          (region.pixelCount - b) * 10 < region.pixelCount) {
        region.type = RegionInfo::ThinInk;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::addBorderInks()  // add syntethic inks: lines between two
                                        // adjacent fill-regions
{
  int lx                   = m_regionRas->getLx();
  int ly                   = m_regionRas->getLy();
  static const int dd[][2] = {{0, -1}, {-1, 0},  {1, 0},  {0, 1},
                              {1, 1},  {-1, -1}, {-1, 1}, {1, -1}};

  m_syntheticInkRas = new WorkRaster<unsigned char>(lx, ly);
  for (int i = 0; i < lx * ly; i++) m_syntheticInkRas->pixels(0)[i] = 0;

  // calculate brightness of all colors in order to decide the region on which
  // the border to be added
  QList<int> colorsBrightness;
  QVector<TPixel32>::iterator c;
  for (c = m_colors.begin(); c != m_colors.end(); ++c)
    colorsBrightness.append((int)(*c).r * 30 + (int)(*c).g * 59 +
                            (int)(*c).b * 11);

  int borderInkColorIndex = m_colors.count();
  m_colors.append(TPixel32(255, 0, 0));

  RegionInfo borderInkRegion;
  borderInkRegion.type       = RegionInfo::SyntheticInk;
  borderInkRegion.colorIndex = borderInkColorIndex;
  int borderInkRegionIndex   = m_regions.count();
  m_regions.append(borderInkRegion);

  for (int y = 0; y < ly; y++) {
    unsigned short *workScanLine  = m_regionRas->pixels(y);
    unsigned char *borderScanLine = m_borderRas->pixels(y);
    for (int x = 0; x < lx; x++) {
      int c = workScanLine[x];
      if (borderScanLine[x] != 1) continue;  // consider border pixel only
      if (0 == (m_regions[c].type & RegionInfo::Paint) &&
          m_regions[c].type != RegionInfo::Unknown)
        continue;  // consider paint pixels only

      // is touching a different no-ink pixel?
      bool touchesOtherRegion = false;
      for (int j = 0; j < 8; j++) {
        int x1 = x + dd[j][0], y1 = y + dd[j][1];
        if (0 <= x1 && x1 < lx && 0 <= y1 && y1 < ly) {
          int c1 = m_regionRas->pixels(y1)[x1];

          if (m_regions[c1].type == RegionInfo::Background) {
            touchesOtherRegion = true;
            break;
          } else if (m_regions[c1].type == RegionInfo::Unknown ||
                     (m_regions[c1].type & RegionInfo::Paint) != 0) {
            // OLD: note: we consider only regions with a lower index, to avoid
            // to create double border strokes
            // NEW: we put syntetic ink pixels in larger regions
            // UPDATE 2017/5/12 : we put ink on darker style
            if (colorsBrightness[m_regions[c1].colorIndex] >
                colorsBrightness[m_regions[c].colorIndex]) {
              touchesOtherRegion = true;
              break;
            }
          }
        }
      }

      if (touchesOtherRegion) {
        m_syntheticInkRas->pixels(y)[x] = 1;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::measureThickness() {
  QTime timer;
  timer.start();
  if (!m_regionRas || !m_borderRas) return;
  unsigned short *regionBuffer = m_regionRas->pixels();
  unsigned char *borderBuffer  = m_borderRas->pixels();
  int lx                       = m_regionRas->getLx();
  int ly                       = m_regionRas->getLy();

  if (!m_dotRas || m_dotRas->getLx() != lx || m_dotRas->getLy() != ly) {
    delete m_dotRas;
    m_dotRas = new WorkRaster<unsigned char>(lx, ly);
  }
  memset(m_dotRas->pixels(), 0, lx * ly);
  // for(int i=0;i<lx*ly;i++) m_dotRas->pixels()[i]=0;

  unsigned char *dotBuffer = m_dotRas->pixels();

  for (int y = 0; y < ly; y++) {
    for (int x = 0; x < lx; x++) {
      if (borderBuffer[y * lx + x] != 1) continue;
      if (dotBuffer[y * lx + x] != 0) continue;
      int regionId       = regionBuffer[y * lx + x];
      RegionInfo &region = m_regions[regionId];
      int type           = region.type;
      if (type == RegionInfo::Background || type == RegionInfo::LargePaint ||
          type == RegionInfo::ThinInk)
        continue;

      int thickness = measureThickness(x, y);
      if (thickness < 1) continue;
      QMap<int, int>::Iterator it = region.thicknessHistogram.find(thickness);
      if (it != region.thicknessHistogram.end())
        it.value() += 1;
      else
        region.thicknessHistogram.insert(thickness, 1);
    }
  }

  for (int i = 0; i < m_regions.count(); i++) {
    RegionInfo &region = m_regions[i];
    int type           = region.type;
    if (type == RegionInfo::Background || type == RegionInfo::LargePaint ||
        type == RegionInfo::ThinInk)
      continue;
    int thicknessCount = 0;
    double thickness   = 0.0;
    for (QMap<int, int>::Iterator it = region.thicknessHistogram.begin();
         it != region.thicknessHistogram.end(); ++it) {
      thicknessCount += it.value();
      thickness += it.key() * it.value();
    }
    if (thicknessCount > 0) {
      thickness *= 1.0 / thicknessCount;
      region.thickness = thickness;
    }
  }
  qDebug() << "measure thickness. time=" << timer.elapsed();
}

//-----------------------------------------------------------------------------

int Naa2TlvConverter::measureThickness(int x0, int y0) {
  if (!m_regionRas || !m_borderRas || !m_dotRas) return -1;
  unsigned short *regionBuffer = m_regionRas->pixels();
  unsigned char *borderBuffer  = m_borderRas->pixels();
  unsigned char *dotBuffer     = m_dotRas->pixels();

  int lx = m_regionRas->getLx();
  int ly = m_regionRas->getLy();

  int k0 = lx * y0 + x0;
  // no needs to worry about image limits
  if (x0 - 1 < 0 || x0 + 1 >= lx || y0 - 1 < 0 || y0 + 1 >= ly) return -1;
  // we must start from a border
  if (borderBuffer[k0] != 1) return -1;

  // far from other measures
  if (dotBuffer[k0] != 0) return -1;

  int regionId     = regionBuffer[k0];
  RegionInfo &info = m_regions[regionId];

  // directions
  const int dd[8] = {1, lx + 1, lx, lx - 1, -1, -1 - lx, -lx, 1 - lx};

  // a is a direction index; a : inside; a+1 : outside
  int a = 0;
  while (a < 8 &&
         !(regionBuffer[k0 + dd[a]] == regionId &&
           regionBuffer[k0 + dd[(a + 1) % 8]] != regionId))
    a++;
  if (a == 8) {
    // k0 is an isolated point or (strange!) an intern point
    qDebug() << "Isolated point or intern point";
    return -1;
  }
  int ka = k0 + dd[a];

  int b                                          = (a + 2) % 8;
  while (regionBuffer[k0 + dd[b]] != regionId) b = (b + 1) % 8;
  // a..b = boundaries
  int kb = k0 + dd[b];
  if (a == b || ((b + 1) % 8) == a) return -1;  // k0 is a corner

  dotBuffer[k0] = 1;
  dotBuffer[ka] = 2;
  dotBuffer[kb] = 3;

  int c       = (b + 1) % 8;
  bool isThin = false;
  while (c != a) {
    if (regionBuffer[k0 + dd[c]] != regionId) {
      isThin = true;
      break;
    }
    c = (c + 1) % 8;
  }
  if (isThin) {
    // stroke larga un pixel, con paint da una parte e dall'altra.
    return 1;
  }

  int baseLength = 3;

  int d      = b;
  int oldk   = k0;
  int k      = kb;
  int lastd2 = 0;
  for (int i = 0; i < baseLength; i++) {
    int x = k % lx;
    int y = k / lx;
    if (x <= 1 || x >= lx - 1 || y <= 1 || y >= ly - 1)
      break;  // just to be sure
    int d2 = (x - x0) * (x - x0) + (y - y0) * (y - y0);
    if (d2 <= lastd2) break;
    lastd2                                          = d2;
    int d1                                          = (d + 4) % 8;
    d1                                              = (d1 + 1) % 8;
    while (regionBuffer[k + dd[d1]] != regionId) d1 = (d1 + 1) % 8;
    Q_ASSERT(regionBuffer[k + dd[d1]] == regionId);
    oldk         = k;
    k            = k + dd[d1];
    d            = d1;
    dotBuffer[k] = 4;
  }

  // punto ad un estremo (lungo il confine)
  int x1 = k % lx;
  int y1 = k / lx;

  d      = a;
  oldk   = k0;
  k      = ka;
  lastd2 = 0;
  for (int i = 0; i < baseLength; i++) {
    int x = k % lx;
    int y = k / lx;
    if (x <= 1 || x >= lx - 1 || y <= 1 || y >= ly - 1)
      break;  // just to be sure
    int d2 = (x - x0) * (x - x0) + (y - y0) * (y - y0);
    if (d2 <= lastd2) break;
    lastd2                                          = d2;
    int d1                                          = (d + 4) % 8;
    d1                                              = (d1 + 7) % 8;
    while (regionBuffer[k + dd[d1]] != regionId) d1 = (d1 + 7) % 8;
    Q_ASSERT(regionBuffer[k + dd[d1]] == regionId);
    oldk         = k;
    k            = k + dd[d1];
    d            = d1;
    dotBuffer[k] = 5;
  }

  // punto all'estremo opposto (lungo il confine)
  int x2 = k % lx;
  int y2 = k / lx;

  int dx = y2 - y1, dy = x1 - x2;

  int delta2 = dx * dx + dy * dy;
  if (delta2 < baseLength * baseLength * 3) return -1;

  int adx = abs(dx), ady = abs(dy);
  int sgx = dx > 0 ? 1 : -1, sgy = dy > 0 ? 1 : -1;
  int thickness          = 1;
  const int maxThickness = 64;
  for (; thickness < maxThickness; thickness++) {
    int x, y;
    if (adx > ady) {
      x = x0 + thickness * sgx;
      y = y0 + sgy * (thickness * ady + adx / 2) / adx;
    } else {
      y = y0 + thickness * sgy;
      x = x0 + sgx * (thickness * adx + ady / 2) / ady;
    }
    k = y * lx + x;
    if (0 <= x && x < lx && 0 <= y && y < ly && regionBuffer[k] == regionId)
      dotBuffer[k] = 6;
    else
      break;
  }
  return thickness;
}

//-----------------------------------------------------------------------------

TToonzImageP Naa2TlvConverter::makeTlv(bool transparentSyntheticInks,
                                       QList<int> &usedStyleIds, double dpi) {
  if (!m_valid || m_colors.empty() || m_regions.empty() || !m_regionRas)
    return TToonzImageP();
  int lx                = m_regionRas->getLx();
  int ly                = m_regionRas->getLy();
  TPalette *palette     = m_palette;
  if (!palette) palette = new TPalette();

  TRasterCM32P ras(lx, ly);

  QList<int> styleIds;

  for (int i = 0; i < m_colors.count() - 1; i++) {
    TPixel32 color  = m_colors.at(i);
    int styleId     = palette->getClosestStyle(color);
    TColorStyle *cs = styleId < 0 ? 0 : palette->getStyle(styleId);
    if (cs) {
      if (cs->getMainColor() != color) cs = 0;
    }
    if (cs == 0) {
      styleId = palette->addStyle(color);
      palette->getPage(0)->addStyle(styleId);
      cs = palette->getStyle(styleId);
    }
    styleIds.append(styleId);
    if (!usedStyleIds.contains(styleId)) usedStyleIds.append(styleId);
  }
  styleIds.append(0);  // syntetic ink

  // int synteticInkStyleId = palette->getPage(0)->addStyle(TPixel32(0,0,0,0));
  // styleIds.append(synteticInkStyleId);

  for (int y = 0; y < ly; y++) {
    unsigned short *workScanLine = m_regionRas->pixels(y);
    TPixelCM32 *outScanLine      = ras->pixels(y);
    for (int x = 0; x < lx; x++) {
      int c = workScanLine[x];
      Q_ASSERT(0 <= c && c < m_regions.count());

      int color = m_regions[c].colorIndex;
      Q_ASSERT(0 <= color && color < styleIds.count());

      int styleId = styleIds.at(color);

      RegionInfo::Type type = m_regions.at(c).type;
      if (type == RegionInfo::Background)
        outScanLine[x] = TPixelCM32();
      else if (type & RegionInfo::Ink)
        outScanLine[x] = TPixelCM32(styleId, 0, 0);
      else if (m_syntheticInkRas->pixels(y)[x] == 1)
        outScanLine[x] =
            TPixelCM32(transparentSyntheticInks ? 0 : styleId, 0, 0);
      else
        outScanLine[x] = TPixelCM32(0, styleId, 255);
    }
  }

  struct locals {
    static bool compare(const QPair<int, int> &p1, const QPair<int, int> &p2) {
      return p1.second > p2.second;
    }

    static void addPaint(QList<QPair<int, int>> &list, const int paintId) {
      if (paintId == 0) return;
      for (int i = 0; i < list.size(); i++)
        if (list[i].first == paintId) {
          list[i].second += 1;
          return;
        }
      list.append(QPair<int, int>(paintId, 1));
    }
  };  // locals

  // Here, expand paint area under the solid ink pixel in order to prevent the
  // gap appears when the "Add Antialiasing" option is activated.
  TRasterCM32P copiedRas = ras->clone();
  copiedRas->lock();
  for (int y = 0; y < ly; y++) {
    TPixelCM32 *topScanLine    = copiedRas->pixels((y == 0) ? y : y - 1);
    TPixelCM32 *middleScanLine = copiedRas->pixels(y);
    TPixelCM32 *bottomScanLine = copiedRas->pixels((y == ly - 1) ? y : y + 1);
    TPixelCM32 *outScanLine    = ras->pixels(y);
    for (int x = 0; x < lx; x++) {
      if (!middleScanLine[x].isPureInk() || middleScanLine[x].getPaint() != 0)
        continue;

      int prev_x = (x == 0) ? x : x - 1;
      int next_x = (x == lx - 1) ? x : x + 1;
      QList<QPair<int, int>> neighborPaints;
      locals::addPaint(neighborPaints, topScanLine[prev_x].getPaint());
      locals::addPaint(neighborPaints, topScanLine[x].getPaint());
      locals::addPaint(neighborPaints, topScanLine[next_x].getPaint());
      locals::addPaint(neighborPaints, middleScanLine[prev_x].getPaint());
      locals::addPaint(neighborPaints, middleScanLine[next_x].getPaint());
      locals::addPaint(neighborPaints, bottomScanLine[prev_x].getPaint());
      locals::addPaint(neighborPaints, bottomScanLine[x].getPaint());
      locals::addPaint(neighborPaints, bottomScanLine[next_x].getPaint());
      std::sort(neighborPaints.begin(), neighborPaints.end(), locals::compare);

      if (!neighborPaints.isEmpty())
        outScanLine[x].setPaint(neighborPaints[0].first);
    }
  }
  copiedRas->unlock();

  TToonzImageP ti = new TToonzImage(ras, ras->getBounds());
  ti->setPalette(palette);

  if (dpi > 0.0)  // for now, accept only square pixel
    ti->setDpi(dpi, dpi);
  else
    ti->setDpi(72, 72);

  return ti;
}

//-----------------------------------------------------------------------------

TVectorImageP Naa2TlvConverter::vectorize(const TToonzImageP &ti) {
  CenterlineConfiguration conf;
  if (!ti) return TVectorImageP();
  TPalette *palette = ti->getPalette();

  VectorizerCore vc;

  TAffine dpiAff;
  double factor = Stage::inch;
  double dpix = factor / 72, dpiy = factor / 72;

  ti->getDpi(dpix, dpiy);
  TPointD center = ti->getRaster()->getCenterD();

  if (dpix != 0.0 && dpiy != 0.0) dpiAff = TScale(factor / dpix, factor / dpiy);
  factor                                 = norm(dpiAff * TPointD(1, 0));

  conf.m_affine         = dpiAff * TTranslation(-center);
  conf.m_thickScale     = factor;
  conf.m_leaveUnpainted = false;
  conf.m_makeFrame      = true;
  conf.m_penalty        = 0.0;
  conf.m_despeckling    = 0;

  TImageP img(ti.getPointer());
  TVectorImageP vi = vc.vectorize(img, conf, palette);
  vi->setPalette(palette);
  return vi;
}

//-----------------------------------------------------------------------------

void Naa2TlvConverter::removeUnusedStyles(const QList<int> &styleIds) {
  // Remove unused styles from input palette
  if (!m_palette) return;
  for (int p = m_palette->getPageCount() - 1; p >= 0; p--) {
    TPalette::Page *page = m_palette->getPage(p);
    for (int s = page->getStyleCount() - 1; s >= 0; s--) {
      int styleId = page->getStyleId(s);
      if (styleId == -1) continue;
      // check if the style is used or not
      if (!styleIds.contains(styleId)) page->removeStyle(s);
    }
    // erase empty page
    if (page->getStyleCount() == 0) m_palette->erasePage(p);
  }
}
