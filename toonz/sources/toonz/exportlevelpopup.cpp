

#include "exportlevelpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "filebrowser.h"
#include "columnselection.h"
#include "menubarcommandids.h"
#include "iocommand.h"
#include "exportlevelcommand.h"
#include "formatsettingspopups.h"

// TnzQt includes
#include "toonzqt/checkbox.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/framenavigator.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tcamera.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tproject.h"
#include "toonz/stage.h"
#include "toonz/preferences.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"

// TnzCore includes
#include "tiio.h"
#include "tproperty.h"

// Qt includes
#include <QDir>

#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QTabBar>
#include <QStackedWidget>
#include <QSplitter>
#include <QToolBar>

//********************************************************************************
//    Export callbacks  definition
//********************************************************************************

namespace {

struct MultiExportOverwriteCB final : public IoCmd::OverwriteCallbacks {
  bool m_yesToAll;
  bool m_stopped;

  MultiExportOverwriteCB() : m_yesToAll(false), m_stopped(false) {}
  bool overwriteRequest(const TFilePath &fp) override {
    if (m_yesToAll) return true;
    if (m_stopped) return false;

    int ret = DVGui::MsgBox(
        QObject::tr("Warning: file %1 already exists.").arg(toQString(fp)),
        QObject::tr("Continue Exporting"), QObject::tr("Continue to All"),
        QObject::tr("Stop Exporting"), 1);

    m_yesToAll = (ret == 2);
    m_stopped  = (ret == 0) || (ret == 3);
    return !m_stopped;
  }
};

//=============================================================================

struct MultiExportProgressCB final : public IoCmd::ProgressCallbacks {
  QString m_processedName;
  DVGui::ProgressDialog m_pb;

public:
  MultiExportProgressCB() : m_pb("", QObject::tr("Cancel"), 0, 0) {
    m_pb.show();
  }
  static QString msg() {
    return QObject::tr("Exporting level of %1 frames in %2");
  }

  void setProcessedName(const QString &name) override {
    m_processedName = name;
  }
  void setRange(int min, int max) override {
    m_pb.setMaximum(max);
    buildString();
  }
  void setValue(int val) override { m_pb.setValue(val); }
  bool canceled() const override { return m_pb.wasCanceled(); }
  void buildString() {
    m_pb.setLabelText(
        msg().arg(QString::number(m_pb.maximum())).arg(m_processedName));
  }
};

}  // namespace

//********************************************************************************
//    Swatch  definition
//********************************************************************************

class ExportLevelPopup::Swatch final : public PlaneViewer {
public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent) {}

  TImageP image() const { return m_img; }
  TImageP &image() { return m_img; }

protected:
  void showEvent(QShowEvent *se) override;
  void keyPressEvent(QKeyEvent *ke) override;
  void keyPressEvent(QShowEvent *se);
  void paintGL() override;

  void setActualPixelSize();

private:
  struct ShortcutZoomer final : public ImageUtils::ShortcutZoomer {
    ShortcutZoomer(Swatch *swatch) : ImageUtils::ShortcutZoomer(swatch) {}

  private:
    bool zoom(bool zoomin, bool resetZoom) override {
      return false;
    }  // Already covered by PlaneViewer
    bool setActualPixelSize() override {
      static_cast<Swatch *>(getWidget())->setActualPixelSize();
      return true;
    }
  };

private:
  TImageP m_img;  //!< Image shown in the swatch.
};

//========================================================================

void ExportLevelPopup::Swatch::showEvent(QShowEvent *se) {
  // Set current scene's chessboard color
  TPixel32 pix1, pix2;
  Preferences::instance()->getChessboardColors(pix1, pix2);

  setBgColor(pix1, pix2);
}

//------------------------------------------------------------------------

void ExportLevelPopup::Swatch::keyPressEvent(QKeyEvent *ke) {
  if (ShortcutZoomer(this).exec(ke)) return;

  PlaneViewer::keyPressEvent(ke);
}

//------------------------------------------------------------------------

