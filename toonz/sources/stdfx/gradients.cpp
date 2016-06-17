

#include "trop.h"
#include "traster.h"

#include "tspectrumparam.h"
//------------------------------------------------------------------

namespace {
template <class T>
void doComputeRadialT(TRasterPT<T> ras, TPointD posTrasf,
                      const TSpectrumT<T> &spectrum, double period,
                      double count, double cycle, const TAffine &aff) {
  int j;
  double maxRadius = period * count;
  double freq      = 1.0 / period;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD posAux = posTrasf;
    T *pix         = ras->pixels(j);
    T *endPix      = pix + ras->getLx();
    while (pix < endPix) {
      double radius = norm(posAux);
      double t      = 1;
      if (radius < maxRadius) {
        t = (radius + cycle) * freq;
        t -= floor(t);
      }
      // double polinomfactor=(-2*t+3)*(t*t);
      *pix++ = spectrum.getPremultipliedValue(t);
      posAux.x += aff.a11;
      posAux.y += aff.a21;
    }
    posTrasf.x += aff.a12;
    posTrasf.y += aff.a22;
  }
  ras->unlock();
}
}

//------------------------------------------------------------------

void multiRadial(const TRasterP &ras, TPointD posTrasf,
                 const TSpectrumParamP colors, double period, double count,
                 double cycle, const TAffine &aff, double frame) {
  if ((TRaster32P)ras)
    doComputeRadialT<TPixel32>(ras, posTrasf, colors->getValue(frame), period,
                               count, cycle, aff);
  else if ((TRaster64P)ras)
    doComputeRadialT<TPixel64>(ras, posTrasf, colors->getValue64(frame), period,
                               count, cycle, aff);
  else
    throw TException("MultiRadialGradientFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

namespace {
template <class T>
void doComputeLinearT(TRasterPT<T> ras, TPointD posTrasf,
                      const TSpectrumT<T> &spectrum, double period,
                      double count, double w_amplitude, double w_freq,
                      double w_phase, double cycle, const TAffine &aff) {
  double shift     = 0;
  double maxRadius = period * count / 2.;
  double freq      = 1.0 / period;
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD posAux = posTrasf;

    // TPointD pos = tile.m_pos;
    // pos.y += j;
    T *pix    = ras->pixels(j);
    T *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (w_amplitude) shift = w_amplitude * sin(w_freq * posAux.y + w_phase);
      double radius          = posAux.x + shift;
      double t               = 1;
      if (fabs(radius) < maxRadius) {
        t = (radius + maxRadius + cycle) * freq;
        t -= floor(t);
      } else if (radius < 0)
        t                  = 0;
      double polinomfactor = (-2 * t + 3) * (t * t);
      // pos.x += 1.0;
      *pix++ = spectrum.getPremultipliedValue(polinomfactor);
      posAux.x += aff.a11;
      posAux.y += aff.a21;
    }
    posTrasf.x += aff.a12;
    posTrasf.y += aff.a22;
  }
  ras->unlock();
}
}
//------------------------------------------------------------------

void multiLinear(const TRasterP &ras, TPointD posTrasf,
                 const TSpectrumParamP colors, double period, double count,
                 double amplitude, double freq, double phase, double cycle,
                 const TAffine &aff, double frame) {
  if ((TRaster32P)ras)
    doComputeLinearT<TPixel32>(ras, posTrasf, colors->getValue(frame), period,
                               count, amplitude, freq, phase, cycle, aff);
  else if ((TRaster64P)ras)
    doComputeLinearT<TPixel64>(ras, posTrasf, colors->getValue64(frame), period,
                               count, amplitude, freq, phase, cycle, aff);
  else
    throw TException("MultiLinearGradientFx: unsupported Pixel Type");
}
