

#include "shifttracetool.h"
#include "toonz/onionskinmask.h"
#include "toonz/tonionskinmaskhandle.h"
#include "tools/cursors.h"
#include "timage.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "tools/toolhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/dpiscale.h"
#include "toonz/stage.h"
#include "tpixel.h"
#include "toonzqt/menubarcommand.h"

#include "toonz/preferences.h"
#include "toonzqt/gutil.h"

#include "tgl.h"
#include <math.h>
#include <QKeyEvent>

//=============================================================================

static bool circumCenter(TPointD &out, const TPointD &a, const TPointD &b,
                         const TPointD &c) {
  double d = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
  if (fabs(d) < 0.0001) {
    out = TPointD();
    return false;
  }
  out.x = ((a.y * a.y + a.x * a.x) * (b.y - c.y) +
           (b.y * b.y + b.x * b.x) * (c.y - a.y) +
           (c.y * c.y + c.x * c.x) * (a.y - b.y)) /
          d;
  out.y = ((a.y * a.y + a.x * a.x) * (c.x - b.x) +
           (b.y * b.y + b.x * b.x) * (a.x - c.x) +
           (c.y * c.y + c.x * c.x) * (b.x - a.x)) /
          d;
  return true;
}

//=============================================================================

ShiftTraceTool::ShiftTraceTool()
    : TTool("T_ShiftTrace")
    , m_ghostIndex(0)
    , m_curveStatus(NoCurve)
    , m_gadget(NoGadget)
    , m_highlightedGadget(NoGadget) {
  bind(TTool::AllTargets);  // Deals with tool deactivation internally
}

void ShiftTraceTool::clearData() {
  m_ghostIndex        = 0;
  m_curveStatus       = NoCurve;
  m_gadget            = NoGadget;
  m_highlightedGadget = NoGadget;

  m_box = TRectD();
  for (int i = 0; i < 2; i++) {
    m_row[i]    = -1;
    m_aff[i]    = TAffine();
    m_center[i] = TPointD();
  }
}

void ShiftTraceTool::updateBox() {
  if (m_ghostIndex < 0 || 2 <= m_ghostIndex || m_row[m_ghostIndex] < 0) return;

  TImageP img;

  TApplication *app = TTool::getApplication();
  if (app->getCurrentFrame()->isEditingScene()) {
    int col      = app->getCurrentColumn()->getColumnIndex();
    int row      = m_row[m_ghostIndex];
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    TXshCell cell       = xsh->getCell(row, col);
    TXshSimpleLevel *sl = cell.getSimpleLevel();
    if (sl) {
      m_dpiAff = getDpiAffine(sl, cell.m_frameId);
      img      = cell.getImage(false);
    }
  }
  // on editing level
  else {
    TXshLevel *level = app->getCurrentLevel()->getLevel();
    if (!level) return;
    TXshSimpleLevel *sl = level->getSimpleLevel();
    if (!sl) return;

    const TFrameId &ghostFid = sl->index2fid(m_row[m_ghostIndex]);
    m_dpiAff                 = getDpiAffine(sl, ghostFid);
    img                      = sl->getFrame(ghostFid, false);
  }

  if (img) {
    if (TRasterImageP ri = img) {
      TRasterP ras = ri->getRaster();
      m_box        = (convert(ras->getBounds()) - ras->getCenterD()) *
              ri->getSubsampling();
    } else if (TToonzImageP ti = img) {
      TRasterP ras = ti->getRaster();
      m_box        = (convert(ras->getBounds()) - ras->getCenterD()) *
              ti->getSubsampling();
    } else if (TVectorImageP vi = img) {
      m_box = vi->getBBox();
    }
  }
}

