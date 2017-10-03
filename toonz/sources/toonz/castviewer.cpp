

#include "castviewer.h"

// Tnz6 includes
#include "castselection.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "iocommand.h"
#include "filmstripcommand.h"
#include "flipbook.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/trepetitionguard.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/infoviewer.h"
#include "historytypes.h"

// TnzCore includes
#include "tundo.h"
#include "tfiletype.h"
#include "tsystem.h"

// Qt includes
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QHeaderView>
#include <QPainter>
#include <QBoxLayout>
#include <QLabel>
#include <QByteArray>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QDir>

const char *AudioFolderName = "Audio";

using namespace DVGui;

namespace {

// Se il widget del cast viewer viene spostato in toonzqt (e quindi fatto
// dipendere dallo sceneHandle)
// questo undo puo' essere spostato in un nuovo file levelsetcommand in
// toonzlib.
class MoveLevelToFolderUndo final : public TUndo {
  TLevelSet *m_levelSet;
  std::wstring m_levelName;
  TFilePath m_oldFolder;
  TFilePath m_newFolder;

public:
  MoveLevelToFolderUndo(TLevelSet *levelSet, std::wstring levelName,
                        TFilePath newFolder)
      : m_levelSet(levelSet), m_levelName(levelName), m_newFolder(newFolder) {
    TXshLevel *level = m_levelSet->getLevel(m_levelName);
    m_oldFolder      = m_levelSet->getFolder(level);
  }

  void undo() const override {
    TXshLevel *level = m_levelSet->getLevel(m_levelName);
    m_levelSet->moveLevelToFolder(m_oldFolder, level);
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }

  void redo() const override {
    TXshLevel *level = m_levelSet->getLevel(m_levelName);
    m_levelSet->moveLevelToFolder(m_newFolder, level);
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Move Level to Cast Folder");
  }
};
}

//=============================================================================
//
// CastTreeViewer
//
//-----------------------------------------------------------------------------

