

#include "cleanupsettingspopup.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "cleanupsettingsmodel.h"
#include "cleanuppaletteviewer.h"
#include "cleanupswatch.h"
#include "menubarcommandids.h"
#include "pane.h"
#include "floatingpanelcommand.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/tabbar.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/filefield.h"
#include "toonzqt/cleanupcamerasettingswidget.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshsimplelevel.h"

// Qt includes
#include <QSplitter>
#include <QStackedWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QAction>
#include <QLabel>

//**********************************************************************
//    Cleanup Tab implementation
//**********************************************************************

CleanupTab::CleanupTab() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  setLayout(mainLayout);

  mainLayout->setSizeConstraint(QLayout::SetMinimumSize);
  mainLayout->setSpacing(0);
  mainLayout->setMargin(0);

  QWidget *settingsBox = new QWidget(this);
  mainLayout->addWidget(settingsBox);
  mainLayout->addStretch();  // This is the reason we need mainLayout

  QGridLayout *settingsLayout = new QGridLayout(settingsBox);
  settingsBox->setLayout(settingsLayout);

  settingsLayout->setSizeConstraint(QLayout::SetMaximumSize);
  settingsLayout->setSpacing(9);
  settingsLayout->setMargin(12);

  int row = 0;

  //  AutoCenter
  m_autoCenter = new DVGui::CheckBox(tr("Autocenter"), this);
  settingsLayout->addWidget(m_autoCenter, row++, 1, Qt::AlignLeft);

  m_autoCenter->setFixedSize(150, DVGui::WidgetHeight);

  //  Pegbar Holes
  settingsLayout->addWidget(new QLabel(tr("Pegbar Holes:")), row, 0,
                            Qt::AlignRight);

  m_pegHolesOm = new QComboBox(this);
  settingsLayout->addWidget(m_pegHolesOm, row++, 1, Qt::AlignLeft);

  m_pegHolesOm->setFixedHeight(DVGui::WidgetHeight);
  m_pegHolesOm->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  QStringList pegbarHoles;
  pegbarHoles << "Bottom"
              << "Top"
              << "Left"
              << "Right";
  m_pegHolesOm->addItems(pegbarHoles);

  //  Field Guide
  settingsLayout->addWidget(new QLabel(tr("Field Guide:")), row, 0,
                            Qt::AlignRight);

  m_fieldGuideOm = new QComboBox(this);
  settingsLayout->addWidget(m_fieldGuideOm, row++, 1, Qt::AlignLeft);

  m_fieldGuideOm->setFixedHeight(DVGui::WidgetHeight);
  m_fieldGuideOm->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  std::vector<std::string> fdgNames;
  CleanupParameters::getFdgNames(fdgNames);

  for (int i = 0; i < (int)fdgNames.size(); i++)
    m_fieldGuideOm->addItem(QString(fdgNames[i].c_str()));

  settingsLayout->addWidget(new DVGui::Separator(), row++, 0, 1, 2);

  //  Rotate
  settingsLayout->addWidget(new QLabel(tr("Rotate:")), row, 0, Qt::AlignRight);

  m_rotateOm = new QComboBox(this);
  settingsLayout->addWidget(m_rotateOm, row++, 1, Qt::AlignLeft);

  m_rotateOm->setFixedHeight(DVGui::WidgetHeight);
  m_rotateOm->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  QStringList rotate;
  rotate << "0"
         << "90"
         << "180"
         << "270";
  m_rotateOm->addItems(rotate);

  //  Flip
  settingsLayout->addWidget(new QLabel(tr("Flip:")), row, 0, Qt::AlignRight);

  QWidget *flipWidget = new QWidget();
  settingsLayout->addWidget(flipWidget, row++, 1, Qt::AlignLeft);

  QHBoxLayout *flipLayout = new QHBoxLayout();
  flipWidget->setLayout(flipLayout);

  flipLayout->setSizeConstraint(QLayout::SetFixedSize);
  flipLayout->setMargin(0);

  m_flipX = new DVGui::CheckBox(tr("Horizontal"), flipWidget);
  flipLayout->addWidget(m_flipX, 0, Qt::AlignLeft);

  m_flipX->setFixedHeight(DVGui::WidgetHeight);

  m_flipY = new DVGui::CheckBox(tr("Vertical"), flipWidget);
  flipLayout->addWidget(m_flipY, 0, Qt::AlignLeft);

  m_flipY->setFixedHeight(DVGui::WidgetHeight);

  flipLayout->addStretch(1);

  //  Save In
  settingsLayout->addWidget(new QLabel(tr("Save in:")), row, 0, Qt::AlignRight);

  m_pathField = new DVGui::FileField(this, QString(""));
  settingsLayout->addWidget(m_pathField, row++, 1);

  m_pathField->setFixedHeight(DVGui::WidgetHeight);

  //  Connections
  bool ret = true;
  ret      = ret && connect(m_autoCenter, SIGNAL(stateChanged(int)),
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

  assert(ret);
}

