

#include "toonzqt/camerasettingswidget.h"

// TnzQt includes
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvdialog.h"
#include <toonzqt/gutil.h>

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/tcamera.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/preferences.h"
#include "toonz/stage.h"

// TnzCore includes
#include "tconvert.h"
#include "tsystem.h"
#include "tfilepath_io.h"
#include "tutil.h"

// Qt includes
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QApplication>
#include <QComboBox>
#include <QMessageBox>
// #include <QList>
#include <QStringList>
#include <QRegularExpression>
// #include <QPainter>
#include <QTextStream>
// #include <QString>

using namespace DVGui;

namespace {
/*--- Remove trailing zeros from decimals ---*/
QString removeZeros(QString srcStr) {
  if (!srcStr.contains('.')) return srcStr;

  for (int i = srcStr.length() - 1; i >= 0; --i) {
    if (srcStr.at(i) == '0')
      srcStr.chop(1);
    else if (srcStr.at(i) == '.') {
      srcStr.chop(1);
      break;
    } else
      break;
  }
  return srcStr;
}

// Preference values of the unit - translation text
QMap<QString, QString> unitTrMap;

}  // namespace

//=============================================================================

QValidator::State SimpleExpValidator::validate(QString &input, int &pos) const {
  /*--- Allowed characters ---*/
  static const QString validChars = QStringLiteral("0123456789/.");

  int slashCount = 0;

  for (int i = 0; i < input.length(); ++i) {
    /*--- First, if it contains invalid characters, return Invalid ---*/
    if (!validChars.contains(input.at(i))) return QValidator::Invalid;
    if (input.at(i) == '/') ++slashCount;
  }

  /*--- Content is regular numbers. If convertible to Double, OK ---*/
  if (slashCount == 0) {
    bool ok;
    double tmp = input.toDouble(&ok);
    if (ok)
      return (tmp > 0) ? QValidator::Acceptable : QValidator::Intermediate;
    else
      return QValidator::Intermediate;
  } else if (slashCount >= 2) {
    return QValidator::Intermediate;
  } else  // slashCount == 1
  {
    if (input.front() == '/' || input.back() == '/')
      return QValidator::Intermediate;

    QStringList strList = input.split('/');
    /*---
     * Check if convertible to Double on both sides of slash. If not,
     * Intermediate.
     * ---*/
    for (const QString &tmpStr : strList) {
      bool ok;
      double tmp = tmpStr.toDouble(&ok);
      if (!ok) return QValidator::Intermediate;
      if (ok && tmp <= 0) return QValidator::Intermediate;
    }
    /*--- Convertible to Double on both sides, so OK ---*/
    return QValidator::Acceptable;
  }
}

//=============================================================================

SimpleExpField::SimpleExpField(QWidget *parent) : QLineEdit(parent) {
  m_validator = new SimpleExpValidator(this);
  setValidator(m_validator);
}

//--------------------------------------------------------------------------
/*--- Set value normally ---*/
void SimpleExpField::setValue(double value) { setText(QString::number(value)); }

//--------------------------------------------------------------------------
/*--- For A/R: if value is close to w/h, insert "w/h" string ---*/
void SimpleExpField::setValue(double value, int w, int h) {
  QString str;
  if (areAlmostEqual(value, static_cast<double>(w) / h, 1e-5))
    str = QString("%1/%2").arg(w).arg(h);
  else
    str = QString::number(value);

  setText(str);
}

//--------------------------------------------------------------------------

double SimpleExpField::getValue() {
  int slashCount = text().count('/');
  if (slashCount == 0)
    return text().toDouble();
  else if (slashCount == 1) {
    QStringList strList = text().split('/');
    return strList.at(0).toDouble() / strList.at(1).toDouble();
  }
  std::cout << "more than one slash!" << std::endl;
  return 0.1;
}

//--------------------------------------------------------------------------

void SimpleExpField::focusInEvent(QFocusEvent *event) {
  m_previousValue = text();
  QLineEdit::focusInEvent(event);
}

//--------------------------------------------------------------------------

void SimpleExpField::focusOutEvent(QFocusEvent *event) {
  int dummy;
  QString tmp = text();
  if (validator()->validate(tmp, dummy) != QValidator::Acceptable)
    setText(m_previousValue);
  QLineEdit::focusOutEvent(event);
}

//=============================================================================

