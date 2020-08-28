#pragma once

#ifndef TOOLVIEWER_INCLUDED
#define TOOLVIEWER_INCLUDED

// TnzTools includes
#include "tools/cursors.h"

// TnzLib includes
#include "toonz/imagepainter.h"

// TnzCore includes
#include "tcommon.h"
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//  Forward declarations

class TInputManager;

//===================================================================

//*****************************************************************************************
//    TToolViewer declaration
//*****************************************************************************************

/*!
  \brief    The TToolViewer class is the abstract base class that provides an
  interface for
            TTool viewer widgets (it is required that such widgets support
  OpenGL).
*/

class TToolViewer {
protected:
  ImagePainter::VisualSettings
      m_visualSettings;  //!< Settings used by the Viewer to draw scene contents

  int guidedStrokePickMode = 0;
  int m_guidedFrontStroke  = -1;
  int m_guidedBackStroke   = -1;

public:
  TToolViewer() {}
  virtual ~TToolViewer() {}

  virtual TInputManager *getInputManager() const = 0;

  const ImagePainter::VisualSettings &visualSettings() const {
    return m_visualSettings;
  }
  ImagePainter::VisualSettings &visualSettings() { return m_visualSettings; }

  virtual double getPixelSize()
      const = 0;  //!< Returns the length of a pixel in current OpenGL
                  //!< coordinates

  virtual void invalidateAll() = 0;    //!< Redraws the entire viewer, passing
                                       //! through Qt's event system
  virtual void GLInvalidateAll() = 0;  //!< Redraws the entire viewer, bypassing
                                       //! Qt's event system
  virtual void GLInvalidateRect(const TRectD &rect) = 0;  //!< Same as
                                                          //! GLInvalidateAll(),
  //! for a specific
  //! clipping rect
  virtual void invalidateToolStatus() = 0;  //!< Forces the viewer to update the
                                            //! perceived status of tools
  virtual TAffine getViewMatrix() const {
    return TAffine();
  }  //!< Gets the viewer's current view affine (ie the transform from
     //!<  starting to current <I> world view <\I>)

  virtual TAffine4 get3dViewMatrix() const {
    return TAffine4(getViewMatrix());
  }  //!< Gets the viewer's current view affine 3d (ie the transform from
     //!<  starting to current <I> world view <\I>)

  //! Gets the viewer's window size to calculate win coordinates from pos
  virtual TDimensionI getWindowSize() const = 0;

  //! return the column index of the drawing intersecting point \b p
  //! (window coordinate, pixels, bottom-left origin)
  virtual int posToColumnIndex(const TPointD &p, double distance,
                               bool includeInvisible = true) const = 0;
  virtual void posToColumnIndexes(const TPointD &p, std::vector<int> &indexes,
                                  double distance,
                                  bool includeInvisible = true) const = 0;

  //! return the row of the drawing intersecting point \b p (used with
  //! onionskins)
  //! (window coordinate, pixels, bottom-left origin)
  virtual int posToRow(const TPointD &p, double distance,
                       bool includeInvisible  = true,
                       bool currentColumnOnly = false) const = 0;

  //! return pos in pixel, bottom-left origin
  virtual TPointD worldToPos(const TPointD &worldPos) const = 0;

  //! return the OpenGL nameId of the object intersecting point \b p
  //! (window coordinate, pixels, bottom-left origin)
  virtual int pick(const TPointD &point) = 0;

  // note: winPos in pixel, top-left origin;
  // when no camera movements have been defined then worldPos = 0 at camera
  // center
  virtual TPointD winToWorld(const TPointD &winPos) const = 0;

  // delta.x: right panning, pixels; delta.y: down panning, pixels
  virtual void pan(const TPointD &delta) = 0;

  // center: window coordinates, pixels, bottomleft origin
  virtual void zoom(const TPointD &center, double scaleFactor) = 0;

  virtual void rotate(const TPointD &center, double angle) = 0;
  virtual void rotate3D(double dPhi, double dTheta)        = 0;
  virtual bool is3DView() const      = 0;
  virtual bool getIsFlippedX() const = 0;
  virtual bool getIsFlippedY() const = 0;

  virtual double projectToZ(const TPointD &delta) = 0;

  virtual TPointD getDpiScale() const = 0;
  virtual int getVGuideCount()        = 0;
  virtual int getHGuideCount()        = 0;
  virtual double getHGuide(int index) = 0;
  virtual double getVGuide(int index) = 0;

  virtual void
  resetInputMethod() = 0;  // Intended to call QWidget->resetInputContext()

  virtual void setFocus() = 0;

  /*-- Toolで画面の内外を判断するため --*/
  virtual TRectD getGeometry() const = 0;

  virtual void bindFBO() {}
  virtual void releaseFBO() {}

  int getGuidedStrokePickerMode() { return guidedStrokePickMode; }
  void setGuidedStrokePickerMode(int mode) { guidedStrokePickMode = mode; }

  int getGuidedStrokePickerCursor() {
    if (guidedStrokePickMode < 0)
      return ToolCursor::PickPrevCursor;
    else if (guidedStrokePickMode > 0)
      return ToolCursor::PickNextCursor;
    else
      return ToolCursor::PointingHandCursor;
  }

  int getGuidedFrontStroke() { return m_guidedFrontStroke; }
  void setGuidedFrontStroke(int strokeIdx) {
    m_guidedFrontStroke = strokeIdx;
    invalidateAll();
  }

  int getGuidedBackStroke() { return m_guidedBackStroke; }
  void setGuidedBackStroke(int strokeIdx) {
    m_guidedBackStroke = strokeIdx;
    invalidateAll();
  }

  void getGuidedFrameIdx(int *backIdx, int *frontIdx);
  void doPickGuideStroke(const TPointD &pos);
};

#endif
