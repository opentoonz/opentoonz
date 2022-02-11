

#include "filebrowserpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "iocommand.h"
#include "exportlevelcommand.h"
#include "filebrowser.h"
#include "tapp.h"
#include "filebrowsermodel.h"
#include "formatsettingspopups.h"
#include "magpiefileimportpopup.h"
#include "columnselection.h"
#include "convertpopup.h"
#include "matchline.h"
#include "colormodelbehaviorpopup.h"
#if defined(x64)
#include "penciltestpopup.h"  // FrameNumberLineEdit
#else
#include "penciltestpopup_qt.h"
#endif

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/tselectionhandle.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/studiopalette.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tcamera.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshleveltypes.h"
// specify in the preference whether to replace the level after saveLevelAs
// command
#include "toonz/preferences.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/levelset.h"
#include "toonz/palettecmd.h"
#include "toonz/stage.h"

// TnzCore includes
#include "tsystem.h"
#include "tiio.h"
#include "tlevel_io.h"
#include "tundo.h"

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QCoreApplication>
#include <QMainWindow>
#include <QApplication>

//***********************************************************************************
//    FileBrowserPopup  implementation
//***********************************************************************************

FileBrowserPopup::FileBrowserPopup(const QString &title, Options options,
                                   QString applyButtonTxt,
                                   QWidget *customWidget)
    : QDialog(TApp::instance()->getMainWindow())
    , m_isDirectoryOnly(false)
    , m_multiSelectionEnabled(options & MULTISELECTION)
    , m_forSaving(options & FOR_SAVING)
    , m_dialogSize(800, 600)
    , m_customWidget(customWidget) {
  setWindowTitle(title);
  setModal(false);

  m_browser        = new FileBrowser(this, 0, false, m_multiSelectionEnabled);
  m_nameFieldLabel = new QLabel(tr("File name:"));
  m_nameField      = new DVGui::LineEdit(this);
  m_okButton       = new QPushButton(tr("OK"), this);
  m_cancelButton   = new QPushButton(tr("Cancel"), this);
  QPushButton *applyButton = 0;
  if (options & WITH_APPLY_BUTTON)
    applyButton = new QPushButton(
        (applyButtonTxt.isEmpty()) ? tr("Apply") : applyButtonTxt, this);

  std::list<std::vector<TFrameId>> tmp_list;
  m_currentFIdsSet = tmp_list;

  //-------------

  m_okButton->setMaximumWidth(100);
  m_okButton->setAutoDefault(false);
  m_cancelButton->setMaximumWidth(100);
  m_cancelButton->setAutoDefault(false);
  if (applyButton) {
    applyButton->setMaximumWidth(100);
    applyButton->setAutoDefault(false);
  }

  // layout
  if (!(options & CUSTOM_LAYOUT)) {
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setMargin(0);
    mainLayout->setSpacing(3);
    {
      mainLayout->addWidget(m_browser, 1);

      QHBoxLayout *bottomLay = new QHBoxLayout();
      bottomLay->setMargin(5);
      bottomLay->setSpacing(3);
      {
        bottomLay->addWidget(m_nameFieldLabel, 0);
        bottomLay->addWidget(m_nameField, 1);
      }
      mainLayout->addLayout(bottomLay);

      if (m_customWidget) mainLayout->addWidget(m_customWidget);

      QHBoxLayout *buttonsLay = new QHBoxLayout();
      buttonsLay->setMargin(5);
      buttonsLay->setSpacing(15);
      {
        buttonsLay->addStretch();
        buttonsLay->addWidget(m_okButton);
        if (applyButton) buttonsLay->addWidget(applyButton);
        buttonsLay->addWidget(m_cancelButton);
      }
      mainLayout->addLayout(buttonsLay);
    }
    setLayout(mainLayout);
  }

  // Establish connections
  bool ret = true;
  ret =
      ret && connect(m_okButton, SIGNAL(clicked()), this, SLOT(onOkPressed()));
  ret = ret && connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(close()));
  ret =
      ret &&
      connect(
          m_browser,
          SIGNAL(filePathsSelected(const std::set<TFilePath> &,
                                   const std::list<std::vector<TFrameId>> &)),
          this,
          SLOT(onFilePathsSelected(const std::set<TFilePath> &,
                                   const std::list<std::vector<TFrameId>> &)));
  ret = ret && connect(m_browser, SIGNAL(filePathClicked(const TFilePath &)),
                       this, SIGNAL(filePathClicked(const TFilePath &)));
  if (applyButton) {
    ret = ret &&
          connect(applyButton, SIGNAL(clicked()), this, SLOT(onApplyPressed()));
  }
  assert(ret);

  resize(m_dialogSize);
  /*- Qt5 でQDialogがParentWidgetの中心位置に来ない不具合を回避する -*/
  QPoint dialogCenter       = mapToGlobal(rect().center());
  QPoint parentWindowCenter = TApp::instance()->getMainWindow()->mapToGlobal(
      TApp::instance()->getMainWindow()->rect().center());
  move(parentWindowCenter - dialogCenter);
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::setFilterTypes(const QStringList &typesList) {
  m_browser->setFilterTypes(typesList);
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::removeFilterType(const QString &type) {
  m_browser->removeFilterType(type);
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::addFilterType(const QString &type) {
  m_browser->addFilterType(type);
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::setFileMode(bool isDirectoryOnly) {
  m_isDirectoryOnly = isDirectoryOnly;
  if (isDirectoryOnly) {
    m_nameFieldLabel->setText(tr("Folder name:"));
    connect(m_browser, SIGNAL(treeFolderChanged(const TFilePath &)), this,
            SLOT(onFilePathClicked(const TFilePath &)));
  } else {
    m_nameFieldLabel->setText(tr("File name:"));
    disconnect(m_browser, SIGNAL(treeFolderChanged(const TFilePath &)), this,
               SLOT(onFilePathClicked(const TFilePath &)));
  }
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::setFolder(const TFilePath &folderPath) {
  m_browser->setFolder(folderPath, true);
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::setFilename(const TFilePath &filename) {
  m_nameField->setText(QString::fromStdWString(filename.getWideString()));
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::onOkPressed() {
  const TFilePath &folder = m_browser->getFolder();

  // Rebuild paths selection in case the text field has an explicit entry
  if (!m_nameField->text().isEmpty()) {
    const QString &str = m_nameField->text();
    if (!isValidFileName(QFileInfo(str).baseName()) && !m_isDirectoryOnly) {
      DVGui::error(
          QObject::tr("A filename cannot be empty or contain any of the "
                      "following characters:\n \\ / : * ? \" < > |"));
      return;
    }
    if (isReservedFileName_message(QFileInfo(str).baseName())) return;

    m_selectedPaths.clear();
    if (!m_isDirectoryOnly)
      m_selectedPaths.insert(folder +
                             TFilePath(m_nameField->text().toStdWString()));
    else
      m_selectedPaths.insert(TFilePath(m_nameField->text().toStdWString()));
  }

  // Analyze and correct the paths selection
  std::set<TFilePath> pathSet;

  std::set<TFilePath>::const_iterator pt, pEnd(m_selectedPaths.end());
  for (pt = m_selectedPaths.begin(); pt != pEnd; ++pt) {
    if (folder == TFilePath()) {
      // history                                      // That means, TFilePath()
      // represents
      if (*pt == TFilePath() ||
          !pt->isAbsolute())  // the History folder? Really? That's lame...
      {
        DVGui::error(tr("Invalid file"));
        return;
      }
    } else {
      if (!m_isDirectoryOnly)
        pathSet.insert(*pt);
      else
        pathSet.insert(folder);
    }

    if (!m_multiSelectionEnabled) break;
  }

  m_browser->selectNone();

  m_selectedPaths.swap(pathSet);
  if (execute()) {
    QCoreApplication::processEvents();  // <Solves a bug on XP... Bugzilla
                                        // #6041>
    accept();  // Tempted to remove the above - it should
  }            // NOT be here, maybe in the executes()...
}

//-----------------------------------------------------------------------------
/*! process without closing the browser
 */
void FileBrowserPopup::onApplyPressed() {
  TFilePath folder = m_browser->getFolder();
  std::set<TFilePath> pathSet;
  if (!m_nameField->text().isEmpty())  // if the user has written in the text
                                       // field that wins on the item
                                       // sselection!
  {
    m_selectedPaths.clear();
    if (!m_isDirectoryOnly)
      m_selectedPaths.insert(folder +
                             TFilePath(m_nameField->text().toStdString()));
    else
      m_selectedPaths.insert(TFilePath(m_nameField->text().toStdWString()));
  }

  std::set<TFilePath>::const_iterator it = m_selectedPaths.begin();
  while (it != m_selectedPaths.end()) {
    if (folder == TFilePath()) {
      // history
      if (*it == TFilePath() || !it->isAbsolute()) {
        DVGui::error(tr("Invalid file"));
        return;
      }
    } else {
      if (!m_isDirectoryOnly)
        pathSet.insert(*it);
      else
        pathSet.insert(folder);
    }
    if (!m_multiSelectionEnabled) break;
    ++it;
  }

  m_browser->selectNone();

  m_selectedPaths.swap(pathSet);
  if (executeApply()) {
    QCoreApplication::processEvents();
  }
}
//-----------------------------------------------------------------------------

void FileBrowserPopup::onFilePathClicked(const TFilePath &fp) {
  std::set<TFilePath> app;
  app.insert(fp);
  std::list<std::vector<TFrameId>> tmp;
  onFilePathsSelected(app, tmp);
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::onFilePathsSelected(
    const std::set<TFilePath> &paths,
    const std::list<std::vector<TFrameId>> &fIds) {
  if (paths.size() == 0 && m_forSaving) return;

  m_selectedPaths  = paths;
  m_currentFIdsSet = fIds;

  if (paths.size() == 1) {
    const TFilePath &fp = *paths.begin();
    QString text;
    if (!m_isDirectoryOnly)
      text = QString::fromStdWString(fp.getLevelNameW());
    else
      text = QString::fromStdWString(m_browser->getFolder().getWideString());

    m_nameField->setText(text);
  } else
    m_nameField->setText("");
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::onFilePathDoubleClicked(const TFilePath &) {
  // do nothing by default
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::setOkText(const QString &text) {
  m_okButton->setText(text);
  // if the button label is "Save" then the browser is assumed as for saving
  if (text == QObject::tr("Save")) m_forSaving = true;
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::hideEvent(QHideEvent *e) {
  TSelectionHandle::getCurrent()->popSelection();
  m_dialogSize = size();
  move(pos());
  resize(size());
}

//-----------------------------------------------------------------------------

void FileBrowserPopup::showEvent(QShowEvent *) {
  TSelectionHandle::getCurrent()->pushSelection();
  m_selectedPaths.clear();
  m_currentFIdsSet.clear();

  TFilePath projectPath = TProjectManager::instance()->getCurrentProjectPath();
  if (m_currentProjectPath != projectPath) {
    m_currentProjectPath = projectPath;
    initFolder();

    // set initial folder of all browsers to $scenefolder when the scene folder
    // mode is set in user preferences
    if (Preferences::instance()->getPathAliasPriority() ==
        Preferences::SceneFolderAlias) {
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (scene && !scene->isUntitled())
        setFolder(scene->getScenePath().getParentDir());
    }

    m_nameField->update();
    m_nameField->setFocus();
  }
  resize(m_dialogSize);
}

//-----------------------------------------------------------------------------
// utility function. Make the widget to be a child of modal file browser in
// order to allow control.

void FileBrowserPopup::setModalBrowserToParent(QWidget *widget) {
  if (!widget) return;
  for (QWidget *pwidget : QApplication::topLevelWidgets()) {
    if ((pwidget->isWindow()) && (pwidget->isModal()) &&
        (pwidget->isVisible())) {
      FileBrowserPopup *popup = qobject_cast<FileBrowserPopup *>(pwidget);
      if (popup) {
        // According to the description of QDialog;
        // "setParent() function  will clear the window flags specifying the
        // window-system properties for the widget (in particular it will reset
        // the Qt::Dialog flag)."
        // So keep the window flags and set back after calling setParent().
        Qt::WindowFlags flags = widget->windowFlags();
        widget->setParent(pwidget);
        widget->setWindowFlags(flags);
        return;
      }
    }
  }
}

//***********************************************************************************
//    GenericLoadFilePopup  implementation
//***********************************************************************************

GenericLoadFilePopup::GenericLoadFilePopup(const QString &title)
    : FileBrowserPopup(title) {}

//-----------------------------------------------------------------------------

bool GenericLoadFilePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &path = *m_selectedPaths.begin();
  assert(!path.isEmpty());  // Should always be, see
                            // FileBrowserPopup::onOk..()
  return TFileStatus(path).doesExist();
}

//-----------------------------------------------------------------------------

TFilePath GenericLoadFilePopup::getPath() {
  return (exec() == QDialog::Rejected) ? TFilePath() : *m_selectedPaths.begin();
}

//***********************************************************************************
//    GenericSaveFilePopup  implementation
//***********************************************************************************

GenericSaveFilePopup::GenericSaveFilePopup(const QString &title,
                                           QWidget *customWidget)
    : FileBrowserPopup(title, Options(FOR_SAVING), "", customWidget) {
  connect(m_nameField, SIGNAL(returnPressedNow()), m_okButton,
          SLOT(animateClick()));
}

//-----------------------------------------------------------------------------

bool GenericSaveFilePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath path(*m_selectedPaths.begin());
  assert(!path.isEmpty());

  // In case the path is not coherent with the specified type filter,
  // it means that the user specified it by typing and forgot the right type
  // (yep, even if a DIFFERENT type was specified - next time do it right :P)
  const QStringList &extList = m_browser->getFilterTypes();

  if (!m_isDirectoryOnly &&
      !extList.contains(QString::fromStdString(path.getType()))) {
    path =
        TFilePath(path.getWideString() + L"." + extList.first().toStdWString());
  }

  // Ask for user permission to overwrite if necessary
  if (!m_isDirectoryOnly && TFileStatus(path).doesExist()) {
    const QString &question =
        QObject::tr("File %1 already exists.\nDo you want to overwrite it?")
            .arg(toQString(path));

    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return false;
  }

  m_selectedPaths.clear();
  m_selectedPaths.insert(path);

  return true;
}

//-----------------------------------------------------------------------------

TFilePath GenericSaveFilePopup::getPath() {
  return (exec() == QDialog::Rejected) ? TFilePath() : *m_selectedPaths.begin();
}

//=============================================================================
// LoadScenePopup

LoadScenePopup::LoadScenePopup() : FileBrowserPopup(tr("Load Scene")) {
  setOkText(tr("Load"));
  addFilterType("tnz");
  addFilterType("xdts");

  // set the initial current path according to the current module
  setInitialFolderByCurrentRoom();

  connect(m_browser, SIGNAL(filePathDoubleClicked(const TFilePath &)), this,
          SLOT(onFilePathDoubleClicked(const TFilePath &)));
}

bool LoadScenePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (fp.getType() != "tnz" && fp.getType() != "xdts") {
    DVGui::error(toQString(fp) + tr(" is not a scene file."));
    return false;
  }

  if (!TFileStatus(fp).doesExist()) {
    DVGui::error(toQString(fp) + tr(" does not exist."));
    return false;
  }

  return IoCmd::loadScene(fp);
}

void LoadScenePopup::initFolder() { setInitialFolderByCurrentRoom(); }

void LoadScenePopup::setInitialFolderByCurrentRoom() {
  QString roomName  = TApp::instance()->getCurrentRoomName();
  TProjectP project = TProjectManager::instance()->getCurrentProject();
  TFilePath scenePath;
  if (roomName == "Cleanup" || roomName == "InknPaint")
    scenePath = project->getFolder(TProject::Drawings, true);
  else if (roomName == "PltEdit")
    scenePath = project->getFolder(TProject::Palettes, true);
  else
    scenePath = project->getFolder(TProject::Scenes, true);
  setFolder(scenePath);
}

void LoadScenePopup::showEvent(QShowEvent *e) {
  m_nameField->clear();
  FileBrowserPopup::showEvent(e);
}

void LoadScenePopup::onFilePathDoubleClicked(const TFilePath &path) {
  Q_UNUSED(path);
  onOkPressed();
}

//=============================================================================
// LoadSubScenePopup

LoadSubScenePopup::LoadSubScenePopup()
    : FileBrowserPopup(tr("Load Sub-Xsheet")) {
  setOkText(tr("Load"));
  addFilterType("tnz");
  TFilePath scenePath =
      TProjectManager::instance()->getCurrentProject()->getScenesPath();
  setFolder(scenePath);
}

bool LoadSubScenePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (fp.getType() != "tnz") {
    DVGui::error(toQString(fp) + tr(" is not a scene file."));
    return false;
  }

  if (!TFileStatus(fp).doesExist()) {
    DVGui::error(toQString(fp) + tr(" does not exist."));
    return false;
  }

  return IoCmd::loadSubScene(fp);
}

void LoadSubScenePopup::initFolder() {
  setFolder(TProjectManager::instance()->getCurrentProject()->getScenesPath());
}

void LoadSubScenePopup::showEvent(QShowEvent *e) {
  m_nameField->clear();
  FileBrowserPopup::showEvent(e);
}

//=============================================================================
// SaveSceneAsPopup

SaveSceneAsPopup::SaveSceneAsPopup()
    : FileBrowserPopup(tr("Save Scene"), Options(FOR_SAVING)) {
  setOkText(tr("Save"));
  addFilterType("tnz");
  connect(m_nameField, SIGNAL(returnPressedNow()), m_okButton,
          SLOT(animateClick()));
}

bool SaveSceneAsPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (isSpaceString(QString::fromStdString(
          fp.getName())))  // Lol. Cmon! Really necessary?
    return false;

  return IoCmd::saveScene(fp, 0);
}

void SaveSceneAsPopup::initFolder() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene->isUntitled())
    setFolder(scene->getScenePath().getParentDir());
  else
    setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());
}

//=============================================================================
// SaveSubSceneAsPopup

SaveSubSceneAsPopup::SaveSubSceneAsPopup()
    : FileBrowserPopup(tr("Sub-xsheet"), Options(FOR_SAVING)) {
  setOkText(tr("Save"));
  connect(m_nameField, SIGNAL(returnPressedNow()), m_okButton,
          SLOT(animateClick()));
}

bool SaveSubSceneAsPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  return IoCmd::saveScene(*m_selectedPaths.begin(), IoCmd::SAVE_SUBXSHEET);
}

void SaveSubSceneAsPopup::initFolder() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene->isUntitled())
    setFolder(scene->getScenePath().getParentDir());
  else
    setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());
}

