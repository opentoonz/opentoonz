

#include "toonz/rasterbrush.h"

#include <vector>
#include <tcurves.h>

using namespace std;

namespace {

//=====================================================================================
// forward declaration
void lightPixel(const TRasterCM32P &ras, const TPoint &pix, double distance,
                int styleId, bool checkAntialiasedPixel);

class ConeSubVolume {
  const static double m_values[21];

public:
  ConeSubVolume() {}

  // calcola il sottovolume di un cono di raggio e volume unitario in base
  static double compute(double cover) {
    double x = (10 * tcrop(cover, -1.0, 1.0)) + 10;
    assert(0 <= x && x <= 20);
    int i = tfloor(x);
    if (i == 20)
      return m_values[i];
    else
      // Interpolazione lineare.
      return (-(x - (i + 1)) * m_values[i]) - (-(x - i) * m_values[i + 1]);
  }
};

const double ConeSubVolume::m_values[] = {

    1.0,      0.99778,  0.987779,  0.967282,  0.934874,  0.889929,   0.832457,
    0.763067, 0.683002, 0.594266,  0.5,       0.405734,  0.316998,   0.236933,
    0.167543, 0.110071, 0.0651259, 0.0327182, 0.0122208, 0.00221986, 0.0};

//========================================================================================================

// Permette di disegnare un disco pieno
class Disk {
  TPointD m_centre;
  double m_radius;
  bool m_doAntialias;

  // Traccia una linea tra due punti per riempire il disco
  void fill(const TRasterCM32P &ras, const TPoint &p1, const TPoint &p2,
            int styleId) const {
    if (p1.y == p2.y) {
      int xMax, xMin;
      if (p1.x > p2.x) {
        xMax = p1.x;
        xMin = p2.x;
      } else {
        xMin = p1.x;
        xMax = p2.x;
      }
      TPixelCM32 color(styleId, 0, 0);
      for (int i = xMin; i <= xMax; i++) ras->pixels(p1.y)[i] = color;
    } else {
      int yMax, yMin;
      if (p1.y > p2.y) {
        yMax = p1.y;
        yMin = p2.y;
      } else {
        yMin = p1.y;
        yMax = p2.y;
      }
      TPixelCM32 color(styleId, 0, 0);
      for (int i = yMin; i <= yMax; i++) ras->pixels(i)[p1.x] = color;
    }
  }

  // Calcola la distanza di un pixel dal centro "reale"
  inline double distancePointToCentre(const TPoint &point) const {
    double d = sqrt((point.x - m_centre.x) * (point.x - m_centre.x) +
                    (point.y - m_centre.y) * (point.y - m_centre.y));
    return d;
  }

  // Calcola la distanza tra un punto della cironferenza ideale ed il pixel che
  // la approssima
  // Inoltre calcola la distanza dei pixel vicini per gestire l'antialias
  void computeDistances(double distances[3], const TPoint &point,
                        const TPoint &centre, bool upperPoint) const {
    TPoint p = point - centre;
    if (upperPoint) {
      double d     = distancePointToCentre(point);
      distances[0] = d - m_radius;
      d            = distancePointToCentre(TPoint(point.x, point.y + 1));
      distances[1] = d - m_radius;
      d            = distancePointToCentre(TPoint(point.x, point.y - 1));
      distances[2] = d - m_radius;
      return;
    } else {
      double d     = distancePointToCentre(point);
      distances[0] = d - m_radius;
      d            = distancePointToCentre(TPoint(point.x + 1, point.y));
      distances[1] = d - m_radius;
      d            = distancePointToCentre(TPoint(point.x - 1, point.y));
      distances[2] = d - m_radius;
      return;
    }
  }

