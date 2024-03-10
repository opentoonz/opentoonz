
#include "xshcolumnviewer.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "columnselection.h"
#include "xsheetdragtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/intfield.h"
#include "toonzqt/fxiconmanager.h"

// TnzLib includes
#include "toonz/txshcolumn.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/stage2.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/columnfan.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/fxcommand.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"
#include "toonz/childstack.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshmeshcolumn.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/levelset.h"

// TnzCore includes
#include "tconvert.h"

#include <QApplication>
#include <QMainWindow>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QToolTip>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDesktopWidget>

#include <QBitmap>

//=============================================================================

namespace {
const QSet<TXshLevel *> getLevels(TXshColumn *column) {
  QSet<TXshLevel *> levels;

  TXshCellColumn *cellColumn = column->getCellColumn();
  if (cellColumn) {
    int i, r0, r1;
    cellColumn->getRange(r0, r1);
    for (i = r0; i <= r1; i++) {
      TXshCell cell = cellColumn->getCell(i);
      // TXshSimpleLevel *sl = cell.getSimpleLevel();
      if (!cell.isEmpty()) levels.insert(cell.m_level.getPointer());
    }
  }
  return levels;
}

bool containsRasterLevel(TColumnSelection *selection) {
  if (!selection || selection->isEmpty()) return false;
  std::set<int> indexes = selection->getIndices();
  TXsheet *xsh          = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (auto const &e : indexes) {
    TXshColumn *col = xsh->getColumn(e);
    if (!col || col->getColumnType() != TXshColumn::eLevelType) continue;

    TXshCellColumn *cellCol = col->getCellColumn();
    if (!cellCol) continue;

    int i;
    for (i = 0; i < cellCol->getMaxFrame() + 1; i++) {
      TXshCell cell = cellCol->getCell(i);
      if (cell.isEmpty()) continue;
      TXshSimpleLevel *level = cell.getSimpleLevel();
      if (!level || level->getChildLevel() ||
          level->getProperties()->getDirtyFlag())
        continue;
      int type = level->getType();
      if (type == OVL_XSHLEVEL || type == TZP_XSHLEVEL) return true;
    }
  }
  return false;
}

const QIcon getColorChipIcon(TPixel32 color) {
  QColor qCol((int)color.r, (int)color.g, (int)color.b, (int)color.m);
  QPixmap pixmap(12, 12);
  if (color.m == TPixel32::maxChannelValue) {
    pixmap.fill(qCol);
    return QIcon(pixmap);
  }
  static QPixmap checkPm;
  if (checkPm.isNull()) {
    checkPm = QPixmap(12, 12);
    checkPm.fill(Qt::white);
    QPainter cp(&checkPm);
    cp.fillRect(0, 0, 6, 6, Qt::black);
    cp.fillRect(6, 6, 6, 6, Qt::black);
  }
  pixmap = checkPm;
  QPainter p(&pixmap);
  p.fillRect(0, 0, 12, 12, qCol);
  return QIcon(pixmap);
}

bool containsVectorLevel(int col) {
  TXshColumn *column =
      TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(col);
  TXshColumn::ColumnType type = column->getColumnType();
  if (type != TXshColumn::eLevelType) return false;

  const QSet<TXshLevel *> levels = getLevels(column);
  QSet<TXshLevel *>::const_iterator it2;
  bool isVector = false;
  for (it2 = levels.begin(); it2 != levels.end(); it2++) {
    TXshLevel *lvl = *it2;
    int type       = lvl->getType();
    if (type == PLI_XSHLEVEL) {
      isVector = true;
      return true;
    }
  }
  return false;
}

QIcon createLockIcon(XsheetViewer *viewer) {
  QColor bgColor_on, bgColor_off;

  QString svgFilePath_on  = viewer->getXsheetLockButtonOnImage();
  QString svgFilePath_off = viewer->getXsheetLockButtonOffImage();

  QPixmap pm_on  = svgToPixmap(svgFilePath_on, QSize(16, 16),
                               Qt::KeepAspectRatio, bgColor_on);
  QPixmap pm_off = svgToPixmap(svgFilePath_off, QSize(16, 16),
                               Qt::KeepAspectRatio, bgColor_off);

  QIcon lockIcon;
  lockIcon.addPixmap(pm_off);
  lockIcon.addPixmap(pm_on, QIcon::Normal, QIcon::On);

  return lockIcon;
}

bool isCtrlPressed = false;
}  // namespace

//=============================================================================
// ColumnMaskUndo
//-----------------------------------------------------------------------------
class ColumnMaskUndo final : public TUndo {
  int m_col;
  bool m_isMask;
  std::string m_name;

public:
  ColumnMaskUndo(int column, bool isMask, std::string name)
      : m_col(column), m_isMask(isMask), m_name(name) {}
  ~ColumnMaskUndo() {}

  void undo() const override {
    TXshColumn *column =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(m_col);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) return;

    if (containsVectorLevel(m_col)) {
      column->setIsMask(m_isMask);
      TApp::instance()->getCurrentScene()->notifySceneChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    }
  }

  void redo() const override {
    TXshColumn *column =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(m_col);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) return;

    if (containsVectorLevel(m_col)) {
      column->setIsMask(!m_isMask);
      TApp::instance()->getCurrentScene()->notifySceneChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    }
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Toggle vector column as mask. ");
    return str;
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//-----------------------------------------------------------------------------

static void getVolumeCursorRect(QRect &out, double volume,
                                const QPoint &origin) {
  int ly = 60;
  int v  = tcrop(0, ly, (int)(volume * ly));
  out.setX(origin.x() + 11);
  out.setY(origin.y() + 60 - v);
  out.setWidth(8);
  out.setHeight(8);
}

//=============================================================================
// MotionPathMenu
//-----------------------------------------------------------------------------

MotionPathMenu::MotionPathMenu(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_mDeleteRect(QRect(0, 0, ColumnWidth - 13, RowHeight))
    , m_mNormalRect(QRect(0, RowHeight, ColumnWidth - 13, RowHeight))
    , m_mRotateRect(QRect(0, RowHeight * 2, ColumnWidth - 13, RowHeight))
    , m_pos(QPoint()) {
  setMouseTracking(true);
  setFixedSize(ColumnWidth - 12, 3 * RowHeight);
  setWindowFlags(Qt::FramelessWindowHint);
}

//-----------------------------------------------------------------------------

MotionPathMenu::~MotionPathMenu() {}

//-----------------------------------------------------------------------------

void MotionPathMenu::paintEvent(QPaintEvent *) {
  QPainter p(this);

  static QPixmap motionPixmap = QPixmap(":Resources/motionpath.svg");
  static QPixmap motionDeletePixmap =
      QPixmap(":Resources/motionpath_delete.svg");
  static QPixmap motionRotatePixmap = QPixmap(":Resources/motionpath_rot.svg");

  QColor overColor = QColor(49, 106, 197);

  p.fillRect(m_mDeleteRect,
             QBrush((m_mDeleteRect.contains(m_pos)) ? overColor : grey225));
  p.drawPixmap(m_mDeleteRect, motionDeletePixmap);

  p.fillRect(m_mNormalRect,
             QBrush((m_mNormalRect.contains(m_pos)) ? overColor : grey225));
  p.drawPixmap(m_mNormalRect, motionPixmap);

  p.fillRect(m_mRotateRect,
             QBrush((m_mRotateRect.contains(m_pos)) ? overColor : grey225));
  p.drawPixmap(m_mRotateRect, motionRotatePixmap);
}

//-----------------------------------------------------------------------------

void MotionPathMenu::mousePressEvent(QMouseEvent *event) {
  m_pos                   = event->pos();
  TStageObjectId objectId = TApp::instance()->getCurrentObject()->getObjectId();
  TStageObject *pegbar =
      TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
          objectId);

  if (m_mDeleteRect.contains(m_pos))
    pegbar->setStatus(TStageObject::XY);
  else if (m_mNormalRect.contains(m_pos)) {
    pegbar->setStatus(TStageObject::PATH);
    TApp::instance()->getCurrentObject()->setIsSpline(true);
  } else if (m_mRotateRect.contains(m_pos)) {
    pegbar->setStatus(TStageObject::PATH_AIM);
    TApp::instance()->getCurrentObject()->setIsSpline(true);
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  hide();
}

//-----------------------------------------------------------------------------

void MotionPathMenu::mouseMoveEvent(QMouseEvent *event) {
  m_pos = event->pos();
  update();
}

//-----------------------------------------------------------------------------

void MotionPathMenu::mouseReleaseEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void MotionPathMenu::leaveEvent(QEvent *event) { hide(); }

//=============================================================================
// ChangeObjectWidget
//-----------------------------------------------------------------------------

ChangeObjectWidget::ChangeObjectWidget(QWidget *parent)
    : QListWidget(parent), m_width(40), m_objectHandle(0), m_xsheetHandle(0) {
  setMouseTracking(true);
  setObjectName("XshColumnChangeObjectWidget");
  setAutoFillBackground(true);

  bool ret = connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this,
                     SLOT(onItemSelected(QListWidgetItem *)));
  assert(ret);
}

//-----------------------------------------------------------------------------