//=============================================================================
// LoadLevelPopup
namespace {
QPushButton *createShowButton(QWidget *parent) {
  QPushButton *button = new QPushButton(parent);
  button->setObjectName("menuToggleButton");
  button->setFixedSize(15, 15);
  button->setIcon(createQIcon("menu_toggle"));
  button->setCheckable(true);
  button->setChecked(false);
  button->setAutoDefault(false);
  return button;
}
}  // namespace

LoadLevelPopup::LoadLevelPopup()
    : FileBrowserPopup(tr("Load Level"),
                       Options(MULTISELECTION | WITH_APPLY_BUTTON), "",
                       new QWidget(0)) {
  setModal(false);
  setOkText(tr("Load"));

  QWidget *optionWidget = (QWidget *)m_customWidget;

  // choose tlv caching behavior
  QLabel *cacheBehaviorLabel = new QLabel(tr("TLV Caching Behavior"), this);
  m_loadTlvBehaviorComboBox  = new QComboBox(this);

  //----Load Subsequence Level
  QPushButton *showSubsequenceButton = createShowButton(this);
  QLabel *subsequenceLabel = new QLabel(tr("Load Subsequence Level"), this);
  m_subsequenceFrame       = new QFrame(this);
  m_fromFrame              = new FrameNumberLineEdit(this, TFrameId(1));
  m_toFrame                = new FrameNumberLineEdit(this, TFrameId(1));

  //----Arrangement in Xsheet
  m_arrLvlPropWidget                 = new QWidget(this);
  QPushButton *showArrangementButton = createShowButton(this);
  QLabel *arrangementLabel =
      new QLabel(tr("Level Settings & Arrangement in Xsheet"), this);
  m_arrangementFrame = new QFrame(this);
  m_xFrom            = new FrameNumberLineEdit(this, TFrameId(1));
  m_xTo              = new FrameNumberLineEdit(this, TFrameId(1));
  m_stepCombo        = new QComboBox(this);
  m_incCombo         = new QComboBox(this);
  m_posFrom          = new DVGui::IntLineEdit(this, 1, 1);
  m_posTo            = new DVGui::IntLineEdit(this, 1, 1);

  //----Level Properties
  m_levelPropertiesFrame = new QFrame(this);
  m_levelName            = new DVGui::LineEdit(this);
  m_dpiWidget            = new QWidget(this);
  m_dpiPolicy            = new QComboBox(this);
  m_dpi                  = new DVGui::DoubleLineEdit(this);
  m_subsampling          = new DVGui::IntLineEdit(this, 1, 1);
  m_antialias            = new DVGui::IntLineEdit(this, 10, 0, 100);
  m_premultiply          = new DVGui::CheckBox(tr("Premultiply"), this);
  m_whiteTransp = new DVGui::CheckBox(tr("White As Transparent"), this);

  m_notExistLabel = new QLabel(tr("(FILE DOES NOT EXIST)"));

  //----
  m_loadTlvBehaviorComboBox->addItem(tr("On Demand"),
                                     IoCmd::LoadResourceArguments::ON_DEMAND);
  m_loadTlvBehaviorComboBox->addItem(tr("All Icons"),
                                     IoCmd::LoadResourceArguments::ALL_ICONS);
  m_loadTlvBehaviorComboBox->addItem(
      tr("All Icons & Images"),
      IoCmd::LoadResourceArguments::ALL_ICONS_AND_IMAGES);
  // use the default value set in the preference
  m_loadTlvBehaviorComboBox->setCurrentIndex(
      m_loadTlvBehaviorComboBox->findData(
          Preferences::instance()->getInitialLoadTlvCachingBehavior()));
  cacheBehaviorLabel->setObjectName("TitleTxtLabel");

  //----Load Subsequence Level
  m_subsequenceFrame->setObjectName("LoadLevelFrame");
  subsequenceLabel->setObjectName("TitleTxtLabel");
  m_subsequenceFrame->hide();
  m_fromFrame->setMaximumWidth(50);
  m_toFrame->setMaximumWidth(50);

  //----Arrangement in Xsheet
  m_arrangementFrame->setObjectName("LoadLevelFrame");
  m_levelPropertiesFrame->setObjectName("LoadLevelFrame");
  arrangementLabel->setObjectName("TitleTxtLabel");
  m_arrLvlPropWidget->hide();

  QStringList sList;
  sList << QString("Auto") << QString("1") << QString("2") << QString("3")
        << QString("4") << QString("5") << QString("6") << QString("7")
        << QString("8");
  m_stepCombo->addItems(sList);
  m_incCombo->addItems(sList);

  //----Level Properties
  m_dpiPolicy->addItem(QObject::tr("Image DPI"), LevelOptions::DP_ImageDpi);
  m_dpiPolicy->addItem(QObject::tr("Custom DPI"), LevelOptions::DP_CustomDpi);
  m_dpi->setRange(1, (std::numeric_limits<double>::max)());
  m_dpi->setFixedWidth(54);

  // initialize with the default value
  LevelOptions options;
  setLevelProperties(options);

  //"FILE DOES NOT EXIST" label
  m_notExistLabel->setObjectName("FileDoesNotExistLabel");
  m_notExistLabel->hide();

  //----layout
  auto createVBoxLayout = [](int margin, int spacing) {
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setMargin(margin);
    layout->setSpacing(spacing);
    return layout;
  };
  auto createHBoxLayout = [](int margin, int spacing) {
    QHBoxLayout *layout = new QHBoxLayout();
    layout->setMargin(margin);
    layout->setSpacing(spacing);
    return layout;
  };

  QVBoxLayout *mainLayout = createVBoxLayout(5, 3);
  {
    QHBoxLayout *cacheLay = createHBoxLayout(0, 5);
    {
      cacheLay->addStretch(1);
      cacheLay->addWidget(cacheBehaviorLabel, 0);
      cacheLay->addWidget(m_loadTlvBehaviorComboBox, 0);
    }
    mainLayout->addLayout(cacheLay, 0);

    //----Load Subsequence Level

    QHBoxLayout *subsequenceHeadLay = createHBoxLayout(0, 5);
    {
      QFontMetrics metrics(font());
      subsequenceHeadLay->addSpacing(metrics.width("File name:") + 3);
      subsequenceHeadLay->addWidget(m_notExistLabel, 0);
      subsequenceHeadLay->addStretch(1);

      subsequenceHeadLay->addWidget(subsequenceLabel, 0);
      subsequenceHeadLay->addWidget(showSubsequenceButton, 0);
    }
    mainLayout->addLayout(subsequenceHeadLay, 0);

    QHBoxLayout *subsequenceLay = createHBoxLayout(5, 5);
    {
      subsequenceLay->addWidget(new QLabel(tr("From:"), this), 0);
      subsequenceLay->addWidget(m_fromFrame, 0);
      subsequenceLay->addWidget(new QLabel(tr(" To:"), this), 0);
      subsequenceLay->addWidget(m_toFrame, 0);
    }
    m_subsequenceFrame->setLayout(subsequenceLay);
    mainLayout->addWidget(m_subsequenceFrame, 0,
                          Qt::AlignRight | Qt::AlignVCenter);

    //----Arrangement in Xsheet

    QHBoxLayout *arrangementHeadLay = createHBoxLayout(0, 3);
    {
      arrangementHeadLay->addWidget(arrangementLabel, 1,
                                    Qt::AlignRight | Qt::AlignVCenter);
      arrangementHeadLay->addWidget(showArrangementButton, 0);
    }
    mainLayout->addLayout(arrangementHeadLay);

    QHBoxLayout *bottomLay = createHBoxLayout(0, 10);
    {
      QGridLayout *levelLay = new QGridLayout();
      levelLay->setMargin(5);
      levelLay->setSpacing(5);
      {
        levelLay->addWidget(new QLabel(tr("Level Name:"), this), 0, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        levelLay->addWidget(m_levelName, 0, 1);
        QHBoxLayout *dpiLay = createHBoxLayout(0, 5);
        {
          dpiLay->addSpacing(10);
          dpiLay->addWidget(new QLabel(tr("DPI:"), this), 0,
                            Qt::AlignRight | Qt::AlignVCenter);
          dpiLay->addWidget(m_dpiPolicy, 0);
          dpiLay->addWidget(m_dpi, 1);
        }
        m_dpiWidget->setLayout(dpiLay);
        levelLay->addWidget(m_dpiWidget, 0, 2, 1, 3);

        levelLay->addWidget(m_premultiply, 1, 0, 1, 2);
        levelLay->addWidget(m_whiteTransp, 2, 0, 1, 2);

        // levelLay->addWidget(m_doAntialias, 1, 3, 1, 2);
        levelLay->addWidget(new QLabel(tr("Antialias Softness:"), this), 1, 2,
                            1, 2, Qt::AlignRight | Qt::AlignVCenter);
        levelLay->addWidget(m_antialias, 1, 4);
        levelLay->addWidget(new QLabel(tr("Subsampling:"), this), 2, 2, 1, 2,
                            Qt::AlignRight | Qt::AlignVCenter);
        levelLay->addWidget(m_subsampling, 2, 4);
      }
      levelLay->setColumnStretch(1, 1);
      levelLay->setColumnStretch(4, 1);
      m_levelPropertiesFrame->setLayout(levelLay);
      bottomLay->addWidget(m_levelPropertiesFrame, 0);

      QGridLayout *arrLay = new QGridLayout();
      arrLay->setMargin(5);
      arrLay->setSpacing(5);
      {
        arrLay->addWidget(new QLabel(tr("From:"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
        arrLay->addWidget(m_xFrom, 0, 1);
        arrLay->addWidget(new QLabel(tr(" To:"), this), 0, 2,
                          Qt::AlignRight | Qt::AlignVCenter);
        arrLay->addWidget(m_xTo, 0, 3);
        arrLay->addWidget(new QLabel(tr(" Step:"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
        arrLay->addWidget(m_stepCombo, 1, 1);
        arrLay->addWidget(new QLabel(tr(" Inc:"), this), 1, 2,
                          Qt::AlignRight | Qt::AlignVCenter);
        arrLay->addWidget(m_incCombo, 1, 3);
        arrLay->addWidget(new QLabel(tr(" Frames:"), this), 2, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
        arrLay->addWidget(m_posFrom, 2, 1);
        arrLay->addWidget(new QLabel(tr("::"), this), 2, 2, Qt::AlignCenter);
        arrLay->addWidget(m_posTo, 2, 3);
      }
      arrLay->setColumnStretch(1, 1);
      arrLay->setColumnStretch(3, 1);
      m_arrangementFrame->setLayout(arrLay);
      bottomLay->addWidget(m_arrangementFrame, 0);
    }
    m_arrLvlPropWidget->setLayout(bottomLay);

    mainLayout->addWidget(m_arrLvlPropWidget, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
  }
  optionWidget->setLayout(mainLayout);

  //----signal-slot connections
  //----Load Subsequence Level
  connect(showSubsequenceButton, SIGNAL(toggled(bool)), m_subsequenceFrame,
          SLOT(setVisible(bool)));
  connect(m_fromFrame, SIGNAL(editingFinished()),
          SLOT(onSubsequentFrameChanged()));
  connect(m_toFrame, SIGNAL(editingFinished()),
          SLOT(onSubsequentFrameChanged()));

  //----Arrangement in Xsheet
  connect(showArrangementButton, SIGNAL(toggled(bool)), m_arrLvlPropWidget,
          SLOT(setVisible(bool)));
  connect(m_xFrom, SIGNAL(editingFinished()), SLOT(updatePosTo()));
  connect(m_xTo, SIGNAL(editingFinished()), SLOT(updatePosTo()));
  connect(m_posFrom, SIGNAL(editingFinished()), SLOT(updatePosTo()));
  connect(m_stepCombo, SIGNAL(currentIndexChanged(int)), SLOT(updatePosTo()));
  connect(m_incCombo, SIGNAL(currentIndexChanged(int)), SLOT(updatePosTo()));

  connect(m_nameField, SIGNAL(editingFinished()), this,
          SLOT(onNameSetEditted()));
  connect(m_browser, SIGNAL(treeFolderChanged(const TFilePath &)), this,
          SLOT(onNameSetEditted()));
  connect(m_browser, SIGNAL(filePathDoubleClicked(const TFilePath &)), this,
          SLOT(onFilePathDoubleClicked(const TFilePath &)));
  //----Level Properties
  connect(m_dpiPolicy, SIGNAL(activated(int)), this,
          SLOT(onDpiPolicyActivated()));
  connect(m_premultiply, SIGNAL(clicked(bool)), this,
          SLOT(onDoPremultiplyClicked()));
  connect(m_whiteTransp, SIGNAL(clicked(bool)), this,
          SLOT(onWhiteTranspClicked()));
}

//-----------------------------------------------------------------------

void LoadLevelPopup::onNameSetEditted() {
  getCurrentPathSet().clear();
  getCurrentFIdsSet().clear();

  // if nothing input
  if (m_nameField->text() == "") {
    m_notExistLabel->hide();
    updateBottomGUI();
  }
  // if the path exists
  else {
    TFilePath path =
        m_browser->getFolder() + TFilePath(m_nameField->text().toStdString());
    getCurrentPathSet().insert(path);
    if (TSystem::doesExistFileOrLevel(path)) {
      m_notExistLabel->hide();
      updateBottomGUI();
    } else {
      m_notExistLabel->show();

      m_fromFrame->setValue(TFrameId(1));
      m_toFrame->setValue(TFrameId(1));
      m_subsequenceFrame->setEnabled(true);

      m_xFrom->setValue(TFrameId(1));
      m_xTo->setValue(TFrameId(1));

      m_levelName->setText(QString::fromStdString(path.getName()));

      m_arrangementFrame->setEnabled(true);
      m_levelName->setEnabled(true);
      m_levelPropertiesFrame->setEnabled(true);
      updatePosTo();
    }
  }

  update();
}

//-----------------------------------------------------------------------

void LoadLevelPopup::updatePosTo() {
  // calculate how many frames to be occupied in the xsheet
  TFilePath fp = getCurrentPath();

  if (QString::fromStdString(fp.getType()) == "tpl") {
    m_posTo->setText(m_posFrom->text());
    return;
  }

  TFrameId xFrom = m_xFrom->getValue();
  TFrameId xTo   = m_xTo->getValue();

  int frameLength;

  bool isScene = (QString::fromStdString(fp.getType()) == "tnz");

  if (isScene) {  // scene does not consider frame suffixes
    xFrom = TFrameId(xFrom.getNumber());
    xTo   = TFrameId(xTo.getNumber());
  }

  auto frameLengthBetweenFIds = [&]() {
    if (xFrom > xTo) return 0;
    int ret = xTo.getNumber() - xFrom.getNumber() + 1;
    if (!xTo.getLetter().isEmpty()) ret++;
    return ret;
  };

  //--- if loading the "missing" level
  if (m_notExistLabel->isVisible()) {
    int inc = m_incCombo->currentIndex();
    if (inc == 0)  // Inc = Auto
    {
      frameLength =
          frameLengthBetweenFIds() * ((m_stepCombo->currentIndex() == 0)
                                          ? 1
                                          : m_stepCombo->currentIndex());

    } else  // Inc =! Auto
    {
      int loopAmount;
      loopAmount  = tceil((double)(frameLengthBetweenFIds()) / (double)inc);
      frameLength = loopAmount * ((m_stepCombo->currentIndex() == 0)
                                      ? inc
                                      : m_stepCombo->currentIndex());
    }
  }

  //-- if loading the existing level
  else if (m_incCombo->currentIndex() == 0)  // Inc = Auto
  {
    if (isScene) {
      frameLength = frameLengthBetweenFIds();
    } else {
      std::vector<TFrameId> fIds = getCurrentFIds();
      //--- If loading the level with sequential files, reuse the list of
      // TFrameId
      if (fIds.size() != 0) {
        if (m_stepCombo->currentIndex() == 0)  // Step = Auto
        {
          frameLength = 0;
          std::vector<TFrameId>::iterator it;
          for (it = fIds.begin(); it != fIds.end(); it++) {
            if (xFrom <= *it && *it <= xTo)
              frameLength++;
            else if (xTo < *it)
              break;
          }
        } else  // Step != Auto
        {
          std::vector<TFrameId>::iterator it;
          int loopAmount = 0;
          for (it = fIds.begin(); it != fIds.end(); it++) {
            if (xFrom <= *it && *it <= xTo) loopAmount++;
          }
          frameLength = loopAmount * m_stepCombo->currentIndex();
        }
      }
      // loading another type of level such as tlv
      else {
        if (fp.isEmpty()) return;
        try {
          TLevelReaderP lr(fp);
          TLevelP level;
          if (lr) level = lr->loadInfo();
          if (!level.getPointer()) return;

          if (m_stepCombo->currentIndex() == 0)  // Step = Auto
          {
            frameLength = 0;
            TLevel::Iterator it;
            for (it = level->begin(); it != level->end(); it++) {
              if (xFrom <= it->first && it->first <= xTo)
                frameLength++;
              else if (xTo < it->first)
                break;
            }
          } else  // Step != Auto
          {
            TLevel::Iterator it;
            int loopAmount = 0;
            for (it = level->begin(); it != level->end(); it++) {
              if (xFrom <= it->first && it->first <= xTo) loopAmount++;
            }
            frameLength = loopAmount * m_stepCombo->currentIndex();
          }
        } catch (...) {
          return;
        }
      }
    }
  }
  // Inc != Auto
  else {
    int inc = m_incCombo->currentIndex();
    int loopAmount;
    loopAmount  = tceil((double)(frameLengthBetweenFIds()) / (double)inc);
    frameLength = loopAmount * ((m_stepCombo->currentIndex() == 0)
                                    ? inc
                                    : m_stepCombo->currentIndex());
  }

  m_posTo->setText(
      QString::number(m_posFrom->text().toInt() + frameLength - 1));
}
//-----------------------------------------------------------------------
/*! if the from / to values in the subsequent box, update m_xFrom and m_xTo
 */
void LoadLevelPopup::onSubsequentFrameChanged() {
  m_xFrom->setValue(m_fromFrame->getValue());
  m_xTo->setValue(m_toFrame->getValue());
  updatePosTo();
}

//-----------------------------------------------------------------------

void LoadLevelPopup::showEvent(QShowEvent *e) {
  m_nameField->clear();

  FileBrowserPopup::showEvent(e);

  bool ret         = true;
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  ret              = ret &&
        connect(fh, SIGNAL(frameSwitched()), this, SLOT(onFrameSwitched()));
  ret = ret &&
        connect(fh, SIGNAL(frameTypeChanged()), this, SLOT(onFrameSwitched()));

  TSelectionHandle *sh = TApp::instance()->getCurrentSelection();
  ret = ret && connect(sh, SIGNAL(selectionChanged(TSelection *)), this,
                       SLOT(onSelectionChanged(TSelection *)));
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(preferenceChanged(const QString &)), this,
                       SLOT(onPreferenceChanged(const QString &)));
  assert(ret);

  onFrameSwitched();
  onSelectionChanged(sh->getSelection());
  onPreferenceChanged("");
  onNameSetEditted();  // clear currentPathSet
}

//-----------------------------------------------------------------------

void LoadLevelPopup::hideEvent(QHideEvent *e) {
  FileBrowserPopup::hideEvent(e);

  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  disconnect(fh, SIGNAL(frameSwitched()), this, SLOT(onFrameSwitched()));
  disconnect(fh, SIGNAL(frameTypeChanged()), this, SLOT(onFrameSwitched()));

  TSelectionHandle *sh = TApp::instance()->getCurrentSelection();
  disconnect(sh, SIGNAL(selectionChanged(TSelection *)), this,
             SLOT(onSelectionChanged(TSelection *)));
  disconnect(TApp::instance()->getCurrentScene(),
             SIGNAL(preferenceChanged(const QString &)), this,
             SLOT(onPreferenceChanged(const QString &)));
}

//-----------------------------------------------------------------------

void LoadLevelPopup::onFrameSwitched() {
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  if (fh->isEditingLevel())
    m_posFrom->setText("1");
  else
    m_posFrom->setText(QString::number(fh->getFrame() + 1));
  updatePosTo();
}

//-----------------------------------------------------------------------

bool LoadLevelPopup::execute() {
  if (m_selectedPaths.size() == 1) {
    const TFilePath &fp = *m_selectedPaths.begin();
    //---- SubSequent load
    // if loading the "missing" level
    if (m_notExistLabel->isVisible()) {
      TFrameId firstLoadingFId = m_fromFrame->getValue();
      TFrameId lastLoadingFId  = m_toFrame->getValue();
      setLoadingLevelRange(firstLoadingFId, lastLoadingFId);
    } else if (m_subsequenceFrame->isEnabled() &&
               m_subsequenceFrame->isVisible()) {
      std::vector<TFrameId> fIds = getCurrentFIds();
      TFrameId firstFrame;
      TFrameId lastFrame;
      // if the level is sequential and there is a reusable list of TFrameId
      if (fIds.size() != 0) {
        firstFrame = fIds[0];
        lastFrame  = fIds[fIds.size() - 1];
      }
      // another case such as loading tlv
      else {
        try {
          TLevelReaderP lr(fp);
          TLevelP level;
          if (lr) level = lr->loadInfo();
          if (!level.getPointer()) return false;

          firstFrame = level->begin()->first;
          lastFrame  = (--level->end())->first;
          lr         = TLevelReaderP();
        } catch (...) {
          return false;
        }
      }
      TFrameId firstLoadingFId = m_fromFrame->getValue();
      TFrameId lastLoadingFId  = m_toFrame->getValue();
      if (firstFrame != firstLoadingFId || lastFrame != lastLoadingFId)
        setLoadingLevelRange(firstLoadingFId, lastLoadingFId);
    }

    IoCmd::LoadResourceArguments args(fp);

    args.row0 = m_posFrom->text().toInt() - 1;

    args.frameCount = m_posTo->text().toInt() - m_posFrom->text().toInt() + 1;

    if ((int)getCurrentFIdsSet().size() != 0)
      args.frameIdsSet.push_back(*getCurrentFIdsSet().begin());

    else if (m_notExistLabel->isVisible()) {
      TFrameId firstLoadingFId = m_fromFrame->getValue();
      TFrameId lastLoadingFId  = m_toFrame->getValue();
      // putting the Fids in order to avoid LoadInfo later
      std::vector<TFrameId> tmp_fids;
      int i = firstLoadingFId.getNumber();
      if (!firstLoadingFId.getLetter().isEmpty()) {
        tmp_fids.push_back(firstLoadingFId);
        i++;
      }
      for (; i <= lastLoadingFId.getNumber(); i++) {
        tmp_fids.push_back(TFrameId(i));
      }
      if (!lastLoadingFId.getLetter().isEmpty())
        tmp_fids.push_back(lastLoadingFId);
      args.frameIdsSet.push_back(tmp_fids);
    }

    TFrameId xFrom = m_xFrom->getValue();
    if (!xFrom.isEmptyFrame()) args.xFrom = xFrom;
    TFrameId xTo = m_xTo->getValue();
    if (!xTo.isEmptyFrame()) args.xTo = xTo;

    args.levelName             = m_levelName->text().toStdWString();
    args.step                  = m_stepCombo->currentIndex();
    args.inc                   = m_incCombo->currentIndex();
    args.doesFileActuallyExist = !m_notExistLabel->isVisible();
    args.cachingBehavior       = IoCmd::LoadResourceArguments::CacheTlvBehavior(
        m_loadTlvBehaviorComboBox->currentData().toInt());

    if (m_arrLvlPropWidget->isVisible() &&
        m_levelPropertiesFrame->isEnabled()) {
      for (IoCmd::LoadResourceArguments::ResourceData &rd :
           args.resourceDatas) {
        rd.m_options = LevelOptions();
        getLevelProperties(*rd.m_options);
      }
    }

    return 0 < IoCmd::loadResources(args, true, 0);

  } else {
    std::set<TFilePath>::const_iterator it;
    IoCmd::LoadResourceArguments args;

    args.row0 = m_posFrom->text().toInt() - 1;

    for (it = m_selectedPaths.begin(); it != m_selectedPaths.end(); ++it)
      args.resourceDatas.push_back(*it);

    std::list<std::vector<TFrameId>> fIdsSet = getCurrentFIdsSet();
    if (fIdsSet.size() > 0) {
      std::list<std::vector<TFrameId>>::const_iterator fIdIt;
      for (fIdIt = fIdsSet.begin(); fIdIt != fIdsSet.end(); ++fIdIt)
        args.frameIdsSet.insert(args.frameIdsSet.begin(), *fIdIt);
    }

    args.cachingBehavior = IoCmd::LoadResourceArguments::CacheTlvBehavior(
        m_loadTlvBehaviorComboBox->currentData().toInt());

    if (m_arrLvlPropWidget->isVisible() &&
        m_levelPropertiesFrame->isEnabled()) {
      for (IoCmd::LoadResourceArguments::ResourceData &rd :
           args.resourceDatas) {
        rd.m_options = LevelOptions();
        getLevelProperties(*rd.m_options);
      }
    }

    return 0 < IoCmd::loadResources(args, true, 0);
  }
}

//-----------------------------------------------------------------------

void LoadLevelPopup::initFolder() {
  TFilePath fp;

  TProject *project =
      TProjectManager::instance()->getCurrentProject().getPointer();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  if (scene) fp = scene->decodeFilePath(project->getProjectFolder());

  setFolder(fp);
  onFilePathsSelected(getCurrentPathSet(), getCurrentFIdsSet());
}

//----------------------------------------------------------------------------

void LoadLevelPopup::onFilePathDoubleClicked(const TFilePath &path) {
  Q_UNUSED(path);
  onOkPressed();
}

//----------------------------------------------------------------------------

void LoadLevelPopup::onFilePathsSelected(
    const std::set<TFilePath> &paths,
    const std::list<std::vector<TFrameId>> &fIds) {
  m_notExistLabel->hide();
  FileBrowserPopup::onFilePathsSelected(paths, fIds);
  updateBottomGUI();
}

//----------------------------------------------------------------------------

void LoadLevelPopup::updateBottomGUI() {
  auto disableAll = [&]() {
    m_fromFrame->setText("");
    m_toFrame->setText("");
    m_subsequenceFrame->setEnabled(false);

    m_xFrom->setText("");
    m_xTo->setText("");
    m_levelName->setText("");
    m_posTo->setText("");
    m_arrangementFrame->setEnabled(false);
    m_levelPropertiesFrame->setEnabled(false);
  };

  std::set<TFilePath> paths                = getCurrentPathSet();
  std::list<std::vector<TFrameId>> fIdsSet = getCurrentFIdsSet();

  if (paths.empty() || paths.size() > 1) {
    disableAll();
    if (paths.size() > 1) {
      m_levelName->setEnabled(false);
      m_levelPropertiesFrame->setEnabled(true);
    }
    return;
  }

  TFilePath fp               = getCurrentPath();
  std::vector<TFrameId> fIds = getCurrentFIds();

  QString ext = QString::fromStdString(fp.getType());

  // initialize
  if (fp.isEmpty() || ext == "") {
    disableAll();
    return;
  } else if (ext == "tpl") {
    m_fromFrame->setText("1");
    m_toFrame->setText("1");
    m_subsequenceFrame->setEnabled(false);

    m_xFrom->setText("1");
    m_xTo->setText("1");
    m_levelName->setText(QString::fromStdString(fp.getName()));
    m_posTo->setText(m_posFrom->text());
    m_arrangementFrame->setEnabled(false);
    m_levelPropertiesFrame->setEnabled(false);
  } else if (ext == "tnz") {
    ToonzScene scene;
    scene.setScenePath(fp);
    int sceneLength = scene.getFrameCount();
    QString str;
    m_fromFrame->setText(str.number(std::min(1, sceneLength)));
    m_toFrame->setText(str.number(sceneLength));
    m_subsequenceFrame->setEnabled(false);

    m_xFrom->setText(m_fromFrame->text());
    m_xTo->setText(m_toFrame->text());
    m_levelName->setText(QString::fromStdString(fp.getName()));
    m_stepCombo->setCurrentIndex(0);
    m_incCombo->setCurrentIndex(0);
    m_arrangementFrame->setEnabled(false);
    m_levelPropertiesFrame->setEnabled(false);
  } else {
    TFrameId firstFrame;
    TFrameId lastFrame;
    // for level with sequential frames
    if (fIds.size() != 0) {
      firstFrame = fIds[0];
      lastFrame  = fIds[fIds.size() - 1];
    } else {
      try {
        TLevelReaderP lr(fp);
        TLevelP level;
        if (lr) level = lr->loadInfo();
        if (!level.getPointer() || level->getTable()->size() == 0) return;

        firstFrame = level->begin()->first;
        lastFrame  = (--level->end())->first;
      } catch (...) {
        disableAll();
        return;
      }
    }
    m_fromFrame->setValue(firstFrame);
    m_toFrame->setValue(lastFrame);
    m_subsequenceFrame->setEnabled(true);

    m_xFrom->setValue(firstFrame);
    m_xTo->setValue(lastFrame);

    // if some option in the preferences is selected, load the level with
    // removing
    // six letters of the scene name from the level name
    m_levelName->setText(getLevelNameWithoutSceneNumber(fp.getName()));

    // If the option "Show "ABC" Appendix to the Frame Number in Xsheet Cell" is
    // ON, frame numbers normally increment at interval of 10.
    // Placing such level with "Auto" step option will cause unwanted
    // spacing between frames in Xsheet. Setting the step to "1" can prevent
    // such problem.
    if (Preferences::instance()->isShowFrameNumberWithLettersEnabled() &&
        m_stepCombo->currentIndex() == 0)
      m_stepCombo->setCurrentIndex(1);

    m_arrangementFrame->setEnabled(true);

    m_levelName->setEnabled(true);
    m_levelPropertiesFrame->setEnabled(true);
  }
  updatePosTo();
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
/*! if some option in the preferences is selected, load the level with removing
        six letters of the scene name from the level name
*/
QString LoadLevelPopup::getLevelNameWithoutSceneNumber(std::string orgName) {
  QString levelOrgName = QString::fromStdString(orgName);

  // check out the preference
  if (!Preferences::instance()->isRemoveSceneNumberFromLoadedLevelNameEnabled())
    return levelOrgName;

  // do nothing if the level name has less than 7 letters
  if (levelOrgName.size() <= 6) return levelOrgName;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return levelOrgName;

  QString sceneName = QString::fromStdWString(scene->getSceneName()).left(5);

  // if the first 5 letters are same a the scene name, then remove the letters
  // to the under score
  // this code is intended to cover both the case "c0001_hogehoge.tif" and
  // "c0001A_tif"
  if (!levelOrgName.startsWith(sceneName)) return levelOrgName;

  if (!levelOrgName.contains("_")) return levelOrgName;

  return levelOrgName.right(levelOrgName.size() - levelOrgName.indexOf("_") -
                            1);
}

//----------------------------------------------------------------------------
/*! if the x-sheet cells are selected, load levels at the upper-left corner of
 * the selection
 */
void LoadLevelPopup::onSelectionChanged(TSelection *selection) {
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);

  if (!cellSelection) return;

  int r0, r1, c0, c1;
  cellSelection->getSelectedCells(r0, c0, r1, c1);

  m_posFrom->setText(QString::number(r0 + 1));

  updatePosTo();
}

//----------------------------------------------------------------------------

void LoadLevelPopup::onPreferenceChanged(const QString &propertyName) {
  if (!propertyName.isEmpty() && propertyName != "pixelsOnly") return;
  bool pixelsMode = Preferences::instance()->getBoolValue(pixelsOnly);
  m_dpiWidget->setHidden(pixelsMode);
  if (pixelsMode) {
    m_dpiPolicy->setCurrentIndex(
        m_dpiPolicy->findData(LevelOptions::DP_ImageDpi));
    m_dpi->setValue(Stage::standardDpi);
    m_dpi->setEnabled(false);
  }
}

//----------------------------------------------------------------------------

void LoadLevelPopup::setLevelProperties(LevelOptions &options) {
  m_dpiPolicy->setCurrentIndex(m_dpiPolicy->findData(options.m_dpiPolicy));
  m_dpi->setValue(options.m_dpi);
  m_subsampling->setValue(options.m_subsampling);
  m_antialias->setValue(options.m_antialias);
  m_whiteTransp->setChecked(options.m_whiteTransp);
  m_premultiply->setChecked(options.m_premultiply);
  onDpiPolicyActivated();
}

//----------------------------------------------------------------------------

void LoadLevelPopup::getLevelProperties(LevelOptions &options) {
  options.m_dpiPolicy =
      LevelOptions::DpiPolicy(m_dpiPolicy->currentData().toInt());
  options.m_dpi         = m_dpi->getValue();
  options.m_subsampling = m_subsampling->getValue();
  options.m_antialias   = m_antialias->getValue();
  options.m_whiteTransp = m_whiteTransp->isChecked();
  options.m_premultiply = m_premultiply->isChecked();
}

//----------------------------------------------------------------------------

void LoadLevelPopup::onDpiPolicyActivated() {
  m_dpi->setEnabled(m_dpiPolicy->currentData().toInt() ==
                    LevelOptions::DP_CustomDpi);
}

//----------------------------------------------------------------------------
// exclusive with the whiteTransp option
void LoadLevelPopup::onDoPremultiplyClicked() {
  if (m_whiteTransp->isChecked()) m_whiteTransp->setChecked(false);
}

//----------------------------------------------------------------------------
// exclusive with the doPremultiply option
void LoadLevelPopup::onWhiteTranspClicked() {
  if (m_premultiply->isChecked()) m_premultiply->setChecked(false);
}

//=============================================================================
// SaveLevelAsPopup

SaveLevelAsPopup::SaveLevelAsPopup()
    : FileBrowserPopup(tr("Save Level"), Options(FOR_SAVING)) {
  setOkText(tr("Save"));
  connect(m_nameField, SIGNAL(returnPressedNow()), m_okButton,
          SLOT(animateClick()));
}

bool SaveLevelAsPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath &fp = (TFilePath &)*m_selectedPaths.begin();

  if (isSpaceString(QString::fromStdString(fp.getName()))) return false;

  // pointer to be replaced
  TXshLevel *levelToBeReplaced =
      TApp::instance()->getCurrentLevel()->getLevel();
  TXsheet *xsh       = TApp::instance()->getCurrentXsheet()->getXsheet();
  int curColumnIndex = TApp::instance()->getCurrentColumn()->getColumnIndex();

  bool ret = IoCmd::saveLevel(fp);

  // ask whether to expose the saved level in xsheet
  bool doExpose = true;
  if (levelToBeReplaced->getType() & FULLCOLOR_TYPE)
    doExpose = false;
  else if (ret &&
           !Preferences::instance()->isReplaceAfterSaveLevelAsEnabled()) {
    QString question(QObject::tr("Do you want to expose the renamed level ?"));
    int val = DVGui::MsgBox(question,
                            QObject::tr("Expose"),            // val = 1
                            QObject::tr("Don't expose"), 0);  // val = 2
    if (val == 0) return false;                               // close button
    if (val == 2) doExpose = false;
  }

  // exposing the level
  if (ret && doExpose) {
    // if the extensions are missing, add them here
    TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(
        TApp::instance()->getCurrentLevel()->getLevel());
    if (!sl) return false;
    std::string ext = sl->getPath().getType();
    if (fp.getType() == "") fp = fp.withType(ext);

    IoCmd::LoadResourceArguments args(fp);
    args.expose = false;
    int count   = IoCmd::loadResources(args);
    if (count == 0) return false;
    TXshLevelP xl = args.loadedLevels[0];

    // in case of replacing with a saved file
    if (Preferences::instance()->isReplaceAfterSaveLevelAsEnabled()) {
      for (int c = 0; c < xsh->getColumnCount(); c++) {
        if (xsh->isColumnEmpty(c)) continue;
        int r0, r1;
        xsh->getCellRange(c, r0, r1);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, c);
          if (cell.m_level.getPointer() != levelToBeReplaced) continue;
          cell.m_level = xl;
          xsh->setCell(r, c, cell);
        }
      }
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

      //--- update the scene cast
      TLevelSet *levelSet =
          TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
      for (int i = 0; i < levelSet->getLevelCount(); i++) {
        TXshLevel *tmpLevel = levelSet->getLevel(i);
        if (tmpLevel != levelToBeReplaced) continue;
        if (!TApp::instance()
                 ->getCurrentScene()
                 ->getScene()
                 ->getTopXsheet()
                 ->isLevelUsed(tmpLevel))
          levelSet->removeLevel(tmpLevel);
      }

    }
    // In case of loading the saved file into a vacant column
    else {
      // find the leftmost empty column
      int emptyColumnIndex = xsh->getFirstFreeColumnIndex();

      // if the scene frame is selected and the old level is found in the
      // current column,
      // then place the new level at the same frame with the old one
      if (TApp::instance()->getCurrentFrame()->isEditingScene()) {
        // check out the current column
        int r0, r1;
        xsh->getCellRange(curColumnIndex, r0, r1);

        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, curColumnIndex);
          if (!cell.isEmpty() &&
              cell.m_level.getPointer() == levelToBeReplaced) {
            // set the new level at the same frame with the old one
            cell.m_level = xl;
            xsh->setCell(r, emptyColumnIndex, cell);
          }
        }
      }
      // if the level editing mode, then just place the new level
      else {
        std::vector<TFrameId> dummy_fIds;
        xsh->exposeLevel(0, emptyColumnIndex, xl.getPointer(), dummy_fIds);
      }
    }

    TApp::instance()->getCurrentLevel()->setLevel(xl.getPointer());

    TApp::instance()->getCurrentScene()->notifyCastChange();
    TApp::instance()->getCurrentLevel()->notifyLevelChange();

    DvDirModel::instance()->refreshFolder(fp.getParentDir());

    // reset undo memory!!
    if (Preferences::instance()->getBoolValue(resetUndoOnSavingLevel))
      TUndoManager::manager()->reset();
  }

  if (ret)
    std::cout << "SaveLevelAs complete." << std::endl;
  else
    std::cout << "SaveLevelAs failed for some reason." << std::endl;

  return ret;
}

void SaveLevelAsPopup::initFolder() {
  TProject *project =
      TProjectManager::instance()->getCurrentProject().getPointer();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp;
  if (scene) fp = scene->decodeFilePath(project->getFolder(TProject::Drawings));
  setFolder(fp);
}

//---------------------------------------------------------------------------
/*
  For Save Level As command, it is needed to check if the current level is
  selected (just like Save Level command) BEFORE opening the popup. So I decided
  to use an original MenuItemHandler rather than OpenPopupCommandHandler.
  06/07/2016 Shun
*/

class SaveLevelAsCommandHandler final : public MenuItemHandler {
  SaveLevelAsPopup *m_popup;

public:
  SaveLevelAsCommandHandler() : MenuItemHandler(MI_SaveLevelAs), m_popup(0) {}
  void execute() override {
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl) {
      DVGui::warning(QObject::tr("No Current Level"));
      return;
    }
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) {
      DVGui::warning(QObject::tr("No Current Scene"));
      return;
    }
    if (!m_popup) m_popup = new SaveLevelAsPopup();
    m_popup->show();
    m_popup->raise();
    m_popup->activateWindow();
  }
} saveLevelAsCommandHandler;

//=============================================================================
// Used both for ReplaceLevelPopup and ReplaceParentDirectoryPopup
// which are needed to maintain the current selection on open.

template <class T>
class OpenReplaceFilePopupHandler final : public MenuItemHandler {
  T *m_popup;

public:
  OpenReplaceFilePopupHandler(CommandId cmdId)
      : MenuItemHandler(cmdId), m_popup(0) {}

  // The current selection is cleared on the construction of FileBrowser
  // ( by calling makeCurrent() in DvDirTreeView::currentChanged() ).
  // Thus checking the selection must be done BEFORE making the browser
  // or it fails to get the selection on the first call of this command.
  bool initialize(TCellSelection::Range &range, std::set<int> &columnRange,
                  bool &replaceCells) {
    TSelection *sel = TApp::instance()->getCurrentSelection()->getSelection();
    if (!sel) return false;
    TCellSelection *cellSel     = dynamic_cast<TCellSelection *>(sel);
    TColumnSelection *columnSel = dynamic_cast<TColumnSelection *>(sel);
    if ((!cellSel && !columnSel) || sel->isEmpty()) {
      DVGui::error(
          QObject::tr("Nothing to replace: no cells or columns selected."));
      return false;
    }
    if (cellSel) {
      range        = cellSel->getSelectedCells();
      replaceCells = true;
    } else if (columnSel) {
      columnRange  = columnSel->getIndices();
      replaceCells = false;
    }
    return true;
  }

  void execute() override {
    TCellSelection::Range range;
    std::set<int> columnRange;
    bool replaceCells;
    if (!initialize(range, columnRange, replaceCells)) return;
    if (!m_popup) m_popup = new T();
    m_popup->setRange(range, columnRange, replaceCells);
    m_popup->show();
    m_popup->raise();
    m_popup->activateWindow();
  }
};

//=============================================================================
// ReplaceLevelPopup

ReplaceLevelPopup::ReplaceLevelPopup()
    : FileBrowserPopup(tr("Replace Level"), Options(WITH_APPLY_BUTTON)) {
  setOkText(tr("Replace"));
  connect(TApp::instance()->getCurrentSelection(),
          SIGNAL(selectionChanged(TSelection *)), this,
          SLOT(onSelectionChanged(TSelection *)));
}

void ReplaceLevelPopup::setRange(TCellSelection::Range &range,
                                 std::set<int> &columnRange,
                                 bool &replaceCells) {
  m_range        = range;
  m_columnRange  = columnRange;
  m_replaceCells = replaceCells;
}

bool ReplaceLevelPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();
  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(tr("File not found\n") + toQString(fp));
    return false;
  }

  IoCmd::LoadResourceArguments args(fp);
  args.expose = false;
  args.col0   = m_range.m_c0;
  args.col1   = m_range.m_c1;
  args.row0   = m_range.m_r0;
  args.row1   = m_range.m_r1;
  int count   = IoCmd::loadResources(args);
  if (count == 0) return false;
  TXshLevelP xl = args.loadedLevels[0];

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  // cell selection
  if (m_replaceCells) {
    int r, c;
    for (c = m_range.m_c0; c <= m_range.m_c1; c++)
      for (r = m_range.m_r0; r <= m_range.m_r1; r++) {
        TXshCell cell = xsh->getCell(r, c);
        if (!cell.m_level.getPointer()) continue;
        cell.m_level = xl;
        xsh->setCell(r, c, cell);
      }
  }
  // column selection
  else {
    int frameLength           = xsh->getFrameCount();
    std::set<int>::iterator i = m_columnRange.begin();
    while (i != m_columnRange.end()) {
      for (int r = 0; r < frameLength; r++) {
        TXshCell cell = xsh->getCell(r, *i);
        if (!cell.m_level.getPointer()) continue;
        cell.m_level = xl;
        xsh->setCell(r, *i, cell);
      }
      i++;
    }
  }

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();

  DvDirModel::instance()->refreshFolder(fp.getParentDir());

  return true;
}
//-----------------------------------------------------------------------------

