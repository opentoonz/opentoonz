

#include "vectorizerpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "fileselection.h"
#include "castselection.h"
#include "cellselection.h"
#include "overwritepopup.h"
#include "vectorizerswatch.h"
#include "filebrowserpopup.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/intfield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/namebuilder.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/toonzscene.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/dpiscale.h"
#include "toonz/txshchildlevel.h"
#include "toonz/levelset.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/imagemanager.h"
#include "toonz/Naa2TlvConverter.h"

// TnzCore includes
#include "tsystem.h"
#include "tconvert.h"
#include "tpalette.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tcolorstyles.h"
#include "tstroke.h"
#include "tpersistset.h"

// Qt includes
#include <QFrame>
#include <QComboBox>
#include <QCoreApplication>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QString>
#include <QLabel>
#include <QSplitter>
#include <QGridLayout>
#include <QScrollArea>
#include <QAction>
#include <QMainWindow>
#include <QToolButton>

using namespace DVGui;

//********************************************************************************
//    Local namespace  classes
//********************************************************************************

namespace {

struct Param {
  QString m_name;
  int m_bit;

  Param(const QString &name, int bit) : m_name(name), m_bit(bit) {}
};

struct ParamGroup {
  int m_startRow, m_separatorRow;
  std::vector<Param> m_params;

