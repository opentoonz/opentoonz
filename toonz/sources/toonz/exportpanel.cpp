

#include "exportpanel.h"

#include <QtGlobal>

// Tnz6 includes
#include "tapp.h"
#include "moviegenerator.h"
#include "menubarcommandids.h"
#include "fileselection.h"
#include "iocommand.h"
#include "filebrowser.h"
#include "flipbook.h"
#include "formatsettingspopups.h"
#include "fileviewerpopup.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/sceneproperties.h"
#include "toonz/onionskinmask.h"
#include "toonz/preferences.h"
#include "toutputproperties.h"
#include "toonz/txshleveltypes.h"

// TnzBase includes
#include "trasterfx.h"
#include "tenv.h"

// TnzCore includes
#include "timage_io.h"
#include "tfiletype.h"
#include "tstream.h"
#include "tconvert.h"
#include "tsystem.h"
#include "tiio.h"

// Qt includes
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QUrl>
#include <QProgressDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QLayout>
#include <QComboBox>
#include <QBoxLayout>
#include <QApplication>
#include <QBuffer>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QMainWindow>
#include <QMimeData>
#include <QLabel>

TEnv::IntVar ExportUseMarkers("ExportUseMarkers", 1);

bool checkForMeshColumns(TXsheet *xsh,
                         std::vector<TXshChildLevel *> &childLevels) {
  bool foundMesh = false;
  int count      = xsh->getColumnCount();
  for (int c = 0; c < count; c++) {
    int r0, r1;
    int n = xsh->getCellRange(c, r0, r1);
    if (n > 0) {
      bool changed = false;
      std::vector<TXshCell> cells(n);
      xsh->getCells(r0, c, n, &cells[0]);
      for (int i = 0; i < n; i++) {
        TXshCell currCell = cells[i];
        if (!cells[i].isEmpty() &&
            cells[i].m_level->getType() == MESH_XSHLEVEL) {
          return true;
        }
        // check the sub xsheets too
        if (!cells[i].isEmpty() &&
            cells[i].m_level->getType() == CHILD_XSHLEVEL) {
          TXshChildLevel *level = cells[i].m_level->getChildLevel();
          // make sure we haven't already checked the level
          if (level &&
              std::find(childLevels.begin(), childLevels.end(), level) ==
                  childLevels.end()) {
            childLevels.push_back(level);
            TXsheet *subXsh = level->getXsheet();
            foundMesh       = checkForMeshColumns(subXsh, childLevels);
          }
        }
      }
    }
  }

  return foundMesh;
}

//=============================================================================
// RenderLocker
//-----------------------------------------------------------------------------

class RenderLocker {
  bool &m_rendering;

public:
  RenderLocker(bool &rendering) : m_rendering(rendering) { m_rendering = true; }
  ~RenderLocker() { m_rendering = false; }
};

//=============================================================================
//
//-----------------------------------------------------------------------------
RenderController::RenderController()
    : m_progressDialog(0)
    , m_frame(0)
    , m_rendering(false)
    , m_cancelled(false)
    , m_properties(0)
    , m_movieExt("mov")
    , m_useMarkers(false) {
  m_properties = new TOutputProperties();
}

//-----------------------------------------------------------------------------

RenderController::~RenderController() {}

//-----------------------------------------------------------------------------

void RenderController::setUseMarkers(bool useMarkers) {
  m_useMarkers = useMarkers;
}

//-----------------------------------------------------------------------------

void RenderController::setMovieExt(string ext) { m_movieExt = ext; }

//-----------------------------------------------------------------------------

bool RenderController::addScene(MovieGenerator &g, ToonzScene *scene) {
  bool hasMesh = false;
  std::vector<TXshChildLevel *> childLevels;
  hasMesh      = checkForMeshColumns(scene->getXsheet(), childLevels);
  TXsheet *xsh = scene->getXsheet();
  if (hasMesh) return false;

  int frameCount = scene->getFrameCount();
  if (frameCount < 0) return false;
  int r0 = 0, r1 = frameCount - 1, step = 0;
  if (m_useMarkers) {
    int r0M, r1M, step;
    scene->getProperties()->getPreviewProperties()->getRange(r0M, r1M, step);
    if (r0M <= r1M) {
      r0 = r0M;
      r1 = r1M;
    }
  }
  if (r1 < r0) return false;
  TPixel color = scene->getProperties()->getBgColor();
  if (m_movieExt == "mp4" || m_movieExt == "webm") color.m = 255;
  g.setBackgroundColor(color);
  g.addScene(*scene, r0, r1);
  return true;
}

