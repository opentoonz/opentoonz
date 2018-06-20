
#include "xshcolumnviewer.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "columnselection.h"
#include "xsheetdragtool.h"
#include "tapp.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/intfield.h"

// TnzLib includes
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

#include <QBitmap>
//=============================================================================

namespace {
const QSet<TXshSimpleLevel *> getLevels(TXshColumn *column) {
  QSet<TXshSimpleLevel *> levels;
  TXshCellColumn *cellColumn = column->getCellColumn();
  if (cellColumn) {
    int i, r0, r1;
    cellColumn->getRange(r0, r1);
    for (i = r0; i <= r1; i++) {
      TXshCell cell       = cellColumn->getCell(i);
      TXshSimpleLevel *sl = cell.getSimpleLevel();
      if (sl) levels.insert(sl);
    }
  }
  return levels;
}

bool containsRasterLevel(TColumnSelection *selection) {
  if (!selection || selection->isEmpty()) return false;
  set<int> indexes = selection->getIndices();
  TXsheet *xsh     = TApp::instance()->getCurrentXsheet()->getXsheet();
  set<int>::iterator it;
  for (it = indexes.begin(); it != indexes.end(); it++) {
    TXshColumn *col = xsh->getColumn(*it);
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
  pixmap.fill(qCol);
  return QIcon(pixmap);
}

bool isCtrlPressed = false;
}

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

#if QT_VERSION >= 0x050500
MotionPathMenu::MotionPathMenu(QWidget *parent, Qt::WindowFlags flags)
#else
MotionPathMenu::MotionPathMenu(QWidget *parent, Qt::WFlags flags)
#endif
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
    : QListWidget(parent), m_width(40) {
  setMouseTracking(true);
  setObjectName("XshColumnChangeObjectWidget");
  setAutoFillBackground(true);
}

//-----------------------------------------------------------------------------

ChangeObjectWidget::~ChangeObjectWidget() {}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::show(const QPoint &pos) {
  refresh();
  int itemNumber = count();
  if (itemNumber > 10) {
    itemNumber = 10;
    m_width += 15;
  }
  setGeometry(pos.x(), pos.y(), m_width, itemNumber * 16 + 2);
  QListWidget::show();
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

void ChangeObjectWidget::focusOutEvent(QFocusEvent *e) {
  if (!isVisible()) return;
  hide();
  parentWidget()->update();
}

//-----------------------------------------------------------------------------

void ChangeObjectWidget::selectCurrent(const QString &text) {
  QList<QListWidgetItem *> itemList = findItems(text, Qt::MatchExactly);
  clearSelection();
  if (itemList.size() < 1) return;
  QListWidgetItem *currentWidgetItem = itemList.at(0);
  setCurrentItem(currentWidgetItem);
}

//=============================================================================
// ChangeObjectParent
//-----------------------------------------------------------------------------

ChangeObjectParent::ChangeObjectParent(QWidget *parent)
    : ChangeObjectWidget(parent) {
  bool ret = connect(this, SIGNAL(currentTextChanged(const QString &)), this,
                     SLOT(onTextChanged(const QString &)));
  assert(ret);
}

//-----------------------------------------------------------------------------

ChangeObjectParent::~ChangeObjectParent() {}

//-----------------------------------------------------------------------------

void ChangeObjectParent::refresh() {
  clear();
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  TXsheet *xsh                   = m_xsheetHandle->getXsheet();
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObjectId parentId = xsh->getStageObject(currentObjectId)->getParent();
  TStageObjectTree *tree  = xsh->getStageObjectTree();
  int objectCount         = tree->getStageObjectCount();
  QString text;
  QList<QString> pegbarList;
  QList<QString> columnList;
  QString theLongestTxt;
  int i;
  for (i = 0; i < objectCount; i++) {
    TStageObjectId id = tree->getStageObject(i)->getId();
    int index         = id.getIndex();
    QString indexStr(std::to_string(id.getIndex() + 1).c_str());
    QString newText;
    if (id == parentId) {
      if (parentId.isPegbar())
        text = QString("Peg ") + indexStr;
      else if (parentId.isColumn())
        text = QString("Col ") + indexStr;
    }
    if (id == currentObjectId) continue;
    if (id.isPegbar()) {
      newText = QString("Peg ") + indexStr;
      pegbarList.append(newText);
    }
    if (id.isColumn() && (!xsh->isColumnEmpty(index) || index < 2)) {
      newText = QString("Col ") + indexStr;
      columnList.append(newText);
    }
    if (newText.length() > theLongestTxt.length()) theLongestTxt = newText;
  }
  for (i = 0; i < columnList.size(); i++) addItem(columnList.at(i));
  for (i = 0; i < pegbarList.size(); i++) addItem(pegbarList.at(i));

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

  m_width = QFontMetrics(font).width(theLongestTxt) + 2;
  selectCurrent(text);
}

//-----------------------------------------------------------------------------

void ChangeObjectParent::onTextChanged(const QString &text) {
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  if (text.isEmpty()) {
    hide();
    return;
  }
  bool isPegbar                        = false;
  if (text.startsWith("Peg")) isPegbar = true;
  QString number                       = text;
  number.remove(0, 4);
  int index = number.toInt() - 1;
  if (index < 0) {
    hide();
    return;
  }
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObjectId newStageObjectId;
  if (isPegbar)
    newStageObjectId = TStageObjectId::PegbarId(index);
  else
    newStageObjectId = TStageObjectId::ColumnId(index);

  if (newStageObjectId == currentObjectId) return;

  TStageObject *stageObject =
      m_xsheetHandle->getXsheet()->getStageObject(currentObjectId);
  TStageObjectCmd::setParent(currentObjectId, newStageObjectId, "B",
                             m_xsheetHandle);

  hide();
  m_objectHandle->notifyObjectIdChanged(false);
  m_xsheetHandle->notifyXsheetChanged();
}

//=============================================================================
// ChangeObjectHandle
//-----------------------------------------------------------------------------

ChangeObjectHandle::ChangeObjectHandle(QWidget *parent)
    : ChangeObjectWidget(parent) {
  bool ret = connect(this, SIGNAL(currentTextChanged(const QString &)), this,
                     SLOT(onTextChanged(const QString &)));
  assert(ret);
}

//-----------------------------------------------------------------------------

ChangeObjectHandle::~ChangeObjectHandle() {}

//-----------------------------------------------------------------------------

void ChangeObjectHandle::refresh() {
  clear();
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  assert(xsh);
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  TStageObject *stageObject      = xsh->getStageObject(currentObjectId);
  m_width                        = 28;

  int i;
  QString str;
  if (stageObject->getParent().isColumn()) {
    for (i = 0; i < 20; i++) addItem(str.number(20 - i));
  }
  for (i = 0; i < 26; i++) addItem(QString(char('A' + i)));

  std::string handle = stageObject->getParentHandle();
  if (handle[0] == 'H' && handle.length() > 1) handle = handle.substr(1);

  selectCurrent(QString::fromStdString(handle));
}

//-----------------------------------------------------------------------------

void ChangeObjectHandle::onTextChanged(const QString &text) {
  assert(m_xsheetHandle);
  assert(m_objectHandle);
  TStageObjectId currentObjectId = m_objectHandle->getObjectId();
  QString handle                 = text;
  if (text.toInt() != 0) handle  = QString("H") + handle;
  if (handle.isEmpty()) return;
  std::vector<TStageObjectId> ids;
  ids.push_back(currentObjectId);
  TStageObjectCmd::setParentHandle(ids, handle.toStdString(), m_xsheetHandle);
  hide();
  m_objectHandle->notifyObjectIdChanged(false);
  m_xsheetHandle->notifyXsheetChanged();
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

  TXsheet *xsh = m_xsheetHandle->getXsheet();
  std::string name =
      xsh->getStageObject(TStageObjectId::ColumnId(col))->getName();
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
  TStageObjectId columnId = TStageObjectId::ColumnId(m_col);
  TXshColumn *column =
      m_xsheetHandle->getXsheet()->getColumn(columnId.getIndex());
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
    : area(nArea), p(nP), col(nCol) {
  m_viewer = area->m_viewer;
  o        = m_viewer->orientation();
  app      = TApp::instance();
  xsh      = m_viewer->getXsheet();
  column   = col >= 0 ? xsh->getColumn(col) : 0;
  isEmpty  = col >= 0 && xsh->isColumnEmpty(col);

  TStageObjectId currentColumnId = app->getCurrentObject()->getObjectId();

  // check if the column is current
  isCurrent = false;
  if (currentColumnId == TStageObjectId::CameraId(0))  // CAMERA
    isCurrent = col == -1;
  else
    isCurrent = m_viewer->getCurrentColumn() == col;

  orig = m_viewer->positionToXY(CellPosition(0, max(col, 0)));
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
  static QPixmap sound = svgToPixmap(":Resources/sound_header_off.svg");
  return sound;
}
const QPixmap &ColumnArea::Pixmaps::soundPlaying() {
  static QPixmap soundPlaying = svgToPixmap(":Resources/sound_header_on.svg");
  return soundPlaying;
}

//-----------------------------------------------------------------------------

void ColumnArea::DrawHeader::levelColors(QColor &columnColor,
                                         QColor &dragColor) const {
  enum { Normal, Reference, Control } usage = Reference;
  if (column) {
    if (column->isControl()) usage                             = Control;
    if (column->isRendered() || column->getMeshColumn()) usage = Normal;
  }

  if (usage == Reference) {
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
    if (column->isControl()) usage  = Control;
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

  QRect rect = o->rect(PredefinedRect::LAYER_HEADER).translated(orig);

  int x0 = rect.left();
  int x1 = rect.right();
  int y0 = rect.top();
  int y1 = rect.bottom();

  // fill base color
  if (isEmpty || col < 0)
    p.fillRect(rect, m_viewer->getEmptyColumnHeadColor());
  else {
    p.fillRect(rect, columnColor);

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

  p.setPen(m_viewer->getVerticalLineColor());
  QLine vertical =
      o->verticalLine(m_viewer->columnToLayerAxis(col), o->frameSide(rect));
  if (isEmpty || col < 0 || o->isVerticalTimeline()) p.drawLine(vertical);

  // highlight selection
  bool isSelected =
      m_viewer->getColumnSelection()->isColumnSelected(col) && !isEditingSpline;
  bool isCameraSelected = col == -1 && isCurrent && !isEditingSpline;

  QColor pastelizer(m_viewer->getColumnHeadPastelizer());
  pastelizer.setAlpha(50);

  QColor colorSelection(m_viewer->getSelectedColumnHead());
  colorSelection.setAlpha(170);
  p.fillRect(rect,
             (isSelected || isCameraSelected) ? colorSelection : pastelizer);
}

void ColumnArea::DrawHeader::drawEye() const {
  if (col < 0 || isEmpty || !o->flag(PredefinedFlag::EYE_AREA_VISIBLE)) return;
  QColor bgColor;
  QImage icon;
  int buttonType = !column->isPreviewVisible() ? PREVIEW_OFF_XSHBUTTON
                                               : PREVIEW_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, icon, !o->isVerticalTimeline());

  QRect prevViewRect = o->rect(PredefinedRect::EYE_AREA).translated(orig);
  QRect eyeRect      = o->rect(PredefinedRect::EYE).translated(orig);
  // preview visible toggle
  p.setPen(m_viewer->getVerticalLineColor());

  if (column->getSoundTextColumn()) {
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

  p.drawImage(eyeRect, icon);
}

void ColumnArea::DrawHeader::drawPreviewToggle(int opacity) const {
  if (col < 0 || isEmpty ||
      !o->flag(PredefinedFlag::PREVIEW_LAYER_AREA_VISIBLE))
    return;
  // camstand visible toggle
  QColor bgColor;
  QImage icon;
  int buttonType =
      !column->isCamstandVisible()
          ? CAMSTAND_OFF_XSHBUTTON
          : opacity < 255 ? CAMSTAND_TRANSP_XSHBUTTON : CAMSTAND_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, icon, !o->isVerticalTimeline());

  QRect tableViewRect =
      o->rect(PredefinedRect::PREVIEW_LAYER_AREA).translated(orig);
  QRect tableViewImgRect =
      o->rect(PredefinedRect::PREVIEW_LAYER).translated(orig);

  p.setPen(m_viewer->getVerticalLineColor());

  if (column->getPaletteColumn() || column->getSoundTextColumn()) {
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
  p.drawImage(tableViewImgRect, icon);
}

void ColumnArea::DrawHeader::drawLock() const {
  if (col < 0 || isEmpty || !o->flag(PredefinedFlag::LOCK_AREA_VISIBLE)) return;
  QColor bgColor;
  QImage icon;
  int buttonType = !column->isLocked() ? LOCK_OFF_XSHBUTTON : LOCK_ON_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, icon, !o->isVerticalTimeline());

  QRect lockModeRect    = o->rect(PredefinedRect::LOCK_AREA).translated(orig);
  QRect lockModeImgRect = o->rect(PredefinedRect::LOCK).translated(orig);

  if (o->isVerticalTimeline() &&
      m_viewer->getXsheetLayout() == QString("Classic") &&
      buttonType == LOCK_OFF_XSHBUTTON && !bgColor.alpha())
    bgColor = QColor(255, 255, 255, 128);

  // lock button
  p.setPen(m_viewer->getVerticalLineColor());
  p.fillRect(lockModeRect, bgColor);
  if (o->flag(PredefinedFlag::LOCK_AREA_BORDER)) p.drawRect(lockModeRect);

  // For Legacy (layout=1), Lock Off button is not displayed in Xsheet mode
  if (o->isVerticalTimeline() &&
      m_viewer->getXsheetLayout() == QString("Classic") &&
      buttonType == LOCK_OFF_XSHBUTTON)
    return;
  p.drawImage(lockModeImgRect, icon);
}

void ColumnArea::DrawHeader::drawConfig() const {
  if (col < 0 || isEmpty || !o->flag(PredefinedFlag::CONFIG_AREA_VISIBLE))
    return;
  QColor bgColor;
  QImage icon;
  int buttonType = CONFIG_XSHBUTTON;
  m_viewer->getButton(buttonType, bgColor, icon, !o->isVerticalTimeline());

  QRect configRect    = o->rect(PredefinedRect::CONFIG_AREA).translated(orig);
  QRect configImgRect = o->rect(PredefinedRect::CONFIG).translated(orig);

  // config button
  p.setPen(m_viewer->getVerticalLineColor());
  p.fillRect(configRect, bgColor);
  if (o->flag(PredefinedFlag::CONFIG_AREA_BORDER)) p.drawRect(configRect);

  if (column->getSoundColumn() || column->getPaletteColumn() ||
      column->getSoundTextColumn())
    return;

  p.drawImage(configImgRect, icon);
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
  if (col < 0) name = std::string("Camera");

  // ZeraryFx columns store name elsewhere
  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn)
    name = ::to_string(zColumn->getZeraryColumnFx()->getZeraryFx()->getName());

  QRect columnName = o->rect(PredefinedRect::LAYER_NAME).translated(orig);

  bool nameBacklit = false;
  int rightadj     = -2;
  int leftadj      = 3;
  int valign = o->isVerticalTimeline() ? Qt::AlignVCenter : Qt::AlignBottom;

  if (!isEmpty) {
    if (o->isVerticalTimeline() &&
        m_viewer->getXsheetLayout() !=
            QString("Classic"))  // Legacy - No background
    {
      if (columnName.contains(area->m_pos)) {
        p.fillRect(columnName,
                   m_viewer->getXsheetDragBarHighlightColor());  // Qt::yellow);
        nameBacklit = true;
      } else
        p.fillRect(columnName, m_viewer->getXsheetColumnNameBgColor());
    }

    if (o->flag(PredefinedFlag::LAYER_NAME_BORDER)) p.drawRect(columnName);

    if (o->isVerticalTimeline() &&
        m_viewer->getXsheetLayout() == QString("Classic")) {
      rightadj = -20;

      if (column->isPreviewVisible() && !column->getSoundTextColumn() &&
          !column->getPaletteColumn())
        nameBacklit = true;
    } else if (Preferences::instance()->isShowColumnNumbersEnabled()) {
      if (o->isVerticalTimeline())
        rightadj = -20;
      else
        leftadj = 24;
    }

    if (!o->isVerticalTimeline()) {
      if (column->getSoundColumn())
        rightadj -= 97;
      else if (column->getFilterColorId())
        rightadj -= 15;
    }

    p.setPen((isCurrent) ? m_viewer->getSelectedColumnTextColor()
                         : nameBacklit ? Qt::black : m_viewer->getTextColor());
  } else
    p.setPen((isCurrent) ? m_viewer->getSelectedColumnTextColor()
                         : m_viewer->getTextColor());

  p.drawText(columnName.adjusted(leftadj, 0, rightadj, 0),
             Qt::AlignLeft | valign | Qt::TextSingleLine,
             QString(name.c_str()));
}

void ColumnArea::DrawHeader::drawThumbnail(QPixmap &iconPixmap) const {
  if (col < 0 || isEmpty) return;

  QRect thumbnailRect =
      o->rect(PredefinedRect::THUMBNAIL_AREA).translated(orig);
  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::THUMBNAIL_AREA_BORDER)) p.drawRect(thumbnailRect);

  // sound thumbnail
  if (column->getSoundColumn()) {
    TXshSoundColumn *sc =
        xsh->getColumn(col) ? xsh->getColumn(col)->getSoundColumn() : 0;

    drawSoundIcon(sc->isPlaying());
    drawVolumeControl(sc->getVolume());
    return;
  }

  if (!o->isVerticalTimeline() ||
      !o->flag(PredefinedFlag::THUMBNAIL_AREA_VISIBLE))
    return;

  QRect thumbnailImageRect =
      o->rect(PredefinedRect::THUMBNAIL).translated(orig);

  // pallete thumbnail
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

  // for zerary fx, display fxId here instead of thumbnail
  TXshZeraryFxColumn *zColumn = dynamic_cast<TXshZeraryFxColumn *>(column);
  if (zColumn) {
    QFont lastfont = p.font();
    QFont font("Verdana", 8);
    p.setFont(font);

    TFx *fx        = zColumn->getZeraryColumnFx()->getZeraryFx();
    QString fxName = QString::fromStdWString(fx->getFxId());
    p.drawText(thumbnailImageRect, Qt::TextWrapAnywhere | Qt::TextWordWrap,
               fxName);
    p.setFont(lastfont);
  } else {
    TXshLevelColumn *levelColumn = column->getLevelColumn();
    TXshMeshColumn *meshColumn   = column->getMeshColumn();

    if (Preferences::instance()->getColumnIconLoadingPolicy() ==
            Preferences::LoadOnDemand &&
        ((levelColumn && !levelColumn->isIconVisible()) ||
         (meshColumn && !meshColumn->isIconVisible()))) {
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
    }
  }
}

void ColumnArea::DrawHeader::drawPegbarName() const {
  if (col < 0 || isEmpty || !o->flag(PredefinedFlag::PEGBAR_NAME_VISIBLE))
    return;

  TStageObjectId columnId = m_viewer->getObjectId(col);
  TStageObjectId parentId = xsh->getStageObjectParent(columnId);

  // pegbar name
  QRect pegbarnamerect = o->rect(PredefinedRect::PEGBAR_NAME).translated(orig);
  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::PEGBAR_NAME_BORDER)) p.drawRect(pegbarnamerect);

  if (column->getSoundColumn() || column->getSoundTextColumn() ||
      column->getPaletteColumn())
    return;

  p.setPen(m_viewer->getTextColor());

  p.drawText(pegbarnamerect.adjusted(3, 0, 0, 0),
             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
             QString(parentId.toString().c_str()));
}