//-----------------------------------------------------------------------------

QString CleanupTab::pathString(const TFilePath &path, bool lpNone) {
  return path.isEmpty() ? lpNone ? QString("+extras") : QString("+drawings")
                        : toQString(path);
}

//-----------------------------------------------------------------------------

void CleanupTab::updateGui(CleanupParameters *params,
                           CleanupParameters *oldParams) {
  m_autoCenter->setChecked(params->m_autocenterType ==
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
  m_pathField->setPath(
      pathString(m_path, params->m_lineProcessingMode == lpNone));
}

//-----------------------------------------------------------------------------

void CleanupTab::onGenericSettingsChange() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  params->m_autocenterType = m_autoCenter->isChecked()
                                 ? CleanupTypes::AUTOCENTER_FDG
                                 : CleanupTypes::AUTOCENTER_NONE;
  params->m_pegSide =
      (CleanupTypes::PEGS_SIDE)(m_pegHolesOm->currentIndex() + 1);
  params->setFdgByName(m_fieldGuideOm->currentText().toStdString());
  params->m_rotate = m_rotateOm->currentIndex() * 90;
  params->m_flipx  = m_flipX->isChecked();
  params->m_flipy  = m_flipY->isChecked();

  model->commitChanges();
}

//-----------------------------------------------------------------------------

void CleanupTab::onPathChange() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  m_path = params->m_path = TFilePath(m_pathField->getPath().toStdWString());

  model->commitChanges();
}

//**********************************************************************
//    Processing Tab implementation
//**********************************************************************