ChangeObjectWidget::~ChangeObjectWidget() {}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::show(const QPoint &pos) {
  refresh();
  int scrollbarW = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  int itemNumber = count();
  if (itemNumber > 10) {
    itemNumber = 10;
    m_width += scrollbarW;
  }
  int height = 0;
  for (int i = 0; i < itemNumber; i++) height += sizeHintForRow(i);
  setGeometry(pos.x(), pos.y(), m_width, height + 2);
  QListWidget::show();
  raise();
  setFocus();
  scrollToItem(currentItem());
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::setObjectHandle(TObjectHandle *objectHandle) {
  m_objectHandle = objectHandle;
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::setXsheetHandle(TXsheetHandle *xsheetHandle) {
  m_xsheetHandle = xsheetHandle;
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::mouseMoveEvent(QMouseEvent *event) {
  QListWidgetItem *currentWidgetItem = itemAt(event->pos());
  if (!currentWidgetItem) return;
  clearSelection();
  currentWidgetItem->setSelected(true);
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::wheelEvent(QWheelEvent *event) {
  QListWidget::wheelEvent(event);
  event->accept();
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::focusOutEvent(QFocusEvent *e) {
  if (!isVisible()) return;
  hide();
  if (parentWidget()) {
    parentWidget()->update();
  }
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::addText(const QString &text, const QString &display) {
  QListWidgetItem *item = new QListWidgetItem(display);
  item->setData(Qt::UserRole, text);
  addItem(item);
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::addText(const QString &text, const QColor &textColor) {
  QListWidgetItem *item = new QListWidgetItem(text);
  item->setForeground(textColor);
  addItem(item);
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::addText(const TStageObjectId &id,
                                 const QString &display,
                                 const QColor &identColor) {
  QListWidgetItem *item = new QListWidgetItem(display);
  QPixmap pixmap(4, 8);
  pixmap.fill(identColor);
  QIcon icon(pixmap);
  item->setIcon(icon);
  item->setData(Qt::UserRole, id.getCode());
  addItem(item);
}

//=============================================================================
// ChangeObjectParent
//-----------------------------------------------------------------------------

ChangeObjectParent::ChangeObjectParent(QWidget *parent)
    : ChangeObjectWidget(parent) {}

//-----------------------------------------------------------------------------

ChangeObjectParent::~ChangeObjectParent() {}

//-----------------------------------------------------------------------------

void ChangeObjectParent::refresh() {
  clear();
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  XsheetViewer *viewer           = TApp::instance()->getCurrentXsheetViewer();
  TXsheet *xsh                   = m_xsheetHandle->getXsheet();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObjectId parentId = xsh->getStageObject(currentObjectId)->getParent();
  TStageObject *currentObject        = xsh->getStageObject(currentObjectId);
  std::list<TStageObject *> children = currentObject->getChildren();
  TStageObjectTree *tree             = xsh->getStageObjectTree();
  int objectCount                    = tree->getStageObjectCount();
  QList<TStageObjectId> pegbarListID, columnListID;
  QList<QString> pegbarListTr, columnListTr;
  QList<QColor> pegbarListColor, columnListColor;
  TStageObjectId currentId;
  QString theLongestTxt;
  int i;
  for (i = 0; i < objectCount; i++) {
    TStageObjectId id = tree->getStageObject(i)->getId();
    int index         = id.getIndex();
    QString indexStr(std::to_string(id.getIndex() + 1).c_str());
    QColor newTextBG;

    // Remove childs from parent list
    bool found = (std::find(children.begin(), children.end(),
                            xsh->getStageObject(id)) != children.end());
    if (id == currentObjectId || found) continue;

    TStageObjectId newTextID = id;
    QString newTextTr;
    if (tree->getStageObject(i)->hasSpecifiedName())
      newTextTr = QString::fromStdString(tree->getStageObject(i)->getName());
    else
      newTextTr = getNameTr(id);

    if (id.isTable())
      newTextBG = viewer->getTableColor();
    else if (id.isPegbar())
      newTextBG = viewer->getPegColor();
    else if (id.isCamera()) {
      bool isActive = (id == xsh->getStageObjectTree()->getCurrentCameraId());
      newTextBG     = isActive ? viewer->getActiveCameraColor()
                               : viewer->getOtherCameraColor();
    } else if (id.isColumn() && (!xsh->isColumnEmpty(index))) {
      TXshColumn *colx = xsh->getColumn(index);
      if (!colx->canBeParent()) continue;

      QColor unused;
      viewer->getColumnColor(newTextBG, unused, id.getIndex(), xsh);
    } else
      continue;

    if (id == parentId) currentId = newTextID;
    if (newTextTr.length() > theLongestTxt.length()) theLongestTxt = newTextTr;
    if (id.isColumn()) {
      columnListID.append(newTextID);
      columnListTr.append(newTextTr);
      columnListColor.append(newTextBG);
    } else {
      pegbarListID.append(newTextID);
      pegbarListTr.append(newTextTr);
      pegbarListColor.append(newTextBG);
    }
  }
  for (i = 0; i < columnListID.size(); i++)
    addText(columnListID.at(i), columnListTr.at(i), columnListColor.at(i));
  for (i = 0; i < pegbarListID.size(); i++)
    addText(pegbarListID.at(i), pegbarListTr.at(i), pegbarListColor.at(i));

  m_width = fontMetrics().width(theLongestTxt) + 32;
  selectCurrent(currentId);
}

//-----------------------------------------------------------------------------

QString ChangeObjectParent::getNameTr(const TStageObjectId id) {
  if (id.isTable()) return tr("Table");
  // return untranslated string for other types
  else
    return QString::fromStdString(id.toString());
}

//-----------------------------------------------------------------------------

void ChangeObjectParent::onItemSelected(QListWidgetItem *item) {
  assert(m_xsheetHandle);
  assert(m_objectHandle);

  QVariant data = item->data(Qt::UserRole);
  if (!data.isValid()) return;

  TStageObjectId newStageObjectId;
  newStageObjectId.setCode(data.toUInt());

  TXsheet *xsh                   = m_xsheetHandle->getXsheet();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObjectId currentParentId =
      xsh->getStageObject(currentObjectId)->getParent();

  if (newStageObjectId == currentObjectId) return;

  if (newStageObjectId == currentParentId) {
    hide();
    return;
  }

  TStageObject *stageObject =
      m_xsheetHandle->getXsheet()->getStageObject(currentObjectId);
  TStageObjectCmd::setParent(currentObjectId, newStageObjectId, "B",
                             m_xsheetHandle);

  hide();
  m_objectHandle->notifyObjectIdChanged(false);
  m_xsheetHandle->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ChangeObjectParent::selectCurrent(const TStageObjectId &id) {
  clearSelection();
  int numRows = count();
  for (int row = 0; row < numRows; row++) {
    QListWidgetItem *it = item(row);
    QVariant display    = it->data(Qt::UserRole);
    if (!display.isValid()) continue;
    if (id.getCode() == display.toUInt()) {
      setCurrentItem(it);
      return;
    }
  }
}

//=============================================================================
// ChangeObjectHandle
//-----------------------------------------------------------------------------

ChangeObjectHandle::ChangeObjectHandle(QWidget *parent)
    : ChangeObjectWidget(parent) {}

//-----------------------------------------------------------------------------

ChangeObjectHandle::~ChangeObjectHandle() {}

//-----------------------------------------------------------------------------

void ChangeObjectHandle::refresh() {
  clear();
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
  TXsheet *xsh         = m_xsheetHandle->getXsheet();
  assert(xsh);
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObject *stageObject      = xsh->getStageObject(currentObjectId);
  m_width                        = 28;

  int i;
  QString str;
  QColor colorHndHook = viewer->getPreviewFrameTextColor();
  QColor colorHndDef  = viewer->getSelectedColumnTextColor();
  QColor colorHndNone = viewer->getTextColor();
  if (stageObject->getParent().isColumn()) {
    for (i = 0; i < 20; i++) addText(str.number(20 - i), colorHndHook);
  }
  for (i = 0; i < 26; i++) {
    if (i == 1)
      addText(QString("B"), colorHndDef);
    else
      addText(QString(char('A' + i)), colorHndNone);
  }

  std::string handle = stageObject->getParentHandle();
  if (handle[0] == 'H' && handle.length() > 1) handle = handle.substr(1);

  selectCurrent(QString::fromStdString(handle));
}

//-----------------------------------------------------------------------------

void ChangeObjectHandle::onItemSelected(QListWidgetItem *item) {
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  QString text                   = item->text();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  QString handle                 = text;
  if (text.toInt() != 0) handle = QString("H") + handle;
  if (handle.isEmpty()) return;
  std::vector<TStageObjectId> ids;
  ids.push_back(currentObjectId);
  TStageObjectCmd::setParentHandle(ids, handle.toStdString(), m_xsheetHandle);
  hide();
  m_objectHandle->notifyObjectIdChanged(false);
  m_xsheetHandle->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ChangeObjectHandle::selectCurrent(const QString &text) {
  clearSelection();
  int numRows = count();
  for (int row = 0; row < numRows; row++) {
    QListWidgetItem *it = item(row);
    if (text == it->text()) {
      setCurrentItem(it);
      return;
    }
  }
}

//=============================================================================
// RenameColumnField
//-----------------------------------------------------------------------------

RenameColumnField::RenameColumnField(QWidget *parent, XsheetViewer *viewer)
    : QLineEdit(parent), m_col(-1) {
  setFixedSize(20, 20);
  connect(this, SIGNAL(returnPressed()), SLOT(renameColumn()));
}

//-----------------------------------------------------------------------------

void RenameColumnField::show(const QRect &rect, int col) {
  move(rect.topLeft());
  setFixedSize(rect.size());
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  setFont(font);
  m_col = col;

  TXsheet *xsh    = m_xsheetHandle->getXsheet();
  int cameraIndex = xsh->getCameraColumnIndex();
  std::string name =
      col >= 0 ? xsh->getStageObject(TStageObjectId::ColumnId(col))->getName()
               : xsh->getStageObject(TStageObjectId::CameraId(cameraIndex))
                     ->getName();
  TXshColumn *column          = xsh->getColumn(col);
  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn)
    name = ::to_string(zColumn->getZeraryColumnFx()->getZeraryFx()->getName());
  setText(QString(name.c_str()));
  selectAll();

  QWidget::show();
  raise();
  setFocus();
}

//-----------------------------------------------------------------------------

void RenameColumnField::renameColumn() {
  std::string newName     = text().toStdString();
  int cameraIndex         = m_xsheetHandle->getXsheet()->getCameraColumnIndex();
  TStageObjectId columnId = m_col >= 0 ? TStageObjectId::ColumnId(m_col)
                                       : TStageObjectId::CameraId(cameraIndex);
  TXshColumn *column =
      m_xsheetHandle->getXsheet()->getColumn(columnId.getIndex());
  if (!column && m_col >= 0) {
    m_xsheetHandle->getXsheet()->insertColumn(m_col);
    column = m_xsheetHandle->getXsheet()->getColumn(columnId.getIndex());
  }

  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn)
    TFxCommand::renameFx(zColumn->getZeraryColumnFx(), ::to_wstring(newName),
                         m_xsheetHandle);
  else
    TStageObjectCmd::rename(columnId, newName, m_xsheetHandle);

  m_xsheetHandle->notifyXsheetChanged();
  m_col = -1;
  setText("");
  hide();
}

//-----------------------------------------------------------------------------

void RenameColumnField::focusOutEvent(QFocusEvent *e) {
  std::wstring newName = text().toStdWString();
  if (!newName.empty())
    renameColumn();
  else
    hide();

  QLineEdit::focusOutEvent(e);
}

//=============================================================================
// ColumnArea
//-----------------------------------------------------------------------------

void ColumnArea::onControlPressed(bool pressed) {
  isCtrlPressed = pressed;
  update();
}

const bool ColumnArea::isControlPressed() { return isCtrlPressed; }

//-----------------------------------------------------------------------------

ColumnArea::DrawHeader::DrawHeader(ColumnArea *nArea, QPainter &nP, int nCol)
    : area(nArea), p(nP), col(nCol), reservedLevel(nullptr) {
  m_viewer = area->m_viewer;
  o        = m_viewer->orientation();
  app      = TApp::instance();
  xsh      = m_viewer->getXsheet();
  column   = xsh->getColumn(col);
  isEmpty  = col >= 0 ? xsh->isColumnEmpty(col) : false;

  if (isEmpty && Preferences::instance()->isLinkColumnNameWithLevelEnabled() &&
      m_viewer->getXsheet()
          ->getStageObject(TStageObjectId::ColumnId(col))
          ->hasSpecifiedName()) {
    std::string columnName = m_viewer->getXsheet()
                                 ->getStageObject(TStageObjectId::ColumnId(col))
                                 ->getName();
    ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();
    TLevelSet *levelSet = scene->getLevelSet();
    reservedLevel       = levelSet->getLevel(to_wstring(columnName));
  }

  TStageObjectId currentColumnId = app->getCurrentObject()->getObjectId();

  // check if the column is current
  isCurrent = false;
  if (currentColumnId ==
      TStageObjectId::CameraId(xsh->getCameraColumnIndex()))  // CAMERA
    isCurrent = col == -1;
  else
    isCurrent = m_viewer->getCurrentColumn() == col;

  orig = m_viewer->positionToXY(CellPosition(0, std::max(col, -1)));
}

void ColumnArea::DrawHeader::prepare() const {
  // Preparing painter
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);

  p.setFont(font);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

//-----------------------------------------------------------------------------
const QPixmap &ColumnArea::Pixmaps::sound() {
  static QPixmap sound = generateIconPixmap("sound_header");
  return sound;
}
const QPixmap &ColumnArea::Pixmaps::soundPlaying() {
  static QPixmap soundPlaying = generateIconPixmap("sound_header_on");
  return soundPlaying;
}

//-----------------------------------------------------------------------------

void ColumnArea::DrawHeader::levelColors(QColor &columnColor,
                                         QColor &dragColor) const {
  if (col < 0) {
    TStageObjectId cameraId =
        m_viewer->getXsheet()->getStageObjectTree()->getCurrentCameraId();
    bool isActive =
        cameraId.getIndex() == m_viewer->getXsheet()->getCameraColumnIndex();
    columnColor = isActive ? m_viewer->getActiveCameraColor()
                           : m_viewer->getOtherCameraColor();
    dragColor   = isActive ? m_viewer->getActiveCameraColor()
                           : m_viewer->getOtherCameraColor();
    return;
  }
  enum { Normal, Reference, Control } usage = Reference;
  if (column) {
    if (column->isControl()) usage = Control;
    if (column->isRendered() || column->getMeshColumn()) usage = Normal;
  }

  if (reservedLevel) {
    int ltype;
    m_viewer->getCellTypeAndColors(ltype, columnColor, dragColor,
                                   TXshCell(reservedLevel, TFrameId()));
  } else if (usage == Reference) {
    columnColor = m_viewer->getReferenceColumnColor();
    dragColor   = m_viewer->getReferenceColumnBorderColor();
  } else
    m_viewer->getColumnColor(columnColor, dragColor, col, xsh);
}
void ColumnArea::DrawHeader::soundColors(QColor &columnColor,
                                         QColor &dragColor) const {
  m_viewer->getColumnColor(columnColor, dragColor, col, xsh);
}
void ColumnArea::DrawHeader::paletteColors(QColor &columnColor,
                                           QColor &dragColor) const {
  enum { Normal, Reference, Control } usage = Reference;
  if (column) {  // Check if column is a mask
    if (column->isControl()) usage = Control;
    if (column->isRendered()) usage = Normal;
  }

  if (usage == Reference) {
    columnColor = m_viewer->getReferenceColumnColor();
    dragColor   = m_viewer->getReferenceColumnBorderColor();
  } else {
    columnColor = m_viewer->getPaletteColumnColor();
    dragColor   = m_viewer->getPaletteColumnBorderColor();
  }
}

void ColumnArea::DrawHeader::drawBaseFill(const QColor &columnColor,
                                          const QColor &dragColor) const {
  // check if the column is reference
  bool isEditingSpline = app->getCurrentObject()->isSpline();

  QRect rect = o->rect((col < 0) ? PredefinedRect::CAMERA_LAYER_HEADER
                                 : PredefinedRect::LAYER_HEADER)
                   .translated(orig);

  int x0 = rect.left();
  int x1 = rect.right();
  int y0 = rect.top();
  int y1 = rect.bottom();

  // Fill base color, in timeline view adjust it right upto thumbnail so column
  // head color doesn't show under icon switches.
  if (isEmpty && !reservedLevel)
    p.fillRect(o->isVerticalTimeline() ? rect : rect.adjusted(80, 0, 0, 0),
               m_viewer->getEmptyColumnHeadColor());
  else if (col < 0)
    p.fillRect(o->isVerticalTimeline() ? rect : rect.adjusted(80, 0, 0, 0),
               columnColor);
  else {
    QBrush brush(columnColor,
                 (reservedLevel) ? Qt::DiagCrossPattern : Qt::SolidPattern);
    p.fillRect(o->isVerticalTimeline() ? rect : rect.adjusted(80, 0, 0, 0),
               brush);

    if (o->flag(PredefinedFlag::DRAG_LAYER_VISIBLE)) {
      // column handle
      QRect sideBar = o->rect(PredefinedRect::DRAG_LAYER).translated(x0, y0);

      if (o->flag(PredefinedFlag::DRAG_LAYER_BORDER)) {
        p.setPen(m_viewer->getVerticalLineColor());
        p.drawRect(sideBar);
      }

      p.fillRect(sideBar, sideBar.contains(area->m_pos)
                              ? m_viewer->getXsheetDragBarHighlightColor()
                              : dragColor);
    }
  }

  p.setPen(m_viewer->getVerticalLineHeadColor());
  QLine vertical =
      o->verticalLine(m_viewer->columnToLayerAxis(col), o->frameSide(rect));
  if (isEmpty || o->isVerticalTimeline()) p.drawLine(vertical);

  // highlight selection
  bool isSelected =
      m_viewer->getColumnSelection()->isColumnSelected(col) && !isEditingSpline;
  bool isCameraSelected = col == -1 && isCurrent && !isEditingSpline;

  QColor pastelizer(m_viewer->getColumnHeadPastelizer());

  QColor colorSelection(m_viewer->getSelectedColumnHead());
  p.fillRect(o->isVerticalTimeline() ? rect : rect.adjusted(80, 0, 0, 0),
             isSelected ? colorSelection : pastelizer);
}

void ColumnArea::DrawHeader::drawEye() const {
  if (isEmpty || !o->flag(PredefinedFlag::EYE_AREA_VISIBLE)) return;
  if (col < 0 && o->isVerticalTimeline())
    return;  // no preview eye in the camera column
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isPreviewVisible() ? PREVIEW_OFF_XSHBUTTON
                                               : PREVIEW_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath,
                      !o->isVerticalTimeline());

  QRect prevViewRect = o->rect(PredefinedRect::EYE_AREA).translated(orig);
  QRect eyeRect      = o->rect(PredefinedRect::EYE).translated(orig);
  // preview visible toggle
  if (o->isVerticalTimeline())
    p.setPen(m_viewer->getColumnIconLineColor());    // Preview border color
  else
    p.setPen(m_viewer->getTimelineIconLineColor());  // Preview border color

  if (col < 0 || column->getSoundTextColumn()) {
    if (o->flag(PredefinedFlag::EYE_AREA_BORDER)) p.drawRect(prevViewRect);
    return;
  }

  p.fillRect(prevViewRect, bgColor);  //   PreviewVisibleColor);
  if (o->flag(PredefinedFlag::EYE_AREA_BORDER)) p.drawRect(prevViewRect);

  // For Legacy (layout=1), Preview Off button is not displayed in Xsheet mode
  if (o->isVerticalTimeline() &&
      m_viewer->getXsheetLayout() == QString("Classic") &&
      buttonType == PREVIEW_OFF_XSHBUTTON)
    return;

  QPixmap icon =
      svgToPixmap(svgFilePath, eyeRect.size(), Qt::KeepAspectRatio, bgColor);

  p.drawPixmap(eyeRect, icon);
}

void ColumnArea::DrawHeader::drawPreviewToggle(int opacity) const {
  if (isEmpty || !o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_VISIBLE)) return;
  if (col < 0 && o->isVerticalTimeline())
    return;  // no camstand toggle in the camera column
  // camstand visible toggle
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isCamstandVisible() ? CAMSTAND_OFF_XSHBUTTON
                   : opacity < 255              ? CAMSTAND_TRANSP_XSHBUTTON
                                                : CAMSTAND_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath,
                      !o->isVerticalTimeline());

  QRect tableViewRect =
      o->rect(PredefinedRect::PREVIEW_LAYER_AREA).translated(orig);
  QRect tableViewImgRect =
      o->rect(PredefinedRect::PREVIEW_LAYER).translated(orig);

  if (o->isVerticalTimeline())
    p.setPen(m_viewer->getColumnIconLineColor());    // Camstand border color
  else
    p.setPen(m_viewer->getTimelineIconLineColor());  // Camstand border color

  if (col < 0 || column->getPaletteColumn() || column->getSoundTextColumn()) {
    if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
      p.drawRect(tableViewRect);
    return;
  }

  p.fillRect(tableViewRect, bgColor);  //   CamStandVisibleColor);
  if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
    p.drawRect(tableViewRect);

  // For Legacy (layout=1), Camstand Off button is not displayed in Xsheet mode
  if (o->isVerticalTimeline() &&
      m_viewer->getXsheetLayout() == QString("Classic") &&
      buttonType == CAMSTAND_OFF_XSHBUTTON)
    return;

  QPixmap icon = svgToPixmap(svgFilePath, tableViewImgRect.size(),
                             Qt::KeepAspectRatio, bgColor);

  p.drawPixmap(tableViewImgRect, icon);
}

// shows united visibility toggle, which has the same state as the camstand
// toggle (preview toggle should synchonize with the camstand toggle)
void ColumnArea::DrawHeader::drawUnifiedViewToggle(int opacity) const {
  if (isEmpty || !o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_VISIBLE)) return;
  if (col < 0 && o->isVerticalTimeline())
    return;  // no camstand toggle in the camera column
  // camstand visible toggle
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isCamstandVisible() ? PREVIEW_OFF_XSHBUTTON
                   : opacity < 255              ? UNIFIED_TRANSP_XSHBUTTON
                                                : PREVIEW_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath,
                      !o->isVerticalTimeline());

  QRect unifiedViewRect =
      o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA).translated(orig);
  QRect unifiedViewImgRect =
      o->rect(PredefinedRect::UNIFIEDVIEW_LAYER).translated(orig);

  if (o->isVerticalTimeline())
    p.setPen(m_viewer->getColumnIconLineColor());    // border color
  else
    p.setPen(m_viewer->getTimelineIconLineColor());  // border color

  if (col < 0 || column->getPaletteColumn() || column->getSoundTextColumn()) {
    if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
      p.drawRect(unifiedViewRect);
    return;
  }

  p.fillRect(unifiedViewRect, bgColor);
  if (o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_BORDER))
    p.drawRect(unifiedViewRect);

  QPixmap icon = svgToPixmap(svgFilePath, unifiedViewImgRect.size(),
                             Qt::KeepAspectRatio, bgColor);

  p.drawPixmap(unifiedViewImgRect, icon);
}

void ColumnArea::DrawHeader::drawLock() const {
  if (isEmpty || !o->flag(PredefinedFlag::LOCK_AREA_VISIBLE)) return;
  QColor bgColor;
  QString svgFilePath;
  int buttonType = !column->isLocked() ? LOCK_OFF_XSHBUTTON : LOCK_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, svgFilePath,
                      !o->isVerticalTimeline());

  QRect lockModeRect = o->rect((col < 0) ? PredefinedRect::CAMERA_LOCK_AREA
                                         : PredefinedRect::LOCK_AREA)
                           .translated(orig);
  QRect lockModeImgRect =
      o->rect((col < 0) ? PredefinedRect::CAMERA_LOCK : PredefinedRect::LOCK)
          .translated(orig);

  if (o->isVerticalTimeline() &&
      m_viewer->getXsheetLayout() == QString("Classic") &&
      buttonType == LOCK_OFF_XSHBUTTON && !bgColor.alpha())
    bgColor = QColor(255, 255, 255, 128);

  // lock button
  if (o->isVerticalTimeline())
    p.setPen(m_viewer->getColumnIconLineColor());    // Lock border color
  else
    p.setPen(m_viewer->getTimelineIconLineColor());  // Lock border color

  p.fillRect(lockModeRect, bgColor);
  if (o->flag(PredefinedFlag::LOCK_AREA_BORDER)) p.drawRect(lockModeRect);

  // For Legacy (layout=1), Lock Off button is not displayed in Xsheet mode
  if (o->isVerticalTimeline() &&
      m_viewer->getXsheetLayout() == QString("Classic") &&
      buttonType == LOCK_OFF_XSHBUTTON)
    return;

  QPixmap icon = svgToPixmap(svgFilePath, lockModeImgRect.size(),
                             Qt::KeepAspectRatio, bgColor);

  p.drawPixmap(lockModeImgRect, icon);
}