void ExportLevelPopup::Swatch::paintGL() {
  glPushAttrib(GL_COLOR_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // Note GL_ONE instead of GL_SRC_ALPHA: it's needed since the input
  // image is supposedly premultiplied - and it works because the
  // viewer's background is opaque.
  // See tpixelutils.h's overPixT function for comparison.

  pushGLWorldCoordinates();
  {
    drawBackground();
    if (m_img) draw(m_img);
  }
  popGLCoordinates();

  glPopAttrib();
}

//------------------------------------------------------------------------

void ExportLevelPopup::Swatch::setActualPixelSize() {
  double dpix, dpiy;
  if (TRasterImageP ri = image())
    ri->getDpi(dpix, dpiy);
  else if (TToonzImageP ti = image())
    ti->getDpi(dpix, dpiy);
  else
    assert(false);

  if (dpix == 0.0 || dpiy == 0.0) dpix = dpiy = Stage::inch;

  setViewZoom(dpix / Stage::inch);
}

//********************************************************************************
//    ExportLevelPopup  implementation
//********************************************************************************

ExportLevelPopup::ExportLevelPopup()
    : FileBrowserPopup(tr("Export Level"), Options(CUSTOM_LAYOUT)) {
  setOkText(tr("Export"));

  TabBarContainter *tabBarContainer = new TabBarContainter;
  QTabBar *tabBar                   = new QTabBar;
  QStackedWidget *stackedWidget     = new QStackedWidget;
  QFrame *exportOptionsPage         = new QFrame;
  // Options / Swatch splitter
  QSplitter *splitter     = new QSplitter(Qt::Vertical);
  QScrollArea *scrollArea = new QScrollArea(splitter);
  QFrame *topWidget       = new QFrame(scrollArea);
  m_exportOptions         = new ExportLevelPopup::ExportOptions(splitter);
  m_swatch                = new Swatch(splitter);
  FrameNavigator *frameNavigator = new FrameNavigator;

  // Additional buttons between file name and Ok/Cancel
  QWidget *fileFormatContainer = new QWidget;
  // Format selection combo
  QLabel *formatLabel = new QLabel(tr("Format:"));
  m_format            = new QComboBox(this);
  // Retas compliant checkbox
  m_retas = new DVGui::CheckBox(tr("Retas Compliant"));
  // Format options button
  m_formatOptions = new QPushButton(tr("Options"));
  //-----

  tabBar->addTab(tr("File Browser"));
  tabBar->addTab(tr("Export Options"));

  tabBar->setDrawBase(false);
  tabBar->setExpanding(false);

  splitter->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                      QSizePolicy::MinimumExpanding));

  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setMinimumWidth(450);

  m_swatch->setMinimumHeight(150);
  m_swatch->setFocusPolicy(Qt::WheelFocus);

  static const int toolbarHeight = 22;
  frameNavigator->setFixedHeight(toolbarHeight);
  m_levelFrameIndexHandle.setFrame(
      0);  // Due to TFrameHandle's initialization, the initial frame
  frameNavigator->setFrameHandle(
      &m_levelFrameIndexHandle);  // is -1. Don't ask me why. Patching to 0.

  formatLabel->setFixedHeight(DVGui::WidgetHeight);
  m_format->setFixedHeight(DVGui::WidgetHeight);
  m_format->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

  QStringList formats;
  formats << "tga"
          << "tif"
          << "png"
          << "jpg"
          << "bmp";
  formats.sort();
  m_format->addItems(formats);

  m_retas->setMinimumHeight(DVGui::WidgetHeight);
  m_formatOptions->setMinimumSize(60, 25);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(3);
  {
    mainLayout->addWidget(tabBarContainer, 0);

    QHBoxLayout *tabBarLayout = new QHBoxLayout;
    tabBarLayout->setMargin(0);
    {
      tabBarLayout->addSpacing(6);
      tabBarLayout->addWidget(tabBar);
    }
    tabBarContainer->setLayout(tabBarLayout);

    // File Browser
    stackedWidget->addWidget(m_browser);
    // Export Options Page
    QVBoxLayout *eoPageLayout = new QVBoxLayout;
    eoPageLayout->setMargin(0);
    eoPageLayout->setSpacing(0);
    {
      // top area - options
      QVBoxLayout *topVLayout =
          new QVBoxLayout(topWidget);  // Needed to justify at top
      {
        topVLayout->addWidget(m_exportOptions);
        topVLayout->addStretch(1);
      }
      topWidget->setLayout(topVLayout);

      scrollArea->setWidget(topWidget);
      splitter->addWidget(scrollArea);

      // bottom area - swatch
      splitter->addWidget(m_swatch);
      splitter->setStretchFactor(0, 1);

      eoPageLayout->addWidget(splitter, 1);

      // toolbar
      QHBoxLayout *toolbarLayout = new QHBoxLayout;
      {
        toolbarLayout->addStretch(1);
        toolbarLayout->addWidget(frameNavigator, 0, Qt::AlignCenter);
        toolbarLayout->addStretch(1);
      }
      eoPageLayout->addLayout(toolbarLayout, 0);
    }
    exportOptionsPage->setLayout(eoPageLayout);
    stackedWidget->addWidget(exportOptionsPage);

    mainLayout->addWidget(stackedWidget, 1);

    //-------------- Buttons Toolbar ---------------------
    QHBoxLayout *bottomLay = new QHBoxLayout();
    bottomLay->setMargin(5);
    bottomLay->setSpacing(3);
    {
      bottomLay->addWidget(m_nameFieldLabel);
      bottomLay->addWidget(m_nameField, 1);

      QHBoxLayout *fileFormatLayout = new QHBoxLayout;
      fileFormatLayout->setMargin(0);
      fileFormatLayout->setSpacing(8);
      fileFormatLayout->setAlignment(Qt::AlignHCenter);
      {
        fileFormatLayout->addWidget(formatLabel);
        fileFormatLayout->addWidget(m_format);
        fileFormatLayout->addWidget(m_retas);
        fileFormatLayout->addWidget(m_formatOptions);
      }
      fileFormatContainer->setLayout(fileFormatLayout);
      bottomLay->addWidget(fileFormatContainer);

      bottomLay->addWidget(m_okButton);
      bottomLay->addWidget(m_cancelButton);
    }
    mainLayout->addLayout(bottomLay, 0);
  }
  setLayout(mainLayout);

  // Establish connections
  bool ret = true;
  ret      = connect(tabBar, SIGNAL(currentChanged(int)), stackedWidget,
                SLOT(setCurrentIndex(int)));
  ret = connect(m_format, SIGNAL(currentIndexChanged(const QString &)),
                SLOT(onformatChanged(const QString &))) &&
        ret;
  ret = connect(m_retas, SIGNAL(stateChanged(int)), SLOT(onRetas(int))) && ret;
  ret = connect(m_formatOptions, SIGNAL(clicked()), SLOT(onOptionsClicked())) &&
        ret;
  ret = connect(&m_levelFrameIndexHandle, SIGNAL(frameSwitched()),
                SLOT(updatePreview())) &&
        ret;
  ret = connect(m_exportOptions, SIGNAL(optionsChanged()),
                SLOT(updatePreview())) &&
        ret;
  assert(ret);
}

