

#include "tools/cursormanager.h"
#include "tools/tool.h"
#include "tools/cursors.h"

#include "../toonz/preferences.h"

#include <QWidget>
#include <QPixmap>
#include <assert.h>
#include <map>
#include <QDebug>
#include <QPainter>

namespace {

const struct {
  int cursorType;
  const char *pixmapFilename;
  int x, y;
  bool flippable;
} cursorInfo[] = {
    {ToolCursor::PenCursor, "brush", 16, 15, false},
    {ToolCursor::PenLargeCursor, "brush_large", 16, 15, false},
    {ToolCursor::PenCrosshairCursor, "brush_crosshair", 16, 15, false},
    {ToolCursor::BenderCursor, "bender", 9, 7, true},
    {ToolCursor::CutterCursor, "cutter", 6, 24, true},  // 12,20, ???},
    {ToolCursor::EraserCursor, "eraser", 7, 21, true},  // 15,16, ???},
    {ToolCursor::DistortCursor, "selection_distort", 11, 6, true},
    {ToolCursor::FillCursor, "fill", 3, 26, true},
    {ToolCursor::MoveCursor, "move", 15, 15, false},
    {ToolCursor::MoveEWCursor, "move_ew", 15, 15, false},
    {ToolCursor::MoveNSCursor, "move_ns", 15, 15, false},
    {ToolCursor::DisableCursor, "disable", 15, 15, false},
    {ToolCursor::MoveZCursor, "", 0, 0, false},
    {ToolCursor::MoveZCursorBase, "move_z_notext", 15, 15, false},
    {ToolCursor::FxGadgetCursor, "", 0, 0, false},
    {ToolCursor::FxGadgetCursorBase, "edit_FX_notext", 11, 6, true},
    {ToolCursor::FlipHCursor, "flip_h", 15, 15, false},
    {ToolCursor::FlipVCursor, "flip_v", 15, 15, false},
    {ToolCursor::IronCursor, "iron", 15, 15, true},
    {ToolCursor::LevelSelectCursor, "level_select", 7, 3, true},
    {ToolCursor::MagnetCursor, "magnet", 18, 18, true},
    {ToolCursor::PanCursor, "pan", 17, 17, true},

    {ToolCursor::PickerCursorLine, "", 0, 0, false},
    {ToolCursor::PickerCursorLineBase, "picker_style", 7, 22, true},
    {ToolCursor::PickerCursorArea, "", 0, 0, false},
    {ToolCursor::PickerCursorAreaBase, "picker_style", 7, 22, true},
    {ToolCursor::PickerCursor, "picker_style", 7, 22, true},

    {ToolCursor::PumpCursor, "pump", 16, 23, false},
    {ToolCursor::RotCursor, "rot", 15, 15, false},
    {ToolCursor::RotTopLeft, "rot_top_left", 15, 15, false},
    {ToolCursor::RotBottomRight, "rot_bottom_right", 15, 15, false},
    {ToolCursor::RotBottomLeft, "rot_bottom_left", 15, 15, false},
    {ToolCursor::RotateCursor, "rotate", 15, 19, true},
    {ToolCursor::ScaleCursor, "scale", 15, 15, false},
    {ToolCursor::ScaleInvCursor, "scale_inv", 15, 15, false},
    {ToolCursor::ScaleHCursor, "scale_h", 15, 15, false},
    {ToolCursor::ScaleVCursor, "scale_v", 15, 15, false},
    {ToolCursor::EditFxCursor, "", 0, 0, false},
    {ToolCursor::EditFxCursorBase, "edit_FX_notext", 11, 6, true},
    {ToolCursor::ScaleGlobalCursor, "scale_global", 15, 15, false},
    {ToolCursor::ScaleHVCursor, "", 0, 0, false},
    {ToolCursor::ScaleHVCursorBase, "scale_hv_notext", 15, 15, false},
    {ToolCursor::StrokeSelectCursor, "stroke_select", 11, 6, true},
    {ToolCursor::TapeCursor, "tape", 4, 23, true},
    {ToolCursor::TrackerCursor, "tracker", 16, 15, false},
    {ToolCursor::TypeInCursor, "type_in", 16, 19, false},
    {ToolCursor::TypeOutCursor, "type_out", 16, 19, false},
    {ToolCursor::ZoomCursor, "zoom", 14, 14, true},
    {ToolCursor::PinchCursor, "pinch_curve", 6, 16, true},
    {ToolCursor::PinchAngleCursor, "pinch_angle", 6, 16, true},
    {ToolCursor::PinchWaveCursor, "pinch_wave", 6, 16, true},
    {ToolCursor::SplineEditorCursor, "stroke_select", 11, 6, true},
    {ToolCursor::SplineEditorCursorAdd, "selection_add", 11, 6, true},
    {ToolCursor::SplineEditorCursorSelect, "selection_convert", 11, 6, true},
    {ToolCursor::NormalEraserCursor, "normaleraser", 7, 19, true},
    {ToolCursor::RectEraserCursor, "recteraser", 3, 26, true},
    {ToolCursor::PickerCursorOrganize, "picker_style_organize", 7, 22, true},
    {ToolCursor::PickerRGB, "", 0, 0, false},
    {ToolCursor::PickerRGBBase, "picker_style", 7, 22, true},
    {ToolCursor::PickerRGBWhite, "picker_rgb_white", 7, 22, true},
    {ToolCursor::FillCursorL, "karasu", 7, 25, true},
    {ToolCursor::RulerModifyCursor, "ruler_modify", 7, 7, true},
    {ToolCursor::RulerNewCursor, "ruler_new", 7, 7, true},
    {0, 0, 0, 0, false}};

struct CursorData {
  QPixmap pixmap;
  int x, y;
};

const struct {
  int decorateType;
  const char *pixmapFilename;
} decorateInfo[] = {{ToolCursor::Ex_FreeHand, "ex_freehand"},
                    {ToolCursor::Ex_PolyLine, "ex_polyline"},
                    {ToolCursor::Ex_Rectangle, "ex_rectangle"},
                    {ToolCursor::Ex_Line, "ex_line"},
                    {ToolCursor::Ex_Area, "ex_area"},
                    {ToolCursor::Ex_Fill_NoAutopaint, "ex_fill_no_autopaint"},
                    {ToolCursor::Ex_FX, "ex_FX"},
                    {ToolCursor::Ex_Z, "ex_z"},
                    {ToolCursor::Ex_StyleLine, "ex_style_line"},
                    {ToolCursor::Ex_StyleArea, "ex_style_area"},
                    {ToolCursor::Ex_RGB, "ex_rgb"},
                    {ToolCursor::Ex_HV, "ex_hv"},
                    {0, 0}};
};