void ReplaceLevelPopup::initFolder() {
  setFolder(
      TProjectManager::instance()->getCurrentProject()->getProjectFolder());
}

//-----------------------------------------------------------------------------

void ReplaceLevelPopup::onSelectionChanged(TSelection *sel) {
  if (!sel) return;
  TCellSelection *cellSel     = dynamic_cast<TCellSelection *>(sel);
  TColumnSelection *columnSel = dynamic_cast<TColumnSelection *>(sel);
  if ((!cellSel && !columnSel) || sel->isEmpty()) return;
  if (cellSel) {
    m_range        = cellSel->getSelectedCells();
    m_replaceCells = true;
  } else if (columnSel) {
    m_columnRange  = columnSel->getIndices();
    m_replaceCells = false;
  }
}

//=============================================================================
// SavePaletteAsPopup

SavePaletteAsPopup::SavePaletteAsPopup()
    : FileBrowserPopup(tr("Save Palette"), Options(FOR_SAVING)) {
  setOkText(tr("Save"));
  addFilterType("tpl");
  connect(m_nameField, SIGNAL(returnPressedNow()), m_okButton,
          SLOT(animateClick()));
}

bool SavePaletteAsPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  TFilePath fp(*m_selectedPaths.begin());

  TPaletteHandle *paletteHandle =
      TApp::instance()->getPaletteController()->getCurrentLevelPalette();
  TPalette *palette = paletteHandle->getPalette();

  if (!palette) {
    DVGui::warning("No current palette exists");
    return true;
  }

  const std::string &type = fp.getType();

  if (!type.empty() && type != "tpl")
    return false;
  else if (type.empty())
    fp = fp.getParentDir() + TFilePath(fp.getName() + ".tpl");

  if (TFileStatus(fp).doesExist()) {
    const QString &question =
        QObject::tr(
            "The palette %1 already exists.\nDo you want to overwrite it?")
            .arg(toQString(fp));
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return false;
  }

  const TFilePath &refImagePath = palette->getRefImgPath();
  if (!refImagePath.isEmpty()) palette->setRefImgPath(TFilePath());

  // In questo caso non voglio salvare la reference image.
  StudioPalette::instance()->save(fp, palette);
  if (!refImagePath.isEmpty()) palette->setRefImgPath(refImagePath);

  palette->setDirtyFlag(false);
  paletteHandle->notifyPaletteChanged();

  return true;
}