//-----------------------------------------------------------------------------

bool RenderController::onFrameCompleted(int frameCount) {
  if (m_cancelled) {
    m_cancelled = false;
    return false;
  }

  ++m_frame;
  m_progressDialog->setValue(m_frame);
  qApp->processEvents();
  return true;
}

//-----------------------------------------------------------------------------

int RenderController::computeClipFrameCount(const TFilePath &clipPath,
                                            bool useMarkers, int *frameOffset) {
  if (frameOffset) *frameOffset = 0;

  ToonzScene scene;
  scene.load(clipPath);
  if (useMarkers) {
    int r0, r1, step;
    scene.getProperties()->getPreviewProperties()->getRange(r0, r1, step);
    if (r1 >= r0) {
      if (frameOffset) *frameOffset = r0;
      return r1 - r0 + 1;
    }
  }
  return scene.getFrameCount();
}

//-----------------------------------------------------------------------------

int RenderController::computeTotalFrameCount(
    const std::vector<TFilePath> &clipList, bool useMarkers) {
  int count = 0;
  int i;
  for (i = 0; i < (int)clipList.size(); i++)
    count += computeClipFrameCount(clipList[i], useMarkers);
  return count;
}

//-----------------------------------------------------------------------------

void RenderController::myCancel() { m_cancelled = true; }

//-----------------------------------------------------------------------------

