

#include "toonzqt/functionsheet.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/functionviewer.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tstageobject.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"

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
#include <QPushButton>

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
    int d    = row - m_oldRow;
    m_oldRow = row;
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
    int r0 = std::min(row, m_firstRow);
    int r1 = std::max(row, m_firstRow);
    int c0 = std::min(col, m_firstCol);
    int c1 = std::max(col, m_firstCol);
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
//    FunctionSheetButtonArea  implementation
//********************************************************************************

FunctionSheetButtonArea::FunctionSheetButtonArea(QWidget *parent)
    : QWidget(parent) {
  m_syncSizeBtn = new QPushButton("", this);
  m_syncSizeBtn->setCheckable(true);
  m_syncSizeBtn->setFixedSize(20, 20);
  static QPixmap syncScaleImg =
      recolorPixmap(svgToPixmap(getIconThemePath("actions/17/syncscale.svg")));
  QPixmap offPm(17, 17);
  offPm.fill(Qt::transparent);
  {
    QPainter p(&offPm);
    p.setOpacity(0.7);
    p.drawPixmap(0, 0, syncScaleImg);
  }
  QIcon icon;
  icon.addPixmap(offPm);
  icon.addPixmap(syncScaleImg, QIcon::Normal, QIcon::On);
  m_syncSizeBtn->setIcon(icon);

  m_syncSizeBtn->setToolTip(tr("Toggle synchronizing zoom with xsheet"));

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setMargin(2);
  layout->setSpacing(0);
  {
    layout->addStretch();
    layout->addWidget(m_syncSizeBtn, 0, Qt::AlignCenter);
  }
  setLayout(layout);

  connect(m_syncSizeBtn, SIGNAL(clicked(bool)), this,
          SIGNAL(syncSizeBtnToggled(bool)));
}

void FunctionSheetButtonArea::setSyncSizeBtnState(bool on) {
  m_syncSizeBtn->setChecked(on);
}

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
  int y3 = 53;
  if (m_sheet->isSmallHeader()) y3 = 34;

  /*--- Fill header with background color ---*/
  for (int c = c0; c <= c1; c++) {
    FunctionTreeModel::Channel *channel = m_sheet->getChannel(c);
    if (!channel) break;
    int x0 = getViewer()->columnToX(c);
    int x1 = getViewer()->columnToX(c + 1) - 1;
    // Column Width
    int width = x1 - x0 + 1;

    painter.fillRect(x0, y0, width, y3 - y0, getViewer()->getBGColor());
  }

  for (int c = c0; c <= c1; ++c) {
    FunctionTreeModel::Channel *channel = m_sheet->getChannel(c);
    if (!channel) {
      if (c != c0) {
        // draw "the end" border
        int x0 = getViewer()->columnToX(c);
        painter.setPen(getViewer()->getColumnHeaderBorderColor());
        painter.drawLine(x0, y0, x0, y3);
      }
      break;
    }
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
      painter.setPen(m_sheet->getViewer()->getCurrentTextColor());

    QString text = channel->getShortName();
    int d        = 8;
    int flags    = Qt::AlignLeft | Qt::AlignVCenter;
    if (!m_sheet->isSmallHeader()) flags |= Qt::TextWrapAnywhere;
    painter.drawText(x0 + d, y1, width - d, y3 - y1 + 1, flags, text);

    // warning of losing expression reference
    TXsheet *xsh = m_sheet->getViewer()->getXsheetHandle()->getXsheet();
    if (xsh->isReferenceManagementIgnored(channel->getParam())) {
      static QIcon paramIgnoredIcon(":Resources/paramignored_on.svg");
      painter.drawPixmap(QPoint(x0 + 30, y1 + 20),
                         paramIgnoredIcon.pixmap(21, 17));
    }

    // group name
    if (firstGroupColumn) {
      int tmpwidth = (lastGroupColumn) ? width : width * 2;
      painter.setPen(getViewer()->getTextColor());
      if (group == currentGroup)
        painter.setPen(m_sheet->getViewer()->getCurrentTextColor());
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
  } else {
    QString tooltip = channel->getExprRefName();
    TXsheet *xsh    = m_sheet->getViewer()->getXsheetHandle()->getXsheet();
    if (xsh->isReferenceManagementIgnored(channel->getParam()))
      tooltip +=
          "\n" + tr("Some key(s) in this parameter loses original reference in "
                    "expression.\nManually changing any keyframe will clear "
                    "the warning.");

    setToolTip(tooltip);
  }

  // modify selected channel by left dragging
  if (m_clickedColumn >= 0 && channel && e->buttons() & Qt::LeftButton) {
    int fromC      = std::min(m_clickedColumn, col);
    int toC        = std::max(m_clickedColumn, col);
    int lastKeyPos = 0;
    for (int c = fromC; c <= toC; c++) {
      FunctionTreeModel::Channel *tmpChan = m_sheet->getChannel(c);
      if (!tmpChan) continue;
      std::set<double> frames;
      tmpChan->getParam()->getKeyframes(frames);
      if (!frames.empty())
        lastKeyPos = std::max(lastKeyPos, (int)*frames.rbegin());
    }
    QRect rect(std::min(m_clickedColumn, col), 0,
               std::abs(col - m_clickedColumn) + 1, lastKeyPos + 1);
    getViewer()->selectCells(rect);
  }
}