//=============================================================================
// CursorManager
//-----------------------------------------------------------------------------

class CursorManager {  // singleton

  std::map<int, CursorData> m_cursors;
  std::map<int, CursorData> m_cursorsLeft;

  CursorManager() {}

public:
  static CursorManager *instance() {
    static CursorManager _instance;
    return &_instance;
  }

  void doDecoration(QPixmap &pixmap, int decorationFlag, bool useLeft) {
    if (decorationFlag == 0) return;
    if (decorationFlag > ToolCursor::Ex_Negate) {
      QPainter p(&pixmap);
      p.setCompositionMode(QPainter::CompositionMode_SourceOver);
      for (int i = 0; decorateInfo[i].pixmapFilename; i++)
        if (decorationFlag & decorateInfo[i].decorateType) {
          QString leftStr      = "";
          if (useLeft) leftStr = "_left";
          QString path         = QString(":Resources/") +
                         decorateInfo[i].pixmapFilename + leftStr + ".png";
          p.drawPixmap(0, 0, QPixmap(path));
        }
    }
    // negate
    if (decorationFlag & ToolCursor::Ex_Negate) {
      QImage img = pixmap.toImage();
      img.invertPixels(QImage::InvertRgb);  // leave the alpha channel unchanged
      pixmap = QPixmap::fromImage(img);
    }
  }