void ColumnArea::DrawHeader::drawParentHandleName() const {
  if (col < 0 || isEmpty ||
      !o->flag(PredefinedFlag::PARENT_HANDLE_NAME_VISIBILE) ||
      column->getSoundColumn() || column->getSoundTextColumn() ||
      column->getPaletteColumn())
    return;

  QRect parenthandleRect =
      o->rect(PredefinedRect::PARENT_HANDLE_NAME).translated(orig);
  p.setPen(m_viewer->getVerticalLineColor());
  if (o->flag(PredefinedFlag::PARENT_HANDLE_NAME_BORDER))
    p.drawRect(parenthandleRect);

  TStageObjectId columnId = m_viewer->getObjectId(col);
  TStageObjectId parentId = xsh->getStageObjectParent(columnId);

  p.setPen(m_viewer->getTextColor());

  std::string handle = xsh->getStageObject(columnId)->getParentHandle();
  if (handle[0] == 'H' && handle.length() > 1) handle = handle.substr(1);
  if (parentId != TStageObjectId::TableId)
    p.drawText(parenthandleRect,
               Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine,
               QString::fromStdString(handle));
}

void ColumnArea::DrawHeader::drawFilterColor() const {
  if (col < 0 || isEmpty || !column->getFilterColorId() ||
      column->getSoundColumn() || column->getSoundTextColumn() ||
      column->getPaletteColumn())
    return;

  QRect filterColorRect =
      o->rect(PredefinedRect::FILTER_COLOR).translated(orig);
  p.drawPixmap(filterColorRect,
               getColorChipIcon(column->getFilterColor()).pixmap(12, 12));
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
#if QT_VERSION >= 0x050500
ColumnArea::ColumnArea(XsheetViewer *parent, Qt::WindowFlags flags)
#else
ColumnArea::ColumnArea(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QWidget(parent, flags)
    , m_viewer(parent)
    , m_pos(-1, -1)
    , m_tooltip(tr(""))
    , m_col(-1)
    , m_columnTransparencyPopup(0)
    , m_transparencyPopupTimer(0)
    , m_isPanning(false) {
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
#ifndef LINETEST
  TObjectHandle *objectHandle = TApp::instance()->getCurrentObject();
  m_changeObjectParent        = new ChangeObjectParent(0);
  m_changeObjectParent->setObjectHandle(objectHandle);
  m_changeObjectParent->setXsheetHandle(xsheetHandle);
  m_changeObjectParent->hide();

  m_changeObjectHandle = new ChangeObjectHandle(0);
  m_changeObjectHandle->setObjectHandle(objectHandle);
  m_changeObjectHandle->setXsheetHandle(xsheetHandle);
  m_changeObjectHandle->hide();
#else
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
               QBrush(m_viewer->getDarkBGColor()));
    p.fillRect(x0, y0 + 17, 2, rect.height() - 34,
               QBrush(m_viewer->getLightLightBGColor()));
    p.fillRect(x0 + 3, y0 + 20, 2, rect.height() - 36,
               QBrush(m_viewer->getLightLightBGColor()));
    p.fillRect(x0 + 6, y0 + 17, 2, rect.height() - 34,
               QBrush(m_viewer->getLightLightBGColor()));

    p.setPen(m_viewer->getVerticalLineColor());
    p.drawLine(x0 - 1, y0 + 17, x0 - 1, rect.height());
    p.setPen(m_viewer->getDarkLineColor());
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

    p.setPen(m_viewer->getDarkLineColor());
    p.fillRect(x0 + 1, y0, 18, rect.height(),
               QBrush(m_viewer->getDarkBGColor()));
    p.fillRect(x0 + 17, y0, rect.width() - 34, 2,
               QBrush(m_viewer->getLightLightBGColor()));
    p.fillRect(x0 + 20, y0 + 3, rect.width() - 36, 2,
               QBrush(m_viewer->getLightLightBGColor()));
    p.fillRect(x0 + 17, y0 + 6, rect.width() - 34, 2,
               QBrush(m_viewer->getLightLightBGColor()));

    p.setPen(m_viewer->getVerticalLineColor());
    p.drawLine(x0 + 17, y0 - 1, rect.width(), y0 - 1);
    p.setPen(m_viewer->getDarkLineColor());
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
  TColumnSelection *selection = m_viewer->getColumnSelection();
  const Orientation *o        = m_viewer->orientation();

  // Preparing painter
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName       = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);

  p.setFont(font);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Retrieve reference coordinates
  int currentColumnIndex = m_viewer->getCurrentColumn();
  int layerAxis          = m_viewer->columnToLayerAxis(col);

  QPoint orig = m_viewer->positionToXY(CellPosition(0, col));
  QRect rect  = o->rect(PredefinedRect::LAYER_HEADER).translated(orig);

  TApp *app    = TApp::instance();
  TXsheet *xsh = m_viewer->getXsheet();

  TStageObjectId columnId        = m_viewer->getObjectId(col);
  TStageObjectId currentColumnId = app->getCurrentObject()->getObjectId();
  TStageObjectId parentId        = xsh->getStageObjectParent(columnId);

  // Retrieve column properties
  // Check if the column is empty
  bool isEmpty       = col >= 0 && xsh->isColumnEmpty(col);
  TXshColumn *column = col >= 0 ? xsh->getColumn(col) : 0;

  bool isEditingSpline = app->getCurrentObject()->isSpline();

  // check if the column is current
  bool isCurrent = false;
  if (currentColumnId == TStageObjectId::CameraId(0))  // CAMERA
    isCurrent = col == -1;
  else
    isCurrent = m_viewer->getCurrentColumn() == col;

  bool isSelected =
      m_viewer->getColumnSelection()->isColumnSelected(col) && !isEditingSpline;
  bool isCameraSelected = col == -1 && isCurrent && !isEditingSpline;

  // Draw column
  DrawHeader drawHeader(this, p, col);
  drawHeader.prepare();
  QColor columnColor, dragColor;
  drawHeader.levelColors(columnColor, dragColor);
  drawHeader.drawBaseFill(columnColor, dragColor);
  drawHeader.drawEye();
  drawHeader.drawPreviewToggle(column ? column->getOpacity() : 0);
  drawHeader.drawLock();
  drawHeader.drawConfig();
  drawHeader.drawColumnName();
  drawHeader.drawColumnNumber();
  QPixmap iconPixmap = getColumnIcon(col);
  drawHeader.drawThumbnail(iconPixmap);
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
  drawHeader.drawFilterColor();
}