CastTreeViewer::CastTreeViewer(QWidget *parent)
    : QTreeWidget(parent), m_dropTargetItem(0) {
  setObjectName("OnePixelMarginFrame");
  // setObjectName("BrowserTreeView");
  // setStyleSheet("#BrowserTreeView {border: 0px; margin: 1px;
  // qproperty-autoFillBackground: true;}");

  header()->close();
  setIconSize(QSize(21, 17));
  setUniformRowHeights(true);
  // m_treeViewer->setColumnCount(1);

  setAcceptDrops(true);
  connect(this, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this,
          SLOT(onItemChanged(QTreeWidgetItem *, int)));
  connect(this,
          SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
          this, SLOT(onFolderChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
  connect(TApp::instance()->getCurrentScene(), SIGNAL(sceneSwitched()), this,
          SLOT(rebuildCastTree()));
  connect(TApp::instance()->getCurrentScene(),
          SIGNAL(castFolderAdded(const TFilePath &)), this,
          SLOT(onCastFolderAdded(const TFilePath &)));

  connect(TApp::instance()->getCurrentScene(), SIGNAL(nameSceneChanged()), this,
          SLOT(onSceneNameChanged()));

  // Connect all possible changes that can alter the
  // bottom horizontal scrollbar to resize contents...
  connect(this, SIGNAL(expanded(const QModelIndex &)), this,
          SLOT(resizeToConts()));

  connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
          SLOT(resizeToConts()));

  connect(this->model(), SIGNAL(layoutChanged()), this, SLOT(resizeToConts()));

  rebuildCastTree();
}

//-----------------------------------------------------------------------------

//! Resizes viewport to contents
void CastTreeViewer::resizeToConts(void) { resizeColumnToContents(0); }

//-----------------------------------------------------------------------------

void CastTreeViewer::resizeEvent(QResizeEvent *event) {
  resizeColumnToContents(0);
  QTreeView::resizeEvent(event);
}

//-----------------------------------------------------------------------------

TLevelSet *CastTreeViewer::getLevelSet() {
  return TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
}

//-----------------------------------------------------------------------------

void CastTreeViewer::enableCommands() {
  enableCommand(this, MI_Clear, &CastTreeViewer::deleteFolder);
}

//-----------------------------------------------------------------------------

void CastTreeViewer::onFolderChanged(QTreeWidgetItem *current,
                                     QTreeWidgetItem *previous) {
  // rende la selezione corrente; serve per intercettare il comando MI_Clear
  makeCurrent();
}

//-----------------------------------------------------------------------------

void CastTreeViewer::populateFolder(QTreeWidgetItem *folder) {
  TFilePath folderPath(
      folder->data(1, Qt::DisplayRole).toString().toStdWString());
  std::vector<TFilePath> folders;
  getLevelSet()->listFolders(folders, folderPath);
  int i;
  for (i = 0; i < (int)folders.size(); i++) {
    TFilePath fp   = folders[i];
    QString fpQstr = QString::fromStdWString(fp.getWideString());
    QString name =
        QString::fromStdWString(fp.withoutParentDir().getWideString());
    QTreeWidgetItem *child =
        new QTreeWidgetItem((QTreeWidgetItem *)0, QStringList(name) << fpQstr);
    if (name != AudioFolderName)
      child->setFlags(child->flags() | Qt::ItemIsEditable);
    folder->addChild(child);
    populateFolder(child);
  }
}

//-----------------------------------------------------------------------------

void CastTreeViewer::onSceneNameChanged() {
  QTreeWidgetItem *root = topLevelItem(0);
  if (!root) return;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString rootName  = QString("Root");
  if (scene) {
    std::wstring name =
        (scene->isUntitled()) ? L"Untitled" : scene->getSceneName();
    rootName = rootName.fromStdWString(name);
  }
  root->setText(0, rootName);
}

//-----------------------------------------------------------------------------

void CastTreeViewer::onCastFolderAdded(const TFilePath &path) {
  QTreeWidgetItem *root = topLevelItem(0)->child(0);
  assert(root->data(1, Qt::DisplayRole).toString() ==
         toQString(getLevelSet()->getDefaultFolder()));
  QString childName     = QString::fromStdWString(path.getWideName());
  QString childPathQstr = QString::fromStdWString(path.getWideString());
  QTreeWidgetItem *childItem =
      new QTreeWidgetItem(root, QStringList(childName) << childPathQstr);
  childItem->setFlags(childItem->flags() | Qt::ItemIsEditable);
  root->addChild(childItem);
  setCurrentItem(childItem);
}

//-----------------------------------------------------------------------------

void CastTreeViewer::rebuildCastTree() {
  clear();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString rootName  = QString("Root");
  if (scene) {
    std::wstring name =
        (scene->isUntitled()) ? L"Untitled" : scene->getSceneName();
    rootName = rootName.fromStdWString(name);
  }

  QTreeWidgetItem *root =
      new QTreeWidgetItem((QTreeWidgetItem *)0, QStringList(rootName));
  static QPixmap clapboard(":Resources/clapboard.png");
  root->setIcon(0, clapboard);
  insertTopLevelItem(0, root);
  populateFolder(root);
}

//-----------------------------------------------------------------------------

QSize CastTreeViewer::sizeHint() const { return QSize(100, 100); }

//-----------------------------------------------------------------------------

void CastTreeViewer::paintEvent(QPaintEvent *event) {
  QTreeWidget::paintEvent(event);
  QPainter p(viewport());
  if (m_dropTargetItem) {
    p.setPen(QColor(50, 105, 200));
    p.drawRect(visualItemRect(m_dropTargetItem).adjusted(0, 0, -1, 0));
  }
}

//-----------------------------------------------------------------------------

void CastTreeViewer::dragEnterEvent(QDragEnterEvent *e) {
  m_dropFilePath = TFilePath();
  if (acceptResourceOrFolderDrop(e->mimeData()->urls())) {
    if (e->mimeData()->urls().size() != 1) return;
    TFilePath path(e->mimeData()->urls()[0].toLocalFile().toStdWString());

    if (path.getType() == "tnz")
      m_dropFilePath = path;
    else
      return;
  }
  if (!e->mimeData()->hasFormat("application/vnd.toonz.levels") &&
      m_dropFilePath == TFilePath())
    return;
  m_dropTargetItem = itemAt(e->pos());
  if (m_dropTargetItem &&
      m_dropTargetItem->data(0, Qt::DisplayRole).toString() == AudioFolderName)
    m_dropTargetItem = 0;

  e->acceptProposedAction();
  viewport()->update();
}

//-----------------------------------------------------------------------------

void CastTreeViewer::dragMoveEvent(QDragMoveEvent *event) {
  if (!event->mimeData()->hasFormat("application/vnd.toonz.levels") ||
      m_dropFilePath != TFilePath())
    return;

  m_dropTargetItem  = itemAt(event->pos());
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString rootName  = QString("Root");
  if (scene) {
    std::wstring name =
        (scene->isUntitled()) ? L"Untitled" : scene->getSceneName();
    rootName = rootName.fromStdWString(name);
  }
  if (m_dropTargetItem &&
          m_dropTargetItem->data(0, Qt::DisplayRole).toString() ==
              AudioFolderName ||
      m_dropFilePath != TFilePath() &&
          m_dropTargetItem->data(0, Qt::DisplayRole).toString() == rootName)
    m_dropTargetItem = 0;

  if (!m_dropTargetItem)
    event->ignore();
  else
    event->acceptProposedAction();
  viewport()->update();
}

//-----------------------------------------------------------------------------

void CastTreeViewer::dropEvent(QDropEvent *event) {
  m_dropTargetItem = 0;
  if (m_dropFilePath != TFilePath()) {
    IoCmd::loadScene(m_dropFilePath);
    m_dropFilePath   = TFilePath();
    m_dropTargetItem = 0;
    update();
    return;
  }

  if (!event->mimeData()->hasFormat("application/vnd.toonz.levels")) return;
  m_dropTargetItem = itemAt(event->pos());
  if (!m_dropTargetItem) return;
  TFilePath folderPath(
      m_dropTargetItem->data(1, Qt::DisplayRole).toString().toStdWString());
  m_dropTargetItem = 0;
  update();

  if (folderPath.getName() == AudioFolderName) {
    event->ignore();
    return;
  }
  event->acceptProposedAction();

  TLevelSet *levelSet = getLevelSet();

  const CastItems *castItems =
      dynamic_cast<const CastItems *>(event->mimeData());
  assert(castItems);
  if (!castItems) return;
  int i;
  TUndoManager::manager()->beginBlock();
  for (i = 0; i < castItems->getItemCount(); i++) {
    CastItem *item = castItems->getItem(i);
    if (LevelCastItem *li = dynamic_cast<LevelCastItem *>(item)) {
      TXshLevel *level       = li->getLevel();
      std::wstring levelName = level->getName();
      MoveLevelToFolderUndo *undo =
          new MoveLevelToFolderUndo(levelSet, levelName, folderPath);
      levelSet->moveLevelToFolder(folderPath, level);
      TUndoManager::manager()->add(undo);
    } else if (SoundCastItem *si = dynamic_cast<SoundCastItem *>(item)) {
    }
  }
  TUndoManager::manager()->endBlock();
  emit itemMovedToFolder();
}

//-----------------------------------------------------------------------------

void CastTreeViewer::dragLeaveEvent(QDragLeaveEvent *event) {
  m_dropTargetItem = 0;
  m_dropFilePath   = TFilePath();
  update();
}

//-----------------------------------------------------------------------------

void CastTreeViewer::onItemChanged(QTreeWidgetItem *item, int column) {
  if (column != 0) return;
  if (item->isSelected()) {
    TFilePath oldPath(item->data(1, Qt::DisplayRole).toString().toStdWString());
    TFilePath newPath =
        getLevelSet()->renameFolder(oldPath, item->text(0).toStdWString());
    item->setText(1, QString::fromStdWString(newPath.getWideString()));
  }
}

//-----------------------------------------------------------------------------

TFilePath CastTreeViewer::getCurrentFolder() const {
  if (!currentItem()) return TFilePath();
  QVariant data = currentItem()->data(1, Qt::DisplayRole);
  if (data == QVariant()) return TFilePath();
  return TFilePath(data.toString().toStdWString());
}

//-----------------------------------------------------------------------------

void CastTreeViewer::folderUp() {
  QTreeWidgetItem *item = currentItem();
  if (item && item->parent() != (QTreeWidgetItem *)0)
    setCurrentItem(item->parent());
}

//-----------------------------------------------------------------------------

void CastTreeViewer::newFolder() {
  QTreeWidgetItem *parentItem = currentItem();
  if (parentItem == (QTreeWidgetItem *)0) return;
  QString parentName = parentItem->data(0, Qt::DisplayRole).toString();
  if (parentName == AudioFolderName) return;

  TFilePath parentPath =
      TFilePath(parentItem->data(1, Qt::DisplayRole).toString().toStdWString());
  QString childName("New Folder");
  TFilePath childPath   = parentPath + childName.toStdWString();
  QString childPathQstr = QString::fromStdWString(childPath.getWideString());
  QTreeWidgetItem *childItem =
      new QTreeWidgetItem(parentItem, QStringList(childName) << childPathQstr);
  childItem->setFlags(childItem->flags() | Qt::ItemIsEditable);
  parentItem->addChild(childItem);
  setCurrentItem(childItem);
  editItem(childItem);
  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  levelSet->createFolder(parentPath, childName.toStdWString());
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void CastTreeViewer::deleteFolder() {
  QTreeWidgetItem *item = currentItem();
  if (!item || !item->parent()) return;
  QString itemName = item->data(0, Qt::DisplayRole).toString();
  if (itemName == AudioFolderName) return;
  int ret = DVGui::MsgBox(tr("Delete folder ") + item->text(0) + "?", tr("Yes"),
                          tr("No"), 1);
  if (ret == 2 || ret == 0) return;
  QTreeWidgetItem *parentItem = item->parent();

  TFilePath childPath(item->data(1, Qt::DisplayRole).toString().toStdWString());
  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  levelSet->removeFolder(childPath);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);

  parentItem->removeChild(item);
  setCurrentItem(parentItem);
}

//=============================================================================
//
// CastBrowser
//
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
CastBrowser::CastBrowser(QWidget *parent, Qt::WindowFlags flags)
#else
CastBrowser::CastBrowser(QWidget *parent, Qt::WFlags flags)
#endif
    : QSplitter(parent)
    , m_treeViewer(0)
    , m_folderName(0)
    , m_itemViewer(0)
    , m_castItems(new CastItems()) {
  // style sheet
  setObjectName("CastBrowser");
  setFrameStyle(QFrame::StyledPanel);
  setStyleSheet("QSplitter::handle {height:4px;}");
  setStyleSheet("#CastBrowser { margin:1px;border:0px }");

  m_treeViewer = new CastTreeViewer(this);
  m_treeViewer->resize(300, m_treeViewer->size().height());

  QFrame *box = new QFrame(this);
  box->setFrameStyle(QFrame::StyledPanel);
  QVBoxLayout *boxLayout = new QVBoxLayout(box);
  boxLayout->setMargin(0);
  boxLayout->setSpacing(0);

  m_folderName = new QLabel("", box);
  m_folderName->setFrameStyle(QFrame::StyledPanel);
  m_folderName->setStyleSheet("border-bottom: 1px solid black");
  m_itemViewer = new DvItemViewer(box, false, true, DvItemViewer::Cast);
  DvItemViewerPanel *viewerPanel = m_itemViewer->getPanel();
  viewerPanel->setMissingTextColor(QColor(200, 0, 0));
  CastSelection *castSelection = new CastSelection();
  castSelection->setBrowser(this);
  viewerPanel->setSelection(castSelection);
  viewerPanel->addColumn(DvItemListModel::FrameCount, 50);
  m_itemViewer->setModel(this);

  DvItemViewerTitleBar *titleBar = new DvItemViewerTitleBar(m_itemViewer, box);
  // titleBar->hide();
  DvItemViewerButtonBar *buttonBar =
      new DvItemViewerButtonBar(m_itemViewer, box);

  boxLayout->addWidget(m_folderName);
  boxLayout->addWidget(titleBar);
  boxLayout->addWidget(m_itemViewer);
  boxLayout->addWidget(buttonBar);

  boxLayout->setAlignment(buttonBar, Qt::AlignBottom);
  box->setLayout(boxLayout);

  addWidget(m_treeViewer);
  addWidget(box);

  setStretchFactor(1, 2);

  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();

  connect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(refresh()));
  connect(sceneHandle, SIGNAL(castChanged()), this, SLOT(refresh()));

  TXsheetHandle *xhseetHandle = TApp::instance()->getCurrentXsheet();
  connect(xhseetHandle, SIGNAL(xsheetChanged()), m_itemViewer, SLOT(update()));

  connect(buttonBar, SIGNAL(folderUp()), m_treeViewer, SLOT(folderUp()));
  connect(buttonBar, SIGNAL(newFolder()), m_treeViewer, SLOT(newFolder()));

  connect(m_treeViewer,
          SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
          this, SLOT(folderChanged(QTreeWidgetItem *, QTreeWidgetItem *)));

  connect(m_treeViewer, SIGNAL(itemMovedToFolder()), this, SLOT(refresh()));
}