//-----------------------------------------------------------------------------

ExportLevelPopup::~ExportLevelPopup() {
  // The child FrameNavigator must be destroyed BEFORE m_levelFrameIndexHandle.
  // This is by FrameNavigator's implicit contract that the supplied frame
  // handle's
  // lifetime exceeds its own.

  QWidget().setLayout(layout());  // Reparents all layout-managed child widgets
}  // (i.e. all) to a temporary

//-----------------------------------------------------------------------------

void ExportLevelPopup::showEvent(QShowEvent *se) {
  // Save current selection - will be restored in the hideEvent()
  {
    TSelectionHandle *selectionHandle = TApp::instance()->getCurrentSelection();
    TSelection *selection             = selectionHandle->getSelection();
    if (Preferences::instance()->getPixelsOnly()) {
      m_exportOptions->m_widthFld->hide();
      m_exportOptions->m_heightFld->hide();
      m_exportOptions->m_widthLabel->hide();
      m_exportOptions->m_heightLabel->hide();
      m_exportOptions->m_dpiLabel->hide();
    } else {
      m_exportOptions->m_widthFld->show();
      m_exportOptions->m_heightFld->show();
      m_exportOptions->m_widthLabel->show();
      m_exportOptions->m_heightLabel->show();
      m_exportOptions->m_dpiLabel->show();
    }
    selectionHandle->pushSelection();
    selectionHandle->setSelection(selection);
  }

  // WARNING: What happens it the restored selection is NO MORE VALID ?
  //          Consider that this popup is NOT MODAL !!
  //
  //          The same applies to all the other popups in
  //          filebrowserpopup.cpp...

  updateOnSelection();  // Here the selection is used
  updatePreview();

  // Establish connections
  TApp *app = TApp::instance();

  bool ret = true;
  {
    ret = connect(app->getCurrentSelection(),
                  SIGNAL(selectionChanged(TSelection *)),
                  SLOT(updateOnSelection())) &&
          ret;

    ret = connect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel *)),
                  this, SLOT(updatePreview())) &&
          ret;
    ret = connect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()), this,
                  SLOT(updatePreview())) &&
          ret;
  }
  assert(ret);

  QDialog::showEvent(se);
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::hideEvent(QHideEvent *he) {
  QDialog::hideEvent(he);

  TApp *app = TApp::instance();

  app->getCurrentSelection()->disconnect(this);
  app->getCurrentLevel()->disconnect(this);

  // Restore original selection
  TApp::instance()->getCurrentSelection()->popSelection();

  // Reset popup state
  m_swatch->image() = TImageP();
}