CameraSettingsWidget::CameraSettingsWidget(bool forCleanup)
    : m_forCleanup(forCleanup), m_arValue(0), m_currentLevel(nullptr) {
  if (unitTrMap.isEmpty()) {
    unitTrMap["cm"]    = tr("cm");
    unitTrMap["mm"]    = tr("mm");
    unitTrMap["inch"]  = tr("inch");
    unitTrMap["field"] = tr("field");
    unitTrMap["pixel"] = tr("pixel");
  }

  m_xPrev    = new QRadioButton();
  m_yPrev    = new QRadioButton();
  m_arPrev   = new QRadioButton();
  m_inchPrev = new QRadioButton();
  m_dotPrev  = new QRadioButton();

  m_lxFld = new MeasuredDoubleLineEdit();
  m_lyFld = new MeasuredDoubleLineEdit();

  m_arFld = new SimpleExpField(this);

  m_xResFld   = new DVGui::IntLineEdit();
  m_yResFld   = new DVGui::IntLineEdit();
  m_xDpiFld   = new DoubleLineEdit();
  m_yDpiFld   = new DoubleLineEdit();
  m_unitLabel = new QLabel();
  if (Preferences::instance()->getPixelsOnly())
    m_unitLabel->setText(tr("Pixels"));
  else
    m_unitLabel->setText(
        unitTrMap.value(Preferences::instance()->getCameraUnits()));
  m_dpiLabel = new QLabel(tr("DPI"));
  m_resLabel = new QLabel(tr("Pixels"));
  m_xLabel   = new QLabel(tr("x"));

  m_fspChk = new QPushButton("");

  m_useLevelSettingsBtn = new QPushButton(tr("Use Current Level Settings"));

  m_presetListOm    = new QComboBox();
  m_addPresetBtn    = new QPushButton(tr("Add"));
  m_removePresetBtn = new QPushButton(tr("Remove"));

  //----
  m_useLevelSettingsBtn->setEnabled(false);
  m_useLevelSettingsBtn->setFocusPolicy(Qt::NoFocus);
  m_lxFld->installEventFilter(this);
  m_lyFld->installEventFilter(this);
  m_arFld->installEventFilter(this);
  m_xResFld->installEventFilter(this);
  m_yResFld->installEventFilter(this);
  m_xDpiFld->installEventFilter(this);
  m_yDpiFld->installEventFilter(this);
  m_xResFld->setMinimumWidth(0);
  m_xResFld->setMaximumWidth(QWIDGETSIZE_MAX);
  m_yResFld->setMinimumWidth(0);
  m_yResFld->setMaximumWidth(QWIDGETSIZE_MAX);

  /*--- Set display precision to 4 digits ---*/
  m_lxFld->setDecimals(4);
  m_lyFld->setDecimals(4);

  m_lxFld->setMeasure("camera.lx");
  m_lyFld->setMeasure("camera.ly");
  m_lxFld->setRange(std::numeric_limits<double>::epsilon(),
                    std::numeric_limits<double>::infinity());
  m_lyFld->setRange(std::numeric_limits<double>::epsilon(),
                    std::numeric_limits<double>::infinity());

  m_xResFld->setRange(1, 10000000);
  m_yResFld->setRange(1, 10000000);

  m_xDpiFld->setRange(1, std::numeric_limits<double>::infinity());
  m_yDpiFld->setRange(1, std::numeric_limits<double>::infinity());

  m_fspChk->setFixedSize(20, 20);
  m_fspChk->setCheckable(true);
  m_fspChk->setChecked(true);
  m_fspChk->setToolTip(tr("Force Squared Pixel"));
  m_fspChk->setObjectName("ForceSquaredPixelButton");
  m_fspChk->setIcon(createQIcon("squarepixel"));

  m_presetListOm->setFocusPolicy(Qt::StrongFocus);
  m_presetListOm->installEventFilter(this);
  m_addPresetBtn->setObjectName("PushButton_NoPadding");
  m_removePresetBtn->setObjectName("PushButton_NoPadding");

  m_inchPrev->setFixedSize(13, 13);
  m_dotPrev->setFixedSize(13, 13);
  m_inchPrev->setObjectName("CameraSettingsRadioButton_Small");
  m_dotPrev->setObjectName("CameraSettingsRadioButton_Small");

  m_xPrev->setObjectName("CameraSettingsRadioButton");
  m_yPrev->setObjectName("CameraSettingsRadioButton");
  m_arPrev->setObjectName("CameraSettingsRadioButton");

  // radio buttons groups
  auto *group1 = new QButtonGroup;
  group1->addButton(m_xPrev);
  group1->addButton(m_yPrev);
  group1->addButton(m_arPrev);

  auto *group2 = new QButtonGroup;
  group2->addButton(m_inchPrev);
  group2->addButton(m_dotPrev);

  m_arPrev->setChecked(true);
  m_dotPrev->setChecked(true);

  //------ layout

  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setSpacing(3);
  mainLay->setContentsMargins(3, 3, 3, 3);
  {
    QGridLayout *gridLay = new QGridLayout();
    gridLay->setHorizontalSpacing(2);
    gridLay->setVerticalSpacing(3);
    {
      gridLay->addWidget(m_xPrev, 0, 2, Qt::AlignCenter);
      gridLay->addWidget(m_yPrev, 0, 4, Qt::AlignCenter);

      gridLay->addWidget(m_inchPrev, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
      gridLay->addWidget(m_unitLabel, 1, 1, Qt::AlignRight | Qt::AlignVCenter);
      gridLay->addWidget(m_lxFld, 1, 2);
      gridLay->addWidget(new QLabel("x"), 1, 3, Qt::AlignCenter);
      gridLay->addWidget(m_lyFld, 1, 4);

      gridLay->addWidget(m_arPrev, 2, 2, Qt::AlignRight | Qt::AlignVCenter);
      gridLay->addWidget(new QLabel(tr("A/R")), 2, 3, Qt::AlignCenter);
      gridLay->addWidget(m_arFld, 2, 4);

      gridLay->addWidget(m_dotPrev, 3, 0, Qt::AlignRight | Qt::AlignVCenter);
      gridLay->addWidget(m_resLabel, 3, 1, Qt::AlignRight | Qt::AlignVCenter);
      gridLay->addWidget(m_xResFld, 3, 2);
      gridLay->addWidget(m_xLabel, 3, 3, Qt::AlignCenter);
      gridLay->addWidget(m_yResFld, 3, 4);

      gridLay->addWidget(m_dpiLabel, 4, 1, Qt::AlignRight | Qt::AlignVCenter);
      gridLay->addWidget(m_xDpiFld, 4, 2);
      gridLay->addWidget(m_fspChk, 4, 3, Qt::AlignCenter);
      gridLay->addWidget(m_yDpiFld, 4, 4);
    }
    gridLay->setColumnStretch(0, 0);
    gridLay->setColumnStretch(1, 0);
    gridLay->setColumnStretch(2, 1);
    gridLay->setColumnStretch(3, 0);
    gridLay->setColumnStretch(4, 1);
    mainLay->addLayout(gridLay);

    mainLay->addWidget(m_useLevelSettingsBtn);

    QHBoxLayout *resListLay = new QHBoxLayout();
    resListLay->setSpacing(3);
    resListLay->setContentsMargins(1, 1, 1, 1);
    {
      resListLay->addWidget(m_presetListOm, 1);
      resListLay->addWidget(m_addPresetBtn, 0);
      resListLay->addWidget(m_removePresetBtn, 0);
    }
    mainLay->addLayout(resListLay);
  }
  setLayout(mainLay);

  // initialize fields
  TCamera camera;
  setFields(&camera);

  // Modernized signal-slot connections using Qt5 style
  connect(m_lxFld, &MeasuredDoubleLineEdit::editingFinished, this,
          &CameraSettingsWidget::onLxChanged);
  connect(m_lyFld, &MeasuredDoubleLineEdit::editingFinished, this,
          &CameraSettingsWidget::onLyChanged);
  connect(m_arFld, &SimpleExpField::editingFinished, this,
          &CameraSettingsWidget::onArChanged);
  connect(m_xResFld, &DVGui::IntLineEdit::editingFinished, this,
          &CameraSettingsWidget::onXResChanged);
  connect(m_yResFld, &DVGui::IntLineEdit::editingFinished, this,
          &CameraSettingsWidget::onYResChanged);
  connect(m_xDpiFld, &DoubleLineEdit::editingFinished, this,
          &CameraSettingsWidget::onXDpiChanged);
  connect(m_yDpiFld, &DoubleLineEdit::editingFinished, this,
          &CameraSettingsWidget::onYDpiChanged);

  connect(m_fspChk, &QPushButton::clicked, this,
          &CameraSettingsWidget::onFspChanged);

  connect(m_xPrev, &QRadioButton::toggled, this,
          &CameraSettingsWidget::onPrevToggled);
  connect(m_yPrev, &QRadioButton::toggled, this,
          &CameraSettingsWidget::onPrevToggled);
  connect(m_dotPrev, &QRadioButton::toggled, this,
          &CameraSettingsWidget::onPrevToggled);
  connect(m_inchPrev, &QRadioButton::toggled, this,
          &CameraSettingsWidget::onPrevToggled);

  connect(m_useLevelSettingsBtn, &QPushButton::clicked, this,
          &CameraSettingsWidget::useLevelSettings);

  // Using textActivated instead of activated (deprecated in Qt 5.15)
  connect(m_presetListOm, &QComboBox::textActivated, this,
          &CameraSettingsWidget::onPresetSelected);

  connect(m_addPresetBtn, &QPushButton::clicked, this,
          &CameraSettingsWidget::addPreset);
  connect(m_removePresetBtn, &QPushButton::clicked, this,
          &CameraSettingsWidget::removePreset);
}

CameraSettingsWidget::~CameraSettingsWidget() { setCurrentLevel(nullptr); }

void CameraSettingsWidget::showEvent(QShowEvent *e) {
  if (Preferences::instance()->getCameraUnits() == "pixel") {
    m_resLabel->hide();
    m_dpiLabel->hide();
    m_xLabel->hide();
    m_xResFld->hide();
    m_yResFld->hide();
    m_xDpiFld->hide();
    m_yDpiFld->hide();
    m_fspChk->hide();
    m_dotPrev->hide();
    m_lxFld->setDecimals(0);
    m_lyFld->setDecimals(0);
  } else {
    m_resLabel->show();
    m_dpiLabel->show();
    m_xLabel->show();
    m_xResFld->show();
    m_yResFld->show();
    m_xDpiFld->show();
    m_yDpiFld->show();
    m_fspChk->show();
    m_dotPrev->show();
    m_lxFld->setDecimals(4);
    m_lyFld->setDecimals(4);
  }
  if (Preferences::instance()->getPixelsOnly())
    m_unitLabel->setText(tr("Pixels"));
  else
    m_unitLabel->setText(
        unitTrMap.value(Preferences::instance()->getCameraUnits()));
}

void CameraSettingsWidget::loadPresetList() {
  if (m_presetListFile.isEmpty()) return;
  m_presetListOm->clear();
  m_presetListOm->addItem(tr("<custom>"));

  QFile file(m_presetListFile);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&file);
    while (!in.atEnd()) {
      QString line = in.readLine().trimmed();
      if (!line.isEmpty()) m_presetListOm->addItem(line);
    }
  }
  m_presetListOm->setCurrentIndex(0);
}

