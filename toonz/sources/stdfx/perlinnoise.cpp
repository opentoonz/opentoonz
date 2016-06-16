

#include "trandom.h"
//#include "tfxparam.h"
#include "perlinnoise.h"

// using std::cout;
// using std::endl;
//-------------------------------------------------------------------

double PerlinNoise::LinearNoise(double x, double y, double t) {
  int ix, iy, it, ix1, iy1, it1;
  double dx, dy, dt, val1, val2, val3, val4, val5, val6;

  ix = (int)x;
  iy = (int)y;
  it = (int)t;

  dx = x - ix;
  dy = y - iy;
  dt = t - it;

  ix = ix % Size;
  iy = iy % Size;
  it = it % TimeSize;

  ix1 = (ix + 1) % Size;
  iy1 = (iy + 1) % Size;
  it1 = (it + 1) % TimeSize;

  val1 = Noise[it + TimeSize * iy + TimeSize * Size * ix] +
         dx * (Noise[it + TimeSize * iy + TimeSize * Size * ix1] -
               Noise[it + TimeSize * iy + TimeSize * Size * ix]);
  val2 = Noise[it + TimeSize * iy1 + TimeSize * Size * ix] +
         dx * (Noise[it + TimeSize * iy1 + TimeSize * Size * ix1] -
               Noise[it + TimeSize * iy1 + TimeSize * Size * ix]);
  val3 = Noise[it1 + TimeSize * iy + TimeSize * Size * ix] +
         dx * (Noise[it1 + TimeSize * iy + TimeSize * Size * ix1] -
               Noise[it1 + TimeSize * iy + TimeSize * Size * ix]);
  val4 = Noise[it1 + TimeSize * iy1 + TimeSize * Size * ix] +
         dx * (Noise[it1 + TimeSize * iy1 + TimeSize * Size * ix1] -
               Noise[it1 + TimeSize * iy1 + TimeSize * Size * ix]);

  val5 = (val1 + dy * (val2 - val1));
  val6 = (val3 + dy * (val4 - val3));

  return (val5 + dt * (val6 - val5));
}

double PerlinNoise::Turbolence(double u, double v, double k, double grain) {
  u += Offset;
  v += Offset;
  Pixel_size = 0.05;
  double t = 0.0, scale = 1.0, tscale = 0;

  u /= grain;
  v /= grain;
  k /= 10;
  while (scale > Pixel_size) {
    tscale += scale;
    t += LinearNoise(u / scale, v / scale, k / scale) * scale;
    scale /= 2.0;
  }
  return t / tscale;
}

double PerlinNoise::Turbolence(double u, double v, double k, double grain,
                               double min, double max) {
  u += Offset;
  v += Offset;
  Pixel_size = 0.05;
  double t = 0.0, scale = 1.0, tscale = 0;

  u /= grain;
  v /= grain;
  k /= 10;
  while (scale > Pixel_size) {
    tscale += scale;
    t += LinearNoise(u / scale, v / scale, k / scale) * scale;
    scale /= 2.0;
  }
  t = t / tscale;
  if (t < min)
    t = 0;
  else {
    if (t > max)
      t = 1;
    else
      t = (t - min) / ((max - min));
  }
  return t;
}

double PerlinNoise::Marble(double u, double v, double k, double grain) {
  u += Offset;
  v += Offset;
  Pixel_size = 0.05;
  double t = 0.0, scale = 1.0, tscale = 0;

  u /= grain;
  v /= grain;
  k /= 10;
  while (scale > Pixel_size) {
    tscale += scale;
    t += LinearNoise(u / scale, v / scale, k / scale) * scale;
    scale /= 2.0;
  }
  t = 10 * t;
  return (t - (int)t);
}

double PerlinNoise::Marble(double u, double v, double k, double grain,
                           double min, double max) {
  u += Offset;
  v += Offset;
  Pixel_size = 0.05;
  double t = 0.0, scale = 1.0, tscale = 0;

  u /= grain;
  v /= grain;
  k /= 10;
  while (scale > Pixel_size) {
    tscale += scale;
    t += LinearNoise(u / scale, v / scale, k / scale) * scale;
    scale /= 2.0;
  }
  t = 10 * t;
  t = (t - (int)t);
  if (t < min)
    t = 0;
  else {
    if (t > max)
      t = 1;
    else
      t = (t - min) / ((max - min));
  }
  return t;
}

PerlinNoise::PerlinNoise() : Noise(new float[Size * Size * TimeSize]) {
  TRandom random(1);
  for (int i = 0; i < Size; i++) {
    for (int j = 0; j < Size; j++) {
      for (int k = 0; k < TimeSize; k++) {
        float tmp = random.getFloat();
        // cout << "tmp = " << tmp << "HM = "<< HowMany<< endl;
        Noise[k + TimeSize * j + TimeSize * Size * i] = tmp;
      }
    }
  }

  // cout << "HowMany = " << HowMany <<endl;
}

int PerlinNoise::Size     = 60;
int PerlinNoise::TimeSize = 20;
int PerlinNoise::Offset   = 1000000;
double PerlinNoise::Pixel_size =
    0.01;  // il pixel size va animato da 1 (escluso)
// a 0.1 (consigliato) fino ad un min di 0.001

namespace {
template <typename PIXEL>
void doCloudsT(const TRasterPT<PIXEL> &ras, const TSpectrumT<PIXEL> &spectrum,
               TPointD &tilepos, double evolution, double size, double min,
               double max, int type, double scale) {
  int j;
  TAffine aff = TScale(1 / scale);
  PerlinNoise Noise;
  ras->lock();
  if (type == PNOISE_CLOUDS) {
    for (j = 0; j < ras->getLy(); j++) {
      TPointD pos = tilepos;
      pos.y += j;
      PIXEL *pix    = ras->pixels(j);
      PIXEL *endPix = pix + ras->getLx();
      while (pix < endPix) {
        TPointD posAff = aff * pos;
        double pnoise =
            Noise.Turbolence(posAff.x, posAff.y, evolution, size, min, max);
        pos.x += 1.0;
        *pix++ = spectrum.getPremultipliedValue(pnoise);
      }
    }
  } else {
    for (j = 0; j < ras->getLy(); j++) {
      TPointD pos = tilepos;
      pos.y += j;
      PIXEL *pix    = ras->pixels(j);
      PIXEL *endPix = pix + ras->getLx();
      while (pix < endPix) {
        TPointD posAff = aff * pos;
        double pnoise =
            Noise.Marble(posAff.x, posAff.y, evolution, size, min, max);
        pos.x += 1.0;
        *pix++ = spectrum.getPremultipliedValue(pnoise);
      }
    }
  }
  ras->unlock();
}
}

void doClouds(const TRasterP &ras, const TSpectrumParamP colors, TPointD pos,
              double evolution, double size, double min, double max, int type,
              double scale, double frame) {
  if ((TRaster32P)ras)
    doCloudsT<TPixel32>(ras, colors->getValue(frame), pos, evolution, size, min,
                        max, type, scale);
  else if ((TRaster64P)ras)
    doCloudsT<TPixel64>(ras, colors->getValue64(frame), pos, evolution, size,
                        min, max, type, scale);
  else
    throw TException("CloudsFx: unsupported Pixel Type");
}