void SavePaletteAsPopup::initFolder() {
  setFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
}

//=============================================================================
// LoadColorModelPopup

LoadColorModelPopup::LoadColorModelPopup()
    : FileBrowserPopup(tr("Load Color Model"), Options(), "", new QFrame(0)) {
  QFrame *optionFrame = (QFrame *)m_customWidget;
  m_paletteFrame      = new DVGui::LineEdit("", this);

  // layout
  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);
  {
    mainLayout->addStretch(1);
    mainLayout->addWidget(new QLabel(tr("Frames :"), this), 0);
    mainLayout->addWidget(m_paletteFrame, 0);
  }
  optionFrame->setLayout(mainLayout);

  setOkText(tr("Load"));
  addFilterType("bmp");
  addFilterType("jpg");
  addFilterType("nol");
  addFilterType("pic");
  addFilterType("pict");
  addFilterType("pct");
  addFilterType("png");
  addFilterType("rgb");
  addFilterType("sgi");
  addFilterType("tga");
  addFilterType("tif");
  addFilterType("tiff");
  addFilterType("tlv");
  addFilterType("pli");
  addFilterType("psd");
}

//--------------------------------------------------------------

void LoadColorModelPopup::onFilePathsSelected(
    const std::set<TFilePath> &paths) {
  std::list<std::vector<TFrameId>> tmp;
  FileBrowserPopup::onFilePathsSelected(paths, tmp);

  m_paletteFrame->setText("");
  if (paths.size() == 1) {
    // Initialize the line with the level's starting frame
    const TFilePath &fp = *paths.begin();
    try {
      TLevelReaderP lr(fp);
      TLevelP level;
      if (lr) level = lr->loadInfo();

      if (level.getPointer() && level->begin() != level->end()) {
        int firstFrame = level->begin()->first.getNumber();
        if (firstFrame > 0)
          m_paletteFrame->setText(QString::number(firstFrame));
      }
    } catch (...) {
    }
  }
}

