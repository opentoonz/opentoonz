#pragma once

#ifndef FUNCTIONTOOLBAR_INCLUDED
#define FUNCTIONTOOLBAR_INCLUDED

// TnzCore includes
#include "tdoubleparam.h"

// TnzQt includes
#include "toonzqt/lineedit.h"

// Qt includes
#include <QToolBar>
#include <QAction>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//============================================================

//    Forward declarations

class TDoubleParam;
class TFrameHandle;

namespace DVGui {
class MeasuredDoubleLineEdit;
}

class FrameNavigator;
class FunctionKeyframeNavigator;
class FunctionSelection;

//============================================================

namespace DVGui {
class ToolBar : public QToolBar {
public:
  ToolBar(QWidget *parent = 0) : QToolBar(parent) {
    setFixedHeight(22);          // ;_;
    setIconSize(QSize(23, 17));  //
  }
};
}

//*************************************************************************
//    FunctionToolbar  declaration
//*************************************************************************

class FunctionToolbar final : public DVGui::ToolBar, public TParamObserver {
  Q_OBJECT

  QToolBar *m_valueToolbar, *m_keyframeToolbar;

  DVGui::MeasuredDoubleLineEdit *m_valueFld;
  FrameNavigator *m_frameNavigator;
  FunctionKeyframeNavigator *m_keyframeNavigator;
  DVGui::LineEdit *m_stepFld;

  TDoubleParam *m_curve;
  TFrameHandle *m_frameHandle;

  FunctionSelection *m_selection;

  // in una toolbar ogni widget ha un'action associata
  QAction
      *m_valueFldAction;  // brutto: da eliminare. serve solo per fare show/hide
  QAction *m_keyframeNavigatorAction;  // brutto: da eliminare. serve solo per
                                       // fare show/hide

public:
  FunctionToolbar(QWidget *parent = 0);
  ~FunctionToolbar();

  void setSelection(FunctionSelection *);
  void setFrameHandle(TFrameHandle *frameHandle);

  void onChange(const TParamChange &) override;

  bool anyWidgetHasFocus();

signals:

  void numericalColumnToggled();

public slots:

  void setCurve(TDoubleParam *curve);
  void setFrame(double frame);

  void onNextKeyframe(QWidget *panel);
  void onPrevKeyframe(QWidget *panel);

private slots:

  void onValueFieldChanged();
  void onFrameSwitched();
  void onNavFrameSwitched();
  void onSelectionChanged();
};

#endif  // FUNCTIONTOOLBAR_INCLUDED