  // Illumina otto pixel symmetrici per ottenere la circonferenza del disco.
  // Per ogni pixel illumina anche i vicini per gestire l'antialias.
  void makeAntiAliasedDiskBorder(const TRasterCM32P &ras, const TPoint &centre,
                                 const TPoint &point,
                                 double distancesAntialias[3], int styleId,
                                 int maxPointToFill) const {
    computeDistances(distancesAntialias, point + centre, centre, true);
    lightPixel(ras, point + centre, distancesAntialias[0], styleId, false);
    lightPixel(ras, TPoint(point.x, point.y + 1) + centre,
               distancesAntialias[1], styleId, false);
    lightPixel(ras, TPoint(point.x, point.y - 1) + centre,
               distancesAntialias[2], styleId, false);

    computeDistances(distancesAntialias, TPoint(point.y, point.x) + centre,
                     centre, false);
    lightPixel(ras, TPoint(point.y, point.x) + centre, distancesAntialias[0],
               styleId, false);
    lightPixel(ras, TPoint(point.y + 1, point.x) + centre,
               distancesAntialias[1], styleId, false);
    lightPixel(ras, TPoint(point.y - 1, point.x) + centre,
               distancesAntialias[2], styleId, false);

    computeDistances(distancesAntialias, TPoint(-point.x, -point.y) + centre,
                     centre, true);
    lightPixel(ras, TPoint(-point.x, -point.y) + centre, distancesAntialias[0],
               styleId, false);
    lightPixel(ras, TPoint(-point.x, -point.y - 1) + centre,
               distancesAntialias[2], styleId, false);
    lightPixel(ras, TPoint(-point.x, -point.y + 1) + centre,
               distancesAntialias[1], styleId, false);

    computeDistances(distancesAntialias,
                     TPoint(-point.y + centre.x, point.x + centre.y), centre,
                     false);
    lightPixel(ras, TPoint(-point.y, point.x) + centre, distancesAntialias[0],
               styleId, false);
    lightPixel(ras, TPoint(-point.y - 1, point.x) + centre,
               distancesAntialias[2], styleId, false);
    lightPixel(ras, TPoint(-point.y + 1, point.x) + centre,
               distancesAntialias[1], styleId, false);

    if ((point.x + centre.x) != (centre.x)) {
      computeDistances(distancesAntialias, TPoint(point.y, -point.x) + centre,
                       centre, false);
      lightPixel(ras, TPoint(point.y, -point.x) + centre, distancesAntialias[0],
                 styleId, false);
      lightPixel(ras, TPoint(point.y + 1, -point.x) + centre,
                 distancesAntialias[1], styleId, false);
      lightPixel(ras, TPoint(point.y - 1, -point.x) + centre,
                 distancesAntialias[2], styleId, false);

      computeDistances(distancesAntialias, TPoint(point.x, -point.y) + centre,
                       centre, true);
      lightPixel(ras, TPoint(point.x, -point.y) + centre, distancesAntialias[0],
                 styleId, false);
      lightPixel(ras, TPoint(point.x, -point.y - 1) + centre,
                 distancesAntialias[2], styleId, false);
      lightPixel(ras, TPoint(point.x, -point.y + 1) + centre,
                 distancesAntialias[1], styleId, false);

      computeDistances(distancesAntialias, TPoint(-point.y, -point.x) + centre,
                       centre, false);
      lightPixel(ras, TPoint(-point.y, -point.x) + centre,
                 distancesAntialias[0], styleId, false);
      lightPixel(ras, TPoint(-point.y - 1, -point.x) + centre,
                 distancesAntialias[2], styleId, false);
      lightPixel(ras, TPoint(-point.y + 1, -point.x) + centre,
                 distancesAntialias[1], styleId, false);

      computeDistances(distancesAntialias, TPoint(-point.x, point.y) + centre,
                       centre, true);
      lightPixel(ras, TPoint(-point.x, point.y) + centre, distancesAntialias[0],
                 styleId, false);
      lightPixel(ras, TPoint(-point.x, point.y + 1) + centre,
                 distancesAntialias[1], styleId, false);
      lightPixel(ras, TPoint(-point.x, point.y - 1) + centre,
                 distancesAntialias[2], styleId, false);
    }
    if (maxPointToFill <= point.y - 2) {
      fill(ras, TPoint(point.x, maxPointToFill) + centre,
           TPoint(point.x, point.y - 2) + centre, styleId);
      fill(ras, TPoint(maxPointToFill, point.x) + centre,
           TPoint(point.y - 2, point.x) + centre, styleId);
      fill(ras, TPoint(point.x, -maxPointToFill) + centre,
           TPoint(point.x, -point.y + 2) + centre, styleId);
      fill(ras, TPoint(-maxPointToFill, -point.x) + centre,
           TPoint(-point.y + 2, -point.x) + centre, styleId);

      if (point.x != 0) {
        fill(ras, TPoint(maxPointToFill, -point.x) + centre,
             TPoint(point.y - 2, -point.x) + centre, styleId);
        fill(ras, TPoint(-point.x, -maxPointToFill) + centre,
             TPoint(-point.x, -point.y + 2) + centre, styleId);
        fill(ras, TPoint(-maxPointToFill, point.x) + centre,
             TPoint(-point.y + 2, point.x) + centre, styleId);
        fill(ras, TPoint(-point.x, maxPointToFill) + centre,
             TPoint(-point.x, point.y - 2) + centre, styleId);
      }
    }
  }