//-----------------------------------------------------------------------------

void ColumnArea::drawSoundColumnHead(QPainter &p, int col) {  // AREA
  TColumnSelection *selection = m_viewer->getColumnSelection();
  const Orientation *o        = m_viewer->orientation();

  int x = m_viewer->columnToLayerAxis(col);

  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName       = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);
  p.setFont(font);

  TXsheet *xsh = m_viewer->getXsheet();
  TXshSoundColumn *sc =
      xsh->getColumn(col) ? xsh->getColumn(col)->getSoundColumn() : 0;

  QPoint orig = m_viewer->positionToXY(CellPosition(0, col));
  QRect rect  = m_viewer->orientation()
                   ->rect(PredefinedRect::LAYER_HEADER)
                   .translated(orig);

  QPoint columnNamePos = orig + QPoint(12, o->cellHeight());

  bool isCurrent = m_viewer->getCurrentColumn() == col;

  DrawHeader drawHeader(this, p, col);
  drawHeader.prepare();
  QColor columnColor, dragColor;
  //  drawHeader.soundColors(columnColor, dragColor);
  drawHeader.levelColors(columnColor, dragColor);
  drawHeader.drawBaseFill(columnColor, dragColor);
  drawHeader.drawEye();
  drawHeader.drawPreviewToggle(255);
  drawHeader.drawLock();
  drawHeader.drawConfig();
  drawHeader.drawColumnName();
  drawHeader.drawColumnNumber();
  // Sound columns don't have an image. Passing in an image
  // for arguement, but it will be ignored.
  static QPixmap iconignored;
  drawHeader.drawThumbnail(iconignored);
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
  drawHeader.drawFilterColor();
}