void RenderController::generateMovie(TFilePath outPath, bool emitSignal) {
  if (outPath == TFilePath()) {
    DVGui::warning(tr("Please specify an file name."));
    return;
  }
  if (m_clipList.empty()) {
    DVGui::warning(tr("Drag a scene into the box to export a scene."));
    return;
  }

  // verifico che il "generatore" sia libero.
  if (m_rendering) return;
  m_cancelled = false;

  RenderLocker locker(m_rendering);

  if (outPath.getName() == "") outPath = outPath.withName("untitled");

  int sceneCount = (int)m_clipList.size();
  if (sceneCount < 1) {
    DVGui::warning(tr("Drag a scene into the box to export a scene."));
    return;
  }

  // Risoluzione e Settings
  TDimension resolution(0, 0);
  TFilePath fp;
  TFilePath firstClipPath = fp = m_clipList[0];
  int sceneIndex;

  try {
    getMovieProperties(firstClipPath, resolution);

    for (sceneIndex = 1; sceneIndex < sceneCount; sceneIndex++) {
      TDimension testResolution(0, 0);
      fp = m_clipList[sceneIndex];
      getMovieProperties(fp, testResolution);
      if (testResolution != resolution) {
        /*wstring text = L"The ";
text += fp.getLevelNameW();
text += L" scene has a different resolution from the ";
text += firstClipPath.getLevelNameW();
text += L" scene.\nThe output result may differ from what you expect. What do
you want to do?";*/
        QString text =
            tr("The %1  scene has a different resolution from the %2 scene.\n \
                          The output result may differ from what you expect. What do you want to do?")
                .arg(QString::fromStdWString(fp.getLevelNameW()))
                .arg(QString::fromStdWString(firstClipPath.getLevelNameW()));
        int ret = DVGui::MsgBox(text, tr("Continue"), tr("Cancel"));
        if (ret == 2)
          return;
        else
          break;
      }
    }
  } catch (TException const &) {
    QString msg;
    msg = QObject::tr(
              "There were problems loading the scene %1.\n Some files may be "
              "missing.")
              .arg(QString::fromStdWString(fp.getWideString()));

    DVGui::warning(msg);
    return;
  } catch (...) {
    QString msg = QObject::tr(
                      "There were problems loading the scene %1.\n Some files "
                      "may be missing.")
                      .arg(QString::fromStdWString(fp.getWideString()));

    DVGui::warning(msg);
    return;
  }

  // Inizializzo la progressBar
  DVGui::ProgressDialog progress(tr("Exporting ..."), tr("Abort"), 0, 1,
                                 TApp::instance()->getMainWindow());
  progress.setWindowModality(Qt::WindowModal);
  progress.setWindowTitle(tr("Exporting"));
  progress.setValue(0);
  progress.show();
  connect(&progress, SIGNAL(canceled()), this, SLOT(myCancel()));
  qApp->processEvents();
  m_progressDialog = &progress;
  m_frame          = 0;

  int totalFrameCount = computeTotalFrameCount(m_clipList, m_useMarkers);
  progress.setMaximum(totalFrameCount);

  try {
    // Inizializzo il movieGenerator
    MovieGenerator movieGenerator(outPath, resolution, *m_properties,
                                  m_useMarkers);
    movieGenerator.setListener(this);

    OnionSkinMask osMask;  // = app->getOnionSkinMask(false);
    if (!osMask.isEmpty() && osMask.isEnabled())
      movieGenerator.setOnionSkin(
          TApp::instance()->getCurrentColumn()->getColumnIndex(), osMask);
    movieGenerator.setSceneCount(sceneCount);

    // Aggiungo i soundtrack al movieGenerator
    for (sceneIndex = 0; sceneIndex < sceneCount; sceneIndex++) {
      TFilePath fp = m_clipList[sceneIndex];
      int frameOffset;
      int sceneFrames = computeClipFrameCount(fp, m_useMarkers, &frameOffset);
      if (sceneFrames == 0) continue;
      ToonzScene scene;
      scene.load(fp);
      try {
        movieGenerator.addSoundtrack(scene, frameOffset, sceneFrames);
      } catch (const TException &) {
        QString text = tr("The %1 scene contains an audio file with different "
                          "characteristics from the one used in the first "
                          "exported scene.\nThe audio file will not be "
                          "included in the rendered clip.")
                           .arg(QString::fromStdWString(fp.getLevelNameW()));
        DVGui::warning(text);
      }
    }

    // Aggiungo le scene al movieGenerator
    for (sceneIndex = 0; sceneIndex < sceneCount; sceneIndex++) {
      TFilePath fp    = m_clipList[sceneIndex];
      int sceneFrames = computeClipFrameCount(fp, m_useMarkers);
      if (sceneFrames == 0) continue;
      ToonzScene scene;
      scene.load(fp);
      std::vector<TXshChildLevel *> childLevels;
      if (checkForMeshColumns(scene.getXsheet(), childLevels)) {
        QString text = tr("The %1 scene contains a plastic deformed level.\n"
                          "These levels can't be exported with this tool.")
                           .arg(QString::fromStdWString(fp.getLevelNameW()));
        DVGui::warning(text);
        continue;
      }
      addScene(movieGenerator, &scene);
    }

    movieGenerator.close();
    if (m_frame >= totalFrameCount &&
        Preferences::instance()->isGeneratedMovieViewEnabled()) {
      if (Preferences::instance()->isDefaultViewerEnabled() &&
          (outPath.getType() == "mov" || outPath.getType() == "avi" ||
           outPath.getType() == "3gp")) {
        QString name = QString::fromStdString(outPath.getName());

        if (!TSystem::showDocument(outPath)) {
          QString msg(QObject::tr("It is not possible to display the file %1: "
                                  "no player associated with its format")
                          .arg(QString::fromStdWString(
                              outPath.withoutParentDir().getWideString())));
          DVGui::warning(msg);
        }
      } else {
        int r0 = 1, r1 = totalFrameCount, step = 1;

        const TRenderSettings rs = m_properties->getRenderSettings();

        double timeStretchFactor;
        timeStretchFactor = (double)rs.m_timeStretchTo / rs.m_timeStretchFrom;
        int numFrames     = troundp((r1 - r0 + 1) * timeStretchFactor);
        r0                = troundp(r0 * timeStretchFactor);
        r1                = r0 + numFrames - 1;

        TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
        prop->m_frameRate              = m_properties->getFrameRate();
        TSoundTrack *snd =
            TApp::instance()->getCurrentXsheet()->getXsheet()->makeSound(prop);

        ::viewFile(outPath, 1, totalFrameCount, step, rs.m_shrinkX, snd, 0);
      }
    }
  } catch (TException &) {
    return;
  } catch (...) {
    return;
  }
  // per sicurezza
  progress.setValue(totalFrameCount);

  if (emitSignal) emit movieGenerated();
}

//-----------------------------------------------------------------------------

void RenderController::getMovieProperties(const TFilePath &scenePath,
                                          TDimension &resolution) {
  if (scenePath == TFilePath()) return;
  ToonzScene scene;
  scene.loadNoResources(scenePath);
  resolution = scene.getCurrentCamera()->getRes();
}

