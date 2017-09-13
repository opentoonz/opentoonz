

#include "toonzqt/functionsheet.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/functionviewer.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tunit.h"

#include "../toonz/menubarcommandids.h"

// Qt includes
#include <QPainter>
#include <QGridLayout>
#include <QPaintEvent>
#include <QMenu>
#include <QApplication>  //for drag&drop
#include <QDrag>

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {

const int cColumnDragHandleWidth = 8;

const int cGroupShortNameY =
    0;  //!< Column header's y pos for channel groups' short name tabs
const int cGroupLongNameY = 27;  //!< Same for its long name tabs
const int cChannelNameY = 50;  //!< Column header's y pos of channel name tabs,
                               //! up to the widget's height
const int cColHeadersEndY = 87;  //!< End of column headers y pos

}  // namespace

//********************************************************************************
//    Function Sheet Tools
//********************************************************************************

/*--- NumericalColumnsのセグメントの左側のバーをクリックしたとき ---*/
class MoveChannelsDragTool final : public Spreadsheet::DragTool {
  FunctionSheet *m_sheet;
  std::vector<KeyframeSetter *> m_setters;
  int m_oldRow;
  QRect m_selectedCells;
  int m_firstKeyframeRow;

public:
  MoveChannelsDragTool(FunctionSheet *sheet)
      : m_sheet(sheet), m_firstKeyframeRow(-1) {}

  void click(int row, int col, QMouseEvent *e) override {
    m_firstKeyframeRow                  = -1;
    FunctionTreeModel::Channel *channel = m_sheet->getChannel(col);
    if (!channel) return;
    TDoubleParam *curve = channel->getParam();
    int k0 = -1, k1 = -1;
    if (curve->isKeyframe(row))
      k0 = k1 = curve->getClosestKeyframe(row);
    else {
      k0 = curve->getPrevKeyframe(row);
      k1 = curve->getNextKeyframe(row);
    }
    // return if clicking outside of the segments
    if (k0 < 0 || k1 < 0) return;
    int r0 = tround(curve->keyframeIndexToFrame(k0));
    int r1 = tround(curve->keyframeIndexToFrame(k1));
    if (m_sheet->isSelectedCell(row, col)) {
      m_selectedCells = m_sheet->getSelectedCells();
      m_selectedCells.setTop(r0);
      m_selectedCells.setBottom(r1);
    } else
      m_selectedCells = QRect(col, r0, 1, r1 - r0 + 1);

    m_sheet->selectCells(m_selectedCells);

    /*---
シンプルに左のバーをクリックした場合はcolは1つだけ。
すでに複数Columnが選択されている上でその選択範囲内のセルの左バーをクリックした場合は
横（Column）幅は選択範囲に順ずるようになる。高さ(row)はクリックしたセグメントと同じになる。
---*/
    /*--- セグメントごとドラッグに備えてKeyFrameを格納する ---*/
    for (int col = m_selectedCells.left(); col <= m_selectedCells.right();
         ++col) {
      TDoubleParam *curve = m_sheet->getCurve(col);
      if (!curve) continue;
      KeyframeSetter *setter = new KeyframeSetter(curve);
      for (int k = 0; k < curve->getKeyframeCount(); k++) {
        int row = (int)curve->keyframeIndexToFrame(k);
        if (r0 <= row && row <= r1) {
          if (m_firstKeyframeRow < 0 || row < m_firstKeyframeRow)
            m_firstKeyframeRow = row;
          setter->selectKeyframe(k);
        }
      }
      m_setters.push_back(setter);
    }
    m_oldRow = row;
  }

  void drag(int row, int col, QMouseEvent *e) override {
    int d                             = row - m_oldRow;
    m_oldRow                          = row;
    if (d + m_firstKeyframeRow < 0) d = -m_firstKeyframeRow;
    m_firstKeyframeRow += d;
    for (int i = 0; i < (int)m_setters.size(); i++)
      m_setters[i]->moveKeyframes(d, 0.0);
    m_selectedCells.translate(0, d);
    m_sheet->selectCells(m_selectedCells);
  }

  void release(int row, int col, QMouseEvent *e) override {
    for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i];
    m_setters.clear();
  }
};

//-----------------------------------------------------------------------------
/*--- NumericalColumnsのセル部分をクリックしたとき ---*/
class FunctionSheetSelectionTool final : public Spreadsheet::DragTool {
  int m_firstRow, m_firstCol;
  FunctionSheet *m_sheet;

public:
  FunctionSheetSelectionTool(FunctionSheet *sheet)
      : m_sheet(sheet), m_firstRow(-1), m_firstCol(-1) {}