void ColumnArea::DrawHeader::drawConfig() const {
  if (isEmpty || (col >= 0 && !o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE)) ||
      (col < 0 && !o->flag(PredefinedFlag::CAMERA_CONFIG_AREA_VISIBLE)))
    return;
  QColor bgColor;
  QString svgIconPath;

  int buttonType = CONFIG_XSHBUTTON;

  m_viewer->getButton(buttonType, bgColor, svgIconPath,
                      !o->isVerticalTimeline());

  QRect configRect = o->rect((col < 0) ? PredefinedRect::CAMERA_CONFIG_AREA
                                       : PredefinedRect::CONFIG_AREA)
                         .translated(orig);
  QRect configImgRect = o->rect((col < 0) ? PredefinedRect::CAMERA_CONFIG
                                          : PredefinedRect::CONFIG)
                            .translated(orig);

  // config button
  if (o->isVerticalTimeline())
    p.setPen(m_viewer->getColumnIconLineColor());
  else
    p.setPen(m_viewer->getTimelineIconLineColor());
  p.fillRect(configRect, bgColor);
  if (o->flag((col < 0) ? PredefinedFlag::CAMERA_CONFIG_AREA_BORDER
                        : PredefinedFlag::CONFIG_AREA_BORDER))
    p.drawRect(configRect);

  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);

  if (zColumn || column->getPaletteColumn() || column->getSoundTextColumn())
    return;

  QPixmap icon = svgToPixmap(svgIconPath, configImgRect.size(),
                             Qt::KeepAspectRatio, bgColor);

  p.drawPixmap(configImgRect, icon);
}

void ColumnArea::DrawHeader::drawColumnNumber() const {
  if (col < 0 || isEmpty || !o->flag(PredefinedFlag::LAYER_NUMBER_VISIBLE) ||
      !Preferences::instance()->isShowColumnNumbersEnabled())
    return;

  QRect pos = o->rect(PredefinedRect::LAYER_NUMBER).translated(orig);

  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::LAYER_NUMBER_BORDER)) p.drawRect(pos);

  p.setPen((isCurrent) ? m_viewer->getSelectedColumnTextColor()
                       : m_viewer->getTextColor());

  int valign = o->isVerticalTimeline() ? Qt::AlignVCenter : Qt::AlignBottom;

  p.drawText(pos, Qt::AlignHCenter | valign | Qt::TextSingleLine,
             QString::number(col + 1));
}

void ColumnArea::DrawHeader::drawColumnName() const {
  if (!o->flag(PredefinedFlag::LAYER_NAME_VISIBLE)) return;

  TStageObjectId columnId    = m_viewer->getObjectId(col);
  TStageObject *columnObject = xsh->getStageObject(columnId);

  // Build column name
  std::string name(columnObject->getName());

  // if a single level is in the column, show the level name instead
  if (column && !columnObject->hasSpecifiedName() &&
      Preferences::instance()->getLevelNameDisplayType() ==
          Preferences::ShowLevelNameOnColumnHeader) {
    QSet<TXshLevel *> levels = getLevels(column);
    if (levels.size() == 1) name = to_string((*levels.begin())->getName());
  }
  //  if (col < 0) name = std::string("Camera");

  // ZeraryFx columns store name elsewhere
  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn && !isEmpty)
    name = ::to_string(zColumn->getZeraryColumnFx()->getZeraryFx()->getName());

  QRect columnName = o->rect((col < 0) ? PredefinedRect::CAMERA_LAYER_NAME
                                       : PredefinedRect::LAYER_NAME)
                         .translated(orig);

  bool nameBacklit = false;
  int rightadj     = -2;
  int leftadj      = 3;
  int valign = o->isVerticalTimeline() ? Qt::AlignVCenter : Qt::AlignBottom;

  if (!isEmpty) {
    if (o->isVerticalTimeline() &&
        m_viewer->getXsheetLayout() !=
            QString("Classic"))  // Legacy - No background
    {
      if (columnName.contains(area->m_pos) && col >= 0) {
        p.fillRect(columnName.adjusted(0, -1, 0, 0),
                   m_viewer->getXsheetDragBarHighlightColor());  // Qt::yellow);
        nameBacklit = true;
      } else
        p.fillRect(columnName, m_viewer->getXsheetColumnNameBgColor());
    }

    if (o->flag(PredefinedFlag::LAYER_NAME_BORDER))
      p.drawRect(columnName.adjusted(0, 0, 2, 0));

    if (o->isVerticalTimeline() &&
        m_viewer->getXsheetLayout() == QString("Classic")) {
      rightadj = -20;

      if (column->isPreviewVisible() && !column->getSoundTextColumn() &&
          !column->getPaletteColumn() && col >= 0)
        nameBacklit = true;
    } else if (Preferences::instance()->isShowColumnNumbersEnabled()) {
      if (o->isVerticalTimeline())
        rightadj = -20;
      else
        leftadj = 24;
    }

    p.setPen((isCurrent)   ? m_viewer->getSelectedColumnTextColor()
             : nameBacklit ? Qt::black
                           : m_viewer->getTextColor());
  } else
    p.setPen((isCurrent) ? m_viewer->getSelectedColumnTextColor()
                         : m_viewer->getTextColor());

  if (o->isVerticalTimeline() && col < 0) {
    QString cameraName = QString::fromStdString(name);
    p.save();
    p.translate(columnName.topRight());
    p.rotate(90);
    p.drawText(columnName.translated(-columnName.topLeft())
                   .transposed()
                   .adjusted(5, 0, 0, 0),
               Qt::AlignLeft | valign, cameraName);
    p.restore();
    return;
  }

  p.drawText(columnName.adjusted(leftadj, 0, rightadj, 0),
             Qt::AlignLeft | valign | Qt::TextSingleLine,
             QString(name.c_str()));
}