  const CursorData &getCursorData(int cursorType) {
    // se e' gia' in tabella lo restituisco
    std::map<int, CursorData>::iterator it;

    if (Preferences::instance()->getCursorBrushStyle() == "Simple")
      cursorType = ToolCursor::PenCursor;

    if (cursorType == ToolCursor::PenCursor) {
      QString brushType = Preferences::instance()->getCursorBrushType();
      if (brushType == "Large")
        cursorType = ToolCursor::PenLargeCursor;
      else if (brushType == "Crosshair")
        cursorType = ToolCursor::PenCrosshairCursor;
    }

    bool useLeft =
        (Preferences::instance()->getCursorBrushStyle() == "Left-Handed");
    if (useLeft) {
      it = m_cursorsLeft.find(cursorType);
      if (it != m_cursorsLeft.end()) return it->second;
    } else {
      it = m_cursors.find(cursorType);
      if (it != m_cursors.end()) return it->second;
    }

    int decorationsFlag = cursorType & ~(0xFF);
    int baseCursorType  = cursorType & 0xFF;

    if (baseCursorType == ToolCursor::CURSOR_ARROW) {
      CursorData data;
      QCursor leftArrow(Qt::ArrowCursor);
      data.pixmap = leftArrow.pixmap();
      data.x      = leftArrow.hotSpot().x();
      data.y      = leftArrow.hotSpot().y();

      if (useLeft) {
        QImage target = (&data.pixmap)->toImage();
        (&data.pixmap)->convertFromImage(target.mirrored(true, false));
        data.x = data.pixmap.width() - data.x - 1;
        it     = m_cursorsLeft.insert(std::make_pair(cursorType, data)).first;
      } else
        it = m_cursors.insert(std::make_pair(cursorType, data)).first;

      return it->second;
    }

    // provo a cercarlo in cursorInfo[]
    int i;
    for (i = 0; cursorInfo[i].pixmapFilename; i++)
      if (baseCursorType == cursorInfo[i].cursorType) {
        QString path =
            QString(":Resources/") + cursorInfo[i].pixmapFilename + ".png";
        CursorData data;
        data.pixmap = QPixmap(path);
        if (data.pixmap.isNull())
          data = getCursorData(ToolCursor::CURSOR_ARROW);
        else {
          data.x = cursorInfo[i].x;
          data.y = cursorInfo[i].y;
          if (useLeft && cursorInfo[i].flippable) {
            QImage target = (&data.pixmap)->toImage();
            (&data.pixmap)->convertFromImage(target.mirrored(true, false));
            data.x = data.pixmap.width() - cursorInfo[i].x - 1;
          }
          if (decorationsFlag != 0)
            doDecoration(data.pixmap, decorationsFlag, useLeft);
        }
        if (useLeft)
          it = m_cursorsLeft.insert(std::make_pair(cursorType, data)).first;
        else
          it = m_cursors.insert(std::make_pair(cursorType, data)).first;
        return it->second;
      }
    // niente da fare. uso un default
    CursorData data;
    static const QPixmap standardCursorPixmap("cursors/hook.png");
    data.pixmap = standardCursorPixmap;
    data.x = data.y = 0;
    if (useLeft)
      it = m_cursorsLeft.insert(std::make_pair(cursorType, data)).first;
    else
      it = m_cursors.insert(std::make_pair(cursorType, data)).first;
    return it->second;
  }

  QCursor getCursor(int cursorType) {
    // assert(cursorType!=0);

    QCursor cursor;
    /*
if (cursorType == ToolCursor::CURSOR_ARROW)
  cursor = Qt::ArrowCursor;
else */
    if (cursorType == ToolCursor::ForbiddenCursor)
      cursor = Qt::ForbiddenCursor;
    else {
      const CursorData &data = getCursorData(cursorType);
      cursor                 = QCursor(data.pixmap, data.x, data.y);
    }

    return cursor;
  }
};

//-----------------------------------------------------------------------------

void setToolCursor(QWidget *viewer, int cursorType) {
  viewer->setCursor(CursorManager::instance()->getCursor(cursorType));
}

//-----------------------------------------------------------------------------

QCursor getToolCursor(int cursorType) {
  return CursorManager::instance()->getCursor(cursorType);
}