  ParamGroup(int startRow, int separatorRow)
      : m_startRow(startRow), m_separatorRow(separatorRow) {}
};

}  // namespace

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {

std::vector<ParamGroup> l_centerlineParamGroups;
std::vector<ParamGroup> l_outlineParamGroups;

bool l_quitLoop = false;

//=============================================================================

VectorizerParameters *getCurrentVectorizerParameters() {
  return TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getProperties()
      ->getVectorizerParameters();
}

//-----------------------------------------------------------------------------

bool getSelectedLevels(std::set<TXshLevel *> &levels, int &r0, int &c0, int &r1,
                       int &c1) {
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();

  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(TSelection::getCurrent());
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());

  if (castSelection) {
    std::vector<TXshLevel *> selectedLevels;
    castSelection->getSelectedLevels(selectedLevels);

    for (int i = 0; i < (int)selectedLevels.size(); ++i)
      levels.insert(selectedLevels[i]);

    return false;
  } else if (cellSelection) {
    cellSelection->getSelectedCells(r0, c0, r1, c1);

    for (int c = c0; c <= c1; ++c) {
      for (int r = r0; r <= r1; ++r) {
        TXshCell cell = xsheet->getCell(r, c);

        if (TXshLevel *level = cell.isEmpty() ? 0 : cell.getSimpleLevel())
          levels.insert(level);
      }
    }

    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

TXshLevel *getSelectedLevel() {
  std::set<TXshLevel *> levels;
  int r0, c0, r1, c1;

  getSelectedLevels(levels, r0, c0, r1, c1);

  return (levels.size() == 1) ? *levels.begin() : (TXshLevel *)0;
}

//-----------------------------------------------------------------------------

TFilePath getSelectedLevelPath() {
  TXshLevel *level = getSelectedLevel();

  return level
             ? TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(
                   level->getPath())
             : TFilePath();
}

//-----------------------------------------------------------------------------

void getSelectedFids(std::vector<TFrameId> &fids, TXshSimpleLevel *level,
                     int r0, int c0, int r1, int c1) {
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();

  std::set<TFrameId> fidsSet;
  for (int c = c0; c <= c1; ++c) {
    for (int r = r0; r <= r1; ++r) {
      TXshCell cell = xsheet->getCell(r, c);

      TXshSimpleLevel *curLevel = cell.isEmpty() ? 0 : cell.getSimpleLevel();
      if (curLevel != level) continue;

      fidsSet.insert(cell.getFrameId());
    }
  }

  std::set<TFrameId>::iterator fst, fsEnd = fidsSet.end();
  for (fst = fidsSet.begin(); fst != fsEnd; ++fst) fids.push_back(*fst);
}

}  // namespace

//*****************************************************************************
//    Vectorizer implementation
//*****************************************************************************

Vectorizer::Vectorizer()
    : m_dialog(new OverwriteDialog)
    , m_isCanceled(false)
    , m_dialogShown(false) {}

//-----------------------------------------------------------------------------

Vectorizer::~Vectorizer() {
  // DO NOT REMOVE - DESTRUCTS INCOMPLETE TYPES IN THE HEADER FILE
}

//-----------------------------------------------------------------------------

TVectorImageP Vectorizer::doVectorize(TImageP img, TPalette *palette,
                                      const VectorizerConfiguration &conf) {
  TToonzImageP ti  = img;
  TRasterImageP ri = img;

  if (!ti && !ri) return TVectorImageP();

  VectorizerCore vCore;
  connect(&vCore, SIGNAL(partialDone(int, int)), this,
          SIGNAL(partialDone(int, int)));
  connect(this, SIGNAL(transmitCancel()), &vCore, SLOT(onCancel()),
          Qt::DirectConnection);  // Direct connection *must* be
                                  // established for child cancels
  return vCore.vectorize(img, conf, palette);
}

//-----------------------------------------------------------------------------

void Vectorizer::setLevel(const TXshSimpleLevelP &level) {
  m_level = level;

  // Creo il livello pli
  TXshSimpleLevel *sl = m_level.getPointer();
  if (!sl) return;

  int rowCount = sl->getFrameCount();
  if (rowCount <= 0 || sl->isEmpty()) return;

  TXshLevel *xl;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  // Build the new level name
  std::wstring levelName = sl->getName() + L"v";
  {
    std::unique_ptr<NameBuilder> nameBuilder(
        NameBuilder::getBuilder(levelName));

    for (;;) {
      levelName = nameBuilder->getNext();
      if (scene->getLevelSet()->getLevel(levelName) == 0) break;
    }
  }

  TFilePath dstPath = sl->getPath().withName(levelName).withType("pli");
  dstPath           = scene->decodeFilePath(dstPath);

  bool overWrite = false;
  if (TSystem::doesExistFileOrLevel(dstPath)) {
    m_dialogShown = true;

    std::wstring name = m_dialog->execute(scene, dstPath, true);
    if (m_dialog->cancelPressed()) return;

    switch (m_dialog->getChoice()) {
    case OverwriteDialog::KEEP_OLD:
      xl          = scene->getLevelSet()->getLevel(levelName);
      if (!xl) xl = scene->loadLevel(dstPath);

      m_vLevel = xl->getSimpleLevel();
      return;
    case OverwriteDialog::OVERWRITE:
      overWrite = true;
      break;
    default:
      levelName = name;
      break;
    }
  }

  xl = scene->createNewLevel(PLI_XSHLEVEL, levelName);

  TXshSimpleLevel *vl = xl->getSimpleLevel();
  assert(vl);

  if (overWrite) {
    vl->setPath(scene->codeFilePath(dstPath));
    vl->setName(levelName);
  }

  TPalette *palette                          = 0;
  if (sl->getType() == TZP_XSHLEVEL) palette = sl->getPalette();

  palette = palette ? palette->clone() : new TPalette;

  palette->setPaletteName(vl->getName());
  vl->setPalette(palette);

  m_vLevel = vl;
}

//-----------------------------------------------------------------------------

int Vectorizer::doVectorize() {
  struct {
    Vectorizer *m_this;

    CenterlineConfiguration m_cConf;
    NewOutlineConfiguration m_oConf;

    void updateConfig(double weight) {
      if (m_this->m_params.m_isOutline)
        m_oConf = m_this->m_params.getOutlineConfiguration(weight);
      else
        m_cConf = m_this->m_params.getCenterlineConfiguration(weight);
    }

  } locals = {this};

  VectorizerConfiguration &configuration =
      m_params.m_isOutline
          ? static_cast<VectorizerConfiguration &>(locals.m_oConf)
          : static_cast<VectorizerConfiguration &>(locals.m_cConf);

  if (!m_vLevel) return 0;

  if (m_dialog->getChoice() == OverwriteDialog::KEEP_OLD && m_dialogShown)
    return m_fids.size();

  TXshSimpleLevel *sl = m_level.getPointer();
  if (!sl) return 0;

  int rowCount = sl->getFrameCount();
  if (rowCount <= 0 || sl->isEmpty()) return 0;

  double frameRange[2] = {static_cast<double>(m_fids.front().getNumber()) - 1,
                          static_cast<double>(m_fids.back().getNumber()) - 1};

  int count = 0;

  std::vector<TFrameId>::const_iterator ft, fEnd = m_fids.end();
  for (ft = m_fids.begin(); ft != fEnd; ++ft) {
    // Retrieve the image to be vectorized
    TImageP img;
    if (sl->getType() == OVL_XSHLEVEL || sl->getType() == TZP_XSHLEVEL ||
        sl->getType() == TZI_XSHLEVEL)
      img = sl->getFullsampledFrame(*ft, ImageManager::dontPutInCache);

    if (!img) continue;

    // Build image-toonz coordinate transformation
    TAffine dpiAff = getDpiAffine(sl, *ft, true);
    double factor  = norm(dpiAff * TPointD(1, 0));

    TPointD center;
    if (TToonzImageP ti = img)
      center = ti->getRaster()->getCenterD();
    else if (TRasterImageP ri = img)
      center = ri->getRaster()->getCenterD();

    // Build vectorizer configuration
    double weight = (ft->getNumber() - 1 - frameRange[0]) /
                    std::max(frameRange[1] - frameRange[0], 1.0);
    weight = tcrop(weight, 0.0, 1.0);

    locals.updateConfig(weight);  // TEMPORARY

    configuration.m_affine     = dpiAff * TTranslation(-center);
    configuration.m_thickScale = factor;

    // Build vectorization label to be displayed
    QString labelName = QString::fromStdWString(sl->getShortName());
    labelName.push_back(' ');
    labelName.append(QString::fromStdString(ft->expand(TFrameId::NO_PAD)));

    emit frameName(labelName);

    // Perform vectorization
    if (TVectorImageP vi =
            doVectorize(img, m_vLevel->getPalette(), configuration)) {
      TFrameId fid = *ft;

      if (fid.getNumber() < 0) fid = TFrameId(1, ft->getLetter());

      m_vLevel->setFrame(fid, vi);
      vi->setPalette(m_vLevel->getPalette());

      emit frameDone(++count);
    }

    // Stop if canceled
    if (m_isCanceled) break;
  }

  m_dialogShown = false;

  return count;
}

//-----------------------------------------------------------------------------

void Vectorizer::run() { doVectorize(); }

//*****************************************************************************
//    VectorizerPopup implentation
//*****************************************************************************

#if QT_VERSION >= 0x050500
VectorizerPopup::VectorizerPopup(QWidget *parent, Qt::WindowFlags flags)
#else
VectorizerPopup::VectorizerPopup(QWidget *parent, Qt::WFlags flags)
#endif
    : Dialog(TApp::instance()->getMainWindow(), true, false, "Vectorizer")
    , m_sceneHandle(TApp::instance()->getCurrentScene()) {
  struct Locals {
    int m_bit;

    Locals() : m_bit() {}

    static void addParameterGroup(std::vector<ParamGroup> &paramGroups,
                                  int group, int startRow,
                                  int separatorRow = -1) {
      assert(group <= paramGroups.size());

      if (group == paramGroups.size())
        paramGroups.push_back(ParamGroup(startRow, separatorRow));
    }

    void addParameter(std::vector<ParamGroup> &paramGroups,
                      const QString &paramName) {
      paramGroups.back().m_params.push_back(Param(paramName, m_bit++));
    }

  } locals;

  // Su MAC i dialog modali non hanno bottoni di chiusura nella titleBar
  setModal(false);
  setWindowTitle(tr("Convert-to-Vector Settings"));

  setLabelWidth(125);

  setTopMargin(0);
  setTopSpacing(0);

  // Build vertical layout
  beginVLayout();

  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                      QSizePolicy::MinimumExpanding));
  addWidget(splitter);

  QToolBar *leftToolBar = new QToolBar, *rightToolBar = new QToolBar;
  leftToolBar->setObjectName("MediumPaddingToolBar");
  rightToolBar->setObjectName("MediumPaddingToolBar");
  leftToolBar->setIconSize(QSize(17, 17));
  rightToolBar->setIconSize(QSize(17, 17));
  {
    QWidget *toolbarsContainer = new QWidget(this);
    toolbarsContainer->setFixedHeight(22);
    addWidget(toolbarsContainer);

    QHBoxLayout *toolbarsLayout = new QHBoxLayout(toolbarsContainer);
    toolbarsContainer->setLayout(toolbarsLayout);

    toolbarsLayout->setMargin(0);
    toolbarsLayout->setSpacing(0);

    QToolBar *spacingToolBar = new QToolBar(
        toolbarsContainer);  // The spacer object must be a toolbar.
    spacingToolBar->setFixedHeight(
        22);  // It's related to qss choices... I know it's stinky

    toolbarsLayout->addWidget(leftToolBar, 0, Qt::AlignLeft);
    toolbarsLayout->addWidget(spacingToolBar, 1);
    toolbarsLayout->addWidget(rightToolBar, 0, Qt::AlignRight);
  }

  endVLayout();

  // Build parameters area
  QScrollArea *paramsArea = new QScrollArea(splitter);
  paramsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  paramsArea->setWidgetResizable(true);
  splitter->addWidget(paramsArea);
  splitter->setStretchFactor(0, 1);

  m_paramsWidget = new QFrame(paramsArea);
  paramsArea->setWidget(m_paramsWidget);

  m_paramsLayout = new QGridLayout;
  m_paramsWidget->setLayout(m_paramsLayout);

  int group = 0, row = 0;

  locals.addParameterGroup(::l_centerlineParamGroups, group, row);
  locals.addParameterGroup(::l_outlineParamGroups, group++, row);

  // Vectorization mode
  m_typeMenu = new QComboBox(this);
  m_typeMenu->setFixedSize(245, WidgetHeight);
  QStringList formats;
  formats << tr("Centerline") << tr("Outline");
  m_typeMenu->addItems(formats);
  m_typeMenu->setMinimumHeight(WidgetHeight);
  bool isOutline = m_sceneHandle->getScene()
                       ->getProperties()
                       ->getVectorizerParameters()
                       ->m_isOutline;
  m_typeMenu->setCurrentIndex(isOutline ? 1 : 0);
  connect(m_typeMenu, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onTypeChange(int)));

  m_paramsLayout->addWidget(new QLabel(tr("Mode")), row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_typeMenu, row++, 1);

  locals.addParameter(l_centerlineParamGroups, tr("Mode"));
  locals.addParameter(l_outlineParamGroups, tr("Mode"));

  //-------------------- Parameters area - Centerline ------------------------

  locals.addParameterGroup(l_centerlineParamGroups, group++, row);

  // Threshold
  m_cThresholdLabel = new QLabel(tr("Threshold"));
  m_cThreshold      = new IntField(this);

  m_paramsLayout->addWidget(m_cThresholdLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_cThreshold, row++, 1);

  locals.addParameter(l_centerlineParamGroups, tr("Threshold"));

  // Accuracy
  m_cAccuracyLabel = new QLabel(tr("Accuracy"));
  m_cAccuracy      = new IntField(this);

  m_paramsLayout->addWidget(m_cAccuracyLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_cAccuracy, row++, 1);

  locals.addParameter(l_centerlineParamGroups, tr("Accuracy"));

  // Despeckling
  m_cDespecklingLabel = new QLabel(tr("Despeckling"));
  m_cDespeckling      = new IntField(this);

  m_paramsLayout->addWidget(m_cDespecklingLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_cDespeckling, row++, 1);

  locals.addParameter(l_centerlineParamGroups, tr("Despeckling"));

  // Max Thickness
  m_cMaxThicknessLabel = new QLabel(tr("Max Thickness"));
  m_paramsLayout->addWidget(m_cMaxThicknessLabel, row, 0, Qt::AlignRight);

  m_cMaxThickness = new IntField(this);
  m_cMaxThickness->enableSlider(false);
  m_paramsLayout->addWidget(m_cMaxThickness, row++, 1, Qt::AlignLeft);

  locals.addParameter(l_centerlineParamGroups, tr("Max Thickness"));

  // Thickness Calibration
  m_cThicknessRatioLabel = new QLabel(tr("Thickness Calibration"));
  m_paramsLayout->addWidget(m_cThicknessRatioLabel, row, 0, Qt::AlignRight);

  /*m_cThicknessRatio = new IntField(this);
paramsLayout->addWidget(m_cThicknessRatio, row++, 1);*/

  QHBoxLayout *cThicknessRatioLayout = new QHBoxLayout;

  cThicknessRatioLayout->addSpacing(20);

  m_cThicknessRatioFirstLabel = new QLabel(tr("Start:"));
  cThicknessRatioLayout->addWidget(m_cThicknessRatioFirstLabel);

  m_cThicknessRatioFirst = new MeasuredDoubleLineEdit(this);
  m_cThicknessRatioFirst->setMeasure("percentage");
  cThicknessRatioLayout->addWidget(m_cThicknessRatioFirst);

  m_cThicknessRatioLastLabel = new QLabel(tr("End:"));
  cThicknessRatioLayout->addWidget(m_cThicknessRatioLastLabel);

  m_cThicknessRatioLast = new MeasuredDoubleLineEdit(this);
  m_cThicknessRatioLast->setMeasure("percentage");
  cThicknessRatioLayout->addWidget(m_cThicknessRatioLast);

  cThicknessRatioLayout->addStretch(1);

  m_paramsLayout->addLayout(cThicknessRatioLayout, row++, 1);

  locals.addParameter(l_centerlineParamGroups, tr("Thickness Calibration"));

  // Checkboxes
  {
    static const QString name = tr("Preserve Painted Areas");
    locals.addParameter(l_centerlineParamGroups, name);

    m_cPaintFill = new CheckBox(name, this);
    m_cPaintFill->setFixedHeight(WidgetHeight);
    m_paramsLayout->addWidget(m_cPaintFill, row++, 1);
  }

  {
    static const QString name = tr("Add Border");
    locals.addParameter(l_centerlineParamGroups, name);

    m_cMakeFrame = new CheckBox(name, this);
    m_cMakeFrame->setFixedHeight(WidgetHeight);
    m_paramsLayout->addWidget(m_cMakeFrame, row++, 1);
  }

  locals.addParameterGroup(l_centerlineParamGroups, group++, row + 1, row);

  m_cNaaSourceSeparator = new Separator(tr("Full color non-AA images"));
  m_paramsLayout->addWidget(m_cNaaSourceSeparator, row++, 0, 1, 2);

  {
    static const QString name = tr("Enhanced ink recognition");
    locals.addParameter(l_centerlineParamGroups, name);

    m_cNaaSource = new CheckBox(name, this);
    m_cNaaSource->setFixedHeight(WidgetHeight);
    m_paramsLayout->addWidget(m_cNaaSource, row++, 1);
  }

  //-------------------- Parameters area - Outline ------------------------

  group = 1;
  locals.addParameterGroup(l_outlineParamGroups, group++, row);

  // Accuracy
  m_oAccuracyLabel = new QLabel(tr("Accuracy"));
  m_oAccuracy      = new IntField(this);

  m_paramsLayout->addWidget(m_oAccuracyLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oAccuracy, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Accuracy"));

  // Despeckling
  m_oDespecklingLabel = new QLabel(tr("Despeckling"));
  m_oDespeckling      = new IntField(this);

  m_paramsLayout->addWidget(m_oDespecklingLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oDespeckling, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Despeckling"));

  // Paint Fill
  {
    static const QString name = tr("Preserve Painted Areas");
    locals.addParameter(l_outlineParamGroups, name);

    m_oPaintFill = new CheckBox(name, this);
    m_oPaintFill->setFixedHeight(WidgetHeight);
    m_paramsLayout->addWidget(m_oPaintFill, row++, 1);
  }

  locals.addParameterGroup(l_outlineParamGroups, group++, row + 1, row);

  m_oCornersSeparator = new Separator(tr("Corners"));
  m_paramsLayout->addWidget(m_oCornersSeparator, row++, 0, 1, 2);

  // Adherence
  m_oAdherenceLabel = new QLabel(tr("Adherence"));
  m_oAdherence      = new IntField(this);

  m_paramsLayout->addWidget(m_oAdherenceLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oAdherence, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Adherence"));

  // Angle
  m_oAngleLabel = new QLabel(tr("Angle"));
  m_oAngle      = new IntField(this);

  m_paramsLayout->addWidget(m_oAngleLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oAngle, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Angle"));

  // Relative
  m_oRelativeLabel = new QLabel(tr("Curve Radius"));
  m_oRelative      = new IntField(this);

  m_paramsLayout->addWidget(m_oRelativeLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oRelative, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Curve Radius"));

  locals.addParameterGroup(l_outlineParamGroups, group++, row + 1, row);

  m_oFullColorSeparator = new Separator(tr("Raster Levels"));
  m_paramsLayout->addWidget(m_oFullColorSeparator, row++, 0, 1, 2);

  // Max Colors
  m_oMaxColorsLabel = new QLabel(tr("Max Colors"));
  m_oMaxColors      = new IntField(this);

  m_paramsLayout->addWidget(m_oMaxColorsLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oMaxColors, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Max Colors"));

  // Transparent Color
  m_oTransparentColorLabel = new QLabel(tr("Transparent Color"), this);
  m_oTransparentColor = new ColorField(this, true, TPixel32::Transparent, 48);
  m_paramsLayout->addWidget(m_oTransparentColorLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oTransparentColor, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Transparent Color"));

  locals.addParameterGroup(l_outlineParamGroups, group++, row + 1, row);

  m_oTlvSeparator = new Separator(tr("TLV Levels"));
  m_paramsLayout->addWidget(m_oTlvSeparator, row++, 0, 1, 2);

  // Tone Threshold
  m_oToneThresholdLabel = new QLabel(tr("Tone Threshold"));
  m_oToneThreshold      = new IntField(this);

  m_paramsLayout->addWidget(m_oToneThresholdLabel, row, 0, Qt::AlignRight);
  m_paramsLayout->addWidget(m_oToneThreshold, row++, 1);

  locals.addParameter(l_outlineParamGroups, tr("Tone Threshold"));

  m_paramsLayout->setRowStretch(row, 1);

  //-------------------- Swatch area ------------------------

  m_swatchArea = new VectorizerSwatchArea(this);
  splitter->addWidget(m_swatchArea);
  m_swatchArea->setEnabled(false);  // Initally not enabled

  connect(this, SIGNAL(valuesChanged()), m_swatchArea,
          SLOT(invalidateContents()));

  //---------------------- Toolbar --------------------------

  QAction *swatchAct = new QAction(createQIconOnOff("preview", true),
                                   tr("Toggle Swatch Preview"), this);
  swatchAct->setCheckable(true);
  leftToolBar->addAction(swatchAct);

  QAction *centerlineAct = new QAction(createQIconOnOff("opacitycheck", true),
                                       tr("Toggle Centerlines Check"), this);
  centerlineAct->setCheckable(true);
  leftToolBar->addAction(centerlineAct);

  QToolButton *visibilityButton = new QToolButton(this);
  visibilityButton->setIcon(createQIcon("options"));
  visibilityButton->setPopupMode(QToolButton::InstantPopup);

  QMenu *visibilityMenu = new QMenu(visibilityButton);
  visibilityButton->setMenu(visibilityMenu);

  rightToolBar->addWidget(visibilityButton);
  rightToolBar->addSeparator();

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

  connect(swatchAct, SIGNAL(triggered(bool)), m_swatchArea,
          SLOT(enablePreview(bool)));
  connect(centerlineAct, SIGNAL(triggered(bool)), m_swatchArea,
          SLOT(enableDrawCenterlines(bool)));
  connect(visibilityMenu, SIGNAL(aboutToShow()), this,
          SLOT(populateVisibilityMenu()));
  connect(saveAct, SIGNAL(triggered()), this, SLOT(saveParameters()));
  connect(loadAct, SIGNAL(triggered()), this, SLOT(loadParameters()));
  connect(resetAct, SIGNAL(triggered()), this, SLOT(resetParameters()));

  //------------------- Convert Button ----------------------

  // Convert Button
  m_okBtn = new QPushButton(QString(tr("Convert")), this);
  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(onOk()));

  addButtonBarWidget(m_okBtn);

  // All detailed signals convey to the unique valuesChanged() signal. That
  // makes it easier
  // to disconnect update notifications whenever we loadConfiguration(..).
  connect(this, SIGNAL(valuesChanged()), this, SLOT(updateSceneSettings()));

  // Connect value changes to update the global
  // VectorizerPopUpSettingsContainer.
  // connect(m_typeMenu,SIGNAL(currentIndexChanged(const QString
  // &)),this,SLOT(updateSceneSettings()));
  connect(m_cThreshold, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_cAccuracy, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_cDespeckling, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_cMaxThickness, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  // connect(m_cThicknessRatio,SIGNAL(valueChanged(bool)),this,SLOT(onValueEdited(bool)));
  connect(m_cThicknessRatioFirst, SIGNAL(valueChanged()), this,
          SLOT(onValueEdited()));
  connect(m_cThicknessRatioLast, SIGNAL(valueChanged()), this,
          SLOT(onValueEdited()));
  connect(m_cMakeFrame, SIGNAL(stateChanged(int)), this, SLOT(onValueEdited()));
  connect(m_cPaintFill, SIGNAL(stateChanged(int)), this, SLOT(onValueEdited()));
  connect(m_cNaaSource, SIGNAL(stateChanged(int)), this, SLOT(onValueEdited()));

  connect(m_oAccuracy, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oDespeckling, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oPaintFill, SIGNAL(stateChanged(int)), this, SLOT(onValueEdited()));
  connect(m_oAdherence, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oAngle, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oRelative, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oDespeckling, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oMaxColors, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));
  connect(m_oTransparentColor, SIGNAL(colorChanged(const TPixel32 &, bool)),
          this, SLOT(onValueEdited(const TPixel32 &, bool)));
  connect(m_oToneThreshold, SIGNAL(valueChanged(bool)), this,
          SLOT(onValueEdited(bool)));

  refreshPopup();

  // Non e' corretto: manca la possibilita' di aggiornare la selezione del
  // livello corrente
  //  connect(TApp::instance()->getCurrentLevel(), SIGNAL(xshLevelChanged()),
  //                                         this, SLOT(updateValues()));
}

//-----------------------------------------------------------------------------

VectorizerParameters *VectorizerPopup::getParameters() const {
  assert(m_sceneHandle);

  ToonzScene *scene = m_sceneHandle->getScene();
  assert(scene);

  TSceneProperties *sceneProp = scene->getProperties();
  assert(sceneProp);

  assert(sceneProp->getVectorizerParameters());
  return sceneProp->getVectorizerParameters();
}

//-----------------------------------------------------------------------------

void VectorizerPopup::onValueEdited(bool isDrag) {
  if (!isDrag) emit valuesChanged();
}

//-----------------------------------------------------------------------------

bool VectorizerPopup::isLevelToConvert(TXshSimpleLevel *sl) {
  return (sl->getPath().getType() != "pli");
}

//-----------------------------------------------------------------------------

bool VectorizerPopup::apply() {
  std::set<TXshLevel *> levels;

  ToonzScene *scene = m_sceneHandle->getScene();
  if (!scene) {
    assert(scene);
    return false;
  }

  TSceneProperties *sceneProp = scene->getProperties();
  if (!sceneProp) return false;

  VectorizerParameters *vectorizerParameters =
      sceneProp->getVectorizerParameters();
  if (!vectorizerParameters) return false;

  int r0               = 0;
  int c0               = 0;
  int r1               = 0;
  int c1               = 0;
  bool isCellSelection = getSelectedLevels(levels, r0, c0, r1, c1);
  if (levels.empty()) {
    error(tr("The current selection is invalid."));
    return false;
  }

  // Initialize Progress bar
  m_progressDialog = new DVGui::ProgressDialog("", "Cancel", 0, 1);
  m_progressDialog->setWindowFlags(
      Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
  m_progressDialog->setWindowTitle(QString("Convert To Vector..."));
  m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);
  m_progressDialog->setWindowModality(
      Qt::WindowModal);  // No user interaction is allowed during vectorization
  m_progressDialog->setFixedSize(200, 100);

  // Initialize vectorizer
  m_vectorizer = new Vectorizer;

  m_vectorizer->setParameters(*vectorizerParameters);

  connect(m_vectorizer, SIGNAL(frameName(QString)), this,
          SLOT(onFrameName(QString)), Qt::QueuedConnection);
  connect(m_vectorizer, SIGNAL(frameDone(int)), this, SLOT(onFrameDone(int)),
          Qt::QueuedConnection);
  connect(m_vectorizer, SIGNAL(partialDone(int, int)), this,
          SLOT(onPartialDone(int, int)), Qt::QueuedConnection);
  // We DON'T want the progress bar to be hidden at cancel press - since its
  // modal
  // behavior prevents the user to interfere with a possibly still active
  // vectorization.
  disconnect(m_progressDialog, SIGNAL(canceled()), m_progressDialog,
             SLOT(onCancel()));
  // We first inform the vectorizer of a cancel press;
  bool ret = connect(m_progressDialog, SIGNAL(canceled()), m_vectorizer,
                     SLOT(cancel()));
  // which eventually transmits the command to vectorization core, allowing
  // full-time cancels
  ret = ret && connect(m_progressDialog, SIGNAL(canceled()), m_vectorizer,
                       SIGNAL(transmitCancel()));
  // Only after the vectorizer has terminated its process - or got cancelled, we
  // are allowed
  // to proceed here.
  ret = ret && connect(m_vectorizer, SIGNAL(finished()), this,
                       SLOT(onFinished()), Qt::QueuedConnection);
  assert(ret);

  int newIndexColumn = c1 + 1;
  for (auto const level : levels) {
    TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);
    if (!sl || !sl->getSimpleLevel() || !isLevelToConvert(sl)) {
      QString levelName = tr(::to_string(sl->getName()).c_str());
      QString errorMsg =
          tr("Cannot convert to vector the current selection.") + levelName;
      error(errorMsg);
      continue;
    }

    std::vector<TFrameId> fids;

    if (isCellSelection)
      getSelectedFids(fids, sl, r0, c0, r1, c1);
    else
      sl->getFids(fids);
    assert(fids.size() > 0);

    close();

    // Re-initialize progress Bar
    m_progressDialog->setMaximum(fids.size() * 100);
    m_progressDialog->setValue(0);
    m_currFrame = 0;

    // Re-initialize vectorizer
    m_vectorizer->setLevel(sl);
    m_vectorizer->setFids(fids);

    // Start vectorizing
    m_vectorizer->start();
    m_progressDialog->show();

    // Wait the vectorizer...
    while (!l_quitLoop)
      QCoreApplication::processEvents(QEventLoop::AllEvents |
                                      QEventLoop::WaitForMoreEvents);

    l_quitLoop = false;

    // Assign output X-sheet cells
    TXshSimpleLevel *vl = m_vectorizer->getVectorizedLevel();
    if (isCellSelection && vl) {
      TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
      xsheet->insertColumn(newIndexColumn);

      int r, c;
      for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
          TXshCell cell = xsheet->getCell(r, c);
          TXshSimpleLevel *level =
              (!cell.isEmpty()) ? cell.getSimpleLevel() : 0;
          if (level != sl) continue;
          TFrameId curFid = cell.getFrameId();
          std::vector<TFrameId> newFids;
          vl->getFids(newFids);
          for (auto const &fid : newFids) {
            if (fid.getNumber() ==
                    curFid.getNumber() ||  // Hanno stesso numero di frame
                (fid.getNumber() == 1 &&
                 curFid.getNumber() ==
                     -2))  // La vecchia cella non ha numero di frame
              xsheet->setCell(r, newIndexColumn, TXshCell(vl, fid));
          }
        }
      }
      newIndexColumn += 1;
    } else if (vl) {
      std::vector<TFrameId> gomi;
      scene->getXsheet()->exposeLevel(
          0, scene->getXsheet()->getFirstFreeColumnIndex(), vl, gomi);
    }

    if (m_vectorizer->isCanceled()) break;
  }

  m_progressDialog->close();
  delete m_vectorizer;

  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

  return true;
}

//-----------------------------------------------------------------------------

void VectorizerPopup::onFinished() { l_quitLoop = true; }

//-----------------------------------------------------------------------------

void VectorizerPopup::onFrameName(QString frameName) {
  QString label = tr("Conversion in progress: ") + frameName;
  m_progressDialog->setLabelText(label);
}

//-----------------------------------------------------------------------------

void VectorizerPopup::onFrameDone(int frameCount) {
  m_progressDialog->setValue(
      frameCount * 100);  // 100 multiplier stands for partial progresses
  m_currFrame = frameCount;
}

//-----------------------------------------------------------------------------

void VectorizerPopup::onPartialDone(int partial, int total) {
  int value = (m_currFrame + partial / (double)total) * 100.0;

  // NOTA: Puo' essere che la seguente non sia vera - dipende dall'ordine di
  // esecuzione dei segnali
  // onFrameDone e onPartialDone - se i primi si fanno in massa prima... Puo'
  // generare uno stack overflow...
  // NOTA: Non va ancora bene. Cosi' si attenua largamente il problema, ma a
  // volte puo' ancora succedere.
  if (value > m_progressDialog->value() + 5 &&
      value < m_progressDialog->maximum()) {
    // qDebug("Partial %d of %d;  Value %d of %d", partial, total, value,
    // m_progressDialog->maximum());
    m_progressDialog->setValue(value);
  }
  /*else
{
if(value != m_progressDialog->value())
qDebug("ERRORE: VALORE PB= %d; Valore: %d",m_progressDialog->value(),value);
}*/
}

//-----------------------------------------------------------------------------

void VectorizerPopup::onOk() { apply(); }

//-----------------------------------------------------------------------------

//! Copies the pop-up settings into scene settings.
void VectorizerPopup::updateSceneSettings() {
  VectorizerParameters *vParams = getParameters();
  assert(vParams);

  bool outline         = (m_typeMenu->currentIndex() == 1);
  vParams->m_isOutline = outline;

  if (outline) {
    vParams->m_oDespeckling      = m_oDespeckling->getValue();
    vParams->m_oAccuracy         = m_oAccuracy->getValue();
    vParams->m_oAdherence        = m_oAdherence->getValue();
    vParams->m_oAngle            = m_oAngle->getValue();
    vParams->m_oRelative         = m_oRelative->getValue();
    vParams->m_oMaxColors        = m_oMaxColors->getValue();
    vParams->m_oToneThreshold    = m_oToneThreshold->getValue();
    vParams->m_oTransparentColor = m_oTransparentColor->getColor();
    vParams->m_oPaintFill        = m_oPaintFill->isChecked();
  } else {
    vParams->m_cThreshold    = m_cThreshold->getValue();
    vParams->m_cAccuracy     = m_cAccuracy->getValue();
    vParams->m_cDespeckling  = m_cDespeckling->getValue();
    vParams->m_cMaxThickness = m_cMaxThickness->getValue();
    vParams->m_cThicknessRatioFirst =
        m_cThicknessRatioFirst->getValue() * 100.0;
    vParams->m_cThicknessRatioLast = m_cThicknessRatioLast->getValue() * 100.0;
    vParams->m_cMakeFrame          = m_cMakeFrame->isChecked();
    vParams->m_cPaintFill          = m_cPaintFill->isChecked();
    vParams->m_cNaaSource          = m_cNaaSource->isChecked();
  }
}

//-----------------------------------------------------------------------------

void VectorizerPopup::refreshPopup() { setType(getParameters()->m_isOutline); }

//-----------------------------------------------------------------------------

void VectorizerPopup::updateVisibility() {
  struct Locals {
    QGridLayout *const m_paramsLayout;

    void setVisible(QLayoutItem *item, bool visible) {
      if (item) {
        if (QWidget *w = item->widget())
          w->setVisible(visible);
        else if (QLayout *l = item->layout()) {
          int i, iCount = l->count();
          for (i = 0; i != iCount; ++i) setVisible(l->itemAt(i), visible);
        }
      }
    }

    void setVisible(int row, bool visible) {
      int c, cCount = m_paramsLayout->columnCount();
      for (c = 0; c != cCount; ++c)
        setVisible(m_paramsLayout->itemAtPosition(row, c), visible);
    }

    void setVisible(const std::vector<ParamGroup> &paramGroups,
                    int visibilityBits) {
      // Iterate parameter groups
      std::vector<ParamGroup>::const_iterator pgt, pgEnd = paramGroups.end();
      for (pgt = paramGroups.begin(); pgt != pgEnd; ++pgt) {
        bool groupVisible = false;

        // Iterate parameters
        int r, rCount = int(pgt->m_params.size());
        for (r = 0; r != rCount; ++r) {
          bool visible = (visibilityBits >> pgt->m_params[r].m_bit) & 1;

          setVisible(pgt->m_startRow + r, visible);
          groupVisible = visible | groupVisible;
        }

        // Finally, set group header's visibility
        if (pgt->m_separatorRow >= 0)
          setVisible(pgt->m_separatorRow, groupVisible);
      }
    }

  } locals = {m_paramsLayout};

  VectorizerParameters *vParams = getParameters();
  assert(vParams);

  locals.setVisible(
      vParams->m_isOutline ? l_outlineParamGroups : l_centerlineParamGroups,
      vParams->m_visibilityBits);
}

//-----------------------------------------------------------------------------

void VectorizerPopup::onTypeChange(int indexType) {
  ToonzScene *scene = m_sceneHandle->getScene();
  if (!scene) return;
  TSceneProperties *sceneProp = scene->getProperties();
  if (!sceneProp) return;
  VectorizerParameters *vectorizerParameters =
      sceneProp->getVectorizerParameters();
  if (!vectorizerParameters) return;
  bool isOutline        = vectorizerParameters->m_isOutline;
  bool isNewTypeOutline = (indexType == 0) ? false : true;
  if (isNewTypeOutline == isOutline) return;

  vectorizerParameters->m_isOutline = isNewTypeOutline;
  setType(isNewTypeOutline);

  m_swatchArea->invalidateContents();
}

//-----------------------------------------------------------------------------

void VectorizerPopup::setType(bool outline) {
  disconnect(m_typeMenu, SIGNAL(currentIndexChanged(int)), this,
             SLOT(onTypeChange(int)));

  // Setting child visibility alot invokes several layout updates - causing
  // extensive flickering
  m_paramsWidget->layout()->setEnabled(false);

  bool centerline = !outline;
  m_typeMenu->setCurrentIndex((int)outline);

  m_cThresholdLabel->setVisible(centerline);
  m_cThreshold->setVisible(centerline);
  m_cAccuracyLabel->setVisible(centerline);
  m_cAccuracy->setVisible(centerline);
  m_cDespecklingLabel->setVisible(centerline);
  m_cDespeckling->setVisible(centerline);
  m_cMaxThicknessLabel->setVisible(centerline);
  m_cMaxThickness->setVisible(centerline);
  // m_cThicknessRatio->setVisible(centerline);
  m_cThicknessRatioLabel->setVisible(centerline);
  m_cThicknessRatioFirstLabel->setVisible(centerline);
  m_cThicknessRatioFirst->setVisible(centerline);
  m_cThicknessRatioLastLabel->setVisible(centerline);
  m_cThicknessRatioLast->setVisible(centerline);

  m_cPaintFill->setVisible(centerline);
  m_cMakeFrame->setVisible(centerline);
  m_cNaaSourceSeparator->setVisible(centerline);
  m_cNaaSource->setVisible(centerline);

  m_oAccuracyLabel->setVisible(outline);
  m_oAccuracy->setVisible(outline);
  m_oDespecklingLabel->setVisible(outline);
  m_oDespeckling->setVisible(outline);
  m_oPaintFill->setVisible(outline);
  m_oCornersSeparator->setVisible(outline);
  m_oAngleLabel->setVisible(outline);
  m_oAngle->setVisible(outline);
  m_oAdherenceLabel->setVisible(outline);
  m_oAdherence->setVisible(outline);
  m_oRelativeLabel->setVisible(outline);
  m_oRelative->setVisible(outline);
  m_oFullColorSeparator->setVisible(outline);
  m_oMaxColorsLabel->setVisible(outline);
  m_oMaxColors->setVisible(outline);
  m_oTransparentColorLabel->setVisible(outline);
  m_oTransparentColor->setVisible(outline);
  m_oTlvSeparator->setVisible(outline);
  m_oToneThresholdLabel->setVisible(outline);
  m_oToneThreshold->setVisible(outline);

  m_paramsWidget->layout()->setEnabled(true);

  loadConfiguration(outline);

  connect(m_typeMenu, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onTypeChange(int)));

  updateVisibility();
}

//-----------------------------------------------------------------------------

// This is essentially the inverse of the previous one.
void VectorizerPopup::loadConfiguration(bool isOutline) {
  disconnect(SIGNAL(valuesChanged()));  // Avoid notifications for value changes

  ToonzScene *scene = m_sceneHandle->getScene();
  assert(scene);

  TSceneProperties *sceneProp = scene->getProperties();
  assert(sceneProp);

  VectorizerParameters *vParams = sceneProp->getVectorizerParameters();
  assert(vParams);

  loadRanges(isOutline);

  if (isOutline) {
    m_oDespeckling->setValue(vParams->m_oDespeckling);
    m_oAdherence->setValue(vParams->m_oAdherence);
    m_oAngle->setValue(vParams->m_oAngle);
    m_oRelative->setValue(vParams->m_oRelative);
    m_oAccuracy->setValue(vParams->m_oAccuracy);
    m_oPaintFill->setChecked(vParams->m_oPaintFill);
    m_oMaxColors->setValue(vParams->m_oMaxColors);
    m_oTransparentColor->setColor(vParams->m_oTransparentColor);
    m_oToneThreshold->setValue(vParams->m_oToneThreshold);
  } else {
    m_cThreshold->setValue(vParams->m_cThreshold);
    m_cDespeckling->setValue(vParams->m_cDespeckling);
    m_cPaintFill->setChecked(vParams->m_cPaintFill);
    m_cMakeFrame->setChecked(vParams->m_cMakeFrame);
    m_cNaaSource->setChecked(vParams->m_cNaaSource);
    m_cMaxThickness->setValue(vParams->m_cMaxThickness);
    m_cAccuracy->setValue(vParams->m_cAccuracy);
    // m_cThicknessRatio->setValue(vParams->m_cThicknessRatio);
    m_cThicknessRatioFirst->setValue(vParams->m_cThicknessRatioFirst / 100.0);
    m_cThicknessRatioLast->setValue(vParams->m_cThicknessRatioLast / 100.0);
  }

  // Reconnect changes update
  connect(this, SIGNAL(valuesChanged()), this, SLOT(updateSceneSettings()));
  connect(this, SIGNAL(valuesChanged()), m_swatchArea,
          SLOT(invalidateContents()));

  m_swatchArea->updateContents();
}

//-----------------------------------------------------------------------------

void VectorizerPopup::loadRanges(int outline) {
  if (outline) {
    m_oAccuracy->setRange(0, 10);
    m_oDespeckling->setRange(0, 10);
    m_oAdherence->setRange(0, 100);
    m_oAngle->setRange(0, 180);
    m_oRelative->setRange(0, 100);
    m_oMaxColors->setRange(1, 256);
    m_oToneThreshold->setRange(0, 255);
  } else {
    m_cThreshold->setRange(1, 10);
    m_cAccuracy->setRange(1, 10);
    m_cDespeckling->setRange(1, 10);
    m_cMaxThickness->setRange(0, (std::numeric_limits<int>::max)());
    // m_cThicknessRatio->setRange(0,100);
    m_cThicknessRatioFirst->setRange(0, 1.0);
    m_cThicknessRatioLast->setRange(0, 1.0);
  }
}

//-----------------------------------------------------------------------------

void VectorizerPopup::showEvent(QShowEvent *se) {
  refreshPopup();
  connect(m_sceneHandle, SIGNAL(sceneSwitched()), SLOT(refreshPopup()));
}

//-----------------------------------------------------------------------------

void VectorizerPopup::hideEvent(QHideEvent *he) {
  refreshPopup();
  disconnect(m_sceneHandle, SIGNAL(sceneSwitched()), this,
             SLOT(refreshPopup()));
  Dialog::hideEvent(he);
}

//-----------------------------------------------------------------------------

void VectorizerPopup::populateVisibilityMenu() {
  struct Locals {
    VectorizerPopup *m_this;

    void addActions(QMenu *menu, const std::vector<ParamGroup> &paramGroups,
                    int visibilityBits) {
      std::vector<ParamGroup>::const_iterator gt, gEnd = paramGroups.end();

      for (gt = paramGroups.begin(); gt != gEnd; ++gt) {
        if (gt->m_separatorRow >= 0) menu->addSeparator();

        std::vector<Param>::const_iterator pt, pEnd = gt->m_params.end();
        for (pt = gt->m_params.begin(); pt != pEnd; ++pt) {
          QAction *visibleParam = menu->addAction(pt->m_name);
          visibleParam->setCheckable(true);
          visibleParam->setChecked(visibilityBits & (1 << pt->m_bit));
          visibleParam->setData(pt->m_bit);

          bool ret = connect(visibleParam, SIGNAL(toggled(bool)), m_this,
                             SLOT(visibilityToggled()));
          assert(ret);
        }
      }
    }

  } locals = {this};

  QMenu *menu = qobject_cast<QMenu *>(sender());
  menu->clear();

  VectorizerParameters *vParams = getParameters();
  locals.addActions(menu, vParams->m_isOutline ? l_outlineParamGroups
                                               : l_centerlineParamGroups,
                    vParams->m_visibilityBits);
}

//-----------------------------------------------------------------------------

void VectorizerPopup::visibilityToggled() {
  QAction *action = qobject_cast<QAction *>(sender());
  assert(action);

  const QVariant &data = action->data();
  assert(data.canConvert<int>());

  int row = action->data().toInt();

  VectorizerParameters *vParams = getParameters();
  vParams->m_visibilityBits ^= (1 << row);

  updateVisibility();
}

//-----------------------------------------------------------------------------

void VectorizerPopup::saveParameters() {
  struct {
    VectorizerPopup *m_this;

    static bool vectorizerType(TPersist *persist) {
      return (dynamic_cast<VectorizerParameters *>(persist) != 0);
    }

    void saveParams(const TFilePath &fp)  // May throw due to I/O failure
    {
      // Read the complete file first
      TPersistSet levelSettings;

      if (TSystem::doesExistFileOrLevel(fp)) {
        TIStream is(fp);

        if (!is)
          throw TException(
              tr("File could not be opened for read").toStdWString());

        is >> levelSettings;
      }

      // Replace data to be saved
      VectorizerParameters *params = getCurrentVectorizerParameters();

      levelSettings.insert(
          std::unique_ptr<TPersist>(new VectorizerParameters(*params)));

      // Save the new settings
      TOStream os(fp);

      if (!os)
        throw TException(
            tr("File could not be opened for write").toStdWString());

      os << levelSettings;
    }

  } locals = {this};

  // Retrieve current level path
  TFilePath folder, fileName;

  const TFilePath &levelPath = getSelectedLevelPath();
  if (!levelPath.isEmpty()) {
    folder   = levelPath.getParentDir();
    fileName = TFilePath(levelPath.getWideName()).withType("tnzsettings");
  }

  // Open save popup with defaulted path
  static GenericSaveFilePopup *popup =
      new GenericSaveFilePopup(tr("Save Vectorizer Parameters"));

  popup->setFilterTypes(QStringList("tnzsettings"));
  popup->setFolder(folder);
  popup->setFilename(fileName);

  fileName = popup->getPath();
  if (!fileName.isEmpty()) {
    try {
      locals.saveParams(fileName);
    } catch (const TException &e) {
      DVGui::error(QString::fromStdWString(e.getMessage()));
    }
  }
}

//-----------------------------------------------------------------------------

void VectorizerPopup::loadParameters() {
  struct {
    VectorizerPopup *m_this;

    void loadParams(const TFilePath &fp)  // May throw due to I/O failure
    {
      TIStream is(fp);

      if (!is)
        throw TException(
            tr("File could not be opened for read").toStdWString());

      VectorizerParameters *vParams = getCurrentVectorizerParameters();
      const std::string &vParamsTag = vParams->getStreamTag();

      std::string tagName;
      while (is.matchTag(tagName)) {
        if (tagName == vParamsTag)
          is >> *vParams, is.matchEndTag();
        else
          is.skipCurrentTag();
      }
    }

  } locals = {this};

  // Retrieve current level path
  TFilePath folder, fileName;

  const TFilePath &levelPath = getSelectedLevelPath();
  if (!levelPath.isEmpty()) {
    folder   = levelPath.getParentDir();
    fileName = TFilePath(levelPath.getWideName()).withType("tnzsettings");
  }

  // Open load popup with defaulted path
  static GenericLoadFilePopup *popup =
      new GenericLoadFilePopup(tr("Load Vectorizer Parameters"));

  popup->setFilterTypes(QStringList("tnzsettings"));
  popup->setFolder(folder);
  popup->setFilename(fileName);

  fileName = popup->getPath();
  if (!fileName.isEmpty()) {
    try {
      locals.loadParams(fileName);
      refreshPopup();  // Update GUI to reflect changes
    } catch (const TException &e) {
      DVGui::error(QString::fromStdWString(e.getMessage()));
    }
  }
}

//-----------------------------------------------------------------------------

void VectorizerPopup::resetParameters() {
  *getCurrentVectorizerParameters() = VectorizerParameters();
  refreshPopup();
}

//*****************************************************************************
//    VectorizerPopupCommand instantiation
//*****************************************************************************

OpenPopupCommandHandler<VectorizerPopup> openVectorizerPopup(
    MI_ConvertToVectors);
