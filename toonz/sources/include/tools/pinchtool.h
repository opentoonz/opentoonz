#pragma once

#ifndef PINCHTOOL_H
#define PINCHTOOL_H

#include "tools/tool.h"
#include "tproperty.h"
#include "toonz/observer.h"
#include "ext/ContextStatus.h"
#include "ext/Selector.h"

// For Qt translation support
#include <QCoreApplication>

class TUndo;

namespace ToonzExt {
class StrokeDeformation;
}

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//
// PinchTool (private data)
//
//=============================================================================

class DVAPI PinchTool : public TTool {
  Q_DECLARE_TR_FUNCTIONS(PinchTool)

  TMouseEvent m_lastMouseEvent;
  ToonzExt::StrokeDeformation *m_deformation;
  ToonzExt::ContextStatus m_status;
  ToonzExt::Selector m_selector;
  TUndo *m_undo;
  bool m_draw, m_active, m_cursorEnabled;
  TThickPoint m_cursor;
  TPointD m_prev, m_curr, m_down;
  unsigned int m_n;

  bool m_showSelector;

  TDoubleProperty m_toolRange;
  TDoubleProperty m_toolCornerSize;
  TBoolProperty m_autoOrManual;
  TPropertyGroup m_prop;

  void updateInterfaceStatus(const TMouseEvent &);
  void updateStrokeStatus(TStroke *stroke, double w);
  int updateCursor() const;
  TStroke *getClosestStroke(const TPointD &pos, double &w) const;

public:
  PinchTool();
  virtual ~PinchTool();

  ToolType getToolType() const { return TTool::LevelWriteTool; }

  void setShowSelector(bool show) { m_showSelector = show; }

  void onEnter();
  void onLeave();

  void updateTranslation();

  void draw();

  void leftButtonDown(const TPointD &pos, const TMouseEvent &);

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e);

  void invalidateCursorArea();

  void mouseMove(const TPointD &pos, const TMouseEvent &e);

  bool moveCursor(const TPointD &pos);

  bool keyDown(QKeyEvent *) override;

  void onActivate();
  void onDeactivate();

  // viene usato??
  void update(const TGlobalChange &);

  void onImageChanged();

  int getCursorId() const { return updateCursor(); }
  TPropertyGroup *getProperties(int targetType) { return &m_prop; }
};

#endif  // PINCHTOOL_H