//-----------------------------------------------------------------------------

void ColumnArea::drawPaletteColumnHead(QPainter &p, int col) {  // AREA
  TColumnSelection *selection = m_viewer->getColumnSelection();
  const Orientation *o        = m_viewer->orientation();

  QPoint orig = m_viewer->positionToXY(CellPosition(0, max(col, 0)));

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName       = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  font.setPixelSize(XSHEET_FONT_PX_SIZE);

  p.setFont(font);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  int currentColumnIndex = m_viewer->getCurrentColumn();
  int x                  = m_viewer->columnToLayerAxis(col);

  QRect rect(x, 0, o->cellWidth(), height());

  TXsheet *xsh = m_viewer->getXsheet();

  bool isEmpty = false;
  if (col >= 0)  // Verifico se la colonna e' vuota
    isEmpty = xsh->isColumnEmpty(col);

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
  drawHeader.drawFilterColor();
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
    fontName       = "Helvetica";
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
  static QPixmap iconPixmap(svgToPixmap(":Resources/magpie.svg"));
  drawHeader.drawThumbnail(iconPixmap);
  drawHeader.drawPegbarName();
  drawHeader.drawParentHandleName();
  drawHeader.drawFilterColor();
}

//-----------------------------------------------------------------------------

QPixmap ColumnArea::getColumnIcon(int columnIndex) {
  const Orientation *o = m_viewer->orientation();

  if (columnIndex == -1) {  // Indice colonna = -1 -> CAMERA
    TApp *app             = TApp::instance();
    static QPixmap camera = QPixmap(":Resources/camera.png");
    return camera;
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
    bool onDemand = false;
    if (Preferences::instance()->getColumnIconLoadingPolicy() ==
        Preferences::LoadOnDemand) {
      onDemand = m_viewer->getCurrentColumn() != columnIndex;
      if (!onDemand) {
        TXshColumn *column           = xsh->getColumn(columnIndex);
        TXshLevelColumn *levelColumn = column->getLevelColumn();
        TXshMeshColumn *meshColumn   = column->getMeshColumn();
        if ((levelColumn && !levelColumn->isIconVisible()) ||
            (meshColumn && !meshColumn->isIconVisible()))
          return QPixmap();
      }
    }
    QPixmap icon =
        IconGenerator::instance()->getIcon(xl, cell.m_frameId, false, onDemand);
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
    int colCount = qMax(1, xsh->getColumnCount());
    c1           = qMin(c1, colCount - 1);
  }

  ColumnFan *columnFan = xsh->getColumnFan(m_viewer->orientation());
  int col;
  for (col = c0; col <= c1; col++) {
    // draw column fan (collapsed columns)
    if (!columnFan->isActive(col)) {
      drawFoldedColumnHead(p, col);
    } else if (col >= 0) {
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
        drawLevelColumnHead(p, col);
        break;
      }
    }
  }

  p.setPen(grey150);
  p.setBrush(Qt::NoBrush);
  if (m_viewer->orientation()->isVerticalTimeline())
    p.drawRect(toBeUpdated.adjusted(0, 0, -1, -3));
  else
    p.drawRect(toBeUpdated.adjusted(0, 0, -3, -1));

  if (getDragTool()) getDragTool()->drawColumnsArea(p);
}