  void makeNoAntiAliasedDiskBorder(const TRasterCM32P &ras,
                                   const TPoint &centre, const TPoint &point,
                                   int styleId, int maxPointToFill) const {
    if (((int)(m_radius * 2)) % 2 == 0) {
      lightPixel(ras, TPoint(point.x - 1, point.y) + centre, -1, styleId,
                 false);
      lightPixel(ras, TPoint(point.y - 1, -point.x + 1) + centre, -1, styleId,
                 false);
      lightPixel(ras, TPoint(-point.x, -point.y + 1) + centre, -1, styleId,
                 false);
      lightPixel(ras, TPoint(-point.y, point.x) + centre, -1, styleId, false);
    } else {
      lightPixel(ras, point + centre, -1, styleId, false);
      lightPixel(ras, TPoint(point.y, -point.x) + centre, -1, styleId, false);
      lightPixel(ras, TPoint(-point.x, -point.y) + centre, -1, styleId, false);
      lightPixel(ras, TPoint(-point.y, point.x) + centre, -1, styleId, false);
    }

    if ((point.x + centre.x) != (centre.x)) {
      if (((int)(m_radius * 2)) % 2 == 0) {
        lightPixel(ras, TPoint(point.y - 1, point.x) + centre, -1, styleId,
                   false);
        lightPixel(ras, TPoint(point.x - 1, -point.y + 1) + centre, -1, styleId,
                   false);
        lightPixel(ras, TPoint(-point.y, -point.x + 1) + centre, -1, styleId,
                   false);
        lightPixel(ras, TPoint(-point.x, point.y) + centre, -1, styleId, false);
      } else {
        lightPixel(ras, TPoint(point.y, point.x) + centre, -1, styleId, false);
        lightPixel(ras, TPoint(point.x, -point.y) + centre, -1, styleId, false);
        lightPixel(ras, TPoint(-point.y, -point.x) + centre, -1, styleId,
                   false);
        lightPixel(ras, TPoint(-point.x, point.y) + centre, -1, styleId, false);
      }
    }
    if (maxPointToFill <= point.y - 1) {
      if (((int)(m_radius * 2)) % 2 == 0) {
        fill(ras, TPoint(point.x - 1, 0) + centre,
             TPoint(point.x - 1, point.y) + centre, styleId);
        fill(ras, TPoint(0, -point.x + 1) + centre,
             TPoint(point.y - 1, -point.x + 1) + centre, styleId);
        fill(ras, TPoint(-point.x, 0) + centre,
             TPoint(-point.x, -point.y + 1) + centre, styleId);
        fill(ras, TPoint(0, point.x) + centre,
             TPoint(-point.y, point.x) + centre, styleId);
        if (point.x != 0) {
          fill(ras, TPoint(0, point.x) + centre,
               TPoint(point.y - 1, point.x) + centre, styleId);
          fill(ras, TPoint(point.x - 1, 0) + centre,
               TPoint(point.x - 1, -point.y + 1) + centre, styleId);
          fill(ras, TPoint(0, -point.x + 1) + centre,
               TPoint(-point.y, -point.x + 1) + centre, styleId);
          fill(ras, TPoint(-point.x, 0) + centre,
               TPoint(-point.x, point.y) + centre, styleId);
        }
      } else {
        fill(ras, TPoint(point.x, 0) + centre,
             TPoint(point.x, point.y) + centre, styleId);
        fill(ras, TPoint(0, -point.x) + centre,
             TPoint(point.y, -point.x) + centre, styleId);
        fill(ras, TPoint(-point.x, 0) + centre,
             TPoint(-point.x, -point.y) + centre, styleId);
        fill(ras, TPoint(0, point.x) + centre,
             TPoint(-point.y, point.x) + centre, styleId);

        if (point.x != 0) {
          fill(ras, TPoint(0, point.x) + centre,
               TPoint(point.y, point.x) + centre, styleId);
          fill(ras, TPoint(point.x, -0) + centre,
               TPoint(point.x, -point.y) + centre, styleId);
          fill(ras, TPoint(0, -point.x) + centre,
               TPoint(-point.y, -point.x) + centre, styleId);
          fill(ras, TPoint(-point.x, 0) + centre,
               TPoint(-point.x, point.y) + centre, styleId);
        }
      }
    }
  }

public:
  Disk() : m_centre(0.0, 0.0), m_radius(1.0), m_doAntialias(true) {}

