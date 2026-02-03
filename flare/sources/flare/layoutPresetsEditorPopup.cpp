#include "layoutPresetsEditorPopup.h"

#include <QListWidget>
#include <QLineEdit>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QListWidgetItem>
#include <qpointer.h>

#include "toonzqt/filefield.h"
#include "toonzqt/doublefield.h"
#include "tenv.h"
#include "layoutUtils.h"
#include "tapp.h"
#include "tsystem.h"
#include <QtGlobal>
#include <algorithm>
#include "toonzqt/colorfield.h"
#include "tpixel.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "viewerdraw.h"
#include "toonz/toonzfolders.h"
#include <toonzqt/dvdialog.h>

//=============================================================================
LayoutPresetsEditorPopup* LayoutPresetsEditorPopup::s_instance = nullptr;
//=============================================================================
LayoutPresetsEditorPopup* LayoutPresetsEditorPopup::instance(QWidget* parent) {
  if (!s_instance) {
    s_instance = new LayoutPresetsEditorPopup(parent);
  }
  return s_instance;
}

void LayoutPresetsEditorPopup::showAndPosition(const QPoint& globalPos) {
  adjustSize();
  QSize popupSize = size();

  int x = globalPos.x() - popupSize.width() / 2;
  int y = globalPos.y() - 10;

  move(x, y);
  show();
  raise();
  activateWindow();
}

