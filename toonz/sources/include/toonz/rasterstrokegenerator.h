#pragma once

#include "trastercm.h"
#include "ttilesaver.h"
#include "tstopwatch.h"
#include <fstream>
#include <QSet>
#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

enum Tasks { BRUSH, ERASE, PAINTBRUSH, FINGER };

enum ColorType { INK, PAINT, INKNPAINT, NONE };

class DVAPI RasterStrokeGenerator {
  typedef std::vector<TThickPoint>::iterator Iterator;

  TRasterCM32P
      m_raster;  // L'immagine raster sulla quale dobbiamo disegnare lo stroke
  std::vector<TThickPoint> m_points;  // Il vettore di punti che rappresentano
                                      // la spina dorsale dello stroke
  int m_styleId;                      // L'ink-style dello stroke
  bool m_selective;
  TRect m_boxOfRaster;  // Un rettangolo della stessa dimensione di "m_raster"
  ColorType m_colorType;
  Tasks m_task;
  int m_eraseStyle;
  int m_selectedStyle;
  bool m_keepAntiAlias;
  bool m_doAnArc;
  bool m_isPaletteOrder;  // Used in the Draw Order option of Brush Tool,
                          // use style order to define line stacking order
  QSet<int> m_aboveStyleIds;

  // Ricalcola i punti in un nuovo sistema di riferimento
  void translatePoints(std::vector<TThickPoint> &points,
                       const TPoint &newOrigin) const;

  // Effettua la over.
  void placeOver(const TRasterCM32P &out, const TRasterCM32P &in,
                 const TPoint &p) const;

public:
  RasterStrokeGenerator(const TRasterCM32P &raster, Tasks task,
                        ColorType colorType, int styleId, const TThickPoint &p,
                        bool selective, int selectedStyle, bool keepAntialias,
                        bool isPaletteOrder = false);
  ~RasterStrokeGenerator();
  void setRaster(const TRasterCM32P &ras) { m_raster = ras; }
  void setStyle(int styleId) { m_styleId = styleId; }
  int getStyleId() const { return m_styleId; }
  bool isSelective() { return m_selective; }

  bool isPaletteOrder() { return m_isPaletteOrder; }
  void setAboveStyleIds(QSet<int> &ids) { m_aboveStyleIds = ids; }

  // Inserisce un punto in "m_points"
  void add(const TThickPoint &p);

  // Disegna il tratto interamente
  void generateStroke(bool isPencil) const;

  // Ritorna m_points
  std::vector<TThickPoint> getPointsSequence() { return m_points; }
  void setPointsSequence(const std::vector<TThickPoint> &points) {
    m_points = points;
  }

  // Ritorna il rettangolo contenente i dischi generati con centri in "points" e
  // raggio "points.thick" +2 pixel a bordo
  TRect getBBox(const std::vector<TThickPoint> &points) const;

  TRect getLastRect() const;

  TRect generateLastPieceOfStroke(bool isPencil, bool closeStroke = false);
};