//-----------------------------------------------------------------------------

CastBrowser::~CastBrowser() {}

//-----------------------------------------------------------------------------

void CastBrowser::sortByDataModel(DataType dataType, bool isDiscendent) {
  if (dataType != getCurrentOrderType()) {
    // Sort by another dataType
    int i;
    for (i = 1; i < m_castItems->getItemCount(); i++) {
      int index = i;
      while (index > 0 && compareData(dataType, index - 1, index) > 0) {
        m_castItems->swapItem(index - 1, index);
        index = index - 1;
      }
    }
    setIsDiscendentOrder(true);
    setOrderType(dataType);
  }
  // Sort by order (invert current)
  if (isDiscendentOrder() != isDiscendent) {
    setIsDiscendentOrder(isDiscendent);
    std::vector<CastItem *> items;
    for (int i = 0; i < m_castItems->getItemCount(); i++)
      items.push_back(m_castItems->getItem(i)->clone());
    m_castItems->clear();
    for (int i = items.size() - 1; i >= 0; i--) m_castItems->addItem(items[i]);
  }

  m_itemViewer->getPanel()->update();
}

//-----------------------------------------------------------------------------

void CastBrowser::folderChanged(QTreeWidgetItem *current,
                                QTreeWidgetItem *previous) {
  refresh();
}

