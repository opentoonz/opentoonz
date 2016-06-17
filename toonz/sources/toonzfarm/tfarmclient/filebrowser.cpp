

#include "filebrowser.h"
#include "thumbnail.h"
#include "thumbnailviewer.h"
#include "tw/treeview.h"
#include "tw/colors.h"
#include "tw/button.h"
#include "tw/scrollbar.h"
#include "tw/mainshell.h"
#include "tfilepath.h"
#include "tsystem.h"
#include "tenv.h"

#include "tw/message.h"

using namespace TwConsts;

//===================================================================

namespace {

#include "filebrowser_icons.h"

//===================================================================

class DirectoryBrowser : public ThumbnailViewer {
  TFilePath m_filepath;
  vector<string> m_fileTypes;

  GenericFileBrowserAction *m_fileSelChangeAction;
  GenericFileBrowserAction *m_fileDblClickAction;

public:
  DirectoryBrowser(TWidget *parent, const vector<string> &fileTypes)
      : ThumbnailViewer(parent)
      , m_fileSelChangeAction(0)
      , m_fileDblClickAction(0)
      , m_fileTypes(fileTypes) {}

  ~DirectoryBrowser() {
    if (m_fileSelChangeAction) delete m_fileSelChangeAction;

    if (m_fileDblClickAction) delete m_fileDblClickAction;
  }

  void setPath(const TFilePath &filepath);
  void setFilter(const vector<string> &fileTypes);

  TFilePath getPath() const { return m_filepath; }

  void onDoubleClick(int index);
  void onSelect(int index);

  void setFileSelChangeAction(GenericFileBrowserAction *action) {
    m_fileSelChangeAction = action;
  }

  void setFileDblClickAction(GenericFileBrowserAction *action) {
    m_fileDblClickAction = action;
  }
};

//-------------------------------------------------------------------

void DirectoryBrowser::setPath(const TFilePath &filepath) {
  m_filepath = filepath;
  loadDirectory(filepath, m_fileTypes);
}

//-------------------------------------------------------------------

void DirectoryBrowser::setFilter(const vector<string> &fileTypes) {
  m_fileTypes = fileTypes;
  if (!m_filepath.isEmpty()) loadDirectory(m_filepath, m_fileTypes);
}

//-------------------------------------------------------------------

void DirectoryBrowser::onDoubleClick(int index) {
  Thumbnail *thumbnail = getItem(index);
  if (thumbnail->getType() != Thumbnail::SCENE) return;
  TFilePath path =
      thumbnail->getPath().getParentDir() + (thumbnail->getName() + "_files");
  setPath(path);
  invalidate();
}

//-------------------------------------------------------------------

void DirectoryBrowser::onSelect(int index) {
  if (index != -1) {
    Thumbnail *thumbnail = getItem(index);
    if (m_fileSelChangeAction)
      m_fileSelChangeAction->sendCommand(thumbnail->getPath());
  }
}

//===================================================================

class MyTreeViewItem : public TTreeViewItem {
public:
  MyTreeViewItem(TTreeViewItemParent *parent) : TTreeViewItem(parent) {}

  TDimension getIconSize() const { return TDimension(26, 26); }

  void drawIcon(TTreeView *w, const TPoint &origin) {
    TRect rect(origin, getIconSize());
    w->setColor(TWidget::Black);
    w->drawRect(rect);
    rect = rect.enlarge(-4);
    w->setColor(Red);
    w->drawRect(rect);
    w->setColor(Blue);
    w->fillRect(rect.enlarge(-1));
  }

  void drawName(TTreeView *w, const TPoint &origin) {
    string name         = toString(getName());
    TDimension textSize = w->getTextSize(name);
    TDimension iconSize = getIconSize();

    TPoint pos = origin;
    pos.x += iconSize.lx;

    if (isSelected()) {
      w->setColor(ToonzHighlightColor);
      w->fillRect(pos.x + 2, pos.y + 2, pos.x + textSize.lx + 8,
                  pos.y + iconSize.ly - 1 - 2);
    }
    w->setColor(TWidget::Black);
    w->drawText(pos + TPoint(5, iconSize.ly / 2 - textSize.ly / 2), name);
  }

  /*
void draw(TTreeView *w, const TPoint &origin) {
drawIcon(w,origin);
drawName(w,origin);
}
*/
};

//===================================================================

class MyTreeView : public TTreeView {
public:
  MyTreeView(TWidget *parent, string name = "MyTreeView")
      : TTreeView(parent, name) {
    setBackgroundColor(White);
  }