  Disk(const TThickPoint &p, bool doAntialias) : m_doAntialias(doAntialias) {
    if (m_doAntialias) {
      m_centre = TPointD(p.x, p.y);
      m_radius = p.thick * 0.5;
    } else {
      m_centre = TPointD(tround(p.x), tround(p.y));
      m_radius = tround(p.thick) * 0.5;
    }
  }

  // Disegna un disco
  void draw(const TRasterCM32P &ras, int styleId) const {
    double distances[3];
    int maxPointToFill = 0;
    TPoint centre      = convert(m_centre);
    TPoint point;
    if (m_doAntialias) {
      TPoint point(0, tround(m_radius));
      int d = 1 - tround(m_radius), dE = 3, dSE = -2 * tround(m_radius) + 5;
      makeAntiAliasedDiskBorder(ras, centre, point, distances, styleId,
                                maxPointToFill);

      while (point.y > point.x) {
        if (d < 0) {
          d += dE;
          dE += 2;
          dSE += 2;
          point.x++;
          maxPointToFill++;
        } else {
          d += dSE;
          dE += 2;
          dSE += 4;
          point.x++;
          point.y--;
          maxPointToFill++;
        }
        makeAntiAliasedDiskBorder(ras, centre, point, distances, styleId,
                                  maxPointToFill);
      }
    } else {
      TPoint point(0, tround(m_radius - 0.5));
      int d = 3 - 2 * (int)m_radius;
      while (point.y > point.x) {
        makeNoAntiAliasedDiskBorder(ras, centre, point, styleId,
                                    maxPointToFill);
        if (d < 0)
          d = d + 4 * point.x + 6;
        else {
          d = d + 4 * (point.x - point.y) + 10;
          point.y--;
        }
        maxPointToFill++;
        point.x++;
      }
      if (point.x == point.y)
        makeNoAntiAliasedDiskBorder(ras, centre, point, styleId,
                                    maxPointToFill);
    }
  }

  TPointD getCentre() const { return m_centre; }

  double getRadius() const { return m_radius; }

  void setCentre(const TPointD &centre) { m_centre = centre; }

  void setCentre(double x, double y) {
    m_centre.x = x;
    m_centre.y = y;
  }