//-----------------------------------------------------------------------------

TFilePath RenderController::getClipPath(int index) const {
  assert(0 <= index && index < getClipCount());
  return m_clipList[index];
}

//-----------------------------------------------------------------------------

void RenderController::setClipPath(int index, const TFilePath &path) {
  assert(0 <= index && index < getClipCount());
  m_clipList[index] = path;
}

//-----------------------------------------------------------------------------

void RenderController::addClipPath(const TFilePath &path) {
  m_clipList.push_back(path);
}

//-----------------------------------------------------------------------------

void RenderController::insertClipPath(int index, const TFilePath &path) {
  assert(0 <= index && index <= getClipCount());
  m_clipList.insert(m_clipList.begin() + index, path);
}

//-----------------------------------------------------------------------------

void RenderController::removeClipPath(int index) {
  assert(0 <= index && index < getClipCount());
  m_clipList.erase(m_clipList.begin() + index);
}

//-----------------------------------------------------------------------------

void RenderController::resetClipList() { m_clipList.clear(); }

//=============================================================================
//
// ClipListViewer
//
//-----------------------------------------------------------------------------

ClipListViewer::ClipListViewer(QWidget *parent)
    : DvItemViewer(parent)
    , m_dropInsertionPoint(-1)
    , m_loadSceneFromExportPanel(false) {
  resetList();
  setModel(this);
  setAcceptDrops(true);
  getPanel()->enableSingleColumn(true);
}

//-----------------------------------------------------------------------------

const char *ClipListViewer::m_mimeFormat = "application/vnd.toonz.cliplist";

//-----------------------------------------------------------------------------

RenderController *ClipListViewer::getController() const {
  return RenderController::instance();
}

//-----------------------------------------------------------------------------

void ClipListViewer::resetList() {
  if (!m_loadSceneFromExportPanel) getController()->resetClipList();
}

//-----------------------------------------------------------------------------

void ClipListViewer::removeSelectedClips() {
  if (getPanel()->getSelection()->isEmpty()) return;
  int i;
  for (i = getController()->getClipCount() - 1; i >= 0; i--) {
    if (getPanel()->getSelection()->isSelected(i))
      getController()->removeClipPath(i);
  }
  updateContentSize();
  getPanel()->getSelection()->selectNone();
  getPanel()->update();
}

//-----------------------------------------------------------------------------

void ClipListViewer::moveSelectedClips(int dstIndex) {
  std::vector<TFilePath> selectedClips;
  int i;
  for (i = getController()->getClipCount() - 1; i >= 0; i--)
    if (getPanel()->getSelection()->isSelected(i)) {
      if (i < dstIndex) dstIndex--;
      selectedClips.push_back(getController()->getClipPath(i));
      getController()->removeClipPath(i);
    }
  if (dstIndex < 0)
    dstIndex = 0;
  else if (dstIndex > getController()->getClipCount())
    dstIndex = getController()->getClipCount();
  for (i = 0; i < (int)selectedClips.size(); i++)
    getController()->insertClipPath(dstIndex + i, selectedClips[i]);
  getPanel()->getSelection()->selectNone();
  getPanel()->update();
}

//-----------------------------------------------------------------------------

void ClipListViewer::getSelectedClips(std::vector<TFilePath> &selectedClips) {
  int i;
  for (i = 0; i < getController()->getClipCount(); i++)
    if (getPanel()->getSelection()->isSelected(i))
      selectedClips.push_back(getController()->getClipPath(i));
}

//-----------------------------------------------------------------------------

static QByteArray packStringList(const QStringList &lst) {
  QByteArray ba;
  QDataStream ds(&ba, QIODevice::WriteOnly);
  ds << (qint32)lst.size();
  int i;
  for (i = 0; i < lst.size(); i++) ds << lst.at(i);
  return ba;
}

static QStringList unpackStringList(const QByteArray &ba) {
  QStringList lst;
  QDataStream ds(ba);
  qint32 n = 0;
  ds >> n;
  int i;
  for (i = 0; i < n; i++) {
    QString s;
    ds >> s;
    lst.append(s);
  }
  return lst;
}

//-----------------------------------------------------------------------------

