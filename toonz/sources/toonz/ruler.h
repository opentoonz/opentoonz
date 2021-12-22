#pragma once

#ifndef RULER_INCLUDED
#define RULER_INCLUDED

#include <QWidget>
#include "toonz/sceneproperties.h"

// forward declaration
class SceneViewer;

//=============================================================================
// Ruler
//-----------------------------------------------------------------------------

/*! La classe si occupa della visualizzazione e della gestione
    di una linea guida (puo' essere orizzontale o verticale)
*/
class Ruler final : public QWidget {
  Q_OBJECT

  QColor m_parentBgColor;
  Q_PROPERTY(QColor ParentBGColor READ getParentBGColor WRITE setParentBGColor)

  QColor m_scaleColor;
  Q_PROPERTY(QColor ScaleColor READ getScaleColor WRITE setScaleColor)

  QColor m_borderColor;
  Q_PROPERTY(QColor BorderColor READ getBorderColor WRITE setBorderColor)

  QColor m_handleColor;
  Q_PROPERTY(QColor HandleColor READ getHandleColor WRITE setHandleColor)

  QColor m_handleDragColor;
  Q_PROPERTY(
      QColor HandleDragColor READ getHandleDragColor WRITE setHandleDragColor)

  void setParentBGColor(const QColor &color) { m_parentBgColor = color; }
  QColor getParentBGColor() const { return m_parentBgColor; }

  void setScaleColor(const QColor &color) { m_scaleColor = color; }
  QColor getScaleColor() const { return m_scaleColor; }

  void setBorderColor(const QColor &color) { m_borderColor = color; }
  QColor getBorderColor() const { return m_borderColor; }

  void setHandleColor(const QColor &color) { m_handleColor = color; }
  QColor getHandleColor() const { return m_handleColor; }

  void setHandleDragColor(const QColor &color) { m_handleDragColor = color; }
  QColor getHandleDragColor() const { return m_handleDragColor; }

  SceneViewer *m_viewer;
  bool m_vertical;
  bool m_moving;
  bool m_hiding;

  typedef TSceneProperties::Guides Guides;

public:
  Ruler(QWidget *parent, SceneViewer *viewer, bool vertical);
  Guides &getGuides() const;

  int getGuideCount() const;
  double getGuide(int index) const;

  double getUnit() const;
  void getIndices(double origin, double iunit, int size, int &i0, int &i1,
                  int &ic) const;

  double getZoomScale() const;
  double getPan() const;

  void drawVertical(QPainter &);
  void drawHorizontal(QPainter &);
  void paintEvent(QPaintEvent *) override;

  double posToValue(const QPoint &p) const;

  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif
