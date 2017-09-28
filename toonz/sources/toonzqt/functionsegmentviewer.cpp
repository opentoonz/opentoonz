

#include "toonzqt/functionsegmentviewer.h"

// TnzQt includes
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/expressionfield.h"
#include "toonzqt/dvdialog.h"
#include "tw/stringtable.h"
#include "toonzqt/functionsheet.h"
#include "toonzqt/functionpanel.h"

// TnzLib includes
#include "toonz/doubleparamcmd.h"
#include "toonz/txsheetexpr.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"

// TnzBase includes
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include "texpression.h"
#include "tunit.h"

// TnzCore includes
#include "tconvert.h"

// Qt includes
#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QIntValidator>
#include <QTextEdit>

using namespace DVGui;

//-----------------------------------------------------------------------------

static LineEdit *createField() {
  LineEdit *field = new LineEdit();
  /*
field->setMaximumWidth(100);
field->setFixedHeight(100);
field->setMinimumHeight(100);
*/
  return field;
}

//=============================================================================

FunctionSegmentPage::FunctionSegmentPage(FunctionSegmentViewer *parent)
    : QWidget(parent), m_viewer(parent) {}

FunctionSegmentPage::~FunctionSegmentPage() {}

//=============================================================================

class FunctionEmptySegmentPage final : public FunctionSegmentPage {
public:
  FunctionEmptySegmentPage(FunctionSegmentViewer *parent = 0)
      : FunctionSegmentPage(parent) {}

  void refresh() override {}
  void apply() override {}
  void init(int segmentLength) override {}
};

//=============================================================================