void ColumnArea::DrawHeader::drawThumbnail(QPixmap &iconPixmap) const {
  if (isEmpty) return;

  QRect thumbnailRect = o->rect((col < 0) ? PredefinedRect::CAMERA_ICON_AREA
                                          : PredefinedRect::THUMBNAIL_AREA)
                            .translated(orig);

  // Minimum layout has no thumbnail area
  if (thumbnailRect.isEmpty()) return;

  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::THUMBNAIL_AREA_BORDER)) p.drawRect(thumbnailRect);

  // sound thumbnail
  if (column->getSoundColumn()) {
    TXshSoundColumn *sc =
        xsh->getColumn(col) ? xsh->getColumn(col)->getSoundColumn() : 0;

    drawSoundIcon(sc->isPlaying());
    // Volume control now in config button. If no config button
    // draw control
    if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE))
      drawVolumeControl(sc->getVolume());
    return;
  }

  if (!o->flag((col < 0) ? PredefinedFlag::CAMERA_ICON_VISIBLE
                         : PredefinedFlag::THUMBNAIL_AREA_VISIBLE))
    return;
  QRect thumbnailImageRect = o->rect((col < 0) ? PredefinedRect::CAMERA_ICON
                                               : PredefinedRect::THUMBNAIL)
                                 .translated(orig);

  // palette thumbnail
  if (column->getPaletteColumn()) {
    p.drawPixmap(thumbnailImageRect, iconPixmap);
    return;
  }

  // soundtext thumbnail
  if (column->getSoundTextColumn()) {
    p.drawPixmap(thumbnailImageRect, iconPixmap);
    return;
  }

  // All other thumbnails
  p.setPen(m_viewer->getTextColor());

  TXshLevelColumn *levelColumn = column->getLevelColumn();
  TXshMeshColumn *meshColumn   = column->getMeshColumn();
  TXshZeraryFxColumn *zColumn  = dynamic_cast<TXshZeraryFxColumn *>(column);

  if (Preferences::instance()->getColumnIconLoadingPolicy() ==
          Preferences::LoadOnDemand &&
      ((levelColumn && !levelColumn->isIconVisible()) ||
       (meshColumn && !meshColumn->isIconVisible()) ||
       (zColumn && !zColumn->isIconVisible())) &&
      col >= 0) {
    // display nothing
  } else {
    if (!iconPixmap.isNull()) {
      p.drawPixmap(thumbnailImageRect, iconPixmap);
    }
    // notify that the column icon is already shown
    if (levelColumn)
      levelColumn->setIconVisible(true);
    else if (meshColumn)
      meshColumn->setIconVisible(true);
    else if (zColumn)
      zColumn->setIconVisible(true);
  }
}

void ColumnArea::DrawHeader::drawPegbarName() const {
  if (isEmpty || !o->flag(PredefinedFlag::PEGBAR_NAME_VISIBLE)) return;
  // the camera column may have parent pegbar, but it is not displayed for now
  if (col < 0) return;

  TStageObjectId columnId = m_viewer->getObjectId(col);
  TStageObjectId parentId = xsh->getStageObjectParent(columnId);

  QString name;
  if (xsh->getStageObject(parentId)->hasSpecifiedName())
    name = QString::fromStdString(xsh->getStageObject(parentId)->getName());
  else
    name = ChangeObjectParent::getNameTr(parentId);

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  // set font size in pixel
  font.setPixelSize(XSHEET_FONT_PX_SIZE);

  int handleWidth    = 20;
  std::string handle = xsh->getStageObject(columnId)->getParentHandle();
  if (handle == "B") handleWidth = 0;  // Default handle

  int width = QFontMetrics(font).width(name);

  while (width > o->rect(PredefinedRect::PEGBAR_NAME).width() - handleWidth) {
    name.remove(-1, 1000);
    width = QFontMetrics(font).width(name);
  }

  // pegbar name
  QRect pegbarnamerect = o->rect(PredefinedRect::PEGBAR_NAME).translated(orig);
  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::PEGBAR_NAME_BORDER)) p.drawRect(pegbarnamerect);

  if (column->getSoundColumn() || column->getSoundTextColumn()) return;

  if (Preferences::instance()->isParentColorsInXsheetColumnEnabled() &&
      column->isPreviewVisible()) {
    QColor parentColor = Qt::transparent;
    if (parentId.isCamera()) {
      bool isActive =
          (parentId == xsh->getStageObjectTree()->getCurrentCameraId());
      parentColor = isActive ? m_viewer->getActiveCameraColor()
                             : m_viewer->getOtherCameraColor();
    } else if (parentId.isPegbar()) {
      parentColor = m_viewer->getPegColor();
    } else if (parentId.isTable()) {
      parentColor = m_viewer->getTableColor();
    } else if (parentId.isColumn()) {
      int columnIndex = parentId.getIndex();
      QColor unused;
      m_viewer->getColumnColor(parentColor, unused, columnIndex, xsh);
    }
    if (parentColor != Qt::transparent) {
      QRect parentrect = pegbarnamerect;
      parentrect.adjust(1, 1, 0, 0);
      p.fillRect(parentrect, parentColor);
    }
  }

  p.setPen(m_viewer->getTextColor());

  p.drawText(pegbarnamerect.adjusted(3, 0, 0, 0),
             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, name);
}

void ColumnArea::DrawHeader::drawParentHandleName() const {
  if (col < 0 || isEmpty ||
      !o->flag(PredefinedFlag::PARENT_HANDLE_NAME_VISIBILE) ||
      column->getSoundColumn() || column->getSoundTextColumn())
    return;

  QRect parenthandleRect =
      o->rect(PredefinedRect::PARENT_HANDLE_NAME).translated(orig);
  p.setPen(Qt::yellow);  // m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::PARENT_HANDLE_NAME_BORDER))
    p.drawRect(parenthandleRect);

  TStageObjectId columnId = m_viewer->getObjectId(col);
  TStageObjectId parentId = xsh->getStageObjectParent(columnId);
  std::string handle      = xsh->getStageObject(columnId)->getParentHandle();
  if (handle[0] == 'H' && handle.length() > 1) handle = handle.substr(1);

  if (handle == "B") {  // Default handle
    QPen pen(m_viewer->getVerticalLineColor());
    pen.setStyle(Qt::PenStyle::DotLine);
    p.setPen(pen);
    int offset = parenthandleRect.x() + 2;
    p.drawLine(offset, parenthandleRect.y(), offset, parenthandleRect.bottom());
    return;
  }

  p.setPen(m_viewer->getVerticalLineColor());
  p.drawRect(parenthandleRect.adjusted(2, 0, 0, 0));

  p.setPen(m_viewer->getTextColor());
  p.drawText(parenthandleRect,
             Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine,
             QString::fromStdString(handle));
}

void ColumnArea::DrawHeader::drawFilterColor() const {
  if (col < 0 || isEmpty || column->getColorFilterId() == 0 ||
      column->getSoundColumn() || column->getSoundTextColumn() ||
      column->getPaletteColumn())
    return;

  TPixel32 filterColor = TApp::instance()
                             ->getCurrentScene()
                             ->getScene()
                             ->getProperties()
                             ->getColorFilterColor(column->getColorFilterId());

  QRect filterColorRect =
      o->rect(PredefinedRect::FILTER_COLOR).translated(orig);
  p.drawPixmap(filterColorRect, getColorChipIcon(filterColor).pixmap(12, 12));
}

void ColumnArea::DrawHeader::drawSoundIcon(bool isPlaying) const {
  QRect rect = m_viewer->orientation()
                   ->rect(PredefinedRect::SOUND_ICON)
                   .translated(orig);
  p.drawPixmap(rect, isPlaying ? Pixmaps::soundPlaying() : Pixmaps::sound());
}

void ColumnArea::DrawHeader::drawVolumeControl(double volume) const {
  // slider subdivisions
  p.setPen(m_viewer->getTextColor());
  QPoint divisionsTopLeft =
      o->point(PredefinedPoint::VOLUME_DIVISIONS_TOP_LEFT) + orig;
  int layerAxis = o->layerAxis(divisionsTopLeft);
  int frameAxis = o->frameAxis(divisionsTopLeft);
  if (o->isVerticalTimeline()) {
    if (m_viewer->getXsheetLayout() == QString("Classic")) {
      for (int i = 0; i <= 20; i++, frameAxis += 3)
        if ((i % 10) == 0)
          p.drawLine(o->horizontalLine(frameAxis,
                                       NumberRange(layerAxis - 3, layerAxis)));
        else if (i & 1)
          p.drawLine(
              o->horizontalLine(frameAxis, NumberRange(layerAxis, layerAxis)));
        else
          p.drawLine(o->horizontalLine(frameAxis,
                                       NumberRange(layerAxis - 2, layerAxis)));
    } else {
      for (int i = 0; i <= 20; i++, layerAxis += 3)
        if ((i % 10) == 0)
          p.drawLine(o->verticalLine(layerAxis,
                                     NumberRange(frameAxis, frameAxis + 3)));
        else if (i & 1)
          p.drawLine(
              o->verticalLine(layerAxis, NumberRange(frameAxis, frameAxis)));
        else
          p.drawLine(o->verticalLine(layerAxis,
                                     NumberRange(frameAxis, frameAxis + 2)));
    }
  } else {
    for (int i = 0; i <= 20; i++, frameAxis += 3)
      if ((i % 10) == 0)
        p.drawLine(o->horizontalLine(frameAxis,
                                     NumberRange(layerAxis, layerAxis + 3)));
      else if (i & 1)
        p.drawLine(
            o->horizontalLine(frameAxis, NumberRange(layerAxis, layerAxis)));
      else
        p.drawLine(o->horizontalLine(frameAxis,
                                     NumberRange(layerAxis, layerAxis + 2)));
  }

  // slider track
  QPainterPath track =
      o->path(PredefinedPath::VOLUME_SLIDER_TRACK).translated(orig);
  p.drawPath(track);

  // cursor
  QRect trackRect = o->rect(PredefinedRect::VOLUME_TRACK).translated(orig);
  if (o->flag(PredefinedFlag::VOLUME_AREA_VERTICAL)) volume = 1 - volume;

  layerAxis = o->layerSide(trackRect).middle();
  frameAxis = o->frameSide(trackRect).weight(volume);
  if (o->isVerticalTimeline() &&
      !o->flag(PredefinedFlag::VOLUME_AREA_VERTICAL)) {
    layerAxis = o->frameSide(trackRect).middle();
    frameAxis = o->layerSide(trackRect).weight(volume);
  }
  QPoint cursor = o->frameLayerToXY(frameAxis, layerAxis) + QPoint(1, 0);
  if (o->isVerticalTimeline() &&
      !o->flag(PredefinedFlag::VOLUME_AREA_VERTICAL)) {
    cursor = o->frameLayerToXY(layerAxis, frameAxis) + QPoint(1, 0);
  }
  QPainterPath head =
      o->path(PredefinedPath::VOLUME_SLIDER_HEAD).translated(cursor);
  p.fillPath(head, QBrush(Qt::white));
  p.setPen(m_viewer->getLightLineColor());
  p.drawPath(head);
}

//=============================================================================
// ColumnArea
//-----------------------------------------------------------------------------
ColumnArea::ColumnArea(XsheetViewer *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_viewer(parent)
    , m_pos(-1, -1)
    , m_tooltip(tr(""))
    , m_col(-1)
    , m_columnTransparencyPopup(0)
    , m_transparencyPopupTimer(0)
    , m_isPanning(false)
    , m_soundColumnPopup(0) {
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  TObjectHandle *objectHandle = TApp::instance()->getCurrentObject();
  m_changeObjectParent        = new ChangeObjectParent(m_viewer);
  m_changeObjectParent->setObjectHandle(objectHandle);
  m_changeObjectParent->setXsheetHandle(xsheetHandle);
  m_changeObjectParent->hide();

  m_changeObjectHandle = new ChangeObjectHandle(m_viewer);
  m_changeObjectHandle->setObjectHandle(objectHandle);
  m_changeObjectHandle->setXsheetHandle(xsheetHandle);
  m_changeObjectHandle->hide();

#ifdef LINETEST
  // linetest had options around a motion path
  // you could configure from the column header
  m_motionPathMenu = new MotionPathMenu(0);
#endif

  m_renameColumnField = new RenameColumnField(this, m_viewer);
  m_renameColumnField->setXsheetHandle(xsheetHandle);
  m_renameColumnField->hide();

  QActionGroup *actionGroup = new QActionGroup(this);
  m_subsampling1            = new QAction(tr("&Subsampling 1"), actionGroup);
  m_subsampling2            = new QAction(tr("&Subsampling 2"), actionGroup);
  m_subsampling3            = new QAction(tr("&Subsampling 3"), actionGroup);
  m_subsampling4            = new QAction(tr("&Subsampling 4"), actionGroup);
  actionGroup->addAction(m_subsampling1);
  actionGroup->addAction(m_subsampling2);
  actionGroup->addAction(m_subsampling3);
  actionGroup->addAction(m_subsampling4);

  connect(actionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onSubSampling(QAction *)));
  connect(xsheetHandle, SIGNAL(xsheetCameraChange(int)), this,
          SLOT(onXsheetCameraChange(int)));
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

ColumnArea::~ColumnArea() {}

//-----------------------------------------------------------------------------

DragTool *ColumnArea::getDragTool() const { return m_viewer->getDragTool(); }
void ColumnArea::setDragTool(DragTool *dragTool) {
  m_viewer->setDragTool(dragTool);
}

//-----------------------------------------------------------------------------
void ColumnArea::drawFoldedColumnHead(QPainter &p, int col) {
  const Orientation *o = m_viewer->orientation();

  QPoint orig = m_viewer->positionToXY(CellPosition(0, col));
  QRect rect  = o->rect(PredefinedRect::FOLDED_LAYER_HEADER).translated(orig);

  int x0, y0, x, y;

  if (o->isVerticalTimeline()) {
    x0 = rect.topLeft().x() + 1;
    y0 = 0;

    p.setPen(m_viewer->getDarkLineColor());
    p.fillRect(x0, y0 + 1, rect.width(), 18,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0, y0 + 17, 2, rect.height() - 34,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 3, y0 + 20, 2, rect.height() - 36,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 6, y0 + 17, 2, rect.height() - 34,
               QBrush(m_viewer->getFoldedColumnBGColor()));

    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 - 1, y0 + 17, x0 - 1, rect.height());
    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 + 2, y0 + 17, x0 + 2, rect.height());
    p.drawLine(x0 + 5, y0 + 17, x0 + 5, rect.height());
    p.drawLine(x0, y0 + 17, x0 + 1, 17);
    p.drawLine(x0 + 3, y0 + 20, x0 + 4, 20);
    p.drawLine(x0 + 6, y0 + 17, x0 + 7, 17);

    // triangolini
    p.setPen(Qt::black);
    x = x0;
    y = 12;
    p.drawPoint(QPointF(x, y));
    x++;
    p.drawLine(x, y - 1, x, y + 1);
    x++;
    p.drawLine(x, y - 2, x, y + 2);
    x += 3;
    p.drawLine(x, y - 2, x, y + 2);
    x++;
    p.drawLine(x, y - 1, x, y + 1);
    x++;
    p.drawPoint(x, y);
  } else {
    x0 = 0;
    y0 = rect.topLeft().y() + 1;

    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.fillRect(x0 + 1, y0, 18, rect.height(),
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 17, y0, rect.width() - 34, 2,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 20, y0 + 3, rect.width() - 36, 2,
               QBrush(m_viewer->getFoldedColumnBGColor()));
    p.fillRect(x0 + 17, y0 + 6, rect.width() - 34, 2,
               QBrush(m_viewer->getFoldedColumnBGColor()));

    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 + 17, y0 - 1, rect.width(), y0 - 1);
    p.setPen(m_viewer->getFoldedColumnLineColor());
    p.drawLine(x0 + 17, y0 + 2, rect.width(), y0 + 2);
    p.drawLine(x0 + 17, y0 + 5, rect.width(), y0 + 5);
    p.drawLine(x0 + 17, y0, 17, y0 + 1);
    p.drawLine(x0 + 20, y0 + 3, 20, y0 + 4);
    p.drawLine(x0 + 17, y0 + 6, 17, y0 + 7);

    // triangolini
    p.setPen(Qt::black);
    x = 12;
    y = y0;
    p.drawPoint(QPointF(x, y));
    y++;
    p.drawLine(x - 1, y, x + 1, y);
    y++;
    p.drawLine(x - 2, y, x + 2, y);
    y += 3;
    p.drawLine(x - 2, y, x + 2, y);
    y++;
    p.drawLine(x - 1, y, x + 1, y);
    y++;
    p.drawPoint(x, y);
  }
}

