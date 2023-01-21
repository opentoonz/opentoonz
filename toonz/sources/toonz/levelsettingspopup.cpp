

#include "levelsettingspopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "flipbook.h"
#include "fileviewerpopup.h"
#include "castselection.h"
#include "fileselection.h"
#include "columnselection.h"
#include "levelcommand.h"

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
#include "tundo.h"

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
  else if (dpi.x < 0.0 || dpi.y < 0.0)
    return LevelSettingsPopup::tr("[Various]");
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

void uniteValue(QString &oldValue, QString &newValue, bool isFirst) {
  if (isFirst)
    oldValue = newValue;
  else if (oldValue != newValue &&
           oldValue != LevelSettingsPopup::tr("[Various]"))
    oldValue = LevelSettingsPopup::tr("[Various]");
}

void uniteValue(int &oldValue, int &newValue, bool isFirst) {
  if (isFirst)
    oldValue = newValue;
  else if (oldValue != -1 && oldValue != newValue)
    oldValue = -1;
}

void uniteValue(TPointD &oldValue, TPointD &newValue, bool isFirst) {
  if (isFirst)
    oldValue = newValue;
  else if (oldValue != TPointD(-1, -1) && oldValue != newValue)
    oldValue = TPointD(-1, -1);
}

void uniteValue(Qt::CheckState &oldValue, Qt::CheckState &newValue,
                bool isFirst) {
  if (isFirst)
    oldValue = newValue;
  else if (oldValue != Qt::PartiallyChecked && oldValue != newValue)
    oldValue = Qt::PartiallyChecked;
}

void uniteValue(double &oldValue, double &newValue, bool isFirst) {
  if (isFirst)
    oldValue = newValue;
  else if (oldValue != -1.0 && oldValue != newValue)
    oldValue = -1.0;
}

class LevelSettingsUndo final : public TUndo {
public:
  enum Type {
    Name,
    Path,
    ScanPath,
    DpiType,
    Dpi,
    DoPremulti,
    WhiteTransp,
    Softness,
    Subsampling,
    LevelType,
    ColorSpaceGamma
  };

private:
  TXshLevelP m_xl;
  const Type m_type;
  const QVariant m_before;
  const QVariant m_after;

  void setName(const QString name) const {
    TLevelSet *levelSet =
        TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
    levelSet->renameLevel(m_xl.getPointer(), name.toStdWString());
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }

  void setPath(const TFilePath path) const {
    TXshSoundLevelP sdl = m_xl->getSoundLevel();
    if (sdl) {
      sdl->setPath(path);
      sdl->loadSoundTrack();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
      return;
    }

    TXshSimpleLevelP sl  = m_xl->getSimpleLevel();
    TXshPaletteLevelP pl = m_xl->getPaletteLevel();
    if (!sl && !pl) return;
    if (sl) {
      sl->setPath(path);
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->setPalette(sl->getPalette());
      sl->invalidateFrames();
      std::vector<TFrameId> frames;
      sl->getFids(frames);
      for (auto const &fid : frames) {
        IconGenerator::instance()->invalidate(sl.getPointer(), fid);
      }
    } else if (pl) {
      pl->setPath(path);
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->setPalette(pl->getPalette());
      pl->load();
    }
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void setScanPath(const TFilePath path) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() != TZP_XSHLEVEL) return;
    sl->setScannedPath(path);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void setDpiType(LevelProperties::DpiPolicy policy) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) return;
    sl->getProperties()->setDpiPolicy(policy);
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setDpi(TPointD dpi) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) return;
    sl->getProperties()->setDpi(dpi);
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setSubsampling(int subsampling) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) return;
    sl->getProperties()->setSubsampling(subsampling);
    sl->invalidateFrames();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()
        ->getCurrentXsheet()
        ->getXsheet()
        ->getStageObjectTree()
        ->invalidateAll();
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setDoPremulti(bool on) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) return;
    sl->getProperties()->setDoPremultiply(on);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setSoftness(int softness) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) return;
    sl->getProperties()->setDoAntialias(softness);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setWhiteTransp(bool on) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) return;
    sl->getProperties()->setWhiteTransp(on);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setColorSpaceGamma(double gamma) const {
    TXshSimpleLevelP sl = m_xl->getSimpleLevel();
    if (!sl || sl->getType() != OVL_XSHLEVEL) return;
    sl->getProperties()->setColorSpaceGamma(gamma);
    sl->invalidateFrames();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()
        ->getCurrentXsheet()
        ->getXsheet()
        ->getStageObjectTree()
        ->invalidateAll();
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void setValue(const QVariant value) const {
    switch (m_type) {
    case Name:
      setName(value.toString());
      break;
    case Path:
      setPath(TFilePath(value.toString()));
      break;
    case ScanPath:
      setScanPath(TFilePath(value.toString()));
      break;
    case DpiType:
      setDpiType(LevelProperties::DpiPolicy(value.toInt()));
      break;
    case Dpi: {
      QPointF dpi = value.toPointF();
      setDpi(TPointD(dpi.x(), dpi.y()));
      break;
    }
    case Subsampling:
      setSubsampling(value.toInt());
      break;
    case DoPremulti:
      setDoPremulti(value.toBool());
      break;
    case Softness:
      setSoftness(value.toInt());
      break;
    case WhiteTransp:
      setWhiteTransp(value.toBool());
      break;
    case ColorSpaceGamma:
      setColorSpaceGamma(value.toDouble());
      break;
    default:
      break;
    }
    // This signal is for updating the level settings
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  }

public:
  LevelSettingsUndo(TXshLevel *xl, Type type, const QVariant before,
                    const QVariant after)
      : m_xl(xl), m_type(type), m_before(before), m_after(after) {}

  void undo() const override { setValue(m_before); }

  void redo() const override { setValue(m_after); }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Edit Level Settings : %1")
        .arg(QString::fromStdWString(m_xl->getName()));
  }
};