//-----------------------------------------------------------------------------

void CastBrowser::refresh() {
  if (isHidden()) return;
  refreshData();
  m_itemViewer->refresh();
}

//-----------------------------------------------------------------------------

/*
void CastBrowser::getLevels(std::vector<TXshLevel*> &levels) const
{
  TLevelSet *levelSet = CastTreeViewer::getLevelSet();
  TFilePath folder = m_treeViewer->getCurrentFolder();
  if(folder != TFilePath())
    levelSet->listLevels(levels, folder);
  else
    levelSet->listLevels(levels);
}
*/

//-----------------------------------------------------------------------------

void CastBrowser::refreshData() {
  m_castItems->clear();
  TLevelSet *levelSet = CastTreeViewer::getLevelSet();
  TFilePath folder    = m_treeViewer->getCurrentFolder();
  m_folderName->setText(toQString(folder));
  std::vector<TXshLevel *> levels;
  if (folder != TFilePath())
    levelSet->listLevels(levels, folder);
  else
    levelSet->listLevels(levels);

  int i;
  for (i = 0; i < (int)levels.size(); i++) {
    if (levels[i]->getSimpleLevel())
      m_castItems->addItem(new LevelCastItem(
          levels[i], m_itemViewer->getPanel()->getIconSize()));
    else if (levels[i]->getPaletteLevel())
      m_castItems->addItem(
          new PaletteCastItem(levels[i]->getPaletteLevel(),
                              m_itemViewer->getPanel()->getIconSize()));
    else if (levels[i]->getSoundLevel())
      m_castItems->addItem(new SoundCastItem(
          levels[i]->getSoundLevel(), m_itemViewer->getPanel()->getIconSize()));
  }
}