  void drawButton(const TPoint &p, bool open) {
    int r1 = 3, r2 = 1;
    TRect rect(p.x - r1, p.y - r1, p.x + r1, p.y + r1);
    setColor(getBackgroundColor());
    fillRect(rect.enlarge(-1));
    setColor(Gray210);
    drawRect(rect);
    setColor(Black);
    drawLine(p.x - r2, p.y, p.x + r2, p.y);
    if (!open) drawLine(p.x, p.y - r2, p.x, p.y + r2);
  }
};

//===================================================================

class SimpleFileItem : public MyTreeViewItem {
  TFilePath m_path;

public:
  SimpleFileItem(TTreeViewItemParent *parent, const TFilePath &path)
      : MyTreeViewItem(parent), m_path(path) {
    setIsLeaf(true);
  }

  wstring getName() const { return m_path.withoutParentDir().getWideString(); }

  TFilePath getPath() const { return m_path; }
};

//-------------------------------------------------------------------

bool caseInsensitiveLessThan(const TFilePath &fp1, const TFilePath &fp2) {
  return (stricmp(toString(fp1.getWideString()).c_str(),
                  toString(fp2.getWideString()).c_str()) < 0);
}

//-------------------------------------------------------------------

class FileFolderItem : public MyTreeViewItem {
  TFilePath m_path;

public:
  FileFolderItem(TTreeViewItemParent *parent, const TFilePath &path)
      : MyTreeViewItem(parent), m_path(path) {}
  wstring getName() const {
    return m_path.isRoot() ? m_path.getWideString()
                           : m_path.withoutParentDir().getWideString();
  }

  TDimension getIconSize() const { return TDimension(16, 16); }

  void onOpen() {
    clearItems();
    try {
      TFilePathSet fps = TSystem::readDirectory(m_path);
      fps.sort(caseInsensitiveLessThan);
      for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
        if (TFileStatus(*it).isDirectory()) new FileFolderItem(this, *it);
        // else
        //  new SimpleFileItem(this, *it);
      }
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }
  TFilePath getPath() const { return m_path; }

  void drawIcon(TTreeView *w, const TPoint &origin) {
    w->rectwrite(isOpen() ? openfolder : closedfolder, origin);
  }
};

//-------------------------------------------------------------------

class MyComputerFolder : public MyTreeViewItem {
public:
  MyComputerFolder(TTreeViewItemParent *parent) : MyTreeViewItem(parent) {}

  wstring getName() const { return toWideString("My Computer"); }

  void onOpen() {
    clearItems();

    TFilePathSet fps = TSystem::getDisks();
    for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
      TTreeViewItem *item = new FileFolderItem(this, *it);
    }
  }
  void drawIcon(TTreeView *w, const TPoint &origin) {
    w->rectwrite(mycomputer, origin);
  }
};

//-------------------------------------------------------------------

class HistoryFolder : public MyTreeViewItem {
public:
  HistoryFolder(TTreeViewItemParent *parent) : MyTreeViewItem(parent) {}

  wstring getName() const { return toWideString("History"); }

  void onOpen() { clearItems(); }
  void drawIcon(TTreeView *w, const TPoint &origin) {
    w->rectwrite(history, origin);
  }
};

//-------------------------------------------------------------------

class LibraryFolder : public MyTreeViewItem {
public:
  LibraryFolder(TTreeViewItemParent *parent) : MyTreeViewItem(parent) {}

  wstring getName() const { return toWideString("Library"); }

  void onOpen() {
    clearItems();
    TFilePath root = TEnv::getRootDir();
    new FileFolderItem(this, root + "patterns");
    new FileFolderItem(this, root + "images");
    new FileFolderItem(this, root + "scenes");
  }
  void drawIcon(TTreeView *w, const TPoint &origin) {
    w->rectwrite(library, origin);
  }
};

//-------------------------------------------------------------------

class WorkFolder : public FileFolderItem {
public:
  WorkFolder(TTreeViewItemParent *parent)
      : FileFolderItem(parent, TEnv::getRootDir() + "work") {}

  TDimension getIconSize() const { return TDimension(24, 24); }

  wstring getName() const { return toWideString("Work"); }

  void drawIcon(TTreeView *w, const TPoint &origin) {
    w->rectwrite(workinprogress, origin);
  }
};
//-------------------------------------------------------------------