void ShiftTraceTool::updateData() {
  m_box = TRectD();
  for (int i = 0; i < 2; i++) m_row[i] = -1;
  m_dpiAff                             = TAffine();
  TApplication *app                    = TTool::getApplication();

  OnionSkinMask osm  = app->getCurrentOnionSkin()->getOnionSkinMask();
  int previousOffset = osm.getShiftTraceGhostFrameOffset(0);
  int forwardOffset  = osm.getShiftTraceGhostFrameOffset(1);
  // we must find the prev (m_row[0]) and next (m_row[1]) reference images
  // (either might not exist)
  // see also stage.cpp, StageBuilder::addCellWithOnionSkin
  if (app->getCurrentFrame()->isEditingScene()) {
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    int row       = app->getCurrentFrame()->getFrame();
    int col       = app->getCurrentColumn()->getColumnIndex();
    TXshCell cell = xsh->getCell(row, col);
    int r;
    r = row + previousOffset;
    if (r >= 0 && xsh->getCell(r, col) != cell &&
        (cell.getSimpleLevel() == 0 ||
         xsh->getCell(r, col).getSimpleLevel() == cell.getSimpleLevel())) {
      m_row[0] = r;
    }

    r = row + forwardOffset;
    if (r >= 0 && xsh->getCell(r, col) != cell &&
        (cell.getSimpleLevel() == 0 ||
         xsh->getCell(r, col).getSimpleLevel() == cell.getSimpleLevel())) {
      m_row[1] = r;
    }
  }
  // on editing level
  else {
    TXshLevel *level = app->getCurrentLevel()->getLevel();
    if (level) {
      TXshSimpleLevel *sl = level->getSimpleLevel();
      if (sl) {
        TFrameId fid = app->getCurrentFrame()->getFid();
        int row      = sl->guessIndex(fid);
        m_row[0]     = row + previousOffset;
        m_row[1]     = row + forwardOffset;
      }
    }
  }
  updateBox();
}

//
// Compute m_aff[0] and m_aff[1] according to the current curve
//
void ShiftTraceTool::updateCurveAffs() {
  if (m_curveStatus != ThreePointsCurve) {
    m_aff[0] = m_aff[1] = TAffine();
  } else {
    double phi0 = 0, phi1 = 0;
    TPointD center;
    if (circumCenter(center, m_p0, m_p1, m_p2)) {
      TPointD v0 = normalize(m_p0 - center);
      TPointD v1 = normalize(m_p1 - center);
      TPointD v2 = normalize(m_p2 - center);
      TPointD u0(-v0.y, v0.x);
      TPointD u1(-v1.y, v1.x);
      phi0 = atan2((v2 * u0), (v2 * v0)) * 180.0 / 3.1415;
      phi1 = atan2((v2 * u1), (v2 * v1)) * 180.0 / 3.1415;
    }
    m_aff[0] = TTranslation(m_p2 - m_p0) * TRotation(m_p0, phi0);
    m_aff[1] = TTranslation(m_p2 - m_p1) * TRotation(m_p1, phi1);
  }
}

void ShiftTraceTool::updateGhost() {
  OnionSkinMask osm =
      TTool::getApplication()->getCurrentOnionSkin()->getOnionSkinMask();
  osm.setShiftTraceGhostAff(0, m_aff[0]);
  osm.setShiftTraceGhostAff(1, m_aff[1]);
  osm.setShiftTraceGhostCenter(0, m_center[0]);
  osm.setShiftTraceGhostCenter(1, m_center[1]);
  TTool::getApplication()->getCurrentOnionSkin()->setOnionSkinMask(osm);
}

void ShiftTraceTool::reset() {
  int ghostIndex = m_ghostIndex;
  onActivate();
  invalidate();
  m_ghostIndex = ghostIndex;

  TTool::getApplication()
      ->getCurrentTool()
      ->notifyToolChanged();  // Refreshes toolbar values
}

TAffine ShiftTraceTool::getGhostAff() {
  if (0 <= m_ghostIndex && m_ghostIndex < 2)
    return m_aff[m_ghostIndex] * m_dpiAff;
  else
    return TAffine();
}

void ShiftTraceTool::drawDot(const TPointD &center, double r,
                             const TPixel32 &color) {
  tglColor(color);
  tglDrawDisk(center, r);
  glColor3d(0.2, 0.2, 0.2);
  tglDrawCircle(center, r);
}