ProcessingTab::ProcessingTab() {
  QVBoxLayout *mainLayout = new QVBoxLayout();
  setLayout(mainLayout);

  mainLayout->setSizeConstraint(QLayout::SetMinimumSize);
  mainLayout->addSpacing(6);

  m_settingsFrame = new QWidget(this);
  mainLayout->addWidget(m_settingsFrame);

  //----------------- Parameters Layout -----------------------

  QGridLayout *settingsLayout = new QGridLayout(m_settingsFrame);
  m_settingsFrame->setLayout(settingsLayout);

  settingsLayout->setSpacing(9);
  settingsLayout->setMargin(6);
  settingsLayout->setSizeConstraint(QLayout::SetMinimumSize);
  settingsLayout->setColumnStretch(1, 1);  // needed when lpNone

  //-------------------- Parameters ---------------------------

  int row = 0;

  //  Line processing
  settingsLayout->addWidget(new QLabel(tr("Line Processing:")), row, 0,
                            Qt::AlignRight);

  m_lineProcessing = new QComboBox(this);
  m_lineProcessing->setFixedHeight(DVGui::WidgetHeight);
  m_lineProcessing->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  QStringList items;
  items << tr("None") << tr("Greyscale") << tr("Color");
  m_lineProcessing->addItems(items);

  settingsLayout->addWidget(m_lineProcessing, row++, 1, Qt::AlignLeft);

  //  Antialias
  m_antialiasLabel = new QLabel(tr("Antialias:"));
  settingsLayout->addWidget(m_antialiasLabel, row, 0, Qt::AlignRight);

  m_antialias = new QComboBox(this);
  m_antialias->setFixedHeight(DVGui::WidgetHeight);
  m_antialias->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  items.clear();
  items << tr("Standard") << tr("None") << tr("Morphological");
  m_antialias->addItems(items);

  settingsLayout->addWidget(m_antialias, row++, 1, Qt::AlignLeft);

  //  Autoadjust
  m_autoadjustLabel = new QLabel(tr("Autoadjust:"));
  settingsLayout->addWidget(m_autoadjustLabel, row, 0, Qt::AlignRight);

  m_autoadjustOm = new QComboBox(this);
  m_autoadjustOm->setFixedHeight(DVGui::WidgetHeight);
  m_autoadjustOm->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  items.clear();
  items << "None"
        << "Black-Eq"
        << "Histo"
        << "Histo-L";
  m_autoadjustOm->addItems(items);

  settingsLayout->addWidget(m_autoadjustOm, row++, 1, Qt::AlignLeft);

  //  Sharpness
  m_sharpLabel = new QLabel(tr("Sharpness:"));
  settingsLayout->addWidget(m_sharpLabel, row, 0, Qt::AlignRight);
  m_sharpness = new DVGui::DoubleField(this);
  m_sharpness->setFixedHeight(DVGui::WidgetHeight);
  m_sharpness->setValues(90, 0, 100);
  settingsLayout->addWidget(m_sharpness, row++, 1);

  //  Despeckling
  m_despeckLabel = new QLabel(tr("Despeckling:"));
  settingsLayout->addWidget(m_despeckLabel, row, 0, Qt::AlignRight);
  m_despeckling = new DVGui::IntField(this);
  m_despeckling->setFixedHeight(DVGui::WidgetHeight);
  m_despeckling->setValues(2, 0, 20);
  settingsLayout->addWidget(m_despeckling, row++, 1);

  //  MLAA Value
  m_aaValueLabel = new QLabel(tr("MLAA Intensity:"));
  settingsLayout->addWidget(m_aaValueLabel, row, 0, Qt::AlignRight);
  m_aaValue = new DVGui::IntField(this);
  m_aaValue->setFixedHeight(DVGui::WidgetHeight);
  m_aaValue->setValues(70, 0, 100);
  settingsLayout->addWidget(m_aaValue, row++, 1);

  //---------------------- Palette ----------------------------

  m_paletteSep = new DVGui::Separator();
  mainLayout->addWidget(m_paletteSep);
  m_paletteViewer = new CleanupPaletteViewer(this);
  mainLayout->addWidget(
      m_paletteViewer, 1);  // The palette viewer dominates on the stretch below

  mainLayout->addStretch();  // Lp: none sticks to the upper edge

  //-------------------- Connections --------------------------

  bool ret = true;
  ret      = ret && connect(m_sharpness, SIGNAL(valueChanged(bool)),
                       SLOT(onSharpnessChange(bool)));
  ret = ret && connect(m_lineProcessing, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_antialias, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_autoadjustOm, SIGNAL(activated(int)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_despeckling, SIGNAL(valueChanged(bool)),
                       SLOT(onGenericSettingsChange()));
  ret = ret && connect(m_aaValue, SIGNAL(valueChanged(bool)),
                       SLOT(onGenericSettingsChange()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void ProcessingTab::updateGui(CleanupParameters *params,
                              CleanupParameters *oldParams) {
  m_lineProcessing->setCurrentIndex(params->m_lineProcessingMode);
  m_antialias->setCurrentIndex(
      params->m_postAntialias ? 2 : params->m_noAntialias ? 1 : 0);
  m_autoadjustOm->setCurrentIndex((int)params->m_autoAdjustMode);
  m_sharpness->setValue(params->m_sharpness);
  m_despeckling->setValue(params->m_despeckling);
  m_aaValue->setValue(params->m_aaValue);

  updateVisibility();
}

//------------------------------------------------------------------------------

void ProcessingTab::updateVisibility() {
  bool lp     = (m_lineProcessing->currentIndex() != 0);
  bool lpGrey = (m_lineProcessing->currentIndex() == 1);
  bool MLAA   = lp && (m_antialias->currentIndex() == 2);

  m_antialiasLabel->setVisible(lp);
  m_antialias->setVisible(lp);
  m_autoadjustLabel->setVisible(lpGrey);
  m_autoadjustOm->setVisible(lpGrey);
  m_sharpLabel->setVisible(lp);
  m_sharpness->setVisible(lp);
  m_despeckLabel->setVisible(lp);
  m_despeckling->setVisible(lp);
  m_aaValueLabel->setVisible(MLAA);
  m_aaValue->setVisible(MLAA);

  m_paletteViewer->setMode(lpGrey);
  m_paletteViewer->setContrastEnabled(m_antialias->currentIndex() == 0);

  if (!lp && !m_paletteViewer->isHidden()) {
    m_paletteViewer->setVisible(false);
    m_paletteSep->setVisible(false);
  } else if (lp && m_paletteViewer->isHidden()) {
    m_paletteViewer->setVisible(true);
    m_paletteSep->setVisible(false);
  }
}

//-----------------------------------------------------------------------------

void ProcessingTab::onGenericSettingsChange() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  params->m_lineProcessingMode = m_lineProcessing->currentIndex();
  params->m_noAntialias        = (m_antialias->currentIndex() > 0);
  params->m_postAntialias      = (m_antialias->currentIndex() == 2);
  params->m_despeckling        = m_despeckling->getValue();
  params->m_aaValue            = m_aaValue->getValue();
  params->m_autoAdjustMode =
      (CleanupTypes::AUTO_ADJ_MODE)m_autoadjustOm->currentIndex();

  if (params->m_lineProcessingMode == lpNone)
    params->m_transparencyCheckEnabled = false;

  model->commitChanges();
}

//-----------------------------------------------------------------------------

void ProcessingTab::onSharpnessChange(bool dragging = false) {
  if (dragging) return;

  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  model->getCurrentParameters()->m_sharpness = m_sharpness->getValue();
  model->commitChanges();
}

//**********************************************************************
//    CameraTab implementation
//**********************************************************************

CameraTab::CameraTab() {
  bool ret = true;
  ret      = ret &&
        connect(TApp::instance()->getCurrentLevel(),
                SIGNAL(xshLevelSwitched(TXshLevel *)), SLOT(onLevelSwitched()));
  ret = ret && connect(this, SIGNAL(cleanupSettingsChanged()),
                       SLOT(onGenericSettingsChange()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void CameraTab::updateGui(CleanupParameters *params,
                          CleanupParameters *oldParams) {
  blockSignals(true);
  setFields(params);
  blockSignals(false);

  if (params->m_lineProcessingMode != oldParams->m_lineProcessingMode ||
      params->m_camera.getRes() != oldParams->m_camera.getRes() ||
      params->m_camera.getSize() != oldParams->m_camera.getSize() ||
      params->m_closestField != oldParams->m_closestField) {
    updateImageInfo();
  }
}

//-------------------------------------------------------

void CameraTab::updateImageInfo() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  TDimension outRes(0, 0);
  TPointD outDpi;

  params->getOutputImageInfo(outRes, outDpi.x, outDpi.y);
  setImageInfo(outRes.lx, outRes.ly, outDpi.x, outDpi.y);

  TXshSimpleLevel *sl;
  TFrameId fid;
  model->getCleanupFrame(sl, fid);

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath outputPath(
      sl ? scene->decodeFilePath(model->getOutputPath(sl, params))
         : TFilePath());

  setImageInfo(outputPath);
}

//-----------------------------------------------------------------------------

void CameraTab::onLevelSwitched() {
  setCurrentLevel(TApp::instance()->getCurrentLevel()->getLevel());
}

//------------------------------------------------------------------------------

void CameraTab::onGenericSettingsChange() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  getFields(model->getCurrentParameters());
  model->commitChanges();
}

//**********************************************************************
//    CleanupSettings implementation
//**********************************************************************

CleanupSettings::CleanupSettings(QWidget *parent)
    : QWidget(parent), m_attached(false) {
  QVBoxLayout *vLayout = new QVBoxLayout(this);
  vLayout->setMargin(1);  // NOTE: This works to show the 1-pix black border,
                          // because this is a QWidget (not QFrame) heir...
  setLayout(vLayout);

  //   Tabs Container
  // Used to deal with styled background and other stuff

  TabBarContainter *tabBarContainer = new TabBarContainter(this);
  QHBoxLayout *hLayout              = new QHBoxLayout(tabBarContainer);

  hLayout->setMargin(0);
  hLayout->setAlignment(Qt::AlignLeft);
  hLayout->addSpacing(6);

  vLayout->addWidget(tabBarContainer);

  //  Tabs Bar

  DVGui::TabBar *tabBar = new DVGui::TabBar(this);
  hLayout->addWidget(tabBar);

  tabBar->addSimpleTab(tr("Cleanup"));
  tabBar->addSimpleTab(tr("Processing"));
  tabBar->addSimpleTab(tr("Camera"));
  tabBar->setDrawBase(false);

  //  Splitter

  QSplitter *split = new QSplitter(Qt::Vertical, this);
  split->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                   QSizePolicy::MinimumExpanding));
  vLayout->addWidget(split);

  //  Stacked Widget

  QStackedWidget *stackedWidget = new QStackedWidget(split);
  stackedWidget->setMinimumWidth(300);
  // stackedWidget->setMinimumHeight(250);

  split->addWidget(stackedWidget);
  split->setStretchFactor(0, 1);

  //  Cleanup Tab

  QScrollArea *scrollArea = new QScrollArea(stackedWidget);
  stackedWidget->addWidget(scrollArea);

  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_cleanupTab = new CleanupTab;
  scrollArea->setWidget(m_cleanupTab);

  //  Processing Tab

  scrollArea = new QScrollArea(stackedWidget);
  stackedWidget->addWidget(scrollArea);

  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_processingTab = new ProcessingTab;
  scrollArea->setWidget(m_processingTab);

  //  Camera Tab

  scrollArea = new QScrollArea(stackedWidget);
  stackedWidget->addWidget(scrollArea);

  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_cameraTab = new CameraTab;
  scrollArea->setWidget(m_cameraTab);

  m_cameraTab->setCameraPresetListFile(ToonzFolder::getReslistPath(true));

  //  Swatch

  m_swatch = new CleanupSwatch(split, 200, 150);
  split->addWidget(m_swatch);

  //  ToolBar

  QWidget *toolBarWidget = new QWidget(this);
  vLayout->addWidget(toolBarWidget);

  toolBarWidget->setFixedHeight(22);

  QHBoxLayout *toolBarLayout = new QHBoxLayout(toolBarWidget);
  toolBarWidget->setLayout(toolBarLayout);

  toolBarLayout->setMargin(0);
  toolBarLayout->setSpacing(0);

  QToolBar *leftToolBar = new QToolBar(toolBarWidget);
  toolBarLayout->addWidget(leftToolBar, 0, Qt::AlignLeft);

  leftToolBar->setFixedWidth(110);

  m_swatchAct = new QAction(createQIconOnOff("preview", true),
                            tr("Toggle Swatch Preview"), this);
  m_swatchAct->setCheckable(true);
  leftToolBar->addAction(m_swatchAct);
  leftToolBar->addSeparator();

  m_opacityAct = new QAction(createQIconOnOff("opacitycheck", true),
                             tr("Toggle Opacity Check"), this);
  m_opacityAct->setCheckable(true);
  leftToolBar->addAction(m_opacityAct);

  QToolBar *spacingToolBar1 = new QToolBar(toolBarWidget);
  toolBarLayout->addWidget(spacingToolBar1, 1);

  spacingToolBar1->setMinimumHeight(22);

  QToolBar *rightToolBar = new QToolBar(toolBarWidget);
  toolBarLayout->addWidget(rightToolBar, 0, Qt::AlignRight);

  rightToolBar->setFixedWidth(110);

  QAction *saveAct =
      new QAction(createQIconOnOff("save", false), tr("Save Settings"), this);
  rightToolBar->addAction(saveAct);
  QAction *loadAct =
      new QAction(createQIconOnOff("load", false), tr("Load Settings"), this);
  rightToolBar->addAction(loadAct);
  rightToolBar->addSeparator();
  QAction *resetAct = new QAction(createQIconOnOff("resetsize", false),
                                  tr("Reset Settings"), this);
  rightToolBar->addAction(resetAct);

  //  Model-related stuff
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  m_backupParams.assign(model->getCurrentParameters(), false);

  //  Connections

  QAction *opacityCheckCmd =
      CommandManager::instance()->getAction(MI_OpacityCheck);
  assert(opacityCheckCmd);

  bool ret = true;
  ret      = ret && connect(tabBar, SIGNAL(currentChanged(int)), stackedWidget,
                       SLOT(setCurrentIndex(int)));
  ret = ret &&
        connect(m_swatchAct, SIGNAL(toggled(bool)), SLOT(enableSwatch(bool)));
  ret = ret && connect(m_opacityAct, SIGNAL(triggered(bool)), opacityCheckCmd,
                       SLOT(trigger()));
  ret = ret && connect(saveAct, SIGNAL(triggered()), model, SLOT(promptSave()));
  ret = ret && connect(loadAct, SIGNAL(triggered()), model, SLOT(promptLoad()));
  ret = ret && connect(resetAct, SIGNAL(triggered()), this,
                       SLOT(onRestoreSceneSettings()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void CleanupSettings::showEvent(QShowEvent *se) {
  QWidget::showEvent(se);

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

    m_cameraTab->setCurrentLevel(
        TApp::instance()->getCurrentLevel()->getLevel());

    updateGui(false);
    onImageSwitched();
    onClnLoaded();

    if (m_swatchAct->isChecked()) enableSwatch(true);  // attach swatch
  }
}

//-----------------------------------------------------------------------------

void CleanupSettings::hideEvent(QHideEvent *he) {
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

    if (m_swatchAct->isChecked()) enableSwatch(false);  // detach swatch
  }

  QWidget::hideEvent(he);
}

//-----------------------------------------------------------------------------

void CleanupSettings::updateGui(bool needsPostProcess) {
  CleanupParameters *params =
      CleanupSettingsModel::instance()->getCurrentParameters();

  m_cleanupTab->updateGui(params, &m_backupParams);
  m_processingTab->updateGui(params, &m_backupParams);
  m_cameraTab->updateGui(params, &m_backupParams);

  m_backupParams.assign(params, false);

  m_opacityAct->setChecked(params->m_transparencyCheckEnabled);
  if (params->m_lineProcessingMode == lpNone) {
    m_swatch->setRaster(TRasterP(), TAffine(), TRasterP());
    m_swatchAct->setChecked(false);
  } else if (needsPostProcess)
    postProcess();
}

//-----------------------------------------------------------------------------

void CleanupSettings::enableSwatch(bool enable) {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  if (enable) {
    model->attach(CleanupSettingsModel::PREVIEWER);
    connect(model, SIGNAL(previewDataChanged()), this,
            SLOT(onPreviewDataChanged()));
    onPreviewDataChanged();
  } else {
    model->detach(CleanupSettingsModel::PREVIEWER);
    disconnect(model, SIGNAL(previewDataChanged()), this,
               SLOT(onPreviewDataChanged()));
    m_swatch->setRaster(TRasterP(), TAffine(), TRasterP());
  }
}

//-----------------------------------------------------------------------------

void CleanupSettings::enableOpacityCheck(bool enable) {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  CleanupParameters *params   = model->getCurrentParameters();

  params->m_transparencyCheckEnabled = enable;

  model->commitChanges();
}

//-----------------------------------------------------------------------------

void CleanupSettings::onImageSwitched() { m_cameraTab->updateImageInfo(); }

//-----------------------------------------------------------------------------

void CleanupSettings::onPreviewDataChanged() {
  TRasterImageP original, transformed;
  TAffine transform;

  CleanupSettingsModel::instance()->getPreviewData(original, transformed,
                                                   transform);
  m_swatch->setRaster(original ? original->getRaster() : TRasterP(), transform,
                      transformed ? transformed->getRaster() : TRasterP());
}

//-----------------------------------------------------------------------------

void CleanupSettings::postProcess() { m_swatch->updateCleanupped(); }

//-----------------------------------------------------------------------------

void CleanupSettings::onClnLoaded() {
  const TFilePath &fp = CleanupSettingsModel::instance()->clnPath();

  const QString &newWindowTitle =
      (fp.isEmpty())
          ? tr("Cleanup Settings")
          : tr("Cleanup Settings: %1").arg(toQString(fp.withoutParentDir()));

  if (windowTitle() != newWindowTitle) {
    setWindowTitle(newWindowTitle);
    emit windowTitleChanged(windowTitle());
  }
}

//-----------------------------------------------------------------------------

void CleanupSettings::onRestoreSceneSettings() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  model->saveSettingsIfNeeded();
  model->restoreGlobalSettings();
}

//**********************************************************************
//    Toggle Opacity Check Command
//**********************************************************************

class ToggleOpacityCheckCommand final : public MenuItemHandler {
public:
  ToggleOpacityCheckCommand() : MenuItemHandler(MI_OpacityCheck) {}

  void execute() override {
    CleanupSettingsModel *model = CleanupSettingsModel::instance();
    CleanupParameters *params   = model->getCurrentParameters();

    params->m_transparencyCheckEnabled = !params->m_transparencyCheckEnabled;

    model->commitChanges();
  }

} toggleOpacityCheckCommand;

//**********************************************************************
//    Open Popup Command
//**********************************************************************
#if 0
class CleanupSettingsFactory final : public TPanelFactory {
public:
  CleanupSettingsFactory() : TPanelFactory("CleanupSettings") {}

  void initialize(TPanel *panel) {
    CleanupSettings *cleanupSettings = new CleanupSettings(panel);
    panel->setWidget(cleanupSettings);

    bool ret = QObject::connect(cleanupSettings,
                                SIGNAL(windowTitleChanged(const QString &)),
                                panel, SLOT(setWindowTitle(const QString &)));
    assert(ret);

    panel->setMinimumSize(320, 150);
    panel->resize(600, 500);
  }

} cleanupSettingsFactory;
#endif

OpenFloatingPanel cleanupSettingsCommand(MI_CleanupSettings, "CleanupSettings",
                                         QObject::tr("Cleanup Settings"));