//-------------------------------------------------------------------

TPropertyGroup *ExportLevelPopup::getFormatProperties(const std::string &ext) {
  if (m_formatProperties.find(ext) != m_formatProperties.end())
    return m_formatProperties[ext];

  TPropertyGroup *props   = Tiio::makeWriterProperties(ext);
  m_formatProperties[ext] = props;
  return props;
}

//-------------------------------------------------------------------

IoCmd::ExportLevelOptions ExportLevelPopup::getOptions(const std::string &ext) {
  IoCmd::ExportLevelOptions opts = m_exportOptions->getOptions();

  opts.m_props    = getFormatProperties(ext);
  opts.m_forRetas = (m_retas->checkState() == Qt::Checked);

  return opts;
}

//-------------------------------------------------------------------

void ExportLevelPopup::onformatChanged(const QString &text) { checkAlpha(); }

//------------------------------------------------------------------------

void ExportLevelPopup::checkAlpha() {
  QString ext = m_format->currentText();

  if (ext == "bmp" || ext == "jpg") {
    m_exportOptions->m_bgColorField->setAlphaActive(false);
    return;
  }

  TPropertyGroup *props = getFormatProperties(ext.toStdString());
  if (!props) return;

  bool withAlpha = false;
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *p = props->getProperty(i);

    const QString &name = p->getQStringName();
    const QString &val  = QString::fromStdString(p->getValueAsString());

    if (name == "Bits Per Pixel") {
      withAlpha = (val.contains("32") || val.contains("64"));
      break;
    } else if (name == "Alpha Channel") {
      withAlpha = (val == "1");
      break;
    }
  }

  m_exportOptions->m_bgColorField->setAlphaActive(withAlpha);
}

//----------------------------------------

void ExportLevelPopup::onOptionsClicked() {
  std::string ext       = m_format->currentText().toStdString();
  TPropertyGroup *props = getFormatProperties(ext);
  setModal(false);

  if (DVGui::Dialog *dialog =
          openFormatSettingsPopup(this, ext, props, m_browser->getFolder())) {
    bool ret;
    ret = connect(dialog, SIGNAL(dialogClosed()), SLOT(checkAlpha())),
    assert(ret);
  }
}

//--------------------------------------------------------------

void ExportLevelPopup::onRetas(int) {
  if (m_retas->checkState() == Qt::Checked) {
    m_format->setCurrentIndex(m_format->findText("tga"));
    m_format->setEnabled(false);
  } else
    m_format->setEnabled(true);
}

//--------------------------------------------------------------

void ExportLevelPopup::updateOnSelection() {
  // Disable name field in case of multiple selection
  TSelection *sel = TApp::instance()->getCurrentSelection()->getSelection();
  TColumnSelection *colSelection = dynamic_cast<TColumnSelection *>(sel);
  m_nameField->setEnabled(!colSelection ||
                          colSelection->getIndices().size() <= 1);

  // Enable tlv output in case all inputs are pli
  TApp *app = TApp::instance();

  bool allPlis;
  if (colSelection && colSelection->getIndices().size() >= 1) {
    TXsheet *xsh                    = app->getCurrentXsheet()->getXsheet();
    const std::set<int> &colIndices = colSelection->getIndices();

    std::set<int>::const_iterator it, end = colIndices.end();
    for (it = colIndices.begin(); it != end; ++it) {
      int r0, r1, c = *it;
      xsh->getCellRange(c, r0, r1);

      if (r0 <= r1) {
        TXshSimpleLevel *sl = xsh->getCell(r0, c).getSimpleLevel();
        assert(sl);

        if (!sl || (sl->getType() != PLI_XSHLEVEL)) break;
      }
    }

    allPlis = (it == end);
  } else {
    TXshSimpleLevel *sl = app->getCurrentLevel()->getSimpleLevel();
    allPlis             = sl && (sl->getType() == PLI_XSHLEVEL);
  }

  int tlvIdx = m_format->findText("tlv");
  if (allPlis) {
    if (tlvIdx < 0) m_format->addItem("tlv");
  } else {
    if (tlvIdx > 0) m_format->removeItem(tlvIdx);
  }

  m_exportOptions->updateOnSelection();
}

//--------------------------------------------------------------