SpeedInOutSegmentPage::SpeedInOutSegmentPage(FunctionSegmentViewer *parent)
    : FunctionSegmentPage(parent) {
  m_speed0xFld = new LineEdit("0");
  m_speed0yFld = new DVGui::MeasuredDoubleLineEdit();
  m_speed1xFld = new LineEdit("0");
  m_speed1yFld = new DVGui::MeasuredDoubleLineEdit();

  m_firstSpeedFld = new DVGui::MeasuredDoubleLineEdit();
  m_lastSpeedFld  = new DVGui::MeasuredDoubleLineEdit();

  //----layout

  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setHorizontalSpacing(5);
  mainLayout->setVerticalSpacing(5);
  mainLayout->setMargin(2);
  {
    mainLayout->addWidget(new QLabel(tr("First Speed:")), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_firstSpeedFld, 0, 1, 1, 2);

    mainLayout->addWidget(new QLabel(tr("Handle:")), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_speed0yFld, 1, 1);
    mainLayout->addWidget(new QLabel(tr("/")), 1, 2);
    mainLayout->addWidget(m_speed0xFld, 1, 3);

    mainLayout->addWidget(new QLabel(tr("Last Speed:")), 2, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_lastSpeedFld, 2, 1, 1, 2);

    mainLayout->addWidget(new QLabel(tr("Handle:")), 3, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_speed1yFld, 3, 1);
    mainLayout->addWidget(new QLabel(tr("/")), 3, 2);
    mainLayout->addWidget(m_speed1xFld, 3, 3);
  }
  mainLayout->setColumnStretch(0, 0);
  mainLayout->setColumnStretch(1, 1);
  mainLayout->setColumnStretch(2, 0);
  mainLayout->setColumnStretch(3, 1);
  setLayout(mainLayout);

  bool ret = connect(m_speed0xFld, SIGNAL(editingFinished()), this,
                     SLOT(onFirstHandleXChanged()));
  ret = ret && connect(m_speed0yFld, SIGNAL(editingFinished()), this,
                       SLOT(onFirstHandleYChanged()));
  ret = ret && connect(m_firstSpeedFld, SIGNAL(editingFinished()), this,
                       SLOT(onFirstSpeedChanged()));

  ret = ret && connect(m_speed1xFld, SIGNAL(editingFinished()), this,
                       SLOT(onLastHandleXChanged()));
  ret = ret && connect(m_speed1yFld, SIGNAL(editingFinished()), this,
                       SLOT(onLastHandleYChanged()));
  ret = ret && connect(m_lastSpeedFld, SIGNAL(editingFinished()), this,
                       SLOT(onLastSpeedChanged()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::onFirstHandleXChanged() {
  double x = m_speed0xFld->text().toDouble();

  /*--- 前のSegmentが存在して、Linkしていて、SpeedIn/Outでない場合、
            Speedを保持してハンドルの長さを変える。---*/
  int segmentIndex = getViewer()->getSegmentIndex();
  if (segmentIndex > 0) /*--- 前のSegmentが存在している条件 ---*/
  {
    TDoubleKeyframe kf = getCurve()->getKeyframe(segmentIndex);
    /*--- Linkしていて、SpeedIn/Outでない場合の条件 ---*/
    if (kf.m_linkedHandles && kf.m_prevType != TDoubleKeyframe::SpeedInOut) {
      double speed = m_firstSpeedFld->getValue();
      m_speed0yFld->setValue(x * speed);
      return;
    }
  }
  /*--- 条件から外れる場合 ---*/
  double y = m_speed0yFld->getValue();

  if (x != 0.0)
    m_firstSpeedFld->setValue(y / x);
  else
    m_firstSpeedFld->setText(tr("---"));
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::onFirstHandleYChanged() {
  double y = m_speed0yFld->getValue();

  /*--- 前のSegmentが存在して、Linkしていて、SpeedIn/Outでない場合、
  Speedを保持してハンドルの長さを変える。---*/
  int segmentIndex = getViewer()->getSegmentIndex();
  if (segmentIndex > 0) /*--- 前のSegmentが存在している条件 ---*/
  {
    TDoubleKeyframe kf = getCurve()->getKeyframe(segmentIndex);
    /*--- Linkしていて、SpeedIn/Outでない場合の条件 ---*/
    if (kf.m_linkedHandles && kf.m_prevType != TDoubleKeyframe::SpeedInOut) {
      double speed = m_firstSpeedFld->getValue();
      if (!areAlmostEqual(speed, 0.0, 0.001))
        m_speed0xFld->setText(QString::number(y / speed, 'f', 1));
      else
        m_speed0xFld->setText(QString::number(0, 'f', 1));

      return;
    }
  }
  /*--- 条件から外れる場合 ---*/
  double x = m_speed0xFld->text().toDouble();

  if (x != 0.0)
    m_firstSpeedFld->setValue(y / x);
  else
    m_firstSpeedFld->setText(tr("---"));
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::onFirstSpeedChanged() {
  /*--- Speedを変えた場合は、Valueを変える。 ---*/
  double speed = m_firstSpeedFld->getValue();
  double x     = m_speed0xFld->text().toDouble();
  m_speed0yFld->setValue(speed * x);
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::onLastHandleXChanged() {
  double x = m_speed1xFld->text().toDouble();

  /*--- 後のSegmentが存在して、Linkしていて、SpeedIn/Outでない場合、
           Speedを保持してハンドルの長さを変える。 ---*/
  int segmentIndex    = getViewer()->getSegmentIndex();
  TDoubleParam *curve = getCurve();
  if (curve && curve->getKeyframeCount() >= 3 &&
      segmentIndex < curve->getKeyframeCount() -
                         2) /*--- 後のSegmentが存在している条件 ---*/
  {
    TDoubleKeyframe kf = getCurve()->getKeyframe(segmentIndex + 1);
    /*--- Linkしていて、SpeedIn/Outでない場合の条件 ---*/
    if (kf.m_linkedHandles && kf.m_type != TDoubleKeyframe::SpeedInOut) {
      double speed = m_lastSpeedFld->getValue();
      m_speed1yFld->setValue(x * speed);
      return;
    }
  }
  /*--- 条件から外れる場合 ---*/
  double y = m_speed1yFld->getValue();
  if (x != 0.0)
    m_lastSpeedFld->setValue(y / x);
  else
    m_lastSpeedFld->setText(tr("---"));
}

//-----------------------------------------------------------------------------
void SpeedInOutSegmentPage::onLastHandleYChanged() {
  double y = m_speed1yFld->getValue();

  /*--- 後のSegmentが存在して、Linkしていて、SpeedIn/Outでない場合、
  Speedを保持してハンドルの長さを変える。 ---*/
  int segmentIndex    = getViewer()->getSegmentIndex();
  TDoubleParam *curve = getCurve();
  if (curve && curve->getKeyframeCount() >= 3 &&
      segmentIndex < curve->getKeyframeCount() -
                         2) /*--- 後のSegmentが存在している条件 ---*/
  {
    TDoubleKeyframe kf = getCurve()->getKeyframe(segmentIndex + 1);
    /*--- Linkしていて、SpeedIn/Outでない場合の条件 ---*/
    if (kf.m_linkedHandles && kf.m_type != TDoubleKeyframe::SpeedInOut) {
      double speed = m_lastSpeedFld->getValue();
      std::cout << "speed: " << speed << std::endl;
      if (!areAlmostEqual(speed, 0.0, 0.001))
        m_speed1xFld->setText(QString::number(y / speed, 'f', 1));
      else
        m_speed1xFld->setText(QString::number(0, 'f', 1));
      return;
    }
  }
  /*--- 条件から外れる場合 ---*/
  double x = m_speed1xFld->text().toDouble();
  if (x != 0.0)
    m_lastSpeedFld->setValue(y / x);
  else
    m_lastSpeedFld->setText(tr("---"));
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::onLastSpeedChanged() {
  /*--- Speedを変えた場合は、Valueを変える。 ---*/
  double speed = m_lastSpeedFld->getValue();
  double x     = m_speed1xFld->text().toDouble();
  m_speed1yFld->setValue(speed * x);
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::refresh() {
  TDoubleParam *curve = getCurve();
  int kIndex          = getViewer()->getSegmentIndex();
  if (!curve || kIndex < 0 || kIndex + 1 >= curve->getKeyframeCount()) return;
  if (curve->getKeyframe(kIndex).m_type != TDoubleKeyframe::SpeedInOut) return;

  std::string measureName = curve->getMeasureName();
  if (measureName == "zdepth")
    measureName = "zdepth.handle";
  else if (measureName == "zdepth.cam")
    measureName = "zdepth.cam.handle";

  TPointD speedOut = curve->getSpeedOut(kIndex);
  m_speed0xFld->setText(QString::number(speedOut.x, 'f', 1));
  m_speed0yFld->setMeasure(measureName);
  m_speed0yFld->setValue(speedOut.y);

  m_firstSpeedFld->setMeasure(measureName);
  if (speedOut.x != 0.0)
    m_firstSpeedFld->setValue(speedOut.y / speedOut.x);
  else
    m_firstSpeedFld->setText(tr("---"));

  TPointD speedIn = curve->getSpeedIn(kIndex + 1);
  m_speed1xFld->setText(QString::number(speedIn.x, 'f', 1));
  m_speed1yFld->setMeasure(measureName);
  m_speed1yFld->setValue(speedIn.y);
  m_lastSpeedFld->setMeasure(measureName);
  if (speedIn.x != 0.0)
    m_lastSpeedFld->setValue(speedIn.y / speedIn.x);
  else
    m_lastSpeedFld->setText(tr("---"));

  /*--- キーフレームがリンク、かつ隣がSpeedIn/Outでないとき、
          Speed入力BoxをDisableする。それ以外の場合はEnableする ---*/
  // PrevKey
  if (kIndex > 0 && curve->getKeyframe(kIndex).m_linkedHandles &&
      curve->getKeyframe(kIndex).m_prevType != TDoubleKeyframe::SpeedInOut)
    m_firstSpeedFld->setEnabled(false);
  else
    m_firstSpeedFld->setEnabled(true);

  // NextKey
  if (curve->getKeyframeCount() >= 3 &&
      kIndex < curve->getKeyframeCount() - 2 &&
      curve->getKeyframe(kIndex + 1).m_linkedHandles &&
      curve->getKeyframe(kIndex + 1).m_type != TDoubleKeyframe::SpeedInOut)
    m_lastSpeedFld->setEnabled(false);
  else
    m_lastSpeedFld->setEnabled(true);
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::init(int segmentLength) {
  TDoubleParam *curve = getCurve();
  if (!curve) return;

  m_speed0xFld->setText(QString::number((double)segmentLength / 3.0));
  m_speed0yFld->setMeasure(curve->getMeasureName());
  m_speed0yFld->setValue(0);

  m_firstSpeedFld->setMeasure(curve->getMeasureName());
  m_firstSpeedFld->setValue(0);

  m_speed1xFld->setText(QString::number(-(double)segmentLength / 3.0));
  m_speed1yFld->setMeasure(curve->getMeasureName());
  m_speed1yFld->setValue(0);

  m_lastSpeedFld->setMeasure(curve->getMeasureName());
  m_lastSpeedFld->setValue(0);
}

//-----------------------------------------------------------------------------

void SpeedInOutSegmentPage::getGuiValues(TPointD &speedIn, TPointD &speedOut) {
  speedOut.x = m_speed0xFld->text().toDouble();
  speedOut.y = m_speed0yFld->getValue();

  speedIn.x = m_speed1xFld->text().toDouble();
  speedIn.y = m_speed1yFld->getValue();
}

//=============================================================================

EaseInOutSegmentPage::EaseInOutSegmentPage(bool isPercentage,
                                           FunctionSegmentViewer *parent)
    : FunctionSegmentPage(parent)
    , m_fieldScale(isPercentage ? 100.0 : 1.0)
    , m_isPercentage(isPercentage) {
  std::string measureName = isPercentage ? "percentage" : "";

  m_ease0Fld = new DVGui::MeasuredDoubleLineEdit();
  m_ease0Fld->setMeasure(measureName);

  m_ease1Fld = new DVGui::MeasuredDoubleLineEdit();
  m_ease1Fld->setMeasure(measureName);

  m_ease0Fld->setText("0");
  m_ease1Fld->setText("0");

  //----layout

  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setSpacing(5);
  mainLayout->setMargin(2);
  {
    mainLayout->addWidget(new QLabel(tr("Ease In:")), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_ease0Fld, 0, 1);
    mainLayout->addWidget(new QLabel(tr("Ease Out:")), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_ease1Fld, 1, 1);
  }
  mainLayout->setColumnStretch(0, 0);
  mainLayout->setColumnStretch(1, 1);
  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

void EaseInOutSegmentPage::refresh() {
  TDoubleParam *curve = getCurve();
  if (!curve) return;
  TDoubleKeyframe kf0 = curve->getKeyframeAt(getR0());
  TDoubleKeyframe kf1 = curve->getKeyframeAt(getR1());
  m_ease0Fld->setValue(kf0.m_speedOut.x / m_fieldScale);
  m_ease1Fld->setValue(-kf1.m_speedIn.x / m_fieldScale);
}

//-----------------------------------------------------------------------------

void EaseInOutSegmentPage::onEase0Changed() {
  TDoubleParam *curve = getCurve();
  int kIndex          = getViewer()->getSegmentIndex();
  if (!curve || kIndex < 0) return;
  KeyframeSetter setter(curve, kIndex);
  setter.setEaseOut(m_ease0Fld->getValue() * m_fieldScale);
}

//-----------------------------------------------------------------------------

void EaseInOutSegmentPage::onEase1Changed() {
  TDoubleParam *curve = getCurve();
  int kIndex          = getViewer()->getSegmentIndex();
  if (!curve || kIndex < 0) return;
  KeyframeSetter setter(curve, kIndex + 1);
  setter.setEaseIn(-m_ease1Fld->getValue() * m_fieldScale);
}

//-----------------------------------------------------------------------------

void EaseInOutSegmentPage::init(int segmentLength) {
  TDoubleParam *curve = getCurve();
  if (!curve) return;
  int kIndex = getViewer()->getSegmentIndex();

  /*---- 既にあるSegment上でTypeを切り替えたとき ----*/
  if (0 <= kIndex && kIndex < curve->getKeyframeCount() - 1) {
    TDoubleKeyframe keyFrame     = curve->getKeyframe(kIndex);
    TDoubleKeyframe nextKeyFrame = curve->getKeyframe(kIndex + 1);
    double ease0 = 0, ease1 = 0;
    /*--- EaseInOut(Frame)からEaseIO(%)に切り替えたとき ---*/
    if (keyFrame.m_type == TDoubleKeyframe::EaseInOut && m_isPercentage) {
      // absolute -> percentage
      ease0 = keyFrame.m_speedOut.x / (double)segmentLength;
      ease1 = -nextKeyFrame.m_speedIn.x / (double)segmentLength;
      ease0 = tcrop(ease0, 0.0, 1.0);
      ease1 = tcrop(ease1, 0.0, 1.0 - ease0);
    }
    //*--- EaseIO(%)からEaseInOut(Frame)に切り替えたとき ---*/
    else if (keyFrame.m_type == TDoubleKeyframe::EaseInOutPercentage &&
             !m_isPercentage) {
      // percentage -> absolute
      ease0 = keyFrame.m_speedOut.x * 0.01 * (double)segmentLength;
      ease1 = -nextKeyFrame.m_speedIn.x * 0.01 * (double)segmentLength;
      ease0 = tcrop(ease0, 0.0, (double)segmentLength);
      ease1 = tcrop(ease1, 0.0, (double)segmentLength - ease0);
    } else {
      ease1 = ease0 = ((m_isPercentage) ? 1.0 : (double)segmentLength) / 3.0;
    }

    if (!m_isPercentage) {
      ease0 = floor(ease0 + 0.5);
      ease1 = floor(ease1 + 0.5);
    }
    m_ease0Fld->setValue(ease0);
    m_ease1Fld->setValue(ease1);
  } else {
    double value = ((m_isPercentage) ? 1.0 : (double)segmentLength) / 3.0;
    if (!m_isPercentage) value = floor(value + 0.5);

    m_ease0Fld->setValue(value);
    m_ease1Fld->setValue(value);
  }
}

//-----------------------------------------------------------------------------

void EaseInOutSegmentPage::getGuiValues(TPointD &easeIn, TPointD &easeOut) {
  easeOut.x = m_ease0Fld->getValue() * m_fieldScale;
  easeOut.y = 0;

  easeIn.x = -m_ease1Fld->getValue() * m_fieldScale;
  easeIn.y = 0;
}

//=============================================================================

FunctionExpressionSegmentPage::FunctionExpressionSegmentPage(
    FunctionSegmentViewer *parent)
    : FunctionSegmentPage(parent) {
  m_expressionFld = new DVGui::ExpressionField();
  m_expressionFld->setFixedHeight(21);

  QLabel *unitLabel = new QLabel(tr("Unit:"));
  unitLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_unitFld = createField();
  m_unitFld->setFixedWidth(40);
  m_unitFld->setText("inch");

  //---- layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setSpacing(2);
  mainLayout->setMargin(2);
  {
    mainLayout->addSpacing(3);
    mainLayout->addWidget(new QLabel(tr("Expression:")));
    mainLayout->addWidget(m_expressionFld);

    mainLayout->addSpacing(3);
    QHBoxLayout *unitLay = new QHBoxLayout();
    {
      unitLay->addWidget(unitLabel);
      unitLay->addWidget(m_unitFld);
      unitLay->addStretch();
    }
    mainLayout->addLayout(unitLay);
  }

  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

void FunctionExpressionSegmentPage::refresh() {
  TDoubleParam *curve = getCurve();
  if (!curve) {
    m_expressionFld->setGrammar(0);
    return;
  }

  TDoubleKeyframe kf0        = curve->getKeyframeAt(getR0());
  std::string expression     = kf0.m_expressionText;
  bool oldBlockSignalsStatus = m_expressionFld->blockSignals(true);
  m_expressionFld->setGrammar(curve->getGrammar());
  m_expressionFld->setExpression(expression);
  m_expressionFld->blockSignals(oldBlockSignalsStatus);

  std::wstring unitName = ::to_wstring(kf0.m_unitName);
  if (unitName == L"" && curve->getMeasure())
    unitName = curve->getMeasure()->getCurrentUnit()->getDefaultExtension();

  oldBlockSignalsStatus = m_unitFld->blockSignals(true);
  m_unitFld->setText(QString::fromStdWString(unitName));
  m_unitFld->blockSignals(oldBlockSignalsStatus);
}

//-----------------------------------------------------------------------------

void FunctionExpressionSegmentPage::init(int segmentLength) {
  TDoubleParam *curve = getCurve();
  if (!curve) {
    m_expressionFld->setGrammar(0);
    m_expressionFld->setEnabled(false);
    return;
  }

  m_expressionFld->setEnabled(true);
  m_expressionFld->setGrammar(curve->getGrammar());

  int kIndex = getViewer()->getSegmentIndex();

  /*--- すでにあるカーブをExpressionに切り替えた場合 ---*/
  if (kIndex >= 0) {
    TDoubleKeyframe keyFrame      = curve->getKeyframe(kIndex);
    double value                  = curve->getValue(keyFrame.m_frame);
    const TUnit *unit             = 0;
    if (curve->getMeasure()) unit = curve->getMeasure()->getCurrentUnit();
    if (unit) value               = unit->convertTo(value);
    m_expressionFld->setExpression(QString::number(value).toStdString());
    /*--- unitがある場合だけUnitを表示 ---*/
    if (unit)
      m_unitFld->setText(QString::fromStdWString(unit->getDefaultExtension()));
    else
      m_unitFld->setText("");
  } else {
    m_expressionFld->setExpression("0");

    std::wstring unitName = L"inch";
    if (curve->getMeasure())
      unitName = curve->getMeasure()->getCurrentUnit()->getDefaultExtension();
    m_unitFld->setText(QString::fromStdWString(unitName));
  }
}

//-----------------------------------------------------------------------------

void FunctionExpressionSegmentPage::apply() {
  TDoubleParam *curve = getCurve();
  if (!curve) return;

  int kIndex = getViewer()->getSegmentIndex();
  if (kIndex < 0) return;

  std::string expressionText = m_expressionFld->getExpression();
  TExpression expr;
  expr.setGrammar(curve->getGrammar());
  expr.setText(expressionText);
  if (dependsOn(expr, curve)) {
    DVGui::warning(
        tr("There is a circular reference in the definition of the "
           "interpolation."));
    return;
  }

  std::string unitName = m_unitFld->text().toStdString();

  KeyframeSetter setter(curve, kIndex);
  setter.setExpression(m_expressionFld->getExpression());
  setter.setUnitName(unitName);

  /*

TDoubleKeyframe kf0 = curve->getKeyframeAt(getR0());
kf0.m_expressionText = m_expressionFld->getExpression();
kf0.m_unitName = m_unitFld->text().toStdString();
curve->setKeyframe(kf0);

wstring unitExtension = m_unitFld->text().toStdWString();
TMeasure *curveMeasure = curve->getMeasure();
if(curveMeasure)
{
TUnit *unit = curveMeasure->getUnit(unitExtension);
if(unit)
curveMeasure->setCurrentUnit(unit);
else
{
unitExtension = curveMeasure->getCurrentUnit()->getDefaultExtension();
m_unitFld->setText(QString::fromStdWString(unitExtension));
}
}
else
m_unitFld->setText("");
*/
}

//-----------------------------------------------------------------------------
/*! return false if a circular reference is occured
*/
bool FunctionExpressionSegmentPage::getGuiValues(std::string &expressionText,
                                                 std::string &unitName) {
  expressionText = m_expressionFld->getExpression();

  // checking a circular reference
  TDoubleParam *curve = getCurve();
  TExpression expr;
  expr.setGrammar(curve->getGrammar());
  expr.setText(expressionText);
  if (dependsOn(expr, curve)) {
    DVGui::warning(
        tr("There is a circular reference in the definition of the "
           "interpolation."));
    return false;
  }

  unitName = m_unitFld->text().toStdString();

  if (m_expressionFld->hasFocus()) m_expressionFld->clearFocus();

  return true;
}

//=============================================================================

class FileSegmentPage final : public FunctionSegmentPage {
  DVGui::FileField *m_fileFld;
  LineEdit *m_fieldIndexFld;
  LineEdit *m_measureFld;

public:
  FileSegmentPage(FunctionSegmentViewer *parent = 0)
      : FunctionSegmentPage(parent) {
    m_fileFld = new DVGui::FileField(this);
    m_fileFld->setFileMode(QFileDialog::ExistingFile);
    QStringList filters;
    filters.append("dat");
    filters.append("txt");
    m_fileFld->setFilters(filters);

    m_fieldIndexFld             = new LineEdit(this);
    QIntValidator *intValidator = new QIntValidator(1, 100, this);
    m_fieldIndexFld->setValidator(intValidator);

    m_measureFld = new LineEdit(this);
    m_measureFld->setText("inch");

    //----layout
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(5);
    mainLayout->setMargin(2);
    {
      mainLayout->addWidget(new QLabel(tr("File Path:")), 0);
      mainLayout->addWidget(m_fileFld);

      QGridLayout *bottomLay = new QGridLayout();
      bottomLay->setSpacing(5);
      bottomLay->setMargin(0);
      {
        bottomLay->addWidget(new QLabel(tr("Column:")), 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
        bottomLay->addWidget(m_fieldIndexFld, 0, 1);
        bottomLay->addWidget(new QLabel(tr("Unit:")), 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
        bottomLay->addWidget(m_measureFld, 1, 1);
      }
      bottomLay->setColumnStretch(0, 0);
      bottomLay->setColumnStretch(1, 1);
      mainLayout->addLayout(bottomLay);
    }
    setLayout(mainLayout);
  }
  void refresh() override {
    TDoubleKeyframe kf;
    TDoubleParam *curve = getCurve();
    if (curve) kf       = curve->getKeyframeAt(getR0());
    if (curve && kf.m_isKeyframe) {
      TFilePath path;
      int fieldIndex       = 0;
      std::string unitName = "";
      if (kf.m_type == TDoubleKeyframe::File) {
        path                           = kf.m_fileParams.m_path;
        fieldIndex                     = kf.m_fileParams.m_fieldIndex;
        if (fieldIndex < 0) fieldIndex = 0;
        unitName                       = kf.m_unitName;
        if (unitName == "") {
          TMeasure *measure = curve->getMeasure();
          if (measure) {
            const TUnit *unit  = measure->getCurrentUnit();
            if (unit) unitName = ::to_string(unit->getDefaultExtension());
          }
        }
      }
      m_fileFld->setPath(QString::fromStdWString(path.getWideString()));
      m_fieldIndexFld->setText(QString::number(fieldIndex + 1));
      m_measureFld->setText(QString::fromStdString(unitName));
    }
  }

  void init(int segmentLength) override {
    TDoubleParam *curve = getCurve();
    if (!curve) return;

    TMeasure *measure    = curve->getMeasure();
    std::string unitName = "";
    if (measure) {
      const TUnit *unit  = measure->getCurrentUnit();
      if (unit) unitName = ::to_string(unit->getDefaultExtension());
    }
    m_measureFld->setText(QString::fromStdString(unitName));

    m_fileFld->setPath("");
    m_fieldIndexFld->setText("");
  }

  void apply() override {
    TDoubleParam *curve = getCurve();
    if (!curve) return;
    int kIndex = getViewer()->getSegmentIndex();
    if (kIndex < 0) return;

    QString stringPath = m_fileFld->getPath();
    if (stringPath == "") return;
    stringPath.replace("\\", "\\\\");

    TDoubleKeyframe::FileParams fileParams;

    fileParams.m_path       = TFilePath(stringPath.toStdWString());
    fileParams.m_fieldIndex = qMax(0, m_fieldIndexFld->text().toInt() - 1);
    std::string unitName    = m_measureFld->text().toStdString();

    KeyframeSetter setter(curve, kIndex);
    setter.setFile(fileParams);
    setter.setUnitName(unitName);
  }

  void getGuiValues(TDoubleKeyframe::FileParams &fileParam,
                    std::string &unitName) {
    QString stringPath = m_fileFld->getPath();
    stringPath.replace("\\", "\\\\");
    fileParam.m_path       = TFilePath(stringPath.toStdWString());
    fileParam.m_fieldIndex = qMax(0, m_fieldIndexFld->text().toInt() - 1);

    unitName = m_measureFld->text().toStdString();
  }
};

//=============================================================================

SimilarShapeSegmentPage::SimilarShapeSegmentPage(FunctionSegmentViewer *parent)
    : FunctionSegmentPage(parent) {
  m_expressionFld = new DVGui::ExpressionField();
  m_offsetFld     = createField();

  //----layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setSpacing(2);
  mainLayout->setMargin(2);
  {
    mainLayout->addSpacing(3);
    mainLayout->addWidget(new QLabel(tr("Reference Curve:")));
    mainLayout->addWidget(m_expressionFld);

    mainLayout->addSpacing(3);
    QHBoxLayout *offLay = new QHBoxLayout();
    {
      offLay->addWidget(new QLabel(tr("Frame Offset:")));
      offLay->addWidget(m_offsetFld);
      offLay->addStretch();
    }
    mainLayout->addLayout(offLay);
  }
  setLayout(mainLayout);
}

void SimilarShapeSegmentPage::refresh() {
  TDoubleParam *curve = getCurve();
  if (!curve) {
    m_expressionFld->setGrammar(0);
    return;
  }

  TDoubleKeyframe kf0        = curve->getKeyframeAt(getR0());
  std::string expression     = kf0.m_expressionText;
  bool oldBlockSignalsStatus = m_expressionFld->blockSignals(true);
  m_expressionFld->setGrammar(curve->getGrammar());
  m_expressionFld->setExpression(expression);
  m_expressionFld->blockSignals(oldBlockSignalsStatus);

  m_offsetFld->setText(QString::number(kf0.m_similarShapeOffset, 'f', 0));
}

void SimilarShapeSegmentPage::init(int segmentLength) {
  TDoubleParam *curve = getCurve();
  if (!curve) {
    m_expressionFld->setGrammar(0);
    m_expressionFld->setEnabled(false);
    return;
  }

  m_expressionFld->setEnabled(true);
  TDoubleKeyframe kf0        = curve->getKeyframeAt(getR0());
  std::string expression     = kf0.m_expressionText;
  bool oldBlockSignalsStatus = m_expressionFld->blockSignals(true);
  m_expressionFld->setGrammar(curve->getGrammar());
  m_expressionFld->setExpression(expression);
  m_expressionFld->blockSignals(oldBlockSignalsStatus);

  m_offsetFld->setText(QString::number(kf0.m_similarShapeOffset, 'f', 0));
}

void SimilarShapeSegmentPage::apply() {
  TDoubleParam *curve = getCurve();
  if (!curve) return;
  int kIndex = getViewer()->getSegmentIndex();
  if (kIndex < 0) return;

  std::string expressionText = m_expressionFld->getExpression();
  TExpression expr;
  expr.setGrammar(curve->getGrammar());
  expr.setText(expressionText);
  if (!expr.isValid()) {
    DVGui::warning(
        tr("There is a syntax error in the definition of the interpolation."));
    return;
  }
  if (dependsOn(expr, curve)) {
    DVGui::warning(
        tr("There is a circular reference in the definition of the "
           "interpolation."));
    return;
  }
  KeyframeSetter setter(curve, kIndex);
  setter.setSimilarShape(m_expressionFld->getExpression(),
                         m_offsetFld->text().toDouble());
}

void SimilarShapeSegmentPage::getGuiValues(std::string &expressionText,
                                           double &similarShapeOffset) {
  expressionText     = m_expressionFld->getExpression();
  similarShapeOffset = m_offsetFld->text().toDouble();
}

//=============================================================================

FunctionSegmentViewer::FunctionSegmentViewer(QWidget *parent,
                                             FunctionSheet *sheet,
                                             FunctionPanel *panel)
    : QFrame(parent)
    , m_curve(0)
    , m_r0(0)
    , m_r1(0)
    , m_sheet(sheet)
    , m_xshHandle(0)
    , m_panel(panel) {
  setObjectName("FunctionSegmentViewer");

  m_pages[0] = new FunctionEmptySegmentPage(this);
  m_pages[1] = new SpeedInOutSegmentPage(this);
  m_pages[2] = new EaseInOutSegmentPage(false, this);
  m_pages[3] = new EaseInOutSegmentPage(true, this);
  m_pages[4] = new FunctionEmptySegmentPage(this);
  m_pages[5] = new FunctionExpressionSegmentPage(this);
  m_pages[6] = new FileSegmentPage(this);
  m_pages[7] = new FunctionEmptySegmentPage(this);
  m_pages[8] = new SimilarShapeSegmentPage(this);

  m_typeId[0] = TDoubleKeyframe::Linear;
  m_typeId[1] = TDoubleKeyframe::SpeedInOut;
  m_typeId[2] = TDoubleKeyframe::EaseInOut;
  m_typeId[3] = TDoubleKeyframe::EaseInOutPercentage;
  m_typeId[4] = TDoubleKeyframe::Exponential;
  m_typeId[5] = TDoubleKeyframe::Expression;
  m_typeId[6] = TDoubleKeyframe::File;
  m_typeId[7] = TDoubleKeyframe::Constant;
  m_typeId[8] = TDoubleKeyframe::SimilarShape;

  m_typeCombo = new QComboBox;
  m_typeCombo->addItem(tr("Linear"));
  m_typeCombo->addItem(tr("Speed In / Speed Out"));
  m_typeCombo->addItem(tr("Ease In / Ease Out"));
  m_typeCombo->addItem(tr("Ease In / Ease Out %"));
  m_typeCombo->addItem(tr("Exponential"));
  m_typeCombo->addItem(tr("Expression"));
  m_typeCombo->addItem(tr("File"));
  m_typeCombo->addItem(tr("Constant"));
  m_typeCombo->addItem(tr("Similar Shape"));
  m_typeCombo->setCurrentIndex(7);

  //---- common interfaces
  m_fromFld        = new QLineEdit(this);
  m_toFld          = new QLineEdit(this);
  m_paramNameLabel = new QLabel("", this);

  QLabel *typeLabel = new QLabel(tr("Interpolation:"));
  typeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_stepFld = new LineEdit();

  // bottom part: stacked widget
  m_parametersPanel = new QStackedWidget;
  m_parametersPanel->setObjectName("FunctionParametersPanel");

  for (int i = 0; i < tArrayCount(m_pages); i++)
    m_parametersPanel->addWidget(m_pages[i]);
  m_parametersPanel->setCurrentIndex(0);

  // buttons
  QPushButton *applyButton = new QPushButton(tr("Apply"), this);
  m_prevCurveButton        = new QPushButton(this);
  m_nextCurveButton        = new QPushButton(this);
  m_prevLinkButton         = new QPushButton(this);
  m_nextLinkButton         = new QPushButton(this);

  //-----
  QIntValidator *intValidator = new QIntValidator(1, 100, this);
  m_stepFld->setValidator(intValidator);

  QIntValidator *validator = new QIntValidator(this);
  validator->setBottom(1);
  m_fromFld->setValidator(validator);
  m_toFld->setValidator(validator);

  m_stepFld->setEnabled(true);

  applyButton->setFocusPolicy(Qt::NoFocus);

  m_stepFld->setText("1");

  m_prevCurveButton->setFixedSize(50, 15);
  m_nextCurveButton->setFixedSize(50, 15);
  m_prevCurveButton->setFocusPolicy(Qt::NoFocus);
  m_nextCurveButton->setFocusPolicy(Qt::NoFocus);
  m_prevCurveButton->setStyleSheet("padding:0px;");
  m_nextCurveButton->setStyleSheet("padding:0px;");

  m_prevLinkButton->setFixedSize(15, 15);
  m_nextLinkButton->setFixedSize(15, 15);
  m_prevLinkButton->setCheckable(true);
  m_nextLinkButton->setCheckable(true);
  m_prevLinkButton->setFocusPolicy(Qt::NoFocus);
  m_nextLinkButton->setFocusPolicy(Qt::NoFocus);
  m_prevLinkButton->setObjectName("FunctionSegmentViewerLinkButton");
  m_nextLinkButton->setObjectName("FunctionSegmentViewerLinkButton");

  //---- layout

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setSpacing(5);
  mainLayout->setMargin(5);
  {
    m_topbar                  = new QWidget();
    QVBoxLayout *topbarLayout = new QVBoxLayout();
    topbarLayout->setSpacing(5);
    topbarLayout->setMargin(0);
    {
      topbarLayout->addWidget(m_paramNameLabel);

      QHBoxLayout *upperLay = new QHBoxLayout();
      upperLay->setSpacing(3);
      upperLay->setMargin(0);
      {
        upperLay->addWidget(new QLabel(tr("From"), this), 0);
        upperLay->addWidget(m_fromFld, 1);
        upperLay->addSpacing(3);
        upperLay->addWidget(new QLabel(tr("To"), this), 0);
        upperLay->addWidget(m_toFld, 1);
        upperLay->addSpacing(5);
        upperLay->addWidget(new QLabel(tr("Step"), this), 0);
        upperLay->addWidget(m_stepFld, 1);
      }
      topbarLayout->addLayout(upperLay, 0);

      QHBoxLayout *bottomLay = new QHBoxLayout();
      bottomLay->setSpacing(3);
      bottomLay->setMargin(0);
      {
        bottomLay->addWidget(typeLabel, 0);
        bottomLay->addWidget(m_typeCombo, 1);
      }
      topbarLayout->addLayout(bottomLay, 0);
    }
    // end topbar
    m_topbar->setLayout(topbarLayout);

    mainLayout->addWidget(m_topbar, 0);
    mainLayout->addWidget(m_parametersPanel, 0);
    mainLayout->addStretch(1);

    mainLayout->addWidget(applyButton);

    QHBoxLayout *moveLay = new QHBoxLayout();
    moveLay->setMargin(0);
    moveLay->setSpacing(0);
    {
      moveLay->addWidget(m_prevCurveButton, 0);
      moveLay->addWidget(m_prevLinkButton, 0);
      moveLay->addStretch(1);
      moveLay->addWidget(m_nextLinkButton, 0);
      moveLay->addWidget(m_nextCurveButton, 0);
    }
    mainLayout->addLayout(moveLay, 0);
  }
  setLayout(mainLayout);

  //---- signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_typeCombo, SIGNAL(currentIndexChanged(int)),
                       m_parametersPanel, SLOT(setCurrentIndex(int)));
  ret = ret && connect(m_typeCombo, SIGNAL(activated(int)), this,
                       SLOT(onSegmentTypeChanged(int)));
  ret = ret && connect(applyButton, SIGNAL(clicked()), this,
                       SLOT(onApplyButtonPressed()));
  ret = ret && connect(m_prevCurveButton, SIGNAL(clicked()), this,
                       SLOT(onPrevCurveButtonPressed()));
  ret = ret && connect(m_nextCurveButton, SIGNAL(clicked()), this,
                       SLOT(onNextCurveButtonPressed()));
  ret = ret && connect(m_prevLinkButton, SIGNAL(clicked()), this,
                       SLOT(onPrevLinkButtonPressed()));
  ret = ret && connect(m_nextLinkButton, SIGNAL(clicked()), this,
                       SLOT(onNextLinkButtonPressed()));
  assert(ret);

  m_sheet = sheet;
  refresh();
}

FunctionSegmentViewer::~FunctionSegmentViewer() {
  if (m_curve) m_curve->release();
  m_curve = 0;
}

void FunctionSegmentViewer::setSegment(TDoubleParam *curve, int segmentIndex) {
  if (curve != m_curve) {
    if (m_curve) {
      m_curve->removeObserver(this);
      m_curve->release();
    }
    m_curve = curve;
    if (m_curve) {
      m_curve->addRef();
      m_curve->addObserver(this);
    }
  }
  m_segmentIndex = segmentIndex;

  refresh();
}

void FunctionSegmentViewer::setSegmentByFrame(TDoubleParam *curve, int frame) {
  bool curveSwitched = false;
  if (curve != m_curve) {
    curveSwitched = true;
    if (m_curve) {
      m_curve->removeObserver(this);
      m_curve->release();
    }
    m_curve = curve;
    if (m_curve) {
      m_curve->addRef();
      m_curve->addObserver(this);
    }
  }
  bool segmentSwitched = false;
  if (m_curve && (curveSwitched || frame < m_r0 || frame > m_r1)) {
    int segmentIndex = -1;
    if (m_curve->isKeyframe(frame)) {
      int k1 = m_curve->getNextKeyframe(frame);
      if (k1 >= 1)
        segmentIndex = k1 - 1;
      else {
        int k0                    = m_curve->getPrevKeyframe(frame);
        if (k0 >= 0) segmentIndex = k0;
      }
    } else {
      segmentIndex = m_curve->getPrevKeyframe(frame);
      if (m_curve->getNextKeyframe(frame) < 0) segmentIndex = -1;
    }
    if (m_segmentIndex != segmentIndex) {
      m_segmentIndex  = segmentIndex;
      segmentSwitched = true;
    }
  }
  if (curveSwitched || segmentSwitched) refresh();
}

void FunctionSegmentViewer::refresh() {
  if (m_sheet->isVisible()) {
    m_paramNameLabel->setText(m_sheet->getSelectedParamName());
    if (m_sheet->getSelectedParamName().isEmpty()) {
      m_curve        = 0;
      m_segmentIndex = -1;
    }
  } else {
    m_paramNameLabel->setText("");
  }

  if (m_curve &&
      (m_segmentIndex < 0 || m_segmentIndex + 1 >= m_curve->getKeyframeCount()))
    m_segmentIndex = -1;

  m_prevCurveButton->setEnabled(false);
  m_prevLinkButton->setEnabled(false);
  m_prevCurveButton->setText(" --- ");
  m_prevLinkButton->setChecked(false);
  m_nextCurveButton->setEnabled(false);
  m_nextLinkButton->setEnabled(false);
  m_nextCurveButton->setText(" --- ");
  m_nextLinkButton->setChecked(false);

  // if some segment is selected
  if (m_curve && m_segmentIndex >= 0) {
    m_topbar->show();
    // m_parametersPanel->show();
    m_r0 = m_curve->keyframeIndexToFrame(m_segmentIndex);
    m_r1 = m_curve->keyframeIndexToFrame(m_segmentIndex + 1);
    m_fromFld->setText(QString::number(m_r0 + 1));
    m_toFld->setText(QString::number(m_r1 + 1));

    TDoubleKeyframe kf = m_curve->getKeyframeAt(m_r0);
    int pageIndex      = typeToIndex(kf.m_type);
    m_typeCombo->setEnabled(true);
    m_typeCombo->setCurrentIndex(pageIndex);
    if (0 <= pageIndex && pageIndex < tArrayCount(m_pages)) {
      m_parametersPanel->setCurrentIndex(pageIndex);
      m_pages[pageIndex]->refresh();
    }
    m_stepFld->setText(QString::number(kf.m_step));

    /*--- 前後のキーフレームの表示を更新 ---*/
    // Prev
    if (m_segmentIndex != 0) {
      TDoubleKeyframe prevKf = m_curve->getKeyframe(m_segmentIndex - 1);
      m_prevCurveButton->setEnabled(true);
      m_prevLinkButton->setEnabled(true);
      m_prevCurveButton->setText(tr("< ") + typeToString(prevKf.m_type));
      m_prevLinkButton->setChecked(kf.m_linkedHandles);
    }
    // Next
    if (m_curve->getKeyframeCount() - 2 != m_segmentIndex) {
      TDoubleKeyframe nextKf = m_curve->getKeyframe(m_segmentIndex + 1);
      m_nextCurveButton->setEnabled(true);
      m_nextLinkButton->setEnabled(true);
      m_nextCurveButton->setText(typeToString(nextKf.m_type) + tr(" >"));
      m_nextLinkButton->setChecked(nextKf.m_linkedHandles);
    }

  }
  /* ---
Segmentが選ばれていない場合
既にキーフレームが有る場合は、選択範囲上下どちらかと最近傍のキーフレームでSegmentを作る
キーフレームが無い場合は、選択範囲を入力
ただし選択範囲が１フレームのときは、そのフレームから１５フレームの範囲で入力
---*/
  else {
    m_stepFld->setText("1");

    m_parametersPanel->setCurrentIndex(0);

    m_typeCombo->setCurrentIndex(7);

    m_r0 = m_r1 = -1;

    QRect selectedCells = m_sheet->getSelectedCells();
    /*--- 選択範囲が空のとき、もしくはカーブが選ばれていないとき ---*/
    if (selectedCells.isEmpty() || !m_curve) {
      m_typeCombo->setEnabled(false);
      m_fromFld->setText("");
      m_toFld->setText("");
    }
    /*--- 何かカーブが選択されている ---*/
    else {
      m_typeCombo->setEnabled(true);
      int s0 = selectedCells.top();
      int s1 = selectedCells.bottom();

      /*--- セグメントの上側が選択されているとき ---*/
      int next = m_curve->getNextKeyframe(s0);
      if (next >= 0) {
        m_fromFld->setText(QString::number(s0 + 1));
        m_toFld->setText(
            QString::number(m_curve->getKeyframe(next).m_frame + 1));
        //*--- 前後のキーフレームの表示を更新 ---*/
        if (m_curve->getKeyframeCount() >= 2) {
          TDoubleKeyframe nextKf = m_curve->getKeyframe(next);
          m_nextCurveButton->setEnabled(true);
          m_nextLinkButton->setEnabled(true);
          m_nextCurveButton->setText(typeToString(nextKf.m_type) + tr(" >"));
          m_nextLinkButton->setChecked(nextKf.m_linkedHandles);
        }
      } else {
        /*--- セグメントの下側が選択されているとき ---*/
        int prev = m_curve->getPrevKeyframe(s0);
        if (prev >= 0) {
          m_fromFld->setText(
              QString::number(m_curve->getKeyframe(prev).m_frame + 1));
          m_toFld->setText(QString::number(s1 + 1));

          //*--- 前後のキーフレームの表示を更新 ---*/
          if (prev > 0) {
            TDoubleKeyframe kf     = m_curve->getKeyframe(prev);
            TDoubleKeyframe prevKf = m_curve->getKeyframe(prev - 1);
            m_prevCurveButton->setEnabled(true);
            m_prevLinkButton->setEnabled(true);
            m_prevCurveButton->setText(tr("< ") + typeToString(prevKf.m_type));
            m_prevLinkButton->setChecked(kf.m_linkedHandles);
          }
        }
        /*--- キーフレームが１個もない場合 ---*/
        else {
          if (s0 == s1) {
            int endFrame;
            if (m_xshHandle) {
              endFrame = m_xshHandle->getXsheet()->getFrameCount();
              /*---
               * xsheetに何もLevelがないとき,又は選択フレームがXsheetをはみだしているとき
               * ---*/
              if (endFrame == 0 || s0 + 1 >= endFrame) endFrame = s0 + 16;
            } else {
              endFrame = s0 + 16;
            }
            m_fromFld->setText(QString::number(s0 + 1));
            m_toFld->setText(QString::number(endFrame));

          } else {
            m_fromFld->setText(QString::number(s0 + 1));
            m_toFld->setText(QString::number(s1 + 1));
          }
        }
      }
    }
  }
}

void FunctionSegmentViewer::onCurveChanged() {
  int pageIndex = m_typeCombo->currentIndex();
  if (0 <= pageIndex && pageIndex < tArrayCount(m_pages))
    m_pages[pageIndex]->refresh();
  update();
}

void FunctionSegmentViewer::onStepFieldChanged(const QString &text) {
  if (!segmentIsValid()) return;
  int step             = 1;
  if (text != "") step = text.toInt();
  if (step < 1) step   = 1;
  KeyframeSetter setter(m_curve, m_segmentIndex);
  setter.setStep(step);
}

int FunctionSegmentViewer::typeToIndex(int typeId) const {
  for (int i = 0; i < tArrayCount(m_typeId); i++)
    if (m_typeId[i] == typeId) return i;
  return -1;
}

bool FunctionSegmentViewer::segmentIsValid() const {
  return m_curve && 0 <= m_segmentIndex &&
         m_segmentIndex + 1 < m_curve->getKeyframeCount();
}
/*----
 すでにカーブがあり元のTypeの場合→本来の値に更新（refresh）
 EaseIn/Out⇔EaseIn/Out(％)→値を変換
 その他→各ページのイニシャライズ(ExpressionはGrammerを入れる)
---*/
void FunctionSegmentViewer::onSegmentTypeChanged(int typeIndex) {
  if (!m_curve) return;

  /*--- すでにカーブがあり元のTypeの場合→カーブの情報本来の値に更新（refresh)
   * ---*/
  if (m_segmentIndex >= 0) {
    TDoubleKeyframe::Type currentType =
        m_curve->getKeyframe(m_segmentIndex).m_type;
    if (typeIndex == typeToIndex(currentType)) {
      m_pages[typeIndex]->refresh();
      return;
    }
  }

  int segmentLength = m_toFld->text().toInt() - m_fromFld->text().toInt();

  m_pages[typeIndex]->init(segmentLength);
}

void FunctionSegmentViewer::onApplyButtonPressed() {
  /*--- カーブを何も掴んでいなればreturn ---*/
  if (!m_curve) return;

  /*--- パラメータの現状を取得 ---*/
  int fromFrame = m_fromFld->text().toInt() - 1;
  int toFrame   = m_toFld->text().toInt() - 1;
  TDoubleKeyframe::Type comboType =
      (TDoubleKeyframe::Type)indexToType(m_typeCombo->currentIndex());
  int step = m_stepFld->text().toInt();

  TPointD speedIn(0, 0);
  TPointD speedOut(0, 0);
  std::string expressionText = "";
  std::string unitName       = "inch";
  TDoubleKeyframe::FileParams fileParams;
  double similarShapeOffset = 0.0;

  /*--- 現在のindexに合わせて、必要なパラメータをGUIから持ってきて格納 ---*/
  switch (comboType) {
  case TDoubleKeyframe::Linear:
    // no param
    break;
  case TDoubleKeyframe::SpeedInOut: {
    SpeedInOutSegmentPage *page = qobject_cast<SpeedInOutSegmentPage *>(
        m_parametersPanel->currentWidget());
    if (page) page->getGuiValues(speedIn, speedOut);
  } break;
  case TDoubleKeyframe::EaseInOut:
  case TDoubleKeyframe::EaseInOutPercentage: {
    EaseInOutSegmentPage *page = qobject_cast<EaseInOutSegmentPage *>(
        m_parametersPanel->currentWidget());
    if (page) page->getGuiValues(speedIn, speedOut);
  } break;
  case TDoubleKeyframe::Exponential:
    // no param
    break;
  case TDoubleKeyframe::Expression: {
    FunctionExpressionSegmentPage *page =
        qobject_cast<FunctionExpressionSegmentPage *>(
            m_parametersPanel->currentWidget());
    if (page) {
      bool ok = page->getGuiValues(expressionText, unitName);
      if (!ok) return;
    }
  } break;
  case TDoubleKeyframe::File: {
    FileSegmentPage *page =
        dynamic_cast<FileSegmentPage *>(m_parametersPanel->currentWidget());
    if (page) page->getGuiValues(fileParams, unitName);
  } break;
  case TDoubleKeyframe::None:
    // no param
    break;
  case TDoubleKeyframe::SimilarShape: {
    SimilarShapeSegmentPage *page = qobject_cast<SimilarShapeSegmentPage *>(
        m_parametersPanel->currentWidget());
    if (page) page->getGuiValues(expressionText, similarShapeOffset);
  } break;
  default:
    break;
  }

  /*--- from -
   * toに合わせてキーフレームを作成しようと試みる。すでに有る場合はスキップ
   * ---*/
  if (fromFrame < 0) fromFrame        = 0;
  if (toFrame < 0) toFrame            = 0;
  if (fromFrame >= toFrame) fromFrame = toFrame + 1;

  if (!m_curve->isKeyframe(fromFrame))
    KeyframeSetter::setValue(m_curve, fromFrame, m_curve->getValue(fromFrame));
  if (!m_curve->isKeyframe(toFrame))
    KeyframeSetter::setValue(m_curve, toFrame, m_curve->getValue(toFrame));

  /*--- 現在のSegmentIndexを更新 ---*/
  m_segmentIndex = m_curve->getClosestKeyframe(fromFrame);

  /*--- パラメータを一括で設定 ---*/
  KeyframeSetter setter(m_curve, m_segmentIndex);

  if (m_panel) setter.setPixelRatio(m_panel->getPixelRatio(m_curve));

  setter.setAllParams(step, comboType, speedIn, speedOut, expressionText,
                      unitName, fileParams, similarShapeOffset);
}

// for displaying the types of neighbor segments
QString FunctionSegmentViewer::typeToString(int typeId) const {
  int i;
  for (i = 0; i < tArrayCount(m_typeId); i++)
    if (m_typeId[i] == typeId) break;

  switch (i) {
  case 0:
    return tr("Linear");
    break;
  case 1:
    return tr("Speed");
    break;
  case 2:
    return tr("Ease");
    break;
  case 3:
    return tr("Ease%");
    break;
  case 4:
    return tr("Expo");
    break;
  case 5:
    return tr("Expr");
    break;
  case 6:
    return tr("File");
    break;
  case 7:
    return tr("Const");
    break;
  case 8:
    return tr("Similar");
    break;
  default:
    return tr("????");
    break;
  }
}

void FunctionSegmentViewer::onPrevCurveButtonPressed() {
  if (!m_curve) return;

  if (m_segmentIndex == 0) return;

  int currentKeyIndex;
  if (m_segmentIndex > 0) currentKeyIndex = m_segmentIndex;
  /*--- Segmentが選択されていない→Segmentの下側が選ばれているはず ---*/
  else {
    QRect selectedCells = m_sheet->getSelectedCells();
    if (selectedCells.isEmpty()) return;

    currentKeyIndex = m_curve->getPrevKeyframe(selectedCells.top());
    if (currentKeyIndex != m_curve->getKeyframeCount() - 1) return;
  }

  int col = m_sheet->getColumnIndexByCurve(m_curve);
  /*--- Sheet上にCurveが表示されていない場合はcolに-1が返る ---*/
  if (col < 0) return;

  TDoubleKeyframe currentKey = m_curve->getKeyframe(currentKeyIndex);
  TDoubleKeyframe prevKey    = m_curve->getKeyframe(currentKeyIndex - 1);

  int r0 = (int)prevKey.m_frame;
  int r1 = (int)currentKey.m_frame;

  m_sheet->getSelection()->selectSegment(m_curve, currentKeyIndex - 1,
                                         QRect(col, r0, 1, r1 - r0 + 1));
  m_sheet->updateAll();
}

void FunctionSegmentViewer::onNextCurveButtonPressed() {
  if (!m_curve) return;

  if (m_segmentIndex == m_curve->getKeyframeCount() - 2) return;

  int currentKeyIndex;
  if (m_segmentIndex >= 0) {
    currentKeyIndex = m_segmentIndex;
  }
  /*--- Segmentが選択されていない→Segmentの上側が選ばれているはず ---*/
  else {
    QRect selectedCells = m_sheet->getSelectedCells();
    if (selectedCells.isEmpty()) return;

    currentKeyIndex = m_curve->getNextKeyframe(selectedCells.top());
    if (currentKeyIndex != 0) return;
  }

  int col = m_sheet->getColumnIndexByCurve(m_curve);
  /*--- Sheet上にCurveが表示されていない場合はcolに-1が返る ---*/
  if (col < 0) return;

  TDoubleKeyframe nextKey     = m_curve->getKeyframe(currentKeyIndex + 1);
  TDoubleKeyframe nextNextKey = m_curve->getKeyframe(currentKeyIndex + 2);

  int r0 = (int)nextKey.m_frame;
  int r1 = (int)nextNextKey.m_frame;

  m_sheet->getSelection()->selectSegment(m_curve, currentKeyIndex + 1,
                                         QRect(col, r0, 1, r1 - r0 + 1));
  m_sheet->updateAll();
}

void FunctionSegmentViewer::onPrevLinkButtonPressed() {
  if (m_prevLinkButton->isChecked())
    KeyframeSetter(m_curve, m_segmentIndex).linkHandles();
  else
    KeyframeSetter(m_curve, m_segmentIndex).unlinkHandles();
}

void FunctionSegmentViewer::onNextLinkButtonPressed() {
  if (m_nextLinkButton->isChecked())
    KeyframeSetter(m_curve, m_segmentIndex + 1).linkHandles();
  else
    KeyframeSetter(m_curve, m_segmentIndex + 1).unlinkHandles();
}

bool FunctionSegmentViewer::anyWidgetHasFocus() {
  return m_topbar->hasFocus() || m_fromFld->hasFocus() || m_toFld->hasFocus() ||
         m_typeCombo->hasFocus() || m_stepFld->hasFocus() ||
         m_prevCurveButton->hasFocus() || m_nextCurveButton->hasFocus() ||
         m_prevLinkButton->hasFocus() || m_nextLinkButton->hasFocus();
}

/*! in order to avoid FunctionViewer to get focus while editing the expression
*/
bool FunctionSegmentViewer::isExpressionPageActive() {
  return (m_typeCombo->currentIndex() == 5);
}
