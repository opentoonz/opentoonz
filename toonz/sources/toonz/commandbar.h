#pragma once

#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <memory>

#include "saveloadqsettings.h"
#include "toonz/txsheet.h"
#include "toonzqt/keyframenavigator.h"

#include <QList>
#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class QAction;
class QMenu;
class QResizeEvent;
class QToolButton;

//=============================================================================
// CommandBar
//-----------------------------------------------------------------------------

class CommandBar : public QToolBar, public SaveLoadQSettings {
  Q_OBJECT
protected:
  bool m_isCollapsible;
  bool m_isXsheetToolbar;
  bool m_roomStateLoaded;
  bool m_initialFloatingSizeApplied;
  bool m_userCompact;
  bool m_autoCompact;
  bool m_compactPresentation;
  bool m_compactTransition;
  bool m_compactUpdatePending;
  int m_compactThreshold;
  int m_expandedLongSide;
  QList<QAction *> m_compactActions;
  QMenu *m_compactMenu;
  QToolButton *m_compactButton;
  QAction *m_compactWidgetAction;

public:
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags(),
             bool isCollapsible = false, bool isXsheetToolbar = false);

  void save(QSettings &settings) const override;
  void load(QSettings &settings) override;

signals:
  void updateVisibility();

protected:
  static void fillToolbar(CommandBar *toolbar, bool isXsheetToolbar = false);
  static void buildDefaultToolbar(CommandBar *toolbar);
  void contextMenuEvent(QContextMenuEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void applyInitialFloatingSize();
  void applyCompactPanelSize(bool compact);
  void showContextMenu(const QPoint &globalPos);
  void scheduleCompactUpdate();
  void updateCompactState();
  void setCompactPresentation(bool compact);
  void rebuildCompactMenu();
  void setUserCompact(bool compact);
  int compactThreshold() const;

protected slots:
  void doCustomizeCommandBar();
  void onOrientationChanged(Qt::Orientation orientation);
};

#endif  // COMMANDBAR_H