void CameraSettingsWidget::savePresetList() {
  QFile file(m_presetListFile);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  int n = m_presetListOm->count();
  for (int i = 1; i < n; ++i) out << m_presetListOm->itemText(i) << "\n";
}

bool CameraSettingsWidget::parsePresetString(const QString &str, QString &name,
                                             int &xres, int &yres,
                                             QString &ar) {
  // str : name, 1024x768, 4/3
  int b;
  b = str.lastIndexOf(",");
  if (b <= 1) return false;
  b = str.lastIndexOf(",", b - 1);
  if (b <= 0) return false;

  QRegularExpression rx(R"( *([0-9]+)x([0-9]+) *, *(\d*(\.\d+)?|\d+/\d+) *)");
  QRegularExpressionMatch match = rx.match(str.mid(b + 1));
  if (!match.hasMatch()) return false;

  name = b > 0 ? str.left(b).trimmed() : "";
  xres = match.captured(1).toInt();
  yres = match.captured(2).toInt();
  ar   = match.captured(3);
  return true;
}

//--------------------------------------------------------------------------

bool CameraSettingsWidget::parsePresetString(const QString &str, QString &name,
                                             int &xres, int &yres, double &fx,
                                             double &fy, QString &xoffset,
                                             QString &yoffset, double &ar,
                                             bool forCleanup) {
  /*
  parsing preset string with QString::split().
  !NOTE! fx/fy (camera size in inch) and xoffset/yoffset (camera offset used in
  cleanup camera) are optional,
  in order to keep compatibility with default (Harlequin's) reslist.txt
  */

  QStringList tokens = str.split(",", Qt::SkipEmptyParts);

  if (!(tokens.count() == 3 ||
        (!forCleanup && tokens.count() == 4) || /*- with "fx x fy" token -*/
        (forCleanup &&
         tokens.count() ==
             6))) /*- with "fx x fy", xoffset and yoffset tokens -*/
    return false;

  /*- name -*/
  name = tokens[0];

  /*- xres, yres  (like:  1024x768) -*/
  QStringList values = tokens[1].split("x");
  if (values.count() != 2) return false;
  bool ok;
  xres = values[0].toInt(&ok);
  if (!ok) return false;
  yres = values[1].toInt(&ok);
  if (!ok) return false;

  if (tokens.size() >= 4) {
    /*- fx, fy -*/
    values = tokens[2].split("x");
    if (values.size() != 2) return false;
    fx = values[0].toDouble(&ok);
    if (!ok) return false;
    fy = values[1].toDouble(&ok);
    if (!ok) return false;

    /*- xoffset, yoffset -*/
    if (forCleanup) {
      xoffset = tokens[3];
      yoffset = tokens[4];
      /*- remove single space -*/
      if (xoffset.startsWith(' ')) xoffset.remove(0, 1);
      if (yoffset.startsWith(' ')) yoffset.remove(0, 1);
    }
  }

  /*- AR -*/
  ar = aspectRatioStringToValue(tokens.last());

  return true;
}