//--------------------------------------------------------------

bool LoadColorModelPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  TPaletteHandle *paletteHandle =
      TApp::instance()->getPaletteController()->getCurrentLevelPalette();

  TPalette *palette = paletteHandle->getPalette();
  if (!palette || palette->isCleanupPalette()) {
    DVGui::error(QObject::tr("Cannot load Color Model in current palette."));
    return false;
  }

  PaletteCmd::ColorModelLoadingConfiguration config;

  // if the palette is locked, replace the color model's palette with the
  // destination
  if (palette->isLocked()) {
    // do nothing as config will use behavior = ReplaceColorModelPlt by default
    // config.behavior = PaletteCmd::ReplaceColorModelPlt;
  } else {
    ColorModelBehaviorPopup popup(m_selectedPaths, 0);
    int ret = popup.exec();
    if (ret == QDialog::Rejected) return false;
    popup.getLoadingConfiguration(config);
  }

  std::vector<int> framesInput = string2Indexes(m_paletteFrame->text());

  if (!framesInput.empty()) {
    for (int i = 0; i < framesInput.size(); i++)
      std::cout << framesInput[i] << std::endl;
  }

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  int isLoaded = PaletteCmd::loadReferenceImage(paletteHandle, config, fp,
                                                scene, framesInput);
  // return value - isLoaded
  // 2: failed to get palette
  // 1: failed to get image
  // 0: OK
  if (isLoaded == 2) {
    std::cout << "GetCurrentPalette Failed!" << std::endl;
    return false;
  } else if (isLoaded == 1) {
    std::cout << "GetReferenceImage Failed!" << std::endl;
    return false;
  } else if (0 != isLoaded) {
    std::cout << "loadReferenceImage Failed for some reason." << std::endl;
    return false;
  }

  // no changes in the icon with replace (Keep the destination palette) option
  if (config.behavior != PaletteCmd::ReplaceColorModelPlt) {
    TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
    if (!level) return true;
    std::vector<TFrameId> fids;
    level->getFids(fids);
    invalidateIcons(level, fids);
  }

  return true;
}

