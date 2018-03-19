

// System includes
#include "tsystem.h"
#include "timagecache.h"
#include "ttimer.h"

// Geometry
#include "tgeometry.h"

// Image
#include "tiio.h"
#include "timageinfo.h"
#include "trop.h"
#include "tropcm.h"

// Sound
#include "tsop.h"
#include "tsound.h"

// Strings
#include "tconvert.h"

// File-related includes
#include "tfilepath.h"
#include "tfiletype.h"
#include "filebrowsermodel.h"
#include "fileviewerpopup.h"

// OpenGL
#include "tgl.h"
#include "tvectorgl.h"
#include "tvectorrenderdata.h"

// Qt helpers
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"

// App-Stage includes
#include "tapp.h"
#include "toutputproperties.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "toonz/tproject.h"

// Image painting
#include "toonz/imagepainter.h"

// Preview
#include "previewfxmanager.h"

// Panels
#include "pane.h"

// recent files
#include "mainwindow.h"

// Other widgets
#include "toonzqt/flipconsole.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "filmstripselection.h"
#include "castselection.h"
#include "histogrampopup.h"

// Qt includes
#include <QApplication>
#include <QDesktopWidget>
#include <QSettings>
#include <QPainter>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QButtonGroup>
#include <QToolBar>
#include <QMainWindow>
#include <QUrl>
#include <QObject>
#include <QDesktopServices>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

#include <stdint.h>  // for uintptr_t

#ifdef _WIN32
#include "avicodecrestrictions.h"
#endif

#include "flipbook.h"

//-----------------------------------------------------------------------------

using namespace ImageUtils;

namespace {
QString getShortcut(const char *id) {
  return QString::fromStdString(
      CommandManager::instance()->getShortcutFromId(id));
}
}  // namespace

//-----------------------------------------------------------------------------

namespace {
/* inline TRect getImageBounds(const TImageP& img)
  {
    if(TRasterImageP ri= img)
      return ri->getRaster()->getBounds();
    else if(TToonzImageP ti= img)
      return ti->getRaster()->getBounds();
    else
    {
      TVectorImageP vi= img;
      return convert(vi->getBBox());
    }
  }*/

//-----------------------------------------------------------------------------

inline TRectD getImageBoundsD(const TImageP &img) {
  if (TRasterImageP ri = img)
    return TRectD(0, 0, ri->getRaster()->getLx(), ri->getRaster()->getLy());
  else if (TToonzImageP ti = img)
    return TRectD(0, 0, ti->getSize().lx, ti->getSize().ly);
  else {
    TVectorImageP vi = img;
    return vi->getBBox();
  }
}
}  // namespace

//=============================================================================
/*! \class FlipBook
                \brief The FlipBook class provides to view level images.

                Inherits \b QWidget.

                The object is composed of grid layout \b QGridLayout which
   contains an image
                viewer \b ImageViewer and a double button bar. It is possible
   decide widget
                title and which button bar show by setting QString and bool in
   constructor.

                You can set level to show in FlipBook using \b setLevel(); and
   call
                onDrawFrame() to show FlipBook current frame. The current frame
   can be
                set directly using setCurrentFrame(int index) or using slot
   methods connected
                to button bar.

    \sa FlipBookPool class.
*/
FlipBook::FlipBook(QWidget *parent, QString viewerTitle,
                   UINT flipConsoleButtonMask, UCHAR flags,
                   bool isColorModel)  //, bool showOnlyPlayBackgroundButton)
    : QWidget(parent),
      m_viewerTitle(viewerTitle),
      m_levelNames(),
      m_levels(),
      m_playSound(false),
      m_snd(0),
      m_player(0)
      //, m_doCompare(false)
      ,
      m_currentFrameToSave(0),
      m_lw(),
      m_lr(),
      m_loadPopup(0),
      m_savePopup(0),
      m_shrink(1),
      m_isPreviewFx(false),
      m_previewedFx(0),
      m_previewXsh(0),
      m_previewUpdateTimer(this),
      m_xl(0),
      m_title1(),
      m_poolIndex(-1),
      m_freezed(false),
      m_loadbox(),
      m_dim(),
      m_loadboxes(),
      m_freezeButton(0),
      m_flags(flags) {
  setAcceptDrops(true);
  setFocusPolicy(Qt::StrongFocus);

  // flipConsoleButtonMask = flipConsoleButtonMask & ~FlipConsole::eSubCamera;

  ImageUtils::FullScreenWidget *fsWidget =
      new ImageUtils::FullScreenWidget(this);

  m_imageViewer = new ImageViewer(
      fsWidget, this, flipConsoleButtonMask == FlipConsole::cFullConsole);
  fsWidget->setWidget(m_imageViewer);

  setFocusProxy(m_imageViewer);
  m_title = m_viewerTitle;
  m_imageViewer->setIsColorModel(isColorModel);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  {
    mainLayout->addWidget(fsWidget, 1);
    m_flipConsole = new FlipConsole(
        mainLayout, flipConsoleButtonMask, true, 0,
        (viewerTitle == "") ? "FlipConsole" : viewerTitle, this, !isColorModel);
    mainLayout->addWidget(m_flipConsole);
  }
  setLayout(mainLayout);

  // signal-slot connection
  bool ret = connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                     this, SLOT(onButtonPressed(FlipConsole::EGadget)));

  m_flipConsole->setFrameRate(TApp::instance()
                                  ->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());

  mainLayout->addWidget(m_flipConsole);

  m_previewUpdateTimer.setSingleShot(true);

  ret = ret && connect(parentWidget(), SIGNAL(closeButtonPressed()), this,
                       SLOT(onCloseButtonPressed()));
  ret = ret && connect(parentWidget(), SIGNAL(doubleClick(QMouseEvent *)), this,
                       SLOT(onDoubleClick(QMouseEvent *)));
  ret = ret && connect(&m_previewUpdateTimer, SIGNAL(timeout()), this,
                       SLOT(performFxUpdate()));

  assert(ret);

  m_viewerTitle = (m_viewerTitle.isEmpty()) ? tr("Flipbook") : m_viewerTitle;
  parentWidget()->setWindowTitle(m_viewerTitle);
}

//-----------------------------------------------------------------------------
/*! add freeze button to the flipbook. called from the function
   PreviewFxManager::openFlipBook.
        this button will hide for re-use, at onCloseButtonPressed
*/
void FlipBook::addFreezeButtonToTitleBar() {
  // If there is the button already, then reuse it.
  if (m_freezeButton) {
    m_freezeButton->show();
    return;
  }

  // If there is not, then newly make it
  TPanel *panel = qobject_cast<TPanel *>(parentWidget());
  if (panel) {
    TPanelTitleBar *titleBar = panel->getTitleBar();
    m_freezeButton           = new TPanelTitleBarButton(
        titleBar, ":Resources/pane_freeze_off.svg",
        ":Resources/pane_freeze_over.svg", ":Resources/pane_freeze_on.svg");
    m_freezeButton->setToolTip("Freeze");
    titleBar->add(QPoint(-55, 0), m_freezeButton);
    connect(m_freezeButton, SIGNAL(toggled(bool)), this, SLOT(freeze(bool)));
    QPoint p(titleBar->width() - 62, 1);
    m_freezeButton->move(p);
    m_freezeButton->show();
  }
}

//-----------------------------------------------------------------------------

void FlipBook::freeze(bool on) {
  if (on)
    freezePreview();
  else
    unfreezePreview();
}

//-----------------------------------------------------------------------------

void FlipBook::focusInEvent(QFocusEvent *e) {
  m_flipConsole->makeCurrent();
  QWidget::focusInEvent(e);
}

//-----------------------------------------------------------------------------

namespace {

enum { eBegin, eIncrement, eEnd };

static DVGui::ProgressDialog *Pd = 0;

class ProgressBarMessager final : public TThread::Message {
public:
  int m_choice;
  int m_val;
  QString m_str;
  ProgressBarMessager(int choice, int val, const QString &str = "")
      : m_choice(choice), m_val(val), m_str(str) {}
  void onDeliver() override {
    switch (m_choice) {
    case eBegin:
      if (!Pd)
        Pd = new DVGui::ProgressDialog(
            QObject::tr("Saving previewed frames...."), QObject::tr("Cancel"),
            0, m_val);
      else
        Pd->setMaximum(m_val);
      Pd->show();
      break;
    case eIncrement:
      if (Pd->wasCanceled()) {
        delete Pd;
        Pd = 0;
      } else {
        // if (m_val==Pd->maximum()) Pd->hide();
        Pd->setValue(m_val);
      }
      break;
    case eEnd: {
      DVGui::info(m_str);
      delete Pd;
      Pd = 0;
    } break;
    default:
      assert(false);
    }
  }

  TThread::Message *clone() const override {
    return new ProgressBarMessager(*this);
  }
};

}  // namespace

//=============================================================================