  void click(int row, int col, QMouseEvent *e) override {
    if (0 != (e->modifiers() & Qt::ShiftModifier) &&
        !m_sheet->getSelectedCells().isEmpty()) {
      QRect selectedCells = m_sheet->getSelectedCells();
      if (col < selectedCells.center().x()) {
        m_firstCol = selectedCells.right();
        selectedCells.setLeft(col);
      } else {
        m_firstCol = selectedCells.left();
        selectedCells.setRight(col);
      }
      if (row < selectedCells.center().y()) {
        m_firstRow = selectedCells.bottom();
        selectedCells.setTop(row);
      } else {
        m_firstRow = selectedCells.top();
        selectedCells.setBottom(row);
      }
      m_sheet->selectCells(selectedCells);
    } else {
      // regular click, no shift
      m_firstCol = col;
      m_firstRow = row;
      QRect selectedCells(col, row, 1, 1);
      m_sheet->selectCells(selectedCells);
    }
  }

  void drag(int row, int col, QMouseEvent *e) override {
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    int r0           = qMin(row, m_firstRow);
    int r1           = qMax(row, m_firstRow);
    int c0           = qMin(col, m_firstCol);
    int c1           = qMax(col, m_firstCol);
    QRect selectedCells(c0, r0, c1 - c0 + 1, r1 - r0 + 1);
    m_sheet->selectCells(selectedCells);
  }

  void release(int row, int col, QMouseEvent *e) override {
    if (row == m_firstRow && col == m_firstCol) {
      if (Preferences::instance()->isMoveCurrentEnabled())
        m_sheet->setCurrentFrame(row);
      FunctionTreeModel::Channel *channel = m_sheet->getChannel(col);
      if (channel) channel->setIsCurrent(true);
    }
  }
};

//********************************************************************************
//    FunctionSheetRowViewer  implementation
//********************************************************************************

FunctionSheetRowViewer::FunctionSheetRowViewer(FunctionSheet *parent)
    : Spreadsheet::RowPanel(parent), m_sheet(parent) {
  setMinimumSize(QSize(100, 100));
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);
}

//-----------------------------------------------------------------------------