//--------------------------------------------------------------------------

void CameraSettingsWidget::setPresetListFile(const TFilePath &fp) {
  m_presetListFile = QString::fromStdWString(fp.getWideString());
  loadPresetList();
}

bool CameraSettingsWidget::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() == QEvent::FocusIn) {
    if (m_xPrev->isChecked() && obj == m_lxFld)  // x-prev, fld=lx
      m_yPrev->setChecked(true);
    else if (m_yPrev->isChecked() && obj == m_lyFld)  // y-prev, fld=ly
      m_xPrev->setChecked(true);
    else if (m_arPrev->isChecked() && obj == m_arFld)  // ar-prev, fld=ar
      m_xPrev->setChecked(true);

    if (m_inchPrev->isChecked() &&
        (obj == m_lxFld || obj == m_lyFld ||
         obj == m_arFld))  // inchPrev, fld = lx|ly|ar
      m_dotPrev->setChecked(true);
    else if (m_dotPrev->isChecked() &&
             (obj == m_xResFld ||
              obj == m_yResFld))  // dotPrev, fld = xres|yres
      m_inchPrev->setChecked(true);
  }
  // ignore wheelevent on the combobox
  else if (e->type() == QEvent::Wheel) {
    QComboBox *combo = qobject_cast<QComboBox *>(obj);
    if (combo && !combo->hasFocus()) return true;
  }

  return QObject::eventFilter(obj, e);
}

