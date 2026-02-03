

#include "trop.h"
#include "tpixelutils.h"

/* NOTE: Scale operations can be performed using Look-Up-Tables.
         This is convenient for 8-bit channels, but perhaps not for 16-bit ones:

            In the 32-bit case, we perform max 256 channel operations,
            equal to performing plain scale on a 16 x 16 image.

            In the 64-bit case, the cost is 65536 channel operations,
            equal to performing plain scale on a 256 x 256 image.

         The raster size is used to discriminate LUT usage in the latter case
*/

//-----------------------------------------------------------------------------

namespace {

template <typename Chan>
void buildLUT(Chan *lut, double a, double k, int chanLow, int chanHigh) {
  int i, max = (std::numeric_limits<Chan>::max)();

  a += 0.5;  // round rather than trunc
  for (i = 0; i <= max; ++i)
    lut[i] = tcrop((int)(a + i * k), chanLow, chanHigh);
}

//-----------------------------------------------------------------------------

template <typename T>
void do_greyScale_lut(TRasterPT<T> rout, TRasterPT<T> rin, double a, double k,
                      int out0, int out1) {
  assert(rout->getSize() == rin->getSize());

  typedef typename T::Channel Channel;
  int chanValuesCount = T::maxChannelValue + 1;

  int fac = chanValuesCount / 256;
  out0    = std::max(fac * out0, 0),
  out1    = std::min(fac * out1, T::maxChannelValue);

  // Build lut
  Channel *lut = new Channel[chanValuesCount];
  buildLUT(lut, a, k, out0, out1);

  // Perform scale
  T *in, *end, *out;

  int y, lx = rin->getLx(), ly = rin->getLy();
  for (y = 0; y < ly; ++y) {
    in = rin->pixels(y), end = in + lx, out = rout->pixels(y);
    for (; in < end; ++in, ++out) out->value = lut[in->value];
  }

  delete[] lut;
}

//-----------------------------------------------------------------------------

template <typename T>
void do_greyAdjust(TRasterPT<T> rout, TRasterPT<T> rin, const int in0,
                   const int in1, const int out0, const int out1) {
  typedef typename T::Channel Channel;

  assert(rout->getSize() == rin->getSize());

  // Build scale parameters
  double k = (out1 - out0) / (double)(in1 - in0);
  double a = out0 - k * in0;

  do_greyScale_lut(rout, rin, a, k, out0, out1);
}

//-----------------------------------------------------------------------------

template <typename T>
void do_rgbmScale_lut(TRasterPT<T> rout, TRasterPT<T> rin, const double *a,
                      const double *k, const int *out0, const int *out1) {
  assert(rout->getSize() == rin->getSize());

  typedef typename T::Channel Channel;
  int m, max = T::maxChannelValue, chanValuesCount = max + 1;

  int fac   = chanValuesCount / 256;
  int out0R = std::max(fac * out0[0], 0),
      out1R = std::min(fac * out1[0], T::maxChannelValue);
  int out0G = std::max(fac * out0[1], 0),
      out1G = std::min(fac * out1[1], T::maxChannelValue);
  int out0B = std::max(fac * out0[2], 0),
      out1B = std::min(fac * out1[2], T::maxChannelValue);
  int out0M = std::max(fac * out0[3], 0),
      out1M = std::min(fac * out1[3], T::maxChannelValue);

  double aFac[4];
  for (int i = 0; i < 4; i++) aFac[i] = a[i] * (double)fac;

  // Build luts
  Channel *lut_r = new Channel[chanValuesCount];
  buildLUT(lut_r, aFac[0], k[0], out0R, out1R);

  Channel *lut_g = new Channel[chanValuesCount];
  buildLUT(lut_g, aFac[1], k[1], out0G, out1G);

  Channel *lut_b = new Channel[chanValuesCount];
  buildLUT(lut_b, aFac[2], k[2], out0B, out1B);

  Channel *lut_m = new Channel[chanValuesCount];
  buildLUT(lut_m, aFac[3], k[3], out0M, out1M);

  // Retrieve de/premultiplication luts
  const double *lut_prem   = premultiplyTable<Channel>();
  const double *lut_deprem = depremultiplyTable<Channel>();
  double premFac, depremFac;

  // Process raster
  int y, lx = rin->getLx(), ly = rin->getLy();
  T *in, *end, *out;

  for (y = 0; y < ly; ++y) {
    in = rin->pixels(y), end = in + lx, out = rout->pixels(y);
    for (; in < end; ++in, ++out) {
      m         = lut_m[in->m];
      depremFac = lut_deprem[in->m];
      premFac   = lut_prem[m];

      out->r = premFac * lut_r[std::min((int)(in->r * depremFac), max)];
      out->g = premFac * lut_g[std::min((int)(in->g * depremFac), max)];
      out->b = premFac * lut_b[std::min((int)(in->b * depremFac), max)];
      out->m = m;
    }
  }

  delete[] lut_r;
  delete[] lut_g;
  delete[] lut_b;
  delete[] lut_m;
}

//-----------------------------------------------------------------------------

template <typename T>
void do_rgbmScale(TRasterPT<T> rout, TRasterPT<T> rin, const double *a,
                  const double *k, const int *out0, const int *out1) {
  assert(rout->getSize() == rin->getSize());

  typedef typename T::Channel Channel;
  int m, chanValuesCount = T::maxChannelValue + 1;

  int fac = chanValuesCount / 256;

  int out0R = std::max(fac * out0[0], 0),
      out1R = std::min(fac * out1[0], T::maxChannelValue);
  int out0G = std::max(fac * out0[1], 0),
      out1G = std::min(fac * out1[1], T::maxChannelValue);
  int out0B = std::max(fac * out0[2], 0),
      out1B = std::min(fac * out1[2], T::maxChannelValue);
  int out0M = std::max(fac * out0[3], 0),
      out1M = std::min(fac * out1[3], T::maxChannelValue);

  // Retrieve de/premultiplication luts
  const double *lut_prem   = premultiplyTable<Channel>();
  const double *lut_deprem = depremultiplyTable<Channel>();
  double premFac, depremFac;

  double aFac[4];
  for (int i = 0; i < 4; i++) aFac[i] = a[i] * (double)fac;

  // Process raster
  int y, lx = rin->getLx(), ly = rin->getLy();
  T *in, *end, *out;

  for (y = 0; y < ly; ++y) {
    in = rin->pixels(y), end = in + lx, out = rout->pixels(y);
    for (; in < end; ++in, ++out) {
      m         = tcrop((int)(aFac[3] + k[3] * in->m), out0M, out1M);
      depremFac = lut_deprem[in->m];
      premFac   = lut_prem[m];

      out->r = premFac *
               tcrop((int)(aFac[0] + k[0] * in->r * depremFac), out0R, out1R);
      out->g = premFac *
               tcrop((int)(aFac[1] + k[1] * in->g * depremFac), out0G, out1G);
      out->b = premFac *
               tcrop((int)(aFac[2] + k[2] * in->b * depremFac), out0B, out1B);
      out->m = m;
    }
  }
}

//-----------------------------------------------------------------------------

template <>
void do_rgbmScale<TPixelF>(TRasterFP rout, TRasterFP rin, const double *a,
                           const double *k, const int *out0, const int *out1) {
  assert(rout->getSize() == rin->getSize());

  float fac = 1.f / 255.f;

  float out0R = std::max(fac * (float)out0[0], 0.f);
  float out1R = std::min(fac * (float)out1[0], 1.f);
  float out0G = std::max(fac * (float)out0[1], 0.f);
  float out1G = std::min(fac * (float)out1[1], 1.f);
  float out0B = std::max(fac * (float)out0[2], 0.f);
  float out1B = std::min(fac * (float)out1[2], 1.f);
  float out0M = std::max(fac * (float)out0[3], 0.f);
  float out1M = std::min(fac * (float)out1[3], 1.f);

  float aFac[4];
  for (int i = 0; i < 4; i++) aFac[i] = a[i] * (float)fac;

  // Process raster
  int y, lx = rin->getLx(), ly = rin->getLy();
  TPixelF *in, *end, *out;
  float m;

  for (y = 0; y < ly; ++y) {
    in = rin->pixels(y), end = in + lx, out = rout->pixels(y);
    for (; in < end; ++in, ++out) {
      m = tcrop(aFac[3] + (float)k[3] * in->m, out0M, out1M);

      if (in->m <= 0.f) {
        out->r = m * tcrop(aFac[0], out0R, out1R);
        out->g = m * tcrop(aFac[1], out0G, out1G);
        out->b = m * tcrop(aFac[2], out0B, out1B);
        out->m = m;
      } else {
        out->r = m * tcrop(aFac[0] + (float)k[0] * in->r / in->m, out0R, out1R);
        out->g = m * tcrop(aFac[1] + (float)k[1] * in->g / in->m, out0G, out1G);
        out->b = m * tcrop(aFac[2] + (float)k[2] * in->b / in->m, out0B, out1B);
        out->m = m;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void do_rgbmScaleFloat(TRasterFP rout, TRasterFP rin, const double *a,
                       const double *k, const int *out0, const int *out1) {
  assert(rout->getSize() == rin->getSize());
  float fac   = 1.f / 255.f;
  float out0R = std::max(fac * (float)out0[0], 0.f);
  float out1R = fac * (float)out1[0];
  float out0G = std::max(fac * (float)out0[1], 0.f);
  float out1G = fac * (float)out1[1];
  float out0B = std::max(fac * (float)out0[2], 0.f);
  float out1B = fac * (float)out1[2];
  float out0M = std::max(fac * (float)out0[3], 0.f);
  float out1M = fac * (float)out1[3];

  float aFac[4];
  for (int i = 0; i < 4; i++) aFac[i] = a[i] * fac;

  // Process raster
  for (int y = 0; y < rin->getLy(); ++y) {
    TPixelF *in = rin->pixels(y), *out = rout->pixels(y);
    TPixelF *end = in + rin->getLx();
    for (; in < end; ++in, ++out) {
      out->m = tcrop(aFac[3] + (float)k[3] * in->m, out0M, out1M);
      if (out->m == 0.f) {
        out->r = 0.f;
        out->g = 0.f;
        out->b = 0.f;
      } else if (in->m == 0.f) {
        out->r = out->m * tcrop(aFac[0], out0R, out1R);
        out->g = out->m * tcrop(aFac[1], out0G, out1G);
        out->b = out->m * tcrop(aFac[2], out0B, out1B);
      } else {
        out->r =
            out->m * tcrop(aFac[0] + (float)k[0] * in->r / in->m, out0R, out1R);
        out->g =
            out->m * tcrop(aFac[1] + (float)k[1] * in->g / in->m, out0G, out1G);
        out->b =
            out->m * tcrop(aFac[2] + (float)k[2] * in->b / in->m, out0B, out1B);
      }
    }
  }
}

//-----------------------------------------------------------------------------

template <typename T, typename ScaleFunc>
void do_rgbmAdjust(TRasterPT<T> rout, TRasterPT<T> rin, ScaleFunc scaleFunc,
                   const int *in0, const int *in1, const int *out0,
                   const int *out1, bool doClamp = true) {
  assert(rout->getSize() == rin->getSize());

  double a[5], k[5];

  // Build scale parameters
  int i;
  for (i = 0; i < 5; ++i) {
    k[i] = (out1[i] - out0[i]) / (double)(in1[i] - in0[i]);
    a[i] = out0[i] - k[i] * in0[i];
  }
  for (i = 1; i < 4; ++i) {
    a[i] += k[i] * a[0];
    k[i] *= k[0];
  }

  // Ensure that the output is cropped according to output params
  int out0i[4], out1i[4];
  if (doClamp) {
    out0i[0] = std::max(out0[0], tcrop((int)(a[0] + k[0] * out0[1]), 0, 255));
    out1i[0] = std::min(out1[0], tcrop((int)(a[0] + k[0] * out1[1]), 0, 255));

    out0i[1] = std::max(out0[0], tcrop((int)(a[0] + k[0] * out0[2]), 0, 255));
    out1i[1] = std::min(out1[0], tcrop((int)(a[0] + k[0] * out1[2]), 0, 255));

    out0i[2] = std::max(out0[0], tcrop((int)(a[0] + k[0] * out0[3]), 0, 255));
    out1i[2] = std::min(out1[0], tcrop((int)(a[0] + k[0] * out1[3]), 0, 255));
  } else {
    out0i[0] = std::max(out0[0], (int)(a[0] + k[0] * out0[1]));
    out1i[0] = std::min(out1[0], (int)(a[0] + k[0] * out1[1]));

    out0i[1] = std::max(out0[0], (int)(a[0] + k[0] * out0[2]));
    out1i[1] = std::min(out1[0], (int)(a[0] + k[0] * out1[2]));

    out0i[2] = std::max(out0[0], (int)(a[0] + k[0] * out0[3]));
    out1i[2] = std::min(out1[0], (int)(a[0] + k[0] * out1[3]));
  }
  out0i[3] = out0[4];
  out1i[3] = out1[4];

  scaleFunc(rout, rin, &a[1], &k[1], out0i, out1i);
}

}  // namespace

//-----------------------------------------------------------------------------

void TRop::rgbmScale(TRasterP rout, TRasterP rin, const double *k,
                     const double *a, const int *out0, const int *out1) {
  if (rout->getSize() != rin->getSize()) throw TRopException("size mismatch");

  rout->lock();
  rin->lock();

  if ((TRaster32P)rout && (TRaster32P)rin)
    do_rgbmScale_lut<TPixel32>(rout, rin, a, k, out0, out1);
  else if ((TRaster64P)rout && (TRaster64P)rin) {
    if (rin->getLx() * rin->getLy() < TPixel64::maxChannelValue)
      do_rgbmScale<TPixel64>(rout, rin, a, k, out0, out1);
    else
      do_rgbmScale_lut<TPixel64>(rout, rin, a, k, out0, out1);
  } else if ((TRasterGR8P)rout && (TRasterGR8P)rin)
    do_greyScale_lut<TPixelGR8>(rout, rin, a[0], k[0], out0[0], out1[0]);
  else if ((TRasterGR16P)rout && (TRasterGR16P)rin)
    do_greyScale_lut<TPixelGR16>(rout, rin, a[0], k[0], out0[0], out1[0]);
  else if ((TRasterFP)rout && (TRasterFP)rin)
    do_rgbmScale<TPixelF>(rout, rin, a, k, out0, out1);
  else {
    rout->unlock();
    rin->unlock();
    throw TRopException("pixel type mismatch");
  }

  rout->unlock();
  rin->unlock();
}

//-----------------------------------------------------------------------------

void TRop::rgbmScale(TRasterP rout, TRasterP rin, double kr, double kg,
                     double kb, double km, double ar, double ag, double ab,
                     double am) {
  double k[4], a[4];

  a[0] = ar, a[1] = ag, a[2] = ab, a[3] = am;
  k[0] = kr, k[1] = kg, k[2] = kb, k[3] = km;

  int out0[4], out1[4];

  out0[0] = out0[1] = out0[2] = out0[3] = 0;
  out1[0] = out1[1] = out1[2] = out1[3] = 255;

  rgbmScale(rout, rin, k, a, out0, out1);
}

//-----------------------------------------------------------------------------

void TRop::rgbmAdjust(TRasterP rout, TRasterP rin, const int *in0,
                      const int *in1, const int *out0, const int *out1) {
  if (rout->getSize() != rin->getSize()) throw TRopException("size mismatch");

  rout->lock();
  rin->lock();

  if ((TRaster32P)rout && (TRaster32P)rin)
    do_rgbmAdjust<TPixel32>(rout, rin, &do_rgbmScale_lut<TPixel32>, in0, in1,
                            out0, out1);
  else if ((TRaster64P)rout && (TRaster64P)rin) {
    if (rin->getLx() * rin->getLy() < TPixel64::maxChannelValue)
      do_rgbmAdjust<TPixel64>(rout, rin, &do_rgbmScale<TPixel64>, in0, in1,
                              out0, out1);
    else
      do_rgbmAdjust<TPixel64>(rout, rin, &do_rgbmScale_lut<TPixel64>, in0, in1,
                              out0, out1);
  } else if ((TRasterFP)rout && (TRasterFP)rin)
    do_rgbmAdjust<TPixelF>(rout, rin, &do_rgbmScaleFloat, in0, in1, out0, out1,
                           false);
  else if ((TRasterGR8P)rout && (TRasterGR8P)rin)
    do_greyAdjust<TPixelGR8>(rout, rin, in0[0], in1[0], out0[0], out1[0]);
  else if ((TRasterGR16P)rout && (TRasterGR16P)rin)
    do_greyAdjust<TPixelGR16>(rout, rin, in0[0], in1[0], out0[0], out1[0]);
  else {
    rout->unlock();
    rin->unlock();
    throw TRopException("pixel type mismatch");
  }

  rout->unlock();
  rin->unlock();
}
