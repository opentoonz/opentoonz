

#include "toonzqt/studiopaletteviewer.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/paletteviewer.h"
#include "toonzqt/trepetitionguard.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/intfield.h"
#include "palettesscanpopup.h"
#include "palettedata.h"

// TnzLib includes
#include "toonz/studiopalettecmd.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "saveloadqsettings.h"
#include "tconvert.h"
#include "tundo.h"
#include "tsystem.h"
#include "tcolorstyles.h"

#include "../toonz/menubarcommandids.h"

// Qt includes
#include <QSettings>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QUrl>
#include <QPainter>
#include <QVBoxLayout>
#include <QToolBar>
#include <QInputDialog>
#include <QPushButton>
#include <QDrag>
#include <QApplication>

#include <ctime>

using namespace PaletteViewerGUI;
using namespace DVGui;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------
/*! Return true if path is in folder \b rootPath of \b StudioPalette.
 */
bool isInStudioPaletteFolder(TFilePath path, TFilePath rootPath) {
  if (path.getType() != "tpl") return false;
  StudioPalette *studioPalette = StudioPalette::instance();
  std::vector<TFilePath> childrenPath;
  studioPalette->getChildren(childrenPath, rootPath);
  int i;
  for (i = 0; i < (int)childrenPath.size(); i++) {
    if (path == childrenPath[i])
      return true;
    else if (isInStudioPaletteFolder(path, childrenPath[i]))
      return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
/*! Return true if path is in a \b StudioPalette folder.
 */
bool isInStudioPalette(TFilePath path) {
  if (path.getType() != "tpl") return false;
  StudioPalette *studioPalette = StudioPalette::instance();
  if (isInStudioPaletteFolder(path, studioPalette->getLevelPalettesRoot()))
    return true;
  if (isInStudioPaletteFolder(path, studioPalette->getProjectPalettesRoot()))
    return true;
  return false;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// StudioPaletteTreeViewer
//-----------------------------------------------------------------------------

StudioPaletteTreeViewer::StudioPaletteTreeViewer(
    QWidget *parent, TPaletteHandle *studioPaletteHandle,
    TPaletteHandle *levelPaletteHandle, TXsheetHandle *xsheetHandle,
    TXshLevelHandle *currentLevelHandle)
    : QTreeWidget(parent)
    , m_dropItem(nullptr)
    , m_studioPaletteHandle(studioPaletteHandle)
    , m_levelPaletteHandle(levelPaletteHandle)
    , m_currentLevelHandle(currentLevelHandle)
    , m_xsheetHandle(xsheetHandle)
    , m_folderIcon(QIcon())
    , m_levelPaletteIcon(QIcon(":Resources/palette.svg"))
    , m_studioPaletteIcon(QIcon(":Resources/studiopalette.svg"))
    , m_startPos() {
  setIndentation(14);
  setAlternatingRowColors(true);

  header()->close();
  setUniformRowHeights(true);
  setIconSize(QSize(21, 18));

  QList<QTreeWidgetItem *> paletteItems;

  StudioPalette *studioPalette = StudioPalette::instance();

  TFilePath levelPalettePath = studioPalette->getLevelPalettesRoot();
  paletteItems.append(createRootItem(levelPalettePath));

  TFilePath projectPalettePath = studioPalette->getProjectPalettesRoot();
  if (TSystem::doesExistFileOrLevel(projectPalettePath))
    paletteItems.append(createRootItem(projectPalettePath));

  insertTopLevelItems(0, paletteItems);

  connect(this, &QTreeWidget::itemChanged, this,
          &StudioPaletteTreeViewer::onItemChanged);
  connect(this, &QTreeWidget::itemClicked, this,
          &StudioPaletteTreeViewer::onItemClicked);
  connect(this, &QTreeWidget::currentItemChanged, this,
          &StudioPaletteTreeViewer::onCurrentItemChanged);

  // refresh tree with shortcut key
  QAction *refreshAct = CommandManager::instance()->getAction(MI_RefreshTree);
  connect(refreshAct, &QAction::triggered, this,
          &StudioPaletteTreeViewer::onRefreshTreeShortcutTriggered);
  addAction(refreshAct);

  m_palettesScanPopup = new PalettesScanPopup();

  setAcceptDrops(true);

  // For multiple selection
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  StudioPalette::instance()->addListener(this);
  TProjectManager::instance()->addListener(this);

  refresh();
}

//-----------------------------------------------------------------------------

StudioPaletteTreeViewer::~StudioPaletteTreeViewer() {
  StudioPalette::instance()->removeListener(this);
  TProjectManager::instance()->removeListener(this);
}

//-----------------------------------------------------------------------------
void StudioPaletteTreeViewer::setCurrentLevelHandle(
    TXshLevelHandle *currentLevelHandle) {
  m_currentLevelHandle = currentLevelHandle;
}

//---------------------------------------------------------------------------

void StudioPaletteTreeViewer::setLevelPaletteHandle(
    TPaletteHandle *paletteHandle) {
  m_levelPaletteHandle = paletteHandle;
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::setStdPaletteHandle(
    TPaletteHandle *studioPaletteHandle) {
  m_studioPaletteHandle = studioPaletteHandle;
}

//-----------------------------------------------------------------------------

QTreeWidgetItem *StudioPaletteTreeViewer::createRootItem(TFilePath path) {
  QString rootName = QString::fromStdWString(path.getWideName());
  if (rootName != "Global Palettes") rootName = "Project Palettes";
  QTreeWidgetItem *rootItem =
      new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(rootName));
  rootItem->setIcon(0, createQIcon("folder", true));
  rootItem->setData(1, Qt::UserRole, toQString(path));

  refreshItem(rootItem);

  return rootItem;
}

//-----------------------------------------------------------------------------

bool StudioPaletteTreeViewer::isRootItem(QTreeWidgetItem *item) {
  assert(item);
  TFilePath path = getItemPath(item);

  StudioPalette *studioPalette = StudioPalette::instance();
  if (path == studioPalette->getLevelPalettesRoot() ||
      path == studioPalette->getProjectPalettesRoot())
    return true;

  return false;
}

//-----------------------------------------------------------------------------

QTreeWidgetItem *StudioPaletteTreeViewer::createItem(const TFilePath path) {
  StudioPalette *studioPalette = StudioPalette::instance();
  QString itemName             = toQString(TFilePath(path.getWideName()));
  QTreeWidgetItem *item =
      new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(itemName));
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable |
                 Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
  if (studioPalette->isPalette(path)) {
    if (studioPalette->hasGlobalName(path))
      item->setIcon(0, m_studioPaletteIcon);
    else
      item->setIcon(0, m_levelPaletteIcon);
    item->setFlags(item->flags() | Qt::ItemNeverHasChildren);
  } else if (studioPalette->isFolder(path)) {
    item->setIcon(0, createQIcon("folder", true));
    item->setFlags(item->flags() | Qt::ItemIsDropEnabled);
  }
  item->setData(1, Qt::UserRole, toQString(path));

  return item;
}

//-----------------------------------------------------------------------------

TFilePath StudioPaletteTreeViewer::getItemPath(QTreeWidgetItem *item) {
  TFilePath path =
      (item) ? TFilePath(item->data(1, Qt::UserRole).toString().toStdWString())
             : TFilePath();
  return path;
}

//-----------------------------------------------------------------------------

TFilePath StudioPaletteTreeViewer::getCurrentFolderPath() {
  return getItemPath(currentItem());
}

//-----------------------------------------------------------------------------

QTreeWidgetItem *StudioPaletteTreeViewer::getItem(const TFilePath path) {
  QList<QTreeWidgetItem *> oldItems =
      findItems(QString(""), Qt::MatchContains, 0);
  if (oldItems.isEmpty()) return nullptr;
  int i;
  for (i = 0; i < (int)oldItems.size(); i++) {
    TFilePath oldItemPath(
        oldItems[i]->data(1, Qt::UserRole).toString().toStdWString());
    if (oldItemPath == path)
      return oldItems[i];
    else {
      QTreeWidgetItem *item = getFolderItem(oldItems[i], path);
      if (item) return item;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

QTreeWidgetItem *StudioPaletteTreeViewer::getFolderItem(QTreeWidgetItem *parent,
                                                        const TFilePath path) {
  int childrenCount = parent->childCount();
  int i;
  for (i = 0; i < childrenCount; i++) {
    QTreeWidgetItem *item = parent->child(i);
    if (getItemPath(item) == path)
      return item;
    else {
      item = getFolderItem(item, path);
      if (item) return item;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::resetDropItem() {
  if (!m_dropItem) return;
  m_dropItem = nullptr;
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::refresh() {
  m_openedItems.clear();

  StudioPalette *studioPalette = StudioPalette::instance();

  TFilePath levelPalettePath = studioPalette->getLevelPalettesRoot();
  refreshItem(getItem(levelPalettePath));

  TFilePath projectPalettePath = studioPalette->getProjectPalettesRoot();
  if (!TSystem::doesExistFileOrLevel(projectPalettePath)) return;
  refreshItem(getItem(projectPalettePath));

  // refresh all expanded items
  QList<QTreeWidgetItem *> items =
      findItems(QString(""), Qt::MatchContains | Qt::MatchRecursive, 0);
  if (items.isEmpty()) return;

  for (int i = 0; i < (int)items.size(); i++)
    if (items[i]->isExpanded()) refreshItem(items[i]);
}

//-----------------------------------------------------------------------------

/*! Refresh tree only when this widget has focus
 */
void StudioPaletteTreeViewer::onRefreshTreeShortcutTriggered() {
  if (hasFocus()) refresh();
}

//-----------------------------------------------------------------------------
/*! Update the content of item
 */

void StudioPaletteTreeViewer::refreshItem(QTreeWidgetItem *item) {
  struct Locals {
    bool isUpper(const TFilePath &fp1, const TFilePath &fp2) {
      bool fp1IsFolder = StudioPalette::instance()->isFolder(fp1);
      bool fp2IsFolder = StudioPalette::instance()->isFolder(fp2);
      if (fp1IsFolder == fp2IsFolder)
        return fp1 < fp2;
      else
        return fp1IsFolder;
    }
  } locals;

  TFilePath folderPath = getItemPath(item);
  assert(folderPath != TFilePath());
  // correct only tpl files and folders
  std::vector<TFilePath> childrenPath;
  StudioPalette::instance()->getChildren(childrenPath, folderPath);
  int currentChildCount = item->childCount();
  std::vector<QTreeWidgetItem *> currentChildren;
  int i;
  for (i = 0; i < currentChildCount; i++)
    currentChildren.push_back(item->child(i));

  int childrenPathCount = childrenPath.size();
  int itemIndex         = 0;
  int pathIndex         = 0;
  while (itemIndex < currentChildCount || pathIndex < childrenPathCount) {
    TFilePath path =
        (pathIndex < childrenPathCount) ? childrenPath[pathIndex] : TFilePath();

    QTreeWidgetItem *currentItem =
        (itemIndex < currentChildCount) ? currentChildren[itemIndex] : nullptr;
    TFilePath currentItemPath = getItemPath(currentItem);

    if (path == currentItemPath) {
      itemIndex++;
      pathIndex++;
    } else if ((!path.isEmpty() && locals.isUpper(path, currentItemPath)) ||
               currentItemPath.isEmpty()) {
      currentItem = createItem(path);
      item->insertChild(pathIndex, currentItem);
      pathIndex++;
    } else {
      assert(locals.isUpper(currentItemPath, path) || path.isEmpty());
      assert(currentItem);
      item->removeChild(currentItem);
      itemIndex++;
    }
  }
  m_openedItems.insert(item);
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::resetProjectPaletteFolder() {
  int projectPaletteIndex = 1;
  TFilePath projectPalettePath =
      StudioPalette::instance()->getProjectPalettesRoot();
  // Get the project palette item
  QTreeWidgetItem *projectPaletteItem = topLevelItem(projectPaletteIndex);
  if (projectPaletteItem) {
    // If the item's path equals the current project palette path, return
    if (getItemPath(projectPaletteItem) == projectPalettePath) return;
    // Otherwise remove it
    removeItemWidget(projectPaletteItem, 0);
    delete projectPaletteItem;
    // clear the item list to search the folder from scratch
    m_openedItems.clear();
    // Toonz Palette is not changed, so resurrect the ToonzPaletteRoot
    m_openedItems.insert(topLevelItem(0));
  }
  if (!TSystem::doesExistFileOrLevel(projectPalettePath)) return;
  // Create the new item with the new project folder and insert it into the tree
  // Items in the ProjectPaletteRoot are refreshed here. Stored in openedItems
  // as well
  QTreeWidgetItem *newProjectPaletteItem = createRootItem(projectPalettePath);
  insertTopLevelItem(projectPaletteIndex, newProjectPaletteItem);

  setCurrentItem(0);
}

//-----------------------------------------------------------------------------

/*! Unified click handler for both folders and palettes */
void StudioPaletteTreeViewer::onItemClicked(QTreeWidgetItem *item, int column) {
  if (!item || !m_studioPaletteHandle) return;

  TFilePath path               = getItemPath(item);
  StudioPalette *studioPalette = StudioPalette::instance();

  if (studioPalette->isFolder(path)) {
    // Handle folder click: toggle expansion with lazy loading
    if (!m_openedItems.contains(item)) {
      refreshItem(item);
      m_openedItems.insert(item);
    }

    // Toggle expansion state
    bool wasExpanded = item->isExpanded();
    item->setExpanded(!wasExpanded);

  } else if (studioPalette->isPalette(path)) {
    // Handle palette click: load palette
    if (m_currentPalette && getItemPath(currentItem()) == path) {
      return;  // Already current, no need to reload
    }

    m_currentPalette = studioPalette->getPalette(path, false);

    if (m_currentPalette && m_currentPalette.getPointer()) {
      m_studioPaletteHandle->setPalette(m_currentPalette.getPointer());
      m_studioPaletteHandle->notifyPaletteSwitched();
      StudioPaletteCmd::updateAllLinkedStyles(m_levelPaletteHandle,
                                              m_xsheetHandle);
    }
  }
}
//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::onItemChanged(QTreeWidgetItem *item, int column) {
  if (item != currentItem() || isRootItem(item)) return;
  std::wstring name = item->text(column).toStdWString();
  TFilePath oldPath = getCurrentFolderPath();
  if (oldPath.isEmpty() || name.empty() || oldPath.getWideName() == name)
    return;
  TFilePath newPath(oldPath.getParentDir() +
                    TFilePath(name + ::to_wstring(oldPath.getDottedType())));
  try {
    StudioPaletteCmd::movePalette(newPath, oldPath);
  } catch (TException &e) {
    error(QString(::to_string(e.getMessage()).c_str()));
    item->setText(column, QString::fromStdWString(oldPath.getWideName()));
  } catch (...) {
    error("Can't rename file");
    item->setText(column, QString::fromStdWString(oldPath.getWideName()));
  }
  refreshItem(getItem(oldPath.getParentDir()));
  setCurrentItem(getItem(newPath));
}

//-----------------------------------------------------------------------------
/*! Called when the current palette is switched
 */
void StudioPaletteTreeViewer::onCurrentItemChanged(QTreeWidgetItem *current,
                                                   QTreeWidgetItem *previous) {
  TFilePath oldPath = getItemPath(previous);
  TFilePath newPath = getCurrentFolderPath();
  if (!m_studioPaletteHandle) return;

  if (m_currentPalette.getPointer() && m_currentPalette->getDirtyFlag()) {
    TFilePath oldPath = StudioPalette::instance()->getPalettePath(
        m_currentPalette->getGlobalName());
    if (oldPath == newPath) return;
    std::wstring gname = m_currentPalette->getGlobalName();
    QString question =
        tr("The current palette %1\nin the studio palette has been modified. "
           "Do you want to "
           "save your changes?")
            .arg(QString::fromStdWString(oldPath.getWideString()));
    int ret =
        DVGui::MsgBox(question, tr("Save"), tr("Discard"), tr("Cancel"), 0);
    if (ret == 3) {
      setCurrentItem(getItem(oldPath));
      return;
    }
    if (ret == 1) {
      try {
        // If the palette is level palette (i.e. NOT stdio palette), just
        // overwrite it
        if (gname.empty())
          StudioPalette::instance()->save(oldPath,
                                          m_currentPalette.getPointer());
        else
          StudioPalette::instance()->setPalette(
              oldPath, m_currentPalette.getPointer(), false);
      } catch (TSystemException se) {
        DVGui::warning(QString::fromStdWString(se.getMessage()));
        setCurrentItem(previous);
        return;
      } catch (...) {
        DVGui::warning(
            QString::fromStdWString(oldPath.getWideString() + L"\n") +
            tr("Failed to save palette."));
        setCurrentItem(previous);
        return;
      }
    }
    m_currentPalette->setDirtyFlag(false);
  }
  // load palette here
  m_currentPalette = StudioPalette::instance()->getPalette(newPath, false);
  m_studioPaletteHandle->setPalette(m_currentPalette.getPointer());
  m_studioPaletteHandle->notifyPaletteSwitched();
  StudioPaletteCmd::updateAllLinkedStyles(m_levelPaletteHandle, m_xsheetHandle);
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::addNewPalette() {
  if (!currentItem()) {
    error("Error: No folder selected.");
    return;
  }
  TFilePath newPath;
  try {
    newPath = StudioPaletteCmd::createPalette(getCurrentFolderPath(), "", 0);
  } catch (TException &e) {
    error("Can't create palette: " +
          QString(::to_string(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't create palette");
  }
  refreshItem(currentItem());
  setCurrentItem(getItem(newPath));
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::addNewFolder() {
  if (!currentItem()) {
    error("Error: No folder selected.");
    return;
  }
  TFilePath newPath;
  try {
    newPath = StudioPaletteCmd::addFolder(getCurrentFolderPath());
  } catch (TException &e) {
    error("Can't create palette folder: " +
          QString(::to_string(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't create palette folder");
  }
  refreshItem(currentItem());
  setCurrentItem(getItem(newPath));
}

//-----------------------------------------------------------------------------
/*! Convert level palette to studio palette.
 */
void StudioPaletteTreeViewer::convertToStudioPalette() {
  TFilePath path               = getItemPath(currentItem());
  StudioPalette *studioPalette = StudioPalette::instance();
  if (studioPalette->isPalette(path)) {
    TPalette *palette = studioPalette->getPalette(path);

    if (!palette) {
      error("Can't touch palette");
      return;
    }

    if (m_currentPalette->getPaletteName() != palette->getPaletteName()) {
      error("Can't touch palette");
      return;
    }

    QString question =
        tr("Convert %1 to Studio Palette and Overwrite. \nAre you sure ?")
            .arg(QString::fromStdWString(path.getWideString()));
    int ret = DVGui::MsgBox(question, tr("Convert"), tr("Cancel"));
    if (ret == 0 || ret == 2) return;

    // apply global name
    time_t ltime;
    time(&ltime);
    std::wstring gname =
        std::to_wstring(ltime) + L"_" + std::to_wstring(rand());
    m_currentPalette->setGlobalName(gname);
    studioPalette->setStylesGlobalNames(m_currentPalette.getPointer());
    studioPalette->save(path, m_currentPalette.getPointer());

    m_currentPalette->setDirtyFlag(false);
    currentItem()->setIcon(0, m_studioPaletteIcon);

  } else
    error("Can't find palette");
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::deleteItem(QTreeWidgetItem *item) {
  QTreeWidgetItem *parent = item->parent();
  if (!parent) return;

  if (item->childCount() > 0) {
    QString question;
    question = tr("This folder is not empty. Delete anyway?");
    int ret  = DVGui::MsgBox(question, tr("Delete"), tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }

  TFilePath path               = getItemPath(item);
  StudioPalette *studioPalette = StudioPalette::instance();
  if (studioPalette->isFolder(path)) {
    try {
      StudioPaletteCmd::deleteFolder(path);
    } catch (TException &e) {
      error("Can't delete folder: " +
            QString(::to_string(e.getMessage()).c_str()));
    } catch (...) {
      error("Can't delete folder");
    }
  } else {
    assert(studioPalette->isPalette(path));
    try {
      StudioPaletteCmd::deletePalette(path);
    } catch (TException &e) {
      error("Can't delete palette: " +
            QString(::to_string(e.getMessage()).c_str()));
    } catch (...) {
      error("Can't delete palette");
    }
  }

  refreshItem(parent);
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::deleteItems() {
  QList<QTreeWidgetItem *> items = selectedItems();
  int count                      = items.size();

  if (count == 0) {
    error("Nothing to delete");
    return;
  }
  int i;
  TUndoManager::manager()->beginBlock();
  for (i = 0; i < count; i++) deleteItem(items[i]);
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::searchForPalette() {
  m_palettesScanPopup->setCurrentFolder(getCurrentFolderPath());
  int ret = m_palettesScanPopup->exec();
  if (ret == QDialog::Accepted) refresh();
}

void StudioPaletteTreeViewer::setAsDefault() {
  TFilePath srcPath = getCurrentItemPath();
  if (srcPath.isEmpty()) return;
  StudioPalette *studioPalette = StudioPalette::instance();

  std::string fileName;
  TFilePath folderPath;
  TFilePath dstPath;

  QString label = QObject::tr("Set As...");
  QStringList radioButtons;
  radioButtons.append(QString("Default Toonz Raster Palette"));
  radioButtons.append(QString("Default Toonz Vector Palette"));
  radioButtons.append(QString("Default Raster Drawing Palette"));
  radioButtons.append(QString("Default Cleanup Palette"));

  int type =
      DVGui::RadioButtonMsgBox(DVGui::QUESTION, label, radioButtons, this);

  switch (type) {
  case 1:
    fileName = "Toonz_Raster_Palette.tpl";
    break;
  case 2:
    fileName = "Toonz_Vector_Palette.tpl";
    break;
  case 3:
    fileName = "Raster_Drawing_Palette.tpl";
    break;
  case 4:
    fileName   = "Cleanup_Palette.tpl";
    folderPath = studioPalette->getLevelPalettesRoot() + "Default Palettes";
    TSystem::copyFile(folderPath + fileName, srcPath, true);
    return;
  default:
    return;
  }

  TPalette *srcPalette = studioPalette->getPalette(srcPath);
  if (!srcPalette) return;

  // Remove all Link
  for (int i = 0; i < srcPalette->getPageCount(); ++i) {
    TPalette::Page *page = srcPalette->getPage(i);
    for (int j = 0; j < page->getStyleCount(); ++j) {
      TColorStyle *cs = page->getStyle(j);
      cs->setGlobalName(L"");
    }
  }

  // Save Palette
  folderPath = studioPalette->getProjectPalettesRoot();
  dstPath    = folderPath + fileName;
  if (TSystem::doesExistFileOrLevel(dstPath))
    TSystem::removeFileOrLevel(dstPath);
  TOStream os(dstPath);
  if (srcPalette) os << srcPalette;

  int policy = DVGui::MsgBox(QString("Apply to Global?"), QString("Yes"),
                             QString("No"), 0, this);
  if (policy == 1) {
    folderPath = studioPalette->getLevelPalettesRoot() + "Default Palettes";
    dstPath    = folderPath + fileName;
    TOStream os(dstPath);
    if (srcPalette) os << srcPalette;
  }

  delete srcPalette;
  refresh();
}

//-----------------------------------------------------------------------------

class InvalidateIconsUndo final : public TUndo {
  TPaletteP m_targetPalette, m_oldPalette, m_newPalette;
  TXshLevelHandle *m_levelHandle;

public:
  InvalidateIconsUndo(TXshLevelHandle *levelHandle)
      : m_levelHandle(levelHandle) {}

  void undo() const override {
    TXshLevel *level = m_levelHandle->getLevel();
    if (level) {
      std::vector<TFrameId> fids;
      level->getFids(fids);
      invalidateIcons(level, fids);
    }
  }
  void redo() const override { undo(); }

  int getSize() const override { return sizeof(*this); }
};

//----------------------------------------------------------------------

//-----------------------------------------------------------------------------

AdjustPaletteDialog::AdjustPaletteDialog()
    : Dialog(nullptr, true, true, "Adjust Current Level to This Palette") {
  setWindowTitle(tr("Adjust Current Level to This Palette"));

  beginVLayout();
  m_tolerance = new IntField(this);
  m_tolerance->setRange(0, 255);
  m_tolerance->setValue(0);
  addWidget(tr("Tolerance"), m_tolerance);
  endVLayout();

  QPushButton *okBtn = new QPushButton(tr("Apply"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  bool ret1 = connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
  bool ret2 = connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  assert(ret1 && ret2);

  addButtonBarWidget(okBtn, cancelBtn);
}

int AdjustPaletteDialog::getTolerance() { return m_tolerance->getValue(); }
//-------------------------------------------------------------------------------------------------

void StudioPaletteTreeViewer::loadInCurrentPaletteAndAdaptLevel() {
  QList<QTreeWidgetItem *> items = selectedItems();
  assert(items.size() == 1);

  TPalette *palette = m_levelPaletteHandle->getPalette();
  if (!palette) return;

  // prevent crash when the command is applied to the palette level
  if (!m_currentLevelHandle->getSimpleLevel()) return;

  TPalette *newPalette =
      StudioPalette::instance()->getPalette(getItemPath(items[0]), true);
  if (!newPalette) return;

  AdjustPaletteDialog apd;

  if (apd.exec() != QDialog::Accepted) return;

  /* awful patch: since in StudioPaletteCmd(defined in toonzlib) I cannot use
the invalidateIcons(defined in toonzqt)
i do invalidate icons from here using a "fake TUndo", named InvalidateIconsUndo.
And, since I need to refresh icons at the end of the processing, i have to put
that fake undo twice, one before and one after.
this way , when the user do either an undo or a redo operation, I am ensured
that the last operation is the icon refresh...
*/
  TUndoManager::manager()->beginBlock();

  TUndoManager::manager()->add(new InvalidateIconsUndo(m_currentLevelHandle));

  StudioPaletteCmd::loadIntoCurrentPalette(m_levelPaletteHandle, newPalette,
                                           m_currentLevelHandle,
                                           apd.getTolerance());

  m_currentLevelHandle->getSimpleLevel()->setDirtyFlag(true);

  // TApp::instance()->getCurrentLevel()->notifyLevelChange();
  // No need to notice user

  TUndoManager::manager()->add(new InvalidateIconsUndo(m_currentLevelHandle));

  TUndoManager::manager()->endBlock();

  InvalidateIconsUndo(m_currentLevelHandle).undo();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::loadInCurrentPalette() {
  QList<QTreeWidgetItem *> items = selectedItems();
  int count                      = items.size();
  if (count == 0) return;

  TPalette *palette = m_levelPaletteHandle->getPalette();
  if (!palette) return;

  if (palette->isLocked()) {
    DVGui::warning("Palette is Locked!");
    return;
  }

  TPalette *newPalette =
      StudioPalette::instance()->getPalette(getItemPath(items[0]), false);
  if (!newPalette) return;
  if (m_xsheetHandle) {
    int ret = DVGui::eraseStylesInDemand(palette, m_xsheetHandle, newPalette);
    if (ret == 0) return;
  }

  StudioPaletteCmd::loadIntoCurrentPalette(m_levelPaletteHandle, newPalette);
  m_currentLevelHandle->notifyLevelChange();

  TXshLevel *level = m_currentLevelHandle->getLevel();
  if (level) {
    std::vector<TFrameId> fids;
    level->getFids(fids);
    invalidateIcons(level, fids);
  }

  int i;
  for (i = 1; i < count; i++) {
    TFilePath path = getItemPath(items[i]);
    StudioPaletteCmd::mergeIntoCurrentPalette(m_levelPaletteHandle, path);
  }
  // in order to update the title bar of palette viewer
  m_levelPaletteHandle->getPalette()->setDirtyFlag(true);
  m_levelPaletteHandle->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::replaceCurrentPalette() {
  QList<QTreeWidgetItem *> items = selectedItems();
  int count                      = items.size();
  if (count == 0) return;

  // exec confirmation dialog
  TPalette *current = m_levelPaletteHandle->getPalette();
  if (!current) return;

  QString label;
  if (count != 1)  // replacing to multiple palettes
    label = tr("Replacing all selected palettes with the palette \"%1\". \nAre "
               "you sure ?")
                .arg(QString::fromStdWString(current->getPaletteName()));
  else {
    TPalette *dstPalette =
        StudioPalette::instance()->getPalette(getItemPath(items[0]));
    if (!dstPalette) return;
    label = tr("Replacing the palette \"%1\" with the palette \"%2\". \nAre "
               "you sure ?")
                .arg(QString::fromStdWString(dstPalette->getPaletteName()))
                .arg(QString::fromStdWString(current->getPaletteName()));
  }

  int ret = DVGui::MsgBox(label, tr("Replace"), tr("Cancel"), 1);
  if (ret == 0 || ret == 2) return;

  TUndoManager::manager()->beginBlock();
  int i;
  for (i = 0; i < count; i++)
    StudioPaletteCmd::replaceWithCurrentPalette(
        m_levelPaletteHandle, m_studioPaletteHandle, getItemPath(items[i]));
  TUndoManager::manager()->endBlock();

  if (m_currentPalette) m_currentPalette->setDirtyFlag(false);
  // in order to update display
  onCurrentItemChanged(currentItem(), currentItem());
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::mergeToCurrentPalette() {
  QList<QTreeWidgetItem *> items = selectedItems();
  int count                      = items.size();
  if (count == 0) return;

  TUndoManager::manager()->beginBlock();
  int i;
  for (i = 0; i < count; i++)
    StudioPaletteCmd::mergeIntoCurrentPalette(m_levelPaletteHandle,
                                              getItemPath(items[i]));
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::paintEvent(QPaintEvent *event) {
  QTreeWidget::paintEvent(event);
  QPainter p(viewport());
  if (m_dropItem) {
    p.setPen(QColor(50, 105, 200));
    p.drawRect(visualItemRect(m_dropItem));
  }
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::contextMenuEvent(QContextMenuEvent *event) {
  TFilePath path = getCurrentFolderPath();

  StudioPalette *studioPalette = StudioPalette::instance();

  // Menu for single selection
  QList<QTreeWidgetItem *> items = selectedItems();
  int count                      = items.size();
  if (count == 1) {
    // Verify if click position is in a row containing an item.
    QRect rect = visualItemRect(currentItem());
    if (!QRect(0, rect.y(), width(), rect.height()).contains(event->pos()))
      return;

    bool isFolder = (studioPalette->isFolder(path));

    QMenu menu(this);
    if (isFolder) {
      QAction *newPaletteAct = menu.addAction(tr("New Palette"));
      connect(newPaletteAct, &QAction::triggered, this,
              &StudioPaletteTreeViewer::addNewPalette);

      QAction *newFolderAct = menu.addAction(tr("New Folder"));
      connect(newFolderAct, &QAction::triggered, this,
              &StudioPaletteTreeViewer::addNewFolder);
    }

    if (studioPalette->isFolder(path) &&
        studioPalette->getLevelPalettesRoot() != path &&
        studioPalette->getProjectPalettesRoot() != path) {
      menu.addSeparator();
      QAction *deleteFolderAct = menu.addAction(tr("Delete Folder"));
      connect(deleteFolderAct, &QAction::triggered, this,
              &StudioPaletteTreeViewer::deleteItems);
    } else if (studioPalette->isPalette(path)) {
      if (m_studioPaletteHandle->getPalette()) {
        QAction *setAsDefaultAct = menu.addAction(tr("Set As..."));
        connect(setAsDefaultAct, &QAction::triggered, this,
                &StudioPaletteTreeViewer::setAsDefault);

        menu.addSeparator();

        QAction *loadIntoCurrentAct =
            menu.addAction(tr("Load into Current Palette"));
        connect(loadIntoCurrentAct, &QAction::triggered, this,
                &StudioPaletteTreeViewer::loadInCurrentPalette);

        QAction *adjustLevelAct =
            menu.addAction(tr("Adjust Current Level to This Palette"));
        connect(adjustLevelAct, &QAction::triggered, this,
                &StudioPaletteTreeViewer::loadInCurrentPaletteAndAdaptLevel);

        QAction *mergeToCurrentAct =
            menu.addAction(tr("Merge to Current Palette"));
        connect(mergeToCurrentAct, &QAction::triggered, this,
                &StudioPaletteTreeViewer::mergeToCurrentPalette);

        if (!m_studioPaletteHandle->getPalette()->isLocked()) {
          QAction *replaceWithCurrentAct =
              menu.addAction(tr("Replace with Current Palette"));
          connect(replaceWithCurrentAct, &QAction::triggered, this,
                  &StudioPaletteTreeViewer::replaceCurrentPalette);

          menu.addSeparator();
          QAction *deletePaletteAct = menu.addAction(tr("Delete Palette"));
          connect(deletePaletteAct, &QAction::triggered, this,
                  &StudioPaletteTreeViewer::deleteItems);
        }
      }
      if (!studioPalette->hasGlobalName(path)) {
        menu.addSeparator();
        QAction *convertAct =
            menu.addAction(tr("Convert to Studio Palette and Overwrite"));
        connect(convertAct, &QAction::triggered, this,
                &StudioPaletteTreeViewer::convertToStudioPalette);
      }
    }

    if (isFolder) {
      menu.addSeparator();
      QAction *searchAct = menu.addAction(tr("Search for Palettes"));
      connect(searchAct, &QAction::triggered, this,
              &StudioPaletteTreeViewer::searchForPalette);

      QAction *refreshAct = menu.addAction(tr("Refresh"));
      connect(refreshAct, &QAction::triggered, this,
              &StudioPaletteTreeViewer::refresh);
    }
    menu.exec(event->globalPos());
    return;
  }

  // Menu for multiple selection
  // Verify if click position is in a row containing an item.
  bool areAllPalette      = true;
  bool isClickInSelection = false;
  int i;
  for (i = 0; i < count; i++) {
    QTreeWidgetItem *item = items[i];
    QRect rect            = visualItemRect(item);
    if (QRect(0, rect.y(), width(), rect.height()).contains(event->pos()))
      isClickInSelection = true;
    TFilePath path = getItemPath(item);
    if (studioPalette->isFolder(path)) areAllPalette = false;
  }
  if (!isClickInSelection) return;

  QMenu menu(this);
  if (areAllPalette) {
    QAction *loadAct = menu.addAction(tr("Load into Current Palette"));
    connect(loadAct, &QAction::triggered, this,
            &StudioPaletteTreeViewer::loadInCurrentPalette);

    QAction *mergeAct = menu.addAction(tr("Merge to Current Palette"));
    connect(mergeAct, &QAction::triggered, this,
            &StudioPaletteTreeViewer::mergeToCurrentPalette);

    QAction *replaceAct = menu.addAction(tr("Replace with Current Palette"));
    connect(replaceAct, &QAction::triggered, this,
            &StudioPaletteTreeViewer::replaceCurrentPalette);

    menu.addSeparator();
  }

  QAction *deleteAct = menu.addAction(tr("Delete"));
  connect(deleteAct, &QAction::triggered, this,
          &StudioPaletteTreeViewer::deleteItems);

  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::mousePressEvent(QMouseEvent *event) {
  QTreeWidget::mousePressEvent(event);
  // If left button is not pressed return
  if (event->button() == Qt::LeftButton) m_startPos = event->pos();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::mouseMoveEvent(QMouseEvent *event) {
  // If left button is not pressed return; is not drag event.
  if (!(event->buttons() & Qt::LeftButton)) return;
  if (!m_startPos.isNull() && (m_startPos - event->pos()).manhattanLength() >=
                                  QApplication::startDragDistance())
    startDragDrop();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::mouseReleaseEvent(QMouseEvent *event) {
  QTreeWidget::mouseReleaseEvent(event);
  if (event->button() == Qt::LeftButton) m_startPos = QPoint();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  QList<QUrl> urls;
  for (QTreeWidgetItem *item : selectedItems()) {
    TFilePath path = getItemPath(item);
    if (!path.isEmpty() &&
        (path.getType() == "tpl" || path.getType() == "pli" ||
         path.getType() == "tlv" || path.getType() == "tnz")) {
      urls << pathToUrl(path);
    }
  }

  if (urls.isEmpty()) return;

  QMimeData *mimeData = new QMimeData;
  mimeData->setUrls(urls);

  QDrag *drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->exec(Qt::CopyAction | Qt::MoveAction);
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::dragEnterEvent(QDragEnterEvent *event) {
  const QMimeData *mimeData      = event->mimeData();
  const PaletteData *paletteData = dynamic_cast<const PaletteData *>(mimeData);

  if (acceptResourceDrop(mimeData->urls())) {
    QList<QUrl> urls = mimeData->urls();
    int count        = urls.size();
    if (count == 0) return;

    // Check that at least one url in the drag is a palette to move.
    bool isPalette = false;
    int i;
    for (i = 0; i < count; i++) {
      QUrl url = urls[i];
      TFilePath path(url.toLocalFile().toStdWString());
      if (!path.isEmpty() && isInStudioPalette(path) &&
          (path.getType() == "tpl" || path.getType() == "pli" ||
           path.getType() == "tlv" || path.getType() == "tnz")) {
        isPalette = true;
        break;
      }
    }
    if (!isPalette) return;

    event->acceptProposedAction();
  } else if (paletteData && paletteData->hasOnlyPalette())
    event->acceptProposedAction();
  viewport()->update();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::dragMoveEvent(QDragMoveEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  TFilePath newPath     = getItemPath(item);

  if (item) {
    // drop will not be executed on the same item
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls() && mimeData->urls().size() == 1) {
      TFilePath path =
          TFilePath(mimeData->urls()[0].toLocalFile().toStdWString());
      if (path == getItemPath(item)) {
        m_dropItem = nullptr;
        event->ignore();
        viewport()->update();
        return;
      }
    }
    // when dragging over other items, drop destination will be the parent
    // folder of it
    if (item->flags() & Qt::ItemNeverHasChildren) {
      item = item->parent();
    }
    m_dropItem = item;
    event->acceptProposedAction();
  } else {
    m_dropItem = nullptr;
    event->ignore();
  }
  viewport()->update();
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::dropEvent(QDropEvent *event) {
  TFilePath newPath = getItemPath(m_dropItem);

  resetDropItem();

  if (newPath.isEmpty()) return;

  const QMimeData *mimeData      = event->mimeData();
  const PaletteData *paletteData = dynamic_cast<const PaletteData *>(mimeData);
  if (paletteData) {
    if (paletteData->hasOnlyPalette()) {
      TPalette *palette = paletteData->getPalette();
      if (!palette) return;

      try {
        StudioPaletteCmd::createPalette(
            newPath, ::to_string(palette->getPaletteName()), palette);
      } catch (TException &e) {
        error("Can't create palette: " +
              QString(::to_string(e.getMessage()).c_str()));
      } catch (...) {
        error("Can't create palette");
      }
    }
    return;
  }

  if (!mimeData->hasUrls() || mimeData->urls().size() == 0) return;

  QList<QUrl> urls = mimeData->urls();

  // make the list of palette paths which will be actually moved
  QList<TFilePath> palettePaths;
  for (int i = 0; i < urls.size(); i++) {
    TFilePath path = TFilePath(urls[i].toLocalFile().toStdWString());
    if (path != newPath && path.getParentDir() != newPath)
      palettePaths.append(path);
  }
  if (palettePaths.isEmpty()) return;

  // open the confirmation dialog in order to prevent unintended move
  QString pltName;
  if (palettePaths.size() == 1)
    pltName = tr("the palette \"%1\"")
                  .arg(QString::fromStdWString(palettePaths[0].getWideName()));
  else
    pltName = tr("the selected palettes");
  QString dstName = QString::fromStdWString(newPath.getWideName());

  QString question =
      tr("Move %1 to \"%2\". Are you sure ?").arg(pltName).arg(dstName);
  int ret = DVGui::MsgBox(question, tr("Move"), tr("Cancel"));
  if (ret == 0 || ret == 2) return;

  bool paletteMoved = false;
  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < palettePaths.size(); i++) {
    TFilePath path = palettePaths[i];
    if (isInStudioPalette(path)) {
      TFilePath newPalettePath =
          newPath +
          TFilePath(path.getWideName() + ::to_wstring(path.getDottedType()));
      try {
        StudioPaletteCmd::movePalette(newPalettePath, path);
        paletteMoved = true;
      } catch (TException &e) {
        error("Can't rename palette: " +
              QString(::to_string(e.getMessage()).c_str()));
      } catch (...) {
        error("Can't rename palette");
      }
    }
  }
  TUndoManager::manager()->endBlock();
  if (paletteMoved) {
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::dragLeaveEvent(QDragLeaveEvent *event) {
  resetDropItem();
  update();
}

//=============================================================================
// StudioPaletteViewer
//-----------------------------------------------------------------------------

StudioPaletteViewer::StudioPaletteViewer(QWidget *parent,
                                         TPaletteHandle *studioPaletteHandle,
                                         TPaletteHandle *levelPaletteHandle,
                                         TFrameHandle *frameHandle,
                                         TXsheetHandle *xsheetHandle,
                                         TXshLevelHandle *currentLevelHandle)
    : QSplitter(parent) {
  // style sheet
  setObjectName("StudioPaletteViewer");
  setFrameStyle(QFrame::StyledPanel);

  setAcceptDrops(true);
  setOrientation(Qt::Vertical);

  // First Splitter Widget
  QWidget *treeWidget      = new QWidget(this);
  QVBoxLayout *treeVLayout = new QVBoxLayout(treeWidget);
  treeVLayout->setContentsMargins(0, 0, 0, 0);
  treeVLayout->setSpacing(0);

  m_studioPaletteTreeViewer = new StudioPaletteTreeViewer(
      treeWidget, studioPaletteHandle, levelPaletteHandle, xsheetHandle,
      currentLevelHandle);

  treeVLayout->addWidget(m_studioPaletteTreeViewer);
  treeWidget->setLayout(treeVLayout);

  // Second Splitter Widget
  m_studioPaletteViewer =
      new PaletteViewer(this, PaletteViewerGUI::STUDIO_PALETTE);
  m_studioPaletteViewer->setObjectName("PaletteViewerInStudioPalette");
  m_studioPaletteViewer->setXsheetHandle(xsheetHandle);
  m_studioPaletteViewer->setPaletteHandle(studioPaletteHandle);
  m_studioPaletteViewer->setFrameHandle(frameHandle);

  addWidget(treeWidget);
  addWidget(m_studioPaletteViewer);

  setFocusProxy(m_studioPaletteViewer);
}

//-----------------------------------------------------------------------------

StudioPaletteViewer::~StudioPaletteViewer() {}

//-----------------------------------------------------------------------------
/*! In order to save current palette from the tool button in the PageViewer.
 */
TFilePath StudioPaletteViewer::getCurrentItemPath() {
  return m_studioPaletteTreeViewer->getCurrentItemPath();
}

//-----------------------------------------------------------------------------

int StudioPaletteViewer::getViewMode() const {
  return m_studioPaletteViewer->getViewMode();
}

//-----------------------------------------------------------------------------

void StudioPaletteViewer::setViewMode(int mode) {
  m_studioPaletteViewer->setViewMode(
      (PaletteViewerGUI::PageViewer::ViewMode)mode);
}

//-----------------------------------------------------------------------------

void StudioPaletteViewer::save(QSettings &settings) const {
  m_studioPaletteViewer->save(settings);
}

void StudioPaletteViewer::load(QSettings &settings) {
  m_studioPaletteViewer->load(settings);
}