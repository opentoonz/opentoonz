

//#include "trop.h"
#include "tpalette.h"
#include "tropcm.h"

//------------------------------------------------------------------------------

namespace {

inline double gauss(double x, double y, double x0, double y0, double s) {
  return exp(-((x - x0) * (x - x0) + (y - y0) * (y - y0)) / s) / (s * M_PI);
}

//------------------------------------------------------------------------------

double integ_gauss(double xmin, double ymin, double xmax, double ymax,
                   double x0, double y0, double s) {
  int i, j, n;
  double x1, y1, x2, y2, xstep, ystep, area;
  double sum;
  n     = 50;
  sum   = 0;
  xstep = (xmax - xmin) / n;
  ystep = (ymax - ymin) / n;
  area  = xstep * ystep;
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
      x1 = xmin + xstep * i;
      y1 = ymin + ystep * i;
      x2 = x1 + xstep;
      y2 = y1 + ystep;
      sum += area * (gauss(x1, y1, x0, y0, s) + gauss(x1, y2, x0, y0, s) +
                     gauss(x2, y2, x0, y0, s) + gauss(x2, y1, x0, y0, s)) /
             4;
    }
  return sum;
}

//------------------------------------------------------------------------------

void build_filter(double h[], double x0, double y0, double s) {
  double sum, vv;
  int i, j, k;
  double x1, y1, x2, y2;
  k   = 0;
  sum = 0.0;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++) {
      x1     = -1.5 + j;
      y1     = -1.5 + i;
      x2     = x1 + 1.0;
      y2     = y1 + 1.0;
      h[k++] = vv = integ_gauss(x1, y1, x2, y2, x0, y0, s);
      sum += vv;
    }
  if (sum > 0.0) {
    sum = 1.0 / sum;
    for (k = 0; k < 9; k++) h[k] *= sum;
  }
}

}  // anonymous namespace

//------------------------------------------------------------------------------

void TRop::fracmove(TRasterP rout, TRasterP rin, double dx, double dy) {
  // Using a bilinear filter is best - consider that only 4 pixels should
  // contribute
  // to a fractionarily shifted one, with weights proportional to the
  // intersection areas.
  double w[4] = {1, 0, 0, 0};
  double sum  = 0;

  int idx      = tfloor(dx);
  int idy      = tfloor(dy);
  double fracX = dx - idx;
  double fracY = dy - idy;

  int i, j;
  for (i = 0; i < 2; ++i)
    for (j                = 0; j < 2; ++j)
      sum += w[j + 2 * i] = fabs(fracX - j) * fabs(fracY - i);

  for (i = 0; i < 4; ++i) w[i] /= sum;

  TRop::convolve_i(rout, rin, idx, idy, w, 2);
}

//------------------------------------------------------------------------------

void TRop::fracmove(TRasterP rout, TRasterCM32P rin, const TPaletteP &palette,
                    double dx, double dy) {
  double w[4] = {1, 0, 0, 0};
  double sum  = 0;

  int idx      = tfloor(dx);
  int idy      = tfloor(dy);
  double fracX = dx - idx;
  double fracY = dy - idy;

  int i, j;
  for (i = 0; i < 2; ++i)
    for (j                = 0; j < 2; ++j)
      sum += w[j + 2 * i] = fabs(fracX - j) * fabs(fracY - i);

  for (i = 0; i < 4; ++i) w[i] /= sum;

  TRop::convolve_i(rout, rin, idx, idy, w, 2);
}
