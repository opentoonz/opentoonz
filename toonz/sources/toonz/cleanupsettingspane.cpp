
// ToonzCore includes
#include "tmsgcore.h"

// ToonzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/toonzfolders.h"

// ToonzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"

// Toonz includes
#include "tapp.h"
#include "cleanupsettingsmodel.h"
#include "cleanuppaletteviewer.h"
#include "pane.h"
#include "menubarcommandids.h"
#include "floatingpanelcommand.h"

// Qt includes
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QGroupBox>

#include "cleanupsettingspane.h"

using namespace DVGui;

/*
"Save In"
フィールドのためのFileField。browseDirectoryを再実装して、フィールドが空欄のときは、
カレントレベル（Scan画像。TIF等）の入っているフォルダの１つ上をデフォルトフォルダにして開くようにしたい。
*/
void CleanupSaveInField::browseDirectory() {
  if (!m_fileBrowseButton->hasFocus()) return;
  QString directory = QString();

  if (!m_browserPopupController) return;

  /*
  ここで、m_lastSelectedPathが空のとき、カレントレベルがScan画像の場合、
  そのファイルの入っているフォルダの１つ上のフォルダを初期フォルダにする
  */
  QString initialFolder = m_lastSelectedPath;
  if (initialFolder.isEmpty()) {
    /*--- 親Widgetを取得する ---*/
    CleanupSettingsPane *parentCSP =
        dynamic_cast<CleanupSettingsPane *>(parentWidget());
    if (parentCSP) {
      TFilePath lastSelectedPath = parentCSP->getLastSelectedPath();
      if (!lastSelectedPath.isEmpty()) {
        /*----
         * 親Widgetのm_lastSelectedPathが、CLNファイルの見込み所在地なので、その１つ上のフォルダを初期フォルダにする。---*/
        initialFolder = QString::fromStdWString(
            lastSelectedPath.getParentDir().getParentDir().getWideString());
      }
    }
  }

  m_browserPopupController->openPopup(QStringList(), true, initialFolder);
  if (m_browserPopupController->isExecute())
    directory = m_browserPopupController->getPath();

  if (!directory.isEmpty()) {
    setPath(directory);
    m_lastSelectedPath = directory;
    emit pathChanged();
    return;
  }
}

//=============================================================================
// CleanupSettingsPane
//-----------------------------------------------------------------------------