void CameraSettingsWidget::setCurrentLevel(TXshLevel *xshLevel) {
  TXshSimpleLevel *sl = xshLevel ? xshLevel->getSimpleLevel() : nullptr;
  if (sl && sl->getType() == PLI_XSHLEVEL) sl = nullptr;
  if (sl == m_currentLevel) return;

  if (m_currentLevel) m_currentLevel->release();

  m_currentLevel = sl;
  if (m_currentLevel) m_currentLevel->addRef();

  m_useLevelSettingsBtn->setEnabled(m_currentLevel != nullptr);
}

void CameraSettingsWidget::useLevelSettings() {
  TXshSimpleLevel *sl = m_currentLevel;
  if (!sl) return;

  // Build dpi
  TPointD dpi = sl->getDpi(TFrameId::NO_FRAME, 0);

  // Build physical size
  TDimensionD size(0, 0);
  TDimension res = sl->getResolution();
  if (res.lx <= 0 || res.ly <= 0 || dpi.x <= 0 || dpi.y <= 0) return;

  size.lx = res.lx / dpi.x;
  size.ly = res.ly / dpi.y;

  TCamera camera;
  getFields(&camera);
  camera.setSize(size);
  camera.setRes(res);
  setFields(&camera);
  emit levelSettingsUsed();
  emit changed();
}

void CameraSettingsWidget::setFields(const TCamera *camera) {
  TDimensionD sz = camera->getSize();
  TDimension res = camera->getRes();
  m_lxFld->setValue(sz.lx);
  m_lyFld->setValue(sz.ly);

  m_xResFld->setValue(res.lx);
  m_yResFld->setValue(res.ly);

  setArFld(sz.lx / sz.ly);

  m_xDpiFld->setValue(res.lx / sz.lx);
  m_yDpiFld->setValue(res.ly / sz.ly);
  updatePresetListOm();
}