void FunctionSheetRowViewer::paintEvent(QPaintEvent *e) {
  // calls GenericPanel's event
  Spreadsheet::RowPanel::paintEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheetRowViewer::mousePressEvent(QMouseEvent *e) {
  // calls GenericPanel's event
  Spreadsheet::RowPanel::mousePressEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheetRowViewer::mouseMoveEvent(QMouseEvent *e) {
  // calls GenericPanel's event
  Spreadsheet::RowPanel::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheetRowViewer::mouseReleaseEvent(QMouseEvent *e) {
  Spreadsheet::RowPanel::mouseReleaseEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheetRowViewer::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu                = new QMenu(this);
  CommandManager *cmdManager = CommandManager::instance();
  menu->addAction(cmdManager->getAction(MI_InsertSceneFrame));
  menu->addAction(cmdManager->getAction(MI_RemoveSceneFrame));
  menu->addAction(cmdManager->getAction(MI_InsertGlobalKeyframe));
  menu->addAction(cmdManager->getAction(MI_RemoveGlobalKeyframe));
  menu->exec(event->globalPos());
}

//********************************************************************************
//    FunctionSheetColumnHeadViewer  implementation
//********************************************************************************

FunctionSheetColumnHeadViewer::FunctionSheetColumnHeadViewer(
    FunctionSheet *parent)
    : Spreadsheet::ColumnPanel(parent), m_sheet(parent), m_draggingChannel(0) {
  setMouseTracking(true);  // for updating the tooltip
  setFocusPolicy(Qt::NoFocus);
}

//-----------------------------------------------------------------------------

void FunctionSheetColumnHeadViewer::paintEvent(QPaintEvent *e) {
  QPainter painter(this);

  QRect toBeUpdated = e->rect();
  painter.setClipRect(toBeUpdated);

  // visible columns range
  CellRange visible = getViewer()->xyRectToRange(toBeUpdated);
  int c0            = visible.from().layer();
  int c1            = visible.to().layer();

  if (c0 > c1) return;

  int n = c1 - c0 + 1 + 2;

  FunctionTreeModel::ChannelGroup *currentGroup = 0;

  /*--- Display range + right and left 1 column range ChannelGroup. If there is
   * none, put 0
   * ---*/
  std::vector<FunctionTreeModel::ChannelGroup *> channelGroups(n);
  for (int i = 0; i < n; ++i) {
    channelGroups[i] = 0;

    int c = c0 - 1 + i;
    if (c < 0) continue;

    FunctionTreeModel::Channel *channel = m_sheet->getChannel(c);
    if (!channel) continue;

    channelGroups[i] = channel->getChannelGroup();
    if (!currentGroup && channel->isCurrent()) currentGroup = channelGroups[i];
  }

  int y0 = 0;
  int y1 = 17;  // needs work
  int y2 = 34;
  int y3 = 53;

  /*--- Fill header with background color ---*/
  for (int c = c0; c <= c1; c++) {
    FunctionTreeModel::Channel *channel = m_sheet->getChannel(c);
    if (!channel) continue;
    int x0 = getViewer()->columnToX(c);
    int x1 = getViewer()->columnToX(c + 1) - 1;
    // Column Width
    int width = x1 - x0 + 1;

    painter.fillRect(x0, y0, width, y3 - y0, getViewer()->getBGColor());
  }

  for (int c = c0; c <= c1; ++c) {
    FunctionTreeModel::Channel *channel = m_sheet->getChannel(c);
    if (!channel) continue;
    int i = c - c0 + 1;
    /*---- Channel Column of the current Column and the preceding and following
     * Columns ---*/
    FunctionTreeModel::ChannelGroup *prevGroup = channelGroups[c - c0],
                                    *group     = channelGroups[c - c0 + 1],
                                    *nextGroup = channelGroups[c - c0 + 2];

    /*---- If the group is different from the before and after, flags are set
     * respectively ---*/
    bool firstGroupColumn = prevGroup != group;
    bool lastGroupColumn  = nextGroup != group;

    /*--- The left and right coordinates of the current column ---*/
    int x0 = getViewer()->columnToX(c);
    int x1 = getViewer()->columnToX(c + 1) - 1;
    // Column width
    int width = x1 - x0 + 1;

    QRect selectedRect = m_sheet->getSelectedCells();
    bool isSelected =
        (selectedRect.left() <= c && c <= selectedRect.right()) ? true : false;

    // paint with light color if selected
    if (isSelected)
      painter.fillRect(x0, y1, width, y3 - y1,
                       getViewer()->getCurrentRowBgColor());

    // draw horizonntal lines
    painter.setPen(QPen(getViewer()->getColumnHeaderBorderColor(), 3));
    painter.drawLine(x0, y0, x1, y0);
    painter.setPen(getViewer()->getColumnHeaderBorderColor());
    painter.drawLine(x0, y1, x1, y1);

    // draw vertical bar
    painter.fillRect(x0, y1, 6, y3 - y1,
                     getViewer()->getColumnHeaderBorderColor());
    if (firstGroupColumn)
      painter.fillRect(x0, y0, 6, y1 - y0,
                       getViewer()->getColumnHeaderBorderColor());

    // channel name
    painter.setPen(getViewer()->getTextColor());
    if (channel->isCurrent())
      painter.setPen(getViewer()->getSelectedColumnTextColor());

    QString text = channel->getShortName();
    int d        = 8;
    painter.drawText(x0 + d, y1, width - d, y3 - y1 + 1,
                     Qt::TextWrapAnywhere | Qt::AlignLeft | Qt::AlignVCenter,
                     text);

    // group name
    if (firstGroupColumn) {
      int tmpwidth = (lastGroupColumn) ? width : width * 2;
      painter.setPen(getViewer()->getTextColor());
      if (group == currentGroup)
        painter.setPen(getViewer()->getSelectedColumnTextColor());
      text = group->getShortName();
      painter.drawText(x0 + d, y0, tmpwidth - d, y1 - y0 + 1,
                       Qt::AlignLeft | Qt::AlignVCenter, text);
    }
  }
}

//-----------------------------------------------------------------------------
/*! update tooltips
*/
void FunctionSheetColumnHeadViewer::mouseMoveEvent(QMouseEvent *e) {
  if ((e->buttons() & Qt::MidButton) && m_draggingChannel &&
      (e->pos() - m_dragStartPosition).manhattanLength() >=
          QApplication::startDragDistance()) {
    QDrag *drag         = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setText(m_draggingChannel->getExprRefName());
    drag->setMimeData(mimeData);
    static const QPixmap cursorPixmap(":Resources/dragcursor_exp_text.png");
    drag->setDragCursor(cursorPixmap, Qt::MoveAction);
    Qt::DropAction dropAction = drag->exec();
    return;
  }

  // get the column under the cursor
  int col = getViewer()->xyToPosition(e->pos()).layer();
  FunctionTreeModel::Channel *channel = m_sheet->getChannel(col);
  if (!channel) {
    setToolTip(QString(""));
  } else
    setToolTip(channel->getExprRefName());
}

//-----------------------------------------------------------------------------

void FunctionSheetColumnHeadViewer::mousePressEvent(QMouseEvent *e) {
  QPoint pos   = e->pos();
  int currentC = getViewer()->xyToPosition(pos).layer();
  FunctionTreeModel::Channel *channel;
  for (int c = 0; c <= m_sheet->getChannelCount(); c++) {
    channel = m_sheet->getChannel(c);
    if (!channel || c != currentC) continue;
    break;
  }
  if (channel && e->button() == Qt::MidButton) {
    m_draggingChannel   = channel;
    m_dragStartPosition = e->pos();
    return;
  } else if (channel)
    channel->setIsCurrent(true);
  m_draggingChannel = 0;
  if (!channel) return;

  // Open folder
  FunctionTreeModel::ChannelGroup *channelGroup = channel->getChannelGroup();
  if (!channelGroup->isOpen())
    channelGroup->getModel()->setExpandedItem(channelGroup->createIndex(),
                                              true);
  // Select all segment
  std::set<double> frames;
  channel->getParam()->getKeyframes(frames);

  QRect rect(0, 0, 0, 0);
  if (!frames.empty()) rect = QRect(currentC, 0, 1, (*frames.rbegin()) + 1);

  getViewer()->selectCells(rect);
}

//-----------------------------------------------------------------------------

void FunctionSheetColumnHeadViewer::contextMenuEvent(QContextMenuEvent *ce) {
  // First, select column under cursor
  const QPoint &pos = ce->pos();
  int cursorCol     = getViewer()->xyToPosition(pos).layer();

  if (cursorCol < 0 || cursorCol >= m_sheet->getChannelCount()) return;

  FunctionTreeModel::Channel *channel = m_sheet->getChannel(cursorCol);
  if (!channel) return;

  // Ok, now let's summon a context menu with appropriate options
  FunctionViewer *fv = m_sheet->getViewer();
  if (!fv) {
    assert(fv);
    return;
  }

  const QPoint &globalPos = mapToGlobal(pos);

  if (pos.y() >= cChannelNameY)
    fv->openContextMenu(channel, globalPos);
  else {
    FunctionTreeModel::ChannelGroup *group = channel->getChannelGroup();

    // In this case, commands are different from the tree view. Rather than
    // showing in the tree,
    // channels get ACTIVATED
    QMenu menu;

    QAction showAnimatedOnly(FunctionTreeView::tr("Show Animated Only"), 0);
    QAction showAll(FunctionTreeView::tr("Show All"), 0);
    menu.addAction(&showAnimatedOnly);
    menu.addAction(&showAll);

    // execute menu
    QAction *action = menu.exec(globalPos);

    if (action != &showAll && action != &showAnimatedOnly) return;

    // Process action
    if (action == &showAll) {
      int c, cCount = group->getChildCount();
      for (c = 0; c != cCount; ++c) {
        FunctionTreeModel::Channel *channel =
            dynamic_cast<FunctionTreeModel::Channel *>(group->getChild(c));
        if (channel && !channel->isHidden()) channel->setIsActive(true);
      }
    } else if (action == &showAnimatedOnly) {
      int c, cCount = group->getChildCount();
      for (c = 0; c != cCount; ++c) {
        FunctionTreeModel::Channel *channel =
            dynamic_cast<FunctionTreeModel::Channel *>(group->getChild(c));
        if (channel && !channel->isHidden())
          channel->setIsActive(channel->isAnimated());
      }
    }

    fv->update();
  }
}

//********************************************************************************
//    FunctionSheetCellViewer  implementation
//********************************************************************************

FunctionSheetCellViewer::FunctionSheetCellViewer(FunctionSheet *parent)
    : Spreadsheet::CellPanel(parent)
    , m_sheet(parent)
    , m_editRow(0)
    , m_editCol(0) {
  m_lineEdit = new DVGui::LineEdit(this);
  // lineEdit->setGeometry(10,10,100,30);
  m_lineEdit->hide();
  bool ret = connect(m_lineEdit, SIGNAL(editingFinished()), this,
                     SLOT(onCellEditorEditingFinished()));
  assert(ret);
  setMouseTracking(true);

  setFocusProxy(m_lineEdit);
}

//-----------------------------------------------------------------------------
/*! Called when the cell panel is left/right-clicked
*/
Spreadsheet::DragTool *FunctionSheetCellViewer::createDragTool(QMouseEvent *e) {
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  bool isEmpty              = true;
  TDoubleParam *curve       = m_sheet->getCurve(col);
  if (curve) {
    int kCount = curve->getKeyframeCount();
    if (kCount > 0) {
      int row0 = (int)curve->keyframeIndexToFrame(0);
      int row1 = (int)curve->keyframeIndexToFrame(kCount - 1);
      isEmpty  = row < row0 || row > row1;
    }
  }

  if (!isEmpty) {
    int x = e->pos().x() - getViewer()->columnToX(col);
    if (0 <= x && x < cColumnDragHandleWidth + 9)
      return new MoveChannelsDragTool(m_sheet);
  }
  return new FunctionSheetSelectionTool(m_sheet);

  // return Spreadsheet::CellPanel::createDragTool(e);
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::drawCells(QPainter &painter, int r0, int c0,
                                        int r1, int c1) {
  // key frames
  QColor KeyFrameColor         = getViewer()->getKeyFrameColor();
  QColor KeyFrameBorderColor   = getViewer()->getKeyFrameBorderColor();
  QColor SelectedKeyFrameColor = getViewer()->getSelectedKeyFrameColor();
  // inbetween
  QColor InBetweenColor         = getViewer()->getInBetweenColor();
  QColor InBetweenBorderColor   = getViewer()->getInBetweenBorderColor();
  QColor SelectedInBetweenColor = getViewer()->getSelectedInBetweenColor();
  // empty cells
  QColor SelectedEmptyColor = getViewer()->getSelectedEmptyColor();
  // empty cells in scene frame range
  QColor SelectedSceneRangeEmptyColor =
      getViewer()->getSelectedSceneRangeEmptyColor();

  // top and bottom pos
  int y0 = getViewer()->rowToY(r0);
  int y1 = getViewer()->rowToY(r1 + 1) - 1;
  for (int c = c0; c <= c1; c++) {
    TDoubleParam *curve = m_sheet->getCurve(c);
    /*--- もしカラムcにパラメータが無ければcurveには０が返る ---*/
    if (!curve) continue;
    // left and right pos
    int x0 = getViewer()->columnToX(c);
    int x1 = getViewer()->columnToX(c + 1) - 1;

    // find the curve keyframe range
    int kr0 = 0, kr1 = -1;
    int kCount = curve->getKeyframeCount();
    if (kCount > 0) {
      kr0 = curve->keyframeIndexToFrame(0);
      kr1 = curve->keyframeIndexToFrame(kCount - 1);
    }

    // get the unit
    TMeasure *measure = curve->getMeasure();
    const TUnit *unit = measure ? measure->getCurrentUnit() : 0;

    // draw each cell
    for (int row = r0; row <= r1; row++) {
      int ya = m_sheet->rowToY(row);
      int yb = m_sheet->rowToY(row + 1) - 1;

      bool isSelected = getViewer()->isSelectedCell(row, c);

      double value    = curve->getValue(row);
      if (unit) value = unit->convertTo(value);

      QRect cellRect(x0, ya, x1 - x0 + 1, yb - ya + 1);
      QRect borderRect(x0, ya, 7, yb - ya + 1);
      QColor cellColor, borderColor;

      /*--- キーフレーム間の範囲だけ色をつける ---*/
      if (kr0 <= row && row <= kr1) {
        if (curve->isKeyframe(row)) {
          cellColor   = (isSelected) ? SelectedKeyFrameColor : KeyFrameColor;
          borderColor = KeyFrameBorderColor;
        } else {
          cellColor   = (isSelected) ? SelectedInBetweenColor : InBetweenColor;
          borderColor = InBetweenBorderColor;
        }
        painter.setPen(Qt::NoPen);
        painter.fillRect(cellRect, cellColor);
        painter.fillRect(borderRect, borderColor);

        // display whether segment are Linked
        if (curve->isKeyframe(row)) {
          TDoubleKeyframe kf = curve->getKeyframeAt(row);
          // if the segments are NOT linked, then cut off the side bar
          if (!kf.m_linkedHandles) {
            int rowCenterPos = (ya + yb) / 2;
            QPoint points[4] = {
                QPoint(x0, rowCenterPos), QPoint(x0 + 7, rowCenterPos + 3),
                QPoint(x0 + 7, rowCenterPos - 3), QPoint(x0, rowCenterPos)};
            QBrush oldBrush = painter.brush();
            painter.setBrush(QBrush(cellColor));
            painter.drawPolygon(points, 4);
            painter.setBrush(oldBrush);
          }
        }

        // draw cell value
        painter.setPen(getViewer()->getTextColor());

        /*--- 整数から小数点以下3桁以内の場合はそれ以降の0000を描かない ---*/
        QString text;

        double thousandValue = value * 1000.0;
        if (areAlmostEqual(thousandValue, (double)tround(thousandValue),
                           0.0001)) {
          text = QString::number(value, 'f', 3);
          while (text.endsWith("0")) {
            text.chop(1);
            if (text.endsWith(".")) {
              text.chop(1);
              break;
            }
          }
        } else {
          text = QString::number(value, 'f', 4);
          text.truncate(5);
          text.append("~");
        }

        QString fontName = Preferences::instance()->getInterfaceFont();
        if (fontName == "") {
#ifdef _WIN32
          fontName = "Arial";
#else
          fontName = "Helvetica";
#endif
        }
        static QFont font(fontName, -1, QFont::Bold);
        font.setPixelSize(12);
        painter.setFont(font);
        painter.drawText(cellRect.adjusted(10, 0, 0, 0),
                         Qt::AlignVCenter | Qt::AlignLeft, text);
      }
      // empty and selected cell
      else if (isSelected) {
        int rowCount = getViewer()->getRowCount();
        cellColor    = (row >= rowCount) ? SelectedEmptyColor
                                      : SelectedSceneRangeEmptyColor;
        painter.setPen(Qt::NoPen);
        painter.fillRect(cellRect, cellColor);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::mouseDoubleClickEvent(QMouseEvent *e) {
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  int x0, y0, x1, y1;
  x0 = getViewer()->columnToX(col);
  x1 = getViewer()->columnToX(col + 1) - 1;
  y0 = getViewer()->rowToY(row);
  y1 = getViewer()->rowToY(row + 1) - 1;

  m_editRow = row;
  m_editCol = col;

  TDoubleParam *curve = m_sheet->getCurve(col);
  if (curve) {
    double v          = curve->getValue(row);
    TMeasure *measure = curve->getMeasure();
    const TUnit *unit = measure ? measure->getCurrentUnit() : 0;
    if (unit) v       = unit->convertTo(v);

    m_lineEdit->setText(QString::number(v, 'f', 4));
    // in order to put the cursor to the left end
    m_lineEdit->setSelection(m_lineEdit->text().length(),
                             -m_lineEdit->text().length());
  } else
    m_lineEdit->setText("");

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, 9, QFont::Normal);
  m_lineEdit->setFont(font);

  m_lineEdit->setGeometry(x0 - 2, y0 - 2, x1 - x0 + 1 + 4,
                          y1 - y0 + 1 + 4);  // x0,y0,x1-x0+1,y0-y1+1);
  m_lineEdit->show();
  m_lineEdit->raise();
  m_lineEdit->setFocus();
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::onCellEditorEditingFinished() {
  QString text = m_lineEdit->text();
  if (!text.isEmpty() && m_lineEdit->isReturnPressed()) {
    double value        = text.toDouble();
    TDoubleParam *curve = m_sheet->getCurve(m_editCol);
    if (curve) {
      TMeasure *measure = curve->getMeasure();
      const TUnit *unit = measure ? measure->getCurrentUnit() : 0;
      if (unit) value   = unit->convertFrom(value);
      KeyframeSetter::setValue(curve, m_editRow, value);
    }
  }
  m_lineEdit->hide();
  m_lineEdit->clearFocus();
  m_sheet->setFocus();
  update();
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::mousePressEvent(QMouseEvent *e) {
  // escape from the line edit by clicking outside
  if (m_lineEdit->isVisible()) {
    m_lineEdit->hide();
    m_lineEdit->clearFocus();
    m_sheet->setFocus();
  }

  if (e->button() == Qt::LeftButton || e->button() == Qt::MidButton)
    Spreadsheet::CellPanel::mousePressEvent(e);
  else if (e->button() == Qt::RightButton) {
    update();
    openContextMenu(e);
  }
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::mouseReleaseEvent(QMouseEvent *e) {
  Spreadsheet::CellPanel::mouseReleaseEvent(e);
  /*
  CellPosition cellPosition = getViewer ()->xyToPosition (e->pos ());
int row = cellPosition.frame ();
int col = cellPosition.layer ();
FunctionSheet::DragTool *dragTool = m_sheet->getDragTool();
if(dragTool) dragTool->release(row,col);
m_sheet->setDragTool(0);
*/
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::mouseMoveEvent(QMouseEvent *e) {
  Spreadsheet::CellPanel::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

// TODO: refactor: cfr functionpanel.cpp
void FunctionSheetCellViewer::openContextMenu(QMouseEvent *e) {
  struct locals {
    static void sheet__setSegmentType(FunctionSelection *selection,
                                      TDoubleParam *curve, int segmentIndex,
                                      TDoubleKeyframe::Type type) {
      selection->selectSegment(curve, segmentIndex);
      KeyframeSetter setter(curve, segmentIndex);
      setter.setType(type);
    }
  };  // locals

  QAction deleteKeyframeAction(tr("Delete Key"), 0);
  QAction insertKeyframeAction(tr("Set Key"), 0);
  QAction setLinearAction(tr("Linear Interpolation"), 0);
  QAction setSpeedInOutAction(tr("Speed In / Speed Out Interpolation"), 0);
  QAction setEaseInOutAction(tr("Ease In / Ease Out Interpolation"), 0);
  QAction setEaseInOut2Action(tr("Ease In / Ease Out (%) Interpolation"), 0);
  QAction setExponentialAction(tr("Exponential Interpolation"), 0);
  QAction setExpressionAction(tr("Expression Interpolation"), 0);
  QAction setFileAction(tr("File Interpolation"), 0);
  QAction setConstantAction(tr("Constant Interpolation"), 0);
  QAction setStep1Action(tr("Step 1"), 0);
  QAction setStep2Action(tr("Step 2"), 0);
  QAction setStep3Action(tr("Step 3"), 0);
  QAction setStep4Action(tr("Step 4"), 0);

  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  TDoubleParam *curve       = m_sheet->getCurve(col);
  if (!curve) return;

  bool isEmpty    = true;
  bool isKeyframe = false;

  // find the curve keyframe range
  int kCount = curve->getKeyframeCount();
  if (kCount > 0) {
    if (curve->keyframeIndexToFrame(0) <= row &&
        row <= curve->keyframeIndexToFrame(kCount - 1)) {
      isEmpty    = false;
      isKeyframe = curve->isKeyframe(row);
    }
  }
  int kIndex = curve->getPrevKeyframe(row);

  // build menu
  QMenu menu(0);
  if (!isKeyframe)  // menu.addAction(&deleteKeyframeAction); else
    menu.addAction(&insertKeyframeAction);

  if (!isEmpty && !isKeyframe && kIndex >= 0) {
    menu.addSeparator();
    TDoubleKeyframe kf = curve->getKeyframe(kIndex);
    if (kf.m_type != TDoubleKeyframe::Linear) menu.addAction(&setLinearAction);
    if (kf.m_type != TDoubleKeyframe::SpeedInOut)
      menu.addAction(&setSpeedInOutAction);
    if (kf.m_type != TDoubleKeyframe::EaseInOut)
      menu.addAction(&setEaseInOutAction);
    if (kf.m_type != TDoubleKeyframe::EaseInOutPercentage)
      menu.addAction(&setEaseInOut2Action);
    if (kf.m_type != TDoubleKeyframe::Exponential)
      menu.addAction(&setExponentialAction);
    if (kf.m_type != TDoubleKeyframe::Expression)
      menu.addAction(&setExpressionAction);
    if (kf.m_type != TDoubleKeyframe::File) menu.addAction(&setFileAction);
    if (kf.m_type != TDoubleKeyframe::Constant)
      menu.addAction(&setConstantAction);
    menu.addSeparator();
    if (kf.m_step != 1) menu.addAction(&setStep1Action);
    if (kf.m_step != 2) menu.addAction(&setStep2Action);
    if (kf.m_step != 3) menu.addAction(&setStep3Action);
    if (kf.m_step != 4) menu.addAction(&setStep4Action);
  }

  menu.addSeparator();

  CommandManager *cmdManager = CommandManager::instance();
  menu.addAction(cmdManager->getAction("MI_Cut"));
  menu.addAction(cmdManager->getAction("MI_Copy"));
  menu.addAction(cmdManager->getAction("MI_Paste"));
  menu.addAction(cmdManager->getAction("MI_Clear"));

  menu.addAction(cmdManager->getAction("MI_Insert"));

  FunctionSelection *selection = m_sheet->getSelection();

  // execute menu
  QAction *action = menu.exec(e->globalPos());  // QCursor::pos());
  if (action == &deleteKeyframeAction) {
    KeyframeSetter::removeKeyframeAt(curve, row);
  } else if (action == &insertKeyframeAction) {
    KeyframeSetter(curve).createKeyframe(row);
  } else if (action == &setLinearAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::Linear);
  else if (action == &setSpeedInOutAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::SpeedInOut);
  else if (action == &setEaseInOutAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::EaseInOut);
  else if (action == &setEaseInOut2Action)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::EaseInOutPercentage);
  else if (action == &setExponentialAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::Exponential);
  else if (action == &setExpressionAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::Expression);
  else if (action == &setFileAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::File);
  else if (action == &setConstantAction)
    locals::sheet__setSegmentType(selection, curve, kIndex,
                                  TDoubleKeyframe::Constant);
  else if (action == &setStep1Action)
    KeyframeSetter(curve, kIndex).setStep(1);
  else if (action == &setStep2Action)
    KeyframeSetter(curve, kIndex).setStep(2);
  else if (action == &setStep3Action)
    KeyframeSetter(curve, kIndex).setStep(3);
  else if (action == &setStep4Action)
    KeyframeSetter(curve, kIndex).setStep(4);

  update();
}

//********************************************************************************
//    FunctionSheetColumnToCurveMapper  implementation
//********************************************************************************

class FunctionSheetColumnToCurveMapper final : public ColumnToCurveMapper {
  FunctionSheet *m_sheet;

public:
  FunctionSheetColumnToCurveMapper(FunctionSheet *sheet) : m_sheet(sheet) {}
  TDoubleParam *getCurve(int columnIndex) const override {
    FunctionTreeModel::Channel *channel = m_sheet->getChannel(columnIndex);
    if (channel)
      return channel->getParam();
    else
      return 0;
  }
};

//********************************************************************************
//    FunctionSheet  implementation
//********************************************************************************

FunctionSheet::FunctionSheet(QWidget *parent)
    : SpreadsheetViewer(parent), m_selectedCells(), m_selection(0) {
  setColumnsPanel(m_columnHeadViewer = new FunctionSheetColumnHeadViewer(this));
  setRowsPanel(m_rowViewer = new FunctionSheetRowViewer(this));
  setCellsPanel(m_cellViewer = new FunctionSheetCellViewer(this));

  setColumnCount(20);
}

//-----------------------------------------------------------------------------

FunctionSheet::~FunctionSheet() {}

//-----------------------------------------------------------------------------

bool FunctionSheet::anyWidgetHasFocus() {
  return hasFocus() || m_rowViewer->hasFocus() ||
         m_columnHeadViewer->hasFocus() || m_cellViewer->hasFocus();
}

//-----------------------------------------------------------------------------

void FunctionSheet::setSelection(FunctionSelection *selection) {
  m_selection = selection;
  m_selection->setColumnToCurveMapper(
      new FunctionSheetColumnToCurveMapper(this));
}

//-----------------------------------------------------------------------------

void FunctionSheet::showEvent(QShowEvent *e) {
  m_frameScroller.registerFrameScroller();
  SpreadsheetViewer::showEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheet::hideEvent(QHideEvent *e) {
  m_frameScroller.unregisterFrameScroller();
  SpreadsheetViewer::hideEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheet::onFrameSwitched() {
  setCurrentRow(getCurrentFrame());
  m_rowViewer->update();
  m_cellViewer->update();
}

//-----------------------------------------------------------------------------

void FunctionSheet::setCurrentFrame(int frame) {
  if (getFrameHandle()) getFrameHandle()->setFrame(frame);
}

//-----------------------------------------------------------------------------

int FunctionSheet::getCurrentFrame() const {
  return getFrameHandle() ? getFrameHandle()->getFrame() : 0;
}

//-----------------------------------------------------------------------------

int FunctionSheet::getChannelCount() {
  if (m_functionTreeModel == 0)
    return 0;
  else
    return m_functionTreeModel->getActiveChannelCount();
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel *FunctionSheet::getChannel(int column) {
  if (m_functionTreeModel == 0)
    return 0;
  else
    return m_functionTreeModel->getActiveChannel(column);
}

//-----------------------------------------------------------------------------

TDoubleParam *FunctionSheet::getCurve(int column) {
  FunctionTreeModel::Channel *channel = getChannel(column);
  return channel ? channel->getParam() : 0;
}

//-----------------------------------------------------------------------------

void FunctionSheet::setModel(FunctionTreeModel *model) {
  m_functionTreeModel = model;
}

//-----------------------------------------------------------------------------

void FunctionSheet::setViewer(FunctionViewer *viewer) {
  m_functionViewer = viewer;
}

//-----------------------------------------------------------------------------

QRect FunctionSheet::getSelectedCells() const {
  if (getSelection())
    return getSelection()->getSelectedCells();
  else
    return QRect();
}

//-----------------------------------------------------------------------------

void FunctionSheet::selectCells(const QRect &selectedCells) {
  m_selectedCells = selectedCells;
  if (getSelection()) {
    QList<TDoubleParam *> curves;
    for (int c = selectedCells.left(); c <= selectedCells.right(); c++) {
      TDoubleParam *param              = 0;
      if (c < getChannelCount()) param = getChannel(c)->getParam();
      curves.push_back(param);
    }
    getSelection()->selectCells(selectedCells, curves);

    if (selectedCells.width() == 1 && curves[0] &&
        !getChannel(selectedCells.x())->isCurrent())
      getChannel(selectedCells.x())->setIsCurrent(true);
  }

  updateAll();
}

//-----------------------------------------------------------------------------

void FunctionSheet::updateAll() {
  m_rowViewer->update();
  m_columnHeadViewer->update();
  m_cellViewer->update();
  setColumnCount(getChannelCount());
}

//-----------------------------------------------------------------------------
/*! Display expression name of the current segment
*/
QString FunctionSheet::getSelectedParamName() {
  if (m_functionTreeModel->getCurrentChannel())
    return m_functionTreeModel->getCurrentChannel()->getExprRefName();
  else
    return QString();
}

//-----------------------------------------------------------------------------

int FunctionSheet::getColumnIndexByCurve(TDoubleParam *param) const {
  return m_functionTreeModel->getColumnIndexByCurve(param);
}

//-----------------------------------------------------------------------------
/*! scroll column to show the current one
*/
void FunctionSheet::onCurrentChannelChanged(
    FunctionTreeModel::Channel *channel) {
  if (!channel) return;
  for (int c = 0; c < getChannelCount(); c++) {
    FunctionTreeModel::Channel *tmpChan = getChannel(c);

    if (tmpChan == channel) {
      ensureVisibleCol(c);
      return;
    }
  }
}
