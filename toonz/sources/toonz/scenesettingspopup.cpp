

#include "scenesettingspopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "previewer.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"
#include "toutputproperties.h"

// TnzBase includes
#include "tunit.h"

// TnzCore includes
#include "trop.h"

// Qt includes
#include <QLayout>
#include <QComboBox>
#include <QLabel>
#include <QApplication>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>

using namespace DVGui;

//-----------------------------------------------------------------------------

namespace {

const int labelSize = 110;

//-----------------------------------------------------------------------------

class EditCellMarkUndo final : public TUndo {
  int m_id;
  TSceneProperties::CellMark m_markBefore, m_markAfter;

  EditCellMarkUndo(int id) : m_id(id) {
    m_markBefore = TApp::instance()
                       ->getCurrentScene()
                       ->getScene()
                       ->getProperties()
                       ->getCellMark(id);
  }

public:
  EditCellMarkUndo(int id, TPixel32 color) : EditCellMarkUndo(id) {
    m_markAfter = {m_markBefore.name, color};
  }
  EditCellMarkUndo(int id, QString name) : EditCellMarkUndo(id) {
    m_markAfter = {name, m_markBefore.color};
  }

  void set(const TSceneProperties::CellMark &mark) const {
    TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->setCellMark(mark, m_id);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  }

  void undo() const override { set(m_markBefore); }

  void redo() const override { set(m_markAfter); }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Edit Cell Mark #%1").arg(QString::number(m_id));
  }
};

//-----------------------------------------------------------------------------

class EditColorFilterUndo final : public TUndo {
  int m_id;
  TSceneProperties::ColorFilter m_filterBefore, m_filterAfter;
  ColorFiltersPopup *m_popup;

public:
  EditColorFilterUndo(int id, TPixel32 color, QString name,
                      ColorFiltersPopup *popup)
      : m_id(id), m_popup(popup) {
    m_filterBefore = TApp::instance()
                         ->getCurrentScene()
                         ->getScene()
                         ->getProperties()
                         ->getColorFilter(id);
    m_filterAfter = {name, color};
  }

  void set(const TSceneProperties::ColorFilter &filter) const {
    TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->setColorFilter(filter, m_id);
    m_popup->updateContents();
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  }

  void undo() const override { set(m_filterBefore); }

  void redo() const override { set(m_filterAfter); }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Edit Color Filter #%1").arg(QString::number(m_id));
  }
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
}  //  namespace
//-----------------------------------------------------------------------------

/*
class CheckBoardRect final : public QWidget
{
TRaster32P m_ras;

public:
CheckBoardRect(QWidget *parent, int sizeX, int sizeY)
        : m_ras(sizeX,sizeY)
  {
  setFixedSize(sizeX,sizeY);
  setColors(TPixel32(0, 0, 0), TPixel32(255, 255, 255));
  }
void setColors(const TPixel32 &color1, const TPixel32 &color2)
  {
  TRop::checkBoard(m_ras, color1, color2, TDimensionD(m_ras->getLx()/8,
m_ras->getLy()/8),TPointD(0, 0));
  update();
  }
protected:
void paintEvent(QPaintEvent *event)
  {
        QPainter painter(this);
        QImage img(m_ras->getRawData(), m_ras->getLx(), m_ras->getLy(),
QImage::Format_ARGB32);
  painter.drawImage(0,0,img);
  }
};
*/

//=============================================================================
// CellMarksPopup
//-----------------------------------------------------------------------------

CellMarksPopup::CellMarksPopup(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Cell Marks Settings"));

  QList<TSceneProperties::CellMark> marks = TApp::instance()
                                                ->getCurrentScene()
                                                ->getScene()
                                                ->getProperties()
                                                ->getCellMarks();

  QGridLayout *layout = new QGridLayout();
  layout->setMargin(10);
  layout->setHorizontalSpacing(5);
  layout->setVerticalSpacing(10);
  {
    int id = 0;
    for (auto mark : marks) {
      ColorField *colorF = new ColorField(this, false, mark.color, 20);
      colorF->hideChannelsFields(true);
      QLineEdit *nameF = new QLineEdit(mark.name, this);
      m_fields.append({id, colorF, nameF});

      int row = layout->rowCount();

      layout->addWidget(new QLabel(QString("%1:").arg(id), this), row, 0,
                        Qt::AlignRight | Qt::AlignVCenter);

      layout->addWidget(colorF, row, 1);
      layout->addWidget(nameF, row, 2);

      connect(colorF, SIGNAL(colorChanged(const TPixel32 &, bool)), this,
              SLOT(onColorChanged(const TPixel32 &, bool)));
      connect(nameF, SIGNAL(editingFinished()), this, SLOT(onNameChanged()));
      id++;
    }
  }
  layout->setColumnStretch(2, 1);
  setLayout(layout);
}

