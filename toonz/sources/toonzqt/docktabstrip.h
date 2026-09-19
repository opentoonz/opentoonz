#pragma once

#ifndef DOCKTABSTRIP_H
#define DOCKTABSTRIP_H

#include "tcommon.h"

#include <QColor>
#include <QTabBar>
#include <QWidget>

class QPainter;

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

//! Frame previewing the region a dragged panel would be merged into as a
//! tab. It defaults to the theme's accent color and can be overridden per
//! theme through the "frameColor" stylesheet property.
class DVAPI DockTabMergePreview final : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)

  QColor m_frameColor;

public:
  explicit DockTabMergePreview(QWidget *parent);

  QColor frameColor() const;
  void setFrameColor(const QColor &color) { m_frameColor = color; }

protected:
  void paintEvent(QPaintEvent *event) override;
};

//! Tab bar shown above docked panels that have been merged into a tab
//! group. Supports tab reordering (horizontal drag) and drag-out to float a
//! panel.
class DVAPI DockTabStrip final : public QTabBar {
  Q_OBJECT
  Q_PROPERTY(QColor insertionMarkerColor READ insertionMarkerColor WRITE
                 setInsertionMarkerColor)

  DockLayout *m_layout;
  Region *m_region;
  int m_pressIndex;
  QPoint m_pressPos;
  QPoint m_globalPressPos;
  bool m_dragOutStarted;
  bool m_reordering;
  // Insertion gap while reordering: 0 .. count() (gap after last tab).
  // -1 means no drop target / marker hidden.
  int m_insertionGapIndex;
  QColor m_insertionMarkerColor;

  bool isOutsideTabStrip(const QPoint &globalPos) const;
  void tryBeginDragOut(const QPoint &globalPos);
  int insertionGapAt(const QPoint &pos) const;
  void setInsertionGapIndex(int gap);
  void clearInsertionMarker();
  void paintReorderInsertionMarker(QPainter &painter) const;
  void commitTabReorder();

public:
  static const int kHeight;
  static const int kUndockDragThreshold;

  DockTabStrip(DockLayout *layout, Region *region, QWidget *parent);
  void syncFromRegion();
  void rebindRegion(Region *region) { m_region = region; }

  QColor insertionMarkerColor() const;
  void setInsertionMarkerColor(const QColor &color) {
    m_insertionMarkerColor = color;
  }

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