void ColumnArea::drawLevelColumnHead(QPainter &p, int col) {
  TXshColumn *column = col >= 0 ? m_viewer->getXsheet()->getColumn(col) : 0;
  // Draw column
  DrawHeader drawHeader(this, p, col);
  drawHeader.prepare();
  QColor columnColor, dragColor;
  drawHeader.levelColors(columnColor, dragColor);
  drawHeader.drawBaseFill(columnColor, dragColor);
  if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
    drawHeader.drawUnifiedViewToggle(column ? column->getOpacity() : 0);
  else {
    drawHeader.drawEye();
    drawHeader.drawPreviewToggle(column ? column->getOpacity() : 0);
  }
  drawHeader.drawLock();
  drawHeader.drawColumnName();
  drawHeader.drawColumnNumber();
  QPixmap iconPixmap = getColumnIcon(col);
  drawHeader.drawThumbnail(iconPixmap);
  drawHeader.drawFilterColor();
  drawHeader.drawConfig();
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
}

//-----------------------------------------------------------------------------

void ColumnArea::drawSoundColumnHead(QPainter &p, int col) {  // AREA
  TXsheet *xsh = m_viewer->getXsheet();
  TXshSoundColumn *sc =
      xsh->getColumn(col) ? xsh->getColumn(col)->getSoundColumn() : 0;

  DrawHeader drawHeader(this, p, col);
  drawHeader.prepare();
  QColor columnColor, dragColor;
  //  drawHeader.soundColors(columnColor, dragColor);
  drawHeader.levelColors(columnColor, dragColor);
  drawHeader.drawBaseFill(columnColor, dragColor);
  drawHeader.drawEye();
  drawHeader.drawPreviewToggle(sc ? (troundp(255.0 * sc->getVolume())) : 0);
  drawHeader.drawLock();
  drawHeader.drawConfig();
  drawHeader.drawColumnName();
  drawHeader.drawColumnNumber();
  // Sound columns don't have an image. Passing in an image
  // for argument, but it will be ignored.
  static QPixmap iconignored;
  drawHeader.drawThumbnail(iconignored);
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
  // drawHeader.drawFilterColor();
}

//-----------------------------------------------------------------------------

void ColumnArea::drawPaletteColumnHead(QPainter &p, int col) {  // AREA
  DrawHeader drawHeader(this, p, col);
  drawHeader.prepare();
  QColor columnColor, dragColor;
  drawHeader.paletteColors(columnColor, dragColor);
  drawHeader.drawBaseFill(columnColor, dragColor);
  drawHeader.drawEye();
  drawHeader.drawPreviewToggle(0);
  drawHeader.drawLock();
  drawHeader.drawConfig();
  drawHeader.drawColumnName();
  drawHeader.drawColumnNumber();
  static QPixmap iconPixmap(svgToPixmap(":Resources/palette_header.svg"));
  drawHeader.drawThumbnail(iconPixmap);
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
  // drawHeader.drawFilterColor();
}

//-----------------------------------------------------------------------------

void ColumnArea::drawSoundTextColumnHead(QPainter &p, int col) {  // AREA
  TColumnSelection *selection = m_viewer->getColumnSelection();
  const Orientation *o        = m_viewer->orientation();

  int x = m_viewer->columnToLayerAxis(col);

  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  p.setFont(font);

  QRect rect(x, 0, o->cellWidth(), height());

  TApp *app    = TApp::instance();
  TXsheet *xsh = m_viewer->getXsheet();

  TStageObjectId columnId = m_viewer->getObjectId(col);
  std::string name        = xsh->getStageObject(columnId)->getName();

  bool isEditingSpline = app->getCurrentObject()->isSpline();

  // Check if column is locked and selected
  TXshColumn *column = col >= 0 ? xsh->getColumn(col) : 0;
  bool isLocked      = column != 0 && column->isLocked();
  bool isCurrent     = m_viewer->getCurrentColumn() == col;
  bool isSelected =
      m_viewer->getColumnSelection()->isColumnSelected(col) && !isEditingSpline;

  DrawHeader drawHeader(this, p, col);
  drawHeader.prepare();
  QColor columnColor, dragColor;
  drawHeader.paletteColors(columnColor, dragColor);
  drawHeader.drawBaseFill(columnColor, dragColor);
  drawHeader.drawEye();
  drawHeader.drawPreviewToggle(255);
  drawHeader.drawLock();
  drawHeader.drawConfig();
  drawHeader.drawColumnName();
  drawHeader.drawColumnNumber();
  static QPixmap iconPixmap(generateIconPixmap("notelevel"));
  drawHeader.drawThumbnail(iconPixmap);
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
  // drawHeader.drawFilterColor();
}

//-----------------------------------------------------------------------------

QPixmap ColumnArea::getColumnIcon(int columnIndex) {
  const Orientation *o = m_viewer->orientation();

  if (columnIndex == -1) {  // Indice colonna = -1 -> CAMERA
    if (o->isVerticalTimeline()) {
      static QPixmap camera = svgToPixmap(":Resources/camera_small.svg");
      return camera;
    } else {
      static QPixmap camera = svgToPixmap(":Resources/camera.svg");
      return camera;
    }
  }
  TXsheet *xsh = m_viewer->getXsheet();
  if (!xsh) return QPixmap();
  if (xsh->isColumnEmpty(columnIndex)) return QPixmap();
  int r0, r1;
  xsh->getCellRange(columnIndex, r0, r1);
  if (r0 > r1) return QPixmap();
  TXshCell cell = xsh->getCell(r0, columnIndex);
  TXshLevel *xl = cell.m_level.getPointer();
  if (!xl)
    return QPixmap();
  else {
    TXshColumn *column          = xsh->getColumn(columnIndex);
    TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
    bool onDemand               = false;
    if (Preferences::instance()->getColumnIconLoadingPolicy() ==
        Preferences::LoadOnDemand) {
      onDemand = m_viewer->getCurrentColumn() != columnIndex;
      if (!onDemand) {
        TXshLevelColumn *levelColumn = column->getLevelColumn();
        TXshMeshColumn *meshColumn   = column->getMeshColumn();
        if ((levelColumn && !levelColumn->isIconVisible()) ||
            (meshColumn && !meshColumn->isIconVisible()) ||
            (zColumn && !zColumn->isIconVisible()))
          return QPixmap();
      }
    }
    QPixmap icon =
        zColumn ? FxIconPixmapManager::instance()->getFxIconPm(
                      zColumn->getZeraryColumnFx()->getZeraryFx()->getFxType())
                : IconGenerator::instance()->getIcon(xl, cell.m_frameId, false,
                                                     onDemand);
    QRect thumbnailImageRect = o->rect(PredefinedRect::THUMBNAIL);
    if (thumbnailImageRect.isEmpty()) return QPixmap();
    return scalePixmapKeepingAspectRatio(icon, thumbnailImageRect.size());
  }
}

//-----------------------------------------------------------------------------

void ColumnArea::paintEvent(QPaintEvent *event) {  // AREA
  QRect toBeUpdated = event->rect();

  QPainter p(this);
  p.setClipRect(toBeUpdated);

  TXsheet *xsh        = m_viewer->getXsheet();
  CellRange cellRange = m_viewer->xyRectToRange(toBeUpdated);
  int c0, c1;  // range of visible columns
  c0 = cellRange.from().layer();
  c1 = cellRange.to().layer();
  if (!m_viewer->orientation()->isVerticalTimeline()) {
    int colCount = std::max(1, xsh->getColumnCount());
    c1           = std::min(c1, colCount - 1);
  }

  ColumnFan *columnFan = xsh->getColumnFan(m_viewer->orientation());
  int col;
  for (col = c0; col <= c1; col++) {
    // draw column fan (collapsed columns)
    if (!columnFan->isActive(col)) {
      drawFoldedColumnHead(p, col);
    } else {
      TXshColumn *column = m_viewer->getXsheet()->getColumn(col);

      int colType = (column && !column->isEmpty()) ? column->getColumnType()
                                                   : TXshColumn::eLevelType;

      switch (colType) {
      case TXshColumn::ePaletteType:
        drawPaletteColumnHead(p, col);
        break;
      case TXshColumn::eSoundType:
        drawSoundColumnHead(p, col);
        break;
      case TXshColumn::eSoundTextType:
        drawSoundTextColumnHead(p, col);
        break;
      default:
        drawLevelColumnHead(p, col);  // camera column is also painted here
        break;
      }
    }
  }

  p.setPen(m_viewer->getVerticalLineHeadColor());
  p.setBrush(Qt::NoBrush);
  if (m_viewer->orientation()->isVerticalTimeline())
    p.drawRect(toBeUpdated.adjusted(-1, 0, -1, -3));
  else
    p.drawRect(toBeUpdated.adjusted(0, 0, -2, -1));

  if (getDragTool()) getDragTool()->drawColumnsArea(p);
}

//-----------------------------------------------------------------------------
using namespace DVGui;

