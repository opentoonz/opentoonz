

#include "levelcreatepopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "levelcommand.h"
#include "formatsettingspopups.h"

// TnzTools includes
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/preferences.h"
#include "toonz/palettecontroller.h"
#include "toonz/tproject.h"
#include "toonz/namebuilder.h"
#include "toonz/childstack.h"
#include "toutputproperties.h"

// TnzCore includes
#include "tsystem.h"
#include "tpalette.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tmetaimage.h"
#include "timagecache.h"
#include "tundo.h"
#include "filebrowsermodel.h"

// Qt includes
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QMainWindow>
#include <QRegularExpressionValidator>
#include <QRegularExpression>

using namespace DVGui;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// CreateLevelUndo
//-----------------------------------------------------------------------------

class CreateLevelUndo final : public TUndo {
  int m_rowIndex;
  int m_columnIndex;
  int m_frameCount;
  int m_oldLevelCount;
  int m_step;
  TXshSimpleLevelP m_sl;
  bool m_areColumnsShifted;

public:
  CreateLevelUndo(int row, int column, int frameCount, int step,
                  bool areColumnsShifted)
      : m_rowIndex(row)
      , m_columnIndex(column)
      , m_frameCount(frameCount)
      , m_step(step)
      , m_sl(nullptr)
      , m_areColumnsShifted(areColumnsShifted) {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    m_oldLevelCount   = scene->getLevelSet()->getLevelCount();
  }

  ~CreateLevelUndo() override = default;

  void onAdd(const TXshSimpleLevelP &sl) { m_sl = sl; }

  void undo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();

    if (m_areColumnsShifted) {
      xsh->removeColumn(m_columnIndex);
    } else if (m_frameCount > 0) {
      xsh->removeCells(m_rowIndex, m_columnIndex, m_frameCount);
    }

    if (TLevelSet *levelSet = scene->getLevelSet()) {
      int m = levelSet->getLevelCount();
      while (m > 0 && m > m_oldLevelCount) {
        --m;
        if (TXshLevel *level = levelSet->getLevel(m)) {
          levelSet->removeLevel(level);
        }
      }
    }

    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    if (!m_sl) return;

    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    // Using getPointer() instead of get()
    scene->getLevelSet()->insertLevel(m_sl.getPointer());

    TXsheet *xsh = scene->getXsheet();
    if (m_areColumnsShifted) {
      xsh->insertColumn(m_columnIndex);
    }

    std::vector<TFrameId> fids;
    m_sl->getFids(fids);

    int i = m_rowIndex;
    int f = 0;
    while (i < m_frameCount + m_rowIndex) {
      TFrameId fid = (!fids.empty()) ? fids[f] : TFrameId(i);
      // Using getPointer() instead of get()
      TXshCell cell(m_sl.getPointer(), fid);
      f++;
      xsh->setCell(i, m_columnIndex, cell);
      int temp = i++;

      while (i < m_step + temp) {
        xsh->setCell(i++, m_columnIndex, cell);
      }
    }

    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Create Level %1 at Column %2")
        .arg(QString::fromStdWString(m_sl->getName()))
        .arg(QString::number(m_columnIndex + 1));
  }
};