void ExportLevelPopup::updatePreview() {
  // Build export preview arguments
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  int frameIdx        = m_levelFrameIndexHandle.getFrame();

  const std::string &ext                = m_format->currentText().toStdString();
  const IoCmd::ExportLevelOptions &opts = getOptions(ext);

  // Update displayed image in the preview
  m_swatch->image() =
      sl ? IoCmd::exportedImage(ext, *sl, sl->index2fid(frameIdx), opts)
         : TImageP();

  m_swatch->update();
}

//--------------------------------------------------------------

bool ExportLevelPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath fp(*m_selectedPaths.begin());

  // Build export options
  const std::string &ext                = m_format->currentText().toStdString();
  const IoCmd::ExportLevelOptions &opts = getOptions(ext);

  // Retrieve current column selection
  TApp *app                      = TApp::instance();
  TSelection *selection          = app->getCurrentSelection()->getSelection();
  TColumnSelection *colSelection = dynamic_cast<TColumnSelection *>(selection);
  if (colSelection && colSelection->getIndices().size() > 1) {
    fp = TFilePath(m_browser->getFolder() + TFilePath("a"))
             .withType(ext)
             .withFrame();

    bool ret = true;
    MultiExportOverwriteCB overwriteCB;
    MultiExportProgressCB progressCB;

    TXsheet *xsh                    = app->getCurrentXsheet()->getXsheet();
    const std::set<int> &colIndices = colSelection->getIndices();

    std::set<int>::const_iterator it, end = colIndices.end();
    for (it = colIndices.begin(); it != end; ++it) {
      if (progressCB.canceled()) break;

      int r0, r1, c = *it;
      xsh->getCellRange(c, r0, r1);

      if (r1 >= r0)  // There exists a not-empty cell
      {
        TXshSimpleLevel *sl = xsh->getCell(r0, c).getSimpleLevel();
        assert(sl);

        ret = ret && IoCmd::exportLevel(fp.withName(sl->getName()), sl, opts,
                                        &overwriteCB, &progressCB);
      }
    }

    return ret;
  } else {
    if (!isValidFileName(QString::fromStdString(fp.getName()))) {
      DVGui::error(
          tr("The file name cannot be empty or contain any of the following "
             "characters:(new line)  \\ / : * ? \"  |"));
      return false;
    }

    return IoCmd::exportLevel(fp.withType(ext).withFrame(), 0, opts);
  }
}

//--------------------------------------------------------------

void ExportLevelPopup::initFolder() {
  TFilePath fp;

  TProject *project =
      TProjectManager::instance()->getCurrentProject().getPointer();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  if (scene) fp = scene->decodeFilePath(TFilePath("+drawings"));

  QDir drawingsDir(QString::fromStdWString(fp.getWideString()));
  if (!drawingsDir.exists()) fp = project->getProjectFolder();

  m_browser->setFolder(fp);
}

//********************************************************************************
//    ExportLevelPopup::ExportOptions  implementation
//********************************************************************************