void ClipListViewer::startDragDrop() {
  std::vector<TFilePath> selectedClips;
  getSelectedClips(selectedClips);
  if (selectedClips.empty()) return;

  QStringList slist;
  int i;
  for (i = 0; i < (int)selectedClips.size(); i++)
    slist << QString::fromStdWString(selectedClips[i].getWideString());

  QMimeData *mimeData = new QMimeData;
  mimeData->setData(m_mimeFormat, packStringList(slist));
  QDrag *drag = new QDrag(this);
  drag->setMimeData(mimeData);
  Qt::DropAction dropAction = drag->exec(Qt::MoveAction | Qt::CopyAction);
}

//-----------------------------------------------------------------------------

int ClipListViewer::getItemCount() const {
  return getController()->getClipCount();
}

//-----------------------------------------------------------------------------

QVariant ClipListViewer::getItemData(int index, DataType dataType,
                                     bool isSelected) {
  if (index < 0 || index >= getController()->getClipCount()) return QVariant();
  TFilePath fp      = getController()->getClipPath(index);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  if (dataType == Name)
    return QString::fromStdString(fp.getName());
  else if (dataType == ToolTip || dataType == FullPath)
    return QString::fromStdWString(fp.getWideString());
  else if (dataType == Thumbnail) {
    QSize iconSize = getPanel()->getIconSize();
    QPixmap icon   = IconGenerator::instance()->getIcon(fp);
    return scalePixmapKeepingAspectRatio(icon, iconSize, Qt::transparent);
  }

  return QVariant();
}

//-----------------------------------------------------------------------------

QMenu *ClipListViewer::getContextMenu(QWidget *parent, int index) {
  QMenu *menu                          = new QMenu(parent);
  CommandManager *cm                   = CommandManager::instance();
  const std::set<int> &selectedIndices = getPanel()->getSelectedIndices();
  if (selectedIndices.size() == 1) {
    int index = *selectedIndices.begin();
    if (0 <= index && index < getController()->getClipCount()) {
      QAction *action = new QAction(tr("Load Scene"), menu);
      connect(action, SIGNAL(triggered()), this, SLOT(loadScene()));
      menu->addAction(action);
    }
  }
  menu->addAction(cm->getAction(MI_Clear));
  return menu;
}

//-----------------------------------------------------------------------------

void ClipListViewer::addCurrentScene() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  std::vector<TXshChildLevel *> childLevels;
  if (checkForMeshColumns(scene->getXsheet(), childLevels)) return;
  if (!scene->isUntitled()) {
    int j = getItemCount();
    TFilePath fp(scene->decodeFilePath(scene->getScenePath()));
    getController()->insertClipPath(j, fp);
    updateContentSize();
    getPanel()->update();
    update();
  }
}

//-----------------------------------------------------------------------------

void ClipListViewer::setDropInsertionPoint(const QPoint &pos) {
  int old                  = m_dropInsertionPoint;
  m_dropInsertionPoint     = getItemCount();
  DvItemViewerPanel *panel = getPanel();
  QPoint panelPos          = panel->mapFromParent(pos);
  int i;
  for (i = 0; i < getItemCount(); i++) {
    QRect itemRect = getPanel()->index2pos(i);
    int y          = (itemRect.bottom() + itemRect.top()) / 2;
    if (panelPos.y() < y) {
      m_dropInsertionPoint = i;
      break;
    }
  }
  if (old != m_dropInsertionPoint) getPanel()->update();
}

//-----------------------------------------------------------------------------

void ClipListViewer::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    for (const QUrl &url : event->mimeData()->urls()) {
      TFilePath fp(url.toLocalFile().toStdString());
      if (fp.getType() != "tnz") return;
    }
    setDropInsertionPoint(event->pos());
    event->setDropAction(Qt::CopyAction);
    event->acceptProposedAction();
    update();
  } else if (event->mimeData()->hasFormat(m_mimeFormat)) {
    setDropInsertionPoint(event->pos());
    event->setDropAction(Qt::MoveAction);
    event->acceptProposedAction();
    update();
  }
}

//-----------------------------------------------------------------------------

void ClipListViewer::dragMoveEvent(QDragMoveEvent *event) {
  int oldDropInsertionPoint = m_dropInsertionPoint;
  setDropInsertionPoint(event->pos());
  if (oldDropInsertionPoint != m_dropInsertionPoint) update();
}

//-----------------------------------------------------------------------------