//-----------------------------------------------------------------------------

void FunctionSheetColumnHeadViewer::mousePressEvent(QMouseEvent *e) {
  QPoint pos                          = e->pos();
  int currentC                        = getViewer()->xyToPosition(pos).layer();
  FunctionTreeModel::Channel *channel = m_sheet->getChannel(currentC);
  if (!channel) {
    m_clickedColumn = -1;
    return;
  }

  if (e->button() == Qt::MidButton) {
    m_draggingChannel   = channel;
    m_dragStartPosition = e->pos();
    return;
  } else
    channel->setIsCurrent(true);
  m_draggingChannel = 0;

  if (e->button() == Qt::LeftButton) {
    int lastKeyPos = 0;
    // if the current selection does not contain the first cell in m_firstColumn
    // then we assume that the selection has been modified and treat shift+click
    // as normal click.
    if (getViewer()->getSelectedCells().contains(m_clickedColumn, 0) &&
        (e->modifiers() & Qt::ShiftModifier)) {
      int fromC = std::min(m_clickedColumn, currentC);
      int toC   = std::max(m_clickedColumn, currentC);
      for (int c = fromC; c <= toC; c++) {
        FunctionTreeModel::Channel *tmpChan = m_sheet->getChannel(c);
        if (!tmpChan) continue;
        std::set<double> frames;
        tmpChan->getParam()->getKeyframes(frames);
        if (!frames.empty())
          lastKeyPos = std::max(lastKeyPos, (int)*frames.rbegin());
      }
    } else {
      // Open folder
      FunctionTreeModel::ChannelGroup *channelGroup =
          channel->getChannelGroup();
      if (!channelGroup->isOpen())
        channelGroup->getModel()->setExpandedItem(channelGroup->createIndex(),
                                                  true);
      // Select all segment
      std::set<double> frames;
      channel->getParam()->getKeyframes(frames);
      if (!frames.empty()) lastKeyPos = (int)*frames.rbegin();
      m_clickedColumn = currentC;
    }
    QRect rect(std::min(m_clickedColumn, currentC), 0,
               std::abs(currentC - m_clickedColumn) + 1, lastKeyPos + 1);

    getViewer()->selectCells(rect);
  }
  // Switch selection before opening the context menu
  // if the clicked column is out of the selection
  else if (e->button() == Qt::RightButton) {
    QRect selectedCell = getViewer()->getSelectedCells();
    if (selectedCell.left() > currentC || selectedCell.right() < currentC) {
      int lastKeyPos = 0;
      std::set<double> frames;
      channel->getParam()->getKeyframes(frames);
      if (!frames.empty()) lastKeyPos = (int)*frames.rbegin();
      getViewer()->selectCells(QRect(currentC, 0, 1, lastKeyPos + 1));
    }
  }
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
    QAction hideSelected(FunctionTreeView::tr("Hide Selected"), 0);
    menu.addAction(&showAnimatedOnly);
    menu.addAction(&showAll);
    menu.addAction(&hideSelected);

    // execute menu
    QAction *action = menu.exec(globalPos);

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
    } else if (action == &hideSelected) {
      QRect selectedCells = getViewer()->getSelectedCells();
      // hide the selected columns from the right to the left
      for (int col = selectedCells.right(); col >= selectedCells.left();
           col--) {
        FunctionTreeModel::Channel *chan = m_sheet->getChannel(col);
        if (chan) chan->setIsActive(false);
      }
      // clear cell selection
      getViewer()->selectCells(QRect());
    } else
      return;

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
  ret      = ret && connect(m_lineEdit, SIGNAL(mouseMoved(QMouseEvent *)), this,
                       SLOT(onMouseMovedInLineEdit(QMouseEvent *)));
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
  QColor IgnoredKeyFrameColor  = getViewer()->getIgnoredKeyFrameColor();
  QColor SelectedIgnoredKeyFrameColor =
      getViewer()->getSelectedIgnoredKeyFrameColor();
  // inbetween
  QColor InBetweenColor         = getViewer()->getInBetweenColor();
  QColor InBetweenBorderColor   = getViewer()->getInBetweenBorderColor();
  QColor SelectedInBetweenColor = getViewer()->getSelectedInBetweenColor();
  QColor IgnoredInBetweenColor  = getViewer()->getIgnoredInBetweenColor();
  QColor SelectedIgnoredInBetweenColor =
      getViewer()->getSelectedIgnoredInBetweenColor();

  // empty cells
  QColor SelectedEmptyColor = getViewer()->getSelectedEmptyColor();
  // empty cells in scene frame range
  QColor SelectedSceneRangeEmptyColor =
      getViewer()->getSelectedSceneRangeEmptyColor();

  TXsheet *xsh = m_sheet->getViewer()->getXsheetHandle()->getXsheet();

  bool simpleView = getViewer()->getFrameZoomFactor() <=
                    Orientations::topToBottom()->dimension(
                        PredefinedDimension::SCALE_THRESHOLD);
  bool showIbtwn = !simpleView && m_sheet->isIbtwnValueVisible();

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

    bool isStageObjectCycled = false;
    TStageObject *obj        = m_sheet->getStageObject(c);
    if (obj && obj->isCycleEnabled()) isStageObjectCycled = true;

    bool isParamCycled = curve->isCycleEnabled();
    int rowCount       = getViewer()->getRowCount();

    bool isRefMngIgnored = xsh->isReferenceManagementIgnored(curve);

    // draw each cell
    for (int row = r0; row <= r1; row++) {
      int ya = m_sheet->rowToY(row);
      int yb = m_sheet->rowToY(row + 1) - 1;

      bool isSelected = getViewer()->isSelectedCell(row, c);

      double value = (isStageObjectCycled)
                         ? curve->getValue(obj->paramsTime((double)row))
                         : curve->getValue(row);
      if (unit) value = unit->convertTo(value);
      enum { None, Key, Inbetween, CycleRange } drawValue = None;

      QRect cellRect(x0, ya, x1 - x0 + 1, yb - ya + 1);
      QRect borderRect(x0, ya, 7, yb - ya + 1);
      QColor cellColor, borderColor;

      /*--- キーフレーム間の範囲だけ色をつける ---*/
      if (kr0 <= row && row <= kr1) {
        if (curve->isKeyframe(row)) {
          cellColor =
              (isRefMngIgnored)
                  ? ((isSelected) ? SelectedIgnoredKeyFrameColor
                                  : IgnoredKeyFrameColor)
                  : ((isSelected) ? SelectedKeyFrameColor : KeyFrameColor);
          borderColor = KeyFrameBorderColor;
        } else {
          cellColor =
              (isRefMngIgnored)
                  ? ((isSelected) ? SelectedIgnoredInBetweenColor
                                  : IgnoredInBetweenColor)
                  : ((isSelected) ? SelectedInBetweenColor : InBetweenColor);
          borderColor = InBetweenBorderColor;

          // when the inbetween values are hidden, change the cell colors to
          // semi-transparent if the frame is in middle of the value step
          if (!showIbtwn) {
            TDoubleKeyframe kf =
                curve->getKeyframe(curve->getPrevKeyframe(row));
            int step = kf.m_step;
            if (step > 1 && (row - (int)std::floor(kf.m_frame)) % step != 0)
              cellColor.setAlpha(128);
          }
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

        drawValue =
            (curve->isKeyframe(row)) ? Key : (showIbtwn) ? Inbetween : None;

      }
      // empty cells
      else {
        // show values for cycled parameter.
        // cycle option can be set in two ways; one is as TStageObject,
        // the other is as TDoubleParam.
        // - TStageObject cycle literally cycles values with no offset.
        //   Applied to all transformation parameters of the cycled object.
        // - TDoubleParam cycle includes value offset so that the curve
        //   connects smoothly.
        // - TStageObject cycle option has a priority to TDoubleParam one.
        // (see TStageObject::paramsTime() in tstageobject.cpp)
        if (kCount > 0 && row > kr1 && (isStageObjectCycled || isParamCycled) &&
            (row < rowCount)) {
          drawValue = CycleRange;
        }
        // empty and selected cell
        if (isSelected) {
          cellColor = (row >= rowCount) ? SelectedEmptyColor
                                        : SelectedSceneRangeEmptyColor;
          painter.setPen(Qt::NoPen);
          painter.fillRect(cellRect, cellColor);
        }
      }

      if (drawValue != None) {
        // draw cell value
        if (drawValue == Key || drawValue == Inbetween)
          painter.setPen(getViewer()->getTextColor());
        else {
          QColor semiTranspTextColor = getViewer()->getTextColor();
          semiTranspTextColor.setAlpha(128);
          painter.setPen(semiTranspTextColor);
        }

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
        static QFont font(fontName, -1);
        font.setBold(drawValue == Key);
        font.setPixelSize(12);
        painter.setFont(font);
        painter.drawText(cellRect.adjusted(10, 0, 0, 0),
                         Qt::AlignVCenter | Qt::AlignLeft, text);
      }
    }

    if (kCount > 0 && (isStageObjectCycled || isParamCycled)) {
      // draw the row zigzag
      int ymax           = m_sheet->rowToY(r1 + 1);
      int qx             = x0 + 4;
      int qy             = m_sheet->rowToY(kr1 + 1);
      int zig            = 2;
      QColor zigzagColor = (isStageObjectCycled) ? getViewer()->getTextColor()
                                                 : KeyFrameBorderColor;
      painter.setPen(zigzagColor);
      painter.drawLine(QPoint(qx, qy), QPoint(qx - zig, qy + zig));
      qy += zig;
      while (qy < ymax) {
        painter.drawLine(QPoint(qx - zig, qy), QPoint(qx + zig, qy + 2 * zig));
        painter.drawLine(QPoint(qx + zig, qy + 2 * zig),
                         QPoint(qx - zig, qy + 4 * zig));
        qy += 4 * zig;
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
    if (unit) v = unit->convertTo(v);
    m_currentValue = v;
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
  if (!text.isEmpty() &&
      (m_lineEdit->isReturnPressed() || m_lineEdit->getMouseDragEditing())) {
    double value        = text.toDouble();
    TDoubleParam *curve = m_sheet->getCurve(m_editCol);
    if (curve) {
      TMeasure *measure = curve->getMeasure();
      const TUnit *unit = measure ? measure->getCurrentUnit() : 0;
      if (unit) value = unit->convertFrom(value);
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
  if (e->button() == Qt::LeftButton && e->modifiers() == Qt::ControlModifier) {
    mouseDoubleClickEvent(e);
    if (m_lineEdit->text() != "") {
      m_lineEdit->setMouseDragEditing(true);
      m_mouseXPosition = e->x();
    }
  } else if (e->button() == Qt::LeftButton &&
             e->modifiers() == Qt::AltModifier) {
    CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
    int row                   = cellPosition.frame();
    int col                   = cellPosition.layer();
    TDoubleParam *curve       = m_sheet->getCurve(col);
    if (curve) {
      KeyframeSetter::removeKeyframeAt(curve, row);
    }
  } else if (e->button() == Qt::LeftButton || e->button() == Qt::MidButton)
    Spreadsheet::CellPanel::mousePressEvent(e);
  else if (e->button() == Qt::RightButton) {
    update();
    openContextMenu(e);
  }
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::mouseReleaseEvent(QMouseEvent *e) {
  if (m_lineEdit->getMouseDragEditing()) {
    std::string textValue = m_lineEdit->text().toStdString();
    onCellEditorEditingFinished();
    m_lineEdit->setMouseDragEditing(false);
  } else
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
  if (m_lineEdit->getMouseDragEditing()) {
    double newValue = m_currentValue + ((e->x() - m_mouseXPosition) / 2);
    m_lineEdit->setText(QString::number(newValue, 'f', 4));
    m_updatedValue = newValue;
  } else
    Spreadsheet::CellPanel::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

void FunctionSheetCellViewer::onMouseMovedInLineEdit(QMouseEvent *event) {
  if (m_lineEdit->getMouseDragEditing()) mouseMoveEvent(event);
}

//-----------------------------------------------------------------------------

// TODO: refactor: cfr functionpanel.cpp
void FunctionSheetCellViewer::openContextMenu(QMouseEvent *e) {
  QAction deleteKeyframeAction(tr("Delete Key"), 0);
  QAction insertKeyframeAction(tr("Set Key"), 0);

  QStringList interpNames;
  interpNames << tr("Constant Interpolation") << tr("Linear Interpolation")
              << tr("Speed In / Speed Out Interpolation")
              << tr("Ease In / Ease Out Interpolation")
              << tr("Ease In / Ease Out (%) Interpolation")
              << tr("Exponential Interpolation")
              << tr("Expression Interpolation") << tr("File Interpolation")
              << tr("Similar Shape Interpolation");
  QAction activateCycleAction(tr("Activate Cycle"), 0);
  QAction deactivateCycleAction(tr("Deactivate Cycle"), 0);
  QAction showIbtwnAction(tr("Show Inbetween Values"), 0);
  QAction hideIbtwnAction(tr("Hide Inbetween Values"), 0);

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

  // if the FunctionSelection is not current or when clicking outside of the
  // selection, then select the clicked cell.
  FunctionSelection *selection = m_sheet->getSelection();
  if (!selection->getSelectedCells().contains(col, row)) {
    selection->makeCurrent();
    selection->selectCells(QRect(col, row, 1, 1));
  }
  CommandManager *cmdManager = CommandManager::instance();

  // build menu
  QMenu menu(0);

  // on clicking after last keyframe
  if (kCount > 0 && isEmpty && kIndex == kCount - 1) {
    if (curve->isCycleEnabled())
      menu.addAction(&deactivateCycleAction);
    else
      menu.addAction(&activateCycleAction);
  }

  if (!isKeyframe)  // menu.addAction(&deleteKeyframeAction); else
    menu.addAction(&insertKeyframeAction);

  // change interpolation commands
  QList<QAction *> interpActions;
  int interp = selection->getCommonSegmentType();
  if (interp != -1) {
    menu.addSeparator();
    QMenu *interpMenu = menu.addMenu(tr("Change Interpolation"));
    for (int i = (int)TDoubleKeyframe::Constant;
         i <= (int)TDoubleKeyframe::SimilarShape; i++) {
      if (interp != i) {
        QAction *interpAction = new QAction(interpNames[i - 1], 0);
        interpAction->setData(i);
        interpActions.append(interpAction);
        interpMenu->addAction(interpAction);
      }
    }
  }

  // change step commands
  int step = selection->getCommonStep();
  if (step != -1) {
    QMenu *stepMenu = menu.addMenu(tr("Change Step"));
    if (step != 1) stepMenu->addAction(cmdManager->getAction("MI_ResetStep"));
    if (step != 2) stepMenu->addAction(cmdManager->getAction("MI_Step2"));
    if (step != 3) stepMenu->addAction(cmdManager->getAction("MI_Step3"));
    if (step != 4) stepMenu->addAction(cmdManager->getAction("MI_Step4"));
  }

  menu.addSeparator();

  menu.addAction(cmdManager->getAction("MI_Cut"));
  menu.addAction(cmdManager->getAction("MI_Copy"));
  menu.addAction(cmdManager->getAction("MI_Paste"));
  menu.addAction(cmdManager->getAction("MI_Clear"));

  menu.addAction(cmdManager->getAction("MI_Insert"));

  if (!isEmpty && kIndex >= 0) {
    menu.addSeparator();
    if (m_sheet->isIbtwnValueVisible())
      menu.addAction(&hideIbtwnAction);
    else
      menu.addAction(&showIbtwnAction);
  }

  TSceneHandle *sceneHandle = m_sheet->getViewer()->getSceneHandle();
  // execute menu
  QAction *action = menu.exec(e->globalPos());  // QCursor::pos());
  if (action == &deleteKeyframeAction) {
    KeyframeSetter::removeKeyframeAt(curve, row);
  } else if (action == &insertKeyframeAction) {
    KeyframeSetter(curve).createKeyframe(row);
  } else if (interpActions.contains(action)) {
    selection->setSegmentType((TDoubleKeyframe::Type)action->data().toInt());
  } else if (action == &activateCycleAction)
    KeyframeSetter::enableCycle(curve, true, sceneHandle);
  else if (action == &deactivateCycleAction)
    KeyframeSetter::enableCycle(curve, false, sceneHandle);
  else if (action == &hideIbtwnAction)
    m_sheet->setIbtwnValueVisible(false);
  else if (action == &showIbtwnAction)
    m_sheet->setIbtwnValueVisible(true);

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

FunctionSheet::FunctionSheet(QWidget *parent, bool isFloating)
    : SpreadsheetViewer(parent)
    , m_selectedCells()
    , m_selection(0)
    , m_isFloating(isFloating)
    , m_buttonArea(nullptr) {
  setColumnsPanel(m_columnHeadViewer = new FunctionSheetColumnHeadViewer(this));
  setRowsPanel(m_rowViewer = new FunctionSheetRowViewer(this));
  setCellsPanel(m_cellViewer = new FunctionSheetCellViewer(this));

  setWindowFlag(Qt::Window);
  setColumnCount(20);
  setWindowTitle(tr("Function Editor"));
  setFocusPolicy(Qt::ClickFocus);

  if (m_isFloating) {
    // load the dialog size
    TFilePath fp(ToonzFolder::getMyModuleDir() + TFilePath("popups.ini"));
    QSettings settings(toQString(fp), QSettings::IniFormat);

    setGeometry(settings.value("FunctionSpreadsheet", QRect(500, 500, 400, 300))
                    .toRect());
  }

  setButtonAreaWidget(m_buttonArea = new FunctionSheetButtonArea(this));
  connect(m_buttonArea, SIGNAL(syncSizeBtnToggled(bool)), this,
          SLOT(onSyncSizeBtnToggled(bool)));
}

//-----------------------------------------------------------------------------

FunctionSheet::~FunctionSheet() {
  if (m_isFloating) {
    TFilePath fp(ToonzFolder::getMyModuleDir() + TFilePath("popups.ini"));
    QSettings settings(toQString(fp), QSettings::IniFormat);

    settings.setValue("FunctionSpreadsheet", geometry());
  }
}

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

  if (m_xshHandle && m_syncSize) {
    connect(m_xshHandle, SIGNAL(zoomScaleChanged()), this,
            SLOT(onZoomScaleChanged()));
    onZoomScaleChanged();
  }
}

//-----------------------------------------------------------------------------

void FunctionSheet::hideEvent(QHideEvent *e) {
  m_frameScroller.unregisterFrameScroller();
  SpreadsheetViewer::hideEvent(e);

  if (m_xshHandle && m_syncSize) {
    disconnect(m_xshHandle, SIGNAL(zoomScaleChanged()), this,
               SLOT(onZoomScaleChanged()));
  }
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
      TDoubleParam *param = 0;
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

//-----------------------------------------------------------------------------
/*! Obtains a pointer to the stage object containing the parameter of specified
 * column
 */
TStageObject *FunctionSheet::getStageObject(int column) {
  FunctionTreeModel::Channel *channel = getChannel(column);
  if (!channel) return nullptr;

  FunctionTreeModel::ChannelGroup *channelGroup = channel->getChannelGroup();
  if (!channelGroup) return nullptr;

  // returns nullptr if the channel is a fx parameter
  StageObjectChannelGroup *stageItem =
      dynamic_cast<StageObjectChannelGroup *>(channelGroup);
  if (!stageItem) return nullptr;

  return stageItem->getStageObject();
}

//-----------------------------------------------------------------------------

void FunctionSheet::setSyncSize(bool on) {
  m_syncSize = on;
  m_buttonArea->setSyncSizeBtnState(on);
  update();
}

//-----------------------------------------------------------------------------

int FunctionSheet::getFrameZoomFactor() const {
  if (m_syncSize && m_xshHandle) {
    int zoomFactor = m_xshHandle->getZoomFactor();
    return 50 + (zoomFactor - 20) * 5 / 8;
  }
  return 100;
}

//-----------------------------------------------------------------------------

void FunctionSheet::onSyncSizeBtnToggled(bool on) {
  // switch the flag
  m_syncSize = on;

  if (m_xshHandle) {
    if (on)
      connect(m_xshHandle, SIGNAL(zoomScaleChanged()), this,
              SLOT(onZoomScaleChanged()));
    else
      disconnect(m_xshHandle, SIGNAL(zoomScaleChanged()), this,
                 SLOT(onZoomScaleChanged()));
    onZoomScaleChanged();
  }
}

//-----------------------------------------------------------------------------

void FunctionSheet::onZoomScaleChanged() {
  QPoint xyOrig = positionToXY(CellPosition(getCurrentFrame(), -1));
  setScaleFactor(getFrameZoomFactor());
  QPoint xyNew = positionToXY(CellPosition(getCurrentFrame(), -1));
  scroll(xyNew - xyOrig);
  update();
}