ExportLevelPopup::ExportOptions::ExportOptions(QWidget *parent)
    : QFrame(parent) {
  struct locals {
    static inline QGridLayout *newGridLayout() {
      QGridLayout *layout = new QGridLayout;

      layout->setColumnMinimumWidth(0, 50);
      layout->setColumnStretch(3, 1);

      return layout;
    }
  };

  static const double dmax = (std::numeric_limits<double>::max)(), dmin = -dmax;

  QGridLayout *layout = locals::newGridLayout();
  setLayout(layout);

  {
    int row = 0;

    //----------- Common Options ------------------

    QLabel *bgColorLabel = new QLabel(tr("Background Color:"));
    m_bgColorField =
        new DVGui::ColorField(0, false, TPixel32::White, 35, false);
    layout->addWidget(bgColorLabel, row, 1, Qt::AlignRight);
    layout->addWidget(m_bgColorField, row++, 2, Qt::AlignLeft);

    m_noAntialias = new QCheckBox(tr("No Antialias"));
    layout->addWidget(m_noAntialias, row++, 2, Qt::AlignLeft);

    //-------------- Vector Options ---------------------

    m_pliOptions = new QWidget;
    layout->addWidget(m_pliOptions, row++, 0, 1, 4);
    {
      QGridLayout *layout = locals::newGridLayout();
      m_pliOptions->setLayout(layout);

      layout->setMargin(0);

      int row = 0;

      //-------------- Vectors Export Box ---------------------

      DVGui::Separator *vectorCameraBoxSeparator = new DVGui::Separator;
      vectorCameraBoxSeparator->setName(tr("Vectors Export Box"));
      layout->addWidget(vectorCameraBoxSeparator, row++, 0, 1, 4);

      QGridLayout *exportBoxLayout = new QGridLayout;
      layout->addLayout(exportBoxLayout, row++, 1, 1, 2, Qt::AlignLeft);

      {
        m_widthFld    = new DVGui::MeasuredDoubleLineEdit;
        m_heightFld   = new DVGui::MeasuredDoubleLineEdit;
        m_hResFld     = new DVGui::IntLineEdit;
        m_vResFld     = new DVGui::IntLineEdit;
        m_dpiLabel    = new QLabel;
        m_widthLabel  = new QLabel;
        m_heightLabel = new QLabel;
        m_widthLabel->setText(tr("Width: "));
        m_heightLabel->setText(tr("Height: "));
        m_resScale = new DVGui::MeasuredDoubleLineEdit;

        m_widthFld->setRange(0, dmax);
        m_heightFld->setRange(0, dmax);
        m_hResFld->setRange(0, (std::numeric_limits<int>::max)());
        m_vResFld->setRange(0, (std::numeric_limits<int>::max)());
        m_resScale->setRange(0, dmax);

        m_resScale->setMeasure("percentage");
        m_resScale->setValue(1.0);

        m_widthFld->setFixedSize(50, 20);
        m_heightFld->setFixedSize(50, 20);
        m_vResFld->setFixedSize(50, 20);
        m_hResFld->setFixedSize(50, 20);
        m_resScale->setFixedSize(50, 20);
        exportBoxLayout->addWidget(m_widthLabel, 0, 0, Qt::AlignRight);
        exportBoxLayout->addWidget(m_widthFld, 0, 1, Qt::AlignLeft);
        exportBoxLayout->addWidget(m_heightLabel, 0, 2, Qt::AlignRight);
        exportBoxLayout->addWidget(m_heightFld, 0, 3, Qt::AlignLeft);
        exportBoxLayout->addWidget(m_dpiLabel, 0, 4, Qt::AlignRight);
        exportBoxLayout->addWidget(new QLabel(tr("H Resolution:")), 1, 0,
                                   Qt::AlignRight);
        exportBoxLayout->addWidget(m_hResFld, 1, 1, Qt::AlignLeft);
        exportBoxLayout->addWidget(new QLabel(tr("V Resolution:")), 1, 2,
                                   Qt::AlignRight);
        exportBoxLayout->addWidget(m_vResFld, 1, 3, Qt::AlignLeft);
        exportBoxLayout->addWidget(new QLabel(tr("Scale:")), 1, 4,
                                   Qt::AlignRight);
        exportBoxLayout->addWidget(m_resScale, 1, 5, Qt::AlignLeft);

        exportBoxLayout->setColumnStretch(6, 1);
      }

      //-------------- Vectors Thickness ---------------------

      DVGui::Separator *vectorThicknessSeparator = new DVGui::Separator;
      vectorThicknessSeparator->setName(tr("Vectors Thickness"));
      layout->addWidget(vectorThicknessSeparator, row++, 0, 1, 4);

      QGridLayout *thicknessLayout = new QGridLayout;
      layout->addLayout(thicknessLayout, row++, 1, 1, 2, Qt::AlignLeft);
      {
        QHBoxLayout *thicknessModeLayout = new QHBoxLayout;
        thicknessLayout->addLayout(thicknessModeLayout, 0, 0, Qt::AlignLeft);
        {
          thicknessModeLayout->addWidget(new QLabel(tr("Mode:")));

          m_thicknessTransformMode = new QComboBox;
          thicknessModeLayout->addWidget(m_thicknessTransformMode);

          m_thicknessTransformMode->addItems(
              QStringList() << tr("Scale Thickness") << tr("Add Thickness")
                            << tr("Constant Thickness"));

          thicknessModeLayout->addStretch(1);
        }

        QHBoxLayout *thicknessParamLayout = new QHBoxLayout;
        thicknessLayout->addLayout(thicknessParamLayout, 1, 0, Qt::AlignLeft);
        {
          m_fromThicknessScale        = new DVGui::MeasuredDoubleLineEdit;
          m_fromThicknessDisplacement = new DVGui::MeasuredDoubleLineEdit;
          m_toThicknessScale          = new DVGui::MeasuredDoubleLineEdit;
          m_toThicknessDisplacement   = new DVGui::MeasuredDoubleLineEdit;

          m_fromThicknessScale->setMeasure("percentage");
          m_toThicknessScale->setMeasure("percentage");

          m_fromThicknessScale->setRange(0, dmax);
          m_fromThicknessDisplacement->setRange(dmin, dmax);
          m_toThicknessScale->setRange(0, dmax);
          m_toThicknessDisplacement->setRange(dmin, dmax);

          m_fromThicknessScale->setValue(1.0);
          m_toThicknessScale->setValue(1.0);

          m_fromThicknessDisplacement->setValue(0.0);
          m_toThicknessDisplacement->setValue(0.0);

          thicknessParamLayout->addWidget(new QLabel(tr("Start:")));
          thicknessParamLayout->addWidget(m_fromThicknessScale);
          thicknessParamLayout->addWidget(m_fromThicknessDisplacement);

          thicknessParamLayout->addWidget(new QLabel(tr("End:")));
          thicknessParamLayout->addWidget(m_toThicknessScale);
          thicknessParamLayout->addWidget(m_toThicknessDisplacement);

          thicknessParamLayout->addStretch(1);

          onThicknessTransformModeChanged();
        }

        thicknessLayout->setColumnStretch(1, 1);
      }
    }
  }

  // Install connections
  bool ret = true;
  {
    ret = connect(m_widthFld, SIGNAL(editingFinished()), SLOT(updateXRes())) &&
          ret;
    ret = connect(m_heightFld, SIGNAL(editingFinished()), SLOT(updateYRes())) &&
          ret;
    ret = connect(m_hResFld, SIGNAL(editingFinished()), SLOT(updateYRes())) &&
          ret;  // Note that x and y here
    ret = connect(m_vResFld, SIGNAL(editingFinished()), SLOT(updateXRes())) &&
          ret;  // are exchanged
    ret =
        connect(m_resScale, SIGNAL(editingFinished()), SLOT(scaleRes())) && ret;

    ret = connect(m_thicknessTransformMode, SIGNAL(currentIndexChanged(int)),
                  SLOT(onThicknessTransformModeChanged())) &&
          ret;

    // Option changes
    ret = connect(m_bgColorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
                  SIGNAL(optionsChanged())) &&
          ret;
    ret = connect(m_noAntialias, SIGNAL(clicked(bool)),
                  SIGNAL(optionsChanged())) &&
          ret;

    // Queued connections for camera fields, to guarantee that whenever signals
    // editingFinished()
    // and valueChanged() are emitted at the same time, the former takes
    // precedence (applies value corrections)
    ret = connect(m_widthFld, SIGNAL(valueChanged()), SIGNAL(optionsChanged()),
                  Qt::QueuedConnection) &&
          ret;
    ret = connect(m_heightFld, SIGNAL(valueChanged()), SIGNAL(optionsChanged()),
                  Qt::QueuedConnection) &&
          ret;
    ret = connect(m_hResFld, SIGNAL(textChanged(const QString &)),
                  SIGNAL(optionsChanged()), Qt::QueuedConnection) &&
          ret;
    ret = connect(m_vResFld, SIGNAL(textChanged(const QString &)),
                  SIGNAL(optionsChanged()), Qt::QueuedConnection) &&
          ret;

    ret = connect(m_thicknessTransformMode, SIGNAL(currentIndexChanged(int)),
                  SIGNAL(optionsChanged())) &&
          ret;
    ret = connect(m_fromThicknessScale, SIGNAL(valueChanged()),
                  SIGNAL(optionsChanged())) &&
          ret;
    ret = connect(m_fromThicknessDisplacement, SIGNAL(valueChanged()),
                  SIGNAL(optionsChanged())) &&
          ret;
    ret = connect(m_toThicknessScale, SIGNAL(valueChanged()),
                  SIGNAL(optionsChanged())) &&
          ret;
    ret = connect(m_toThicknessDisplacement, SIGNAL(valueChanged()),
                  SIGNAL(optionsChanged())) &&
          ret;
  }
  assert(ret);

  updateCameraDefault();  // Default values must be available even
                          // if the widget has not yet been shown
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::showEvent(QShowEvent *se) {
  updateCameraDefault();
  updateOnSelection();
}

//-----------------------------------------------------------------------------

IoCmd::ExportLevelOptions ExportLevelPopup::ExportOptions::getOptions() const {
  IoCmd::ExportLevelOptions opts;

  opts.m_bgColor     = m_bgColorField->getColor();
  opts.m_noAntialias = m_noAntialias->isChecked();

  opts.m_camera.setSize(
      TDimensionD(m_widthFld->getValue(), m_heightFld->getValue()));
  opts.m_camera.setRes(
      TDimension(m_hResFld->getValue(), m_vResFld->getValue()));

  switch (m_thicknessTransformMode->currentIndex()) {
    enum { SCALE, ADD, CONSTANT };

  case SCALE:
    opts.m_thicknessTransform[0][0] = opts.m_thicknessTransform[1][0] = 0.0;
    opts.m_thicknessTransform[0][1] = m_fromThicknessScale->getValue();
    opts.m_thicknessTransform[1][1] = m_toThicknessScale->getValue();
    break;

  case ADD:
    opts.m_thicknessTransform[0][1] = opts.m_thicknessTransform[1][1] = 1.0;
    opts.m_thicknessTransform[0][0] =
        m_fromThicknessDisplacement->getValue() * Stage::inch;
    opts.m_thicknessTransform[1][0] =
        m_toThicknessDisplacement->getValue() * Stage::inch;
    break;

  case CONSTANT:
    opts.m_thicknessTransform[0][1] = opts.m_thicknessTransform[1][1] = 0.0;
    opts.m_thicknessTransform[0][0] =
        m_fromThicknessDisplacement->getValue() * Stage::inch;
    opts.m_thicknessTransform[1][0] =
        m_toThicknessDisplacement->getValue() * Stage::inch;
    break;
  }

  return opts;
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::updateOnSelection() {
  TApp *app = TApp::instance();

  TSelection *selection          = app->getCurrentSelection()->getSelection();
  TColumnSelection *colSelection = dynamic_cast<TColumnSelection *>(selection);

  if (colSelection && colSelection->getIndices().size() > 1) {
    bool enabled = false;

    TXsheet *xsh                    = app->getCurrentXsheet()->getXsheet();
    const std::set<int> &colIndices = colSelection->getIndices();

    std::set<int>::const_iterator it, end = colIndices.end();
    for (it = colIndices.begin(); it != end; ++it) {
      int r0, r1, c = *it;
      xsh->getCellRange(c, r0, r1);

      if (r0 <= r1)  // There exists a not-empty cell
      {
        TXshSimpleLevel *sl = xsh->getCell(r0, c).getSimpleLevel();
        assert(sl);

        enabled = enabled || (sl && (sl->getType() == PLI_XSHLEVEL));
      }
    }

    m_pliOptions->setEnabled(enabled);
    return;
  }

  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  m_pliOptions->setEnabled(
      sl && (sl->getType() != TZP_XSHLEVEL && sl->getType() != OVL_XSHLEVEL));
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::updateCameraDefault() {
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  assert(xsheet);

  TStageObjectId cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
  TStageObject *cameraObj = xsheet->getStageObject(cameraId);
  assert(cameraObj);

  TCamera *camera = cameraObj->getCamera();
  assert(camera);

  TDimensionD cameraSize = camera->getSize();
  m_widthFld->setValue(cameraSize.lx);
  m_heightFld->setValue(cameraSize.ly);

  TDimension cameraRes = camera->getRes();
  m_hResFld->setValue(cameraRes.lx);
  m_vResFld->setValue(cameraRes.ly);

  updateYRes();
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::updateXRes() {
  double ly = m_heightFld->getValue(), yres = m_vResFld->getValue(),
         dpi = ly > 0 ? yres / ly : 0;

  m_hResFld->setValue(tround(m_widthFld->getValue() * dpi));
  updateDpi();
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::updateYRes() {
  double lx = m_widthFld->getValue(), xres = m_hResFld->getValue(),
         dpi = lx > 0 ? xres / lx : 0;

  m_vResFld->setValue(tround(m_heightFld->getValue() * dpi));
  updateDpi();
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::scaleRes() {
  m_hResFld->setValue(tround(m_resScale->getValue() * m_hResFld->getValue()));
  m_resScale->setValue(1.0);

  updateYRes();
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::updateDpi() {
  double lx = m_widthFld->getValue(), xres = m_hResFld->getValue(),
         xdpi = lx > 0 ? xres / lx : 0;

  m_dpiLabel->setText(tr("DPI: ") + QString::number(xdpi, 'g', 4));
}

//-----------------------------------------------------------------------------

void ExportLevelPopup::ExportOptions::onThicknessTransformModeChanged() {
  bool scaleMode = (m_thicknessTransformMode->currentIndex() == 0);

  m_fromThicknessScale->setVisible(scaleMode);
  m_toThicknessScale->setVisible(scaleMode);

  m_fromThicknessDisplacement->setVisible(!scaleMode);
  m_toThicknessDisplacement->setVisible(!scaleMode);
}

//********************************************************************************
//    Export Level Command  instantiation
//********************************************************************************

OpenPopupCommandHandler<ExportLevelPopup> exportLevelPopupCommand(
    MI_ExportLevel);
