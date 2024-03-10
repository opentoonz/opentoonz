#pragma once

#ifndef XSHBREADCRUMBS_H
#define XSHBREADCRUMBS_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

#include "toonz/txsheet.h"

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QAction;

//-----------------------------------------------------------------------------

class BreadcrumbClickableLabel : public QLabel {
  Q_OBJECT

public:
  BreadcrumbClickableLabel(QString labelName, QWidget *parent = Q_NULLPTR,
                           Qt::WindowFlags f = Qt::WindowFlags());
  ~BreadcrumbClickableLabel();

signals:
  void clicked();

protected:
  void mousePressEvent(QMouseEvent *event);
};

//-----------------------------------------------------------------------------

namespace XsheetGUI {

enum CrumbType { SEPARATOR = 0, CURRENT, ANCESTOR, CHILD };
enum CrumbWidgetType { LABEL = 0, BUTTON, COMBOBOX };

class Breadcrumb : public QWidget {
  Q_OBJECT

  QWidget *m_crumbWidget;
  CrumbType m_crumbType;
  int m_col;
  std::vector<int> m_colList;
  int m_distanceFromCurrent;

public:
  Breadcrumb(CrumbType crumbType, QString crumbName,
             CrumbWidgetType crumbWidgetType, QWidget *parent);
  ~Breadcrumb() {}

  void setColumnNumber(int col) { m_col = col; }
  void setColumnNumberList(std::vector<int> colList) { m_colList = colList; }
  void setDistanceFromCurrent(int distance) {
    m_distanceFromCurrent = distance;
  }

  QWidget *getCrumbWidget() { return m_crumbWidget; }

public slots:
  void onButtonClicked();
  void onComboBoxIndexChanged(int);
};

//=============================================================================
// BreadcrumbArea
//-----------------------------------------------------------------------------

class BreadcrumbArea final : public QFrame {
  Q_OBJECT

  XsheetViewer *m_viewer;
  std::vector<Breadcrumb *> m_breadcrumbWidgets;

  QHBoxLayout *m_breadcrumbLayout;

public:
  BreadcrumbArea(XsheetViewer *parent  = 0,
                 Qt::WindowFlags flags = Qt::WindowFlags());
  static void toggleBreadcrumbArea();
  void showBreadcrumbs(bool show);

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

public slots:
  void updateBreadcrumbs();
};

}  // namespace XsheetGUI

#endif  // XSHBREADCRUMBS_H