//-----------------------------------------------------------------------------
using namespace DVGui;

ColumnTransparencyPopup::ColumnTransparencyPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup) {
  setFixedWidth(8 + 30 + 8 + 100 + 8 + 8 + 8 + 7);

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

  m_filterColorCombo = new QComboBox(this);
  for (int f = 0; f < (int)TXshColumn::FilterAmount; f++) {
    QPair<QString, TPixel32> info =
        TXshColumn::getFilterInfo((TXshColumn::FilterColor)f);
    if ((TXshColumn::FilterColor)f == TXshColumn::FilterNone)
      m_filterColorCombo->addItem(info.first, f);
    else
      m_filterColorCombo->addItem(getColorChipIcon(info.second), info.first, f);
  }

  QLabel *filterLabel = new QLabel(tr("Filter:"), this);

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(3);
  mainLayout->setSpacing(3);
  {
    QHBoxLayout *hlayout = new QHBoxLayout;
    // hlayout->setContentsMargins(0, 3, 0, 3);
    hlayout->setMargin(0);
    hlayout->setSpacing(3);
    hlayout->addWidget(m_slider);
    hlayout->addWidget(m_value);
    hlayout->addWidget(new QLabel("%"));
    mainLayout->addLayout(hlayout, 0);

    QHBoxLayout *filterColorLay = new QHBoxLayout();
    filterColorLay->setMargin(0);
    filterColorLay->setSpacing(2);
    {
      filterColorLay->addWidget(filterLabel, 0);
      filterColorLay->addWidget(m_filterColorCombo, 1);
    }
    mainLayout->addLayout(filterColorLay, 0);
  }
  setLayout(mainLayout);

  bool ret = connect(m_slider, SIGNAL(sliderReleased()), this,
                     SLOT(onSliderReleased()));
  ret = ret && connect(m_slider, SIGNAL(sliderMoved(int)), this,
                       SLOT(onSliderChange(int)));
  ret = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                       SLOT(onSliderValueChanged(int)));
  ret = ret && connect(m_value, SIGNAL(textChanged(const QString &)), this,
                       SLOT(onValueChanged(const QString &)));

  ret = ret && connect(m_filterColorCombo, SIGNAL(activated(int)), this,
                       SLOT(onFilterColorChanged(int)));
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

