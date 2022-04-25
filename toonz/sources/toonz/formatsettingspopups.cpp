

#include "formatsettingspopups.h"

// Tnz6 includes
#include "tapp.h"
#include "dvwidgets.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toutputproperties.h"

#ifdef _WIN32
#include "avicodecrestrictions.h"
#endif

// TnzCore includes
#include "tlevel_io.h"
#include "tproperty.h"
#include "movsettings.h"
#include "timageinfo.h"
#include "tfiletype.h"

// Qt includes
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QGroupBox>

namespace {
bool checkForSeqNum(QString type) {
  TFileType::Type typeInfo = TFileType::getInfoFromExtension(type);
  if ((typeInfo & TFileType::IMAGE) && !(typeInfo & TFileType::LEVEL))
    return true;
  else
    return false;
}
};  // namespace

//**********************************************************************************
//    FormatSettingsPopup  implementation
//**********************************************************************************

FormatSettingsPopup::FormatSettingsPopup(QWidget *parent,
                                         const std::string &format,
                                         TPropertyGroup *props,
                                         TFrameId *tmplFrameId, bool forInput)
    : Dialog(parent)
    , m_format(format)
    , m_props(props)
    , m_levelPath(TFilePath())
#ifdef _WIN32
    , m_codecRestriction(0)
    , m_codecComboBox(0)
    , m_configureCodec(0)
#endif
    , m_sepCharCB(nullptr)
    , m_paddingCB(nullptr)
    , m_tmplFId(tmplFrameId) {
  setWindowTitle(tr("File Settings"));

  setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);

  m_mainLayout = new QGridLayout();
  m_mainLayout->setMargin(0);
  m_mainLayout->setVerticalSpacing(5);
  m_mainLayout->setHorizontalSpacing(5);
  m_mainLayout->setColumnStretch(0, 0);
  m_mainLayout->setColumnStretch(1, 1);

  if (m_props) {
    int i = 0;
    for (i = 0; i < m_props->getPropertyCount(); i++) {
      TProperty *prop = m_props->getProperty(i);
      if (prop && !prop->getVisible()) continue;
      if (dynamic_cast<TEnumProperty *>(prop))
        buildPropertyComboBox(i, m_props);
      else if (dynamic_cast<TIntProperty *>(prop))
        buildValueField(i, m_props);
      else if (dynamic_cast<TDoubleProperty *>(prop))
        buildDoubleField(i, m_props);
      else if (dynamic_cast<TBoolProperty *>(prop))
        buildPropertyCheckBox(i, m_props);
      else if (dynamic_cast<TStringProperty *>(prop))
        buildPropertyLineEdit(i, m_props);
      else
        assert(false);
    }
  }

#ifdef _WIN32
  if (format == "avi") {
    m_codecRestriction = new QLabel(this);
    m_codecRestriction->setMinimumHeight(70);
    m_codecRestriction->setStyleSheet("border: 1px solid rgba(0,0,0,0.5);");
    m_mainLayout->addWidget(m_codecRestriction, m_mainLayout->rowCount(), 0, 1,
                            2);
    m_configureCodec = new QPushButton(tr("Configure Codec"), this);
    m_configureCodec->setObjectName("PushButton_NoPadding");
    m_configureCodec->setFixedSize(100, DVGui::WidgetHeight);
    m_mainLayout->addWidget(m_configureCodec, m_mainLayout->rowCount(), 0, 1,
                            2);
    connect(m_configureCodec, SIGNAL(released()), this,
            SLOT(onAviCodecConfigure()));
  }