void ClipListViewer::dropEvent(QDropEvent *event) {
  if (event->mimeData()->hasUrls()) {
    int j        = m_dropInsertionPoint;
    if (j < 0) j = getItemCount();
    for (const QUrl &url : event->mimeData()->urls()) {
      TFilePath fp(url.toLocalFile().toStdString());
      if (fp.getType() == "tnz") getController()->insertClipPath((j++), fp);
    }
    event->setDropAction(Qt::CopyAction);
    event->acceptProposedAction();
  } else if (event->mimeData()->hasFormat(m_mimeFormat) &&
             m_dropInsertionPoint >= 0) {
    moveSelectedClips(m_dropInsertionPoint);
    event->setDropAction(Qt::MoveAction);
    event->acceptProposedAction();
  }

  updateContentSize();
  m_dropInsertionPoint = -1;
  getPanel()->update();
  update();
}

//-----------------------------------------------------------------------------

void ClipListViewer::dragLeaveEvent(QDragLeaveEvent *event) {
  m_dropInsertionPoint = -1;
  getPanel()->update();
}

//-----------------------------------------------------------------------------

void ClipListViewer::enableSelectionCommands(TSelection *selection) {
  selection->enableCommand(this, MI_Clear,
                           &ClipListViewer::removeSelectedClips);
}

//-----------------------------------------------------------------------------

void ClipListViewer::draw(QPainter &p) {
  if (m_dropInsertionPoint >= 0) {
    p.setPen(Qt::black);
    // p.drawLine(0,0,width(),height());
    int y = getPanel()->index2pos(m_dropInsertionPoint).topLeft().y() - 2;
    p.drawLine(10, y, width() - 10, y);
  }
}

//-----------------------------------------------------------------------------

void ClipListViewer::loadScene() {
  // se non ho selezionato una sola scena, diversa da quella corrente, esco.
  const std::set<int> &selectedIndices = getPanel()->getSelectedIndices();
  if (selectedIndices.size() != 1) return;
  int index = *selectedIndices.begin();
  if (!(0 <= index && index < getController()->getClipCount())) return;
  // selectedScenePath e' la scena da caricare
  TFilePath selectedScenePath = getController()->getClipPath(index);

  m_loadSceneFromExportPanel = true;

  // se necessario salvo la scena corrente (e le assegno un filepath)
  // se l'utente sceglie "cancel" esco
  if (!IoCmd::saveSceneIfNeeded(tr("Load Scene"))) return;

  // Nella clip list assegno un path esplicito all'eventuale scena corrente
  // (se la scena corrente non e' stata salvata dall'utente elimino il
  // riferimento)
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath scenePath;
  if (!scene->isUntitled()) scenePath = scene->getScenePath();
  // carico la scena corrente
  IoCmd::loadScene(selectedScenePath);
  m_loadSceneFromExportPanel = false;
}