ColumnTransparencyPopup::ColumnTransparencyPopup(XsheetViewer *viewer,
                                                 QWidget *parent)
    : QWidget(parent, Qt::Popup), m_viewer(viewer), m_lockBtn(nullptr) {
  setFixedWidth(8 + 78 + 8 + 100 + 8 + 8 + 8 + 7);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setMinimum(1);
  m_slider->setMaximum(100);
  m_slider->setFixedHeight(14);
  m_slider->setFixedWidth(100);

  m_value = new DVGui::IntLineEdit(this, 1, 1, 100);
  /*m_value->setValidator(new QIntValidator (1, 100, m_value));
m_value->setFixedHeight(16);
m_value->setFixedWidth(30);
static QFont font("Helvetica", 7, QFont::Normal);
m_value->setFont(font);*/

  // contents of the combo box will be updated in setColumn
  m_filterColorCombo = new QComboBox(this);

  // Lock button is moved in the popup for Minimum layout
  QPushButton *lockExtraBtn = nullptr;
  if (m_viewer->getXsheetLayout() == "Minimum") {
    m_lockBtn = new QPushButton(tr("Lock Column"), this);
    m_lockBtn->setCheckable(true);
    m_lockBtn->setIcon(createLockIcon(m_viewer));
    lockExtraBtn = new QPushButton(this);
    QMenu *menu  = new QMenu();
    menu->setObjectName("xsheetColumnAreaMenu_Lock");
    CommandManager *cmdManager = CommandManager::instance();
    menu->addAction(cmdManager->getAction("MI_LockThisColumnOnly"));
    menu->addAction(cmdManager->getAction("MI_LockSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_LockAllColumns"));
    menu->addAction(cmdManager->getAction("MI_UnlockSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_UnlockAllColumns"));
    menu->addAction(cmdManager->getAction("MI_ToggleColumnLocks"));
    lockExtraBtn->setMenu(menu);
    lockExtraBtn->setFixedSize(20, 20);
  }

  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setMargin(3);
  mainLayout->setHorizontalSpacing(6);
  mainLayout->setVerticalSpacing(6);
  {
    mainLayout->addWidget(new QLabel(tr("Opacity:"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->setMargin(0);
    hlayout->setSpacing(3);
    {
      hlayout->addWidget(m_slider);
      hlayout->addWidget(m_value);
      hlayout->addWidget(new QLabel("%"));
    }
    mainLayout->addLayout(hlayout, 0, 1);

    mainLayout->addWidget(new QLabel(tr("Filter:"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addWidget(m_filterColorCombo, 1, 1,
                          Qt::AlignLeft | Qt::AlignVCenter);

    if (m_lockBtn) {
      QHBoxLayout *lockLay = new QHBoxLayout();
      lockLay->setMargin(0);
      lockLay->setSpacing(3);
      {
        lockLay->addWidget(m_lockBtn, 0);
        lockLay->addWidget(lockExtraBtn, 0);
      }
      mainLayout->addLayout(lockLay, 2, 1, Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
  setLayout(mainLayout);

  bool ret = connect(m_slider, SIGNAL(sliderReleased()), this,
                     SLOT(onSliderReleased()));
  ret      = ret && connect(m_slider, SIGNAL(sliderMoved(int)), this,
                            SLOT(onSliderChange(int)));
  ret      = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                            SLOT(onSliderValueChanged(int)));
  ret      = ret && connect(m_value, SIGNAL(textChanged(const QString &)), this,
                            SLOT(onValueChanged(const QString &)));

  ret = ret && connect(m_filterColorCombo, SIGNAL(activated(int)), this,
                       SLOT(onFilterColorChanged()));
  if (m_lockBtn)
    ret = ret && connect(m_lockBtn, SIGNAL(clicked(bool)), this,
                         SLOT(onLockButtonClicked(bool)));

  assert(ret);
}

//----------------------------------------------------------------

void ColumnTransparencyPopup::onSliderValueChanged(int val) {
  if (m_slider->isSliderDown()) return;
  m_value->setText(QString::number(val));
  onSliderReleased();
}

void ColumnTransparencyPopup::onSliderReleased() {
  m_column->setOpacity(troundp(255.0 * m_slider->value() / 100.0));
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//-----------------------------------------------------------------------

void ColumnTransparencyPopup::onSliderChange(int val) {
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));
}

//----------------------------------------------------------------

void ColumnTransparencyPopup::onValueChanged(const QString &str) {
  int val = str.toInt();
  m_slider->setValue(val);
  m_column->setOpacity(troundp(255.0 * val / 100.0));

  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------
// TODO : UNDO
void ColumnTransparencyPopup::onFilterColorChanged() {
  int id = m_filterColorCombo->currentData().toInt();
  if (m_column->getColorFilterId() == id) return;
  m_column->setColorFilterId(id);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------

void ColumnTransparencyPopup::onLockButtonClicked(bool on) {
  assert(m_lockBtn);
  if (!m_lockBtn) return;
  m_column->lock(on);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------

void ColumnTransparencyPopup::setColumn(TXshColumn *column) {
  m_column = column;
  assert(m_column);
  int val = (int)troundp(100.0 * m_column->getOpacity() / 255.0);
  m_slider->setValue(val);
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));

  m_filterColorCombo->clear();
  // initialize color filter combo box
  QList<TSceneProperties::ColorFilter> filters = TApp::instance()
                                                     ->getCurrentScene()
                                                     ->getScene()
                                                     ->getProperties()
                                                     ->getColorFilters();

  for (int f = 0; f < filters.size(); f++) {
    TSceneProperties::ColorFilter filter = filters.at(f);
    if (f == 0)
      m_filterColorCombo->addItem(filter.name, f);
    else if (!filter.name.isEmpty())
      m_filterColorCombo->addItem(getColorChipIcon(filter.color), filter.name,
                                  f);
  }

  m_filterColorCombo->setCurrentIndex(
      m_filterColorCombo->findData(m_column->getColorFilterId()));

  if (m_lockBtn) m_lockBtn->setChecked(m_column->isLocked());
}

/*void ColumnTransparencyPopup::mouseMoveEvent ( QMouseEvent * e )
{
        int val = tcrop((e->pos().x()+10)/(this->width()/(99-1+1)), 1, 99);
        m_value->setText(QString::number(val));
        m_slider->setValue(val);
}*/

void ColumnTransparencyPopup::mouseReleaseEvent(QMouseEvent *e) {
  // hide();
}

//------------------------------------------------------------------------------

SoundColumnPopup::SoundColumnPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup) {
  setFixedWidth(8 + 78 + 8 + 100 + 8 + 8 + 8 + 7);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setMinimum(0);
  m_slider->setMaximum(100);
  m_slider->setFixedHeight(14);
  m_slider->setFixedWidth(100);

  m_value = new DVGui::IntLineEdit(this, 1, 1, 100);

  QLabel *sliderLabel = new QLabel(tr("Volume:"), this);

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(3);
  mainLayout->setSpacing(3);
  {
    QHBoxLayout *hlayout = new QHBoxLayout;
    // hlayout->setContentsMargins(0, 3, 0, 3);
    hlayout->setMargin(0);
    hlayout->setSpacing(3);
    hlayout->addWidget(sliderLabel, 0);
    hlayout->addWidget(m_slider);
    hlayout->addWidget(m_value);
    hlayout->addWidget(new QLabel("%"));
    mainLayout->addLayout(hlayout, 0);
  }
  setLayout(mainLayout);

  bool ret = connect(m_slider, SIGNAL(sliderReleased()), this,
                     SLOT(onSliderReleased()));
  ret      = ret && connect(m_slider, SIGNAL(sliderMoved(int)), this,
                            SLOT(onSliderChange(int)));
  ret      = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                            SLOT(onSliderValueChanged(int)));
  ret      = ret && connect(m_value, SIGNAL(textChanged(const QString &)), this,
                            SLOT(onValueChanged(const QString &)));
  assert(ret);
}

//----------------------------------------------------------------

void SoundColumnPopup::onSliderValueChanged(int val) {
  if (m_slider->isSliderDown()) return;
  m_value->setText(QString::number(val));
  onSliderReleased();
}

void SoundColumnPopup::onSliderReleased() {
  int val = m_slider->value();
  m_column->getSoundColumn()->setVolume(((double)val / 100.0));
  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//-----------------------------------------------------------------------

void SoundColumnPopup::onSliderChange(int val) {
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));
}

//----------------------------------------------------------------

void SoundColumnPopup::onValueChanged(const QString &str) {
  int val = str.toInt();
  m_slider->setValue(val);
  m_column->getSoundColumn()->setVolume(((double)val / 100.0));

  TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  ((ColumnArea *)parent())->update();
}

//----------------------------------------------------------------

void SoundColumnPopup::setColumn(TXshColumn *column) {
  m_column = column;
  assert(m_column);
  double volume = m_column->getSoundColumn()->getVolume();
  int val       = (int)troundp(100.0 * volume);
  m_slider->setValue(val);
  disconnect(m_value, SIGNAL(textChanged(const QString &)), 0, 0);
  m_value->setText(QString::number(val));
  connect(m_value, SIGNAL(textChanged(const QString &)), this,
          SLOT(onValueChanged(const QString &)));
}

void SoundColumnPopup::mouseReleaseEvent(QMouseEvent *e) {
  // hide();
}

//----------------------------------------------------------------

void ColumnArea::openTransparencyPopup() {
  if (m_transparencyPopupTimer) m_transparencyPopupTimer->stop();
  if (m_col < 0) return;
  TXshColumn *column = m_viewer->getXsheet()->getColumn(m_col);
  if (!column || column->isEmpty()) return;

  if (!column->isCamstandVisible()) {
    column->setCamstandVisible(true);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    update();
  }

  m_columnTransparencyPopup->setColumn(column);
  m_columnTransparencyPopup->show();
}

void ColumnArea::openSoundColumnPopup() {
  if (m_col < 0) return;
  TXshColumn *column = m_viewer->getXsheet()->getColumn(m_col);
  if (!column || column->isEmpty()) return;

  if (!column->isCamstandVisible()) {
    column->setCamstandVisible(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    update();
  }

  m_soundColumnPopup->setColumn(column);
  m_soundColumnPopup->show();
}

void ColumnArea::openCameraColumnPopup(QPoint pos) {
  QMenu menu(this);

  TXsheet *xsh           = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectTree *tree = xsh->getStageObjectTree();
  int i, objCount = tree->getStageObjectCount();
  for (i = 0; i < objCount; i++) {
    TStageObject *obj = tree->getStageObject(i);
    if (!obj || !obj->getId().isCamera()) continue;
    TStageObjectId objId = obj->getId();
    std::string objName  = obj->getName();
    QAction *action      = new QAction(QString::fromStdString(objName), this);
    action->setData(objId.getIndex());
    connect(action, SIGNAL(triggered()), this,
            SLOT(onCameraColumnChangedTriggered()));
    menu.addAction(action);
  }
  // Lock button is moved in this menu for Minimum layout
  if (m_viewer->getXsheetLayout() == "Minimum") {
    menu.addSeparator();
    bool isLocked = m_viewer->getXsheet()->getColumn(-1)->isLocked();
    QAction *lockAction =
        new QAction((isLocked) ? tr("Unlock") : tr("Lock"), this);
    lockAction->setCheckable(true);
    lockAction->setChecked(isLocked);
    lockAction->setIcon(createLockIcon(m_viewer));
    menu.addAction(lockAction);
    connect(lockAction, SIGNAL(toggled(bool)), this,
            SLOT(onCameraColumnLockToggled(bool)));
  }

  menu.exec(pos);
}

void ColumnArea::onCameraColumnChangedTriggered() {
  int newIndex = qobject_cast<QAction *>(sender())->data().toInt();
  onXsheetCameraChange(newIndex);
}

void ColumnArea::onCameraColumnLockToggled(bool lock) {
  m_viewer->getXsheet()->getColumn(-1)->lock(lock);
}

//----------------------------------------------------------------

void ColumnArea::onXsheetCameraChange(int newIndex) {
  int oldIndex = m_viewer->getXsheet()->getCameraColumnIndex();
  if (newIndex == oldIndex) return;

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();

  CameraColumnSwitchUndo *undo =
      new CameraColumnSwitchUndo(oldIndex, newIndex, xsheetHandle);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

//----------------------------------------------------------------

void ColumnArea::startTransparencyPopupTimer(QMouseEvent *e) {  // AREA
  if (!m_columnTransparencyPopup)
    m_columnTransparencyPopup = new ColumnTransparencyPopup(m_viewer, this);

  m_columnTransparencyPopup->move(e->globalPos().x(), e->globalPos().y());

  if (!m_transparencyPopupTimer) {
    m_transparencyPopupTimer = new QTimer(this);
    bool ret = connect(m_transparencyPopupTimer, SIGNAL(timeout()), this,
                       SLOT(openTransparencyPopup()));
    assert(ret);
    m_transparencyPopupTimer->setSingleShot(true);
  }

  m_transparencyPopupTimer->start(300);
}

//----------------------------------------------------------------

void ColumnArea::mousePressEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  m_doOnRelease = 0;
  m_viewer->setQtModifiers(event->modifiers());
  assert(getDragTool() == 0);

  m_col = -1;  // new in 6.4

  // both left and right click can change the selection
  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
    TXsheet *xsh   = m_viewer->getXsheet();
    ColumnFan *fan = xsh->getColumnFan(o);
    m_col          = m_viewer->xyToPosition(event->pos()).layer();
    // when clicking the column fan
    if (!fan->isActive(m_col))  // column Fan
    {
      for (auto o : Orientations::all()) {
        fan = xsh->getColumnFan(o);
        for (int i = m_col; !fan->isActive(i); i--) fan->activate(i);
      }

      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      return;
    }
    
    TXshColumn *column = xsh->getColumn(m_col);
    bool isEmpty       = !column || column->isEmpty();
    TApp::instance()->getCurrentObject()->setIsSpline(false);

    // get mouse position
    QPoint mouseInCell =
        event->pos() - m_viewer->positionToXY(CellPosition(0, m_col));
    // int x = event->pos().x() - m_viewer->columnToX(m_col);
    // int y = event->pos().y();
    // QPoint mouseInCell(x, y);
    int x = mouseInCell.x(), y = mouseInCell.y();
    
    // don't make column current when click on some of its toggle buttons
    bool needMakeColumnCurrent = true;
    if ( o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell)
      || o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell) )
      needMakeColumnCurrent = false;
    if ( Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled() ) {
      if ( o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA).contains(mouseInCell) )
        needMakeColumnCurrent = false;
    } else {
      if ( o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell)
        || o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(mouseInCell) )
        needMakeColumnCurrent = false;
    }
      
    // set the clicked column to current
    if (needMakeColumnCurrent)
      m_viewer->setCurrentColumn(m_col);

    // clicking on the camera column
    if (m_col < 0) {
      // lock button
      if (o->rect(PredefinedRect::CAMERA_LOCK_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        m_doOnRelease = isCtrlPressed ? ToggleAllLock : ToggleLock;
      }
      // config button
      else if (o->rect(PredefinedRect::CAMERA_CONFIG_AREA)
                   .contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        m_doOnRelease = OpenSettings;
      }
      // clicking another area means column selection
      else {
        if (m_viewer->getColumnSelection()->isColumnSelected(m_col) &&
            event->button() == Qt::RightButton)
          return;
        setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
      }
    }
    // clicking on the normal columns
    else if (!isEmpty) {
      // grabbing the left side of the column enables column move
      if (o->rect(PredefinedRect::DRAG_LAYER).contains(mouseInCell) ||
          (!o->flag(PredefinedFlag::DRAG_LAYER_VISIBLE)  // If dragbar hidden,
                                                         // layer name/number
                                                         // becomes dragbar
           && (o->rect(PredefinedRect::LAYER_NUMBER).contains(mouseInCell) ||
               o->rect(PredefinedRect::LAYER_NAME).contains(mouseInCell)))) {
        setDragTool(XsheetGUI::DragTool::makeColumnMoveTool(m_viewer));
      }
      // lock button
      else if (o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        m_doOnRelease = isCtrlPressed ? ToggleAllLock : ToggleLock;
      }
      // unified view button
      else if (Preferences::instance()
                   ->isUnifyColumnVisibilityTogglesEnabled() &&
               o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA)
                   .contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (column->getSoundTextColumn()) {
          // do nothing
        } else {
          // sync eye button on release
          m_doOnRelease =
              isCtrlPressed ? ToggleAllTransparency : ToggleTransparency;
          if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
              !column->getSoundColumn())
            startTransparencyPopupTimer(event);
        }
      }
      // separated view - preview button
      else if (!Preferences::instance()
                    ->isUnifyColumnVisibilityTogglesEnabled() &&
               o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (column->getSoundTextColumn()) {
          // do nothing
        } else {
          m_doOnRelease =
              isCtrlPressed ? ToggleAllPreviewVisible : TogglePreviewVisible;
          if (column->getSoundColumn())
            TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
        }
      }
      // separated view - camstand button
      else if (!Preferences::instance()
                    ->isUnifyColumnVisibilityTogglesEnabled() &&
               o->rect(PredefinedRect::PREVIEW_LAYER_AREA)
                   .contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        if (column->getPaletteColumn() || column->getSoundTextColumn()) {
          // do nothing
        } else {
          m_doOnRelease =
              isCtrlPressed ? ToggleAllTransparency : ToggleTransparency;
          if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
              !column->getSoundColumn())
            startTransparencyPopupTimer(event);
        }
      }
      // config button
      else if (o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell)) {
        if (event->button() != Qt::LeftButton) return;
        TXshZeraryFxColumn *zColumn =
            dynamic_cast<TXshZeraryFxColumn *>(column);

        if (column && (zColumn || column->getPaletteColumn() ||
                       column->getSoundTextColumn())) {
          // do nothing
        } else
          m_doOnRelease = OpenSettings;
      }
      // sound column
      else if (column && column->getSoundColumn()) {
        if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell)) {
          TXshSoundColumn *s = column->getSoundColumn();
          if (s) {
            if (s->isPlaying())
              s->stop();
            else {
              s->play();
              if (!s->isPlaying())
                s->stop();  // Serve per vista, quando le casse non sono
                            // attaccate
            }
            int interval = 0;
            if (s->isPlaying()) {
              TSoundTrackP sTrack = s->getCurrentPlaySoundTruck();
              interval            = sTrack->getDuration() * 1000 + 300;
            }
            if (s->isPlaying() && interval > 0) {
              QTimer::singleShot(interval, this, [this, s] {
                if (s && s->isPlaying()) s->stop();
                update();
              });
            }
          }
          update();
        } else if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
                   o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
          setDragTool(XsheetGUI::DragTool::makeVolumeDragTool(m_viewer));
        else
          setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
      }
      // clicking another area means column selection
      else {
        if (event->button() != Qt::LeftButton) return;
        if (xsh->getColumn(m_col)->getSoundTextColumn()) return;

        int y = Preferences::instance()->isShowXSheetToolbarEnabled() ? 30 : 0;
        if (o->rect(PredefinedRect::PEGBAR_NAME)
                .adjusted(0, 0, -20, 0)
                .contains(mouseInCell)) {
          m_changeObjectParent->refresh();
          m_changeObjectParent->show(
              QPoint(o->rect(PredefinedRect::PARENT_HANDLE_NAME).bottomLeft() +
                     QPoint(o->rect(PredefinedRect::CAMERA_CELL).width(), 0) +
                     m_viewer->positionToXY(CellPosition(0, m_col)) +
                     QPoint(-m_viewer->getColumnScrollValue(), y)));
          return;
        }
        if (o->rect(PredefinedRect::PARENT_HANDLE_NAME).contains(mouseInCell)) {
          m_changeObjectHandle->refresh();
          m_changeObjectHandle->show(
              QPoint(o->rect(PredefinedRect::PARENT_HANDLE_NAME).bottomLeft() +
                     m_viewer->positionToXY(CellPosition(0, m_col + 1)) +
                     QPoint(-m_viewer->getColumnScrollValue() + 2, y)));
          return;
        }

        setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));

        if (column) {
          // toggle columnIcon visibility with alt+click
          TXshLevelColumn *levelColumn = column->getLevelColumn();
          TXshMeshColumn *meshColumn   = column->getMeshColumn();
          TXshZeraryFxColumn *zColumn =
              dynamic_cast<TXshZeraryFxColumn *>(column);
          if (Preferences::instance()->getColumnIconLoadingPolicy() ==
                  Preferences::LoadOnDemand &&
              (event->modifiers() & Qt::AltModifier)) {
            if (levelColumn)
              levelColumn->setIconVisible(!levelColumn->isIconVisible());
            else if (meshColumn)
              meshColumn->setIconVisible(!meshColumn->isIconVisible());
            else if (zColumn)
              zColumn->setIconVisible(!zColumn->isIconVisible());
          }
        }
      }
      // update the current fx when zerary fx column is clicked
      if (column && column->getZeraryFxColumn()) {
        TFx *fx = column->getZeraryFxColumn()->getZeraryColumnFx();
        TApp::instance()->getCurrentFx()->setFx(fx);
      }
    } else {
      if (m_viewer->getColumnSelection()->isColumnSelected(m_col) &&
          event->button() == Qt::RightButton)
        return;
      setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
    }

    m_viewer->dragToolClick(event);
    update();

  } else if (event->button() == Qt::MidButton) {
    m_pos       = event->pos();
    m_isPanning = true;
  }
}

