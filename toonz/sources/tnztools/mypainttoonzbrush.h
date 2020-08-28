#pragma once

#ifndef MYPAINTTOONZBRUSH_H
#define MYPAINTTOONZBRUSH_H

#include <toonz/mypaint.h>
#include "traster.h"
#include "trastercm.h"
#include "tcurves.h"
#include <QPainter>
#include <QImage>

class RasterController {
public:
  virtual ~RasterController() {}
  virtual bool askRead(const TRect &rect) { return true; }
  virtual bool askWrite(const TRect &rect) { return true; }
};

//=======================================================
//
// Raster32PMyPaintSurface
//
//=======================================================

class Raster32PMyPaintSurface : public mypaint::Surface {
private:
  class Internal;

  TRaster32P ras;
  RasterController *controller;
  Internal *internal;

  inline static void readPixel(const void *pixelPtr, float &colorR,
                               float &colorG, float &colorB, float &colorA) {
    const TPixel32 &pixel = *(const TPixel32 *)pixelPtr;
    colorR                = (float)pixel.r / (float)TPixel32::maxChannelValue;
    colorG                = (float)pixel.g / (float)TPixel32::maxChannelValue;
    colorB                = (float)pixel.b / (float)TPixel32::maxChannelValue;
    colorA                = (float)pixel.m / (float)TPixel32::maxChannelValue;
  }

  inline static void writePixel(void *pixelPtr, float colorR, float colorG,
                                float colorB, float colorA) {
    TPixel32 &pixel = *(TPixel32 *)pixelPtr;
    pixel.r = (TPixel32::Channel)roundf(colorR * TPixel32::maxChannelValue);
    pixel.g = (TPixel32::Channel)roundf(colorG * TPixel32::maxChannelValue);
    pixel.b = (TPixel32::Channel)roundf(colorB * TPixel32::maxChannelValue);
    pixel.m = (TPixel32::Channel)roundf(colorA * TPixel32::maxChannelValue);
  }

  inline static bool askRead(void *surfaceController,
                             const void * /* surfacePointer */, int x0, int y0,
                             int x1, int y1) {
    Raster32PMyPaintSurface &owner =
        *((Raster32PMyPaintSurface *)surfaceController);
    return !owner.controller ||
           owner.controller->askRead(TRect(x0, y0, x1, y1));
  }

  inline static bool askWrite(void *surfaceController,
                              const void * /* surfacePointer */, int x0, int y0,
                              int x1, int y1) {
    Raster32PMyPaintSurface &owner =
        *((Raster32PMyPaintSurface *)surfaceController);
    return !owner.controller ||
           owner.controller->askWrite(TRect(x0, y0, x1, y1));
  }

public:
  explicit Raster32PMyPaintSurface(const TRaster32P &ras);
  explicit Raster32PMyPaintSurface(const TRaster32P &ras,
                                   RasterController &controller);
  ~Raster32PMyPaintSurface();

  bool getColor(float x, float y, float radius, float &colorR, float &colorG,
                float &colorB, float &colorA) override;

  bool drawDab(const mypaint::Dab &dab) override;

  bool getAntialiasing() const;
  void setAntialiasing(bool value);

  RasterController *getController() const { return controller; }
};

//=======================================================
//
// MyPaintToonzBrush
//
//=======================================================

class MyPaintToonzBrush {
private:
  struct Params {
    enum { Count = 6 };
    union {
      struct { double x, y, pressure, tilt_x, tilt_y, time; };
      struct { double values[Count]; };
    };

    inline explicit Params(double x = 0.0, double y = 0.0,
                           double pressure = 0.0,
                           double tilt_x = 0.0, double tilt_y = 0.0,
                           double time = 0.0)
        : x(x), y(y), pressure(pressure), tilt_x(tilt_x), tilt_y(tilt_y), time(time) {}
        
    double& operator[](int i) { return values[i]; }
    const double& operator[](int i) const { return values[i]; }

    Params& operator+=(const Params &x)
      { for (int i = 0; i < Count; ++i) values[i] += x[i]; return *this; }
    Params& operator-=(const Params &x)
      { for (int i = 0; i < Count; ++i) values[i] -= x[i]; return *this; }
    Params& operator*=(double x)
      { for (int i = 0; i < Count; ++i) values[i] *= x; return *this; }
    Params& operator/=(double x)
      { return *this *= 1/x; }

    Params operator+(const Params &x) const
      { Params p(*this); return p += x; }
    Params operator-(const Params &x) const
      { Params p(*this); return p -= x; }
    Params operator*(double x) const
      { Params p(*this); return p *= x; }
    Params operator/(double x) const
      { Params p(*this); return p /= x; }
  };

  TRaster32P ras;
  Raster32PMyPaintSurface mypaintSurface;
  mypaint::Brush brush;
  bool interpolation;
  bool reset;
  
  Params prevprev, prev, current;

  void strokeTo(const Params &p, double prevtime);
  void strokeBezierSegment(const Params &p0, const Params &p1,
                           const Params &p2, const Params &p3, int level);
  
public:
  MyPaintToonzBrush(const TRaster32P &ras,
                    RasterController &controller,
                    const mypaint::Brush &brush,
                    bool interpolation = false);

  void beginStroke();
  void strokeTo(const TPointD &position, double pressure, const TPointD &tilt,
                double dtime);
  void endStroke();

  const TRaster32P &getRaster() const { return ras; }
  RasterController &getController() { return *mypaintSurface.getController(); }
  const mypaint::Brush &getBrush() const { return brush; }

  // colormapped
  void updateDrawing(const TRasterCM32P rasCM, const TRasterCM32P rasBackupCM,
                     const TRect &bbox, int styleId) const;
};

#endif  // T_BLUREDBRUSH