//--------------------------------------------------------------

void LoadColorModelPopup::showEvent(QShowEvent *e) {
  m_nameField->clear();
  FileBrowserPopup::showEvent(e);
}

//=============================================================================
/*! replace the parent folder path of the levels in the selected cells
 */

ReplaceParentDirectoryPopup::ReplaceParentDirectoryPopup()
    : FileBrowserPopup(tr("Replace Parent Directory")) {
  setOkText(tr("Replace"));
  setFileMode(true);  // isDirectoryOnly
}

void ReplaceParentDirectoryPopup::setRange(TCellSelection::Range &range,
                                           std::set<int> &columnRange,
                                           bool &replaceCells) {
  m_range        = range;
  m_columnRange  = columnRange;
  m_replaceCells = replaceCells;
}

bool ReplaceParentDirectoryPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  // make the level list in the selected cells
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::vector<TXshLevel *> levelsToBeReplaced;

  if (m_replaceCells) {
    int r, c;
    for (c = m_range.m_c0; c <= m_range.m_c1; c++) {
      for (r = m_range.m_r0; r <= m_range.m_r1; r++) {
        TXshCell cell = xsh->getCell(r, c);
        if (cell.isEmpty() ||
            !cell.m_level->getSimpleLevel())  // TLV and PLI only
          continue;
        levelsToBeReplaced.push_back(cell.m_level.getPointer());
      }
    }
  } else {
    // calculate scene length
    int frameLength           = xsh->getFrameCount();
    std::set<int>::iterator i = m_columnRange.begin();
    while (i != m_columnRange.end()) {
      for (int r = 0; r < frameLength; r++) {
        TXshCell cell = xsh->getCell(r, *i);
        if (!cell.m_level.getPointer()) continue;
        levelsToBeReplaced.push_back(cell.m_level.getPointer());
      }
      i++;
    }
  }

  // avoid level duplication
  std::sort(levelsToBeReplaced.begin(), levelsToBeReplaced.end());
  levelsToBeReplaced.erase(
      std::unique(levelsToBeReplaced.begin(), levelsToBeReplaced.end()),
      levelsToBeReplaced.end());

  if (levelsToBeReplaced.empty()) return false;

  // check if the file exists. If exists, load it and store them in the map
  // rewrite the path in the level settings
  bool somethingChanged                 = false;
  std::vector<TXshLevel *>::iterator it = levelsToBeReplaced.begin();
  for (; it != levelsToBeReplaced.end(); it++) {
    TFilePath orgPath = (*it)->getPath();
    if (orgPath.isEmpty()) continue;

    TFilePath newPath = orgPath.withParentDir(fp);

    if (orgPath == newPath) continue;

    // If the file exists
    if (TSystem::doesExistFileOrLevel(newPath)) {
      TXshSimpleLevel *sl = (*it)->getSimpleLevel();
      if (!sl) continue;

      // replace the file with aliases, if possible
      newPath = TApp::instance()->getCurrentScene()->getScene()->codeFilePath(
          newPath);

      sl->setPath(newPath);
      sl->invalidateFrames();
      std::vector<TFrameId> frames;
      sl->getFids(frames);
      std::vector<TFrameId>::iterator f_it = frames.begin();
      for (; f_it != frames.end(); f_it++)
        IconGenerator::instance()->invalidate(sl, *f_it);

      somethingChanged = true;
    }
  }

  if (!somethingChanged) return false;

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

  return true;
}