//-----------------------------------------------------------------------------
}  // anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================
/*! \class LevelSettingsPopup
   \brief The LevelSettingsPopup class provides a dialog to show
   and change current level settings.
   Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

LevelSettingsPopup::LevelSettingsPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, false, "LevelSettings")
    , m_whiteTransp(0)
    , m_scanPathFld(0) {
  setWindowTitle(tr("Level Settings"));

  m_nameFld             = new LineEdit();
  m_pathFld             = new FileField();  // Path
  m_scanPathFld         = new FileField();  // ScanPath
  QLabel *scanPathLabel = new QLabel(tr("Scan Path:"), this);
  m_typeLabel           = new QLabel();  // Level Type
  // Type
  m_dpiTypeOm = new QComboBox();
  // DPI
  QLabel *dpiLabel = new QLabel(tr("DPI:"));
  m_dpiFld         = new DoubleLineEdit();
  m_squarePixCB    = new CheckBox(tr("Forced Squared Pixel"));

  QLabel *widthLabel  = new QLabel(tr("Width:"));
  m_widthFld          = new MeasuredDoubleLineEdit();
  QLabel *heightLabel = new QLabel(tr("Height:"));
  m_heightFld         = new MeasuredDoubleLineEdit();
  // Use Camera Dpi
  m_useCameraDpiBtn = new QPushButton(tr("Use Camera DPI"));

  m_cameraDpiLabel       = new QLabel(tr(""));
  m_imageDpiLabel        = new QLabel(tr(""));
  m_imageResLabel        = new QLabel(tr(""));
  QLabel *cameraDpiTitle = new QLabel(tr("Camera DPI:"));
  QLabel *imageDpiTitle  = new QLabel(tr("Image DPI:"));
  QLabel *imageResTitle  = new QLabel(tr("Resolution:"));

  // subsampling
  m_subsamplingLabel = new QLabel(tr("Subsampling:"));
  m_subsamplingFld   = new DVGui::IntLineEdit(this, 1, 1);

  m_doPremultiply = new CheckBox(tr("Premultiply"), this);

  m_whiteTransp = new CheckBox(tr("White As Transparent"), this);

  m_doAntialias       = new CheckBox(tr("Add Antialiasing"), this);
  m_softnessLabel     = new QLabel(tr("Antialias Softness:"), this);
  m_antialiasSoftness = new DVGui::IntLineEdit(0, 10, 0, 100);

  m_colorSpaceGammaLabel = new QLabel(tr("Color Space Gamma:"));
  m_colorSpaceGammaFld   = new DoubleLineEdit();
  m_colorSpaceGammaFld->setRange(0.1, 10.);

  //----

  m_pathFld->setFileMode(QFileDialog::AnyFile);
  m_scanPathFld->setFileMode(QFileDialog::AnyFile);

  m_dpiTypeOm->addItem(tr("Image DPI"), "Image DPI");
  m_dpiTypeOm->addItem(tr("Custom DPI"), "Custom DPI");

  m_squarePixCB->setCheckState(Qt::Checked);

  m_widthFld->setMeasure("camera.lx");
  m_heightFld->setMeasure("camera.ly");

  if (Preferences::instance()->getCameraUnits() == "pixel") {
    m_widthFld->setDecimals(0);
    m_heightFld->setDecimals(0);
  }

  m_doPremultiply->setTristate();
  m_doPremultiply->setCheckState(Qt::Unchecked);

  m_doAntialias->setTristate();
  m_doAntialias->setCheckState(Qt::Unchecked);
  m_antialiasSoftness->setEnabled(false);

  m_whiteTransp->setTristate();
  m_whiteTransp->setCheckState(Qt::Unchecked);

  // register activation flags
  m_activateFlags[m_nameFld]     = AllTypes;
  m_activateFlags[m_pathFld]     = SimpleLevel | Palette | Sound;
  m_activateFlags[m_scanPathFld] = ToonzRaster;
  m_activateFlags[scanPathLabel] = ToonzRaster | MultiSelection;
  // m_activateFlags[m_typeLabel]   = AllTypes | MultiSelection;

  unsigned int dpiWidgetsFlag  = HasDPILevel | HideOnPixelMode | MultiSelection;
  m_activateFlags[m_dpiTypeOm] = dpiWidgetsFlag;
  m_activateFlags[dpiLabel]    = dpiWidgetsFlag;
  m_activateFlags[m_dpiFld]    = dpiWidgetsFlag;
  m_activateFlags[m_squarePixCB] = dpiWidgetsFlag;

  unsigned int rasterWidgetsFlag     = ToonzRaster | Raster | MultiSelection;
  m_activateFlags[widthLabel]        = rasterWidgetsFlag;
  m_activateFlags[m_widthFld]        = rasterWidgetsFlag;
  m_activateFlags[heightLabel]       = rasterWidgetsFlag;
  m_activateFlags[m_heightFld]       = rasterWidgetsFlag;
  m_activateFlags[m_useCameraDpiBtn] = dpiWidgetsFlag;
  m_activateFlags[m_cameraDpiLabel] =
      AllTypes | HideOnPixelMode | MultiSelection;
  m_activateFlags[m_imageDpiLabel] = dpiWidgetsFlag;
  m_activateFlags[m_imageResLabel] = dpiWidgetsFlag;
  m_activateFlags[cameraDpiTitle] = AllTypes | HideOnPixelMode | MultiSelection;
  m_activateFlags[imageDpiTitle]  = dpiWidgetsFlag;
  m_activateFlags[imageResTitle]  = dpiWidgetsFlag;

  m_activateFlags[m_doPremultiply]     = Raster | MultiSelection;
  m_activateFlags[m_whiteTransp]       = Raster | MultiSelection;
  m_activateFlags[m_doAntialias]       = rasterWidgetsFlag;
  m_activateFlags[m_softnessLabel]     = rasterWidgetsFlag;
  m_activateFlags[m_antialiasSoftness] = rasterWidgetsFlag;
  m_activateFlags[m_subsamplingLabel]  = rasterWidgetsFlag;
  m_activateFlags[m_subsamplingFld]    = rasterWidgetsFlag;

  unsigned int linearFlag                 = LinearRaster | MultiSelection;
  m_activateFlags[m_colorSpaceGammaLabel] = linearFlag;
  m_activateFlags[m_colorSpaceGammaFld]   = linearFlag;

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
      nameLayout->addWidget(scanPathLabel, 2, 0,
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
      dpiBox = new QGroupBox(tr("DPI && Resolution"), this);
    QGridLayout *dpiLayout = new QGridLayout();
    dpiLayout->setMargin(5);
    dpiLayout->setSpacing(5);
    {
      dpiLayout->addWidget(m_dpiTypeOm, 0, 1, 1, 3);
      dpiLayout->addWidget(dpiLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_dpiFld, 1, 1);
      dpiLayout->addWidget(m_squarePixCB, 1, 2, 1, 2,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(widthLabel, 2, 0, Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_widthFld, 2, 1);
      dpiLayout->addWidget(heightLabel, 2, 2,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_heightFld, 2, 3);
      dpiLayout->addWidget(m_useCameraDpiBtn, 3, 1, 1, 3);
      dpiLayout->addWidget(cameraDpiTitle, 4, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_cameraDpiLabel, 4, 1, 1, 3);
      dpiLayout->addWidget(imageDpiTitle, 5, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      dpiLayout->addWidget(m_imageDpiLabel, 5, 1, 1, 3);
      dpiLayout->addWidget(imageResTitle, 6, 0,
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
      bottomLay->addWidget(m_softnessLabel, 0, 0);
      bottomLay->addWidget(m_antialiasSoftness, 0, 1);

      bottomLay->addWidget(m_subsamplingLabel, 1, 0);
      bottomLay->addWidget(m_subsamplingFld, 1, 1);

      bottomLay->addWidget(m_colorSpaceGammaLabel, 2, 0);
      bottomLay->addWidget(m_colorSpaceGammaFld, 2, 1);
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
  connect(m_dpiTypeOm, SIGNAL(activated(int)), SLOT(onDpiTypeChanged(int)));
  connect(m_dpiFld, SIGNAL(editingFinished()), SLOT(onDpiFieldChanged()));
  connect(m_squarePixCB, SIGNAL(stateChanged(int)),
          SLOT(onSquarePixelChanged(int)));
  connect(m_widthFld, SIGNAL(valueChanged()), SLOT(onWidthFieldChanged()));
  connect(m_heightFld, SIGNAL(valueChanged()), SLOT(onHeightFieldChanged()));
  connect(m_useCameraDpiBtn, SIGNAL(clicked()), SLOT(useCameraDpi()));
  connect(m_subsamplingFld, SIGNAL(editingFinished()),
          SLOT(onSubsamplingChanged()));

  /*--- ScanPathの入力に対応 ---*/
  connect(m_scanPathFld, SIGNAL(pathChanged()), SLOT(onScanPathChanged()));
  connect(m_doPremultiply, SIGNAL(clicked(bool)),
          SLOT(onDoPremultiplyClicked()));
  connect(m_doAntialias, SIGNAL(clicked(bool)), SLOT(onDoAntialiasClicked()));
  connect(m_antialiasSoftness, SIGNAL(editingFinished()),
          SLOT(onAntialiasSoftnessChanged()));

  connect(m_whiteTransp, SIGNAL(clicked(bool)), SLOT(onWhiteTranspClicked()));

  connect(m_colorSpaceGammaFld, SIGNAL(editingFinished()),
          SLOT(onColorSpaceGammaFieldChanged()));
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

  // update level settings after cleanup by this connection
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(sceneChanged()), SLOT(onSceneChanged()));
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(preferenceChanged(const QString &)),
                       SLOT(onPreferenceChanged(const QString &)));

  assert(ret);
  updateLevelSettings();
  onPreferenceChanged("");
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
  ret = ret && disconnect(TApp::instance()->getCurrentScene(),
                          SIGNAL(preferenceChanged(const QString &)), this,
                          SLOT(onPreferenceChanged(const QString &)));

  assert(ret);
  Dialog::hideEvent(e);
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSceneChanged() { updateLevelSettings(); }

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onPreferenceChanged(const QString &propertyName) {
  if (!propertyName.isEmpty() && propertyName != "pixelsOnly") return;
  bool pixelsMode = Preferences::instance()->getBoolValue(pixelsOnly);
  QMap<QWidget *, unsigned int>::iterator i = m_activateFlags.begin();
  while (i != m_activateFlags.end()) {
    if (i.value() & HideOnPixelMode) i.key()->setHidden(pixelsMode);
    ++i;
  }
  int decimals = (pixelsMode) ? 0 : 4;
  m_widthFld->setDecimals(decimals);
  m_heightFld->setDecimals(decimals);
  adjustSize();
}

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