#endif

  if (checkForSeqNum(QString::fromStdString(format)) && tmplFrameId) {
    m_sepCharCB = new QComboBox(this);
    m_paddingCB = new QComboBox(this);

    m_sepCharCB->addItem(tr(". (period)"), QChar('.'));
    m_sepCharCB->addItem(tr("_ (underscore)"), QChar('_'));
    m_sepCharCB->setCurrentIndex(m_sepCharCB->findData(
        QChar::fromLatin1(tmplFrameId->getStartSeqInd())));

    m_paddingCB->addItem(tr("No padding"), 0);
    for (int p = 1; p <= 6; p++) m_paddingCB->addItem(QString::number(p), p);
    m_paddingCB->setCurrentIndex(
        m_paddingCB->findData(tmplFrameId->getZeroPadding()));

    int currentRow = m_mainLayout->rowCount();
    if (currentRow > 0)
      m_mainLayout->addItem(new QSpacerItem(15, 15), currentRow, 0);

    QGroupBox *frameFormatGB = new QGroupBox(tr("Frame Number Format"), this);

    frameFormatGB->setObjectName((forInput) ? "FrameFormatBoxInput"
                                            : "FrameFormatBoxOutput");

    QGridLayout *frameFormatLay = new QGridLayout();
    frameFormatLay->setColumnStretch(0, 0);
    frameFormatLay->setColumnStretch(1, 1);
    {
      frameFormatLay->addWidget(new QLabel(tr("Separate Character:"), this), 0,
                                0, Qt::AlignRight);
      frameFormatLay->addWidget(m_sepCharCB, 0, 1, Qt::AlignLeft);

      frameFormatLay->addWidget(new QLabel(tr("Padding:"), this), 1, 0,
                                Qt::AlignRight);
      frameFormatLay->addWidget(m_paddingCB, 1, 1, Qt::AlignLeft);
    }
    frameFormatGB->setLayout(frameFormatLay);

    m_mainLayout->addWidget(frameFormatGB, currentRow + 1, 0, 1, 2);

    connect(m_sepCharCB, SIGNAL(activated(int)), this,
            SLOT(onSepCharCBChanged()));
    connect(m_paddingCB, SIGNAL(activated(int)), this,
            SLOT(onPaddingCBChanged()));
  }

  QPushButton *closeButton = new QPushButton(tr("Close"));
  closeButton->setContentsMargins(4, 4, 4, 4);
  closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  connect(closeButton, SIGNAL(released()), this, SLOT(close()));

  m_mainLayout->addWidget(closeButton, m_mainLayout->rowCount(), 0, 1, 2,
                          Qt::AlignCenter);

  m_topLayout->addLayout(m_mainLayout, 1);
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::setFormatProperties(TPropertyGroup *props) {
  if (props == m_props) return;

  m_props = props;

  for (int i = 0; i < m_props->getPropertyCount(); i++) {
    if (TProperty *prop = m_props->getProperty(i))
      m_widgets[prop->getName()]->setProperty(prop);
  }
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildPropertyComboBox(int index,
                                                TPropertyGroup *props) {
  TEnumProperty *prop = (TEnumProperty *)(props->getProperty(index));

  assert(prop);

  DVGui::PropertyComboBox *comboBox = new DVGui::PropertyComboBox(this, prop);
  m_widgets[prop->getName()]        = comboBox;

#ifdef _WIN32
  connect(comboBox, SIGNAL(currentIndexChanged(const QString &)), this,
          SLOT(onComboBoxIndexChanged(const QString &)));
#endif
  TEnumProperty::Range range = prop->getRange();
  TEnumProperty::Items items = prop->getItems();
  int currIndex              = -1;
  std::wstring defaultVal    = prop->getValue();

  for (int i = 0; i < (int)range.size(); i++) {
    std::wstring nameProp = range[i];

    if (nameProp.find(L"16(GREYTONES)") !=
        -1)  // pezza per il tif: il 16 lo scrive male, e il 48 lo legge male...
      continue;
    /* if (nameProp.find(L"ThunderScan")!=-1) //pezza epr il tif, molte
compressioni non vanno..scive male il file
break;*/

    if (nameProp == defaultVal) currIndex = comboBox->count();
    comboBox->addItem(items[i].UIName, QString::fromStdWString(nameProp));
  }
  if (currIndex >= 0) comboBox->setCurrentIndex(currIndex);

  int row = m_mainLayout->rowCount();
  m_mainLayout->addWidget(new QLabel(prop->getQStringName() + ":", this), row,
                          0, Qt::AlignRight | Qt::AlignVCenter);
  m_mainLayout->addWidget(comboBox, row, 1);

#ifdef _WIN32
  if (m_format == "avi") m_codecComboBox = comboBox;
#endif
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildValueField(int index, TPropertyGroup *props) {
  TIntProperty *prop = (TIntProperty *)(props->getProperty(index));
  assert(prop);

  DVGui::PropertyIntField *v = new DVGui::PropertyIntField(this, prop);
  m_widgets[prop->getName()] = v;

  int row = m_mainLayout->rowCount();
  m_mainLayout->addWidget(new QLabel(prop->getQStringName() + ":", this), row,
                          0, Qt::AlignRight | Qt::AlignVCenter);
  m_mainLayout->addWidget(v, row, 1);
  // get value here - bug loses value if the range doesn't start with 0
  double value = prop->getValue();
  v->setValues(prop->getValue(), prop->getRange().first,
               prop->getRange().second);
  if (prop->getValue() != value) {
    prop->setValue(value);
  }
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildDoubleField(int index, TPropertyGroup *props) {
  TDoubleProperty *prop = (TDoubleProperty *)(props->getProperty(index));
  assert(prop);

  DVGui::PropertyDoubleField *v = new DVGui::PropertyDoubleField(this, prop);
  m_widgets[prop->getName()]    = v;

  int row = m_mainLayout->rowCount();
  m_mainLayout->addWidget(new QLabel(prop->getQStringName() + ":", this), row,
                          0, Qt::AlignRight | Qt::AlignVCenter);
  m_mainLayout->addWidget(v, row, 1);
  // get value here - bug loses value if the range doesn't start with 0
  double value = prop->getValue();
  v->setValues(prop->getValue(), prop->getRange().first,
               prop->getRange().second);
  if (prop->getValue() != value) {
    prop->setValue(value);
  }
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildPropertyCheckBox(int index,
                                                TPropertyGroup *props) {
  TBoolProperty *prop = (TBoolProperty *)(props->getProperty(index));
  assert(prop);

  DVGui::PropertyCheckBox *v =
      new DVGui::PropertyCheckBox(prop->getQStringName(), this, prop);
  m_widgets[prop->getName()] = v;

  m_mainLayout->addWidget(v, m_mainLayout->rowCount(), 1);

  v->setChecked(prop->getValue());
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildPropertyLineEdit(int index,
                                                TPropertyGroup *props) {
  TStringProperty *prop = (TStringProperty *)(props->getProperty(index));
  assert(prop);

  DVGui::PropertyLineEdit *lineEdit = new DVGui::PropertyLineEdit(this, prop);
  m_widgets[prop->getName()]        = lineEdit;
  lineEdit->setText(tr(::to_string(prop->getValue()).c_str()));

  int row = m_mainLayout->rowCount();
  m_mainLayout->addWidget(new QLabel(prop->getQStringName() + ":", this), row,
                          0, Qt::AlignRight | Qt::AlignVCenter);
  m_mainLayout->addWidget(lineEdit, row, 1);
}

//-----------------------------------------------------------------------------

#ifdef _WIN32

void FormatSettingsPopup::onComboBoxIndexChanged(const QString &codecName) {
  if (!m_codecRestriction) return;
  QString msg;
  AviCodecRestrictions::getRestrictions(codecName.toStdWString(), msg);
  m_codecRestriction->setText(msg);
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::onAviCodecConfigure() {
  QString codecName       = m_codecComboBox->currentText();
  std::wstring wCodecName = codecName.toStdWString();
  if (AviCodecRestrictions::canBeConfigured(wCodecName))
    AviCodecRestrictions::openConfiguration(wCodecName, (HWND)winId());
}

#else

void FormatSettingsPopup::onComboBoxIndexChanged(const QString &codecName) {
  // do nothing
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::onAviCodecConfigure() {
  // do nothing
}

#endif

//-----------------------------------------------------------------------------

void FormatSettingsPopup::showEvent(QShowEvent *se) {
#ifdef _WIN32
  if (m_format == "avi") {
    assert(m_codecComboBox);
    m_codecComboBox->blockSignals(true);
    m_codecComboBox->clear();
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    TEnumProperty *eProps =
        dynamic_cast<TEnumProperty *>(m_props->getProperty(0));
    assert(eProps);

    TDimension res(0, 0);
    if (m_levelPath.isEmpty())
      res = scene->getCurrentCamera()->getRes();
    else {
      TLevelReaderP lr(m_levelPath);
      TLevelP level          = lr->loadInfo();
      const TImageInfo *info = lr->getImageInfo(level->begin()->first);
      res.lx                 = info->m_lx;
      res.ly                 = info->m_ly;
    }

    TEnumProperty::Range range = eProps->getRange();
    int currIndex              = -1;
    std::wstring defaultVal    = eProps->getValue();

    QMap<std::wstring, bool> usableCodecs =
        AviCodecRestrictions::getUsableCodecs(res);
    for (int i = 0; i < (int)range.size(); i++) {
      std::wstring nameProp = range[i];
      if (nameProp == L"Uncompressed" ||
          (usableCodecs.contains(nameProp) && usableCodecs[nameProp])) {
        if (nameProp == defaultVal) currIndex = m_codecComboBox->count();
        m_codecComboBox->addItem(QString::fromStdWString(nameProp));
      }
    }
    m_codecComboBox->blockSignals(false);
    if (currIndex >= 0) m_codecComboBox->setCurrentIndex(currIndex);
  }

#endif

  Dialog::showEvent(se);
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::onSepCharCBChanged() {
  assert(m_tmplFId);
  m_tmplFId->setStartSeqInd(m_sepCharCB->currentData().toChar().toLatin1());
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::onPaddingCBChanged() {
  assert(m_tmplFId);
  m_tmplFId->setZeroPadding(m_paddingCB->currentData().toInt());
}

//**********************************************************************************
//    API  functions
//**********************************************************************************

bool openFormatSettingsPopup(QWidget *parent, const std::string &format,
                             TPropertyGroup *props, TFrameId *tmplFId,
                             bool forInput, const TFilePath &levelPath) {
  if (format == "mov" || format == "3gp")  // trattato diversamente; il format
                                           // popup dei mov e' quello di
                                           // quicktime
  {
    // gmt 26/9/07. con la penna capita spesso di premere ripetutamente il
    // bottone.
    // si aprono due popup uno sopra l'altro (brutto di per se e puo' causare
    // dei crash misteriosi)
    static bool popupIsOpen = false;
    if (popupIsOpen) return 0;

    popupIsOpen = true;
    openMovSettingsPopup(props);
    popupIsOpen = false;

    return true;
  }

  if ((!props || props->getPropertyCount() == 0) &&
      !(checkForSeqNum(QString::fromStdString(format)) && tmplFId))
    return false;

  FormatSettingsPopup *popup =
      new FormatSettingsPopup(parent, format, props, tmplFId, forInput);
  popup->setAttribute(Qt::WA_DeleteOnClose);
  popup->setModal(true);

  if (!levelPath.isEmpty()) popup->setLevelPath(levelPath);

  popup->exec();

  return true;
}