//-----------------------------------------------------------------------------

int CastBrowser::getItemCount() const { return m_castItems->getItemCount(); }

//-----------------------------------------------------------------------------

QVariant CastBrowser::getItemData(int index, DataType dataType,
                                  bool isSelected) {
  if (index < 0 || index >= getItemCount()) return QVariant();
  CastItem *item = m_castItems->getItem(index);
  if (dataType == Name)
    return item->getName();
  else if (dataType == Thumbnail)
    return item->getPixmap(isSelected);
  else if (dataType == ToolTip)
    return item->getToolTip();
  else if (dataType == FrameCount)
    return item->getFrameCount();
  else if (dataType == VersionControlStatus) {
    if (!item->exists())
      return VC_Missing;
    else
      return 0;  // No version control status displayed
  } else
    return QVariant();
}

//-----------------------------------------------------------------------------

/*
TXshSimpleLevel *CastBrowser::getSelectedSimpleLevel() const
{
  const std::set<int> &indices = m_itemViewer->getSelectedIndices();
  if(indices.empty()) return 0;

  int index = *indices.begin();

  std::vector<TXshLevel*> levels;
  getLevels(levels);

  if(index<0 || index>=(int)levels.size()) return 0;
  TXshLevel*level = levels[index];
  if(!level) return 0;
  else return level->getSimpleLevel();
}
*/