SelectedLevelType LevelSettingsPopup::getType(TXshLevelP level_p) {
  if (!level_p) return None;
  switch (level_p->getType()) {
  case TZP_XSHLEVEL:
    return ToonzRaster;
  case OVL_XSHLEVEL: {
    if (level_p->getPath().getType() == "exr")
      return LinearRaster;
    else
      return NonLinearRaster;
  }
  case MESH_XSHLEVEL:
    return Mesh;
  case PLI_XSHLEVEL:
    return ToonzVector;
  case PLT_XSHLEVEL:
    return Palette;
  case CHILD_XSHLEVEL:
    return SubXsheet;
  case SND_XSHLEVEL:
    return Sound;
  default:  // Other types are not supported
    return Others;
  }
}

LevelSettingsValues LevelSettingsPopup::getValues(TXshLevelP level) {
  LevelSettingsValues values;

  values.name = QString::fromStdWString(level->getName());

  TXshSimpleLevelP sl         = level->getSimpleLevel();
  TXshPaletteLevelP pl        = level->getPaletteLevel();
  TXshChildLevelP cl          = level->getChildLevel();
  TXshSoundLevelP sdl         = level->getSoundLevel();
  SelectedLevelType levelType = getType(level);
  // path
  if (sl) {
    values.path     = toQString(sl->getPath());
    values.scanPath = toQString(sl->getScannedPath());
  } else if (pl)
    values.path = toQString(pl->getPath());
  else if (sdl)
    values.path = toQString(sdl->getPath());

  // leveltype
  switch (levelType) {
  case ToonzRaster:
    values.typeStr = tr("Toonz Raster level");
    break;
  case NonLinearRaster:
  case LinearRaster:
    values.typeStr = tr("Raster level");
    break;
  case Mesh:
    values.typeStr = tr("Mesh level");
    break;
  case ToonzVector:
    values.typeStr = tr("Toonz Vector level");
    break;
  case Palette:
    values.typeStr = tr("Palette level");
    break;
  case SubXsheet:
    values.typeStr = tr("SubXsheet Level");
    break;
  case Sound:
    values.typeStr = tr("Sound Column");
    break;
  default:
    values.typeStr = tr("Another Level Type");
    break;
  }

  // dpi & res & resampling
  if (levelType & HasDPILevel) {
    LevelProperties::DpiPolicy dpiPolicy = sl->getProperties()->getDpiPolicy();
    assert(dpiPolicy == LevelProperties::DP_ImageDpi ||
           dpiPolicy == LevelProperties::DP_CustomDpi);
    values.dpiType = (dpiPolicy == LevelProperties::DP_ImageDpi) ? 0 : 1;

    // dpi field
    values.dpi = sl->getDpi();
    // image dpi
    values.imageDpi = dpiToString(sl->getImageDpi());

    if (levelType == ToonzRaster || levelType & Raster) {
      // size field
      TDimensionD size(0, 0);
      TDimension res = sl->getResolution();
      if (res.lx > 0 && res.ly > 0 && values.dpi.x > 0 && values.dpi.y > 0) {
        size.lx       = res.lx / values.dpi.x;
        size.ly       = res.ly / values.dpi.y;
        values.width  = tround(size.lx * 100.0) / 100.0;
        values.height = tround(size.ly * 100.0) / 100.0;
      } else {
        values.width  = -1.0;
        values.height = -1.0;
      }

      // image res
      TDimension imgRes = sl->getResolution();
      values.imageRes =
          QString::number(imgRes.lx) + "x" + QString::number(imgRes.ly);

      // subsampling
      values.subsampling = sl->getProperties()->getSubsampling();

      values.doPremulti =
          (sl->getProperties()->doPremultiply()) ? Qt::Checked : Qt::Unchecked;
      values.whiteTransp =
          (sl->getProperties()->whiteTransp()) ? Qt::Checked : Qt::Unchecked;
      values.doAntialias = (sl->getProperties()->antialiasSoftness() > 0)
                               ? Qt::Checked
                               : Qt::Unchecked;
      values.softness    = sl->getProperties()->antialiasSoftness();

      if (levelType == LinearRaster)
        values.colorSpaceGamma = sl->getProperties()->colorSpaceGamma();
    }
  }

  // gather dirty flags (subsampling is editable for only non-dirty levels)
  if (sl && sl->getProperties()->getDirtyFlag()) values.isDirty = Qt::Checked;

  return values;
}

