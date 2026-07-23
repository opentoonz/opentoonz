#pragma once

#ifndef DOCKTABSTRIP_H
#define DOCKTABSTRIP_H

#include "tcommon.h"

#include <QTabBar>
#include <QWidget>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DockLayout;
class Region;
class DockWidget;

//! Frame drawn around a panel targeted by hover-join (theme accent color).
class DVAPI DockJoinHighlight final : public QWidget {
public:
  explicit DockJoinHighlight(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *event) override;
};

//! Tab bar shown above docked panels that have been merged via hover-join.
//! Supports tab reordering (horizontal drag) and drag-out to float a panel.
class DVAPI DockTabStrip final : public QTabBar {
  Q_OBJECT

  DockLayout *m_layout;
  Region *m_region;
  int m_pressIndex;
  QPoint m_pressPos;
  QPoint m_globalPressPos;
  bool m_dragOutStarted;
  bool m_reordering;
  // Insertion gap while reordering: 0 .. count() (gap after last tab).
  // -1 means no drop target / indicator hidden.
  int m_dropGapIndex;
  // Whether the active theme's own stylesheet fails to visually
  // distinguish a selected tab's text from an unselected one (measured at
  // runtime, see updateTabTextColors()), and if so, the tab's own sampled
  // background tone used to wash its text down a bit.
  bool m_dimInactiveTabs;
  QColor m_dimWashColor;

  bool isOutsideTabStrip(const QPoint &globalPos) const;
  void tryBeginDragOut(const QPoint &globalPos);
  void updateTabTextColors();
  int dropGapAt(const QPoint &pos) const;
  void setDropGapIndex(int gap);
  void clearDropIndicator();
  void commitTabReorder();

public:
  static const int kHeight;
  static const int kUndockDragThreshold;

  DockTabStrip(DockLayout *layout, Region *region, QWidget *parent);
  void syncFromRegion();
  void rebindRegion(Region *region) { m_region = region; }

public slots:
  void onCurrentChanged(int index);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  QSize tabSizeHint(int index) const override;
};

#endif  // DOCKTABSTRIP_H