//-----------------------------------------------------------------------------

void CastBrowser::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  const std::set<int> &indices = m_itemViewer->getSelectedIndices();
  if (indices.empty()) return;
  QDrag *drag = new QDrag(this);

  QVariant v = getItemData(*indices.begin(), Thumbnail);
  if (v != QVariant()) {
    QPixmap dropThumbnail = v.value<QPixmap>();
    if (!dropThumbnail.isNull()) drag->setPixmap(dropThumbnail);
  }
  drag->setMimeData(m_castItems->getSelectedItems(indices));
  Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
}

//-----------------------------------------------------------------------------

bool CastBrowser::acceptDrop(const QMimeData *data) const {
  return acceptResourceOrFolderDrop(data->urls());
}

//-----------------------------------------------------------------------------

bool CastBrowser::drop(const QMimeData *data) {
  if (!acceptDrop(data)) return false;

  if (data->hasUrls()) {
    IoCmd::LoadResourceArguments args;

    foreach (const QUrl &url, data->urls()) {
      TFilePath fp(url.toLocalFile().toStdWString());
      args.resourceDatas.push_back(fp);
    }

    args.castFolder = m_treeViewer->getCurrentFolder();

    IoCmd::loadResources(args);
  }

  refresh();
  return true;
}

//-----------------------------------------------------------------------------

QMenu *CastBrowser::getContextMenu(QWidget *parent, int index) {
  const std::set<int> &indices = m_itemViewer->getSelectedIndices();
  if (indices.empty()) {
    QMenu *menu = new QMenu(parent);
    menu->addAction(CommandManager::instance()->getAction(MI_RemoveUnused));
    return menu;
  }
  // controllo cosa c'e' nella selezione.
  // => audioSelected, vectorLevelSelected, paletteSelected
  std::set<int>::const_iterator it;
  bool audioSelected       = false;
  bool paletteSelected     = false;
  bool vectorLevelSelected = false;
  bool meshLevelSelected   = false;
  bool otherFileSelected   = false;
  int levelSelectedCount   = 0;
  for (it = indices.begin(); it != indices.end(); ++it) {
    int index = *it;
    if (index < 0 || index >= m_castItems->getItemCount())
      continue;  // non dovrebbe mai succedere
    TXshSimpleLevel *sl = m_castItems->getItem(index)->getSimpleLevel();
    if (!sl) {
      if (m_castItems->getItem(index)->getPaletteLevel())
        paletteSelected = true;
      else
        audioSelected = true;
      continue;
    }
    levelSelectedCount++;
    if (sl->getType() == PLI_XSHLEVEL)
      vectorLevelSelected = true;
    else if (sl->getType() == MESH_XSHLEVEL)
      meshLevelSelected = true;
    else
      otherFileSelected = true;
  }
  QMenu *menu        = new QMenu(parent);
  CommandManager *cm = CommandManager::instance();

  menu->addAction(cm->getAction(MI_LevelSettings));

  menu->addAction(cm->getAction(MI_ExposeResource));
  menu->addAction(cm->getAction(MI_ShowFolderContents));
  if (!audioSelected && !paletteSelected && !meshLevelSelected)
    menu->addAction(cm->getAction(MI_ViewFile));
  menu->addAction(cm->getAction(MI_FileInfo));

  // MI_EditLevel solo se e' stato selezionato un singolo diverso da livelli
  // palette a livelli audio
  if (indices.size() == 1 && !audioSelected && !paletteSelected)
    menu->addAction(cm->getAction(MI_EditLevel));
  if (!paletteSelected) menu->addAction(cm->getAction(MI_SaveLevel));
  menu->addSeparator();
  // MI_ConvertToVectors se sono stati selezionati solo livelli non vettoriali
  if (!audioSelected && !paletteSelected && !vectorLevelSelected)
    menu->addAction(cm->getAction(MI_ConvertToVectors));
  menu->addSeparator();
  menu->addAction(cm->getAction(MI_RemoveLevel));
  menu->addAction(cm->getAction(MI_RemoveUnused));

  return menu;
}

//-----------------------------------------------------------------------------