//-----------------------------------------------------------------------------
}  // anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================
/*! \class LevelCreatePopup
    \brief The LevelCreatePopup class provides a modal dialog to create a new
   level.

    Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

LevelCreatePopup::LevelCreatePopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "LevelCreate") {
  setWindowTitle(tr("New Level"));

  m_nameFld     = new LineEdit(this);
  m_fromFld     = new DVGui::IntLineEdit(this);
  m_toFld       = new DVGui::IntLineEdit(this);
  m_stepFld     = new DVGui::IntLineEdit(this);
  m_incFld      = new DVGui::IntLineEdit(this);
  m_levelTypeOm = new QComboBox();

  m_pathFld     = new FileField(nullptr);
  m_widthLabel  = new QLabel(tr("Width:"));
  m_widthFld    = new DVGui::MeasuredDoubleLineEdit(nullptr);
  m_heightLabel = new QLabel(tr("Height:"));
  m_heightFld   = new DVGui::MeasuredDoubleLineEdit(nullptr);
  m_dpiLabel    = new QLabel(tr("DPI:"));
  m_dpiFld      = new DoubleLineEdit(nullptr, 66.76);

  m_rasterFormatLabel = new QLabel(tr("Format:"));
  m_rasterFormatOm    = new QComboBox();
  m_frameFormatBtn    = new QPushButton(tr("Frame Format"));

  auto *okBtn     = new QPushButton(tr("OK"), this);
  auto *cancelBtn = new QPushButton(tr("Cancel"), this);
  auto *applyBtn  = new QPushButton(tr("Apply"), this);

  // Replace QRegExp with QRegularExpression
  // Exclude all characters which cannot fit in a filepath (Windows).
  // Dots are also prohibited since they are internally managed by Toonz.
  QRegularExpression rx(R"([^\\/:?*."<>|]+)");
  m_nameFld->setValidator(new QRegularExpressionValidator(rx, this));

  m_levelTypeOm->addItem(tr("Toonz Vector Level"), PLI_XSHLEVEL);
  m_levelTypeOm->addItem(tr("Toonz Raster Level"), TZP_XSHLEVEL);
  m_levelTypeOm->addItem(tr("Raster Level"), OVL_XSHLEVEL);
  m_levelTypeOm->addItem(tr("Scan Level"), TZI_XSHLEVEL);
  m_levelTypeOm->addItem(tr("Assistants Level"), META_XSHLEVEL);

  if (Preferences::instance()->getUnits() == "pixel") {
    m_widthFld->setMeasure("camera.lx");
    m_heightFld->setMeasure("camera.ly");
  } else {
    m_widthFld->setMeasure("level.lx");
    m_heightFld->setMeasure("level.ly");
  }

  m_widthFld->setRange(0.1, std::numeric_limits<double>::max());
  m_heightFld->setRange(0.1, std::numeric_limits<double>::max());
  m_dpiFld->setRange(0.1, std::numeric_limits<double>::max());

  m_rasterFormatOm->addItem("tif", "tif");
  m_rasterFormatOm->addItem("png", "png");
  m_rasterFormatOm->setCurrentIndex(m_rasterFormatOm->findData(
      Preferences::instance()->getDefRasterFormat()));

  okBtn->setDefault(true);

  // Layout
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->setSpacing(0);

  auto *guiLay = new QGridLayout();
  guiLay->setContentsMargins(10, 10, 10, 10);
  guiLay->setVerticalSpacing(10);
  guiLay->setHorizontalSpacing(5);

  // Name
  guiLay->addWidget(new QLabel(tr("Name:")), 0, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_nameFld, 0, 1, 1, 4);

  // From-To
  guiLay->addWidget(new QLabel(tr("From:")), 1, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_fromFld, 1, 1);
  guiLay->addWidget(new QLabel(tr("To:")), 1, 2,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_toFld, 1, 3);

  // Step-Increment
  guiLay->addWidget(new QLabel(tr("Step:")), 2, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_stepFld, 2, 1);
  guiLay->addWidget(new QLabel(tr("Increment:")), 2, 2,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_incFld, 2, 3);

  // Type
  guiLay->addWidget(new QLabel(tr("Type:")), 3, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_levelTypeOm, 3, 1, 1, 3);

  // Save In
  guiLay->addWidget(new QLabel(tr("Save In:")), 4, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_pathFld, 4, 1, 1, 4);

  // Format options (for Raster/Scan levels)
  guiLay->addWidget(m_rasterFormatLabel, 5, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_rasterFormatOm, 5, 1, Qt::AlignLeft);
  guiLay->addWidget(m_frameFormatBtn, 5, 2, 1, 2, Qt::AlignLeft);

  // Width - Height
  guiLay->addWidget(m_widthLabel, 6, 0, Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_widthFld, 6, 1);
  guiLay->addWidget(m_heightLabel, 6, 2, Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_heightFld, 6, 3);

  // DPI
  guiLay->addWidget(m_dpiLabel, 7, 0, Qt::AlignRight | Qt::AlignVCenter);
  guiLay->addWidget(m_dpiFld, 7, 1);

  guiLay->setColumnStretch(0, 0);
  guiLay->setColumnStretch(1, 0);
  guiLay->setColumnStretch(2, 0);
  guiLay->setColumnStretch(3, 0);
  guiLay->setColumnStretch(4, 1);

  m_topLayout->addLayout(guiLay, 1);

  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(30);
  {
    m_buttonLayout->addStretch(1);
    m_buttonLayout->addWidget(okBtn, 0);
    m_buttonLayout->addWidget(applyBtn, 0);
    m_buttonLayout->addWidget(cancelBtn, 0);
    m_buttonLayout->addStretch(1);
  }

  // Modern signal-slot connections using function pointers
  connect(m_levelTypeOm, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &LevelCreatePopup::onLevelTypeChanged);
  connect(m_frameFormatBtn, &QPushButton::clicked, this,
          &LevelCreatePopup::onFrameFormatButton);
  connect(okBtn, &QPushButton::clicked, this, &LevelCreatePopup::onOkBtn);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(applyBtn, &QPushButton::clicked, this,
          &LevelCreatePopup::onApplyButton);

  setSizeWidgetEnable(false);
  setRasterWidgetVisible(false);
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::updatePath() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath defaultPath =
      scene->getDefaultLevelPath(getLevelType()).getParentDir();
  m_pathFld->setPath(toQString(defaultPath));
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::nextName() {
  const auto nameBuilder =
      std::unique_ptr<NameBuilder>(NameBuilder::getBuilder(L""));

  std::wstring levelName;

  // Select a different unique level name in case it already exists (either in
  // scene or on disk)
  while (true) {
    levelName = nameBuilder->getNext();
    if (!levelExists(levelName)) {
      break;
    }
  }

  m_nameFld->setText(QString::fromStdWString(levelName));
}

//-----------------------------------------------------------------------------

bool LevelCreatePopup::levelExists(const std::wstring &levelName) {
  ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();
  TLevelSet *levelSet = scene->getLevelSet();

  TFilePath parentDir(m_pathFld->getPath().toStdWString());
  TFilePath fp = scene->getDefaultLevelPath(getLevelType(), levelName)
                     .withParentDir(parentDir);
  TFilePath actualFp = scene->decodeFilePath(fp);

  if (TSystem::doesExistFileOrLevel(actualFp)) {
    return true;
  }

  if (TXshLevel *level = levelSet->getLevel(levelName)) {
    // Even if the level exists in the scene cast, it can be replaced if it is
    // unused
    if (Preferences::instance()->isAutoRemoveUnusedLevelsEnabled() &&
        !scene->getChildStack()->getTopXsheet()->isLevelUsed(level)) {
      return false;
    }
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------
void LevelCreatePopup::showEvent(QShowEvent *event) {
  Dialog::showEvent(event);
  update();
  nextName();
  m_nameFld->setFocus();

  if (Preferences::instance()->getUnits() == "pixel") {
    m_dpiFld->hide();
    m_dpiLabel->hide();
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
  } else {
    m_dpiFld->show();
    m_dpiLabel->show();
    m_widthFld->setDecimals(4);
    m_heightFld->setDecimals(4);
  }
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::setSizeWidgetEnable(bool isEnable) {
  m_widthLabel->setEnabled(isEnable);
  m_heightLabel->setEnabled(isEnable);
  m_widthFld->setEnabled(isEnable);
  m_heightFld->setEnabled(isEnable);
  m_dpiLabel->setEnabled(isEnable);
  m_dpiFld->setEnabled(isEnable);
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::setRasterWidgetVisible(bool isVisible) {
  m_rasterFormatLabel->setVisible(isVisible);
  m_rasterFormatOm->setVisible(isVisible);
  m_frameFormatBtn->setVisible(isVisible);
  updateGeometry();
}

//-----------------------------------------------------------------------------

int LevelCreatePopup::getLevelType() const {
  return m_levelTypeOm->currentData().toInt();
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::onLevelTypeChanged(int index) {
  int type = m_levelTypeOm->itemData(index).toInt();

  setSizeWidgetEnable(type == OVL_XSHLEVEL || type == TZP_XSHLEVEL);
  setRasterWidgetVisible(type == OVL_XSHLEVEL || type == TZI_XSHLEVEL);

  updatePath();

  std::wstring levelName = m_nameFld->text().toStdWString();
  // Check if the name already exists or if it is a 1-letter name
  // One-letter names are most likely created automatically so
  // this ensures that automatically created names don't skip a letter.
  if (levelExists(levelName) || levelName.length() == 1) {
    nextName();
  }
  m_nameFld->setFocus();
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::onOkBtn() {
  if (apply()) {
    close();
  } else {
    m_nameFld->setFocus();
  }
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::onApplyButton() {
  if (apply()) {
    nextName();
  }
  m_nameFld->setFocus();
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::onFrameFormatButton() {
  // Tentatively use the preview output settings
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  std::string ext = m_rasterFormatOm->currentData().toString().toStdString();
  openFormatSettingsPopup(this, ext, nullptr,
                          &scene->getProperties()->formatTemplateFIdForInput());
}

//-----------------------------------------------------------------------------

bool LevelCreatePopup::apply() {
  TApp *app = TApp::instance();
  int row   = app->getCurrentFrame()->getFrame();
  int col   = app->getCurrentColumn()->getColumnIndex();

  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  bool validColumn = true;
  if (TXshColumn *column = xsh->getColumn(col)) {
    validColumn = (column->getColumnType() == TXshColumn::eLevelType);
  }

  int from   = static_cast<int>(m_fromFld->getValue());
  int to     = static_cast<int>(m_toFld->getValue());
  int inc    = static_cast<int>(m_incFld->getValue());
  int step   = static_cast<int>(m_stepFld->getValue());
  double w   = m_widthFld->getValue();
  double h   = m_heightFld->getValue();
  double dpi = m_dpiFld->getValue();
  int xres   = std::max(tround(w * dpi), 1);
  int yres   = std::max(tround(h * dpi), 1);
  int lType  = getLevelType();

  std::wstring levelName = m_nameFld->text().toStdWString();
  // Remove leading and trailing whitespace
  auto start = levelName.find_first_not_of(L' ');
  if (start == std::wstring::npos) {
    levelName.clear();
  } else {
    auto end  = levelName.find_last_not_of(L' ');
    levelName = levelName.substr(start, end - start + 1);
  }

  if (levelName.empty()) {
    error(tr("No level name specified: please choose a valid level name"));
    return false;
  }

  if (isReservedFileName_message(QString::fromStdWString(levelName))) {
    return false;
  }

  if (from > to) {
    error(tr("Invalid frame range"));
    return false;
  }
  if (inc <= 0) {
    error(tr("Invalid increment value"));
    return false;
  }
  if (step <= 0) {
    error(tr("Invalid step value"));
    return false;
  }

  int numFrames = step * (((to - from) / inc) + 1);

  TXshLevel *existingLevel = scene->getLevelSet()->getLevel(levelName);
  if (existingLevel) {
    // Check if the existing level can be removed
    if (!Preferences::instance()->isAutoRemoveUnusedLevelsEnabled() ||
        scene->getChildStack()->getTopXsheet()->isLevelUsed(existingLevel)) {
      error(
          tr("The level name specified is already used: please choose a "
             "different level name"));
      m_nameFld->selectAll();
      return false;
    }
    // If existingLevel is not null, it will be removed afterwards
  }

  TFilePath parentDir(m_pathFld->getPath().toStdWString());
  TFilePath fp =
      scene->getDefaultLevelPath(lType, levelName).withParentDir(parentDir);

  if (lType == OVL_XSHLEVEL || lType == TZI_XSHLEVEL) {
    fp = fp.withType(m_rasterFormatOm->currentData().toString().toStdString());
  }

  TFilePath actualFp = scene->decodeFilePath(fp);
  if (TSystem::doesExistFileOrLevel(actualFp)) {
    error(
        tr("The level name specified is already used: please choose a "
           "different level name"));
    m_nameFld->selectAll();
    return false;
  }

  parentDir = scene->decodeFilePath(parentDir);
  if (!TFileStatus(parentDir).doesExist()) {
    QString question = tr("Folder %1 doesn't exist.\nDo you want to create it?")
                           .arg(toQString(parentDir));
    int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
    if (ret == 0 || ret == 2) return false;

    try {
      TSystem::mkDir(parentDir);
      DvDirModel::instance()->refreshFolder(parentDir.getParentDir());
    } catch (...) {
      error(tr("Unable to create %1").arg(toQString(parentDir)));
      return false;
    }
  }

  TUndoManager::manager()->beginBlock();

  // existingLevel is not nullptr only if the level is unused AND
  // the preference option AutoRemoveUnusedLevels is ON
  if (existingLevel) {
    bool ok = LevelCmd::removeLevelFromCast(existingLevel, scene, false);
    Q_ASSERT(ok);
    DVGui::info(QObject::tr("Removed unused level %1 from the scene cast. "
                            "(This behavior can be disabled in Preferences.)")
                    .arg(QString::fromStdWString(levelName)));
  }

  // Check if the cells where we want to place the level are empty
  bool areColumnsShifted = false;
  bool isInRange         = true;

  if (col < 0) {
    isInRange = false;
  } else {
    for (int i = row; i < row + numFrames; ++i) {
      if (!xsh->getCell(i, col).isEmpty()) {
        isInRange = false;
        break;
      }
    }
  }

  if (!validColumn) {
    isInRange = false;
  }

  // If the range is occupied by another level, shift the column one to the
  // right
  if (!isInRange) {
    col += 1;
    TApp::instance()->getCurrentColumn()->setColumnIndex(col);
    areColumnsShifted = true;
    xsh->insertColumn(col);
  }

  auto *undo =
      new CreateLevelUndo(row, col, numFrames, step, areColumnsShifted);
  TUndoManager::manager()->add(undo);

  TXshLevel *level =
      scene->createNewLevel(lType, levelName, TDimension(), 0, fp);
  TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);

  Q_ASSERT(sl);
  sl->setPath(fp, true);

  if (lType == TZP_XSHLEVEL || lType == OVL_XSHLEVEL) {
    sl->getProperties()->setDpiPolicy(LevelProperties::DP_ImageDpi);
    sl->getProperties()->setDpi(dpi);
    sl->getProperties()->setImageDpi(TPointD(dpi, dpi));
    sl->getProperties()->setImageRes(TDimension(xres, yres));
  }

  for (int i = from; i <= to; i += inc) {
    TFrameId fid(i);

    if (lType == PLI_XSHLEVEL) {
      sl->setFrame(fid, new TVectorImage());
    } else if (lType == META_XSHLEVEL) {
      sl->setFrame(fid, new TMetaImage());
    } else if (lType == TZP_XSHLEVEL) {
      TRasterCM32P raster(xres, yres);
      raster->fill(TPixelCM32());
      TToonzImageP ti(raster, TRect());
      ti->setDpi(dpi, dpi);
      sl->setFrame(fid, ti);
      ti->setSavebox(TRect(0, 0, xres - 1, yres - 1));

      // This update should be called at least once, or it won't be rendered
      // Almost every level tool will call ToolUtils::updateSavebox() to update
      // But since fill tool tends to not update the savebox, we call it here
      TImageInfo *info = sl->getFrameInfo(fid, true);
      ImageBuilder::setImageInfo(*info, ti.getPointer());
    } else if (lType == OVL_XSHLEVEL) {
      TRaster32P raster(xres, yres);
      raster->clear();
      TRasterImageP ri(raster);
      ri->setDpi(dpi, dpi);
      // Modify frameId to have the same frame format as existing frames
      TFrameId tmplFId = scene->getProperties()->formatTemplateFIdForInput();
      sl->formatFId(fid, tmplFId);
      sl->setFrame(fid, ri);
    }

    TXshCell cell(sl, fid);
    for (int j = 0; j < step; ++j) {
      xsh->setCell(row++, col, cell);
    }
  }

  if (lType == TZP_XSHLEVEL || lType == OVL_XSHLEVEL) {
    sl->save(fp);
    DvDirModel::instance()->refreshFolder(fp.getParentDir());
  }

  undo->onAdd(sl);
  TUndoManager::manager()->endBlock();

  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentScene()->notifyCastChange();
  app->getCurrentXsheet()->notifyXsheetChanged();

  // Change the current image but don't change the current frame or column
  // (both notify the image change to the tool).
  // Need to verify that the correct tool is set.
  app->getCurrentTool()->onImageChanged(
      static_cast<TImage::Type>(app->getCurrentImageType()));

  return true;
}

//-----------------------------------------------------------------------------

void LevelCreatePopup::update() {
  updatePath();

  Preferences *pref = Preferences::instance();
  if (pref->getUnits() == "pixel") {
    m_widthFld->setMeasure("camera.lx");
    m_heightFld->setMeasure("camera.ly");
  } else {
    m_widthFld->setMeasure("level.lx");
    m_heightFld->setMeasure("level.ly");
  }

  if (pref->isNewLevelSizeToCameraSizeEnabled()) {
    TCamera *currCamera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimensionD camSize = currCamera->getSize();
    m_widthFld->setValue(camSize.lx);
    m_heightFld->setValue(camSize.ly);
    m_dpiFld->setValue(currCamera->getDpi().x);
  } else {
    m_widthFld->setValue(pref->getDefLevelWidth());
    m_heightFld->setValue(pref->getDefLevelHeight());
    m_dpiFld->setValue(pref->getDefLevelDpi());
  }

  int levelType = pref->getDefLevelType();
  int index     = m_levelTypeOm->findData(levelType);
  if (index >= 0) {
    m_levelTypeOm->setCurrentIndex(index);
  }
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<LevelCreatePopup> openLevelCreatePopup(MI_NewLevel);