//=============================================================================
//
// ExportPanel
//
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ExportPanel::ExportPanel(QWidget *parent, Qt::WindowFlags flags)
#else
ExportPanel::ExportPanel(QWidget *parent, Qt::WFlags flags)
#endif
    : TPanel(parent)
    , m_clipListViewer(0)
    , m_saveInFileFld(0)
    , m_fileNameFld(0)
    , m_useMarker(0) {
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
  setWindowTitle(tr("Export"));

  setMinimumWidth(270);

  RenderController *renderController = RenderController::instance();

  QFrame *box = new QFrame(this);
  box->setObjectName("exportPanel");
  box->setFrameStyle(QFrame::StyledPanel);
  box->setStyleSheet("#exportPanel { margin: 1px; border: 0px; }");

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  mainLayout->setAlignment(Qt::AlignTop);
  // ClipList
  m_clipListViewer = new ClipListViewer(box);
  m_clipListViewer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();

  connect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(onSceneSwitched()));

  // Settings Box
  QFrame *settingsBox = new QFrame(box);
  settingsBox->setObjectName("settingsBox");
  settingsBox->setFrameStyle(QFrame::StyledPanel);
  QVBoxLayout *settingsLayout = new QVBoxLayout();
  settingsLayout->setMargin(15);
  settingsLayout->setSpacing(5);
  settingsLayout->setAlignment(Qt::AlignTop);

  ToonzScene *scene      = TApp::instance()->getCurrentScene()->getScene();
  std::wstring sceneName = scene->getSceneName();
  TFilePath scenePath =
      scene->getProperties()->getOutputProperties()->getPath().getParentDir();

  // Label + saveInFileFld
  QHBoxLayout *saveIn = new QHBoxLayout;
  saveIn->setMargin(0);
  saveIn->setSpacing(5);
  m_saveInFileFld = new DVGui::FileField(settingsBox);
  m_saveInFileFld->setPath(QString::fromStdWString(scenePath.getWideString()));
  QLabel *dirLabel = new QLabel(tr("Save in:"));
  dirLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  dirLabel->setFixedSize(65, m_saveInFileFld->height());
  saveIn->addWidget(dirLabel);
  saveIn->addWidget(m_saveInFileFld);

  // Label + m_fileNameFld
  QHBoxLayout *fileNname = new QHBoxLayout;
  fileNname->setMargin(0);
  fileNname->setSpacing(5);
  m_fileNameFld =
      new DVGui::LineEdit(QString::fromStdWString(sceneName), settingsBox);
  QLabel *nameLabel = new QLabel(tr("File Name:"));
  nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  nameLabel->setFixedSize(65, m_fileNameFld->height());
  fileNname->addWidget(nameLabel);
  fileNname->addWidget(m_fileNameFld);

  // file formats
  QHBoxLayout *fileFormatLayout = new QHBoxLayout();
  m_fileFormat                  = new QComboBox();
  QStringList formats;
  TImageWriter::getSupportedFormats(formats, true);
  TLevelWriter::getSupportedFormats(formats, true);
  Tiio::Writer::getSupportedFormats(formats, true);
  formats.sort();

  m_fileFormat->addItems(formats);
  m_fileFormat->setFixedHeight(DVGui::WidgetHeight + 2);
  connect(m_fileFormat, SIGNAL(currentIndexChanged(const QString &)),
          SLOT(onFormatChanged(const QString &)));
  // m_fileFormat->setCurrentIndex(formats.indexOf("mov"));

  QPushButton *fileFormatButton = new QPushButton(QString(tr("Options")));
  fileFormatButton->setMinimumWidth(75);
  fileFormatButton->setFixedHeight(DVGui::WidgetHeight);
  connect(fileFormatButton, SIGNAL(pressed()), this, SLOT(openSettingsPopup()));
  QLabel *fileFormat = new QLabel(tr("File Format:"));
  fileFormat->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  fileFormat->setFixedSize(65, fileFormatButton->height());

  fileFormatLayout->addWidget(fileFormat);
  fileFormatLayout->addWidget(m_fileFormat);
  fileFormatLayout->addWidget(fileFormatButton);

  // Use Marker checkbox
  m_useMarker = new QCheckBox(tr("Use Markers"), settingsBox);
  m_useMarker->setMinimumHeight(30);
  m_useMarker->setChecked(ExportUseMarkers);
  connect(m_useMarker, SIGNAL(toggled(bool)), this,
          SLOT(onUseMarkerToggled(bool)));

  // Export button
  QFrame *exportBox = new QFrame(box);
  exportBox->setFrameStyle(QFrame::StyledPanel);
  exportBox->setObjectName("exportBox");

  QVBoxLayout *exportLayout = new QVBoxLayout();
  QPushButton *exportButton = new QPushButton(tr("Export"), exportBox);
  exportButton->setFixedSize(65, DVGui::WidgetHeight);
  connect(exportButton, SIGNAL(pressed()), this, SLOT(generateMovie()));
  exportLayout->addWidget(exportButton, 0, Qt::AlignCenter);
  exportBox->setLayout(exportLayout);

  settingsLayout->addLayout(saveIn);
  settingsLayout->addLayout(fileNname);
  settingsLayout->addLayout(fileFormatLayout);
  settingsLayout->addWidget(m_useMarker);
  settingsBox->setLayout(settingsLayout);

  if (m_clipListViewer) mainLayout->addWidget(m_clipListViewer);
  mainLayout->addWidget(new DVGui::Separator("", this));
  mainLayout->addWidget(settingsBox, 0);
  mainLayout->addWidget(new DVGui::Separator("", this));
  mainLayout->addWidget(exportBox, 0);

  connect(RenderController::instance(), SIGNAL(movieGenerated()), this,
          SLOT(onMovieGenerated()));

  box->setLayout(mainLayout);
  setWidget(box);
  loadExportSettings();
}