void ShiftTraceTool::drawControlRect() {  // TODO
  if (m_ghostIndex < 0 || m_ghostIndex > 1) return;
  int row = m_row[m_ghostIndex];
  if (row < 0) return;

  TRectD box = m_box;
  if (box.isEmpty()) return;
  glPushMatrix();
  tglMultMatrix(getGhostAff());

  TPixel32 color;

  // draw onion-colored rectangle to indicate which ghost is grabbed
  {
    TPixel32 frontOniColor, backOniColor;
    bool inksOnly;
    Preferences::instance()->getOnionData(frontOniColor, backOniColor,
                                          inksOnly);
    color       = (m_ghostIndex == 0) ? backOniColor : frontOniColor;
    double unit = sqrt(tglGetPixelSize2());
    unit *= getDevPixRatio();
    TRectD coloredBox = box.enlarge(3.0 * unit);
    tglColor(color);
    glBegin(GL_LINE_STRIP);
    glVertex2d(coloredBox.x0, coloredBox.y0);
    glVertex2d(coloredBox.x1, coloredBox.y0);
    glVertex2d(coloredBox.x1, coloredBox.y1);
    glVertex2d(coloredBox.x0, coloredBox.y1);
    glVertex2d(coloredBox.x0, coloredBox.y0);
    glEnd();
  }

  color = m_highlightedGadget == TranslateGadget
              ? TPixel32(200, 100, 100)
              : m_highlightedGadget == RotateGadget ? TPixel32(100, 200, 100)
                                                    : TPixel32(120, 120, 120);
  tglColor(color);
  glBegin(GL_LINE_STRIP);
  glVertex2d(box.x0, box.y0);
  glVertex2d(box.x1, box.y0);
  glVertex2d(box.x1, box.y1);
  glVertex2d(box.x0, box.y1);
  glVertex2d(box.x0, box.y0);
  glEnd();
  color = m_highlightedGadget == ScaleGadget ? TPixel32(200, 100, 100)
                                             : TPixel32::White;
  double r = 4 * sqrt(tglGetPixelSize2());
  drawDot(box.getP00(), r, color);
  drawDot(box.getP01(), r, color);
  drawDot(box.getP10(), r, color);
  drawDot(box.getP11(), r, color);
  if (m_curveStatus == NoCurve) {
    color = m_highlightedGadget == MoveCenterGadget ? TPixel32(200, 100, 100)
                                                    : TPixel32::White;
    TPointD c = m_center[m_ghostIndex];
    drawDot(c, r, color);
  }
  glPopMatrix();
}

void ShiftTraceTool::drawCurve() {
  if (m_curveStatus == NoCurve) return;
  double r = 4 * sqrt(tglGetPixelSize2());
  double u = getPixelSize();
  if (m_curveStatus == TwoPointsCurve) {
    TPixel32 color = m_highlightedGadget == CurveP0Gadget
                         ? TPixel32(200, 100, 100)
                         : TPixel32::White;
    drawDot(m_p0, r, color);
    glColor3d(0.2, 0.2, 0.2);
    tglDrawSegment(m_p0, m_p1);
    drawDot(m_p1, r, TPixel32::Red);
  } else if (m_curveStatus == ThreePointsCurve) {
    TPixel32 color = m_highlightedGadget == CurveP0Gadget
                         ? TPixel32(200, 100, 100)
                         : TPixel32::White;
    drawDot(m_p0, r, color);
    color = m_highlightedGadget == CurveP1Gadget ? TPixel32(200, 100, 100)
                                                 : TPixel32::White;
    drawDot(m_p1, r, color);

    glColor3d(0.2, 0.2, 0.2);

    TPointD center;
    if (circumCenter(center, m_p0, m_p1, m_p2)) {
      double radius = norm(center - m_p1);
      glBegin(GL_LINE_STRIP);
      int n = 100;
      for (int i = 0; i < n; i++) {
        double t  = (double)i / n;
        TPointD p = (1 - t) * m_p0 + t * m_p2;
        p         = center + radius * normalize(p - center);
        tglVertex(p);
      }
      for (int i = 0; i < n; i++) {
        double t  = (double)i / n;
        TPointD p = (1 - t) * m_p2 + t * m_p1;
        p         = center + radius * normalize(p - center);
        tglVertex(p);
      }
      glEnd();
    } else {
      tglDrawSegment(m_p0, m_p1);
    }
    color = m_highlightedGadget == CurvePmGadget ? TPixel32(200, 100, 100)
                                                 : TPixel32::White;
    drawDot(m_p2, r, color);
  }
}

void ShiftTraceTool::onActivate() {
  m_ghostIndex  = 0;
  m_curveStatus = NoCurve;
  clearData();
  OnionSkinMask osm =
      TTool::getApplication()->getCurrentOnionSkin()->getOnionSkinMask();
  m_aff[0]    = osm.getShiftTraceGhostAff(0);
  m_aff[1]    = osm.getShiftTraceGhostAff(1);
  m_center[0] = osm.getShiftTraceGhostCenter(0);
  m_center[1] = osm.getShiftTraceGhostCenter(1);
}

void ShiftTraceTool::onDeactivate() {
  QAction *action = CommandManager::instance()->getAction("MI_EditShift");
  action->setChecked(false);
}

