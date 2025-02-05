#include <memory>
#include <cstring>
#include <algorithm>
#include "trop.h"

/*
 * Optimized Brush: 2025-02-05 (Revised)
 * ------------------------------------
 * This version incorporates feedback to improve reliability and performance:
 * - Canonical Bresenham line interpolation for pixel-perfect strokes
 * - Immediate point processing to avoid excessive memory usage
 * - Optimized circle drawing with proper symmetry handling
 * - Cached row pointers for faster pixel access
 * - Simplified and robust pixel filling logic
 */

//=======================================================================

class HalfCord {
  std::unique_ptr<int[]> m_array;
  int m_radius;

public:
  HalfCord(int radius) : m_radius(radius), m_array(new int[radius + 1]) {
    assert(radius >= 0);
    memset(m_array.get(), 0, (m_radius + 1) * sizeof(int));

    // Bresenham's circle algorithm with full symmetry
    int x = 0;
    int y = m_radius;
    int d = 3 - (2 * m_radius);  // Decision variable

    while (y >= x) {
      // Store values for all 8 octants
      m_array[x] = y;
      m_array[y] = x;

      if (d <= 0) {
        d += (4 * x) + 6;
      } else {
        d += 4 * (x - y) + 10;
        y--;
      }
      x++;
    }

    // Fill remaining values using symmetry
    for (int i = 0; i <= m_radius; i++) {
      if (m_array[i] == 0 && i > 0) {
        m_array[i] = m_array[i - 1];
      }
    }
  }

  inline int getCord(int x) const {
    assert(0 <= x && x <= m_radius);
    return m_array[x];
  }

private:
  HalfCord(const HalfCord&)            = delete;
  HalfCord& operator=(const HalfCord&) = delete;
};

//=======================================================================

// Canonical Bresenham line interpolator
struct LineInterpolator {
  int x, y;
  int dx, dy;
  int error;
  int stepX, stepY;
  int x2, y2;

  LineInterpolator(int x1, int y1, int x2, int y2)
      : x(x1), y(y1), x2(x2), y2(y2) {
    dx    = std::abs(x2 - x1);
    dy    = std::abs(y2 - y1);
    stepX = (x1 < x2) ? 1 : -1;
    stepY = (y1 < y2) ? 1 : -1;
    error = (dx > dy ? dx : -dy) / 2;
  }

  bool step() {
    if (x == x2 && y == y2) return false;  // Termination

    int error2 = error;

    if (error2 > -dx) {
      error -= dy;
      x += stepX;
    }
    if (error2 < dy) {
      error += dx;
      y += stepY;
    }

    return true;
  }
};

//=======================================================================

void TRop::brush(TRaster32P ras, const TPoint& aa, const TPoint& bb, int radius,
                 const TPixel32& col) {
  if (!ras || radius < 0) return;

  const int lx = ras->getLx();
  const int ly = ras->getLy();

  // Lock raster once at the beginning
  ras->lock();

  // Early exit for zero radius case with optimized point drawing
  if (radius == 0) {
    LineInterpolator line(aa.x, aa.y, bb.x, bb.y);
    do {
      if (line.x >= 0 && line.x < lx && line.y >= 0 && line.y < ly) {
        ras->pixels(line.y)[line.x] = col;
      }
    } while (line.step());
    ras->unlock();
    return;
  }

  // Create HalfCord instance once
  HalfCord halfCord(radius);

  // Handle special cases efficiently
  if (aa == bb) {
    // Optimized circle drawing
    const int yMin = std::max(aa.y - radius, 0);
    const int yMax = std::min(aa.y + radius, ly - 1);

    for (int y = yMin; y <= yMax; y++) {
      const int delta = std::abs(y - aa.y);
      const int width = halfCord.getCord(delta);
      const int xMin  = std::max(aa.x - width, 0);
      const int xMax  = std::min(aa.x + width, lx - 1);

      TPixel32* row = ras->pixels(y) + xMin;
      std::fill_n(row, xMax - xMin + 1, col);
    }
    ras->unlock();
    return;
  }

  // Implement stroke interpolation for fast drawing
  LineInterpolator interpolator(aa.x, aa.y, bb.x, bb.y);

  do {
    const int yMin = std::max(interpolator.y - radius, 0);
    const int yMax = std::min(interpolator.y + radius, ly - 1);

    for (int y = yMin; y <= yMax; y++) {
      const int delta = std::abs(y - interpolator.y);
      const int width = halfCord.getCord(delta);
      const int xMin  = std::max(interpolator.x - width, 0);
      const int xMax  = std::min(interpolator.x + width, lx - 1);

      TPixel32* row = ras->pixels(y) + xMin;
      std::fill_n(row, xMax - xMin + 1, col);
    }
  } while (interpolator.step());

  ras->unlock();
}