LayoutPresetsEditorPopup::LayoutPresetsEditorPopup(QWidget* parent)
    : DVGui::Dialog(parent, true, false, "LayoutPresetsEditor") {
  setWindowTitle(tr("Layout Presets Editor"));

  setAttribute(Qt::WA_DeleteOnClose);
  setModal(false);
  connect(this, &QObject::destroyed,
          []() { LayoutPresetsEditorPopup::s_instance = nullptr; });

  // Layout
  QHBoxLayout* mainLayout = new QHBoxLayout;
  m_topLayout->addLayout(mainLayout);

  // Left Panel (Preset List)
  QVBoxLayout* leftLayout      = new QVBoxLayout;
  m_presetList                 = new QListWidget(this);
  QHBoxLayout* presetBtnLayout = new QHBoxLayout;
  // Add / Remove
  QPushButton* addPresetBtn    = new QPushButton(tr("+"), this);
  QPushButton* removePresetBtn = new QPushButton(tr("-"), this);

  // Move Up / Move Down
  QPushButton* moveUpBtn   = new QPushButton(tr("Move Up"), this);
  QPushButton* moveDownBtn = new QPushButton(tr("Move Down"), this);

  presetBtnLayout->addWidget(addPresetBtn);
  presetBtnLayout->addWidget(removePresetBtn);
  presetBtnLayout->addWidget(moveUpBtn);
  presetBtnLayout->addWidget(moveDownBtn);

  leftLayout->addWidget(new QLabel(tr("Presets"), this));
  leftLayout->addWidget(m_presetList);
  leftLayout->addLayout(presetBtnLayout);

  // Right Panel (Preset Editor)
  m_editorGroup             = new QGroupBox(tr("Edit Preset"), this);
  QGridLayout* editorLayout = new QGridLayout;

  m_nameEdit = new QLineEdit(this);
  editorLayout->addWidget(new QLabel(tr("Name:"), this), 0, 0);
  editorLayout->addWidget(m_nameEdit, 0, 1, 1, 3);

  m_layoutField = new DVGui::FileField(this);
  m_layoutField->setFileMode(QFileDialog::ExistingFile);
  m_layoutField->setFilters(QStringList() << "png" << "tlv" << "pli"
                                          << "gif" << "webp" << "bmp"
                                          << "tiff" << "tga");
  editorLayout->addWidget(new QLabel(tr("Layout Template:"), this), 1, 0);
  editorLayout->addWidget(m_layoutField, 1, 1, 1, 3);

  m_offsetXEdit = new DVGui::DoubleLineEdit(this);
  m_offsetYEdit = new DVGui::DoubleLineEdit(this);
  editorLayout->addWidget(new QLabel(tr("Offset X(Pixel):"), this), 2, 0);
  editorLayout->addWidget(m_offsetXEdit, 2, 1);
  editorLayout->addWidget(new QLabel(tr("Y(Pixel):"), this), 2, 2);
  editorLayout->addWidget(m_offsetYEdit, 2, 3);

  editorLayout->addWidget(new QLabel(tr("Areas:"), this), 3, 0, Qt::AlignTop);
  m_areaList = new QListWidget(this);
  m_areaList->setMinimumHeight(100);
  editorLayout->addWidget(m_areaList, 3, 1, 1, 3);

  // Area Buttons
  QHBoxLayout* areaBtnLayout = new QHBoxLayout;
  m_addAreaBtn               = new QPushButton(tr("Add Area"), this);
  m_removeAreaBtn            = new QPushButton(tr("Remove Area"), this);
  areaBtnLayout->addWidget(m_addAreaBtn);
  areaBtnLayout->addWidget(m_removeAreaBtn);
  editorLayout->addLayout(areaBtnLayout, 4, 1, 1, 3);

  // ** Area Details Editor Group **
  m_areaEditorGroup             = new QGroupBox(tr("Edit Selected Area"), this);
  QGridLayout* areaEditorLayout = new QGridLayout;

  // Width/Height
  m_widthEdit = new DVGui::DoubleLineEdit(this);
  m_widthEdit->setRange(0.0, 1000000.0);
  m_widthEdit->setDecimals(2);
  m_heightEdit = new DVGui::DoubleLineEdit(this);
  m_heightEdit->setRange(0.0, 1000000.0);
  m_heightEdit->setDecimals(2);

  areaEditorLayout->addWidget(new QLabel(tr("Width (%):"), this), 0, 0);
  areaEditorLayout->addWidget(m_widthEdit, 0, 1);
  areaEditorLayout->addWidget(new QLabel(tr("Height (%):"), this), 0, 2);
  areaEditorLayout->addWidget(m_heightEdit, 0, 3);

  m_colorField =
      new DVGui::ColorField(this, false, TPixel32(255, 0, 0), 40, false);
  m_colorField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  areaEditorLayout->addWidget(new QLabel(tr("Color:"), this), 1, 0,
                              Qt::AlignTop);
  areaEditorLayout->addWidget(m_colorField, 1, 1, 2, 3);

  m_areaEditorGroup->setLayout(areaEditorLayout);
  editorLayout->addWidget(m_areaEditorGroup, 5, 0, 1, 4);

  m_editorGroup->setLayout(editorLayout);

  m_areaEditorGroup->setLayout(areaEditorLayout);
  editorLayout->addWidget(m_areaEditorGroup, 5, 0, 1, 4);

  m_editorGroup->setLayout(editorLayout);

  // Main Layout Assembly
  mainLayout->addLayout(leftLayout, 0);
  mainLayout->addWidget(m_editorGroup, 1);

  // footer buttons
  QPushButton* okBtn     = new QPushButton(tr("Save and Close"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Revert and Close"), this);
  addButtonBarWidget(okBtn, cancelBtn);

  // connections
  connect(okBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::accept);
  connect(cancelBtn, &QPushButton::clicked, this, [&]() {
    m_needSaveIni = false;
    reject();
  });

  // Preset List
  connect(m_presetList, &QListWidget::currentRowChanged, this,
          &LayoutPresetsEditorPopup::onPresetSelected);
  connect(m_areaList, &QListWidget::currentRowChanged, this,
          &LayoutPresetsEditorPopup::onAreaSelected);

  // Buttons
  connect(addPresetBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::onAddPreset);
  connect(removePresetBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::onRemovePreset);
  connect(moveUpBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::onMovePresetUp);
  connect(moveDownBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::onMovePresetDown);

  connect(m_addAreaBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::onAddArea);
  connect(m_removeAreaBtn, &QPushButton::clicked, this,
          &LayoutPresetsEditorPopup::onRemoveArea);

  // Preset field
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          &LayoutPresetsEditorPopup::onNameChanged);
  connect(m_layoutField, &DVGui::FileField::pathChanged, this,
          &LayoutPresetsEditorPopup::onLayoutPathChanged);

  // Offset fields
  connect(m_offsetXEdit, &DVGui::DoubleLineEdit::valueChanged, this,
          &LayoutPresetsEditorPopup::onOffsetXChanged);
  connect(m_offsetYEdit, &DVGui::DoubleLineEdit::valueChanged, this,
          &LayoutPresetsEditorPopup::onOffsetYChanged);

  // Area fields
  connect(m_widthEdit, &DVGui::DoubleLineEdit::valueChanged, this,
          &LayoutPresetsEditorPopup::onAreaParamChanged);
  connect(m_heightEdit, &DVGui::DoubleLineEdit::valueChanged, this,
          &LayoutPresetsEditorPopup::onAreaParamChanged);
  connect(m_colorField, &DVGui::ColorField::colorChanged, this,
          &LayoutPresetsEditorPopup::onAreaParamChanged);

  // Initialization
  m_oldLayoutName = getCurrentLayoutPresetName();

  m_iniFilePath = getLayoutIniPath();
  loadIniFile();

  updatePresetList();

  m_editorGroup->setEnabled(false);
  m_areaEditorGroup->setEnabled(false);
}

void LayoutPresetsEditorPopup::loadIniFile() {
  m_presets.clear();
  if (!TFileStatus(m_iniFilePath).doesExist()) return;

  QSettings settings(m_iniFilePath.getQString(), QSettings::IniFormat);
  QStringList groups = settings.childGroups();
  std::sort(groups.begin(), groups.end());

  for (const QString& group : groups) {
    if (!group.startsWith("Layout") && !group.startsWith("SafeArea")) continue;

    settings.beginGroup(group);
    LayoutPreset preset;
    preset.m_name       = settings.value("name", "Untitled").toString();
    preset.m_layoutPath = settings.value("layout", "").toString();

    QList<QVariant> offset =
        settings.value("layoutOffset", QList<QVariant>()).toList();
    if (offset.size() >= 2) {
      preset.m_layoutOffsetX = offset[0].toDouble();
      preset.m_layoutOffsetY = offset[1].toDouble();
    }

    settings.beginGroup("area");
    QStringList areaKeys = settings.childKeys();
    std::sort(areaKeys.begin(), areaKeys.end(),
              [](const QString& a, const QString& b) {
                return a.toInt() < b.toInt();
              });

    for (const QString& key : areaKeys) {
      preset.m_areas.append(FramesDef::fromVariantList(
          settings.value(key, QList<QVariant>()).toList()));
    }
    settings.endGroup();  // area

    m_presets.append(preset);
    settings.endGroup();  // Layout X
  }
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::saveIniFile() {
  QSettings settings(m_iniFilePath.getQString(), QSettings::IniFormat);
  settings.clear();

  for (int i = 0; i < m_presets.size(); ++i) {
    const LayoutPreset& preset = m_presets.at(i);
    if (m_iniFilePath.withoutParentDir() == TFilePath("safearea.ini"))
      settings.beginGroup(QString("SafeArea%1").arg(i));
    else
      settings.beginGroup(QString("Layout%1").arg(i));

    settings.setValue("name", preset.m_name);
    if (!preset.m_layoutPath.isEmpty()) {
      settings.setValue("layout", preset.m_layoutPath);
      QList<QVariant> offset;
      offset << preset.m_layoutOffsetX << preset.m_layoutOffsetY;
      settings.setValue("layoutOffset", offset);
    }

    settings.beginGroup("area");
    for (int j = 0; j < preset.m_areas.size(); ++j) {
      settings.setValue(QString::number(j),
                        preset.m_areas.at(j).toVariantList());
    }
    settings.endGroup();  // area

    settings.endGroup();  // LayoutX
  }
  settings.sync();
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::updatePresetList() {
  m_presetList->blockSignals(true);
  bool oldPresetFound = false;

  m_presetList->clear();
  for (const LayoutPreset& preset : m_presets) {
    m_presetList->addItem(preset.m_name);

    if (preset.m_name == m_oldLayoutName) {
      oldPresetFound = true;
    }
  }

  if (!oldPresetFound && !m_oldLayoutName.isEmpty()) {
    QString warningText =
        tr("Missing: %1 (Click to Create)").arg(m_oldLayoutName);

    QListWidgetItem* warningItem = new QListWidgetItem(warningText);
    m_presetList->addItem(warningItem);

    warningItem->setForeground(Qt::red);

    // Click to create new preset based on the missing name
    warningItem->setData(Qt::UserRole, m_oldLayoutName);
  }
  m_presetList->blockSignals(false);
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::updateEditorWidgets() {
  int idx = m_presetList->currentRow();
  if (idx < 0 || idx >= m_presets.size()) return;

  const LayoutPreset& preset = m_presets.at(idx);

  m_nameEdit->blockSignals(true);
  m_layoutField->blockSignals(true);
  m_offsetXEdit->blockSignals(true);
  m_offsetYEdit->blockSignals(true);

  m_nameEdit->setText(preset.m_name);
  if (preset.m_layoutPath.isEmpty()) {
    const QString loPath =
        (ToonzFolder::getLibraryFolder() + TFilePath("layouts")).getQString();
    m_layoutField->setPath(loPath);
    m_layoutField->getField()->clear();
  } else
    m_layoutField->setPath(preset.m_layoutPath);

  m_offsetXEdit->setValue(preset.m_layoutOffsetX);
  m_offsetYEdit->setValue(preset.m_layoutOffsetY);

  m_nameEdit->blockSignals(false);
  m_layoutField->blockSignals(false);
  m_offsetXEdit->blockSignals(false);
  m_offsetYEdit->blockSignals(false);
}

QString LayoutPresetsEditorPopup::getCurrentLayoutPresetName() const {
  return TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getProperties()
      ->getLayoutPresetName();
}

void LayoutPresetsEditorPopup::setSceneLayoutPresetName(const QString& name) {
  TSceneProperties* sprop =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  sprop->setLayoutPresetName(name);
  TApp::instance()->getCurrentScene()->notifySceneChanged(true);
}

void LayoutPresetsEditorPopup::triggerPreview() {
  QString nameToApply;
  static int count = 0;
  if (count > 1000) count = 0;
  int presetIdx = m_presetList->currentRow();
  if (presetIdx >= 0 && presetIdx < m_presets.size()) {
    layoutUtils::refreshCacheWithPreset(m_presets.at(presetIdx));
    TApp::instance()->getCurrentScene()->notifySceneChanged(false);
    m_previewActive = true;
  } else {
    layoutUtils::invalidateCurrent();
    TApp::instance()->getCurrentScene()->notifySceneChanged(false);
    m_previewActive = false;
  }
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::updateAreaList() {
  int presetIdx = m_presetList->currentRow();

  m_areaList->blockSignals(true);
  m_areaList->clear();

  if (presetIdx >= 0 && presetIdx < m_presets.size()) {
    const LayoutPreset& preset = m_presets.at(presetIdx);
    for (const FramesDef& def : preset.m_areas) {
      m_areaList->addItem(def.toString());
    }
  }

  m_areaList->blockSignals(false);
  m_areaEditorGroup->setEnabled(false);
}

void LayoutPresetsEditorPopup::accept() {
  saveIniFile();
  if (m_previewActive) {
    layoutUtils::invalidateCurrent();
    m_previewActive = false;
  }

  int presetIdx = m_presetList->currentRow();
  if (presetIdx >= 0 && presetIdx < m_presets.size()) {
    setSceneLayoutPresetName(m_presets.at(presetIdx).m_name);
  }

  QDialog::accept();
}
namespace {
int askSaveQuestion() {
  QString question =
      QObject::tr("The layout preset has been modified.\n\nSave changes?");
  QString Save    = QObject::tr("Save");
  QString Discard = QObject::tr("Don't Save");
  QString Cancel  = QObject::tr("Cancel");

  return DVGui::MsgBox(question, Save, Discard, Cancel, 0);
}
}  // namespace

void LayoutPresetsEditorPopup::reject() {
  if (m_needSaveIni) switch (askSaveQuestion()) {
    case 1:  // Save
      accept();
      return;
    case 2:  // Discard
      break;
    case 3:  // Cancel
    default:
      return;
    }
  layoutUtils::invalidateCurrent();
  m_previewActive = false;
  TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  QDialog::reject();
}

void LayoutPresetsEditorPopup::closeEvent(QCloseEvent* e) {
  if (m_needSaveIni) switch (askSaveQuestion()) {
    case 1:  // Save
      accept();
      break;
    case 2:  // Discard
      m_needSaveIni = false;
      reject();
      break;
    case 3:  // Cancel
    default:
      e->ignore();
      return;
    }
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onPresetSelected() {
  int idx = m_presetList->currentRow();

  // If is missing preset (red warning item)
  if (idx == m_presets.size() && m_presetList->count() > m_presets.size()) {
    QListWidgetItem* currentItem = m_presetList->item(idx);

    QString missingName = currentItem->data(Qt::UserRole).toString();

    m_presetList->blockSignals(true);

    LayoutPreset newPreset;
    newPreset.m_name = missingName;
    newPreset.m_areas.append(FramesDef::fromString("80.0, 80.0"));

    m_presets.append(newPreset);

    // Remove the red warning item
    updatePresetList();

    int newIdx = m_presets.size() - 1;
    m_presetList->setCurrentRow(newIdx);

    m_presetList->blockSignals(false);
    return;
  }
  if (idx < 0 || idx >= m_presets.size()) {
    m_editorGroup->setEnabled(false);
    m_areaList->setCurrentRow(-1);
    return;
  }

  m_editorGroup->setEnabled(true);
  updateEditorWidgets();
  updateAreaList();

  if (m_areaList->count() > 0) {
    m_areaList->setCurrentRow(0);
  } else {
    m_areaEditorGroup->setEnabled(false);
  }
  triggerPreview();
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onAreaSelected() {
  int presetIdx = m_presetList->currentRow();
  int areaIdx   = m_areaList->currentRow();

  m_areaEditorGroup->setEnabled(false);

  if (presetIdx < 0 || areaIdx < 0 || presetIdx >= m_presets.size() ||
      areaIdx >= m_presets.at(presetIdx).m_areas.size()) {
    return;
  }

  m_areaEditorGroup->setEnabled(true);

  const FramesDef& def        = m_presets.at(presetIdx).m_areas[areaIdx];
  const QList<double>& params = def.m_params;

  m_widthEdit->blockSignals(true);
  m_heightEdit->blockSignals(true);
  m_colorField->blockSignals(true);

  m_widthEdit->setValue(params.size() > 0 ? params[0] : 0.0);
  m_heightEdit->setValue(params.size() > 1 ? params[1] : 0.0);

  TPixel32 color;
  if (params.size() >= 5) {
    color.r = (int)params[2];
    color.g = (int)params[3];
    color.b = (int)params[4];
  } else {
    color.r = 255;
    color.g = 0;
    color.b = 0;
  }
  m_colorField->setColor(color);

  m_widthEdit->blockSignals(false);
  m_heightEdit->blockSignals(false);
  m_colorField->blockSignals(false);
}
//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onAddPreset() {
  LayoutPreset newPreset;

  newPreset.m_name = tr("New Preset");

  newPreset.m_areas.append(FramesDef::fromString("80.0, 80.0"));

  m_presets.append(newPreset);

  updatePresetList();

  m_presetList->setCurrentRow(m_presets.size() - 1);
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onRemovePreset() {
  int idx = m_presetList->currentRow();
  if (idx < 0 || idx >= m_presets.size()) return;

  m_presets.removeAt(idx);
  updatePresetList();

  m_editorGroup->setEnabled(false);
  m_areaList->clear();
  m_areaEditorGroup->setEnabled(false);
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onMovePresetUp() {
  int idx = m_presetList->currentRow();

  if (idx <= 0 || idx >= m_presets.size()) return;

  m_presets.swapItemsAt(idx, idx - 1);

  updatePresetList();
  m_presetList->setCurrentRow(idx - 1);

  triggerPreview();
  m_needSaveIni = true;
}
//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onMovePresetDown() {
  int idx = m_presetList->currentRow();

  if (idx < 0 || idx >= m_presets.size() - 1) return;

  m_presets.swapItemsAt(idx, idx + 1);

  updatePresetList();
  m_presetList->setCurrentRow(idx + 1);

  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onAddArea() {
  int presetIdx = m_presetList->currentRow();
  if (presetIdx < 0 || presetIdx >= m_presets.size()) return;

  m_presets[presetIdx].m_areas.append(FramesDef::fromString("90.0, 90.0"));
  updateAreaList();

  m_areaList->setCurrentRow(m_areaList->count() - 1);
  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onRemoveArea() {
  int presetIdx = m_presetList->currentRow();
  int areaIdx   = m_areaList->currentRow();
  if (presetIdx < 0 || areaIdx < 0 || presetIdx >= m_presets.size() ||
      areaIdx >= m_presets.at(presetIdx).m_areas.size())
    return;

  m_presets[presetIdx].m_areas.removeAt(areaIdx);
  updateAreaList();
  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onNameChanged(const QString& text) {
  int idx = m_presetList->currentRow();
  if (idx < 0 || idx >= m_presets.size()) return;

  m_presets[idx].m_name = text;
  m_presetList->item(idx)->setText(text);
  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onLayoutPathChanged() {
  int idx = m_presetList->currentRow();
  if (idx < 0 || idx >= m_presets.size()) return;
  const TFilePath loPath =
      ToonzFolder::getLibraryFolder() + TFilePath("layouts");
  const TFilePath curPath(m_layoutField->getPath());
  if (curPath.getParentDir() == loPath) {
    QString path                = curPath.withoutParentDir().getQString();
    m_presets[idx].m_layoutPath = path;
    m_layoutField->blockSignals(true);
    m_layoutField->setPath(path);
    m_layoutField->blockSignals(false);
  } else {
    m_presets[idx].m_layoutPath = curPath.getQString();
  }
  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onOffsetXChanged() {
  int idx = m_presetList->currentRow();
  if (idx < 0 || idx >= m_presets.size()) return;

  m_presets[idx].m_layoutOffsetX = m_offsetXEdit->getValue();
  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

void LayoutPresetsEditorPopup::onOffsetYChanged() {
  int idx = m_presetList->currentRow();
  if (idx < 0 || idx >= m_presets.size()) return;

  m_presets[idx].m_layoutOffsetY = m_offsetYEdit->getValue();
  triggerPreview();
  m_needSaveIni = true;
}

//-----------------------------------------------------------------------------

// LayoutPresetsEditor.cpp

void LayoutPresetsEditorPopup::onAreaParamChanged() {
  int presetIdx = m_presetList->currentRow();
  int areaIdx   = m_areaList->currentRow();
  if (presetIdx < 0 || areaIdx < 0 || presetIdx >= m_presets.size() ||
      areaIdx >= m_presets.at(presetIdx).m_areas.size())
    return;

  double width  = m_widthEdit->getValue();
  double height = m_heightEdit->getValue();

  TPixel32 color = m_colorField->getColor();

  double r = (double)color.r;
  double g = (double)color.g;
  double b = (double)color.b;

  FramesDef& def = m_presets[presetIdx].m_areas[areaIdx];
  def.m_params.clear();

  def.m_params.append(width);
  def.m_params.append(height);

  bool isDefaultRed = (r == 255.0 && g == 0.0 && b == 0.0);

  if (!isDefaultRed) {
    def.m_params.append(r);
    def.m_params.append(g);
    def.m_params.append(b);
  }

  QString newText = def.toString();
  m_areaList->item(areaIdx)->setText(newText);

  triggerPreview();
  m_needSaveIni = true;
}