void CellMarksPopup::update() {
  QList<TSceneProperties::CellMark> marks = TApp::instance()
                                                ->getCurrentScene()
                                                ->getScene()
                                                ->getProperties()
                                                ->getCellMarks();
  assert(marks.count() == m_fields.count());
  int id = 0;
  for (auto mark : marks) {
    assert(m_fields[id].id == id);
    m_fields[id].colorField->setColor(mark.color);
    m_fields[id].nameField->setText(mark.name);
    id++;
  }
}

void CellMarksPopup::onColorChanged(const TPixel32 &color, bool isDragging) {
  if (isDragging) return;
  // obtain id
  int id             = -1;
  ColorField *colorF = qobject_cast<ColorField *>(sender());
  for (auto field : m_fields) {
    if (field.colorField == colorF) {
      id = field.id;
      break;
    }
  }
  if (id < 0) return;

  // return if the value is unchanged
  TPixel32 oldColor = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getCellMark(id)
                          .color;
  if (color == oldColor) return;

  EditCellMarkUndo *undo = new EditCellMarkUndo(id, color);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

void CellMarksPopup::onNameChanged() {
  // obtain id
  int id           = -1;
  QLineEdit *nameF = qobject_cast<QLineEdit *>(sender());
  for (auto field : m_fields) {
    if (field.nameField == nameF) {
      id = field.id;
      break;
    }
  }
  if (id < 0) return;

  // return if the value is unchanged
  QString oldName = TApp::instance()
                        ->getCurrentScene()
                        ->getScene()
                        ->getProperties()
                        ->getCellMark(id)
                        .name;
  if (nameF->text() == oldName) return;
  // reject empty string
  if (nameF->text().isEmpty()) {
    nameF->setText(oldName);
    return;
  }

  EditCellMarkUndo *undo = new EditCellMarkUndo(id, nameF->text());
  undo->redo();
  TUndoManager::manager()->add(undo);
}

//=============================================================================
// ColorFiltersPopup
//-----------------------------------------------------------------------------

ColorFiltersPopup::ColorFiltersPopup(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Color Filters Settings"));

  QList<TSceneProperties::ColorFilter> filters = TApp::instance()
                                                     ->getCurrentScene()
                                                     ->getScene()
                                                     ->getProperties()
                                                     ->getColorFilters();

  QGridLayout *layout = new QGridLayout();
  layout->setMargin(10);
  layout->setHorizontalSpacing(5);
  layout->setVerticalSpacing(10);
  {
    int id = 0;
    for (auto filter : filters) {
      // skip filter#0 (None)
      if (id == 0) {
        id++;
        continue;
      }
      ColorField *colorF = new ColorField(this, false, filter.color, 20);
      colorF->hideChannelsFields(true);
      QLineEdit *nameF      = new QLineEdit(filter.name, this);
      QPushButton *clearBtn = new QPushButton(this);
      clearBtn->setFixedSize(20, 20);
      clearBtn->setToolTip(tr("Clear"));
      clearBtn->setIcon(createQIcon("delete"));
      clearBtn->setFocusPolicy(Qt::NoFocus);
      clearBtn->setDisabled(filter.name.isEmpty());

      m_fields.insert(id, {colorF, nameF, clearBtn});

      int row = layout->rowCount();

      layout->addWidget(colorF, row, 0);
      layout->addWidget(nameF, row, 1);
      layout->addWidget(clearBtn, row, 2);

      connect(colorF, SIGNAL(colorChanged(const TPixel32 &, bool)), this,
              SLOT(onColorChanged(const TPixel32 &, bool)));
      connect(nameF, SIGNAL(editingFinished()), this, SLOT(onNameChanged()));
      connect(clearBtn, SIGNAL(clicked()), this, SLOT(onClearButtonClicked()));
      id++;
    }
  }
  layout->setColumnStretch(1, 1);
  setLayout(layout);
}

void ColorFiltersPopup::updateContents() {
  QList<TSceneProperties::ColorFilter> filters = TApp::instance()
                                                     ->getCurrentScene()
                                                     ->getScene()
                                                     ->getProperties()
                                                     ->getColorFilters();
  assert(filters.count() == m_fields.count() + 1);
  int id = 0;
  for (const auto &filter : filters) {
    // skip filter#0 (None)
    if (id == 0) {
      id++;
      continue;
    }
    assert(m_fields.contains(id));
    m_fields[id].colorField->setColor(filter.color);
    m_fields[id].nameField->setText(filter.name);
    m_fields[id].clearBtn->setDisabled(filter.name.isEmpty());
    id++;
  }
}

void ColorFiltersPopup::onColorChanged(const TPixel32 &color, bool isDragging) {
  if (isDragging) return;
  // obtain id
  int id             = -1;
  ColorField *colorF = qobject_cast<ColorField *>(sender());
  for (const int keyId : m_fields.keys()) {
    if (m_fields[keyId].colorField == colorF) {
      id = keyId;
      break;
    }
  }
  if (id <= 0) return;

  // return if the value is unchanged
  TSceneProperties::ColorFilter oldCF = TApp::instance()
                                            ->getCurrentScene()
                                            ->getScene()
                                            ->getProperties()
                                            ->getColorFilter(id);
  if (color == oldCF.color) return;

  QString name = oldCF.name;
  if (name.isEmpty()) {
    name = tr("Color Filter %1").arg(id);
    m_fields[id].nameField->setText(name);
    m_fields[id].clearBtn->setEnabled(true);
  }

  EditColorFilterUndo *undo = new EditColorFilterUndo(id, color, name, this);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

void ColorFiltersPopup::onNameChanged() {
  // obtain id
  int id           = -1;
  QLineEdit *nameF = qobject_cast<QLineEdit *>(sender());
  for (const int keyId : m_fields.keys()) {
    if (m_fields[keyId].nameField == nameF) {
      id = keyId;
      break;
    }
  }
  if (id <= 0) return;

  // return if the value is unchanged
  TSceneProperties::ColorFilter oldCF = TApp::instance()
                                            ->getCurrentScene()
                                            ->getScene()
                                            ->getProperties()
                                            ->getColorFilter(id);
  if (nameF->text() == oldCF.name) return;
  // reject empty string
  if (nameF->text().isEmpty()) {
    nameF->setText(oldCF.name);
    return;
  }

  TPixel32 color = oldCF.color;
  if (oldCF.name.isEmpty()) {
    color = TPixel::Red;
    m_fields[id].colorField->setColor(color);
    m_fields[id].clearBtn->setEnabled(true);
  }

  EditColorFilterUndo *undo =
      new EditColorFilterUndo(id, color, nameF->text(), this);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

void ColorFiltersPopup::onClearButtonClicked() {
  // obtain id
  int id                = -1;
  QPushButton *clearBtn = qobject_cast<QPushButton *>(sender());
  for (const int keyId : m_fields.keys()) {
    if (m_fields[keyId].clearBtn == clearBtn) {
      id = keyId;
      break;
    }
  }
  if (id <= 0) return;

  // return if the value is unchanged
  TSceneProperties::ColorFilter oldCF = TApp::instance()
                                            ->getCurrentScene()
                                            ->getScene()
                                            ->getProperties()
                                            ->getColorFilter(id);
  if (oldCF.name.isEmpty()) return;

  m_fields[id].colorField->setColor(TPixel::Black);
  m_fields[id].nameField->setText("");
  m_fields[id].clearBtn->setEnabled(false);

  EditColorFilterUndo *undo =
      new EditColorFilterUndo(id, TPixel::Black, "", this);
  undo->redo();
  TUndoManager::manager()->add(undo);
}
//=============================================================================
// SceneSettingsPopup
//-----------------------------------------------------------------------------

SceneSettingsPopup::SceneSettingsPopup()
    : QDialog(TApp::instance()->getMainWindow())
    , m_cellMarksPopup(nullptr)
    , m_colorFiltersPopup(nullptr) {
  setWindowTitle(tr("Scene Settings"));
  setObjectName("SceneSettings");
  TSceneProperties *sprop = getProperties();

  // Frame Rate
  double frameRate = sprop->getOutputProperties()->getFrameRate();
  m_frameRateFld   = new DoubleLineEdit(this, frameRate);
  m_frameRateFld->setRange(1, 100);
  m_frameRateFld->setDecimals(2);

  // Camera BG color
  m_bgColorFld = new ColorField(this, true, sprop->getBgColor());

  // Field Guide Size - A/R
  int fieldGuideSize = sprop->getFieldGuideSize();
  m_fieldGuideFld    = new DVGui::IntLineEdit(this, fieldGuideSize, 0, 50);
  m_aspectRatioFld   = new DoubleLineEdit(this, 1.38);
  m_aspectRatioFld->setRange(-10000.0, 10000.0);
  m_aspectRatioFld->setDecimals(5);

  // Image Subsampling  - Tlv Subsampling
  int fullcolorSubsampling = sprop->getFullcolorSubsampling();
  m_fullcolorSubsamplingFld =
      new DVGui::IntLineEdit(this, fullcolorSubsampling, 1);

  int tlvSubsampling  = sprop->getTlvSubsampling();
  m_tlvSubsamplingFld = new DVGui::IntLineEdit(this, tlvSubsampling, 1);

  // Marker Interval - Start Frame
  int distance, offset, secDistance;
  sprop->getMarkers(distance, offset, secDistance);
  m_markerIntervalFld = new DVGui::IntLineEdit(this, distance, 0);
  m_startFrameFld     = new DVGui::IntLineEdit(this, offset);

  // Whether the column color filter and transparency is available also in
  // render
  m_colorFilterOnRenderCB = new DVGui::CheckBox(
      tr("Enable Column Color Filter and Transparency for Rendering"), this);
  m_colorFilterOnRenderCB->setChecked(
      sprop->isColumnColorFilterOnRenderEnabled());

  QPushButton *editCellMarksButton =
      new QPushButton(tr("Edit Cell Marks"), this);

  QPushButton *editColorFiltersButton =
      new QPushButton(tr("Edit Column Color Filters"), this);

  // layout
  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setMargin(10);
  mainLayout->setHorizontalSpacing(5);
  mainLayout->setVerticalSpacing(15);
  {
    // Frame Rate
    mainLayout->addWidget(new QLabel(tr("Frame Rate:"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_frameRateFld, 0, 1, 1, 4,
                          Qt::AlignLeft | Qt::AlignVCenter);
    // Camera BG color
    mainLayout->addWidget(new QLabel(tr("Camera BG Color:"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_bgColorFld, 1, 1, 1, 4);
    // Field Guide Size - A/R
    mainLayout->addWidget(new QLabel(tr("Field Guide Size:"), this), 2, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_fieldGuideFld, 2, 1);
    mainLayout->addWidget(new QLabel(tr("A/R:"), this), 2, 2,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_aspectRatioFld, 2, 3, 1, 2,
                          Qt::AlignLeft | Qt::AlignVCenter);
    // Image Subsampling  - Tlv Subsampling
    mainLayout->addWidget(new QLabel(tr("Image Subsampling:"), this), 3, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_fullcolorSubsamplingFld, 3, 1);
    if (m_tlvSubsamplingFld) {
      mainLayout->addWidget(new QLabel(tr("TLV Subsampling:"), this), 4, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      mainLayout->addWidget(m_tlvSubsamplingFld, 4, 1);
    }
    // Marker Interval - Start Frame
    mainLayout->addWidget(new QLabel(tr("Marker Interval:"), this), 5, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_markerIntervalFld, 5, 1);
    mainLayout->addWidget(new QLabel(tr("  Start Frame:"), this), 5, 2,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_startFrameFld, 5, 3);

    // Use Color Filter and Transparency for Rendering
    mainLayout->addWidget(m_colorFilterOnRenderCB, 6, 0, 1, 4);
    mainLayout->addWidget(editColorFiltersButton, 6, 4,
                          Qt::AlignRight | Qt::AlignVCenter);

    // cell marks
    mainLayout->addWidget(new QLabel(tr("Cell Marks:"), this), 7, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(editCellMarksButton, 7, 1, 1, 4,
                          Qt::AlignLeft | Qt::AlignVCenter);
  }
  mainLayout->setColumnStretch(0, 0);
  mainLayout->setColumnStretch(1, 0);
  mainLayout->setColumnStretch(2, 0);
  mainLayout->setColumnStretch(3, 0);
  mainLayout->setColumnStretch(4, 1);
  mainLayout->setRowStretch(9, 1);
  setLayout(mainLayout);

  // signal-slot connections
  bool ret = true;
  // Frame Rate
  ret = ret && connect(m_frameRateFld, SIGNAL(editingFinished()), this,
                       SLOT(onFrameRateEditingFinished()));
  // Camera BG color
  ret =
      ret && connect(m_bgColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                     this, SLOT(setBgColor(const TPixel32 &, bool)));
  // Field Guide Size - A/R
  ret = ret && connect(m_fieldGuideFld, SIGNAL(editingFinished()), this,
                       SLOT(onFieldGuideSizeEditingFinished()));
  ret = ret && connect(m_aspectRatioFld, SIGNAL(editingFinished()), this,
                       SLOT(onFieldGuideAspectRatioEditingFinished()));
  // Image Subsampling  - Tlv Subsampling
  ret = ret && connect(m_fullcolorSubsamplingFld, SIGNAL(editingFinished()),
                       this, SLOT(onFullColorSubsampEditingFinished()));
  if (m_tlvSubsamplingFld)
    ret = ret && connect(m_tlvSubsamplingFld, SIGNAL(editingFinished()), this,
                         SLOT(onTlvSubsampEditingFinished()));
  // Marker Interval - Start Frame
  ret = ret && connect(m_markerIntervalFld, SIGNAL(editingFinished()), this,
                       SLOT(onMakerInformationChanged()));
  ret = ret && connect(m_startFrameFld, SIGNAL(editingFinished()), this,
                       SLOT(onMakerInformationChanged()));

  // Use Color Filter and Transparency for Rendering
  ret = ret && connect(m_colorFilterOnRenderCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onColorFilterOnRenderChanged()));
  ret = ret && connect(editColorFiltersButton, SIGNAL(clicked()), this,
                       SLOT(onEditColorFiltersButtonClicked()));

  // Cell Marks
  ret = ret && connect(editCellMarksButton, SIGNAL(clicked()), this,
                       SLOT(onEditCellMarksButtonClicked()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::showEvent(QShowEvent *e) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneChanged()), SLOT(update()));
  connect(sceneHandle, SIGNAL(sceneSwitched()), SLOT(update()));

  update();
}

void SceneSettingsPopup::hideEvent(QHideEvent *e) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(update()));
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(update()));
}

//-----------------------------------------------------------------------------

TSceneProperties *SceneSettingsPopup::getProperties() const {
  return TApp::instance()->getCurrentScene()->getScene()->getProperties();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::update() {
  TSceneProperties *sprop = getProperties();

  QString str;
  m_frameRateFld->setValue(sprop->getOutputProperties()->getFrameRate());

  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  m_bgColorFld->setChessboardColors(col1, col2);

  TPixel bgColor = sprop->getBgColor();
  m_bgColorFld->setColor(bgColor);

  m_fieldGuideFld->setValue(sprop->getFieldGuideSize());
  m_aspectRatioFld->setValue(sprop->getFieldGuideAspectRatio());

  UnitParameters::setFieldGuideAspectRatio(sprop->getFieldGuideAspectRatio());
  m_fullcolorSubsamplingFld->setValue(sprop->getFullcolorSubsampling());
  if (m_tlvSubsamplingFld)
    m_tlvSubsamplingFld->setValue(sprop->getTlvSubsampling());
  int markerDistance = 0, markerOffset = 0, secDistance;
  sprop->getMarkers(markerDistance, markerOffset, secDistance);
  m_markerIntervalFld->setValue(markerDistance);
  m_startFrameFld->setValue(markerOffset + 1);
  m_colorFilterOnRenderCB->setChecked(
      sprop->isColumnColorFilterOnRenderEnabled());

  if (m_cellMarksPopup) m_cellMarksPopup->update();
  if (m_colorFiltersPopup) m_colorFiltersPopup->updateContents();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFrameRateEditingFinished() {
  TSceneProperties *sprop  = getProperties();
  double frameRate         = sprop->getOutputProperties()->getFrameRate();
  double frameRateFldValue = m_frameRateFld->getValue();
  if (frameRate == frameRateFldValue) return;
  sprop->getOutputProperties()->setFrameRate(frameRateFldValue);
  TApp::instance()->getCurrentScene()->getScene()->updateSoundColumnFrameRate();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFieldGuideSizeEditingFinished() {
  TSceneProperties *sprop = getProperties();
  int fieldGuideSize      = sprop->getFieldGuideSize();
  int fieldGuideSizefld   = m_fieldGuideFld->text().toInt();
  if (fieldGuideSize == fieldGuideSizefld) return;
  sprop->setFieldGuideSize(fieldGuideSizefld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFieldGuideAspectRatioEditingFinished() {
  TSceneProperties *sprop      = getProperties();
  double fieldGuideAspectRatio = sprop->getFieldGuideAspectRatio();
  double aspectRatioFld        = m_aspectRatioFld->text().toDouble();
  if (fieldGuideAspectRatio == aspectRatioFld) return;
  sprop->setFieldGuideAspectRatio(aspectRatioFld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  UnitParameters::setFieldGuideAspectRatio(m_aspectRatioFld->text().toDouble());
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onFullColorSubsampEditingFinished() {
  TSceneProperties *sprop     = getProperties();
  int fullcolorSubsampling    = sprop->getFullcolorSubsampling();
  int fullcolorSubsamplingFld = m_fullcolorSubsamplingFld->getValue();
  if (fullcolorSubsampling == fullcolorSubsamplingFld) return;
  sprop->setFullcolorSubsampling(fullcolorSubsamplingFld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onTlvSubsampEditingFinished() {
  if (!m_tlvSubsamplingFld) return;
  TSceneProperties *sprop = getProperties();
  int tlvSubsampling      = sprop->getTlvSubsampling();
  int tlvSubsamplingFld   = m_tlvSubsamplingFld->getValue();
  if (tlvSubsamplingFld == tlvSubsampling) return;
  sprop->setTlvSubsampling(tlvSubsamplingFld);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onMakerInformationChanged() {
  TSceneProperties *sprop = getProperties();
  int distance, offset, secDistance;
  sprop->getMarkers(distance, offset, secDistance);
  int markerDistance = m_markerIntervalFld->text().toInt();
  int markerOffset   = m_startFrameFld->text().toInt() - 1;

  if (distance == markerDistance && offset == markerOffset) return;
  sprop->setMarkers(markerDistance, markerOffset);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::setBgColor(const TPixel32 &bgColor, bool isDragging) {
  TSceneProperties *sprop = getProperties();
  sprop->setBgColor(bgColor);
  // TODO: forse sarebbe meglio usare una notifica piu' specifica
  if (!isDragging) Previewer::clearAll();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onColorFilterOnRenderChanged() {
  TSceneProperties *sprop = getProperties();
  sprop->enableColumnColorFilterOnRender(m_colorFilterOnRenderCB->isChecked());
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void SceneSettingsPopup::onEditColorFiltersButtonClicked() {
  if (!m_colorFiltersPopup) m_colorFiltersPopup = new ColorFiltersPopup(this);
  m_colorFiltersPopup->show();
  m_colorFiltersPopup->raise();
}
//-----------------------------------------------------------------------------

void SceneSettingsPopup::onEditCellMarksButtonClicked() {
  if (!m_cellMarksPopup) m_cellMarksPopup = new CellMarksPopup(this);
  m_cellMarksPopup->show();
  m_cellMarksPopup->raise();
}

//=============================================================================

OpenPopupCommandHandler<SceneSettingsPopup> openSceneSettingsPopup(
    MI_SceneSettings);