void ReplaceParentDirectoryPopup::initFolder() {
  setFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
}

//=============================================================================
// ImportMagpieFilePopup

ImportMagpieFilePopup::ImportMagpieFilePopup()
    : FileBrowserPopup(tr("Import Toonz Lip Sync File")) {
  setOkText(tr("Load"));
  addFilterType("tls");
}

bool ImportMagpieFilePopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(tr("%1 does not exist.").arg(toQString(fp)));
    return false;
  }

  static MagpieFileImportPopup *magpieFileImportPopup =
      new MagpieFileImportPopup();
  magpieFileImportPopup->setFilePath(fp);
  magpieFileImportPopup->show();

  return true;
}

void ImportMagpieFilePopup::initFolder() {
  TProject *project =
      TProjectManager::instance()->getCurrentProject().getPointer();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp;
  if (scene) fp = scene->decodeFilePath(project->getFolder(TProject::Drawings));
  setFolder(fp);
}

void ImportMagpieFilePopup::showEvent(QShowEvent *e) {
  m_nameField->clear();
  FileBrowserPopup::showEvent(e);
}

//=============================================================================
// BrowserPopup

BrowserPopup::BrowserPopup() : FileBrowserPopup("") {
  setOkText(tr("Choose"));
  m_browser->enableGlobalSelection(false);
}