class FileFolderTreeView : public MyTreeView {
  DirectoryBrowser *m_viewer;
  TTreeViewItem *m_myComputer;

public:
  FileFolderTreeView(TWidget *parent, DirectoryBrowser *viewer)
      : MyTreeView(parent), m_viewer(viewer) {
    m_myComputer = new MyComputerFolder(this);
    m_myComputer->open();

    /*
new HistoryFolder(this);
new LibraryFolder(this);
new WorkFolder(this);
*/
  }

  void leftButtonDoubleClick(const TMouseEvent &e);

  void onSelect(TTreeViewItem *item);
  void setCurrentDir(const TFilePath &dirPath);
};

//===================================================================

void FileFolderTreeView::leftButtonDoubleClick(const TMouseEvent &e) {
  TTreeViewItem *item = getSelectedItem();
  if (item) {
    if (!item->isOpen())
      item->open();
    else
      item->close();

    updateVisibleItems();
    invalidate();
  }
}

//-------------------------------------------------------------------

void FileFolderTreeView::onSelect(TTreeViewItem *item) {
  FileFolderItem *folder = dynamic_cast<FileFolderItem *>(item);
  if (folder && folder->getPath() != m_viewer->getPath()) {
    TFilePath path = folder->getPath();
    m_viewer->setPath(path);
    m_viewer->invalidate();
  }
}

//-------------------------------------------------------------------

TTreeViewItem *openChild(TTreeViewItem *folder, const TFilePath &childPath) {
  int count = folder->getItemCount();
  for (int i = 0; i < count; ++i) {
    FileFolderItem *item = dynamic_cast<FileFolderItem *>(folder->getItem(i));

    if (item && item->getPath() == childPath) {
      item->open();
      return item;
    }
  }

  return 0;
}

//-------------------------------------------------------------------

void FileFolderTreeView::setCurrentDir(const TFilePath &dirPath) {
  assert(TFileStatus(dirPath).isDirectory());

  list<TFilePath> ancestors;
  ancestors.push_back(dirPath);

  if (!dirPath.isRoot()) {
    TFilePath fp = dirPath;
    while (!(fp = fp.getParentDir()).isRoot()) ancestors.push_front(fp);
    ancestors.push_front(fp);
  }

  TTreeViewItem *folder = m_myComputer;
  while (!ancestors.empty() && folder) {
    TFilePath fp = ancestors.front();
    folder       = openChild(folder, fp);
    ancestors.pop_front();
  }

  if (folder) {
    updateVisibleItems();
    select(folder);

    TRect rect;
    getPlacement(m_selectedItemIndex, rect);
    int y = rect.getP00().y;
    if (y <= 0)
      y = m_yoffset - y;
    else if (y > getLy())
      y = m_yoffset - (rect.y1 - getLy());
    else
      y = m_yoffset;

    scrollToY(y);

    updateScrollbars();
  }
}

};  // anonymous namespace

//==============================================================================
//==============================================================================

class FileBrowser::Data {
public:
  Data() {}
  ~Data() {}

  FileFolderTreeView *m_tree;
  DirectoryBrowser *m_panel;
  TScrollbar *m_panelScb, *m_treeVscb, *m_treeHscb;

  TIconButton *m_upButton, *m_createFolderButton, *m_deleteFileButton;

  vector<string> m_fileTypes;
};

//-------------------------------------------------------------------

FileBrowser::FileBrowser(TWidget *parent, string name,
                         const vector<string> &fileTypes)
    : TWidget(parent, name), m_data(new Data()) {
  m_data->m_fileTypes = fileTypes;

  m_data->m_panel = new DirectoryBrowser(this, fileTypes);
  m_data->m_tree  = new FileFolderTreeView(this, m_data->m_panel);

  m_data->m_panelScb = new TScrollbar(this);
  m_data->m_treeHscb = new TScrollbar(this);
  m_data->m_treeVscb = new TScrollbar(this);

  m_data->m_panel->setScrollbars(0, m_data->m_panelScb);
  m_data->m_tree->setScrollbars(m_data->m_treeHscb, m_data->m_treeVscb);

  setBackgroundColor(Gray210);

  m_data->m_upButton = new TIconButton(this, parentfolder);
  tconnect(*m_data->m_upButton, this, &FileBrowser::selectParentDirectory);

  m_data->m_createFolderButton = new TIconButton(this, newfolder);
  m_data->m_deleteFileButton   = new TIconButton(this, trash);
}

//-------------------------------------------------------------------

FileBrowser::~FileBrowser() { delete m_data; }

//-------------------------------------------------------------------