ShiftTraceTool::GadgetId ShiftTraceTool::getGadget(const TPointD &p) {
  std::vector<std::pair<TPointD, GadgetId>> gadgets;
  gadgets.push_back(std::make_pair(m_p0, CurveP0Gadget));
  gadgets.push_back(std::make_pair(m_p1, CurveP1Gadget));
  gadgets.push_back(std::make_pair(m_p2, CurvePmGadget));
  TAffine aff      = getGhostAff();
  double pixelSize = getPixelSize();
  double d         = 15 * pixelSize;  // offset for rotation handle
  if (0 <= m_ghostIndex && m_ghostIndex < 2) {
    gadgets.push_back(std::make_pair(aff * m_box.getP00(), ScaleGadget));
    gadgets.push_back(std::make_pair(aff * m_box.getP01(), ScaleGadget));
    gadgets.push_back(std::make_pair(aff * m_box.getP10(), ScaleGadget));
    gadgets.push_back(std::make_pair(aff * m_box.getP11(), ScaleGadget));
    gadgets.push_back(
        std::make_pair(aff * m_center[m_ghostIndex], MoveCenterGadget));
  }
  int k           = -1;
  double minDist2 = pow(10 * pixelSize, 2);
  for (int i = 0; i < (int)gadgets.size(); i++) {
    double d2 = norm2(gadgets[i].first - p);
    if (d2 < minDist2) {
      minDist2 = d2;
      k        = i;
    }
  }
  if (k >= 0) return gadgets[k].second;

  // rect-point
  if (0 <= m_ghostIndex && m_ghostIndex < 2) {
    TPointD q  = aff.inv() * p;
    double big = 1.0e6;
    double d = big, x = 0, y = 0;
    if (m_box.x0 < q.x && q.x < m_box.x1) {
      x         = q.x;
      double d0 = fabs(m_box.y0 - q.y);
      double d1 = fabs(m_box.y1 - q.y);
      if (d0 < d1) {
        d = d0;
        y = m_box.y0;
      } else {
        d = d1;
        y = m_box.y1;
      }
    }
    if (m_box.y0 < q.y && q.y < m_box.y1) {
      double d0 = fabs(m_box.x0 - q.x);
      double d1 = fabs(m_box.x1 - q.x);
      if (d0 < d) {
        d = d0;
        y = q.y;
        x = m_box.x0;
      }
      if (d1 < d) {
        d = d1;
        y = q.y;
        x = m_box.x1;
      }
    }
    if (d < big) {
      TPointD pp = aff * TPointD(x, y);
      double d   = norm(p - pp);
      if (d < 10 * getPixelSize()) {
        if (m_box.contains(q))
          return TranslateGadget;
        else
          return RotateGadget;
      }
    }
    if (m_box.contains(q))
      return NoGadget_InBox;
    else
      return NoGadget;
  }
  return NoGadget;
}

void ShiftTraceTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  GadgetId highlightedGadget = getGadget(pos);
  if (highlightedGadget != m_highlightedGadget) {
    m_highlightedGadget = highlightedGadget;
    invalidate();
  }
}

void ShiftTraceTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_gadget = m_highlightedGadget;
  m_oldPos = m_startPos = pos;

  bool notify = false;

  if (m_gadget == NoGadget || m_gadget == NoGadget_InBox) {
    if (!e.isCtrlPressed()) {
      if (m_gadget == NoGadget_InBox)
        m_gadget = TranslateGadget;
      else
        m_gadget = RotateGadget;
      // m_curveStatus = NoCurve;
    }
    int row = getViewer()->posToRow(e.m_pos, 5 * getPixelSize(), false, true);
    if (row >= 0) {
      int index         = -1;
      TApplication *app = TTool::getApplication();
      if (app->getCurrentFrame()->isEditingScene()) {
        int currentRow = getFrame();
        if (m_row[0] >= 0 && row < currentRow)
          index = 0;
        else if (m_row[1] >= 0 && row > currentRow)
          index = 1;
      } else {
        if (m_row[0] == row)
          index = 0;
        else if (m_row[1] == row)
          index = 1;
      }

      if (index >= 0) {
        m_ghostIndex = index;
        updateBox();
        m_gadget            = TranslateGadget;
        m_highlightedGadget = TranslateGadget;
        notify              = true;
      }
    }
  }

  m_oldAff = m_aff[m_ghostIndex];
  invalidate();

  if (notify) {
    TTool::getApplication()
        ->getCurrentTool()
        ->notifyToolChanged();  // Refreshes toolbar values
  }
}

void ShiftTraceTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (m_gadget == NoGadget || m_gadget == NoGadget_InBox) {
    if (norm(pos - m_oldPos) > 10 * getPixelSize()) {
      m_curveStatus = TwoPointsCurve;
      m_p0          = m_oldPos;
      m_gadget      = CurveP1Gadget;
    }
  }

  if (isCurveGadget(m_gadget)) {
    if (m_gadget == CurveP0Gadget)
      m_p0 = pos;
    else if (m_gadget == CurveP1Gadget)
      m_p1 = pos;
    else
      m_p2 = pos;
    updateCurveAffs();
  } else if (m_gadget == RotateGadget) {
    TAffine aff = getGhostAff();
    TPointD c   = aff * m_center[m_ghostIndex];
    TPointD a   = m_oldPos - c;
    TPointD b   = pos - c;
    m_oldPos    = pos;
    TPointD u   = normalize(a);
    double phi =
        atan2(-u.y * b.x + u.x * b.y, u.x * b.x + u.y * b.y) * 180.0 / 3.14153;

    TPointD imgC = aff * m_center[m_ghostIndex];

    m_aff[m_ghostIndex] = TRotation(imgC, phi) * m_aff[m_ghostIndex];
  } else if (m_gadget == MoveCenterGadget) {
    TAffine aff   = getGhostAff().inv();
    TPointD delta = aff * pos - aff * m_oldPos;
    m_oldPos      = pos;
    m_center[m_ghostIndex] += delta;
  } else if (m_gadget == TranslateGadget) {
    TPointD delta       = pos - m_oldPos;
    m_oldPos            = pos;
    m_aff[m_ghostIndex] = TTranslation(delta) * m_aff[m_ghostIndex];
  } else if (m_gadget == ScaleGadget) {
    TAffine aff = getGhostAff();
    TPointD c   = aff * m_center[m_ghostIndex];
    TPointD a   = m_oldPos - c;
    TPointD b   = pos - c;
    if (e.isShiftPressed())
      m_aff[m_ghostIndex] = m_oldAff * TScale(b.x / a.x, b.y / a.y);
    else {
      double scale        = std::max(b.x / a.x, b.y / a.y);
      m_aff[m_ghostIndex] = m_oldAff * TScale(scale, scale);
    }
  }

  updateGhost();
  invalidate();
}

void ShiftTraceTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (CurveP0Gadget <= m_gadget && m_gadget <= CurvePmGadget) {
    if (m_curveStatus == TwoPointsCurve) {
      m_p2          = (m_p0 + m_p1) * 0.5;
      m_curveStatus = ThreePointsCurve;
      updateCurveAffs();
      updateGhost();

      m_center[0] = (m_aff[0] * m_dpiAff).inv() * m_p2;
      m_center[1] = (m_aff[1] * m_dpiAff).inv() * m_p2;
    }
  }
  m_gadget = NoGadget;
  invalidate();

  TTool::getApplication()
      ->getCurrentTool()
      ->notifyToolChanged();  // Refreshes toolbar values
}

void ShiftTraceTool::draw() {
  updateData();
  drawControlRect();
  drawCurve();
}

int ShiftTraceTool::getCursorId() const {
  if (m_highlightedGadget == RotateGadget || m_highlightedGadget == NoGadget)
    return ToolCursor::RotateCursor;
  else if (m_highlightedGadget == ScaleGadget)
    return ToolCursor::ScaleCursor;
  else if (isCurveGadget(m_highlightedGadget))
    return ToolCursor::PinchCursor;
  else  // Curve Points, TranslateGadget, NoGadget_InBox
    return ToolCursor::MoveCursor;
}

bool ShiftTraceTool::isEventAcceptable(QEvent *e) {
  // F1, F2 and F3 keys are used for flipping
  QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
  int key             = keyEvent->key();
  return (Qt::Key_F1 <= key && key <= Qt::Key_F3);
}

void ShiftTraceTool::onLeave() {
  OnionSkinMask osm =
      TTool::getApplication()->getCurrentOnionSkin()->getOnionSkinMask();
  osm.clearGhostFlipKey();
  TTool::getApplication()->getCurrentOnionSkin()->setOnionSkinMask(osm);
}

void ShiftTraceTool::setCurrentGhostIndex(int index) {
  m_ghostIndex = index;
  updateBox();
  invalidate();
}

ShiftTraceTool shiftTraceTool;