CleanupSettingsPane::CleanupSettingsPane(QWidget *parent)
    : QFrame(parent), m_attached(false) {
  // Autocenter
  m_autocenterBox = new QGroupBox(tr("Autocenter"), this);
  m_pegHolesOm    = new QComboBox(this);
  m_fieldGuideOm  = new QComboBox(this);
  // Rotate&Flip
  QFrame *rotFlipFrame = new QFrame(this);
  m_rotateOm           = new QComboBox(this);
  m_flipX              = new QCheckBox(tr("Horizontal"), this);
  m_flipY              = new QCheckBox(tr("Vertical"), this);
  // Camera
  QFrame *cameraFrame = new QFrame(this);
  m_cameraWidget      = new CleanupCameraSettingsWidget();
  // LineProcessing
  QFrame *lineProcFrame = new QFrame(this);
  m_antialias           = new QComboBox(this);
  m_sharpness           = new DoubleField(this);
  m_despeckling         = new IntField(this);
  m_aaValueLabel        = new QLabel(tr("MLAA Intensity:"));
  m_aaValue             = new IntField(this);
  m_lineProcessing      = new QComboBox(this);
  m_paletteViewer       = new CleanupPaletteViewer(this);
  m_pathField           = new CleanupSaveInField(this, QString(""));

  QPushButton *saveBtn  = new QPushButton(tr("Save"));
  QPushButton *loadBtn  = new QPushButton(tr("Load"));
  QPushButton *resetBtn = new QPushButton(tr("Reset"));

  // Autocenter
  m_autocenterBox->setCheckable(true);
  QStringList pegbarHoles;
  pegbarHoles << tr("Bottom")
              << tr("Top")
              << tr("Left")
              << tr("Right");
  m_pegHolesOm->addItems(pegbarHoles);
  std::vector<std::string> fdgNames;
  CleanupParameters::getFdgNames(fdgNames);
  for (int i = 0; i < (int)fdgNames.size(); i++)
    m_fieldGuideOm->addItem(QString(fdgNames[i].c_str()));

  // Rotate&Flip
  rotFlipFrame->setObjectName("CleanupSettingsFrame");
  QStringList rotate;
  rotate << "0"
         << "90"
         << "180"
         << "270";
  m_rotateOm->addItems(rotate);
  // Camera
  cameraFrame->setObjectName("CleanupSettingsFrame");
  m_cameraWidget->setCameraPresetListFile(ToonzFolder::getReslistPath(true));
  // LineProcessing
  lineProcFrame->setObjectName("CleanupSettingsFrame");
  QStringList items;
  items << tr("Standard") << tr("None") << tr("Morphological");
  m_antialias->addItems(items);

  items.clear();
  items << tr("Greyscale") << tr("Color");
  m_lineProcessing->addItems(items);

  m_sharpness->setValues(90, 0, 100);
  m_despeckling->setValues(2, 0, 20);
  m_aaValue->setValues(70, 0, 100);

  //  Model-related stuff
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  m_backupParams.assign(model->getCurrentParameters(), false);

  //----layout
  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setSpacing(2);
  mainLay->setMargin(5);
  {
    // Autocenter
    QGridLayout *autocenterLay = new QGridLayout();
    autocenterLay->setMargin(5);
    autocenterLay->setSpacing(3);
    {
      autocenterLay->addWidget(new QLabel(tr("Pegbar Holes")), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      autocenterLay->addWidget(m_pegHolesOm, 0, 1);
      autocenterLay->addWidget(new QLabel(tr("Field Guide")), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      autocenterLay->addWidget(m_fieldGuideOm, 1, 1);
    }
    autocenterLay->setColumnStretch(0, 0);
    autocenterLay->setColumnStretch(1, 1);
    m_autocenterBox->setLayout(autocenterLay);
    mainLay->addWidget(m_autocenterBox, 0);

    // Rotate&Flip
    QGridLayout *rotFlipLay = new QGridLayout();
    rotFlipLay->setMargin(5);
    rotFlipLay->setSpacing(3);
    {
      rotFlipLay->addWidget(new QLabel(tr("Rotate")), 0, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      rotFlipLay->addWidget(m_rotateOm, 0, 1, 1, 2);
      rotFlipLay->addWidget(new QLabel(tr("Flip")), 1, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      rotFlipLay->addWidget(m_flipX, 1, 1);
      rotFlipLay->addWidget(m_flipY, 1, 2);
    }
    rotFlipLay->setColumnStretch(0, 0);
    rotFlipLay->setColumnStretch(1, 0);
    rotFlipLay->setColumnStretch(2, 1);
    rotFlipFrame->setLayout(rotFlipLay);
    mainLay->addWidget(rotFlipFrame, 0);

    // Camera
    QVBoxLayout *cleanupCameraFrameLay = new QVBoxLayout();
    cleanupCameraFrameLay->setMargin(0);
    cleanupCameraFrameLay->setSpacing(0);
    { cleanupCameraFrameLay->addWidget(m_cameraWidget); }
    cameraFrame->setLayout(cleanupCameraFrameLay);
    mainLay->addWidget(cameraFrame, 0);

    // Cleanup Palette
    QGridLayout *lineProcLay = new QGridLayout();
    lineProcLay->setMargin(5);
    lineProcLay->setSpacing(3);
    {
      lineProcLay->addWidget(new QLabel(tr("Line Processing:")), 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      lineProcLay->addWidget(m_lineProcessing, 0, 1);
      lineProcLay->addWidget(new QLabel(tr("Antialias:")), 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      lineProcLay->addWidget(m_antialias, 1, 1);
      lineProcLay->addWidget(new QLabel(tr("Sharpness:")), 2, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      lineProcLay->addWidget(m_sharpness, 2, 1);
      lineProcLay->addWidget(new QLabel(tr("Despeckling:")), 3, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      lineProcLay->addWidget(m_despeckling, 3, 1);
      lineProcLay->addWidget(m_aaValueLabel, 4, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      lineProcLay->addWidget(m_aaValue, 4, 1);

      lineProcLay->addWidget(m_paletteViewer, 5, 0, 1, 2);
    }
    lineProcLay->setRowStretch(0, 0);
    lineProcLay->setRowStretch(1, 0);
    lineProcLay->setRowStretch(2, 0);
    lineProcLay->setRowStretch(3, 0);
    lineProcLay->setRowStretch(4, 0);
    lineProcLay->setRowStretch(5, 1);
    lineProcLay->setColumnStretch(0, 0);
    lineProcLay->setColumnStretch(1, 1);

    lineProcFrame->setLayout(lineProcLay);

    mainLay->addWidget(lineProcFrame, 100);

    // Bottom Parts
    QHBoxLayout *pathLay = new QHBoxLayout();
    pathLay->setMargin(0);
    pathLay->setSpacing(3);
    {
      pathLay->addWidget(new QLabel(tr("Save In")), 0);
      pathLay->addWidget(m_pathField);
    }
    mainLay->addLayout(pathLay, 0);

    mainLay->addSpacing(5);

    QHBoxLayout *saveLoadLay = new QHBoxLayout();
    saveLoadLay->setMargin(0);
    saveLoadLay->setSpacing(1);
    {
      saveLoadLay->addWidget(saveBtn);
      saveLoadLay->addWidget(loadBtn);
      saveLoadLay->addWidget(resetBtn);
    }
    mainLay->addLayout(saveLoadLay, 0);
  }
  setLayout(mainLay);

  //-----signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_autocenterBox, SIGNAL(toggled(bool)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_pegHolesOm, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_fieldGuideOm, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));

  ret = ret && connect(m_rotateOm, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_flipX, SIGNAL(stateChanged(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_flipY, SIGNAL(stateChanged(int)),
                       SLOT(onGenericSettingsChange()));
  ret =
      ret && connect(m_pathField, SIGNAL(pathChanged()), SLOT(onPathChange()));
  ret = ret && connect(m_sharpness, SIGNAL(valueChanged(bool)),
                       SLOT(onSharpnessChange(bool)));
  ret = ret && connect(m_antialias, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_lineProcessing, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_despeckling, SIGNAL(valueChanged(bool)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_aaValue, SIGNAL(valueChanged(bool)),
                       SLOT(onGenericSettingsChange()));
  ret = ret &&
        connect(TApp::instance()->getCurrentLevel(),
                SIGNAL(xshLevelSwitched(TXshLevel *)), SLOT(onLevelSwitched()));
  ret = ret && connect(m_cameraWidget, SIGNAL(cleanupSettingsChanged()),
                       SLOT(onGenericSettingsChange()));

  ret =
      ret && connect(saveBtn, SIGNAL(pressed()), this, SLOT(onSaveSettings()));

  ret = ret && connect(loadBtn, SIGNAL(pressed()), model, SLOT(promptLoad()));
  ret = ret && connect(resetBtn, SIGNAL(pressed()), this,
                       SLOT(onRestoreSceneSettings()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::showEvent(QShowEvent *se) {
  QFrame::showEvent(se);

  if (!m_attached) {
    m_attached = true;
    // Should ensure that swatch is off...
    CleanupSettingsModel *model = CleanupSettingsModel::instance();

    model->attach(CleanupSettingsModel::LISTENER);

    bool ret = true;
    ret      = ret && connect(model, SIGNAL(imageSwitched()), this,
                         SLOT(onImageSwitched()));
    ret = ret && connect(model, SIGNAL(modelChanged(bool)), this,
                         SLOT(updateGui(bool)));
    ret = ret && connect(model, SIGNAL(clnLoaded()), this, SLOT(onClnLoaded()));
    assert(ret);

    m_cameraWidget->setCurrentLevel(
        TApp::instance()->getCurrentLevel()->getLevel());

    updateGui(false);
    onImageSwitched();
    onClnLoaded();
  }
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::hideEvent(QHideEvent *he) {
  // Surprisingly enough, it seems that Qt may trigger hideEvents for widgets
  // that
  // have never been shown - this is why we need to check a user-made bool to
  // know whether
  // model attachment has happened... and no, detaching without attaching is
  // *BAD*

  if (m_attached) {
    m_attached = false;
    // Should put swatch to off...
    CleanupSettingsModel *model = CleanupSettingsModel::instance();

    model->detach(CleanupSettingsModel::LISTENER);

    bool ret = true;
    ret      = ret && disconnect(model, SIGNAL(imageSwitched()), this,
                            SLOT(onImageSwitched()));
    ret = ret && disconnect(model, SIGNAL(modelChanged(bool)), this,
                            SLOT(updateGui(bool)));
    ret = ret &&
          disconnect(model, SIGNAL(clnLoaded()), this, SLOT(onClnLoaded()));
    assert(ret);
  }

  QFrame::hideEvent(he);
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::updateGui(bool needsPostProcess) {
  CleanupParameters *params =
      CleanupSettingsModel::instance()->getCurrentParameters();

  updateGui(params, &m_backupParams);

  m_backupParams.assign(params, false);

  if (needsPostProcess) postProcess();
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::updateGui(CleanupParameters *params,
                                    CleanupParameters *oldParams) {
  m_autocenterBox->setChecked(params->m_autocenterType ==
                              CleanupTypes::AUTOCENTER_FDG);
  m_pegHolesOm->setCurrentIndex(params->m_pegSide - 1);

  QString fieldName = QString::fromStdString(params->getFdgName());
  int index = (fieldName.isEmpty()) ? 0 : m_fieldGuideOm->findText(fieldName);
  assert(index != -1);
  m_fieldGuideOm->setCurrentIndex(index);

  m_rotateOm->setCurrentIndex(params->m_rotate / 90);
  m_flipX->setChecked(params->m_flipx);
  m_flipY->setChecked(params->m_flipy);

  m_path = params->m_path;
  m_pathField->setPath(toQString(m_path));

  m_lineProcessing->setCurrentIndex(
      (params->m_lineProcessingMode == lpGrey) ? 0 : 1);
  m_antialias->setCurrentIndex(
      params->m_postAntialias ? 2 : params->m_noAntialias ? 1 : 0);
  m_sharpness->setValue(params->m_sharpness);
  m_despeckling->setValue(params->m_despeckling);
  m_aaValue->setValue(params->m_aaValue);

  updateVisibility();

  blockSignals(true);
  m_cameraWidget->setFields(params);
  blockSignals(false);

  if (params->m_lineProcessingMode != oldParams->m_lineProcessingMode ||
      params->m_camera.getRes() != oldParams->m_camera.getRes() ||
      params->m_camera.getSize() != oldParams->m_camera.getSize() ||
      params->m_closestField != oldParams->m_closestField) {
    updateImageInfo();
  }
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::updateImageInfo() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  TDimension outRes(0, 0);
  TPointD outDpi;

  params->getOutputImageInfo(outRes, outDpi.x, outDpi.y);
  m_cameraWidget->setImageInfo(outRes.lx, outRes.ly, outDpi.x, outDpi.y);

  TXshSimpleLevel *sl;
  TFrameId fid;
  model->getCleanupFrame(sl, fid);

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath outputPath(
      sl ? scene->decodeFilePath(model->getOutputPath(sl, params))
         : TFilePath());

  m_cameraWidget->setImageInfo(outputPath);

  if (!parentWidget()) return;

  if (model->clnPath().isEmpty())
    parentWidget()->setWindowTitle(tr("Cleanup Settings (Global)"));
  else
    parentWidget()->setWindowTitle(
        tr("Cleanup Settings: ") +
        toQString(model->clnPath().withoutParentDir()));
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::updateVisibility() {
  bool lpGrey = (m_lineProcessing->currentIndex() == 0);
  bool MLAA   = (m_antialias->currentIndex() == 2);

  m_antialias->setVisible(true);
  m_sharpness->setVisible(true);
  m_despeckling->setVisible(true);
  m_aaValueLabel->setVisible(MLAA);
  m_aaValue->setVisible(MLAA);

  m_paletteViewer->setMode(lpGrey);
  m_paletteViewer->setContrastEnabled(m_antialias->currentIndex() == 0);

  m_paletteViewer->setVisible(true);
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onImageSwitched() { updateImageInfo(); }

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onPreviewDataChanged() {
  TRasterImageP original, transformed;
  TAffine transform;

  CleanupSettingsModel::instance()->getPreviewData(original, transformed,
                                                   transform);
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::postProcess() {
  // m_swatch->updateCleanupped();
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onClnLoaded() {
  const TFilePath &fp = CleanupSettingsModel::instance()->clnPath();
  if (fp.isEmpty())
    setWindowTitle(tr("Cleanup Settings"));
  else
    setWindowTitle(
        tr("Cleanup Settings: %1").arg(toQString(fp.withoutParentDir())));
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onRestoreSceneSettings() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  model->saveSettingsIfNeeded();
  model->restoreGlobalSettings();
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onGenericSettingsChange() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  params->m_autocenterType = m_autocenterBox->isChecked()
                                 ? CleanupTypes::AUTOCENTER_FDG
                                 : CleanupTypes::AUTOCENTER_NONE;
  params->m_pegSide =
      (CleanupTypes::PEGS_SIDE)(m_pegHolesOm->currentIndex() + 1);
  params->setFdgByName(m_fieldGuideOm->currentText().toStdString());

  params->m_rotate = m_rotateOm->currentIndex() * 90;
  params->m_flipx  = m_flipX->isChecked();
  params->m_flipy  = m_flipY->isChecked();

  //------

  params->m_lineProcessingMode =
      (int)((m_lineProcessing->currentIndex() == 0) ? lpGrey : lpColor);
  params->m_noAntialias   = (m_antialias->currentIndex() > 0);
  params->m_postAntialias = (m_antialias->currentIndex() == 2);
  params->m_despeckling   = m_despeckling->getValue();
  params->m_aaValue       = m_aaValue->getValue();

  //------

  m_cameraWidget->getFields(model->getCurrentParameters());

  model->commitChanges();
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onPathChange() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  m_path = params->m_path = TFilePath(m_pathField->getPath().toStdWString());

  model->commitChanges();
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onSharpnessChange(bool dragging = false) {
  if (dragging) return;

  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  model->getCurrentParameters()->m_sharpness = m_sharpness->getValue();
  model->commitChanges();
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onLevelSwitched() {
  m_cameraWidget->setCurrentLevel(
      TApp::instance()->getCurrentLevel()->getLevel());
}

//-----------------------------------------------------------------------------

void CleanupSettingsPane::onSaveSettings() {
  /*--- Clueaup保存先を指定していないとエラーを返す ---*/
  if (m_pathField->getPath().isEmpty()) {
    DVGui::warning(tr("Please fill the Save In field."));
    return;
  }
  CleanupSettingsModel::instance()->promptSave();
}