//-----------------------------------------------------------------------------
/*! Update popup value.
                Take current level and act on level type set popup value.
*/
void LevelSettingsPopup::updateLevelSettings() {
  TApp *app = TApp::instance();
  m_selectedLevels.clear();
  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(app->getCurrentSelection()->getSelection());
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      app->getCurrentSelection()->getSelection());
  TColumnSelection *columnSelection = dynamic_cast<TColumnSelection *>(
      app->getCurrentSelection()->getSelection());
  FxSelection *fxSelection =
      dynamic_cast<FxSelection *>(app->getCurrentSelection()->getSelection());
  // - - - Cell Selection
  if (cellSelection) {
    TXsheet *currentXsheet = app->getCurrentXsheet()->getXsheet();
    if (currentXsheet && !cellSelection->isEmpty()) {
      int r0, c0, r1, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);
      for (int c = c0; c <= c1; c++)
        for (int r = r0; r <= r1; r++)
          if (TXshLevelP level = currentXsheet->getCell(r, c).m_level)
            m_selectedLevels.insert(level);
    } else
      m_selectedLevels.insert(app->getCurrentLevel()->getLevel());
  }
  // - - - Column Selection
  else if (columnSelection) {
    TXsheet *currentXsheet = app->getCurrentXsheet()->getXsheet();
    if (currentXsheet && !columnSelection->isEmpty()) {
      int sceneLength             = currentXsheet->getFrameCount();
      std::set<int> columnIndices = columnSelection->getIndices();
      std::set<int>::iterator it;
      for (it = columnIndices.begin(); it != columnIndices.end(); ++it) {
        int columnIndex = *it;
        int r0, r1;
        currentXsheet->getCellRange(columnIndex, r0, r1);
        if (r1 < 0) continue;  // skip empty column
        for (int r = r0; r <= r1; r++)
          if (TXshLevelP level = currentXsheet->getCell(r, columnIndex).m_level)
            m_selectedLevels.insert(level);
      }
    } else
      m_selectedLevels.insert(app->getCurrentLevel()->getLevel());
  }
  // - - - Cast Selection
  else if (castSelection) {
    std::vector<TXshLevel *> levels;
    castSelection->getSelectedLevels(levels);
    for (TXshLevel *level : levels) m_selectedLevels.insert(level);
  }
  // - - - Schematic Nodes Selection
  else if (fxSelection) {
    TXsheet *currentXsheet  = app->getCurrentXsheet()->getXsheet();
    QList<TFxP> selectedFxs = fxSelection->getFxs();
    if (currentXsheet && !selectedFxs.isEmpty()) {
      for (int f = 0; f < selectedFxs.size(); f++) {
        TLevelColumnFx *lcfx =
            dynamic_cast<TLevelColumnFx *>(selectedFxs.at(f).getPointer());
        if (lcfx) {
          int r0, r1;
          int firstRow =
              lcfx->getXshColumn()->getCellColumn()->getRange(r0, r1);
          for (int r = r0; r <= r1; r++)
            if (TXshLevelP level =
                    lcfx->getXshColumn()->getCellColumn()->getCell(r).m_level)
              m_selectedLevels.insert(level);
        }
      }
      if (m_selectedLevels.isEmpty())
        m_selectedLevels.insert(app->getCurrentLevel()->getLevel());
    } else
      m_selectedLevels.insert(app->getCurrentLevel()->getLevel());
  } else
    m_selectedLevels.insert(app->getCurrentLevel()->getLevel());

  // Unite flags, Remove unsupported items
  unsigned int levelTypeFlag         = 0;
  QSet<TXshLevelP>::iterator lvl_itr = m_selectedLevels.begin();
  while (lvl_itr != m_selectedLevels.end()) {
    SelectedLevelType type = getType(*lvl_itr);
    if (type == None || type == Others)
      lvl_itr = m_selectedLevels.erase(lvl_itr);
    else {
      levelTypeFlag |= type;
      ++lvl_itr;
    }
  }
  if (m_selectedLevels.count() >= 2)
    levelTypeFlag |= MultiSelection;
  else if (m_selectedLevels.isEmpty())
    levelTypeFlag |= NoSelection;

  // Enable / Disable all widgets
  QMap<QWidget *, unsigned int>::iterator w_itr = m_activateFlags.begin();
  while (w_itr != m_activateFlags.end()) {
    unsigned int activateFlag = w_itr.value();
    w_itr.key()->setEnabled((levelTypeFlag & activateFlag) == levelTypeFlag);
    ++w_itr;
  }

  // Gather values
  LevelSettingsValues values;
  lvl_itr = m_selectedLevels.begin();
  while (lvl_itr != m_selectedLevels.end()) {
    TXshLevelP level            = *lvl_itr;
    bool isFirst                = (lvl_itr == m_selectedLevels.begin());
    LevelSettingsValues new_val = getValues(level);

    uniteValue(values.name, new_val.name, isFirst);
    uniteValue(values.path, new_val.path, isFirst);
    uniteValue(values.scanPath, new_val.scanPath, isFirst);
    uniteValue(values.typeStr, new_val.typeStr, isFirst);
    uniteValue(values.dpiType, new_val.dpiType, isFirst);
    uniteValue(values.dpi, new_val.dpi, isFirst);
    uniteValue(values.width, new_val.width, isFirst);
    uniteValue(values.height, new_val.height, isFirst);
    uniteValue(values.imageDpi, new_val.imageDpi, isFirst);
    uniteValue(values.imageRes, new_val.imageRes, isFirst);
    uniteValue(values.doPremulti, new_val.doPremulti, isFirst);
    uniteValue(values.whiteTransp, new_val.whiteTransp, isFirst);
    uniteValue(values.doAntialias, new_val.doAntialias, isFirst);
    uniteValue(values.softness, new_val.softness, isFirst);
    uniteValue(values.subsampling, new_val.subsampling, isFirst);
    uniteValue(values.colorSpaceGamma, new_val.colorSpaceGamma, isFirst);
    uniteValue(values.isDirty, new_val.isDirty, isFirst);

    ++lvl_itr;
  }

  m_nameFld->setText(values.name);
  m_pathFld->setPath(values.path);
  m_scanPathFld->setPath(values.scanPath);
  m_typeLabel->setText(values.typeStr);
  m_dpiTypeOm->setCurrentIndex(values.dpiType);
  m_dpiFld->setText(dpiToString(values.dpi));
  if (values.width <= 0.0) {
    m_widthFld->setValue(0.0);
    m_widthFld->setText((values.width < 0.0) ? tr("[Various]") : tr(""));
  } else
    m_widthFld->setValue(values.width);
  if (values.height <= 0.0) {
    m_heightFld->setValue(0.0);
    m_heightFld->setText((values.height < 0.0) ? tr("[Various]") : tr(""));
  } else
    m_heightFld->setValue(values.height);

  m_cameraDpiLabel->setText(dpiToString(getCurrentCameraDpi()));
  m_imageDpiLabel->setText(values.imageDpi);
  m_imageResLabel->setText(values.imageRes);

  m_doPremultiply->setCheckState(values.doPremulti);
  m_whiteTransp->setCheckState(values.whiteTransp);
  m_doAntialias->setCheckState(values.doAntialias);
  if (values.softness < 0)
    m_antialiasSoftness->setText("");
  else
    m_antialiasSoftness->setValue(values.softness);
  if (values.subsampling < 0)
    m_subsamplingFld->setText("");
  else
    m_subsamplingFld->setValue(values.subsampling);

  if (values.colorSpaceGamma > 0)
    m_colorSpaceGammaFld->setValue(values.colorSpaceGamma);
  else if (m_colorSpaceGammaFld->isEnabled())
    m_colorSpaceGammaFld->setText(tr("[Various]"));
  else
    m_colorSpaceGammaFld->setText("");

  // disable the softness field when the antialias is not active
  if (m_doAntialias->checkState() != Qt::Checked)
    m_antialiasSoftness->setDisabled(true);
  // disable the subsampling field when dirty level is selected
  if (values.isDirty != Qt::Unchecked) {
    m_subsamplingFld->setDisabled(true);
    m_colorSpaceGammaFld->setDisabled(true);
  }
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onNameChanged() {
  assert(m_selectedLevels.size() == 1);
  if (m_selectedLevels.size() != 1) return;
  QString text = m_nameFld->text();
  if (text.length() == 0) {
    error("The name " + text +
          " you entered for the level is not valid.\n Please enter a different "
          "name.");
    m_nameFld->setFocus();
    return;
  }

  TXshLevel *level = m_selectedLevels.begin()->getPointer();
  if ((getType(level) & (SimpleLevel | SubXsheet)) == 0) return;

  // in case the level name is not changed
  QString oldName = QString::fromStdWString(level->getName());
  if (oldName == text) return;

  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();

  TUndoManager::manager()->beginBlock();

  // remove existing & unused level
  if (Preferences::instance()->isAutoRemoveUnusedLevelsEnabled()) {
    TXshLevel *existingLevel = levelSet->getLevel(text.toStdWString());
    if (existingLevel &&
        LevelCmd::removeLevelFromCast(existingLevel, nullptr, false))
      DVGui::info(QObject::tr("Removed unused level %1 from the scene cast. "
                              "(This behavior can be disabled in Preferences.)")
                      .arg(text));
  }

  if (!levelSet->renameLevel(level, text.toStdWString())) {
    error("The name " + text +
          " you entered for the level is already used.\nPlease enter a "
          "different name.");
    m_nameFld->setFocus();
    TUndoManager::manager()->endBlock();
    return;
  }

  TUndoManager::manager()->add(
      new LevelSettingsUndo(level, LevelSettingsUndo::Name, oldName, text));

  TUndoManager::manager()->endBlock();

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onPathChanged() {
  assert(m_selectedLevels.size() == 1);
  if (m_selectedLevels.size() != 1) return;
  TXshLevelP levelP = *m_selectedLevels.begin();
  if ((m_activateFlags[m_pathFld] & getType(levelP)) == 0) return;

  QString text = m_pathFld->getPath();
  TFilePath newPath(text.toStdWString());
  newPath =
      TApp::instance()->getCurrentScene()->getScene()->codeFilePath(newPath);
  m_pathFld->setPath(QString::fromStdWString(newPath.getWideString()));

  TXshSoundLevelP sdl = levelP->getSoundLevel();
  if (sdl) {
    // old level is a sound level
    TFileType::Type levelType = TFileType::getInfo(newPath);
    if (levelType == TFileType::AUDIO_LEVEL) {
      TFilePath oldPath = sdl->getPath();
      if (oldPath == newPath) return;
      sdl->setPath(newPath);
      sdl->loadSoundTrack();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
      TUndoManager::manager()->add(
          new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::Path,
                                oldPath.getQString(), newPath.getQString()));
    } else {
      error(tr("The file %1 is not a sound level.")
                .arg(QString::fromStdWString(newPath.getLevelNameW())));
      updateLevelSettings();
    }
    return;
  }

  TXshSimpleLevelP sl  = levelP->getSimpleLevel();
  TXshPaletteLevelP pl = levelP->getPaletteLevel();
  if (!sl && !pl) return;
  TFilePath oldPath = (sl) ? sl->getPath() : pl->getPath();
  if (oldPath == newPath) return;

  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  TXshLevel *lvlWithSamePath = nullptr;
  for (int i = 0; i < levelSet->getLevelCount(); i++) {
    TXshLevel *tmpLvl = levelSet->getLevel(i);
    if (!tmpLvl) continue;
    if (tmpLvl == levelP.getPointer()) continue;
    TXshSimpleLevelP tmpSl  = tmpLvl->getSimpleLevel();
    TXshPaletteLevelP tmpPl = tmpLvl->getPaletteLevel();
    if (!tmpSl && !tmpPl) continue;
    TFilePath tmpPath = (tmpSl) ? tmpSl->getPath() : tmpPl->getPath();
    if (tmpPath == newPath) {
      lvlWithSamePath = tmpLvl;
      break;
    }
  }
  if (lvlWithSamePath) {
    QString question;

    question = "The path you entered for the level " +
               QString(::to_string(lvlWithSamePath->getName()).c_str()) +
               "is already used: this may generate some conflicts in the file "
               "management.\nAre you sure you want to assign the same path to "
               "two different levels?";
    int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
    if (ret == 0 || ret == 2) {
      m_pathFld->setPath(toQString(oldPath));
      return;
    }
  }

  TFileType::Type oldType = TFileType::getInfo(oldPath);
  TFileType::Type newType = TFileType::getInfo(newPath);

  if (sl && sl->getType() == TZP_XSHLEVEL &&
      sl->getScannedPath() != TFilePath()) {
    if (newPath == TFilePath() || newPath == sl->getScannedPath()) {
      newPath = sl->getScannedPath();
      sl->setType(OVL_XSHLEVEL);
      sl->setScannedPath(TFilePath());
      sl->setPath(newPath);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->notifyCastChange();
      updateLevelSettings();
      sl->invalidateFrames();
      std::vector<TFrameId> frames;
      sl->getFids(frames);
      for (auto const &fid : frames) {
        IconGenerator::instance()->invalidate(sl.getPointer(), fid);
      }
      TUndoManager::manager()->beginBlock();
      TUndoManager::manager()->add(
          new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::Path,
                                oldPath.getQString(), newPath.getQString()));
      TUndoManager::manager()->add(new LevelSettingsUndo(
          levelP.getPointer(), LevelSettingsUndo::ScanPath,
          newPath.getQString(), QString()));
      TUndoManager::manager()->add(new LevelSettingsUndo(
          levelP.getPointer(), LevelSettingsUndo::LevelType, TZP_XSHLEVEL,
          OVL_XSHLEVEL));
      TUndoManager::manager()->endBlock();
      return;
    }
  }

  if (oldType != newType ||
      (sl && sl->getType() == TZP_XSHLEVEL && newPath.getType() != "tlv") ||
      (sl && sl->getType() != TZP_XSHLEVEL && newPath.getType() == "tlv") ||
      (pl && newPath.getType() != "tpl")) {
    error("Wrong path");
    m_pathFld->setPath(toQString(oldPath));
    return;
  }
  //-- update the path here
  if (sl) {
    sl->setPath(newPath);
    TApp::instance()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->setPalette(sl->getPalette());
    sl->invalidateFrames();
    std::vector<TFrameId> frames;
    sl->getFids(frames);
    for (auto const &fid : frames) {
      IconGenerator::instance()->invalidate(sl.getPointer(), fid);
    }
  } else if (pl) {
    pl->setPath(newPath);
    TApp::instance()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->setPalette(pl->getPalette());
    pl->load();
  }
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  updateLevelSettings();
  TUndoManager::manager()->add(
      new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::Path,
                            oldPath.getQString(), newPath.getQString()));
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onScanPathChanged() {
  assert(m_selectedLevels.size() == 1);
  if (m_selectedLevels.size() != 1) return;
  TXshLevelP levelP   = *m_selectedLevels.begin();
  TXshSimpleLevelP sl = levelP->getSimpleLevel();
  if (!sl || sl->getType() != TZP_XSHLEVEL) return;

  QString text = m_scanPathFld->getPath();
  TFilePath newScanPath(text.toStdWString());

  // limit the available formats for now
  if (newScanPath.getType() != "tif" && newScanPath.getType() != "tiff" &&
      newScanPath.getType() != "tzi") {
    m_scanPathFld->setPath(toQString(sl->getScannedPath()));
    return;
  }

  newScanPath = TApp::instance()->getCurrentScene()->getScene()->codeFilePath(
      newScanPath);
  m_scanPathFld->setPath(QString::fromStdWString(newScanPath.getWideString()));

  TFilePath oldScanPath = sl->getScannedPath();
  if (oldScanPath == newScanPath) return;

  // check if the "Cleanup Settings > SaveIn" value is the same as the current
  // level
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
      if (sl->getPath().getParentDir() != cleanupLevelPath &&
          sl->getPath().getParentDir() != codedCleanupLevelPath)
        DVGui::warning(
            "\"Save In\" path of the Cleanup Settings does not match.");
    } else
      DVGui::warning("Loading Cleanup Settings failed.");
  } else
    DVGui::warning(".cln file for the Scanned Level not found.");

  sl->setScannedPath(newScanPath);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  updateLevelSettings();
  TUndoManager::manager()->add(new LevelSettingsUndo(
      levelP.getPointer(), LevelSettingsUndo::ScanPath,
      oldScanPath.getQString(), newScanPath.getQString()));
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onDpiTypeChanged(int index) {
  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    QString dpiPolicyStr = m_dpiTypeOm->itemData(index).toString();
    assert(dpiPolicyStr == "Custom DPI" || dpiPolicyStr == "Image DPI");
    LevelProperties *prop = sl->getProperties();
    // dpiPolicyStr ==> dpiPolicy
    LevelProperties::DpiPolicy dpiPolicy = dpiPolicyStr == "Custom DPI"
                                               ? LevelProperties::DP_CustomDpi
                                               : LevelProperties::DP_ImageDpi;
    // se ImageDpi, ma l'immagine non ha dpi -> CustomDpi
    assert(dpiPolicy == LevelProperties::DP_CustomDpi ||
           sl->getImageDpi().x > 0.0 && sl->getImageDpi().y > 0.0);

    if (prop->getDpiPolicy() == dpiPolicy) continue;
    LevelProperties::DpiPolicy oldPolicy = prop->getDpiPolicy();
    prop->setDpiPolicy(dpiPolicy);
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::DpiType, oldPolicy, dpiPolicy));
  }
  TUndoManager::manager()->endBlock();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onDpiFieldChanged() {
  QString w     = m_dpiFld->text();
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
  if (dpi.x <= 0.0) {
    updateLevelSettings();
    return;
  }
  if (dpi.y == 0.0) dpi.y = dpi.x;

  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    TPointD oldDpi = sl->getProperties()->getDpi();
    if (oldDpi == dpi) continue;

    LevelProperties::DpiPolicy oldPolicy = sl->getProperties()->getDpiPolicy();
    sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    sl->getProperties()->setDpi(dpi);

    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::DpiType,
                              oldPolicy, LevelProperties::DP_CustomDpi));
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::Dpi,
        QPointF(oldDpi.x, oldDpi.y), QPointF(dpi.x, dpi.y)));
  }
  TUndoManager::manager()->endBlock();
  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSquarePixelChanged(int value) {
  if (!value) return;
  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    TPointD oldDpi = sl->getDpi();
    if (areAlmostEqual(oldDpi.x, oldDpi.y)) continue;
    TPointD dpi(oldDpi.x, oldDpi.x);
    LevelProperties::DpiPolicy oldPolicy = sl->getProperties()->getDpiPolicy();
    sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    sl->getProperties()->setDpi(dpi);

    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::DpiType,
                              oldPolicy, LevelProperties::DP_CustomDpi));
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::Dpi,
        QPointF(oldDpi.x, oldDpi.y), QPointF(dpi.x, dpi.y)));
  }
  TUndoManager::manager()->endBlock();
  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onWidthFieldChanged() {
  double lx = m_widthFld->getValue();
  if (lx <= 0) {
    updateLevelSettings();
    return;
  }

  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    TPointD dpi = sl->getDpi();
    if (dpi.x <= 0) continue;
    TDimension res = sl->getResolution();
    if (res.lx <= 0) continue;
    dpi.x = res.lx / lx;
    if (m_squarePixCB->isChecked()) dpi.y = dpi.x;

    LevelProperties::DpiPolicy oldPolicy = sl->getProperties()->getDpiPolicy();
    sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    TPointD oldDpi = sl->getProperties()->getDpi();
    sl->getProperties()->setDpi(dpi);

    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::DpiType,
                              oldPolicy, LevelProperties::DP_CustomDpi));
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::Dpi,
        QPointF(oldDpi.x, oldDpi.y), QPointF(dpi.x, dpi.y)));
  }
  TUndoManager::manager()->endBlock();
  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onHeightFieldChanged() {
  double ly = m_heightFld->getValue();
  if (ly <= 0) {
    updateLevelSettings();
    return;
  }

  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    TPointD dpi = sl->getDpi();
    if (dpi.y <= 0) continue;
    TDimension res = sl->getResolution();
    if (res.ly <= 0) continue;
    dpi.y = res.ly / ly;
    if (m_squarePixCB->isChecked()) dpi.x = dpi.y;

    LevelProperties::DpiPolicy oldPolicy = sl->getProperties()->getDpiPolicy();
    sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    TPointD oldDpi = sl->getProperties()->getDpi();
    sl->getProperties()->setDpi(dpi);

    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::DpiType,
                              oldPolicy, LevelProperties::DP_CustomDpi));
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::Dpi,
        QPointF(oldDpi.x, oldDpi.y), QPointF(dpi.x, dpi.y)));
  }
  TUndoManager::manager()->endBlock();
  updateLevelSettings();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::useCameraDpi() {
  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl) continue;
    LevelProperties *prop                = sl->getProperties();
    LevelProperties::DpiPolicy oldPolicy = prop->getDpiPolicy();
    prop->setDpiPolicy(LevelProperties::DP_CustomDpi);
    TPointD oldDpi = prop->getDpi();
    TPointD dpi    = getCurrentCameraDpi();
    prop->setDpi(dpi);

    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::DpiType,
                              oldPolicy, LevelProperties::DP_CustomDpi));
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::Dpi,
        QPointF(oldDpi.x, oldDpi.y), QPointF(dpi.x, dpi.y)));
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onSubsamplingChanged() {
  int subsampling = (int)m_subsamplingFld->getValue();
  if (subsampling <= 0) {
    updateLevelSettings();
    return;
  }

  bool somethingChanged = false;
  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    if (sl->getProperties()->getDirtyFlag()) continue;

    int oldSubsampling = sl->getProperties()->getSubsampling();
    if (oldSubsampling == subsampling) continue;

    sl->getProperties()->setSubsampling(subsampling);
    sl->invalidateFrames();
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::Subsampling, oldSubsampling,
        subsampling));
    somethingChanged = true;
  }
  TUndoManager::manager()->endBlock();
  if (!somethingChanged) return;

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()
      ->getCurrentXsheet()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onDoPremultiplyClicked() {
  Qt::CheckState state = m_doPremultiply->checkState();
  bool on = (state == Qt::Checked || state == Qt::PartiallyChecked);

  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevel *sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    LevelProperties *prop = sl->getProperties();
    if (on == prop->doPremultiply()) continue;
    if (on && prop->whiteTransp()) {
      prop->setWhiteTransp(false);
      TUndoManager::manager()->add(new LevelSettingsUndo(
          levelP.getPointer(), LevelSettingsUndo::WhiteTransp, true, false));
    }
    prop->setDoPremultiply(on);
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::DoPremulti, !on, on));
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onAntialiasSoftnessChanged() {
  int softness = m_antialiasSoftness->getValue();
  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevel *sl = levelP->getSimpleLevel();
    if (!sl) continue;
    int oldSoftness = sl->getProperties()->antialiasSoftness();
    sl->getProperties()->setDoAntialias(softness);
    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::Softness,
                              oldSoftness, softness));
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//----------------------------