//-----------------------------------------------------------------------------

void ColumnArea::mouseMoveEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  m_viewer->setQtModifiers(event->modifiers());
  QPoint pos = event->pos();

  if (m_isPanning) {  // Pan tasto centrale
    QPoint delta = m_pos - pos;
    if (o->isVerticalTimeline())
      delta.setY(0);
    else
      delta.setX(0);
    m_viewer->scroll(delta);
    return;
  }

  int col = m_viewer->xyToPosition(pos).layer();
  if (col < -1) col = 0;
  TXsheet *xsh       = m_viewer->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  QPoint mouseInCell = pos - m_viewer->positionToXY(CellPosition(0, col));
  int x = mouseInCell.x(), y = mouseInCell.y();

#ifdef LINETEST
  // Ensure that the menu of the motion path is hidden
  if ((x - m_mtypeBox.left() > 20 || y < m_mtypeBox.y() ||
       y > m_mtypeBox.bottom()) &&
      !m_motionPathMenu->isHidden())
    m_motionPathMenu->hide();
#endif
  if ((event->buttons() & Qt::LeftButton) != 0 &&
      !visibleRegion().contains(pos)) {
    QRect bounds = visibleRegion().boundingRect();
    if (o->isVerticalTimeline())
      m_viewer->setAutoPanSpeed(bounds, QPoint(pos.x(), bounds.top()));
    else
      m_viewer->setAutoPanSpeed(bounds, QPoint(bounds.left(), pos.y()));
  } else
    m_viewer->stopAutoPan();

  m_pos = pos;

  if (event->buttons() && getDragTool()) {
    m_viewer->dragToolDrag(event);
    update();
    return;
  }

  // Setto i toolTip
  TStageObjectId columnId = m_viewer->getObjectId(col);
  TStageObjectId parentId = xsh->getStageObjectParent(columnId);

  if (col < 0)
    m_tooltip = tr("Click to select camera");
  else if (o->rect(PredefinedRect::DRAG_LAYER).contains(mouseInCell)) {
    m_tooltip = tr("Click to select column, drag to move it");
  } else if (o->rect(PredefinedRect::LAYER_NUMBER).contains(mouseInCell)) {
    if (o->isVerticalTimeline())
      m_tooltip = tr("Click to select column, drag to move it");
    else
      m_tooltip = tr("Click to select column");
  } else if (o->rect(PredefinedRect::LAYER_NAME).contains(mouseInCell)) {
    if (o->isVerticalTimeline())
      m_tooltip =
          tr("Click to select column, drag to move it, double-click to edit");
    else if (column && column->getSoundColumn()) {
      // sound column
      if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell))
        m_tooltip = tr("Click to play the soundtrack back");
      else if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
               o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
        m_tooltip = tr("Set the volume of the soundtrack");
    } else
      m_tooltip = tr("Click to select column, double-click to edit");
  } else if (o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell)) {
    m_tooltip = tr("Lock Toggle");
  } else if (o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell)) {
    m_tooltip = tr("Additional column settings");
  } else if (o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell)) {
    m_tooltip = tr("Preview Visibility Toggle");
  } else if (o->rect(PredefinedRect::PREVIEW_LAYER_AREA)
                 .contains(mouseInCell)) {
    m_tooltip = tr("Camera Stand Visibility Toggle");
  } else if (o->rect(PredefinedRect::PARENT_HANDLE_NAME)
                 .contains(mouseInCell)) {
    m_tooltip = tr("Click to select parent handle");
  } else if (o->rect(PredefinedRect::PEGBAR_NAME).contains(mouseInCell)) {
    m_tooltip = tr("Click to select parent object");
  } else {
    if (column && column->getSoundColumn()) {
      // sound column
      if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell))
        m_tooltip = tr("Click to play the soundtrack back");
      else if (!o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE) &&
               o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
        m_tooltip = tr("Set the volume of the soundtrack");
    } else if (Preferences::instance()->getColumnIconLoadingPolicy() ==
               Preferences::LoadOnDemand)
      m_tooltip = tr("Alt + Click to Toggle Thumbnail");
    else
      m_tooltip = tr("");
  }
  update();
}

//-----------------------------------------------------------------------------

bool ColumnArea::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    if (!m_tooltip.isEmpty())
      QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
    else
      QToolTip::hideText();
  }
  return QWidget::event(event);
}

//-----------------------------------------------------------------------------

void ColumnArea::mouseReleaseEvent(QMouseEvent *event) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = m_viewer->getXsheet();
  int col, totcols = xsh->getColumnCount();
  if (m_doOnRelease != 0) {
    TXshColumn *column = xsh->getColumn(m_col);
    if (m_doOnRelease == ToggleTransparency) {
      column->setCamstandVisible(!column->isCamstandVisible());
      // sync eye button
      if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
        column->setPreviewVisible(column->isCamstandVisible());

      if (column->getSoundColumn())
        app->getCurrentXsheet()->notifyXsheetSoundChanged();
    } else if (m_doOnRelease == TogglePreviewVisible)
      column->setPreviewVisible(!column->isPreviewVisible());
    else if (m_doOnRelease == ToggleLock)
      column->lock(!column->isLocked());
    else if (m_doOnRelease == OpenSettings) {
      QPoint pos = event->pos();
      int col    = m_viewer->xyToPosition(pos).layer();
      // Align popup to be below to CONFIG button
      QRect configRect = m_viewer->orientation()->rect(
          (col < 0) ? PredefinedRect::CAMERA_CONFIG_AREA
                    : PredefinedRect::CONFIG_AREA);
      CellPosition cellPosition(0, col);
      QPoint topLeft     = m_viewer->positionToXY(cellPosition);
      QPoint mouseInCell = pos - topLeft;
      int x              = configRect.left() - mouseInCell.x() +
              1;  // distance from right edge of CONFIG button
      int y =
          mouseInCell.y() -
          configRect.bottom();  // distance from bottum edge of CONFIG button

      if (col < 0) {
        openCameraColumnPopup(
            QPoint(event->globalPos().x() + x, event->globalPos().y() - y));
      } else if (column->getSoundColumn()) {
        if (!m_soundColumnPopup)
          m_soundColumnPopup = new SoundColumnPopup(this);

        m_soundColumnPopup->move(event->globalPos().x() + x,
                                 event->globalPos().y() - y);

        openSoundColumnPopup();
      } else {
        if (!m_columnTransparencyPopup)
          m_columnTransparencyPopup =
              new ColumnTransparencyPopup(m_viewer, this);

        m_columnTransparencyPopup->move(event->globalPos().x() + x,
                                        event->globalPos().y() - y);

        // make sure the popup doesn't go off the screen to the right
        QDesktopWidget *desktop = qApp->desktop();
        QRect screenRect        = desktop->screenGeometry(app->getMainWindow());

        int popupLeft  = event->globalPos().x() + x;
        int popupRight = popupLeft + m_columnTransparencyPopup->width();

        // first condition checks if popup is on same monitor as main app;
        // if popup is on different monitor, leave as is
        if (popupLeft < screenRect.right() && popupRight > screenRect.right()) {
          int distance = popupRight - screenRect.right();
          m_columnTransparencyPopup->move(
              m_columnTransparencyPopup->x() - distance,
              m_columnTransparencyPopup->y());
        }

        openTransparencyPopup();
      }
    } else if (m_doOnRelease == ToggleAllPreviewVisible) {
      for (col = 0; col < totcols; col++) {
        TXshColumn *column = xsh->getColumn(col);
        if (!xsh->isColumnEmpty(col) && !column->getPaletteColumn() &&
            !column->getSoundTextColumn()) {
          column->setPreviewVisible(!column->isPreviewVisible());
        }
      }
    } else if (m_doOnRelease == ToggleAllTransparency) {
      bool sound_changed = false;
      for (col = 0; col < totcols; col++) {
        TXshColumn *column = xsh->getColumn(col);
        if (!xsh->isColumnEmpty(col) && !column->getPaletteColumn() &&
            !column->getSoundTextColumn()) {
          column->setCamstandVisible(!column->isCamstandVisible());
          // sync eye button
          if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
            column->setPreviewVisible(column->isCamstandVisible());
          if (column->getSoundColumn()) sound_changed = true;
        }
      }

      if (sound_changed) {
        app->getCurrentXsheet()->notifyXsheetSoundChanged();
      }
    } else if (m_doOnRelease == ToggleAllLock) {
      int startCol =
          Preferences::instance()->isXsheetCameraColumnVisible() ? -1 : 0;
      for (col = startCol; col < totcols; col++) {
        TXshColumn *column = xsh->getColumn(col);
        if (!xsh->isColumnEmpty(col)) {
          column->lock(!column->isLocked());
        }
      }
    } else
      assert(false);

    app->getCurrentScene()->notifySceneChanged();
    // signal XsheetChanged will invoke PreviewFxManager to all rendered frames,
    // if necessary. it causes slowness when opening preview flipbook of large
    // scene.
    // KNOWN BUG: Side effect, transparency doesn't sync in schematic if false.
    bool isTransparencyRendered =
        app->getCurrentScene()
            ->getScene()
            ->getProperties()
            ->isColumnColorFilterOnRenderEnabled() ||
        Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled();
    bool isStateChanged = m_doOnRelease == TogglePreviewVisible ||
                          m_doOnRelease == ToggleAllPreviewVisible ||
                          m_doOnRelease == ToggleLock ||
                          m_doOnRelease == ToggleAllLock;
    if (isStateChanged ||
        (isTransparencyRendered && (m_doOnRelease == ToggleTransparency ||
                                    m_doOnRelease == ToggleAllTransparency ||
                                    m_doOnRelease == OpenSettings))) {
      app->getCurrentXsheet()->notifyXsheetChanged();
    }
    update();
    m_doOnRelease = 0;
  }

  if (m_transparencyPopupTimer) m_transparencyPopupTimer->stop();

  m_viewer->setQtModifiers(Qt::KeyboardModifiers());
  m_viewer->dragToolRelease(event);
  m_isPanning = false;
  m_viewer->stopAutoPan();
}

