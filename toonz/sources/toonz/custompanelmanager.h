#pragma once
#ifndef CUSTOM_PANEL_MANAGER_H
#define CUSTOM_PANEL_MANAGER_H

// ToonzQt
#include "toonzqt/menubarcommand.h"

#include <QString>
#include <QWidget>

class TPanel;

//=============================================================================
// original widgets for the custom panel
class MyScroller : public QWidget {
  Q_OBJECT
  Qt::Orientation m_orientation;
  QAction* m_actions[2];
  int m_anchorPos;
  QColor m_scrollerBorderColor;
  QColor m_scrollerBGColor;

  Q_PROPERTY(QColor ScrollerBorderColor READ getScrollerBorderColor WRITE
                 setScrollerBorderColor);
  Q_PROPERTY(QColor ScrollerBGColor READ getScrollerBGColor WRITE
                 setScrollerBGColor);

public:
  MyScroller(Qt::Orientation orientation, CommandId command1,
             CommandId command2, QWidget* parent = nullptr);

  void setScrollerBorderColor(const QColor& color) {
    m_scrollerBorderColor = color;
  }
  QColor getScrollerBorderColor() const { return m_scrollerBorderColor; }
  void setScrollerBGColor(const QColor& color) { m_scrollerBGColor = color; }
  QColor getScrollerBGColor() const { return m_scrollerBGColor; }

protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
};

//=============================================================================

class CustomPanelManager {  // singleton
  CustomPanelManager(){};

public:
  static CustomPanelManager* instance();
  void loadCustomPanelEntries();

  TPanel* createCustomPanel(const QString panelName, QWidget* parent);

  void initializeControl(QWidget* customWidget);
};

#endif