void ColumnTransparencyPopup::onFilterColorChanged(int id) {
  m_column->setFilterColorId((TXshColumn::FilterColor)id);
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

  m_filterColorCombo->setCurrentIndex(m_column->getFilterColorId());
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

//----------------------------------------------------------------

void ColumnArea::startTransparencyPopupTimer(QMouseEvent *e) {  // AREA
  if (!m_columnTransparencyPopup)
    m_columnTransparencyPopup = new ColumnTransparencyPopup(
        this);  // Qt::ToolTip|Qt::MSWindowsFixedSizeDialogHint);//Qt::MSWindowsFixedSizeDialogHint|Qt::Tool);

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
    // do nothing for the camera column
    if (m_col < 0)  // CAMERA
    {
      TApp::instance()->getCurrentSelection()->getSelection()->makeNotCurrent();
      m_viewer->getColumnSelection()->selectNone();
    }
    // when clicking the column fan
    else if (m_col >= 0 && !fan->isActive(m_col))  // column Fan
    {
      for (auto o : Orientations::all()) {
        fan = xsh->getColumnFan(o);
        for (int i = m_col; i >= 0 && !fan->isActive(i); i--) fan->activate(i);
      }

      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      return;
    }
    // set the clicked column to current
    else
      m_viewer->setCurrentColumn(m_col);

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

    if (!isEmpty && m_col >= 0) {
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
      else if (o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell) &&
               event->button() == Qt::LeftButton) {
        m_doOnRelease = isCtrlPressed ? ToggleAllLock : ToggleLock;
      }
      // preview button
      else if (o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell) &&
               event->button() == Qt::LeftButton) {
        if (column->getSoundTextColumn()) {
          // do nothing
        } else {
          m_doOnRelease =
              isCtrlPressed ? ToggleAllPreviewVisible : TogglePreviewVisible;
          if (column->getSoundColumn())
            TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
        }
      }
      // camstand button
      else if (o->rect(PredefinedRect::PREVIEW_LAYER_AREA)
                   .contains(mouseInCell) &&
               event->button() == Qt::LeftButton) {
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
      else if (o->rect(PredefinedRect::CONFIG_AREA).contains(mouseInCell) &&
               event->button() == Qt::LeftButton) {
        if (column->getSoundColumn() || column->getPaletteColumn() ||
            column->getSoundTextColumn()) {
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
            if (s->isPlaying() && interval > 0)
              QTimer::singleShot(interval, this, SLOT(update()));
          }
          update();
        } else if (o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
          setDragTool(XsheetGUI::DragTool::makeVolumeDragTool(m_viewer));
        else
          setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
      }
      // clicking another area means column selection
      else {
        if (m_viewer->getColumnSelection()->isColumnSelected(m_col) &&
            event->button() == Qt::RightButton)
          return;

        setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));

        // toggle columnIcon visibility with alt+click
        TXshLevelColumn *levelColumn = column->getLevelColumn();
        TXshMeshColumn *meshColumn   = column->getMeshColumn();
        if (Preferences::instance()->getColumnIconLoadingPolicy() ==
                Preferences::LoadOnDemand &&
            (event->modifiers() & Qt::AltModifier)) {
          if (levelColumn)
            levelColumn->setIconVisible(!levelColumn->isIconVisible());
          else if (meshColumn)
            meshColumn->setIconVisible(!meshColumn->isIconVisible());
        }
      }
      // synchronize the current column and the current fx
      TApp::instance()->getCurrentFx()->setFx(column->getFx());
    } else if (m_col >= 0) {
      if (m_viewer->getColumnSelection()->isColumnSelected(m_col) &&
          event->button() == Qt::RightButton)
        return;
      setDragTool(XsheetGUI::DragTool::makeColumnSelectionTool(m_viewer));
      TApp::instance()->getCurrentFx()->setFx(0);
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

  int col            = m_viewer->xyToPosition(pos).layer();
  if (col < -1) col  = 0;
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
      else if (o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
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
  } else {
    if (column && column->getSoundColumn()) {
      // sound column
      if (o->rect(PredefinedRect::SOUND_ICON).contains(mouseInCell))
        m_tooltip = tr("Click to play the soundtrack back");
      else if (o->rect(PredefinedRect::VOLUME_AREA).contains(mouseInCell))
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
  if (m_doOnRelease != 0 && m_col != -1) {
    TXshColumn *column = xsh->getColumn(m_col);
    if (m_doOnRelease == ToggleTransparency) {
      column->setCamstandVisible(!column->isCamstandVisible());
      if (column->getSoundColumn())
        app->getCurrentXsheet()->notifyXsheetSoundChanged();
    } else if (m_doOnRelease == TogglePreviewVisible)
      column->setPreviewVisible(!column->isPreviewVisible());
    else if (m_doOnRelease == ToggleLock)
      column->lock(!column->isLocked());
    else if (m_doOnRelease == OpenSettings) {
      if (!m_columnTransparencyPopup)
        m_columnTransparencyPopup = new ColumnTransparencyPopup(this);

      // Align popup to be below to CONFIG button
      QRect configRect =
          m_viewer->orientation()->rect(PredefinedRect::CONFIG_AREA);
      QPoint pos = event->pos();
      int col    = m_viewer->xyToPosition(pos).layer();
      CellPosition cellPosition(0, col);
      QPoint topLeft     = m_viewer->positionToXY(cellPosition);
      QPoint mouseInCell = pos - topLeft;
      int x              = configRect.left() - mouseInCell.x() +
              1;  // distance from right edge of CONFIG button
      int y =
          mouseInCell.y() -
          configRect.bottom();  // distance from bottum edge of CONFIG button
      m_columnTransparencyPopup->move(event->globalPos().x() + x,
                                      event->globalPos().y() - y);

      openTransparencyPopup();
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
          if (column->getSoundColumn()) sound_changed = true;
        }
      }

      if (sound_changed) {
        app->getCurrentXsheet()->notifyXsheetSoundChanged();
      }
    } else if (m_doOnRelease == ToggleAllLock) {
      for (col = 0; col < totcols; col++) {
        TXshColumn *column = xsh->getColumn(col);
        if (!xsh->isColumnEmpty(col)) {
          column->lock(!column->isLocked());
        }
      }
    } else
      assert(false);

    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentXsheet()->notifyXsheetChanged();
    update();
    m_doOnRelease = 0;
  }

  if (m_transparencyPopupTimer) m_transparencyPopupTimer->stop();

  m_viewer->setQtModifiers(0);
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

#ifdef LINETEST
  // Camera column
  if (col == -1) return;
#endif

  if (!o->rect(PredefinedRect::LAYER_NAME).contains(mouseInCell)) return;

  TXsheet *xsh = m_viewer->getXsheet();
  if (col >= 0 && xsh->isColumnEmpty(col)) return;

  QRect renameRect = o->rect(PredefinedRect::RENAME_COLUMN).translated(topLeft);
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
  if (col < 0)  // CAMERA
    return;
  m_viewer->setCurrentColumn(col);
  TXsheet *xsh       = m_viewer->getXsheet();
  QPoint topLeft     = m_viewer->positionToXY(CellPosition(0, col));
  QPoint mouseInCell = event->pos() - topLeft;

  QMenu menu(this);
  CommandManager *cmdManager = CommandManager::instance();

  //---- Preview
  if (!xsh->isColumnEmpty(col) &&
      o->rect(PredefinedRect::EYE_AREA).contains(mouseInCell)) {
    menu.setObjectName("xsheetColumnAreaMenu_Preview");

    menu.addAction(cmdManager->getAction("MI_EnableThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_EnableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_EnableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableAllColumns"));
    menu.addAction(cmdManager->getAction("MI_DisableSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_SwapEnabledColumns"));
  }
  //---- Lock
  else if (!xsh->isColumnEmpty(col) &&
           o->rect(PredefinedRect::LOCK_AREA).contains(mouseInCell)) {
    menu.setObjectName("xsheetColumnAreaMenu_Lock");

    menu.addAction(cmdManager->getAction("MI_LockThisColumnOnly"));
    menu.addAction(cmdManager->getAction("MI_LockSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_LockAllColumns"));
    menu.addAction(cmdManager->getAction("MI_UnlockSelectedColumns"));
    menu.addAction(cmdManager->getAction("MI_UnlockAllColumns"));
    menu.addAction(cmdManager->getAction("MI_ToggleColumnLocks"));
  }
  //---- Camstand
  else if (!xsh->isColumnEmpty(col) &&
           o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(mouseInCell)) {
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
  // right clicking another area / right clicking empty column head
  else {
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
    menu.addAction(cmdManager->getAction(MI_FoldColumns));
    menu.addSeparator();
    menu.addAction(cmdManager->getAction(MI_ToggleXSheetToolbar));

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
  const set<int> indexes = selection->getIndices();
  set<int>::const_iterator it;
  for (it = indexes.begin(); it != indexes.end(); it++) {
    TXshColumn *column          = xsh->getColumn(*it);
    TXshColumn::ColumnType type = column->getColumnType();
    if (type != TXshColumn::eLevelType) continue;
    const QSet<TXshSimpleLevel *> levels = getLevels(column);
    QSet<TXshSimpleLevel *>::const_iterator it2;
    for (it2 = levels.begin(); it2 != levels.end(); it2++) {
      TXshSimpleLevel *sl = *it2;
      if (sl->getProperties()->getDirtyFlag()) continue;
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
