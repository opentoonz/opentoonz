#pragma once

#ifndef FUNCTION_SEGMENT_VIEWER_H
#define FUNCTION_SEGMENT_VIEWER_H

#include <QLabel>
#include <QComboBox>

#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include "toonzqt/lineedit.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class FunctionSegmentPage;
class FunctionSheet;
class TXsheetHandle;
class KeyframeSetter;
class FunctionPanel;
class QPushButton;
class QStackedWidget;
namespace DVGui {
class MeasuredDoubleLineEdit;
class ExpressionField;
class FileField;
}

//-----------------------------------------------------------------------------

class FunctionSegmentViewer final : public QFrame, public TParamObserver {
  Q_OBJECT

  TDoubleParam *m_curve;
  int m_segmentIndex;
  int m_r0, m_r1;
  QWidget *m_topbar;

  QLineEdit *m_fromFld;
  QLineEdit *m_toFld;
  QLabel *m_paramNameLabel;

  QComboBox *m_typeCombo;
  DVGui::LineEdit *m_stepFld;
  QStackedWidget *m_parametersPanel;

  FunctionSegmentPage *m_pages[9];
  int m_typeId[9];

  FunctionSheet *m_sheet;
  TXsheetHandle *m_xshHandle;
  FunctionPanel *m_panel;

  // buttons for move segments
  QPushButton *m_prevCurveButton;
  QPushButton *m_nextCurveButton;
  QPushButton *m_prevLinkButton;
  QPushButton *m_nextLinkButton;

public:
  FunctionSegmentViewer(QWidget *parent, FunctionSheet *sheet = 0,
                        FunctionPanel *panel = 0);
  ~FunctionSegmentViewer();

  TDoubleParam *getCurve() const { return m_curve; }
  int getR0() const { return m_r0; }
  int getR1() const { return m_r1; }
  int getSegmentIndex() const { return m_segmentIndex; }

  void refresh();

  // overriden from TDoubleParamObserver
  void onChange(const TParamChange &) override { refresh(); }

  void setXsheetHandle(TXsheetHandle *xshHandle) { m_xshHandle = xshHandle; }

  bool anyWidgetHasFocus();
  // in order to avoid FunctionViewer to get focus while editing the expression
  bool isExpressionPageActive();

private:
  int typeToIndex(int type) const;
  int indexToType(int typeIndex) const { return m_typeId[typeIndex]; }
  bool segmentIsValid() const;

  // for displaying the types of neighbor segments
  QString typeToString(int type) const;

private slots:
  void onSegmentTypeChanged(int type);
  void onCurveChanged();
  void onStepFieldChanged(const QString &);

  void onApplyButtonPressed();
  void onPrevCurveButtonPressed();
  void onNextCurveButtonPressed();
  void onPrevLinkButtonPressed();
  void onNextLinkButtonPressed();

public slots:
  void setSegment(TDoubleParam *curve, int segmentIndex);
  void setSegmentByFrame(TDoubleParam *curve, int frame);
};

//-----------------------------------------------------------------------------

class FunctionSegmentPage : public QWidget {
  Q_OBJECT
  FunctionSegmentViewer *m_viewer;

public:
  FunctionSegmentPage(FunctionSegmentViewer *parent);
  ~FunctionSegmentPage();

  FunctionSegmentViewer *getViewer() const { return m_viewer; }
  TDoubleParam *getCurve() const { return m_viewer->getCurve(); }

  int getR0() const { return m_viewer->getR0(); }
  int getR1() const { return m_viewer->getR1(); }

  virtual void refresh() = 0;
  virtual void apply()   = 0;

  virtual void init(int segmentLength) = 0;

public slots:
  void onFieldChanged() { apply(); }
};

//-----------------------------------------------------------------------------

class SpeedInOutSegmentPage final : public FunctionSegmentPage {
  Q_OBJECT

  DVGui::LineEdit *m_speed0xFld;
  DVGui::MeasuredDoubleLineEdit *m_speed0yFld;
  DVGui::LineEdit *m_speed1xFld;
  DVGui::MeasuredDoubleLineEdit *m_speed1yFld;

  DVGui::MeasuredDoubleLineEdit *m_firstSpeedFld;
  DVGui::MeasuredDoubleLineEdit *m_lastSpeedFld;

public:
  SpeedInOutSegmentPage(FunctionSegmentViewer *parent = 0);
  void refresh() override;
  void apply() override{};

  void getGuiValues(TPointD &speedIn, TPointD &speedOut);
  void init(int segmentLength) override;

public slots:
  void onFirstHandleXChanged();
  void onFirstHandleYChanged();
  void onLastHandleXChanged();
  void onLastHandleYChanged();
  void onFirstSpeedChanged();
  void onLastSpeedChanged();
};

//-----------------------------------------------------------------------------

class EaseInOutSegmentPage final : public FunctionSegmentPage {
  Q_OBJECT
  DVGui::MeasuredDoubleLineEdit *m_ease0Fld, *m_ease1Fld;
  double m_fieldScale;

  bool m_isPercentage;

public:
  EaseInOutSegmentPage(bool percentage, FunctionSegmentViewer *parent = 0);
  void refresh() override;
  void apply() override {}

  void getGuiValues(TPointD &easeIn, TPointD &easeOut);
  void init(int segmentLength) override;

public slots:
  void onEase0Changed();
  void onEase1Changed();
};

//-----------------------------------------------------------------------------

class FunctionExpressionSegmentPage final : public FunctionSegmentPage {
  Q_OBJECT

  DVGui::ExpressionField *m_expressionFld;
  DVGui::LineEdit *m_unitFld;

public:
  FunctionExpressionSegmentPage(FunctionSegmentViewer *parent = 0);
  void refresh() override;
  void apply() override;

  // return false if a circular reference is occured
  bool getGuiValues(std::string &expressionText, std::string &unitName);

  void init(int segmentLength) override;
};

//-----------------------------------------------------------------------------

class SimilarShapeSegmentPage final : public FunctionSegmentPage {
  Q_OBJECT

  DVGui::ExpressionField *m_expressionFld;
  DVGui::LineEdit *m_offsetFld;

public:
  SimilarShapeSegmentPage(FunctionSegmentViewer *parent = 0);

  void refresh() override;
  void apply() override;

  void init(int segmentLength) override;

  void getGuiValues(std::string &expressionText, double &similarShapeOffset);
};

//-----------------------------------------------------------------------------

class FileSegmentPage final : public FunctionSegmentPage {
  Q_OBJECT

  DVGui::FileField *m_fileFld;
  DVGui::LineEdit *m_fieldIndexFld;
  DVGui::LineEdit *m_measureFld;

public:
  FileSegmentPage(FunctionSegmentViewer *parent = 0);
  void refresh() override;
  void init(int segmentLength) override;
  void apply() override;
  void getGuiValues(TDoubleKeyframe::FileParams &fileParam,
                    std::string &unitName);
};

#endif
