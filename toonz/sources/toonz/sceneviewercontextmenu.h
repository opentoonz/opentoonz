#pragma once

#ifndef SCENE_VIEWER_CONTEXT_MENU
#define SCENE_VIEWER_CONTEXT_MENU

#include <QMenu>
#include "tgeometry.h"

class TStageObjectId;
class SceneViewer;
class TXshColumn;

class SceneViewerContextMenu final : public QMenu {
  Q_OBJECT
  SceneViewer *m_viewer;
  int m_groupIndexToBeEntered;

  // Helper methods for adding menu commands
  void addShowHideCommand(QMenu *menu, TXshColumn *column);
  void addSelectCommand(QMenu *menu, const TStageObjectId &id);

public:
  SceneViewerContextMenu(SceneViewer *parent);
  ~SceneViewerContextMenu();

  // Add commands for entering/exiting vector image groups
  void addEnterGroupCommands(const TPointD &pos);

  // Add commands for level operations (show/hide, select)
  void addLevelCommands(std::vector<int> &indices);

public slots:
  // Vector image group operations (used by CommandManager actions)
  void enterVectorImageGroup();
  void exitVectorImageGroup();
};

// Helper GUI class for toggling zero thickness lines visibility
namespace ZeroThickToggleGui {
void addZeroThickCommand(QMenu *menu);

class ZeroThickToggleHandler : public QObject {
  Q_OBJECT

public slots:
  void activate();
  void deactivate();
};
}  // namespace ZeroThickToggleGui

// Helper GUI class for toggling cursor outline visibility
namespace CursorOutlineToggleGui {
void addCursorOutlineCommand(QMenu *menu);

class CursorOutlineToggleHandler : public QObject {
  Q_OBJECT

public slots:
  void activate();
  void deactivate();
};
}  // namespace CursorOutlineToggleGui

// Helper GUI class for toggling viewer indicators visibility
namespace ViewerIndicatorToggleGui {
void addViewerIndicatorCommand(QMenu *menu);

class ViewerIndicatorToggleHandler : public QObject {
  Q_OBJECT

public slots:
  void activate();
  void deactivate();
};
}  // namespace ViewerIndicatorToggleGui

#endif