void LevelSettingsPopup::onDoAntialiasClicked() {
  Qt::CheckState state = m_doAntialias->checkState();
  bool on      = (state == Qt::Checked || state == Qt::PartiallyChecked);
  int softness = on ? m_antialiasSoftness->getValue() : 0;

  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevel *sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;

    int oldSoftness = sl->getProperties()->antialiasSoftness();
    sl->getProperties()->setDoAntialias(softness);
    TUndoManager::manager()->add(
        new LevelSettingsUndo(levelP.getPointer(), LevelSettingsUndo::Softness,
                              oldSoftness, softness));
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  updateLevelSettings();
  if (on) {
    m_doAntialias->setCheckState(Qt::Checked);
    m_antialiasSoftness->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onWhiteTranspClicked() {
  Qt::CheckState state = m_whiteTransp->checkState();
  bool on = (state == Qt::Checked || state == Qt::PartiallyChecked);

  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevel *sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL) continue;
    LevelProperties *prop = sl->getProperties();
    if (on == prop->whiteTransp()) continue;
    if (on && prop->doPremultiply()) {
      prop->setDoPremultiply(false);
      TUndoManager::manager()->add(new LevelSettingsUndo(
          levelP.getPointer(), LevelSettingsUndo::DoPremulti, true, false));
    }
    prop->setWhiteTransp(on);
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::WhiteTransp, !on, on));
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  updateLevelSettings();
}