void FileBrowser::setFilter(const vector<string> &fileTypes) {
  m_data->m_fileTypes = fileTypes;
  m_data->m_panel->setFilter(fileTypes);
}

//-------------------------------------------------------------------

void FileBrowser::draw() {
  setColor(White);
  fillRect(0, 0, 4, getLy() - 1);

  TPoint p;
  p = m_data->m_treeVscb->getGeometry().getP11();

  setColor(Gray150);
  drawLine(p.x + 1, 1, p.x + 1, p.y);

  p = m_data->m_panel->getGeometry().getP01();
  drawLine(p.x - 1, 1, p.x - 1, p.y);
  drawLine(p.x - 1, p.y + 1, getLx() - 2, p.y + 1);
}

//-------------------------------------------------------------------

void FileBrowser::configureNotify(const TDimension &d) {
  /*
TRect rect = getBounds();
int x = 200;
int w = 15;
int xa = x-w-4;
TDimension scbSize(w, rect.getLy());
m_data->m_tree->setGeometry(rect.x0,rect.y0,xa-1,rect.y1);
m_data->m_treeVscb->setGeometry(TPoint(xa,rect.y0),scbSize);

m_data->m_treeHscb->setGeometry(rect.x0, rect.y0,
x-1-scbSize,rect.y0+scbSize-1);

int buttonBarHeight = 30;

rect.y1 -= buttonBarHeight;
scbSize.ly -= buttonBarHeight;

m_data->m_panel->setGeometry(x,rect.y0,rect.x1-w,rect.y1);
m_data->m_panelScb->setGeometry(TPoint(rect.x1-w+1,rect.y0),scbSize);

int y = rect.y1+4;

m_data->m_upButton->setPosition(x-1, y);
m_data->m_createFolderButton->setPosition(x+25, y);
m_data->m_deleteFileButton->setPosition(rect.x1-26, y);

invalidate();

*/

  const int scbSize = 15;

  TRect rect = getBounds();
  int x      = 196;

  m_data->m_tree->setGeometry(rect.x0, rect.y0 + scbSize, x - 1 - scbSize,
                              rect.y1);
  m_data->m_treeVscb->setGeometry(x - scbSize, rect.y0 + scbSize, x - 1,
                                  rect.y1);
  m_data->m_treeHscb->setGeometry(rect.x0, rect.y0, x - 1 - scbSize,
                                  rect.y0 + scbSize - 1);

  int buttonBarHeight = 29;

  rect.y1 -= buttonBarHeight;

  m_data->m_panel->setGeometry(x + 4, rect.y0, rect.x1 - scbSize, rect.y1);
  m_data->m_panelScb->setGeometry(rect.x1 - scbSize + 1, rect.y0, rect.x1,
                                  rect.y1);

  int y = rect.y1 + 4;
  x += 4;
  m_data->m_upButton->setPosition(x - 1, y);
  m_data->m_createFolderButton->setPosition(x + 25, y);
  m_data->m_deleteFileButton->setPosition(rect.x1 - 26, y);

  m_data->m_upButton->invalidate();
  m_data->m_createFolderButton->invalidate();
  m_data->m_deleteFileButton->invalidate();
  m_data->m_treeVscb->invalidate();
  m_data->m_treeHscb->invalidate();
  m_data->m_tree->invalidate();
  invalidate();
}

//-------------------------------------------------------------------

void FileBrowser::setBackgroundColor(const TGuiColor &color) {
  TWidget::setBackgroundColor(color);
  m_data->m_panel->setBackgroundColor(color);
  m_data->m_tree->setBackgroundColor(color);
}

//-------------------------------------------------------------------

void FileBrowser::setFileSelChangeAction(GenericFileBrowserAction *action) {
  m_data->m_panel->setFileSelChangeAction(action);
}

//-------------------------------------------------------------------

void FileBrowser::setFileDblClickAction(GenericFileBrowserAction *action) {
  m_data->m_panel->setFileDblClickAction(action);
}

//-------------------------------------------------------------------

TFilePath FileBrowser::getCurrentDir() const {
  FileFolderItem *item =
      dynamic_cast<FileFolderItem *>(m_data->m_tree->getSelectedItem());
  if (item)
    return item->getPath();
  else
    return TFilePath();
}

//-------------------------------------------------------------------

void FileBrowser::setCurrentDir(const TFilePath &dirPath) {
  m_data->m_tree->setCurrentDir(dirPath);
}

//-------------------------------------------------------------------

void FileBrowser::selectParentDirectory() { m_data->m_tree->selectParent(); }