  void setRadius(double radius) { m_radius = radius; }
};

//===============================================================================================================

// Calcola la distanza di una curva al parametro "t" dalla retta che unisce i
// punti nei parametri "tPrevious" e "tNext"
double findChordalDeviation(const TQuadratic &quadratic, double t,
                            double tPrevious, double tNext) {
  TPointD pPrevious = quadratic.getPoint(tPrevious);
  TPointD pNext     = quadratic.getPoint(tNext);
  TPointD p         = quadratic.getPoint(t);
  TPointD P(p - pPrevious), Q(pNext - pPrevious);
  double PQ = P * Q;
  double QQ = Q * Q;
  return norm(P - (PQ / QQ) * Q);
}

// Accende un pixel calcolandone l'intensita'
/*-- 筆先のアンチエイリアス部分の描画 --*/
void lightPixel(const TRasterCM32P &ras, const TPoint &pix, double distance,
                int styleId, bool checkAntialiasedPixel) {
  TPixelCM32 pixel      = ras->pixels(pix.y)[pix.x];
  double volumeParziale = ConeSubVolume::compute(distance);

  /*- 現在のToneに乗算していく -*/
  int newTone = tround((double)pixel.getTone() * (1.0 - volumeParziale));
  assert(newTone >= 0 && newTone <= 255);
  ras->pixels(pix.y)[pix.x] = TPixelCM32(styleId, pixel.getPaint(), newTone);
}

bool isDiskNecessaryNecessary(const TQuadratic &quadratic, double tCurrent,
                              double lastT, double nextT,
                              bool &idLastDiskDrown) {
  TPoint currentPoint = convert(quadratic.getPoint(tCurrent));
  TPoint lastPoint    = convert(quadratic.getPoint(lastT));
  TPoint nextPoint    = convert(quadratic.getPoint(nextT));
  TPoint lastDiff     = currentPoint - lastPoint;
  TPoint nextDiff     = currentPoint - nextPoint;
  TPoint otherDiff    = lastPoint - nextPoint;
  bool isLastNear     = lastDiff.x == 0 || lastDiff.y == 0;
  bool isNextNear     = nextDiff.x == 0 || nextDiff.y == 0;
  bool isOtheNear     = otherDiff.x == 0 || otherDiff.y == 0;
  if (isLastNear && isNextNear && !isOtheNear && idLastDiskDrown) {
    idLastDiskDrown = false;
    return false;
  }
  return true;
}

// Disegna una porzione di del tratto (un piccolo arco)
void makeLittleArch(const TRasterCM32P &ras, const Disk &disk1,
                    const Disk &disk2, const Disk &disk3, int styleId,
                    bool doAntialias) {
  TPointD center1 = disk1.getCentre();
  TPointD center2 = disk2.getCentre();
  TPointD center3 = disk3.getCentre();
  TQuadratic quadratic(center1, center2, center3);

  disk1.draw(ras, styleId);

  double length = quadratic.getLength();
  if (length < 2) return;
  double t = 0, step = 1 / (length * 1.5), t2 = quadratic.getT(center2);
  bool idLastDiskDrown = true;
  for (t = step; t < 1; t += step) {
    TPointD center = quadratic.getPoint(t);
    double radius =
        disk1.getRadius() + (disk3.getRadius() - disk1.getRadius()) * t;
    Disk disk(TThickPoint(center, radius * 2), doAntialias);
    bool drawDisk = true;
    if (!doAntialias)
      drawDisk = isDiskNecessaryNecessary(
          quadratic, t, t - step, t + step > 1 ? 1 : t + step, idLastDiskDrown);
    if (t != 1 && drawDisk) {
      disk.draw(ras, styleId);
      idLastDiskDrown = true;
    }
  }
  disk3.draw(ras, styleId);
}

// Disegna un piccolo segmento, invece di un archetto.
void makeLittleSegment(const TRasterCM32P &ras, const Disk &disk1,
                       const Disk &disk2, int styleId, bool doAntialias) {
  TPointD center1 = disk1.getCentre();
  TPointD center2 = disk2.getCentre();
  TPointD middle  = (center1 + center2) * 0.5;
  double raius    = (disk1.getRadius() + disk2.getRadius()) * 0.5;
  Disk disk(TThickPoint(middle, raius * 2), doAntialias);
  makeLittleArch(ras, disk1, disk, disk2, styleId, doAntialias);
}

//=============================================================================

}  // namespace

//=============================================================================

// Preso un vettore di punti, disegna una pennelata che li approssima in un
// raster trasparente
void rasterBrush(const TRasterCM32P &rasBuffer,
                 const vector<TThickPoint> &points, int styleId,
                 bool doAntialias) {
  int i, n = points.size();
  if (n == 0)
    return;
  else if (n == 1) {
    Disk disk(points[0], doAntialias);
    disk.draw(rasBuffer, styleId);
    return;
  } else if (n == 2) {
    makeLittleSegment(rasBuffer, Disk(points[0], doAntialias),
                      Disk(points[1], doAntialias), styleId, doAntialias);
    return;
  } else if (n == 4) {
    makeLittleArch(rasBuffer, Disk(points[0], doAntialias),
                   Disk(points[1], doAntialias), Disk(points[2], doAntialias),
                   styleId, doAntialias);
    makeLittleSegment(rasBuffer, Disk(points[2], doAntialias),
                      Disk(points[3], doAntialias), styleId, doAntialias);
    return;
  } else {
    for (i = 0; i + 2 < n; i += 2)
      makeLittleArch(rasBuffer, Disk(points[i], doAntialias),
                     Disk(points[i + 1], doAntialias),
                     Disk(points[i + 2], doAntialias), styleId, doAntialias);
  }
}
