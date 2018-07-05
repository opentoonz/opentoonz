

#include "levelsettingspopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "flipbook.h"
#include "fileviewerpopup.h"
#include "castselection.h"
#include "fileselection.h"
#include "columnselection.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/infoviewer.h"
#include "toonzqt/filefield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/fxselection.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/tcamera.h"
#include "toonz/levelset.h"
#include "toonz/tpalettehandle.h"
#include "toonz/preferences.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshcell.h"
#include "toonz/txsheet.h"
#include "toonz/childstack.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tframehandle.h"

// TnzCore includes
#include "tconvert.h"
#include "tsystem.h"
#include "tfiletype.h"
#include "tlevel.h"
#include "tstream.h"

// Qt includes
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QMainWindow>
#include <QGroupBox>

using namespace DVGui;

//-----------------------------------------------------------------------------
namespace {

//-----------------------------------------------------------------------------

QString dpiToString(const TPointD &dpi) {
  if (dpi.x == 0.0 || dpi.y == 0.0)
    return QString("none");
  else if (areAlmostEqual(dpi.x, dpi.y, 0.01))
    return QString::number(dpi.x);
  else
    return QString::number(dpi.x) + ", " + QString::number(dpi.y);
}

//-----------------------------------------------------------------------------

TPointD getCurrentCameraDpi() {
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TDimensionD size = camera->getSize();
  TDimension res   = camera->getRes();
  return TPointD(res.lx / size.lx, res.ly / size.ly);
}

//-----------------------------------------------------------------------------
}  // anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================
/*! \class LevelSettingsPopup
                \brief The LevelSettingsPopup class provides a dialog to show
   and change
                                         current level settings.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

LevelSettingsPopup::LevelSettingsPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, false, "LevelSettings")
    , m_whiteTransp(0)
    , m_scanPathLabel(0)
    , m_scanPathFld(0) {
  setWindowTitle(tr("Level Settings"));

  m_nameFld       = new LineEdit();
  m_pathFld       = new FileField();  // Path
  m_scanPathLabel = new QLabel(tr("Scan Path:"));
  m_scanPathFld   = new FileField();  // ScanPath
  m_typeLabel     = new QLabel();     // Level Type
  // Type
  m_dpiTypeOm = new QComboBox();
  // DPI
  m_dpiLabel    = new QLabel(tr("DPI:"));
  m_dpiFld      = new DoubleLineEdit();
  m_squarePixCB = new CheckBox(tr("Forced Squared Pixel"));

  m_widthLabel  = new QLabel(tr("Width:"));
  m_widthFld    = new MeasuredDoubleLineEdit();
  m_heightLabel = new QLabel(tr("Height:"));
  m_heightFld   = new MeasuredDoubleLineEdit();
  // Use Camera Dpi
  m_useCameraDpiBtn = new QPushButton(tr("Use Camera DPI"));

  m_cameraDpiLabel = new QLabel(tr(""));
  m_imageDpiLabel  = new QLabel(tr(""));
  m_imageResLabel  = new QLabel(tr(""));
  m_cameraDpiTitle = new QLabel(tr("Camera DPI:"));
  m_imageDpiTitle  = new QLabel(tr("Image DPI:"));
  m_imageResTitle  = new QLabel(tr("Resolution:"));

  // subsampling
  m_subsamplingLabel = new QLabel(tr("Subsampling:"));
  m_subsamplingFld   = new DVGui::IntLineEdit(this, 1, 1);

  m_doPremultiply = new CheckBox(tr("Premultiply"), this);

  m_whiteTransp = new CheckBox(tr("White As Transparent"), this);

  m_doAntialias       = new CheckBox(tr("Add Antialiasing"), this);
  m_antialiasSoftness = new DVGui::IntLineEdit(0, 10, 0, 100);

  //----

  m_pathFld->setFileMode(QFileDialog::AnyFile);
  m_scanPathFld->setFileMode(QFileDialog::AnyFile);

  m_dpiTypeOm->addItem(tr("Image DPI"), "Image DPI");
  m_dpiTypeOm->addItem(tr("Custom DPI"), "Custom DPI");

  m_squarePixCB->setChecked(true);

  /*--- Levelサイズの単位はCameraUnitにする --*/
  m_widthFld->setMeasure("camera.lx");
  m_heightFld->setMeasure("camera.ly");

  if (Preferences::instance()->getCameraUnits() == "pixel") {
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
  }

  m_doPremultiply->setChecked(false);

  m_doAntialias->setChecked(false);
  m_antialiasSoftness->setEnabled(false);

  m_whiteTransp->setChecked(false);

  //----layout

  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(5);
  {
    //--Name&Path
    QGroupBox *nameBox      = new QGroupBox(tr("Name && Path"), this);
    QGridLayout *nameLayout = new QGridLayout();
    nameLayout->setMargin(5);
    nameLayout->setSpacing(5);
    {
      nameLayout->addWidget(new QLabel(tr("Name:"), this), 0, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      nameLayout->addWidget(m_nameFld, 0, 1);
      nameLayout->addWidget(new QLabel(tr("Path:"), this), 1, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      nameLayout->addWidget(m_pathFld, 1, 1);
      nameLayout->addWidget(m_scanPathLabel, 2, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
      nameLayout->addWidget(m_scanPathFld, 2, 1);
      nameLayout->addWidget(m_typeLabel, 3, 1);
    }
    nameLayout->setColumnStretch(0, 0);
    nameLayout->setColumnStretch(1, 1);
    nameBox->setLayout(nameLayout);

    m_topLayout->addWidget(nameBox);

    //----DPI & Resolution
    QGroupBox *dpiBox;
    if (Preferences::instance()->getUnits() == "pixel")
      dpiBox = new QGroupBox(tr("Resolution"), this);
    else
      dpiBox               = new QGroupBox(tr("DPI && Resolution"), this);
    QGridLayout *dpiLayout = new QGridLayout();
    dpiLayout->setMargin(5);
    dpiLayout->setSpacing(5);
    {
      dpiLayout->addWidget(m_dpiTypeOm, 0, 1, 1, 3);
      dpiLayout->addWidget(m_dpiLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_dpiFld, 1, 1);
      dpiLayout->addWidget(m_squarePixCB, 1, 2, 1, 2,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_widthLabel, 2, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_widthFld, 2, 1);
      dpiLayout->addWidget(m_heightLabel, 2, 2,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_heightFld, 2, 3);
      dpiLayout->addWidget(m_useCameraDpiBtn, 3, 1, 1, 3);
      dpiLayout->addWidget(m_cameraDpiTitle, 4, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_cameraDpiLabel, 4, 1, 1, 3);
      dpiLayout->addWidget(m_imageDpiTitle, 5, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_imageDpiLabel, 5, 1, 1, 3);
      dpiLayout->addWidget(m_imageResTitle, 6, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_imageResLabel, 6, 1, 1, 3);
    }
    dpiLayout->setColumnStretch(0, 0);
    dpiLayout->setColumnStretch(1, 1);
    dpiLayout->setColumnStretch(2, 0);
    dpiLayout->setColumnStretch(3, 1);
    dpiBox->setLayout(dpiLayout);

    m_topLayout->addWidget(dpiBox);

    m_topLayout->addWidget(m_doPremultiply);

    m_topLayout->addWidget(m_whiteTransp);

    m_topLayout->addWidget(m_doAntialias);

    //---subsampling
    QGridLayout *bottomLay = new QGridLayout();
    bottomLay->setMargin(3);
    bottomLay->setSpacing(3);
    {
      bottomLay->addWidget(new QLabel(tr("Antialias Softness:"), this), 0, 0);
      bottomLay->addWidget(m_antialiasSoftness, 0, 1);

      bottomLay->addWidget(m_subsamplingLabel, 1, 0);
      bottomLay->addWidget(m_subsamplingFld, 1, 1);
    }
    bottomLay->setColumnStretch(0, 0);
    bottomLay->setColumnStretch(1, 0);
    bottomLay->setColumnStretch(2, 1);
    m_topLayout->addLayout(bottomLay);

    m_topLayout->addStretch();
  }

  //----signal/slot connections
  connect(m_nameFld, SIGNAL(editingFinished()), SLOT(onNameChanged()));
  connect(m_pathFld, SIGNAL(pathChanged()), SLOT(onPathChanged()));
  connect(m_dpiTypeOm, SIGNAL(currentIndexChanged(int)),
          SLOT(onDpiTypeChanged(int)));
  connect(m_dpiFld, SIGNAL(editingFinished()), SLOT(onDpiFieldChanged()));
  connect(m_squarePixCB, SIGNAL(stateChanged(int)),
          SLOT(onSquarePixelChanged(int)));
  connect(m_widthFld, SIGNAL(editingFinished()), SLOT(onWidthFieldChanged()));
  connect(m_heightFld, SIGNAL(editingFinished()), SLOT(onHeightFieldChanged()));
  connect(m_useCameraDpiBtn, SIGNAL(clicked()), SLOT(useCameraDpi()));
  connect(m_subsamplingFld, SIGNAL(editingFinished()),
          SLOT(onSubsamplingChanged()));

  /*--- ScanPathの入力に対応 ---*/
  connect(m_scanPathFld, SIGNAL(pathChanged()), SLOT(onScanPathChanged()));
  connect(m_doPremultiply, SIGNAL(stateChanged(int)),
          SLOT(onDoPremultiplyChanged(int)));
  connect(m_doAntialias, SIGNAL(stateChanged(int)),
          SLOT(onDoAntialiasChanged(int)));
  connect(m_antialiasSoftness, SIGNAL(editingFinished()),
          SLOT(onAntialiasSoftnessChanged()));

  connect(m_whiteTransp, SIGNAL(stateChanged(int)),
          SLOT(onWhiteTranspChanged(int)));

  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::showEvent(QShowEvent *e) {
  bool ret =
      connect(TApp::instance()->getCurrentSelection(),
              SIGNAL(selectionSwitched(TSelection *, TSelection *)), this,
              SLOT(onSelectionSwitched(TSelection *, TSelection *)));
  ret = ret && connect(TApp::instance()->getCurrentSelection(),
                       SIGNAL(selectionChanged(TSelection *)),
                       SLOT(updateLevelSettings()));

  CastSelection *castSelection = dynamic_cast<CastSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (castSelection)
    ret = ret && connect(castSelection, SIGNAL(itemSelectionChanged()), this,
                         SLOT(onCastSelectionChanged()));

  /*--- Cleanupが行われたときに表示を更新するため ---*/
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(sceneChanged()), SLOT(onSceneChanged()));

  assert(ret);
  updateLevelSettings();
  if (Preferences::instance()->getUnits() == "pixel") {
    m_dpiTypeOm->hide();
    m_dpiLabel->hide();
    m_dpiFld->hide();
    m_squarePixCB->hide();
    m_useCameraDpiBtn->hide();
    m_cameraDpiLabel->hide();
    m_imageDpiLabel->hide();
    m_imageDpiTitle->hide();
    m_cameraDpiTitle->hide();
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
  } else {
    m_dpiTypeOm->show();
    m_dpiLabel->show();
    m_dpiFld->show();
    m_squarePixCB->show();
    m_useCameraDpiBtn->show();
    m_cameraDpiLabel->show();
    m_imageDpiLabel->show();
    m_imageDpiTitle->show();
    m_cameraDpiTitle->show();
    m_imageResTitle->show();
    m_imageResLabel->show();
    m_widthFld->setDecimals(4);
    m_heightFld->setDecimals(4);
  }
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::hideEvent(QHideEvent *e) {
  bool ret =
      disconnect(TApp::instance()->getCurrentSelection(),
                 SIGNAL(selectionSwitched(TSelection *, TSelection *)), this,
                 SLOT(onSelectionSwitched(TSelection *, TSelection *)));
  ret = ret && disconnect(TApp::instance()->getCurrentSelection(),
                          SIGNAL(selectionChanged(TSelection *)), this,
                          SLOT(updateLevelSettings()));

  CastSelection *castSelection = dynamic_cast<CastSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (castSelection)
    ret = ret && disconnect(castSelection, SIGNAL(itemSelectionChanged()), this,
                            SLOT(onCastSelectionChanged()));

  ret = ret && disconnect(TApp::instance()->getCurrentScene(),
                          SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));

  assert(ret);
  Dialog::hideEvent(e);
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSceneChanged() { updateLevelSettings(); }

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onCastSelectionChanged() { updateLevelSettings(); }

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSelectionSwitched(TSelection *oldSelection,
                                             TSelection *newSelection) {
  CastSelection *castSelection = dynamic_cast<CastSelection *>(newSelection);
  if (!castSelection) {
    CastSelection *oldCastSelection =
        dynamic_cast<CastSelection *>(oldSelection);
    if (oldCastSelection)
      disconnect(oldCastSelection, SIGNAL(itemSelectionChanged()), this,
                 SLOT(onCastSelectionChanged()));
    return;
  }
  connect(castSelection, SIGNAL(itemSelectionChanged()), this,
          SLOT(onCastSelectionChanged()));
}

//-----------------------------------------------------------------------------
/*! Update popup value.
                Take current level and act on level type set popup value.
*/
void LevelSettingsPopup::updateLevelSettings() {
  TApp *app = TApp::instance();
  TXshLevelP selectedLevel;
  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(app->getCurrentSelection()->getSelection());
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      app->getCurrentSelection()->getSelection());
  TColumnSelection *columnSelection = dynamic_cast<TColumnSelection *>(
      app->getCurrentSelection()->getSelection());
  FxSelection *fxSelection =
      dynamic_cast<FxSelection *>(app->getCurrentSelection()->getSelection());

  /*--セル選択の場合--*/
  if (cellSelection) {
    TXsheet *currentXsheet = app->getCurrentXsheet()->getXsheet();
    if (currentXsheet && !cellSelection->isEmpty()) {
      selectedLevel = 0;
      int r0, c0, r1, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);
      for (int c = c0; c <= c1; c++) {
        for (int r = r0; r <= r1; r++) {
          if (currentXsheet->getCell(r, c).m_level) {
            selectedLevel = currentXsheet->getCell(r, c).m_level;
            break;
          }
        }
        if (selectedLevel) break;
      }
    } else
      selectedLevel = app->getCurrentLevel()->getLevel();
  }
  /*--カラム選択の場合--*/
  else if (columnSelection) {
    TXsheet *currentXsheet = app->getCurrentXsheet()->getXsheet();
    if (currentXsheet && !columnSelection->isEmpty()) {
      selectedLevel   = 0;
      int sceneLength = currentXsheet->getFrameCount();

      std::set<int> columnIndices = columnSelection->getIndices();
      std::set<int>::iterator it;
      /*-- 選択Columnを探索、最初に見つかったLevelの内容を表示 --*/
      for (it = columnIndices.begin(); it != columnIndices.end(); ++it) {
        int columnIndex = *it;
        for (int r = 0; r < sceneLength; r++) {
          if (currentXsheet->getCell(r, columnIndex).m_level) {
            selectedLevel = currentXsheet->getCell(r, columnIndex).m_level;
            break;
          }
        }
        if (selectedLevel) break;
      }
    } else
      selectedLevel = app->getCurrentLevel()->getLevel();
  } else if (castSelection) {
    std::vector<TXshLevel *> levels;
    castSelection->getSelectedLevels(levels);

    int selectedLevelSize                    = levels.size();
    if (selectedLevelSize > 0) selectedLevel = levels[selectedLevelSize - 1];
  }
  /*-- Fx選択（Schematicノード選択）の場合 --*/
  else if (fxSelection) {
    selectedLevel           = 0;
    TXsheet *currentXsheet  = app->getCurrentXsheet()->getXsheet();
    QList<TFxP> selectedFxs = fxSelection->getFxs();
    if (currentXsheet && !selectedFxs.isEmpty()) {
      for (int f = 0; f < selectedFxs.size(); f++) {
        TLevelColumnFx *lcfx =
            dynamic_cast<TLevelColumnFx *>(selectedFxs.at(f).getPointer());
        if (lcfx) {
          int firstRow = lcfx->getXshColumn()->getCellColumn()->getFirstRow();
          TXshLevelP levelP =
              lcfx->getXshColumn()->getCellColumn()->getCell(firstRow).m_level;
          if (levelP) {
            selectedLevel = levelP;
            break;
          }
        }
      }
      if (!selectedLevel) selectedLevel = app->getCurrentLevel()->getLevel();
    } else
      selectedLevel = app->getCurrentLevel()->getLevel();
    // std::cout<<"fxSelection is current!"<<std::endl;
  } else
    selectedLevel = app->getCurrentLevel()->getLevel();

  m_sl  = dynamic_cast<TXshSimpleLevel *>(selectedLevel.getPointer());
  m_pl  = dynamic_cast<TXshPaletteLevel *>(selectedLevel.getPointer());
  m_cl  = dynamic_cast<TXshChildLevel *>(selectedLevel.getPointer());
  m_sdl = dynamic_cast<TXshSoundLevel *>(selectedLevel.getPointer());

  bool isSimpleLevel = m_sl;
  bool isChildLevel  = m_cl;
  bool isRasterLevel = m_sl && (m_sl->getType() & RASTER_TYPE);
  bool isTzpLevel    = m_sl && (m_sl->getType() == TZP_XSHLEVEL);
  bool isMeshLevel   = m_sl && (m_sl->getType() == MESH_XSHLEVEL);

  bool hasDpiEditing = (isRasterLevel || isMeshLevel);

  // name
  if (selectedLevel) {
    m_nameFld->setText(::to_string(selectedLevel->getName()).c_str());
    m_nameFld->setEnabled(true);
  } else {
    m_nameFld->setText(tr(""));
    m_nameFld->setEnabled(false);
  }

  // path
  if (m_sl) {
    m_pathFld->setPath(toQString(m_sl->getPath()));
    if (m_scanPathFld)
      m_scanPathFld->setPath(toQString(m_sl->getScannedPath()));
  } else if (m_pl) {
    m_pathFld->setPath(toQString(m_pl->getPath()));
    if (m_scanPathFld) m_scanPathFld->setPath(tr(""));
  } else if (m_sdl) {
    m_pathFld->setPath(toQString(m_sdl->getPath()));
    if (m_scanPathFld) m_scanPathFld->setPath(tr(""));
  } else {
    m_pathFld->setPath(tr(""));
    if (m_scanPathFld) m_scanPathFld->setPath(tr(""));
  }

  // leveltype
  QString levelTypeString = QString(tr(""));
  if (m_sl) {
    switch (m_sl->getType()) {
    case TZI_XSHLEVEL:
      levelTypeString = tr("Scan level");
      break;
    case PLI_XSHLEVEL:
      levelTypeString = tr("Toonz Vector level");
      break;
    case TZP_XSHLEVEL:
      levelTypeString = tr("Toonz Raster level");
      break;
    case OVL_XSHLEVEL:
      levelTypeString = tr("Raster level");
      break;
    case MESH_XSHLEVEL:
      levelTypeString = tr("Mesh level");
      break;
    default:
      levelTypeString = "?";
      break;
    }
  } else if (m_pl)
    levelTypeString = tr("Palette level");
  else if (m_sdl)
    levelTypeString = tr("Sound Column");

  m_typeLabel->setText(levelTypeString);

  // dpi & res & resampling
  if (hasDpiEditing) {
    LevelProperties::DpiPolicy dpiPolicy =
        m_sl->getProperties()->getDpiPolicy();
    assert(dpiPolicy == LevelProperties::DP_ImageDpi ||
           dpiPolicy == LevelProperties::DP_CustomDpi);
    m_dpiTypeOm->setCurrentIndex(
        (dpiPolicy == LevelProperties::DP_ImageDpi) ? 0 : 1);

    // dpi field
    TPointD dpi = m_sl->getDpi();
    m_dpiFld->setText(dpiToString(dpi));
    m_dpiFld->setCursorPosition(0);

    // image dpi
    m_imageDpiLabel->setText(dpiToString(m_sl->getImageDpi()));

    if (isRasterLevel) {
      // size field
      TDimensionD size(0, 0);
      TDimension res = m_sl->getResolution();
      if (res.lx > 0 && res.ly > 0 && dpi.x > 0 && dpi.y > 0) {
        size.lx = res.lx / dpi.x;
        size.ly = res.ly / dpi.y;
        m_widthFld->setValue(tround(size.lx * 100.0) / 100.0);
        m_heightFld->setValue(tround(size.ly * 100.0) / 100.0);
      } else {
        m_widthFld->setText(tr(""));
        m_heightFld->setText(tr(""));
      }

      // image res
      TDimension imageRes = m_sl->getResolution();
      m_imageResLabel->setText(QString::number(imageRes.lx) + "x" +
                               QString::number(imageRes.ly));

      // subsampling
      m_subsamplingFld->setValue(m_sl->getProperties()->getSubsampling());

      // doPremultiply
      m_doPremultiply->setChecked(m_sl->getProperties()->doPremultiply());
      if (m_whiteTransp)
        m_whiteTransp->setChecked(m_sl->getProperties()->whiteTransp());

      m_antialiasSoftness->setValue(m_sl->getProperties()->antialiasSoftness());
      m_doAntialias->setChecked(m_sl->getProperties()->antialiasSoftness() > 0);
    }
  } else {
    m_dpiFld->setText(tr(""));
    m_widthFld->setText(tr(""));
    m_heightFld->setText(tr(""));

    m_cameraDpiLabel->setText(tr(""));
    m_imageDpiLabel->setText(tr(""));
    m_imageResLabel->setText(tr(""));

    m_subsamplingFld->setText(tr(""));

    m_doPremultiply->setChecked(false);
    m_doAntialias->setChecked(false);
    if (m_whiteTransp) m_whiteTransp->setChecked(false);
  }

  // camera dpi
  m_cameraDpiLabel->setText(dpiToString(getCurrentCameraDpi()));

  m_nameFld->setEnabled((isSimpleLevel || isChildLevel || !!m_pl || !!m_sdl));
  m_pathFld->setEnabled((isSimpleLevel || !!m_sdl || !!m_pl));
  if (m_scanPathLabel) m_scanPathLabel->setEnabled(isTzpLevel);
  if (m_scanPathFld) m_scanPathFld->setEnabled(isTzpLevel);
  m_typeLabel->setEnabled(isSimpleLevel || !!m_pl || !!m_sdl);
  m_dpiTypeOm->setEnabled((isSimpleLevel && m_sl->getImageDpi() != TPointD()));
  m_squarePixCB->setEnabled(hasDpiEditing);
  m_dpiLabel->setEnabled(hasDpiEditing);
  m_dpiFld->setEnabled(hasDpiEditing);
  m_widthLabel->setEnabled(isRasterLevel);
  m_widthFld->setEnabled(isRasterLevel);
  m_heightLabel->setEnabled(isRasterLevel);
  m_heightFld->setEnabled(isRasterLevel);
  m_useCameraDpiBtn->setEnabled(hasDpiEditing);
  m_subsamplingLabel->setEnabled(
      (isRasterLevel && m_sl && !m_sl->getProperties()->getDirtyFlag()));
  m_subsamplingFld->setEnabled(
      (isRasterLevel && m_sl && !m_sl->getProperties()->getDirtyFlag()));
  m_doPremultiply->setEnabled(m_sl && isRasterLevel && !isTzpLevel);
  m_doAntialias->setEnabled(m_sl && isRasterLevel);
  if (m_whiteTransp)
    m_whiteTransp->setEnabled(m_sl && isRasterLevel && !isTzpLevel);
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onNameChanged() {
  QString text     = m_nameFld->text();
  TXshLevel *level = 0;
  if (m_sl)
    level = m_sl.getPointer();
  else if (m_cl)
    level = m_cl.getPointer();
  else
    return;

  if (text.length() == 0) {
    error("The name " + text +
          " you entered for the level is not valid.\n Please enter a different "
          "name.");
    m_nameFld->setFocus();
    return;
  }

  /*-- Level名に変更がない場合 --*/
  if (level->getName() == text.toStdWString()) {
    // warning("Level name unchanged.");
    return;
  }

  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();

  if (!levelSet->renameLevel(level, text.toStdWString())) {
    error("The name " + text +
          " you entered for the level is already used.\nPlease enter a "
          "different name.");
    m_nameFld->setFocus();
    return;
  }

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onPathChanged() {
  QString text = m_pathFld->getPath();
  TFilePath newPath(text.toStdWString());
  newPath =
      TApp::instance()->getCurrentScene()->getScene()->codeFilePath(newPath);
  m_pathFld->setPath(QString::fromStdWString(newPath.getWideString()));
  if (!m_sl && !!m_sdl) {
    // old level is a sound level
    TFileType::Type levelType = TFileType::getInfo(newPath);
    if (levelType == TFileType::AUDIO_LEVEL) {
      TFilePath oldPath = m_sdl->getPath();
      if (oldPath == newPath) return;
      m_sdl->setPath(newPath);
      m_sdl->loadSoundTrack();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
    } else {
      error(tr("The file %1 is not a sound level.")
                .arg(QString::fromStdWString(newPath.getLevelNameW())));
      updateLevelSettings();
    }
    return;
  }

  if (!m_sl) return;
  TFilePath oldPath = m_sl->getPath();
  if (oldPath == newPath) return;

  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  TXshSimpleLevel *sl = 0;
  for (int i = 0; i < levelSet->getLevelCount(); i++) {
    TXshLevel *xl = levelSet->getLevel(i);
    if (!xl) continue;
    sl = xl->getSimpleLevel();
    if (!sl) continue;
    if (sl == m_sl.getPointer()) {
      sl = 0;
      continue;
    }
    if (sl->getPath() == newPath) break;
    sl = 0;
  }
  if (sl) {
    QString question;

    question = "The path you entered for the level " +
               QString(::to_string(sl->getName()).c_str()) +
               "is already used: this may generate some conflicts in the file "
               "management.\nAre you sure you want to assign the same path to "
               "two different levels?";
    int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
    if (ret == 0 || ret == 2) {
      m_pathFld->setPath(toQString(m_sl->getPath()));
      return;
    }
  }

  TFileType::Type oldType = TFileType::getInfo(oldPath);
  TFileType::Type newType = TFileType::getInfo(newPath);

  if (m_sl->getType() == TZP_XSHLEVEL &&
      m_sl->getScannedPath() != TFilePath()) {
    if (newPath == TFilePath() || newPath == m_sl->getScannedPath()) {
      newPath = m_sl->getScannedPath();
      m_sl->setType(OVL_XSHLEVEL);
      m_sl->setScannedPath(TFilePath());
      m_sl->setPath(newPath);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->notifyCastChange();
      updateLevelSettings();
      m_sl->invalidateFrames();
      std::vector<TFrameId> frames;
      m_sl->getFids(frames);
      for (auto const &fid : frames) {
        IconGenerator::instance()->invalidate(m_sl.getPointer(), fid);
      }
      return;
    }
  }

  if (oldType != newType ||
      m_sl->getType() == TZP_XSHLEVEL && newPath.getType() != "tlv" ||
      m_sl->getType() != TZP_XSHLEVEL && newPath.getType() == "tlv") {
    error("Wrong path");
    m_pathFld->setPath(toQString(m_sl->getPath()));
    return;
  }
  /*-- ここでPathを更新 --*/
  m_sl->setPath(newPath);
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->setPalette(m_sl->getPalette());

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  m_sl->invalidateFrames();
  std::vector<TFrameId> frames;
  m_sl->getFids(frames);
  for (auto const &fid : frames) {
    IconGenerator::instance()->invalidate(m_sl.getPointer(), fid);
  }
  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onScanPathChanged() {
  QString text = m_scanPathFld->getPath();
  TFilePath newScanPath(text.toStdWString());

  /*--対応する拡張子で無い場合は、表示を元に戻してreturn--*/
  /* TODO:
   * 他のFormatはDPI情報が無いため省いているが、ラスタ画像なら問題ないか？後で検討のこと
   * 2016/1/30 shun_iwasawa */
  if (newScanPath.getType() != "tif" && newScanPath.getType() != "tiff" &&
      newScanPath.getType() != "tzi") {
    m_scanPathFld->setPath(toQString(m_sl->getScannedPath()));
    return;
  }

  newScanPath = TApp::instance()->getCurrentScene()->getScene()->codeFilePath(
      newScanPath);
  m_scanPathFld->setPath(QString::fromStdWString(newScanPath.getWideString()));

  if (!m_sl || m_sl->getType() != TZP_XSHLEVEL) return;

  /*-- 入力されたScanPathに変更がなければreturn--*/
  TFilePath oldScanPath = m_sl->getScannedPath();
  if (oldScanPath == newScanPath) return;

  /*-- CleanupSettingsのSaveInが今のLevelと一致しているかどうかをチェック --*/
  TFilePath settingsPath =
      (newScanPath.getParentDir() + newScanPath.getName()).withType("cln");
  settingsPath =
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(
          settingsPath);
  if (TSystem::doesExistFileOrLevel(settingsPath)) {
    TIStream is(settingsPath);
    TFilePath cleanupLevelPath;
    std::string tagName;
    while (is.matchTag(tagName)) {
      if (tagName == "cleanupPalette" || tagName == "cleanupCamera" ||
          tagName == "autoCenter" || tagName == "transform" ||
          tagName == "lineProcessing" || tagName == "closestField" ||
          tagName == "fdg") {
        is.skipCurrentTag();
      } else if (tagName == "path") {
        is >> cleanupLevelPath;
        is.closeChild();
      } else
        std::cout << "mismatch tag:" << tagName << std::endl;
    }

    if (cleanupLevelPath != TFilePath()) {
      TFilePath codedCleanupLevelPath =
          TApp::instance()->getCurrentScene()->getScene()->codeFilePath(
              cleanupLevelPath);
      if (m_sl->getPath().getParentDir() != cleanupLevelPath &&
          m_sl->getPath().getParentDir() != codedCleanupLevelPath)
        DVGui::warning(
            "\"Save In\" path of the Cleanup Settings does not match.");
    } else
      DVGui::warning("Loading Cleanup Settings failed.");
  } else
    DVGui::warning(".cln file for the Scanned Level not found.");

  m_sl->setScannedPath(newScanPath);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onDpiTypeChanged(int index) {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  QString dpiPolicyStr = m_dpiTypeOm->itemData(index).toString();
  assert(dpiPolicyStr == "Custom DPI" || dpiPolicyStr == "Image DPI");
  LevelProperties *prop = m_sl->getProperties();

  // dpiPolicyStr ==> dpiPolicy
  LevelProperties::DpiPolicy dpiPolicy = dpiPolicyStr == "Custom DPI"
                                             ? LevelProperties::DP_CustomDpi
                                             : LevelProperties::DP_ImageDpi;
  // se ImageDpi, ma l'immagine non ha dpi -> CustomDpi
  assert(dpiPolicy == LevelProperties::DP_CustomDpi ||
         m_sl->getImageDpi().x > 0.0 && m_sl->getImageDpi().y > 0.0);

  if (prop->getDpiPolicy() == dpiPolicy) return;
  prop->setDpiPolicy(dpiPolicy);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onDpiFieldChanged() {
  QString w = m_dpiFld->text();
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;

  std::string s = w.toStdString();
  TPointD dpi;

  int i = 0, len = s.length();
  while (i < len && s[i] == ' ') i++;
  int j = i;
  while (j < len && '0' <= s[j] && s[j] <= '9') j++;
  if (j < len && s[j] == '.') {
    j++;
    while (j < len && '0' <= s[j] && s[j] <= '9') j++;
  }
  if (i < j) {
    dpi.x = std::stod(s.substr(i, j - i));
    i     = j;
    while (i < len && s[i] == ' ') i++;
    if (i < len && s[i] == ',') {
      i++;
      while (i < len && s[i] == ' ') i++;
      dpi.y = std::stod(s.substr(i));
    }
  }

  if (dpi.x > 0.0) {
    if (dpi.y == 0.0) dpi.y = dpi.x;
    m_sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    m_sl->getProperties()->setDpi(dpi);
  }
  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSquarePixelChanged(int value) {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  if (!value) return;
  TPointD dpi = m_sl->getDpi();
  if (areAlmostEqual(dpi.x, dpi.y)) return;
  TDimension res = m_sl->getResolution();
  dpi.y          = dpi.x;
  m_sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
  m_sl->getProperties()->setDpi(dpi);
  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onWidthFieldChanged() {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  TDimension res = m_sl->getResolution();
  TPointD dpi    = m_sl->getDpi();
  if (dpi.x <= 0) return;
  double oldLx = res.lx / dpi.x;
  double lx    = m_widthFld->getValue();
  double ly    = m_heightFld->getValue();
  if (lx <= 0) {
    lx = oldLx;
    m_widthFld->setValue(oldLx);
  }

  if (m_squarePixCB->isChecked()) {
    ly = (double)(res.ly * lx) / (double)res.lx;
    m_heightFld->setValue(ly);
  }

  dpi.x = res.lx / lx;
  dpi.y = res.ly / ly;

  m_sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
  m_sl->getProperties()->setDpi(dpi);

  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onHeightFieldChanged() {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  TDimension res = m_sl->getResolution();
  if (res.ly == 0 || res.ly <= 0) return;
  TPointD dpi = m_sl->getDpi();
  if (dpi.y <= 0) return;
  double oldLy = res.ly / dpi.y;
  double lx    = m_widthFld->getValue();
  double ly    = m_heightFld->getValue();
  if (ly <= 0) {
    ly = oldLy;
    m_heightFld->setValue(oldLy);
  }

  if (m_squarePixCB->isChecked()) {
    lx = (double)(res.lx * ly) / (double)res.ly;
    m_widthFld->setValue(lx);
  }

  dpi.x = res.lx / lx;
  dpi.y = res.ly / ly;

  m_sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
  m_sl->getProperties()->setDpi(dpi);

  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::useCameraDpi() {
  if (!m_sl) return;
  LevelProperties *prop = m_sl->getProperties();
  prop->setDpiPolicy(LevelProperties::DP_CustomDpi);
  prop->setDpi(getCurrentCameraDpi());
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSubsamplingChanged() {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  if (m_sl->getProperties()->getDirtyFlag()) return;

  /*-- subSamplingの値に変更が無い場合はreturn --*/
  if (m_sl->getProperties()->getSubsampling() ==
      (int)m_subsamplingFld->getValue())
    return;

  m_sl->getProperties()->setSubsampling((int)m_subsamplingFld->getValue());
  m_sl->invalidateFrames();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()
      ->getCurrentXsheet()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onDoPremultiplyChanged(int value) {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  if (value) {
    if (m_whiteTransp) m_whiteTransp->setChecked(false);
    m_sl->getProperties()->setWhiteTransp(false);
  }
  m_sl->getProperties()->setDoPremultiply(value);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onAntialiasSoftnessChanged() {
  m_sl->getProperties()->setDoAntialias(
      m_doAntialias->isChecked() ? m_antialiasSoftness->getValue() : 0);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//----------------------------

void LevelSettingsPopup::onDoAntialiasChanged(int value) {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  m_antialiasSoftness->setEnabled(value != 0);

  m_sl->getProperties()->setDoAntialias(
      value == 0 ? 0 : m_antialiasSoftness->getValue());
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onWhiteTranspChanged(int value) {
  if (!m_sl || m_sl->getType() == PLI_XSHLEVEL) return;
  if (value) {
    m_doPremultiply->setChecked(false);
    m_sl->getProperties()->setDoPremultiply(false);
  }
  m_sl->getProperties()->setWhiteTransp(value);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<LevelSettingsPopup> openLevelSettingsPopup(
    MI_LevelSettings);

//-----------------------------------------------------------------------------

class ViewLevelFileInfoHandler final : public MenuItemHandler {
public:
  ViewLevelFileInfoHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}

  void execute() override {
    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();

    if (FileSelection *fileSelection =
            dynamic_cast<FileSelection *>(selection)) {
      fileSelection->viewFileInfo();
      return;
    }

    std::vector<TXshLevel *> selectedLevels;
    /*-- Cell選択ContextMenu→Infoコマンドの場合は、セル内のLevelの情報を表示する
     * --*/
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
    if (cellSelection && !cellSelection->isEmpty()) {
      TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
      int r0, c0, r1, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);
      for (int c = c0; c <= c1; c++) {
        for (int r = r0; r <= r1; r++) {
          TXshLevel *lv = xsh->getCell(r, c).m_level.getPointer();
          if (!lv) continue;
          std::vector<TXshLevel *>::iterator lvIt =
              find(selectedLevels.begin(), selectedLevels.end(), lv);
          if (lvIt == selectedLevels.end()) selectedLevels.push_back(lv);
        }
      }
    } else if (CastSelection *castSelection =
                   dynamic_cast<CastSelection *>(selection))
      castSelection->getSelectedLevels(selectedLevels);
    else if (TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel())
      selectedLevels.push_back(level);

    if (selectedLevels.empty()) return;

    for (int i = 0; i < selectedLevels.size(); ++i) {
      if (selectedLevels[i]->getSimpleLevel() ||
          selectedLevels[i]->getSoundLevel()) {
        TFilePath path = selectedLevels[i]->getPath();
        path           = selectedLevels[i]->getScene()->decodeFilePath(path);

        if (TSystem::doesExistFileOrLevel(path)) {
          InfoViewer *infoViewer = 0;
          infoViewer             = new InfoViewer();
          infoViewer->setItem(0, 0, path);
        }
      }
    }
  }

} viewLevelFileInfoHandler(MI_FileInfo);

//-----------------------------------------------------------------------------

class ViewLevelHandler final : public MenuItemHandler {
public:
  ViewLevelHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}

  void execute() override {
    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();
    if (FileSelection *fileSelection =
            dynamic_cast<FileSelection *>(selection)) {
      fileSelection->viewFile();
      return;
    }

    TXshSimpleLevel *sl = 0;
    int i               = 0;
    std::vector<TXshSimpleLevel *> simpleLevels;
    /*-- セル選択ContextMenu→Viewコマンドの場合 --*/
    TXsheet *currentXsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
    if (currentXsheet && cellSelection && !cellSelection->isEmpty()) {
      int r0, c0, r1, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);

      for (int c = c0; c <= c1; c++) {
        for (int r = r0; r <= r1; r++) {
          TXshSimpleLevel *lv = currentXsheet->getCell(r, c).getSimpleLevel();
          if (!lv) continue;
          std::vector<TXshSimpleLevel *>::iterator lvIt =
              find(simpleLevels.begin(), simpleLevels.end(), lv);
          if (lvIt == simpleLevels.end()) simpleLevels.push_back(lv);
        }
      }
    } else if (CastSelection *castSelection =
                   dynamic_cast<CastSelection *>(selection)) {
      std::vector<TXshLevel *> selectedLevels;
      castSelection->getSelectedLevels(selectedLevels);
      for (i = 0; i < selectedLevels.size(); i++)
        simpleLevels.push_back(selectedLevels[i]->getSimpleLevel());
    } else if (TApp::instance()->getCurrentLevel()->getSimpleLevel())
      simpleLevels.push_back(
          TApp::instance()->getCurrentLevel()->getSimpleLevel());

    if (!simpleLevels.size()) return;
    for (i = 0; i < simpleLevels.size(); i++) {
      if (!(simpleLevels[i]->getType() & LEVELCOLUMN_XSHLEVEL)) continue;

      TFilePath path = simpleLevels[i]->getPath();
      path           = simpleLevels[i]->getScene()->decodeFilePath(path);
      FlipBook *fb;
      if (TSystem::doesExistFileOrLevel(path))
        fb = ::viewFile(path);
      else {
        fb = FlipBookPool::instance()->pop();
        fb->setLevel(simpleLevels[i]);
      }
      if (fb) {
        FileBrowserPopup::setModalBrowserToParent(fb->parentWidget());
      }
    }
  }

} ViewLevelHandler(MI_ViewFile);
