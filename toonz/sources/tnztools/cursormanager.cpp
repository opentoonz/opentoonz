

#include "tools/cursormanager.h"
#include "tools/tool.h"
#include "tools/cursors.h"

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
} cursorInfo[] = {
    {ToolCursor::PenCursor, "brush", 16, 16},
    {ToolCursor::BenderCursor, "bender", 10, 7},
    {ToolCursor::CutterCursor, "cutter", 6, 24},  // 12,20},
    {ToolCursor::EraserCursor, "eraser", 7, 21},  // 15,16},
    {ToolCursor::DistortCursor, "selection_distort", 11, 6},
    {ToolCursor::FillCursor, "fill", 3, 26},
    {ToolCursor::MoveCursor, "move", 15, 15},
    {ToolCursor::MoveEWCursor, "move_ew", 15, 15},
    {ToolCursor::MoveNSCursor, "move_ns", 15, 15},
    {ToolCursor::DisableCursor, "disable", 15, 15},
    {ToolCursor::MoveZCursor, "move_z", 15, 15},
    {ToolCursor::FxGadgetCursor, "edit_FX", 7, 7},
    {ToolCursor::FlipHCursor, "flip_h", 15, 15},
    {ToolCursor::FlipVCursor, "flip_v", 15, 15},
    {ToolCursor::IronCursor, "iron", 15, 15},
    {ToolCursor::LevelSelectCursor, "level_select", 7, 4},
    {ToolCursor::MagnetCursor, "magnet", 18, 18},
    {ToolCursor::PanCursor, "pan", 18, 19},

    {ToolCursor::PickerCursorLine, "picker_style_line", 7, 22},
    {ToolCursor::PickerCursorArea, "picker_style_area", 7, 22},
    {ToolCursor::PickerCursor, "picker_style", 7, 22},

    {ToolCursor::PumpCursor, "pump", 16, 23},
    {ToolCursor::RotCursor, "rot", 15, 15},
    {ToolCursor::RotTopLeft, "rot_top_left", 15, 15},
    {ToolCursor::RotBottomRight, "rot_bottom_right", 15, 15},
    {ToolCursor::RotBottomLeft, "rot_bottom_left", 15, 15},
    {ToolCursor::RotateCursor, "rotate", 15, 19},
    {ToolCursor::ScaleCursor, "scale", 15, 15},
    {ToolCursor::ScaleInvCursor, "scale_inv", 15, 15},
    {ToolCursor::ScaleHCursor, "scale_h", 15, 15},
    {ToolCursor::ScaleVCursor, "scale_v", 15, 15},
    {ToolCursor::EditFxCursor, "edit_FX", 11, 6},
    {ToolCursor::ScaleGlobalCursor, "scale_global", 15, 15},
    {ToolCursor::ScaleHVCursor, "scale_hv", 15, 15},
    {ToolCursor::StrokeSelectCursor, "stroke_select", 11, 6},
    {ToolCursor::TapeCursor, "tape", 4, 23},
    {ToolCursor::TrackerCursor, "tracker", 12, 15},
    {ToolCursor::TypeInCursor, "type_in", 16, 19},
    {ToolCursor::TypeOutCursor, "type_out", 16, 19},
    {ToolCursor::ZoomCursor, "zoom", 14, 14},
    {ToolCursor::PinchCursor, "pinch_curve", 10, 18},
    {ToolCursor::PinchAngleCursor, "pinch_angle", 6, 16},
    {ToolCursor::PinchWaveCursor, "pinch_wave", 6, 16},
    {ToolCursor::SplineEditorCursor, "stroke_select", 11, 6},
    {ToolCursor::SplineEditorCursorAdd, "selection_add", 11, 6},
    {ToolCursor::SplineEditorCursorSelect, "selection_convert", 11, 6},
    {ToolCursor::NormalEraserCursor, "normaleraser", 3, 26},
    {ToolCursor::RectEraserCursor, "recteraser", 3, 26},
    {ToolCursor::PickerCursorOrganize, "picker_style_organize", 7, 22},
    {ToolCursor::PickerRGB, "picker_rgb", 7, 22},
    {ToolCursor::PickerRGBWhite, "picker_rgb_white", 7, 22},
    {ToolCursor::FillCursorL, "karasu", 7, 25},
    {ToolCursor::RulerModifyCursor, "ruler_modify", 7, 7},
    {ToolCursor::RulerNewCursor, "ruler_new", 7, 7},
    {0, 0, 0, 0}};

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
                    {0, 0}};
};

//=============================================================================
// CursorManager
//-----------------------------------------------------------------------------

class CursorManager {  // singleton

  std::map<int, CursorData> m_cursors;

  CursorManager() {}

public:
  static CursorManager *instance() {
    static CursorManager _instance;
    return &_instance;
  }

  void doDecoration(QPixmap &pixmap, int decorationFlag) {
    if (decorationFlag == 0) return;
    if (decorationFlag > ToolCursor::Ex_Negate) {
      QPainter p(&pixmap);
      p.setCompositionMode(QPainter::CompositionMode_SourceOver);
      for (int i = 0; decorateInfo[i].pixmapFilename; i++)
        if (decorationFlag & decorateInfo[i].decorateType) {
          QString path =
              QString(":Resources/") + decorateInfo[i].pixmapFilename + ".png";
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
    it = m_cursors.find(cursorType);
    if (it != m_cursors.end()) return it->second;

    int decorationsFlag = cursorType & ~(0xFF);
    int baseCursorType  = cursorType & 0xFF;

    // provo a cercarlo in cursorInfo[]
    int i;
    for (i = 0; cursorInfo[i].pixmapFilename; i++)
      if (baseCursorType == cursorInfo[i].cursorType) {
        QString path =
            QString(":Resources/") + cursorInfo[i].pixmapFilename + ".png";
        CursorData data;
        data.pixmap = QPixmap(path);
        if (decorationsFlag != 0) doDecoration(data.pixmap, decorationsFlag);
        data.x = cursorInfo[i].x;
        data.y = cursorInfo[i].y;
        it     = m_cursors.insert(std::make_pair(cursorType, data)).first;
        return it->second;
      }
    // niente da fare. uso un default
    CursorData data;
    static const QPixmap standardCursorPixmap("cursors/hook.png");
    data.pixmap = standardCursorPixmap;
    data.x = data.y = 0;
    it              = m_cursors.insert(std::make_pair(cursorType, data)).first;
    return it->second;
  }

  QCursor getCursor(int cursorType) {
    // assert(cursorType!=0);

    QCursor cursor;
    if (cursorType == ToolCursor::CURSOR_ARROW)
      cursor = Qt::ArrowCursor;
    else if (cursorType == ToolCursor::ForbiddenCursor)
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
