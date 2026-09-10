#pragma once

#include "tools/tool.h"

class TXshSimpleLevel;

//! Interactive tool for Shift & Trace (onion-skin ghost alignment).
//! - Two reference ghosts (previous / following frame) with independent
//!   transforms; stage.cpp composites them using OnionSkinMask.
//! - Control UI: colored outer frames (red / green) for picking which ghost
//!   to edit; inner frame and handles for the active ghost only.
//! - Raster reference rectangle for handles follows GhostBBoxMode (toolbar);
//!   vector levels use the stroke bounding box only.
class DVAPI ShiftTraceTool final : public TTool {
public:
  //! How m_ghostBox[i] is derived for raster / toonz raster ghosts (vector
  //! ignores this and uses TVectorImage::getBBox()).
  enum class GhostBBoxMode { FullRaster = 0, Savebox = 1, ContentAlpha = 2 };

  static GhostBBoxMode getGhostBBoxMode();
  static void setGhostBBoxMode(GhostBBoxMode mode);

  void invalidateGhostContentBBoxCache();
  enum CurveStatus {
    NoCurve,
    TwoPointsCurve,  // just during the first click&drag
    ThreePointsCurve
  };

  enum GadgetId {
    NoGadget,
    NoGadget_InBox,
    CurveP0Gadget,
    CurveP1Gadget,
    CurvePmGadget,
    MoveCenterGadget,
    RotateGadget,
    TranslateGadget,
    ScaleGadget,
    GhostPickPrevGadget,  //!< Previous ghost; red frame (click to select).
    GhostPickNextGadget   //!< Following ghost; green frame (click to select).
  };
  inline bool isCurveGadget(GadgetId id) const {
    return CurveP0Gadget <= id && id <= CurvePmGadget;
  }
  inline bool isGhostPickGadget(GadgetId id) const {
    return id == GhostPickPrevGadget || id == GhostPickNextGadget;
  }

private:
  TPointD m_oldPos, m_startPos;
  int m_ghostIndex;
  TPointD m_p0, m_p1, m_p2;

  CurveStatus m_curveStatus;
  GadgetId m_gadget;
  GadgetId m_highlightedGadget;

  //! Local-space rectangle for handles, per ghost index (centered like before).
  TRectD m_ghostBox[2];
  //! Dpi affine for the ghost's level frame (can differ per index).
  TAffine m_dpiAffPerGhost[2];
  //! Xsheet row index (scene) or level frame index (level edit) per ghost.
  int m_row[2];
  TAffine m_aff[2];
  TPointD m_center[2];
  //! Pivot at reset / mode change: center of m_ghostBox in local space.
  TPointD m_defaultCenter[2];

  TAffine m_oldAff;

  //! Singleton used by setGhostBBoxMode (TEnv) to invalidate the live tool.
  static ShiftTraceTool *s_instance;

  //! Cache for ContentAlpha mode only; one slot per ghost to avoid full raster
  //! scans every redraw. Invalidate when mode changes or clearData().
  TXshSimpleLevel *m_contentBBoxCacheLevel[2];
  TFrameId m_contentBBoxCacheFid[2];
  int m_contentBBoxCacheGhostRow[2];
  TRect m_contentBBoxCachePixelRect[2];
  bool m_contentBBoxCacheValid[2];

  //! Builds m_ghostBox in image space for one raster; cacheSlot is 0 or 1.
  TRectD rasterRefBox(const TRasterP &ras, int subsampling, GhostBBoxMode mode,
                      const TRect &savebox, TXshSimpleLevel *sl,
                      const TFrameId &fid, int cacheSlot);

  //! Fills m_ghostBox[0..1] and m_dpiAffPerGhost from m_row and current edit
  //! context. Called from updateData() each draw.
  void updateAllGhostBoxes();
  //! Keeps m_center on m_defaultCenter when the ghost transform is still default.
  void syncDefaultGhostCenters();
  //! Padding in local units for outer pick frame; matches enlarge(3 * pad) in
  //! draw and getGadget.
  double pickerPadLocal() const;

public:
  ShiftTraceTool();

  ToolType getToolType() const override { return GenericTool; }

  void clearData();
  void updateData();
  void updateCurveAffs();
  void updateGhost();

  void reset() override;

  void mouseMove(const TPointD &, const TMouseEvent &e) override;
  void leftButtonDown(const TPointD &, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &, const TMouseEvent &) override;
  void leftButtonUp(const TPointD &, const TMouseEvent &) override;
  void draw() override;

  TAffine getGhostAff() const;
  TAffine getGhostAffForIndex(int ghostIndex) const;
  GadgetId getGadget(const TPointD &);
  void drawDot(const TPointD &center, double r,
               const TPixel32 &color = TPixel32::White);
  void drawControlRect();
  void drawCurve();

  void onActivate() override;
  void onDeactivate() override;

  void onLeave() override;

  bool isEventAcceptable(QEvent *e) override;

  int getCursorId() const override;

  int getCurrentGhostIndex() { return m_ghostIndex; }
  void setCurrentGhostIndex(int index);

  //! True when ghost shift/scale/rotation and pivot match the default for the
  //! current reference rectangle (bbox mode, frame, etc.).
  bool isGhostAtDefault(int ghostIndex) const;
};