void CastBrowser::expose() {
  std::set<int> selectedIndices =
      m_itemViewer->getPanel()->getSelectedIndices();
  TUndoManager::manager()->beginBlock();
  for (int index : selectedIndices) {
    CastItem *item         = m_castItems->getItem(index);
    TXshSimpleLevel *sl    = item->getSimpleLevel();
    TXshPaletteLevel *pl   = item->getPaletteLevel();
    TXshSoundLevel *soundl = item->getSoundLevel();
    if (sl)
      FilmstripCmd::moveToScene(sl);
    else if (pl)
      FilmstripCmd::moveToScene(pl);
    else if (soundl)
      FilmstripCmd::moveToScene(soundl);
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void CastBrowser::edit() {
  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(TSelection::getCurrent());
  if (!castSelection) return;
  std::vector<TXshLevel *> levels;
  castSelection->getSelectedLevels(levels);
  if ((int)levels.size() == 1) {
    TXshLevel *l = levels[0];
    if (l) {
      TXshSimpleLevel *sl = l->getSimpleLevel();
      if (sl) {
        TApp::instance()->getCurrentLevel()->setLevel(sl);
        return;
      }
    }
    error(tr("It is not possible to edit the selected file."));
  } else
    error(tr("It is not possible to edit more than one file at once."));
}

//-----------------------------------------------------------------------------

void CastBrowser::showFolderContents() {
  int i = 0;
  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(TSelection::getCurrent());
  if (!castSelection) return;
  std::vector<TXshLevel *> levels;
  castSelection->getSelectedLevels(levels);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath folderPath, filePath;
  for (i = 0; i < levels.size(); i++) {
    folderPath = levels[i]->getPath().getParentDir();
    filePath   = levels[i]->getPath();

    folderPath = scene->decodeFilePath(folderPath);
    filePath   = scene->decodeFilePath(filePath);

    if (!TSystem::doesExistFileOrLevel(filePath)) {
      error(
          tr("It is not possible to show the folder containing the selected "
             "file, as the file has not been saved yet."));
    } else {
      if (TSystem::isUNC(folderPath))
        QDesktopServices::openUrl(
            QUrl(QString::fromStdWString(folderPath.getWideString())));
      else
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QString::fromStdWString(folderPath.getWideString())));
    }
  }
}
//-----------------------------------------------------------------------------

void CastBrowser::viewFile() {
  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(TSelection::getCurrent());
  if (!castSelection) return;
  std::vector<TXshLevel *> levels;
  castSelection->getSelectedLevels(levels);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath filePath;
  if (levels.empty()) return;
  int i = 0;
  for (i = 0; i < levels.size(); i++) {
    filePath = levels[i]->getPath();

    filePath = scene->decodeFilePath(filePath);
    if (!TSystem::doesExistFileOrLevel(filePath)) {
      error(
          tr("It is not possible to view the selected file, as the file has "
             "not been saved yet."));
    } else {
      if (!TFileType::isViewable(TFileType::getInfo(filePath))) return;

      if (Preferences::instance()->isDefaultViewerEnabled() &&
          (filePath.getType() == "mov" || filePath.getType() == "avi" ||
           filePath.getType() == "3gp"))
        QDesktopServices::openUrl(QUrl("file:///" + toQString(filePath)));
      else
        ::viewFile(filePath);
    }
  }
}

//-----------------------------------------------------------------------------

void CastBrowser::viewFileInfo() {
  CastSelection *castSelection =
      dynamic_cast<CastSelection *>(TSelection::getCurrent());
  if (!castSelection) return;
  std::vector<TXshLevel *> levels;
  castSelection->getSelectedLevels(levels);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  int i             = 0;

  if (levels.empty()) {
    return;
  } else {
    for (i = 0; i < levels.size(); i++) {
      TFilePath filePath;
      filePath = levels[i]->getPath();

      filePath = scene->decodeFilePath(filePath);
      if (!TSystem::doesExistFileOrLevel(filePath)) {
        error(
            tr("It is not possible to show the info of the selected file, as "
               "the file has not been saved yet."));
      } else {
        InfoViewer *infoViewer = 0;
        infoViewer             = new InfoViewer(this);
        infoViewer->setItem(0, 0, filePath);
      }
    }
  }
}
//=============================================================================

OpenFloatingPanel openCastPane(MI_OpenFileBrowser2, "SceneCast",
                               QObject::tr("Scene Cast"));
