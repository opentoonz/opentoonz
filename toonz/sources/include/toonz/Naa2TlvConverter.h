#pragma once

#ifndef NAA2TLVCONVERTER_H
#define NAA2TLVCONVERTER_H

#include "tcommon.h"
#include "tpixel.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"

#include <QPoint>
#include <QString>
#include <QMap>
#include <QList>
#include <QVector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

template <class T>
class WorkRaster {
public:
  typedef T Pixel;
  WorkRaster(int lx, int ly)
      : m_lx(lx), m_ly(ly), m_buffer(new Pixel[lx * ly]) {}

  inline int getLx() const { return m_lx; }
  inline int getLy() const { return m_ly; }
  inline Pixel *pixels(int y = 0) const { return m_buffer.get() + m_lx * y; }

private:
  std::unique_ptr<Pixel[]> m_buffer;
  int m_lx, m_ly;
};

struct DVAPI RegionInfo {
  int colorIndex;
  int pixelCount;
  QMap<int, int> links;  // region -> link strength (i.e. number of connections)
  QList<int> boundaries;
  QMap<int, int> thicknessHistogram;
  double thickness;
  int perimeter;
  int inkBoundary;
  double disc;
  double param1;
  enum Type {
    Unknown      = 0x0,
    Background   = 0x1,
    Ink          = 0x2,
    SyntheticInk = (0x100 | 0x2),   // generated) ink between paint regions
    MainInk      = (0x200 | 0x2),   // ink adjacent to background region
    ThinInk      = (0x1000 | 0x2),  // ink adjacent to background region
    Paint        = 0x4,
    LargePaint   = (0x400 | 0x4),
    SmallPaint   = (0x800 | 0x4),
    Unused       = 0x8000  // see separateRegions()
  };
  Type type;
  QPoint pos;  // un pixel interno (serve per rintracciare la regione sullo
               // schermo)
  int x0, y0, x1, y1;

  RegionInfo()
      : colorIndex(-1)
      , pixelCount(-1)
      , type(Unknown)
      , thickness(0)
      , perimeter(0)
      , inkBoundary(0)
      , disc(0)
      , param1(0)
      , x0(0)
      , y0(0)
      , x1(-1)
      , y1(-1) {}

  void touchRegion(int regionId) {
    QMap<int, int>::Iterator it = links.find(regionId);
    if (it == links.end())
      links.insert(regionId, 1);
    else
      it.value() += 1;
  }

  inline bool isBackground() const { return type == RegionInfo::Background; }
  inline bool isInk() const { return (type & RegionInfo::Ink) != 0; }

  QString getTypeString() const;
};

class DVAPI Naa2TlvConverter {
public:
  const int MaxColorCount;
  WorkRaster<unsigned short> *m_regionRas;
  WorkRaster<unsigned char> *m_borderRas;
  WorkRaster<unsigned char> *m_dotRas;
  WorkRaster<unsigned char> *m_syntheticInkRas;
  QVector<TPixel32> m_colors;
  QVector<RegionInfo> m_regions;
  double m_inkThickness;
  TPalette *m_palette;
  bool m_valid;

  Naa2TlvConverter();
  ~Naa2TlvConverter();

  void setSourceImage(const TRaster32P &srcRas);
  void setPalette(TPalette *palette);
  TPalette *getPalette() const { return m_palette; }

  void separateRegions();
  void computeLinks();
  void findRegionBorders();
  void erodeRegions();
  void computeMainInkThickness();
  void assignColorTypes();
  void addBorderInks();
  void findBackgroundRegions();
  void findMainInks();
  void findLargePaints();
  void findThinInks();
  void findPaints();
  void findPaints2();
  void findThinPaints();
  void findSuspectInks();

  void measureThickness();

  int measureThickness(int x, int y);

  void process(const TRaster32P &srcRas);

  int getRegionIndex(int x, int y) const {
    if (!!m_regionRas && 0 <= x && x < m_regionRas->getLx() && 0 <= y &&
        y < m_regionRas->getLy())
      return m_regionRas->pixels(y)[x];
    else
      return -1;
  }

  TToonzImageP makeTlv(bool transparentSyntheticInks, QList<int> &usedStyleIds,
                       double dpi = 0.0);

  TVectorImageP vectorize(const TToonzImageP &ti);
  TVectorImageP vectorize(const TRaster32P &ras);
  TVectorImageP vectorize();

  void removeUnusedStyles(const QList<int> &styleIds);
};

#endif
