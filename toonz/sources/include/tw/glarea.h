#pragma once

#ifndef GLAREA_INCLUDED
#define GLAREA_INCLUDED

//#include "traster.h"
#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TStopWatch;

class DVAPI TGLArea : public TWidget {
  void swapBuffers();
  TStopWatch *m_stopwatch;
  double m_framePerSecond;

  void computeOcclusingRegion();
  void deleteOcclusingRegion();
  // void  redrawUncoveredArea(const TPoint &);

  std::vector<TRect> *m_arrayOfRectToRedraw;

protected:
// int m_lx, m_ly;

// double m_angle;

#define PEZZA_DISABLE_REDRAW

#if defined(MACOSX) && defined(PEZZA_DISABLE_REDRAW)
  bool m_disableRedraw;
#endif

  TPoint m_lastPos;
  TPointD m_pan;  // world origin (win coords) (n. m_pan*2 are integer)
  TPointD
      m_zoomFactor;  // "world pixel" dimension (win coords) (integer if >=1)
  TDimensionD m_pixelSize;  // 1/zoomFactor; win pixel dimension (world coords)

  double m_zoomLevel;  // m_zoomFactor = e^m_zoomLevel

  // supported extensions
  static bool m_bgraSupported;

  bool m_isInsideScreen;
  void onGLContextCreated();

public:
  class PickItem {
  public:
    UINT m_label;
    UINT m_zMin;
    UINT m_zMax;

    PickItem(UINT label, UINT zmin, UINT zmax)
        : m_label(label), m_zMin(zmin), m_zMax(zmax) {}

    ~PickItem() {}

  private:
    // not implemented
    PickItem();
  };

  TGLArea(TWidget *parent, string name = "GLArea");
  ~TGLArea();

  void repaint();
  void configureNotify(const TDimension &);
  void create();

  void paintIncrementally();

  //  void rectwrite(const TRaster32P &ras, const TPoint &p);
  void over(const TRaster32P &ras, const TPoint &p);
  void over(const TRasterGR8P &ras, const TPoint &p);

  inline TPointD win2world(const TPointD &p) const;
  inline TPointD world2win(const TPointD &p) const;

  inline TAffine getWin2WorldMatrix() const;
  inline TAffine getWorld2WinMatrix() const;

  void scroll(const TPoint &d);  // viewport coordinates
  void lookAt(
      const TPointD &d);  // world coordinates of the desired window center

  // n. zoomCenter : viewport coords
  void setZoomLevel(double zlevel, const TPoint &zoomCenter);
  void changeZoomLevel(double d, const TPoint &center);
  virtual void onZoomChange() {}

  void fitToWindow(const TRectD &worldRect);

  void middleButtonDown(const TMouseEvent &e);
  void middleButtonDrag(const TMouseEvent &e);
  void middleButtonUp(const TMouseEvent &e);

  void draw();
  virtual void drawIncrementally();

  virtual void drawForPick() { draw(); }

  void pick(const TPoint &point, vector<PickItem> &items);

  double getFramePerSecond() const { return m_framePerSecond; }

  static bool isBGRASupported() { return m_bgraSupported; }

  bool unProject(TPointD &output,

                 const TPoint &mousePos, const T3DPointD &planeOrigin,
                 const T3DPointD &u, const T3DPointD &v) const;

  void makeCurrent();  // Gl context
  bool m_dontSwapBuffers;
};

inline TPointD TGLArea::win2world(const TPointD &p) const {
  TPointD q = p - m_pan;
  return TPointD(m_pixelSize.lx * q.x, m_pixelSize.ly * q.y);
}

inline TPointD TGLArea::world2win(const TPointD &p) const {
  return m_pan + TPointD(p.x * m_zoomFactor.x, p.y * m_zoomFactor.y);
}

inline TAffine TGLArea::getWin2WorldMatrix() const {
  return TScale(m_pixelSize.lx, m_pixelSize.ly) * TTranslation(-m_pan);
}

inline TAffine TGLArea::getWorld2WinMatrix() const {
  return TTranslation(m_pan) * TScale(m_zoomFactor.x, m_zoomFactor.y);
}

#endif