//-----------------------------------------------------------------------------

ExportPanel::~ExportPanel() { saveExportSettings(); }

//-----------------------------------------------------------------------------

void ExportPanel::showEvent(QShowEvent *) {
  if (m_clipListViewer->getItemCount() == 0) {
    m_clipListViewer->addCurrentScene();
  }
}

//-----------------------------------------------------------------------------

void ExportPanel::loadExportSettings() {
  TFilePath exportPath = TEnv::getConfigDir() + "exportsettings.txt";
  if (!TSystem::doesExistFileOrLevel(exportPath)) return;
  TIStream is(exportPath);
  std::string tagName;
  try {
    while (is.matchTag(tagName)) {
      if (tagName == "ExportDir") {
        TFilePath outPath;
        is >> outPath;
        m_saveInFileFld->setPath(
            QString::fromStdWString(outPath.getWideString()));
      } else if (tagName == "ExportFormat") {
        std::string ext;
        is >> ext;
        int index = m_fileFormat->findText(QString::fromStdString(ext));
        m_fileFormat->setCurrentIndex(index);
      } else if (tagName == "FormatSettings") {
        TOutputProperties *outProp =
            RenderController::instance()->getOutputPropertites();
        std::string ext       = m_fileFormat->currentText().toStdString();
        TPropertyGroup *props = outProp->getFileFormatProperties(ext);
        props->loadData(is);
      } else if (tagName == "UseMarkers") {
        int value;
        is >> value;
        m_useMarker->setChecked(value);
      }
      is.closeChild();
    }
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void ExportPanel::saveExportSettings() {
  QString path = m_saveInFileFld->getPath();
  TFilePath outPath(path.toStdWString());

  TFilePath exportPath = TEnv::getConfigDir() + "exportsettings.txt";
  TOStream os(exportPath);
  os.child("ExportDir") << outPath;
  std::string ext = m_fileFormat->currentText().toStdString();
  os.child("ExportFormat") << ext;

  os.openChild("FormatSettings");
  TOutputProperties *outProp =
      RenderController::instance()->getOutputPropertites();
  TPropertyGroup *props = outProp->getFileFormatProperties(ext);
  props->saveData(os);
  os.closeChild();

  os.child("UseMarkers") << (int)m_useMarker->isChecked();
}

//-----------------------------------------------------------------------------

void ExportPanel::generateMovie() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  std::string ext   = RenderController::instance()->getMovieExt();
  QString path      = m_saveInFileFld->getPath();
  TFilePath outPath(path.toStdWString());
  outPath = (outPath + m_fileNameFld->text().toStdWString()).withType(ext);
  if (TFileType::getInfo(outPath) == TFileType::RASTER_IMAGE ||
      outPath.getType() == "pct" || outPath.getType() == "pic" ||
      outPath.getType() == "pict")  // pct e' un formato"livello" (ha i settings
                                    // di quicktime) ma fatto di diversi frames
    outPath = outPath.withFrame(TFrameId::EMPTY_FRAME);
  RenderController::instance()->generateMovie(scene->decodeFilePath(outPath));
}

//-----------------------------------------------------------------------------

void ExportPanel::onUseMarkerToggled(bool toggled) {
  RenderController::instance()->setUseMarkers(toggled);
  if (toggled) {
    ExportUseMarkers = 1;
  } else {
    ExportUseMarkers = 0;
  }
}

//-----------------------------------------------------------------------------

void ExportPanel::onSceneSwitched() {
  if (m_clipListViewer) {
    m_clipListViewer->resetList();
    m_clipListViewer->update();
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  m_fileNameFld->setText(QString::fromStdWString(scene->getSceneName()));
}

//-----------------------------------------------------------------------------

void ExportPanel::onFormatChanged(const QString &format) {
  RenderController::instance()->setMovieExt(format.toStdString());
}

//-----------------------------------------------------------------------------

void ExportPanel::openSettingsPopup() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  std::string ext = RenderController::instance()->getMovieExt();

  TOutputProperties *outProps =
      RenderController::instance()->getOutputPropertites();
  if (!outProps) {
    // non dovrebbe mai capitare!
    assert(0);
    outProps = new TOutputProperties();
    RenderController::instance()->setOutputPropertites(outProps);
  }

  TPropertyGroup *props = outProps->getFileFormatProperties(ext);
  openFormatSettingsPopup(this, ext, props);
}