//-----------------------------------------------------------------------------

void ColumnArea::mouseDoubleClickEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  QPoint pos = event->pos();
  int col    = m_viewer->xyToPosition(pos).layer();
  CellPosition cellPosition(0, col);
  QPoint topLeft     = m_viewer->positionToXY(cellPosition);
  QPoint mouseInCell = pos - topLeft;

  QRect nameRect = o->rect((col < 0) ? PredefinedRect::CAMERA_LAYER_NAME
                                     : PredefinedRect::LAYER_NAME);
  if (!nameRect.contains(mouseInCell)) return;

  TXsheet *xsh = m_viewer->getXsheet();
  // enable to rename empty column when the column name is linked to the level
  if (!Preferences::instance()->isLinkColumnNameWithLevelEnabled() &&
      col >= 0 && xsh->isColumnEmpty(col))
    return;

  QPoint fieldPos =
      (col < 0 && o->isVerticalTimeline()) ? nameRect.topLeft() : topLeft;
  QRect renameRect =
      o->rect(PredefinedRect::RENAME_COLUMN).translated(fieldPos);
  m_renameColumnField->show(renameRect, col);
}

//-----------------------------------------------------------------------------

void ColumnArea::contextMenuEvent(QContextMenuEvent *event) {
#ifndef _WIN32
  /* On windows the widget receive the release event before the menu
     is shown, on linux and osx the release event is lost, never
     received by the widget */
  QMouseEvent fakeRelease(QEvent::MouseButtonRelease, event->pos(),
                          Qt::RightButton, Qt::NoButton, Qt::NoModifier);

  QApplication::instance()->sendEvent(this, &fakeRelease);
#endif
  const Orientation *o = m_viewer->orientation();

  int col = m_viewer->xyToPosition(event->pos()).layer();

  bool isCamera = col < 0;

  m_viewer->setCurrentColumn(col);
  TXsheet *xsh       = m_viewer->getXsheet();
  QPoint topLeft     = m_viewer->positionToXY(CellPosition(0, col));
  QPoint mouseInCell = event->pos() - topLeft;

  QMenu menu(this);
  CommandManager *cmdManager = CommandManager::instance();

  //---- Unified
  if (((isCamera && !o->isVerticalTimeline()) || !xsh->isColumnEmpty(col)) &&
      o->rect(PredefinedRect::UNIFIEDVIEW_LAYER_AREA).contains(mouseInCell) &&
      Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
    menu.setObjectName("xsheetColumnAreaMenu_Preview");

    menu.addAction(cmdManager->getAction("MI_EnableThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_EnableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_EnableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_SwapEnabledColumns"));
  }
  //---- Preview
  else if (((isCamera && !o->isVerticalTimeline()) ||
            !xsh->isColumnEmpty(col)) &&
           o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell) &&
           !Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
    menu.setObjectName("xsheetColumnAreaMenu_Preview");

    menu.addAction(cmdManager->getAction("MI_EnableThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_EnableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_EnableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_SwapEnabledColumns"));
  }
  //---- Camstand
  else if (((isCamera && !o->isVerticalTimeline()) ||
            !xsh->isColumnEmpty(col)) &&
           o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(mouseInCell) &&
           !Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled()) {
    menu.setObjectName("xsheetColumnAreaMenu_Camstand");

    menu.addAction(cmdManager->getAction("MI_ActivateThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_ActivateSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_ActivateAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DeactivateAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DeactivateSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_ToggleColumnsActivation"));
    // hide all columns placed on the left
    menu.addAction(cmdManager->getAction("MI_DeactivateUpperColumns"));
  }
  //---- Lock
  else if ((isCamera || !xsh->isColumnEmpty(col)) &&
           o->rect((isCamera) ? PredefinedRect::CAMERA_LOCK_AREA
                              : PredefinedRect::LOCK_AREA)
               .contains(mouseInCell)) {
    menu.setObjectName("xsheetColumnAreaMenu_Lock");

    menu.addAction(cmdManager->getAction("MI_LockThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_LockSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_LockAllColumns"));
    menu.addAction(cmdManager->getAction("MI_UnlockSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_UnlockAllColumns"));
    menu.addAction(cmdManager->getAction("MI_ToggleColumnLocks"));
  }
  // right clicking another area / right clicking empty column head
  else {
    if (!isCamera) {
      int r0, r1;
      xsh->getCellRange(col, r0, r1);
      TXshCell cell = xsh->getCell(r0, col);
      menu.addAction(cmdManager->getAction(MI_Cut));
      menu.addAction(cmdManager->getAction(MI_Copy));
      menu.addAction(cmdManager->getAction(MI_Paste));
      menu.addAction(cmdManager->getAction(MI_PasteAbove));
      menu.addAction(cmdManager->getAction(MI_Clear));
      menu.addAction(cmdManager->getAction(MI_Insert));
      menu.addAction(cmdManager->getAction(MI_InsertAbove));
      menu.addSeparator();
      menu.addAction(cmdManager->getAction(MI_InsertFx));
      menu.addAction(cmdManager->getAction(MI_NewNoteLevel));
      menu.addAction(cmdManager->getAction(MI_RemoveEmptyColumns));
      menu.addSeparator();
      if (m_viewer->getXsheet()->isColumnEmpty(col) ||
          (cell.m_level && cell.m_level->getChildLevel()))
        menu.addAction(cmdManager->getAction(MI_OpenChild));

      // Close sub xsheet and move to parent sheet
      ToonzScene *scene      = TApp::instance()->getCurrentScene()->getScene();
      ChildStack *childStack = scene->getChildStack();
      if (childStack && childStack->getAncestorCount() > 0) {
        menu.addAction(cmdManager->getAction(MI_CloseChild));
      }

      menu.addAction(cmdManager->getAction(MI_Collapse));
      if (cell.m_level && cell.m_level->getChildLevel()) {
        menu.addAction(cmdManager->getAction(MI_Resequence));
        menu.addAction(cmdManager->getAction(MI_CloneChild));
        menu.addAction(cmdManager->getAction(MI_ExplodeChild));
      }
      menu.addSeparator();
    }
    menu.addAction(cmdManager->getAction(MI_FoldColumns));
    if (Preferences::instance()->isShowKeyframesOnXsheetCellAreaEnabled()) {
      QAction *cameraToggle =
          cmdManager->getAction(MI_ToggleXsheetCameraColumn);
      bool cameraVisible =
          Preferences::instance()->isXsheetCameraColumnVisible();
      if (cameraVisible)
        cameraToggle->setText(tr("Hide Camera Column"));
      else
        cameraToggle->setText(tr("Show Camera Column"));
      menu.addAction(cameraToggle);
    }
    menu.addSeparator();
    menu.addAction(cmdManager->getAction(MI_ToggleXSheetToolbar));
    menu.addAction(cmdManager->getAction(MI_ToggleXsheetBreadcrumbs));

    if (isCamera) {
      menu.exec(event->globalPos());
      return;
    }

    // force the selected cells placed in n-steps
    if (!xsh->isColumnEmpty(col)) {
      menu.addSeparator();
      QMenu *reframeSubMenu = new QMenu(tr("Reframe"), this);
      {
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe1));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe2));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe3));
        reframeSubMenu->addAction(cmdManager->getAction(MI_Reframe4));
        reframeSubMenu->addAction(
            cmdManager->getAction(MI_ReframeWithEmptyInbetweens));
      }
      menu.addMenu(reframeSubMenu);
      menu.addAction(cmdManager->getAction(MI_AutoInputCellNumber));
    }

    if (containsRasterLevel(m_viewer->getColumnSelection())) {
      QMenu *subsampleSubMenu = new QMenu(tr("Subsampling"), this);
      {
        subsampleSubMenu->addAction(m_subsampling1);
        subsampleSubMenu->addAction(m_subsampling2);
        subsampleSubMenu->addAction(m_subsampling3);
        subsampleSubMenu->addAction(m_subsampling4);
      }
      menu.addMenu(subsampleSubMenu);
    }

    if (!xsh->isColumnEmpty(col)) {
      menu.addAction(cmdManager->getAction(MI_ReplaceLevel));
      menu.addAction(cmdManager->getAction(MI_ReplaceParentDirectory));

      // if (containsVectorLevel(col)) {
      //  menu.addSeparator();
      //  QAction *setMask =
      //      new QAction(tr("Temporary Mask (Not in final render)"), this);
      //  setMask->setCheckable(true);
      //  setMask->setChecked(xsh->getColumn(col)->isMask());
      //  setMask->setToolTip(
      //      tr("Only Toonz Vector levels can be used as masks. \n Masks don't
      //      "
      //         "show up in final renders."));
      //  bool ret = true;
      //  ret      = ret &&
      //        connect(setMask, &QAction::toggled, [=]() { onSetMask(col); });
      //  assert(ret);
      //  menu.addAction(setMask);
      //}
    }
  }

  QAction *act  = cmdManager->getAction(MI_Insert),
          *act2 = cmdManager->getAction(MI_InsertAbove),
          *act3 = cmdManager->getAction(MI_Paste),
          *act4 = cmdManager->getAction(MI_PasteAbove);

  QString actText = act->text(), act2Text = act2->text(),
          act3Text = act3->text(), act4Text = act4->text();

  if (o->isVerticalTimeline()) {
    act->setText(tr("&Insert Before"));
    act2->setText(tr("&Insert After"));
    act3->setText(tr("&Paste Insert Before"));
    act4->setText(tr("&Paste Insert After"));
  } else {
    act->setText(tr("&Insert Below"));
    act2->setText(tr("&Insert Above"));
    act3->setText(tr("&Paste Insert Below"));
    act4->setText(tr("&Paste Insert Above"));
  }

  menu.exec(event->globalPos());

  act->setText(actText);
  act2->setText(act2Text);
  act3->setText(act3Text);
  act4->setText(act4Text);
}

//-----------------------------------------------------------------------------

void ColumnArea::onSetMask(int col) {
  TXshColumn *column = m_viewer->getXsheet()->getColumn(m_col);

  std::string name = m_viewer->getXsheet()
                         ->getStageObject(TStageObjectId::ColumnId(col))
                         ->getName();
  ColumnMaskUndo *undo = new ColumnMaskUndo(col, column->isMask(), name);
  undo->redo();
  TUndoManager::manager()->add(undo);
  update();
}

//-----------------------------------------------------------------------------

void ColumnArea::onSubSampling(QAction *action) {
  int subsampling;
  if (action == m_subsampling1)
    subsampling = 1;
  else if (action == m_subsampling2)
    subsampling = 2;
  else if (action == m_subsampling3)
    subsampling = 3;
  else if (action == m_subsampling4)
    subsampling = 4;

  TColumnSelection *selection = m_viewer->getColumnSelection();
  TXsheet *xsh                = m_viewer->getXsheet();
  assert(selection && xsh);
  const std::set<int> indexes = selection->getIndices();
  for (auto const &e : indexes) {
    if (e < 0) continue;  // Ignore camera column
    TXshColumn *column          = xsh->getColumn(e);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) continue;
    const QSet<TXshLevel *> levels = getLevels(column);
    QSet<TXshLevel *>::const_iterator it2;
    for (it2 = levels.begin(); it2 != levels.end(); it2++) {
      TXshSimpleLevel *sl = (*it2)->getSimpleLevel();
      if (!sl || sl->getProperties()->getDirtyFlag()) continue;
      int type = sl->getType();
      if (type == TZI_XSHLEVEL || type == TZP_XSHLEVEL ||
          type == OVL_XSHLEVEL) {
        sl->getProperties()->setSubsampling(subsampling);
        sl->invalidateFrames();
      }
    }
  }
  TApp::instance()
      ->getCurrentXsheet()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}
}  // namespace XsheetGUI