LoadImagesPopup::LoadImagesPopup(FlipBook *flip)
    : FileBrowserPopup(tr("Load Images"), Options(WITH_APPLY_BUTTON),
                       tr("Append"), new QFrame(0))
    , m_flip(flip)
    , m_minFrame(0)
    , m_maxFrame(1000000)
    , m_step(1)
    , m_shrink(1) {
  QFrame *frameRangeFrame = (QFrame *)m_customWidget;

  frameRangeFrame->setObjectName("customFrame");
  frameRangeFrame->setFrameStyle(QFrame::StyledPanel);
  // frameRangeFrame->setFixedHeight(30);

  m_fromField   = new DVGui::LineEdit(this);
  m_toField     = new DVGui::LineEdit(this);
  m_stepField   = new DVGui::LineEdit("1", this);
  m_shrinkField = new DVGui::LineEdit("1", this);

  // Define the append/load filter types
  m_appendFilterTypes << "3gp"
                      << "mov"
                      << "jpg"
                      << "png"
                      << "tga"
                      << "tif"
                      << "tiff"
                      << "bmp"
                      << "sgi"
                      << "rgb"
                      << "nol";

#ifdef _WIN32
  m_appendFilterTypes << "avi";
#endif

  m_loadFilterTypes << "tlv"
                    << "pli" << m_appendFilterTypes;
  m_appendFilterTypes << "psd";

  // layout
  QHBoxLayout *frameRangeLayout = new QHBoxLayout();
  frameRangeLayout->setMargin(5);
  frameRangeLayout->setSpacing(5);
  {
    frameRangeLayout->addStretch(1);

    frameRangeLayout->addWidget(new QLabel(tr("From:")), 0);
    frameRangeLayout->addWidget(m_fromField, 0);

    frameRangeLayout->addSpacing(5);

    frameRangeLayout->addWidget(new QLabel(tr("To:")), 0);
    frameRangeLayout->addWidget(m_toField, 0);

    frameRangeLayout->addSpacing(10);

    frameRangeLayout->addWidget(new QLabel(tr("Step:")), 0);
    frameRangeLayout->addWidget(m_stepField, 0);

    frameRangeLayout->addSpacing(5);

    frameRangeLayout->addWidget(new QLabel(tr("Shrink:")));
    frameRangeLayout->addWidget(m_shrinkField, 0);
  }
  frameRangeFrame->setLayout(frameRangeLayout);

  // Make signal-slot connections
  bool ret = true;

  ret = ret && connect(m_fromField, SIGNAL(editingFinished()), this,
                       SLOT(onEditingFinished()));
  ret = ret && connect(m_toField, SIGNAL(editingFinished()), this,
                       SLOT(onEditingFinished()));
  ret = ret && connect(m_stepField, SIGNAL(editingFinished()), this,
                       SLOT(onEditingFinished()));
  ret = ret && connect(m_shrinkField, SIGNAL(editingFinished()), this,
                       SLOT(onEditingFinished()));

  ret = ret && connect(this, SIGNAL(filePathClicked(const TFilePath &)),
                       SLOT(onFilePathClicked(const TFilePath &)));

  assert(ret);

  setOkText(tr("Load"));

  setWindowTitle(tr("Load / Append Images"));
}

//-----------------------------------------------------------------------------

void LoadImagesPopup::onEditingFinished() {
  int val;
  val    = m_fromField->text().toInt();
  m_from = (val < m_minFrame) ? m_minFrame : val;
  val    = m_toField->text().toInt();
  m_to   = (val > m_maxFrame) ? m_maxFrame : val;

  if (m_to < m_from) m_to = m_from;

  val      = m_stepField->text().toInt();
  m_step   = (val < 1) ? 1 : val;
  val      = m_shrinkField->text().toInt();
  m_shrink = (val < 1) ? 1 : val;

  m_fromField->setText(QString::number(m_from));
  m_toField->setText(QString::number(m_to));
  m_stepField->setText(QString::number(m_step));
  m_shrinkField->setText(QString::number(m_shrink));
}

//-----------------------------------------------------------------------------

bool LoadImagesPopup::execute() { return doLoad(false); }

//-----------------------------------------------------------------------------
/*! Append images with apply button
*/
bool LoadImagesPopup::executeApply() { return doLoad(true); }

//-----------------------------------------------------------------------------

bool LoadImagesPopup::doLoad(bool append) {
  if (m_selectedPaths.empty()) return false;

  ::viewFile(*m_selectedPaths.begin(), m_from, m_to, m_step, m_shrink, 0,
             m_flip, append);

  // register recent files
  std::set<TFilePath>::const_iterator pt;
  for (pt = m_selectedPaths.begin(); pt != m_selectedPaths.end(); ++pt) {
    RecentFiles::instance()->addFilePath(
        toQString(
            TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(
                (*pt))),
        RecentFiles::Flip);
  }
  return true;
}

//-----------------------------------------------------------------------------

void LoadImagesPopup::onFilePathClicked(const TFilePath &fp) {
  TLevel::Iterator it;
  TLevelP level;
  TLevelReaderP lr;

  if (fp == TFilePath()) goto clear;

  lr = TLevelReaderP(fp);
  if (!lr) goto clear;

  level = lr->loadInfo();

  if (!level || level->getFrameCount() == 0) goto clear;

  it = level->begin();
  m_to, m_from = it->first.getNumber();

  for (; it != level->end(); ++it) m_to = it->first.getNumber();

  if (m_from == -2 && m_to == -2) m_from = m_to = 1;

  m_minFrame = m_from;
  m_maxFrame = m_to;
  m_fromField->setText(QString::number(m_from));
  m_toField->setText(QString::number(m_to));
  return;

clear:

  m_minFrame = 0;
  m_maxFrame = 10000000;
  m_fromField->setText("");
  m_toField->setText("");
}

//=============================================================================

SaveImagesPopup::SaveImagesPopup(FlipBook *flip)
    : FileBrowserPopup(tr("Save Flipbook Images")), m_flip(flip) {
  setOkText(tr("Save"));
}

bool SaveImagesPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  return m_flip->doSaveImages(*m_selectedPaths.begin());
}

//=============================================================================
void FlipBook::loadImages() {
  if (!m_loadPopup) {
    m_loadPopup = new LoadImagesPopup(this);  //, frameRangeFrame);
    // move the initial folder to the project root
    m_loadPopup->setFolder(
        TProjectManager::instance()->getCurrentProjectPath().getParentDir());
  }

  m_loadPopup->show();
  m_loadPopup->raise();
  m_loadPopup->activateWindow();
}

//=============================================================================

bool FlipBook::canAppend() {
  // Images can be appended if:
  //  a) There is a name (in particular, an extension) representing currently
  //  held ones.
  //  b) This flipbook is not holding a preview (inappropriate and problematic).
  //  c) The level has no palette. Otherwise, appended images may have a
  //  different palette.
  return !m_levels.empty() && !m_previewedFx && !m_palette;
}

//=============================================================================

void FlipBook::saveImages() {
  if (!m_savePopup) m_savePopup = new SaveImagesPopup(this);

  // initialize the default path every time
  TOutputProperties *op = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getProperties()
                              ->getOutputProperties();
  m_savePopup->setFolder(op->getPath().getParentDir());
  m_savePopup->setFilename(op->getPath().withFrame().withoutParentDir());

  m_savePopup->show();
  m_savePopup->raise();
  m_savePopup->activateWindow();
}

//=============================================================================

FlipBook::~FlipBook() {
  if (m_loadPopup) delete m_loadPopup;
  if (m_savePopup) delete m_savePopup;
}

//=============================================================================