bool CameraSettingsWidget::getFields(TCamera *camera) {
  TDimensionD old_sz = camera->getSize();
  TDimension old_res = camera->getRes();

  if (old_sz == getSize() && old_res == getRes()) return false;
  camera->setSize(getSize());
  camera->setRes(getRes());
  return true;
}

TDimensionD CameraSettingsWidget::getSize() const {
  double lx = m_lxFld->getValue();
  double ly = m_lyFld->getValue();
  return TDimensionD(lx, ly);
}

TDimension CameraSettingsWidget::getRes() const {
  int xRes = static_cast<int>(m_xResFld->getValue());
  int yRes = static_cast<int>(m_yResFld->getValue());
  return TDimension(xRes, yRes);
}

void CameraSettingsWidget::updatePresetListOm() {
  if (m_presetListOm->currentIndex() == 0) return;

  int xres, yres;
  QString name, xoffset, yoffset;
  double fx, fy, ar;

  if (parsePresetString(m_presetListOm->currentText(), name, xres, yres, fx, fy,
                        xoffset, yoffset, ar, m_forCleanup)) {
    bool match = false;
    if (m_forCleanup && m_offsX && m_offsY) {
      match = xres == m_xResFld->getValue() && yres == m_yResFld->getValue() &&
              (fx < 0.0 || fx == m_lxFld->getValue()) &&
              (fy < 0.0 || fy == m_lyFld->getValue()) &&
              (xoffset.isEmpty() || xoffset == m_offsX->text()) &&
              (yoffset.isEmpty() || yoffset == m_offsY->text());
    } else {
      match = xres == m_xResFld->getValue() && yres == m_yResFld->getValue() &&
              (fx < 0.0 || fx == m_lxFld->getValue()) &&
              (fy < 0.0 || fy == m_lyFld->getValue());
    }
    if (!match) m_presetListOm->setCurrentIndex(0);
  }
}

// ly,ar => lx
void CameraSettingsWidget::hComputeLx() {
  m_lxFld->setValue(m_lyFld->getValue() * m_arValue);
}

// lx,ar => ly
void CameraSettingsWidget::hComputeLy() {
  if (m_arValue == 0.0) return;  // shouldn't happen
  m_lyFld->setValue(m_lxFld->getValue() / m_arValue);
}

// lx,ly => ar
void CameraSettingsWidget::computeAr() {
  if (m_lyFld->getValue() == 0.0) return;  // shouldn't happen
  setArFld(m_lxFld->getValue() / m_lyFld->getValue());
}

// xres,xdpi => lx
void CameraSettingsWidget::vComputeLx() {
  if (m_xDpiFld->getValue() == 0.0) return;  // shouldn't happen
  m_lxFld->setValue(m_xResFld->getValue() / m_xDpiFld->getValue());
}

// yres,ydpi => ly
void CameraSettingsWidget::vComputeLy() {
  if (m_yDpiFld->getValue() == 0.0) return;  // shouldn't happen
  m_lyFld->setValue(m_yResFld->getValue() / m_yDpiFld->getValue());
}

// lx,xdpi => xres
void CameraSettingsWidget::computeXRes() {
  m_xResFld->setValue(tround(m_lxFld->getValue() * m_xDpiFld->getValue()));
}

// ly,ydpi => yres
void CameraSettingsWidget::computeYRes() {
  m_yResFld->setValue(tround(m_lyFld->getValue() * m_yDpiFld->getValue()));
}

// lx,xres => xdpi
void CameraSettingsWidget::computeXDpi() {
  if (m_lxFld->getValue() == 0.0) return;  // shouldn't happen
  m_xDpiFld->setValue(m_xResFld->getValue() / m_lxFld->getValue());
}

// ly,yres => ydpi
void CameraSettingsWidget::computeYDpi() {
  if (m_lyFld->getValue() == 0.0) return;  // shouldn't happen
  m_yDpiFld->setValue(m_yResFld->getValue() / m_lyFld->getValue());
}

// set A/R field, assign m_arValue and compute a nice string representation for
// the value (e.g. "4/3" instead of 1.3333333)
void CameraSettingsWidget::setArFld(double ar) {
  m_arValue = ar;
  /*--- If close to W/H pixel size value, display "W/H" ---*/
  m_arFld->setValue(ar, static_cast<int>(m_xResFld->getValue()),
                    static_cast<int>(m_yResFld->getValue()));
}