//-----------------------------------------------------------------------------

void LevelSettingsPopup::onColorSpaceGammaFieldChanged() {
  double colorSpaceGamma = m_colorSpaceGammaFld->getValue();

  bool somethingChanged = false;
  TUndoManager::manager()->beginBlock();
  QSetIterator<TXshLevelP> levelItr(m_selectedLevels);
  while (levelItr.hasNext()) {
    TXshLevelP levelP   = levelItr.next();
    TXshSimpleLevelP sl = levelP->getSimpleLevel();
    if (!sl || sl->getType() != OVL_XSHLEVEL) continue;
    if (sl->getProperties()->getDirtyFlag()) continue;

    double oldGamma = sl->getProperties()->colorSpaceGamma();
    if (areAlmostEqual(colorSpaceGamma, oldGamma)) continue;

    sl->getProperties()->setColorSpaceGamma(colorSpaceGamma);
    sl->invalidateFrames();
    TUndoManager::manager()->add(new LevelSettingsUndo(
        levelP.getPointer(), LevelSettingsUndo::ColorSpaceGamma, oldGamma,
        colorSpaceGamma));
    somethingChanged = true;
  }
  TUndoManager::manager()->endBlock();
  if (!somethingChanged) return;

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()
      ->getCurrentXsheet()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
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
