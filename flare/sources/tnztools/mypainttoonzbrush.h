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
    union {
      struct {
        double x, y, pressure, tx, ty, time;
      };
      struct {
        double values[6];
      };
    };

    inline explicit Params(double x = 0.0, double y = 0.0,
                           double pressure = 0.0, double tx = 0.0,
                           double ty = 0.0, double time = 0.0)
        : x(x), y(y), pressure(pressure), tx(tx), ty(ty), time(time) {}

    inline void setMedian(Params &a, Params &b) {
      for (int i = 0; i < (int)sizeof(values) / sizeof(values[0]); ++i)
        values[i] = 0.5 * (a.values[i] + b.values[i]);
    }
  };

  struct Segment {
    Params p1, p2;
  };

  TRaster32P m_ras;
  Raster32PMyPaintSurface m_mypaintSurface;
  mypaint::Brush m_brush;

  bool m_reset;
  Params m_previous, m_current;
  bool m_interpolation;

public:
  MyPaintToonzBrush(const TRaster32P &ras, RasterController &controller,
                    const mypaint::Brush &brush, bool interpolation = false);
  void beginStroke();
  void strokeTo(const TPointD &position, double pressure, const TPointD &tilt,
                double dtime);
  void endStroke();

  // colormapped
  void updateDrawing(const TRasterCM32P rasCM, const TRasterCM32P rasBackupCM,
                     const TRect &bbox, int styleId, bool lockAlpha) const;

  const TRaster32P &getRaster() const { return m_ras; }
  RasterController &getController() {
    return *m_mypaintSurface.getController();
  }
  const mypaint::Brush &getBrush() const { return m_brush; }
};

#endif  // T_BLUREDBRUSH