bool BrowserPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (!TSystem::doesExistFileOrLevel(fp)) {
    const QString &msg = tr("Path %1 doesn't exists.").arg(toQString(fp));
    DVGui::info(msg);

    return false;
  }

  m_path = fp;
  return true;
}

void BrowserPopup::initFolder(TFilePath path) {
  // if the path is empty
  if (path.isEmpty()) {
    TProject *project =
        TProjectManager::instance()->getCurrentProject().getPointer();
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene)
      setFolder(scene->decodeFilePath(project->getFolder(TProject::Drawings)));
    return;
  }
  if (!TFileStatus(path).doesExist()) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene) path = scene->decodeFilePath(path);
  }

  if (!path.getType().empty()) path = path.getParentDir();

  setFolder(path);
  setFilename(TFilePath());
}

//=============================================================================
// BrowserPopupController
/* N.B. Eliminare nel momento in cui la classe FileBrowserPopup, con tutte le
   classi annesse
                                (FileBrowser, DvDirTreeView, ...), sara'
   spostata nella libreria toonzQt. */
BrowserPopupController::BrowserPopupController() : m_browserPopup() {
  m_isExecute = false;
  DVGui::FileField::setBrowserPopupController(this);
}

void BrowserPopupController::openPopup(QStringList filters,
                                       bool isDirectoryOnly,
                                       QString lastSelectedPath,
                                       const QWidget *parentWidget) {
  if (!m_browserPopup) m_browserPopup = new BrowserPopup();
  m_browserPopup->setWindowTitle(QString(""));

  m_browserPopup->setFilterTypes(filters);

  m_browserPopup->setWindowTitle((isDirectoryOnly)
                                     ? QString(QObject::tr("Choose Folder"))
                                     : QString(QObject::tr("File Browser")));
  m_browserPopup->initFolder(TFilePath(lastSelectedPath.toStdWString()));
  m_browserPopup->setFileMode(isDirectoryOnly);

  Qt::WindowFlags flags = m_browserPopup->windowFlags();
  bool parentSet        = false;
  if (parentWidget) {
    for (QWidget *pwidget : QApplication::topLevelWidgets()) {
      if (pwidget->isWindow() && pwidget->isVisible() &&
          pwidget->isAncestorOf(parentWidget)) {
        m_browserPopup->setParent(pwidget);
        m_browserPopup->setWindowFlags(flags);
        parentSet = true;
        break;
      }
    }
  }

  if (isDirectoryOnly)
    m_browserPopup->setFilename(TFilePath(lastSelectedPath.toStdWString()));

  if (m_browserPopup->exec() == QDialog::Accepted)
    m_isExecute = true;
  else
    m_isExecute = false;

  // set back the parent to the main window in order to prevent to be
  // deleted along with the parent widget
  if (parentSet) {
    m_browserPopup->setParent(TApp::instance()->getMainWindow());
    m_browserPopup->setWindowFlags(flags);
  }
}

// codePath is set to true by default
QString BrowserPopupController::getPath(bool codePath) {
  m_isExecute = false;
  if (!m_browserPopup) return QString();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp      = m_browserPopup->getPath();
  if (scene && codePath) fp = scene->codeFilePath(fp);
  std::cout << ::to_string(fp) << std::endl;
  return toQString(fp);
}

//=============================================================================
BrowserPopupController browserPopupController;
//-----------------------------------------------------------------------------

OpenPopupCommandHandler<SaveSceneAsPopup> saveSceneAsPopupCommand(
    MI_SaveSceneAs);
OpenPopupCommandHandler<SaveSubSceneAsPopup> saveSubSceneAsPopupCommand(
    MI_SaveSubxsheetAs);
OpenPopupCommandHandler<LoadLevelPopup> loadLevelPopupCommand(MI_LoadLevel);
OpenPopupCommandHandler<ConvertPopupWithInput> convertWithInputPopupCommand(
    MI_ConvertFileWithInput);
OpenPopupCommandHandler<SavePaletteAsPopup> savePalettePopupCommand(
    MI_SavePaletteAs);
OpenPopupCommandHandler<LoadColorModelPopup> loadColorModelPopupCommand(
    MI_LoadColorModel);
OpenPopupCommandHandler<ImportMagpieFilePopup> importMagpieFilePopupCommand(
    MI_ImportMagpieFile);

// Note: MI_ReplaceLevel and MI_ReplaceParentDirectory uses the original
// handler in order to obtain the selection information before making the
// browser.
OpenReplaceFilePopupHandler<ReplaceLevelPopup> replaceLevelPopupCommand(
    MI_ReplaceLevel);
OpenReplaceFilePopupHandler<ReplaceParentDirectoryPopup>
    replaceParentFolderPopupCommand(MI_ReplaceParentDirectory);