// compute res or dpi according to the prevalence
void CameraSettingsWidget::computeResOrDpi() {
  computeXRes();
  computeYRes();
}

void CameraSettingsWidget::onLxChanged() {
  assert(!m_inchPrev->isChecked());
  if (m_yPrev->isChecked())
    computeAr();
  else
    hComputeLy();
  computeResOrDpi();
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onLyChanged() {
  assert(!m_inchPrev->isChecked());
  if (m_xPrev->isChecked())
    computeAr();
  else
    hComputeLx();
  computeResOrDpi();
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onArChanged() {
  m_arValue = aspectRatioStringToValue(m_arFld->text());
  if (m_xPrev->isChecked())
    hComputeLy();
  else
    hComputeLx();
  computeResOrDpi();
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onXResChanged() {
  vComputeLx();
  if (m_yPrev->isChecked())
    computeAr();
  else {
    hComputeLy();
    computeYRes();
  }
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onYResChanged() {
  vComputeLy();
  if (m_xPrev->isChecked())
    computeAr();
  else {
    hComputeLx();
    computeXRes();
  }
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onXDpiChanged() {
  if (Preferences::instance()->getPixelsOnly()) {
    m_xDpiFld->setValue(Stage::standardDpi);
    m_yDpiFld->setValue(Stage::standardDpi);
  } else if (m_fspChk->isChecked())
    m_yDpiFld->setValue(m_xDpiFld->getValue());

  if (m_dotPrev->isChecked()) {
    vComputeLx();
    if (m_arPrev->isChecked()) {
      hComputeLy();
      if (!m_fspChk->isChecked()) computeYDpi();
    } else
      computeAr();
  } else {
    computeXRes();
    computeYRes();
  }
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onYDpiChanged() {
  if (Preferences::instance()->getPixelsOnly()) {
    m_xDpiFld->setValue(Stage::standardDpi);
    m_yDpiFld->setValue(Stage::standardDpi);
  } else if (m_fspChk->isChecked())
    m_xDpiFld->setValue(m_yDpiFld->getValue());

  if (m_dotPrev->isChecked()) {
    vComputeLy();
    if (m_arPrev->isChecked()) {
      hComputeLx();
      if (!m_fspChk->isChecked()) computeXDpi();
    } else
      computeAr();
  } else {
    computeXRes();
    computeYRes();
  }
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onFspChanged(bool checked) {
  if (m_fspChk->isChecked()) {
    if (m_xPrev->isChecked())
      m_yDpiFld->setValue(m_xDpiFld->getValue());
    else
      m_xDpiFld->setValue(m_yDpiFld->getValue());
    if (m_dotPrev->isChecked()) {
      vComputeLx();
      vComputeLy();
      computeAr();
    } else {
      computeXRes();
      computeYRes();
    }
  }
  updatePresetListOm();
  emit changed();
}

void CameraSettingsWidget::onPrevToggled(bool checked) {
  /*--- Changing Prevalences does not affect ForceSquaredPixel option ---*/
}

void CameraSettingsWidget::onPresetSelected(const QString &str) {
  if (str == tr("<custom>") || str.isEmpty()) return;

  QString name, xoffset, yoffset;
  int xres = 0, yres = 0;
  double fx = -1.0, fy = -1.0;
  double ar;

  if (parsePresetString(str, name, xres, yres, fx, fy, xoffset, yoffset, ar,
                        m_forCleanup)) {
    m_xResFld->setValue(xres);
    m_yResFld->setValue(yres);
    m_arFld->setValue(ar, tround(xres), tround(yres));
    m_arValue = ar;

    if (fx > 0.0 && fy > 0.0) {
      m_lxFld->setValue(fx);
      m_lyFld->setValue(fy);
    } else {
      if (m_xPrev->isChecked())
        hComputeLy();
      else
        hComputeLx();
    }

    if (Preferences::instance()->getPixelsOnly()) {
      m_lxFld->setValue(xres / Stage::standardDpi);
      m_lyFld->setValue(yres / Stage::standardDpi);
    }

    if (m_forCleanup && m_offsX && m_offsY && !xoffset.isEmpty() &&
        !yoffset.isEmpty()) {
      m_offsX->setText(xoffset);
      m_offsY->setText(yoffset);
      m_offsX->postSetText();  // calls onEditingFinished()
      m_offsY->postSetText();  // calls onEditingFinished()
    }

    /*--- Use loaded values except DPI ---*/
    computeXDpi();
    computeYDpi();

    if (!areAlmostEqual(static_cast<double>(xres), m_arValue * yres) &&
        m_fspChk->isChecked())
      m_fspChk->setChecked(false);
    emit changed();
  } else {
    QMessageBox::warning(this, tr("Bad camera preset"),
                         tr("'%1' doesn't seem a well formed camera preset. \n"
                            "Possibly the preset file has been corrupted")
                             .arg(str));
  }
}

void CameraSettingsWidget::addPreset() {
  int xRes  = static_cast<int>(m_xResFld->getValue());
  int yRes  = static_cast<int>(m_yResFld->getValue());
  double lx = m_lxFld->getValue();
  double ly = m_lyFld->getValue();
  double ar = m_arFld->getValue();

  QString presetString;
  /*--- For Cleanup camera, also store offset ---*/
  if (m_forCleanup) {
    QString xoffset = m_offsX ? m_offsX->text() : QString("0");
    QString yoffset = m_offsY ? m_offsY->text() : QString("0");

    presetString = QString("%1x%2, %3x%4, %5, %6, %7")
                       .arg(xRes)
                       .arg(yRes)
                       .arg(removeZeros(QString::number(lx)))
                       .arg(removeZeros(QString::number(ly)))
                       .arg(xoffset)
                       .arg(yoffset)
                       .arg(aspectRatioValueToString(ar));
  } else {
    presetString = QString("%1x%2, %3x%4, %5")
                       .arg(xRes)
                       .arg(yRes)
                       .arg(removeZeros(QString::number(lx)))
                       .arg(removeZeros(QString::number(ly)))
                       .arg(aspectRatioValueToString(ar));
  }

  bool ok;
  QString qs;
  while (true) {
    qs = DVGui::getText(tr("Preset name"),
                        tr("Enter the name for %1").arg(presetString), "", &ok);

    if (!ok) return;

    if (qs.contains(','))
      QMessageBox::warning(this, tr("Error : Preset Name is Invalid"),
                           tr("The preset name must not use ','(comma)."));
    else
      break;
  }

  m_presetListOm->addItem(qs + "," + presetString);
  m_presetListOm->blockSignals(true);
  m_presetListOm->setCurrentIndex(m_presetListOm->count() - 1);
  m_presetListOm->blockSignals(false);

  savePresetList();
}

void CameraSettingsWidget::removePreset() {
  int index = m_presetListOm->currentIndex();
  if (index <= 0) return;

  // confirmation dialog
  int ret = DVGui::MsgBox(
      tr("Deleting \"%1\".\nAre you sure?").arg(m_presetListOm->currentText()),
      tr("Delete"), tr("Cancel"));
  if (ret == 0 || ret == 2) return;

  m_presetListOm->removeItem(index);
  m_presetListOm->setCurrentIndex(0);
  savePresetList();
}

// A/R : value => string (e.g. '4/3' or '1.23')
double CameraSettingsWidget::aspectRatioStringToValue(const QString &s) {
  if (s.isEmpty()) {
    return 1;
  }
  int i = s.indexOf('/');
  if (i <= 0 || i + 1 >= s.length()) return s.toDouble();
  int num = s.left(i).toInt();
  int den = s.mid(i + 1).toInt();
  if (den <= 0) den = 1;
  return static_cast<double>(num) / den;
}

// A/R : value => string (e.g. '4/3' or '1.23')
/*--- Allow entering camera pixel values, if value is close to X/Y, display
 * "X/Y" ---*/
QString CameraSettingsWidget::aspectRatioValueToString(double value, int width,
                                                       int height) {
  double v = value;

  if (width != 0 && height != 0) {
    if (areAlmostEqual(value, static_cast<double>(width) / height,
                       1e-3)) /*-- 3-digit error --*/
      return QString("%1/%2").arg(width).arg(height);
  }

  double iv = tround(v);
  if (fabs(iv - v) > 0.01) {
    for (int d = 2; d < 20; ++d) {
      int n = tround(v * d);
      if (fabs(n - v * d) <= 0.01)
        return QString::number(n) + "/" + QString::number(d);
    }
    return QString::number(value, 'f', 5);
  } else {
    return QString::number(static_cast<int>(iv));
  }
}