bool FlipBook::doSaveImages(TFilePath fp) {
  QStringList formats;
  TLevelWriter::getSupportedFormats(formats, true);
  Tiio::Writer::getSupportedFormats(formats, true);

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TOutputProperties *outputSettings =
      scene->getProperties()->getOutputProperties();

  std::string ext = fp.getType();

  // Open a notice that the previewFx is rendered in 8bpc regardless of the
  // output settings.
  if (m_isPreviewFx && outputSettings->getRenderSettings().m_bpp == 64) {
    QString question =
        "Save previewed images :\nImages will be saved in 8 bit per channel "
        "with this command.\nDo you want to save images?";
    int ret =
        DVGui::MsgBox(question, QObject::tr("Save"), QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return false;
  }

#ifdef _WIN32
  if (ext == "avi") {
    TPropertyGroup *props = outputSettings->getFileFormatProperties(ext);
    std::string codecName = props->getProperty(0)->getValueAsString();
    TDimension res        = scene->getCurrentCamera()->getRes();
    if (!AviCodecRestrictions::canWriteMovie(::to_wstring(codecName), res)) {
      QString msg(
          QObject::tr("The resolution of the output camera does not fit with "
                      "the options chosen for the output file format."));
      DVGui::warning(msg);
      return false;
    }
  }
#endif

  if (ext == "") {
    ext = outputSettings->getPath().getType();
    fp  = fp.withType(ext);
  }
  if (fp.getName() == "") {
    DVGui::warning(
        tr("The file name cannot be empty or contain any of the following "
           "characters:(new line)  \\ / : * ? \"  |"));
    return false;
  }

  if (!formats.contains(QString::fromStdString(ext))) {
    DVGui::warning(
        tr("It is not possible to save because the selected file format is not "
           "supported."));
    return false;
  }

  int from, to, step;
  m_flipConsole->getFrameRange(from, to, step);

  if (m_currentFrameToSave != 0) {
    DVGui::info("Already saving!");
    return true;
  }

  if (TFileType::getInfo(fp) == TFileType::RASTER_IMAGE || ext == "pct" ||
      ext == "pic" || ext == "pict")  // pct e' un formato"livello" (ha i
                                      // settings di quicktime) ma fatto di
                                      // diversi frames
    fp = fp.withFrame(TFrameId::EMPTY_FRAME);

  fp          = scene->decodeFilePath(fp);
  bool exists = TFileStatus(fp.getParentDir()).doesExist();
  if (!exists) {
    try {
      TFilePath parent = fp.getParentDir();
      TSystem::mkDir(parent);
      DvDirModel::instance()->refreshFolder(parent.getParentDir());
    } catch (TException &e) {
      DVGui::error("Cannot create " + toQString(fp.getParentDir()) + " : " +
                   QString(::to_string(e.getMessage()).c_str()));
      return false;
    } catch (...) {
      DVGui::error("Cannot create " + toQString(fp.getParentDir()));
      return false;
    }
  }

  if (TSystem::doesExistFileOrLevel(fp)) {
    QString question(tr("File %1 already exists.\nDo you want to overwrite it?")
                         .arg(toQString(fp)));
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"));
    if (ret == 2) return false;
  }

  try {
    m_lw = TLevelWriterP(fp,
                         outputSettings->getFileFormatProperties(fp.getType()));
  } catch (...) {
    DVGui::error("It is not possible to save Flipbook content.");
    return false;
  }

  m_lw->setFrameRate(outputSettings->getFrameRate());

  m_currentFrameToSave = 1;

  ProgressBarMessager(eBegin, m_framesCount).sendBlocking();

  QTimer::singleShot(50, this, SLOT(saveImage()));
  return true;
}

//-----------------------------------------------------------------------------

void FlipBook::saveImage() {
  static int savedFrames = 0;

  assert(Pd);
  int from, to, step;

  m_flipConsole->getFrameRange(from, to, step);

  for (; m_currentFrameToSave <= m_framesCount; m_currentFrameToSave++) {
    ProgressBarMessager(eIncrement, m_currentFrameToSave).sendBlocking();
    if (!Pd) break;

    int actualFrame = from + (m_currentFrameToSave - 1) * step;
    TImageP img     = getCurrentImage(actualFrame);
    if (!img) continue;
    TImageWriterP writer = m_lw->getFrameWriter(TFrameId(actualFrame));
    bool failureOnSaving = false;
    if (!writer) continue;
    try {
      writer->save(img);
    } catch (...) {
      QString str(tr("It is not possible to save Flipbook content."));
      ProgressBarMessager(eEnd, 0, str).send();
      m_currentFrameToSave = 0;
      m_lw                 = TLevelWriterP();
      savedFrames          = 0;
      return;
    }
    savedFrames++;
    //		if (!m_pb->changeFraction(m_currentFrameToSave,
    // TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount()))
    //			break;
    m_currentFrameToSave++;

    QTimer::singleShot(50, this, SLOT(saveImage()));
    return;
  }

  QString str = tr("Saved %1 frames out of %2 in %3")
                    .arg(std::to_string(savedFrames).c_str())
                    .arg(std::to_string(m_framesCount).c_str())
                    .arg(::to_string(m_lw->getFilePath()).c_str());

  if (!Pd) str = "Canceled! " + str;

  ProgressBarMessager(eEnd, 0, str).send();

  m_currentFrameToSave = 0;
  m_lw                 = TLevelWriterP();
  savedFrames          = 0;
}

//=============================================================================

void FlipBook::onButtonPressed(FlipConsole::EGadget button) {
  switch (button) {
  case FlipConsole::eSound:
    m_playSound = !m_playSound;
    break;

  case FlipConsole::eHisto:
    m_imageViewer->showHistogram();
    break;

  case FlipConsole::eSaveImg: {
    TRect loadbox = m_loadbox;
    m_loadbox     = TRect();
    TImageP img   = getCurrentImage(m_flipConsole->getCurrentFrame());
    m_loadbox     = loadbox;
    if (!img) {
      DVGui::warning(tr("There are no rendered images to save."));
      return;
    } else if ((TVectorImageP)img) {
      DVGui::warning(
          tr("It is not possible to take or compare snapshots for Toonz vector "
             "levels."));
      return;
    }
    TRasterImageP ri(img);
    TToonzImageP ti(img);
    TImageP clonedImg;
    if (ri)
      clonedImg = TRasterImageP(ri->getRaster()->clone());
    else
      clonedImg = TToonzImageP(ti->getRaster()->clone(), ti->getSavebox());
    TImageCache::instance()->add(QString("TnzCompareImg"), clonedImg);
    break;
  }

  case FlipConsole::eCompare:
    if ((TVectorImageP)getCurrentImage(m_flipConsole->getCurrentFrame())) {
      DVGui::warning(
          tr("It is not possible to take or compare snapshots for Toonz vector "
             "levels."));
      m_flipConsole->setChecked(FlipConsole::eCompare, false);
      return;
    }
    break;

  case FlipConsole::eSave:
    saveImages();
    break;
  }
}

//=============================================================================
// FlipBookPool
//-----------------------------------------------------------------------------

/*! \class FlipBookPool
                \brief The FlipBookPool class is used to store used flipbook
   viewers.

    Flipbooks are generally intended as temporary but friendly floating widgets,
    that gets displayed when a rendered scene or image needs to be shown.
    Since a user may require that the geometry of a flipbook is to be remembered
    between rendering tasks - perhaps even between different Toonz sessions -
    flipbooks are always stored for later use in a \b FlipBookPool class.

    This class implements the basical features to \b pop a flipbook from the
   pool
    or \b push a used one; plus, it provides the \b save and \b load functions
    for persistent storage between Toonz sessions.

    \sa FlipBook class.
*/

FlipBookPool::FlipBookPool() : m_overallFlipCount(0) {
  qRegisterMetaType<ImagePainter::VisualSettings>(
      "ImagePainter::VisualSettings");
}

//-----------------------------------------------------------------------------

FlipBookPool::~FlipBookPool() {}

//-----------------------------------------------------------------------------

FlipBookPool *FlipBookPool::instance() {
  static FlipBookPool poolInstance;
  return &poolInstance;
}

//-----------------------------------------------------------------------------

void FlipBookPool::push(FlipBook *flipbook) {
  m_pool.insert(pair<int, FlipBook *>(flipbook->getPoolIndex(), flipbook));
}

//-----------------------------------------------------------------------------

//! Extracts the first unused flipbook from the flipbook pool.
//! If all known flipbooks are shown, allocates a new flipbook with the
//! first unused flipbook geometry in a geometry pool.
//! Again, if all recorded geometry are used by some existing flipbook, a
//! default geometry is used.
FlipBook *FlipBookPool::pop() {
  FlipBook *flipbook;
  TPanel *panel;

  TMainWindow *currentRoom = TApp::instance()->getCurrentRoom();

  if (m_pool.empty()) {
    panel = TPanelFactory::createPanel(currentRoom, "FlipBook");
    panel->setFloating(true);

    flipbook = static_cast<FlipBook *>(panel->widget());

    // Set geometry
    static int x = 0, y = 0;
    if (m_geometryPool.empty()) {
      panel->setGeometry(x += 50, y += 50, 400, 300);
      flipbook->setPoolIndex(m_overallFlipCount);
      m_overallFlipCount++;
    } else {
      flipbook->setPoolIndex(m_geometryPool.begin()->first);
      QRect geometry(m_geometryPool.begin()->second);
      panel->setGeometry(geometry);
      if ((geometry & QApplication::desktop()->availableGeometry(panel))
              .isEmpty())
        panel->move(x += 50, y += 50);
      m_geometryPool.erase(m_geometryPool.begin());
    }
  } else {
    flipbook = m_pool.begin()->second;
    panel    = (TPanel *)flipbook->parent();
    m_pool.erase(m_pool.begin());
  }

  // The panel need to be added to currentRoom's layout control.
  currentRoom->addDockWidget(panel);
  panel->raise();
  panel->show();

  return flipbook;
}

//-----------------------------------------------------------------------------

//! Saves the content of this flipbook pool.
void FlipBookPool::save() const {
  QSettings history(toQString(m_historyPath), QSettings::IniFormat);
  history.clear();

  history.setValue("count", m_overallFlipCount);

  history.beginGroup("flipbooks");

  std::map<int, FlipBook *>::const_iterator it;
  for (it = m_pool.begin(); it != m_pool.end(); ++it) {
    history.beginGroup(QString::number(it->first));
    TPanel *panel = static_cast<TPanel *>(it->second->parent());
    history.setValue("geometry", panel->geometry());
    history.endGroup();
  }

  std::map<int, QRect>::const_iterator jt;
  for (jt = m_geometryPool.begin(); jt != m_geometryPool.end(); ++jt) {
    history.beginGroup(QString::number(jt->first));
    history.setValue("geometry", jt->second);
    history.endGroup();
  }

  history.endGroup();
}

//-----------------------------------------------------------------------------

//! Loads the pool from input history
void FlipBookPool::load(const TFilePath &historyPath) {
  QSettings history(toQString(historyPath), QSettings::IniFormat);
  m_historyPath = historyPath;

  m_pool.clear();
  m_geometryPool.clear();

  m_overallFlipCount = history.value("count").toInt();

  history.beginGroup("flipbooks");

  QStringList flipBooks(history.childGroups());
  QStringList::iterator it;
  for (it = flipBooks.begin(); it != flipBooks.end(); ++it) {
    history.beginGroup(*it);

    // Retrieve flipbook geometry
    QVariant geom = history.value("geometry");

    // Insert geometry
    m_geometryPool.insert(pair<int, QRect>(it->toInt(), geom.toRect()));

    history.endGroup();
  }

  history.endGroup();
}

//=============================================================================

//! Returns the level frame number corresponding to passed flipbook index
TFrameId FlipBook::Level::flipbookIndexToLevelFrame(int index) {
  TLevel::Iterator it;
  int levelPos;
  if (m_incrementalIndexing) {
    levelPos = (index - 1) * m_step;
    it       = m_level->getTable()->find(m_fromIndex);
    advance(it, levelPos);
  } else {
    levelPos = m_fromIndex + (index - 1) * m_step;
    it       = m_level->getTable()->find(levelPos);
  }
  if (it == m_level->end()) return TFrameId();
  return it->first;
}

//-----------------------------------------------------------------------------

//! Returns the number of flipbook indexes available for this level
int FlipBook::Level::getIndexesCount() {
  return m_incrementalIndexing ? (m_level->getFrameCount() - 1) / m_step + 1
                               : (m_toIndex - m_fromIndex) / m_step + 1;
}

//=============================================================================

bool FlipBook::isSavable() const {
  if (m_levels.empty()) return false;

  for (int i = 0; i < m_levels.size(); i++)
    if (m_levels[i].m_fp != TFilePath() &&
        (m_levels[i].m_fp.getType() == "tlv" ||
         m_levels[i].m_fp.getType() == "pli"))
      return false;

  return true;
}

//=============================================================================

/*! Set the level contained in \b fp to FlipBook; if level exist show in image
                viewer its first frame, set current frame to 1.
                It's possible to change level palette, in fact if \b palette is
   different
                from 0 set level palette to \b palette.
*/
void FlipBook::setLevel(const TFilePath &fp, TPalette *palette, int from,
                        int to, int step, int shrink, TSoundTrack *snd,
                        bool append, bool isToonzOutput) {
  try {
    if (!append) {
      clearCache();
      m_levelNames.clear();
      m_levels.clear();
    }
    m_snd = 0;
    m_xl  = 0;

    m_flipConsole->enableProgressBar(false);
    m_flipConsole->setProgressBarStatus(0);
    m_flipConsole->enableButton(FlipConsole::eSound, snd != 0);
    m_flipConsole->enableButton(FlipConsole::eDefineLoadBox, true);
    m_flipConsole->enableButton(FlipConsole::eUseLoadBox, true);
    if (fp == TFilePath()) return;

    m_shrink = shrink;

    if (fp.getDots() == ".." && fp.getType() != "noext")
      m_levelNames.push_back(toQString(fp.withoutParentDir().withFrame()));
    else
      m_levelNames.push_back(toQString(fp.withoutParentDir()));

    m_snd = snd;

    if (TSystem::doesExistFileOrLevel(fp))  // is a  viewfile
    {
      // m_flipConsole->enableButton(FlipConsole::eCheckBg,
      // true);//fp.getType()!="pli");

      m_lr = TLevelReaderP(fp);

      bool supportsRandomAccess = doesSupportRandomAccess(fp, isToonzOutput);
      if (supportsRandomAccess) m_lr->enableRandomAccessRead(isToonzOutput);

      bool randomAccessRead    = supportsRandomAccess && isToonzOutput;
      bool incrementalIndexing = m_isPreviewFx ? true : false;

      TLevelP level = m_lr->loadInfo();

      if (!level || level->getFrameCount() == 0) {
        if (m_flags & eDontKeepFilesOpened) m_lr = TLevelReaderP();
        return;
      }

      // For the color model, get the reference fids from palette and delete
      // unneeded from the table
      if (m_imageViewer->isColorModel() && palette) {
        std::vector<TFrameId> fids = palette->getRefLevelFids();

        // when loading a single-frame, standard raster image into the
        // ColorModel, skip here.
        // If the fid == NO_FRAME(=-2), fids stores 0.
        if (!fids.empty() && !(fids.size() == 1 && fids[0].getNumber() == 0)) {
          // make the fid list to be deleted
          std::vector<TFrameId> deleteList;
          TLevel::Iterator it;
          for (it = level->begin(); it != level->end(); it++) {
            // If the fid is not included in the reference list, then delete it
            int i;
            for (i = 0; i < (int)fids.size(); i++) {
              if (fids[i].getNumber() == it->first.getNumber()) break;
            }
            if (i == fids.size()) {
              deleteList.push_back(it->first);
            }
          }
          // delete list items here
          if (!deleteList.empty())
            for (int i = 0; i < (int)deleteList.size(); i++)
              level->getTable()->erase(deleteList[i]);
        }
      }

      int fromIndex, toIndex;

      // in order to avoid that the current frame unexpectedly moves to 1 on the
      // Color Model once editing the style
      int current = -1;

      if (from == -1 && to == -1) {
        fromIndex = level->begin()->first.getNumber();
        toIndex   = (--level->end())->first.getNumber();
        if (m_imageViewer->isColorModel())
          current           = m_flipConsole->getCurrentFrame();
        incrementalIndexing = true;
      } else {
        TLevel::Iterator it = level->begin();

        // Adjust the frame interval to read. There is one special case:
        //  If the level read did not support random access, *AND* the level to
        //  show was just rendered,
        //  we have to assume that no level update happened, and the
        //  from-to-step infos are lost.
        //  So, shift the requested interval from 1 and place step to 1.
        fromIndex = from;
        toIndex   = to;
        if (isToonzOutput && !supportsRandomAccess) {
          fromIndex = 1;
          toIndex   = level->getFrameCount();
          step      = 1;
        }

        if (level->begin()->first.getNumber() != TFrameId::NO_FRAME) {
          fromIndex = std::max(fromIndex, level->begin()->first.getNumber());
          toIndex   = std::min(toIndex, (--level->end())->first.getNumber());
        } else {
          fromIndex           = level->begin()->first.getNumber();
          toIndex             = (--level->end())->first.getNumber();
          incrementalIndexing = true;
        }

        // Workaround to display simple background images when loading from
        // the right-click menu context
        fromIndex = std::min(fromIndex, toIndex);
      }

      Level levelToPush(level, fp, fromIndex, toIndex, step);
      levelToPush.m_randomAccessRead    = randomAccessRead;
      levelToPush.m_incrementalIndexing = incrementalIndexing;

      int formatIdx = Preferences::instance()->matchLevelFormat(fp);
      if (formatIdx >= 0 &&
          Preferences::instance()
              ->levelFormat(formatIdx)
              .m_options.m_premultiply) {
        levelToPush.m_premultiply = true;
      }

      m_levels.push_back(levelToPush);

      // Get the frames count to be shown in this flipbook level
      m_framesCount = levelToPush.getIndexesCount();

      assert(m_framesCount <= level->getFrameCount());

      // this value will be used in loadAndCacheAllTlvImages later
      int addingFrameAmount = m_framesCount;

      if (append && !m_levels.empty()) {
        int oldFrom, oldTo, oldStep;
        m_flipConsole->getFrameRange(oldFrom, oldTo, oldStep);
        assert(oldFrom == 1);
        assert(oldStep == 1);
        m_framesCount += oldTo;
      }

      m_flipConsole->setFrameRange(1, m_framesCount, 1, current);

      if (palette && level->getPalette() != palette) level->setPalette(palette);

      m_palette = level->getPalette();

      const TImageInfo *ii = m_lr->getImageInfo();

      if (ii) m_dim = TDimension(ii->m_lx / m_shrink, ii->m_ly / m_shrink);

      int levelFrameCount = 0;
      for (int lev = 0; lev < m_levels.size(); lev++)
        levelFrameCount += m_levels[lev].m_level->getFrameCount();

      if (levelFrameCount == 1)
        m_title = "  ::  1 Frame";
      else
        m_title = "  ::  " + QString::number(levelFrameCount) + " Frames";

      // color model does not concern about the pixel size
      if (ii && !m_imageViewer->isColorModel())
        m_title = m_title + "  ::  " + QString::number(ii->m_lx) + "x" +
                  QString::number(ii->m_ly) + " Pixels";

      if (shrink > 1)
        m_title = m_title + "  ::  " + "Shrink: " + QString::number(shrink);

      // when using the flip module, this signal is to show the loaded level
      // names in application's title bar
      QString arg = QString("Flip : %1").arg(m_levelNames[0]);
      emit imageLoaded(arg);

      // When viewing the tlv, try to cache all frames at the beginning.
      if (!m_imageViewer->isColorModel() && fp.getType() == "tlv" &&
          !(m_flags & eDontKeepFilesOpened) && !m_isPreviewFx) {
        loadAndCacheAllTlvImages(levelToPush,
                                 m_framesCount - addingFrameAmount + 1,  // from
                                 m_framesCount);                         // to
      }

      // An old archived bug says that simulatenous open for read of the same
      // tlv are not allowed...
      // if(m_lr && m_lr->getFilePath().getType()=="tlv")
      //  m_lr = TLevelReaderP();
    } else  // is a render
    {
      m_flipConsole->enableButton(FlipConsole::eCheckBg, true);
      m_previewedFx = 0;
      m_previewXsh  = 0;
      m_levels.clear();
      m_flipConsole->setFrameRange(from, to, step);

      m_framesCount = (to - from) / step + 1;
      m_title       = tr("Rendered Frames  ::  From %1 To %2  ::  Step %3")
                    .arg(QString::number(from))
                    .arg(QString::number(to))
                    .arg(QString::number(step));
      if (shrink > 1)
        m_title = m_title + tr("  ::  Shrink ") + QString::number(shrink);
    }

    // parentWidget()->setWindowTitle(m_title);
    m_imageViewer->setHistogramEnable(true);
    m_imageViewer->setHistogramTitle(m_levelNames[0]);
    m_flipConsole->enableButton(FlipConsole::eSave, isSavable());
    m_flipConsole->showCurrentFrame();
    if (m_flags & eDontKeepFilesOpened) m_lr = TLevelReaderP();
  } catch (...) {
    return;
  }
}

//-----------------------------------------------------------------------------

void FlipBook::setTitle(const QString &title) {
  m_viewerTitle = title;
  if (!m_previewedFx && !m_levelNames.empty())
    m_title = m_viewerTitle + "  ::  " + m_levelNames[0];
  else
    m_title = m_viewerTitle;
}

//-----------------------------------------------------------------------------

void FlipBook::setLevel(TXshSimpleLevel *xl) {
  try {
    clearCache();

    m_xl = xl;

    m_levelNames.push_back(QString::fromStdWString(xl->getName()));
    m_snd         = 0;
    m_previewedFx = 0;
    m_previewXsh  = 0;
    m_levels.clear();

    m_flipConsole->enableButton(FlipConsole::eSound, false);

    m_shrink = 1;
    int step = 1;

    m_framesCount = (m_xl->getFrameCount() - 1) / step + 1;
    m_flipConsole->setFrameRange(1, m_framesCount, step);
    m_flipConsole->enableProgressBar(false);
    m_flipConsole->setProgressBarStatus(0);
    m_palette = m_xl->getPalette();

    const LevelProperties *p = m_xl->getProperties();

    m_title = m_viewerTitle + "  ::  " + m_levelNames[0];

    if (m_framesCount == 1)
      m_title = m_title + "  ::  1 Frame";
    else
      m_title = m_title + "  ::  " + QString::number(m_framesCount) + " Frames";

    if (p) m_dim = p->getImageRes();

    if (p)
      m_title = m_title + "  ::  " + QString::number(p->getImageRes().lx) +
                "x" + QString::number(p->getImageRes().ly) + " Pixels";

    if (m_shrink > 1)
      m_title = m_title + "  ::  " + "Shrink: " + QString::number(m_shrink);

    m_imageViewer->setHistogramEnable(true);
    m_imageViewer->setHistogramTitle(m_levelNames[0]);

    m_flipConsole->showCurrentFrame();

  } catch (...) {
    return;
  }
}

//-----------------------------------------------------------------------------

void FlipBook::setLevel(TFx *previewedFx, TXsheet *xsh, TLevel *level,
                        TPalette *palette, int from, int to, int step,
                        int currentFrame, TSoundTrack *snd) {
  m_xl          = 0;
  m_previewedFx = previewedFx;
  m_previewXsh  = xsh;
  m_isPreviewFx = true;
  m_levels.clear();
  m_levels.push_back(Level(level, TFilePath(), from - 1, to - 1, step));
  m_levelNames.clear();
  m_levelNames.push_back(QString::fromStdString(level->getName()));
  m_title = m_viewerTitle;
  m_flipConsole->setFrameRange(from, to, step, currentFrame);
  m_flipConsole->enableProgressBar(true);

  m_flipConsole->enableButton(FlipConsole::eDefineLoadBox, false);
  m_flipConsole->enableButton(FlipConsole::eUseLoadBox, false);
  m_flipConsole->enableButton(FlipConsole::eSound, snd != 0);
  m_snd         = snd;
  m_framesCount = (to - from) / step + 1;

  m_imageViewer->setHistogramEnable(true);
  m_imageViewer->setHistogramTitle(m_levelNames[0]);
  m_flipConsole->showCurrentFrame();
}

//-----------------------------------------------------------------------------

TFx *FlipBook::getPreviewedFx() const {
  return m_isPreviewFx ? m_previewedFx.getPointer() : 0;
}

//-----------------------------------------------------------------------------

TXsheet *FlipBook::getPreviewXsheet() const {
  return m_isPreviewFx ? m_previewXsh.getPointer() : 0;
}

//-----------------------------------------------------------------------------

TRectD FlipBook::getPreviewedImageGeometry() const {
  if (!m_isPreviewFx) return TRectD();

  // Build viewer's geometry
  QRect viewerGeom(m_imageViewer->geometry());
  viewerGeom.adjust(-1, -1, 1, 1);
  TRectD viewerGeomD(viewerGeom.left(), viewerGeom.top(),
                     viewerGeom.right() + 1, viewerGeom.bottom() + 1);
  TPointD viewerCenter((viewerGeomD.x0 + viewerGeomD.x1) * 0.5,
                       (viewerGeomD.y0 + viewerGeomD.y1) * 0.5);

  // NOTE: The above adjust() is imposed to counter the geometry removal
  // specified in function
  // FlipBook::onDoubleClick.

  // Build viewer-to-camera affine
  TAffine viewToCam(m_imageViewer->getViewAff().inv() *
                    TTranslation(-viewerCenter));

  return viewToCam * viewerGeomD;
}

//-----------------------------------------------------------------------------

void FlipBook::schedulePreviewedFxUpdate() {
  if (m_previewedFx)
    m_previewUpdateTimer.start(
        1000);  // The effective fx update will happen in 1 msec.
}

//-----------------------------------------------------------------------------

void FlipBook::performFxUpdate() {
  // refresh only when the subcamera is active
  if (PreviewFxManager::instance()->isSubCameraActive(m_previewedFx))
    PreviewFxManager::instance()->refreshView(m_previewedFx);
}

//-----------------------------------------------------------------------------

void FlipBook::regenerate() {
  PreviewFxManager::instance()->reset(TFxP(m_previewedFx));
}

//-----------------------------------------------------------------------------

void FlipBook::regenerateFrame() {
  PreviewFxManager::instance()->reset(m_previewedFx, getCurrentFrame() - 1);
}

//-----------------------------------------------------------------------------

void FlipBook::clonePreview() {
  if (!m_previewedFx) return;

  FlipBook *newFlip =
      PreviewFxManager::instance()->showNewPreview(m_previewedFx, true);
  newFlip->m_imageViewer->setViewAff(m_imageViewer->getViewAff());
  PreviewFxManager::instance()->refreshView(m_previewedFx);
}

//-----------------------------------------------------------------------------

void FlipBook::freezePreview() {
  if (!m_previewedFx) return;

  PreviewFxManager::instance()->freeze(this);

  m_freezed = true;

  // sync the button state when triggered by shotcut
  if (m_freezeButton) m_freezeButton->setPressed(true);
}

//-----------------------------------------------------------------------------

void FlipBook::unfreezePreview() {
  if (!m_previewedFx) return;

  PreviewFxManager::instance()->unfreeze(this);

  m_freezed = false;

  // sync the button state when triggered by shotcut
  if (m_freezeButton) m_freezeButton->setPressed(false);
}

//-----------------------------------------------------------------------------

void FlipBook::setProgressBarStatus(const std::vector<UCHAR> *pbStatus) {
  m_flipConsole->setProgressBarStatus(pbStatus);
}

//-----------------------------------------------------------------------------

const std::vector<UCHAR> *FlipBook::getProgressBarStatus() const {
  return m_flipConsole->getProgressBarStatus();
}

//-----------------------------------------------------------------------------

void FlipBook::showFrame(int frame) {
  if (frame < 0) return;
  m_flipConsole->setCurrentFrame(frame);
  m_flipConsole->showCurrentFrame();
}

//-----------------------------------------------------------------------------

void FlipBook::playAudioFrame(int frame) {
  static bool first = true;
  static bool audioCardInstalled;
  if (!m_snd || !m_playSound) return;

  if (first) {
    audioCardInstalled = TSoundOutputDevice::installed();
    first              = false;
  }

  if (!audioCardInstalled) return;

  if (!m_player) {
    m_player = new TSoundOutputDevice();
    m_player->attach(this);
  }
  if (m_player) {
    // Flipbook does not currently support double fps - thus, casting to int in
    // soundtrack playback, too
    int fps = TApp::instance()
                  ->getCurrentScene()
                  ->getScene()
                  ->getProperties()
                  ->getOutputProperties()
                  ->getFrameRate();

    int samplePerFrame = int(m_snd->getSampleRate()) / fps;
    TINT32 firstSample = (frame - 1) * samplePerFrame;
    TINT32 lastSample  = firstSample + samplePerFrame;

    try {
      m_player->play(m_snd, firstSample, lastSample, false, false);
    } catch (TSoundDeviceException &e) {
      std::string msg;
      if (e.getType() == TSoundDeviceException::UnsupportedFormat) {
        try {
          TSoundTrackFormat fmt =
              m_player->getPreferredFormat(m_snd->getFormat());
          m_player->play(TSop::convert(m_snd, fmt), firstSample, lastSample,
                         false, false);
        } catch (TSoundDeviceException &ex) {
          throw TException(ex.getMessage());
          return;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

TImageP FlipBook::getCurrentImage(int frame) {
  std::string id = "";
  TFrameId fid;
  TFilePath fp;

  bool randomAccessRead    = false;
  bool incrementalIndexing = false;
  bool premultiply         = false;
  if (m_xl)  // is an xsheet level
  {
    if (m_xl->getFrameCount() <= 0) return 0;
    return m_xl->getFrame(m_xl->index2fid(frame - 1), false);
  } else if (!m_levels.empty())  // is a viewfile or a previewFx
  {
    TLevelP level;
    QString levelName;
    int from, to, step;
    m_flipConsole->getFrameRange(from, to, step);

    int frameIndex = m_previewedFx ? ((frame - from) / step) + 1 : frame;

    int i = 0;
    // Search all subsequent levels on the flipbook and retrieve the one
    // containing the required frame
    for (i = 0; i < m_levels.size(); i++) {
      int frameIndexesCount = m_levels[i].getIndexesCount();
      if (frameIndex > 0 && frameIndex <= frameIndexesCount) break;
      frameIndex -= frameIndexesCount;
    }

    if (i == m_levels.size() || frame < 0) return 0;

    frame--;

    // Now, get the right frame from the level

    level               = m_levels[i].m_level;
    fp                  = m_levels[i].m_fp;  // fp=empty when previewing fx
    randomAccessRead    = m_levels[i].m_randomAccessRead;
    incrementalIndexing = m_levels[i].m_incrementalIndexing;
    levelName           = m_levelNames[i];
    fid                 = m_levels[i].flipbookIndexToLevelFrame(frameIndex);
    premultiply         = m_levels[i].m_premultiply;
    if (fid == TFrameId()) return 0;
    id = levelName.toStdString() + fid.expand(TFrameId::NO_PAD) +
         ((m_isPreviewFx) ? "" : ::to_string(this));

    if (!m_isPreviewFx)
      m_title1 = m_viewerTitle + " :: " + fp.withoutParentDir().withFrame(fid);
    else
      m_title1 = "";
  } else if (m_levelNames.empty())
    return 0;
  else  // is a render
    id = m_levelNames[0].toStdString() + std::to_string(frame);

  bool showSub = m_flipConsole->isChecked(FlipConsole::eUseLoadBox);

  if (TImageCache::instance()->isCached(id)) {
    TRect loadbox;
    std::map<std::string, TRect>::const_iterator it = m_loadboxes.find(id);
    if (it != m_loadboxes.end()) loadbox = it->second;

    // Resubmit the image to the cache as the 'last one' seen by the flipbook.
    // TImageCache::instance()->add(toString(m_poolIndex) + "lastFlipFrame",
    // img);
    // m_lastViewedFrame = frame+1;
    if ((showSub && m_loadbox == loadbox) || (!showSub && loadbox == TRect()))
      return TImageCache::instance()->get(id, false);
    else
      TImageCache::instance()->remove(id);
  }
  if (fp != TFilePath() && !m_isPreviewFx) {
    int lx = 0, oriLx = 0;
    // TLevelReaderP lr(fp);
    if (!m_lr || (fp != m_lr->getFilePath())) {
      m_lr = TLevelReaderP(fp);
      m_lr->enableRandomAccessRead(randomAccessRead);
    }
    if (!m_lr) return 0;
    // try to get image info only when loading tlv or pli as it is quite time
    // consuming
    if (fp.getType() == "tlv" || fp.getType() == "pli") {
      if (m_lr->getImageInfo()) lx = oriLx = m_lr->getImageInfo()->m_lx;
    }
    TImageReaderP ir = m_lr->getFrameReader(fid);
    ir->setShrink(m_shrink);
    if (m_loadbox != TRect() && showSub) {
      ir->setRegion(m_loadbox);
      lx = m_loadbox.getLx();
    }

    TImageP img = ir->load();

    if (img) {
      TRasterImageP ri = ((TRasterImageP)img);
      TToonzImageP ti  = ((TToonzImageP)img);
      if (premultiply) {
        if (ri)
          TRop::premultiply(ri->getRaster());
        else if (ti)
          TRop::premultiply(ti->getRaster());
      }

      // se e' stata caricata una sottoimmagine alcuni formati in realta'
      // caricano tutto il raster e fanno extract, non si ha quindi alcun
      // risparmio di occupazione di memoria; alloco un raster grande
      // giusto copio la region e butto quello originale.
      if (ri && showSub && m_loadbox != TRect() &&
          ri->getRaster()->getLx() == oriLx)  // questo serve perche' per avi e
                                              // mov la setRegion e'
                                              // completamente ignorata...
        ri->setRaster(ri->getRaster()->extract(m_loadbox)->clone());
      else if (ri && ri->getRaster()->getWrap() > ri->getRaster()->getLx())
        ri->setRaster(ri->getRaster()->clone());
      else if (ti && ti->getCMapped()->getWrap() > ti->getCMapped()->getLx())
        ti->setCMapped(ti->getCMapped()->clone());

      if ((fp.getType() == "tlv" || fp.getType() == "pli") && m_shrink > 1 &&
          (lx == 0 || (ri && ri->getRaster()->getLx() == lx) ||
           (ti && ti->getRaster()->getLx() == lx))) {
        if (ri)
          ri->setRaster(TRop::shrink(ri->getRaster(), m_shrink));
        else if (ti)
          ti->setCMapped(TRop::shrink(ti->getRaster(), m_shrink));
      }

      TPalette *palette = img->getPalette();
      if (m_palette && (!palette || palette != m_palette))
        img->setPalette(m_palette);
      TImageCache::instance()->add(id, img);
      m_loadboxes[id] = showSub ? m_loadbox : TRect();
    }

    // An old archived bug says that simulatenous open for read of the same tlv
    // are not allowed...
    // if(fp.getType()=="tlv")
    //  m_lr = TLevelReaderP();
    if (m_flags & eDontKeepFilesOpened) m_lr = TLevelReaderP();
    return img;
  } else if (fp == TFilePath() && m_isPreviewFx) {
    if (!TImageCache::instance()->isCached(id)) {
      /*string lastFrameCacheId(toString(m_poolIndex) + "lastFlipFrame");
if(TImageCache::instance()->isCached(lastFrameCacheId))
return TImageCache::instance()->get(lastFrameCacheId, false);
else*/
      return 0;
      // showFrame(m_lastViewedFrame);
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

/*! Set current level frame to image viewer. Add the view image in cache.
*/
void FlipBook::onDrawFrame(int frame, const ImagePainter::VisualSettings &vs) {
  try {
    m_imageViewer->setVisual(vs);

    TImageP img = getCurrentImage(frame);

    if (!img) return;

    m_imageViewer->setImage(img);
  } catch (...) {
    m_imageViewer->setImage(TImageP());
  }

  if (m_playSound && !vs.m_drawBlankFrame) playAudioFrame(frame);
}

//-----------------------------------------------------------------------------

void FlipBook::swapBuffers() { m_imageViewer->doSwapBuffers(); }

//-----------------------------------------------------------------------------

void FlipBook::changeSwapBehavior(bool enable) {
  m_imageViewer->changeSwapBehavior(enable);
}

//-----------------------------------------------------------------------------

void FlipBook::setLoadbox(const TRect &box) {
  m_loadbox =
      (m_dim.lx > 0) ? box * TRect(0, 0, m_dim.lx - 1, m_dim.ly - 1) : box;
}

//----------------------------------------------------------------

void FlipBook::clearCache() {
  TLevel::Iterator it;

  if (m_levelNames.empty()) return;
  int i;

  if (!m_levels.empty())  // is a viewfile
    for (i = 0; i < m_levels.size(); i++)
      for (it = m_levels[i].m_level->begin(); it != m_levels[i].m_level->end();
           ++it)
        TImageCache::instance()->remove(
            m_levelNames[i].toStdString() +
            std::to_string(it->first.getNumber()) +
            ((m_isPreviewFx) ? "" : ::to_string(this)));
  else {
    int from, to, step;
    m_flipConsole->getFrameRange(from, to, step);
    for (int i = from; i <= to; i += step)  // is a render
      // color model may loading a part of frames in the level
      if (m_imageViewer->isColorModel() && m_palette) {
        // get the actually-loaded frame list
        std::vector<TFrameId> fids(m_palette->getRefLevelFids());
        if (!fids.empty() && (int)fids.size() >= i) {
          int frame = fids[i - 1].getNumber();
          TImageCache::instance()->remove(m_levelNames[0].toStdString() +
                                          std::to_string(frame));
        } else {
          TImageCache::instance()->remove(m_levelNames[0].toStdString() +
                                          std::to_string(i));
        }
      } else
        TImageCache::instance()->remove(m_levelNames[0].toStdString() +
                                        std::to_string(i));
  }
}

//-----------------------------------------------------------------------------

void FlipBook::onCloseButtonPressed() {
  m_flipConsole->setActive(false);
  closeFlipBook(this);

  reset();

  // hide freeze button in preview fx window
  if (m_freezeButton) {
    m_freezeButton->hide();
    m_imageViewer->setIsRemakingPreviewFx(false);
  }

  // Return the flipbook to the pool in case it was popped from it.
  if (m_poolIndex >= 0) FlipBookPool::instance()->push(this);
}

//-----------------------------------------------------------------------------

void ImageViewer::showHistogram() {
  if (!m_isHistogramEnable) return;
  if (m_histogramPopup->isVisible())
    m_histogramPopup->raise();
  else {
    m_histogramPopup->setImage(getImage());
    m_histogramPopup->show();
  }
}

//-----------------------------------------------------------------------------

void FlipBook::dragEnterEvent(QDragEnterEvent *e) {
  const QMimeData *mimeData = e->mimeData();
  if (!acceptResourceDrop(mimeData->urls()) &&
      !mimeData->hasFormat("application/vnd.toonz.drawings") &&
      !mimeData->hasFormat(CastItems::getMimeFormat()))
    return;

  foreach (QUrl url, mimeData->urls()) {
    TFilePath fp(url.toLocalFile().toStdWString());
    std::string type = fp.getType();
    if (type == "tzp" || type == "tzu" || type == "tnz" || type == "scr" ||
        type == "mesh")
      return;
  }
  if (mimeData->hasFormat(CastItems::getMimeFormat())) {
    const CastItems *items = dynamic_cast<const CastItems *>(mimeData);
    if (!items) return;

    int i;
    for (i = 0; i < items->getItemCount(); i++) {
      CastItem *item      = items->getItem(i);
      TXshSimpleLevel *sl = item->getSimpleLevel();
      if (!sl) return;
    }
  }

  e->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void FlipBook::dropEvent(QDropEvent *e) {
  const QMimeData *mimeData = e->mimeData();
  if (mimeData->hasUrls()) {
    foreach (QUrl url, mimeData->urls()) {
      TFilePath fp(url.toLocalFile().toStdWString());
      if (TFileType::getInfo(fp) != TFileType::UNKNOW_FILE) setLevel(fp);
      e->acceptProposedAction();
      return;
    }
  } else if (mimeData->hasFormat(
                 "application/vnd.toonz.drawings"))  // drag-drop from film
                                                     // strip
  {
    TFilmstripSelection *s =
        dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!s || !sl) return;
    TXshSimpleLevel *newSl = new TXshSimpleLevel();
    newSl->setScene(sl->getScene());
    newSl->setType(sl->getType());
    newSl->setPalette(sl->getPalette());
    newSl->clonePropertiesFrom(sl);
    const std::set<TFrameId> &fids = s->getSelectedFids();
    std::set<TFrameId>::const_iterator it;
    for (it = fids.begin(); it != fids.end(); ++it)
      newSl->setFrame(*it, sl->getFrame(*it, false)->cloneImage());
    setLevel(newSl);
  } else if (mimeData->hasFormat(
                 CastItems::getMimeFormat()))  // Drag-Drop from castviewer
  {
    const CastItems *items = dynamic_cast<const CastItems *>(mimeData);
    if (!items) return;

    int i;
    for (i = 0; i < items->getItemCount(); i++) {
      CastItem *item = items->getItem(i);
      if (TXshSimpleLevel *sl = item->getSimpleLevel()) setLevel(sl);
    }
  }
  m_flipConsole->makeCurrent();
}

//-------------------------------------------------------------------

void FlipBook::reset() {
  if (!m_isPreviewFx)  // The cache is owned by the PreviewFxManager otherwise
    clearCache();
  else
    PreviewFxManager::instance()->detach(this);

  m_levelNames.clear();
  m_levels.clear();
  m_framesCount = 0;
  m_palette     = 0;
  m_imageViewer->setImage(TImageP());
  m_imageViewer->hideHistogram();
  m_isPreviewFx = false;
  m_previewedFx = 0;
  m_previewXsh  = 0;
  m_freezed     = false;
  // sync the freeze button
  if (m_freezeButton) m_freezeButton->setPressed(false);
  m_flipConsole->pressButton(FlipConsole::ePause);
  if (m_playSound) m_flipConsole->pressButton(FlipConsole::eSound);
  if (m_player) m_player->stop();
  if (m_flipConsole->isChecked(FlipConsole::eDefineLoadBox))
    m_flipConsole->pressButton(FlipConsole::eDefineLoadBox);
  if (m_flipConsole->isChecked(FlipConsole::eUseLoadBox))
    m_flipConsole->pressButton(FlipConsole::eUseLoadBox);

  m_flipConsole->enableButton(FlipConsole::eDefineLoadBox, true);
  m_flipConsole->enableButton(FlipConsole::eUseLoadBox, true);

  m_lr = TLevelReaderP();

  m_dim     = TDimension();
  m_loadbox = TRect();
  m_loadboxes.clear();
  // m_lastViewedFrame = -1;
  // TImageCache::instance()->remove(toString(m_poolIndex) + "lastFlipFrame");

  m_flipConsole->enableProgressBar(false);
  m_flipConsole->setProgressBarStatus(0);
  m_flipConsole->setFrameRange(1, 1, 1);

  setTitle(tr("Flipbook"));
  parentWidget()->setWindowTitle(m_title);

  update();
}

//-------------------------------------------------------------------

void FlipBook::showEvent(QShowEvent *e) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneChanged()), m_imageViewer, SLOT(update()));
  // for updating the blank frame button
  if (!m_imageViewer->isColorModel()) {
    connect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
            m_flipConsole, SLOT(onPreferenceChanged(const QString &)));
    m_flipConsole->onPreferenceChanged("");
  }
  m_flipConsole->setActive(true);
  m_imageViewer->update();
}

//-------------------------------------------------------------------

void FlipBook::hideEvent(QHideEvent *e) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneChanged()), m_imageViewer,
             SLOT(update()));
  if (!m_imageViewer->isColorModel()) {
    disconnect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
               m_flipConsole, SLOT(onPreferenceChanged(const QString &)));
  }
  m_flipConsole->setActive(false);
}

//-----------------------------------------------------------------------------

void FlipBook::resizeEvent(QResizeEvent *e) { schedulePreviewedFxUpdate(); }

//-----------------------------------------------------------------------------

void FlipBook::adaptGeometry(const TRect &interestingImgRect,
                             const TRect &imgRect) {
  TRectD imgRectD(imgRect.x0, imgRect.y0, imgRect.x1 + 1, imgRect.y1 + 1);
  TRectD interestingImgRectD(interestingImgRect.x0, interestingImgRect.y0,
                             interestingImgRect.x1 + 1,
                             interestingImgRect.y1 + 1);

  TAffine toWidgetRef(m_imageViewer->getImgToWidgetAffine(imgRectD));
  TRectD interestGeomD(toWidgetRef * interestingImgRectD);
  TRectD imageGeomD(toWidgetRef * imgRectD);
  adaptWidGeometry(
      TRect(tceil(interestGeomD.x0), tceil(interestGeomD.y0),
            tfloor(interestGeomD.x1) - 1, tfloor(interestGeomD.y1) - 1),
      TRect(tceil(imageGeomD.x0), tceil(imageGeomD.y0),
            tfloor(imageGeomD.x1) - 1, tfloor(imageGeomD.y1) - 1),
      true);
}

//-----------------------------------------------------------------------------
/*! When Fx preview is called without the subcamera, render the full region
    of camera by resize flipbook and zoom-out the rendered image.
*/
void FlipBook::adaptGeometryForFullPreview(const TRect &imgRect) {
  TRectD imgRectD(imgRect.x0, imgRect.y0, imgRect.x1 + 1, imgRect.y1 + 1);

  // Get screen geometry
  TPanel *panel = static_cast<TPanel *>(parentWidget());
  if (!panel->isFloating()) return;
  QDesktopWidget *desk =
      static_cast<QApplication *>(QApplication::instance())->desktop();
  QRect screenGeom = desk->availableGeometry(panel);

  while (1) {
    TAffine toWidgetRef(m_imageViewer->getImgToWidgetAffine(imgRectD));
    TRectD imageGeomD(toWidgetRef * imgRectD);
    TRect imageGeom(tceil(imageGeomD.x0) - 1, tceil(imageGeomD.y0) - 1,
                    tfloor(imageGeomD.x1) + 1, tfloor(imageGeomD.y1) + 1);

    if (imageGeom.getLx() <= screenGeom.width() &&
        imageGeom.getLy() <= screenGeom.height()) {
      adaptWidGeometry(imageGeom, imageGeom, false);
      break;
    } else
      m_imageViewer->zoomQt(false, false);
  }
}

//-----------------------------------------------------------------------------

//! Adapts panel geometry to that of passed rect.
void FlipBook::adaptWidGeometry(const TRect &interestWidGeom,
                                const TRect &imgWidGeom, bool keepPosition) {
  TPanel *panel = static_cast<TPanel *>(parentWidget());
  if (!panel->isFloating()) return;

  // Extract image position in screen coordinates
  QRect qgeom(interestWidGeom.x0, interestWidGeom.y0, interestWidGeom.getLx(),
              interestWidGeom.getLy());
  QRect interestGeom(m_imageViewer->mapToGlobal(qgeom.topLeft()),
                     m_imageViewer->mapToGlobal(qgeom.bottomRight()));
  qgeom = QRect(imgWidGeom.x0, imgWidGeom.y0, imgWidGeom.getLx(),
                imgWidGeom.getLy());
  QRect imageGeom(m_imageViewer->mapToGlobal(qgeom.topLeft()),
                  m_imageViewer->mapToGlobal(qgeom.bottomRight()));

  // qDebug("tgeom= [%d, %d] x [%d, %d]", tgeom.x0, tgeom.x1, tgeom.y0,
  // tgeom.y1);
  // qDebug("imagegeom= [%d, %d] x [%d, %d]", imageGeom.left(),
  // imageGeom.right(),
  //  imageGeom.top(), imageGeom.bottom());

  // Get screen geometry
  QDesktopWidget *desk =
      static_cast<QApplication *>(QApplication::instance())->desktop();
  QRect screenGeom = desk->availableGeometry(panel);

  // Get panel margin measures
  QRect margins;
  QRect currView(m_imageViewer->geometry());
  currView.moveTo(m_imageViewer->mapToGlobal(currView.topLeft()));
  QRect panelGeom(panel->geometry());

  margins.setLeft(panelGeom.left() - currView.left());
  margins.setRight(panelGeom.right() - currView.right());
  margins.setTop(panelGeom.top() - currView.top());
  margins.setBottom(panelGeom.bottom() - currView.bottom());

  // Build the minimum flipbook geometry. Adjust the interesting geometry
  // according to it.
  QSize flipMinimumSize(panel->minimumSize());
  flipMinimumSize -=
      QSize(margins.right() - margins.left(), margins.bottom() - margins.top());
  QSize minAddition(
      tceil(std::max(0, flipMinimumSize.width() - interestGeom.width()) * 0.5),
      tceil(std::max(0, flipMinimumSize.height() - interestGeom.height()) *
            0.5));
  interestGeom.adjust(-minAddition.width(), -minAddition.height(),
                      minAddition.width(), minAddition.height());

  // Translate to keep the current view top-left corner, if required
  if (keepPosition) {
    QPoint shift(currView.topLeft() - interestGeom.topLeft());
    interestGeom.translate(shift);
    imageGeom.translate(shift);
  }

  // Intersect with the screen geometry
  QRect newViewerGeom(screenGeom);
  newViewerGeom.adjust(-margins.left(), -margins.top(), -margins.right(),
                       -margins.bottom());

  // when fx previewing in full size (i.e. keepPosition is false ),
  // try to translate geometry and keep the image inside the viewer as much as
  // posiible
  if (keepPosition)
    newViewerGeom &= interestGeom;
  else if (newViewerGeom.intersects(interestGeom)) {
    int d_ns = 0;
    int d_ew = 0;
    if (interestGeom.top() < newViewerGeom.top())
      d_ns = newViewerGeom.top() - interestGeom.top();
    else if (interestGeom.bottom() > newViewerGeom.bottom())
      d_ns = newViewerGeom.bottom() - interestGeom.bottom();
    if (interestGeom.left() < newViewerGeom.left())
      d_ew = newViewerGeom.left() - interestGeom.left();
    else if (interestGeom.right() > newViewerGeom.right())
      d_ew = newViewerGeom.right() - interestGeom.right();
    if (d_ns || d_ew) {
      interestGeom.translate(d_ew, d_ns);
      imageGeom.translate(d_ew, d_ns);
    }
    newViewerGeom &= interestGeom;
  }

  // qDebug("new Viewer= [%d, %d] x [%d, %d]", newViewerGeom.left(),
  // newViewerGeom.right(),
  //  newViewerGeom.top(), newViewerGeom.bottom());

  // Calculate the pan of content image in order to compensate for our geometry
  // change
  QPointF imageGeomCenter((imageGeom.left() + imageGeom.right() + 1) * 0.5,
                          (imageGeom.top() + imageGeom.bottom() + 1) * 0.5);
  QPointF newViewerGeomCenter(
      (newViewerGeom.left() + newViewerGeom.right() + 1) * 0.5,
      (newViewerGeom.top() + newViewerGeom.bottom() + 1) * 0.5);

  /*QPointF imageGeomCenter(
(imageGeom.width()) * 0.5,
(imageGeom.height()) * 0.5
);
QPointF newViewerGeomCenter(
(newViewerGeom.width()) * 0.5,
(newViewerGeom.height()) * 0.5
);*/

  // NOTE: If delta == (0,0) the image is at center. Typically happens when
  // imageGeom doesn't intersect
  // the screen geometry.
  QPointF delta(imageGeomCenter - newViewerGeomCenter);
  TAffine aff(m_imageViewer->getViewAff());
  aff.a13 = delta.x();
  aff.a23 = -delta.y();

  // Calculate new panel geometry
  newViewerGeom.adjust(margins.left(), margins.top(), margins.right(),
                       margins.bottom());

  // Apply changes
  m_imageViewer->setViewAff(aff);
  panel->setGeometry(newViewerGeom);
}

//-----------------------------------------------------------------------------

void FlipBook::onDoubleClick(QMouseEvent *me) {
  TImageP img(m_imageViewer->getImage());
  if (!img) return;

  TAffine toWidgetRef(m_imageViewer->getImgToWidgetAffine());
  TRectD pixGeomD(TScale(1.0 / (double)getDevPixRatio()) * toWidgetRef *
                  getImageBoundsD(img));
  // TRectD pixGeomD(toWidgetRef  * getImageBoundsD(img));
  TRect pixGeom(tceil(pixGeomD.x0), tceil(pixGeomD.y0), tfloor(pixGeomD.x1) - 1,
                tfloor(pixGeomD.y1) - 1);

  // NOTE: The previous line has ceils and floor inverted on purpose. The reason
  // is the following:
  // As the viewer's zoom level is arbitrary, the image is likely to have a not
  // integer geometry
  // with respect to the widget - the problem is, we cannot take the closest
  // integer rect ENCLOSING ours,
  // or the ImageViewer class adds blank lines on image rendering.
  // So, we do the converse - take the closest ENCLOSED one - eventually to be
  // compensated when
  // performing the inverse.

  adaptWidGeometry(pixGeom, pixGeom, false);
}

//-----------------------------------------------------------------------------

void FlipBook::minimize(bool doMinimize) {
  m_imageViewer->setVisible(!doMinimize);
  m_flipConsole->showHideAllParts(!doMinimize);
}

//-----------------------------------------------------------------------------
/*! When viewing the tlv, try to cache all frames at the beginning.
        NOTE : fromFrame and toFrame are frame numbers displayed on the flipbook
*/
void FlipBook::loadAndCacheAllTlvImages(Level level, int fromFrame,
                                        int toFrame) {
  TFilePath fp                                   = level.m_fp;
  if (!m_lr || (fp != m_lr->getFilePath())) m_lr = TLevelReaderP(fp);
  if (!m_lr) return;

  // show the wait cursor when loading a level with more than 50 frames
  if (toFrame - fromFrame > 50) QApplication::setOverrideCursor(Qt::WaitCursor);

  int lx = 0, oriLx = 0;
  if (m_lr->getImageInfo()) lx = oriLx = m_lr->getImageInfo()->m_lx;

  std::string fileName = toQString(fp.withoutParentDir()).toStdString();

  for (int f = fromFrame; f <= toFrame; f++) {
    TFrameId fid = level.flipbookIndexToLevelFrame(f);
    if (fid == TFrameId()) continue;

    std::string id =
        fileName + fid.expand(TFrameId::NO_PAD) + ::to_string(this);

    TImageReaderP ir = m_lr->getFrameReader(fid);
    ir->setShrink(m_shrink);

    TImageP img = ir->load();

    if (!img) continue;

    TToonzImageP ti = ((TToonzImageP)img);
    if (!ti) continue;

    if (ti->getCMapped()->getWrap() > ti->getCMapped()->getLx())
      ti->setCMapped(ti->getCMapped()->clone());
    if (m_shrink > 1 && (lx == 0 || ti->getRaster()->getLx() == lx))
      ti->setCMapped(TRop::shrink(ti->getRaster(), m_shrink));

    TPalette *palette = img->getPalette();

    if (m_palette && (!palette || palette != m_palette))
      img->setPalette(m_palette);

    TImageCache::instance()->add(id, img);
    m_loadboxes[id] = TRect();
  }

  m_lr = TLevelReaderP();

  // revert the cursor
  if (toFrame - fromFrame > 50) QApplication::restoreOverrideCursor();
}

//=============================================================================
// Utility

//-----------------------------------------------------------------------------

//! Displays the passed file on a Flipbook, supporting a wide range of options.
//! Possible options include:
//! \li The range, step and shrink parameters for the loaded level
//! \li A soundtrack to accompany the level's images
//! \li The flipbook where the file is to be opened. If none, a new one is
//! created.
//! \li Whether the level must replace an existing one on the flipbook, or it
//! must
//! rather be appended at its end
//! \li In case the file has a movie format and it is known to be a toonz
//! output,
//! some additional random access information may be retrieved (i.e. images may
//! map
//! to specific frames).
// returns pointer to the opened flipbook to control modality.
FlipBook *viewFile(const TFilePath &path, int from, int to, int step,
                   int shrink, TSoundTrack *snd, FlipBook *flipbook,
                   bool append, bool isToonzOutput) {
  // In case the step and shrink informations are invalid, load them from
  // preferences
  if (step == -1 || shrink == -1) {
    int _step = 1, _shrink = 1;
    Preferences::instance()->getViewValues(_shrink, _step);
    if (step == -1) step     = _step;
    if (shrink == -1) shrink = _shrink;
  }

  // Movie files must not have the ".." extension
  if ((path.getType() == "mov" || path.getType() == "avi" ||
       path.getType() == "3gp") &&
      path.isLevelName()) {
    DVGui::warning(QObject::tr("%1  has an invalid extension format.")
                       .arg(QString::fromStdString(path.getLevelName())));
    return NULL;
  }

  // Windows Screen Saver - avoid
  if (path.getType() == "scr") return NULL;

  // Avi and movs may be viewed by an external viewer, depending on preferences
  if (path.getType() == "mov" || path.getType() == "avi" && !flipbook) {
    QString str;
    QSettings().value("generatedMovieViewEnabled", str);
    if (str.toInt() != 0) {
      TSystem::showDocument(path);
      return NULL;
    }
  }

  // Retrieve a blank flipbook
  if (!flipbook)
    flipbook = FlipBookPool::instance()->pop();
  else if (!append)
    flipbook->reset();

  // Assign the passed level with associated infos
  flipbook->setLevel(path, 0, from, to, step, shrink, snd, append,
                     isToonzOutput);
  return flipbook;
}

//-----------------------------------------------------------------------------
