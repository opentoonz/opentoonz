

#include "xshrowviewer.h"
#include "xsheetviewer.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "xsheetdragtool.h"
#include "toonzqt/gutil.h"
#include "onionskinmaskgui.h"
#include "cellselection.h"
#include "menubarcommandids.h"
#include "toonzqt/menubarcommand.h"

#include "toonz/toonzscene.h"
#include "tconvert.h"

#include "toonz/txsheet.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "toonz/preferences.h"

#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "toonz/tstageobject.h"
#include "toonz/tpinnedrangeset.h"

#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QToolTip>

//=============================================================================

namespace XsheetGUI {

//=============================================================================
// RowArea
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
RowArea::RowArea(XsheetViewer *parent, Qt::WindowFlags flags)
#else
RowArea::RowArea(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QWidget(parent, flags)
    , m_viewer(parent)
    , m_row(-1)
    , m_showOnionToSet(None)
    , m_pos(-1, -1)
    , m_playRangeActiveInMousePress(false)
    , m_mousePressRow(-1)
    , m_tooltip(tr(""))
    , m_r0(0)
    , m_r1(5)
    , m_isPanning(false) {
  setFocusPolicy(Qt::NoFocus);
  setMouseTracking(true);
  connect(TApp::instance()->getCurrentOnionSkin(),
          SIGNAL(onionSkinMaskChanged()), this, SLOT(update()));
  // for displaying the pinned center keys when the skeleton tool is selected
  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
          SLOT(update()));
}

//-----------------------------------------------------------------------------

RowArea::~RowArea() {}

//-----------------------------------------------------------------------------

DragTool *RowArea::getDragTool() const { return m_viewer->getDragTool(); }
void RowArea::setDragTool(DragTool *dragTool) {
  m_viewer->setDragTool(dragTool);
}

//-----------------------------------------------------------------------------

void RowArea::drawRows(QPainter &p, int r0, int r1) {
  int playR0, playR1, step;
  XsheetGUI::getPlayRange(playR0, playR1, step);

  if (!XsheetGUI::isPlayRangeEnabled()) {
    TXsheet *xsh = m_viewer->getXsheet();
    playR1       = xsh->getFrameCount() - 1;
    playR0       = 0;
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
  // set font size in pixel
  font.setPixelSize(XSHEET_FONT_PX_SIZE);

  p.setFont(font);

  // marker interval
  int distance, offset;
  TApp::instance()->getCurrentScene()->getScene()->getProperties()->getMarkers(
      distance, offset);

  // default value
  if (distance == 0) distance = 6;

  QRect visibleRect = visibleRegion().boundingRect();

  int x0                = visibleRect.left();
  int x1                = visibleRect.right();
  int y0                = visibleRect.top();
  int y1                = visibleRect.bottom();
  NumberRange layerSide = m_viewer->orientation()->layerSide(visibleRect);

  for (int r = r0; r <= r1; r++) {
    int frameAxis = m_viewer->rowToFrameAxis(r);

    //--- draw horizontal line
    QColor color = ((r - offset) % distance == 0 && r != 0)
                       ? m_viewer->getMarkerLineColor()
                       : m_viewer->getLightLineColor();
    p.setPen(color);
    QLine horizontalLine =
        m_viewer->orientation()->horizontalLine(frameAxis, layerSide);
    p.drawLine(horizontalLine);

    // draw frame text
    if (playR0 <= r && r <= playR1) {
      p.setPen(((r - m_r0) % step == 0) ? m_viewer->getPreviewFrameTextColor()
                                        : m_viewer->getTextColor());
    }
    // not in preview range
    else
      p.setPen(m_viewer->getTextColor());

    QPoint basePoint = m_viewer->positionToXY(CellPosition(r, 0));
    QRect labelRect  = m_viewer->orientation()
                          ->rect(PredefinedRect::FRAME_LABEL)
                          .translated(basePoint);
    int align = m_viewer->orientation()->dimension(
        PredefinedDimension::FRAME_LABEL_ALIGN);
    // display time and/or frame number
    switch (m_viewer->getFrameDisplayStyle()) {
    case XsheetViewer::SecAndFrame: {
      int frameRate = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getOutputProperties()
                          ->getFrameRate();
      QString str;
      int koma = (r + 1) % frameRate;
      if (koma == 1) {
        int sec = (r + 1) / frameRate;
        str     = QString("%1' %2\"")
                  .arg(QString::number(sec).rightJustified(2, '0'))
                  .arg(QString::number(koma).rightJustified(2, '0'));
      } else {
        if (koma == 0) koma = frameRate;
        str = QString("%1\"").arg(QString::number(koma).rightJustified(2, '0'));
      }

      p.drawText(labelRect, align, str);

      break;
    }

    case XsheetViewer::Frame: {
      QString number = QString::number(r + 1);
      p.drawText(labelRect, align, number);
      break;
    }

    // 6 second sheet (144frames per page)
    case XsheetViewer::SixSecSheet: {
      int frameRate = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getOutputProperties()
                          ->getFrameRate();
      QString str;
      int koma = (r + 1) % (frameRate * 6);
      if ((r + 1) % frameRate == 1) {
        int page = (r + 1) / (frameRate * 6) + 1;
        str      = QString("p%1 %2")
                  .arg(QString::number(page))
                  .arg(QString::number(koma).rightJustified(3, '0'));
      } else {
        if (koma == 0) koma = frameRate * 6;
        str = QString("%1").arg(QString::number(koma).rightJustified(3, '0'));
      }
      p.drawText(labelRect, align, str);
      break;
    }
    // 3 second sheet (72frames per page)
    case XsheetViewer::ThreeSecSheet: {
      int frameRate = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getOutputProperties()
                          ->getFrameRate();
      QString str;
      int koma = (r + 1) % (frameRate * 3);
      if ((r + 1) % frameRate == 1) {
        int page = (r + 1) / (frameRate * 3) + 1;
        str      = QString("p%1 %2")
                  .arg(QString::number(page))
                  .arg(QString::number(koma).rightJustified(2, '0'));
      } else {
        if (koma == 0) koma = frameRate * 3;
        str = QString("%1").arg(QString::number(koma).rightJustified(2, '0'));
      }
      p.drawText(labelRect, align, str);
      break;
    }
    }
  }
}

//-----------------------------------------------------------------------------

void RowArea::drawPlayRange(QPainter &p, int r0, int r1) {
  bool playRangeEnabled = XsheetGUI::isPlayRangeEnabled();

  // Update the play range internal fields
  if (playRangeEnabled) {
    int step;
    XsheetGUI::getPlayRange(m_r0, m_r1, step);
  }
  // if the preview range is not set, then put markers at the first and the last
  // frames of the scene
  else {
    TXsheet *xsh = m_viewer->getXsheet();
    m_r1         = xsh->getFrameCount() - 1;
    if (m_r1 == -1) return;
    m_r0 = 0;
  }

  QColor ArrowColor = (playRangeEnabled) ? QColor(255, 255, 255) : grey150;
  p.setBrush(QBrush(ArrowColor));

  if (m_r0 > r0 - 1 && r1 + 1 > m_r0)
    m_viewer->drawPredefinedPath(p, PredefinedPath::BEGIN_PLAY_RANGE,
                                 CellPosition(m_r0, 0), ArrowColor,
                                 QColor(Qt::black));

  if (m_r1 > r0 - 1 && r1 + 1 > m_r1)
    m_viewer->drawPredefinedPath(p, PredefinedPath::END_PLAY_RANGE,
                                 CellPosition(m_r1, 0), ArrowColor,
                                 QColor(Qt::black));
}

//-----------------------------------------------------------------------------

void RowArea::drawCurrentRowGadget(QPainter &p, int r0, int r1) {
  int currentRow = m_viewer->getCurrentRow();
  if (currentRow < r0 || r1 < currentRow) return;

  QPoint topLeft = m_viewer->positionToXY(CellPosition(currentRow, 0));
  QRect header   = m_viewer->orientation()
                     ->rect(PredefinedRect::FRAME_HEADER)
                     .translated(topLeft)
                     .adjusted(1, 1, 0, 0);
  p.fillRect(header, m_viewer->getCurrentRowBgColor());
}

//-----------------------------------------------------------------------------

void RowArea::drawOnionSkinSelection(QPainter &p) {
  TApp *app            = TApp::instance();
  OnionSkinMask osMask = app->getCurrentOnionSkin()->getOnionSkinMask();

  TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
  assert(xsh);
  int currentRow = m_viewer->getCurrentRow();

  // get onion colors
  TPixel frontPixel, backPixel;
  bool inksOnly;
  Preferences::instance()->getOnionData(frontPixel, backPixel, inksOnly);
  QColor frontColor((int)frontPixel.r, (int)frontPixel.g, (int)frontPixel.b,
                    128);
  QColor backColor((int)backPixel.r, (int)backPixel.g, (int)backPixel.b, 128);

  // If the onion skin is disabled, draw dash line instead.
  if (osMask.isEnabled())
    p.setPen(Qt::red);
  else {
    QPen currentPen = p.pen();
    currentPen.setStyle(Qt::DashLine);
    currentPen.setColor(QColor(128, 128, 128, 255));
    p.setPen(currentPen);
  }

  QRect onionRect = m_viewer->orientation()->rect(PredefinedRect::ONION);
  int onionCenter_frame =
      m_viewer->orientation()->frameSide(onionRect).middle();
  int onionCenter_layer =
      m_viewer->orientation()->layerSide(onionRect).middle();

  //-- draw movable onions

  // draw line between onion skin range
  int minMos   = 0;
  int maxMos   = 0;
  int mosCount = osMask.getMosCount();
  for (int i = 0; i < mosCount; i++) {
    int mos                  = osMask.getMos(i);
    if (minMos > mos) minMos = mos;
    if (maxMos < mos) maxMos = mos;
  }
  p.setBrush(Qt::NoBrush);
  if (minMos < 0)  // previous frames
  {
    int layerAxis = onionCenter_layer;
    int fromFrameAxis =
        m_viewer->rowToFrameAxis(currentRow + minMos) + onionCenter_frame;
    int toFrameAxis = m_viewer->rowToFrameAxis(currentRow) + onionCenter_frame;
    QLine verticalLine = m_viewer->orientation()->verticalLine(
        layerAxis, NumberRange(fromFrameAxis, toFrameAxis));
    if (m_viewer->orientation()->isVerticalTimeline())
      p.drawLine(verticalLine.x1() + 1, verticalLine.y1() + 4,
                 verticalLine.x2() + 1, verticalLine.y2() - 10);
    else
      p.drawLine(verticalLine.x1() + 4, verticalLine.y1() + 1,
                 verticalLine.x2() - 10, verticalLine.y2() + 1);
  }
  if (maxMos > 0)  // forward frames
  {
    int layerAxis = onionCenter_layer;
    int fromFrameAxis =
        m_viewer->rowToFrameAxis(currentRow) + onionCenter_frame;
    int toFrameAxis =
        m_viewer->rowToFrameAxis(currentRow + maxMos) + onionCenter_frame;
    QLine verticalLine = m_viewer->orientation()->verticalLine(
        layerAxis, NumberRange(fromFrameAxis, toFrameAxis));
    if (m_viewer->orientation()->isVerticalTimeline())
      p.drawLine(verticalLine.x1() + 1, verticalLine.y1() + 10,
                 verticalLine.x2() + 1, verticalLine.y2() - 4);
    else
      p.drawLine(verticalLine.x1() + 10, verticalLine.y1() + 1,
                 verticalLine.x2() - 4, verticalLine.y2() + 1);
  }

  // Draw onion skin main handle
  QPoint handleTopLeft = m_viewer->positionToXY(CellPosition(currentRow, 0));
  QRect handleRect     = onionRect.translated(handleTopLeft);
  int angle180         = 16 * 180;
  int turn =
      m_viewer->orientation()->dimension(PredefinedDimension::ONION_TURN) * 16;
  p.setBrush(QBrush(backColor));
  p.drawChord(handleRect, turn, angle180);
  p.setBrush(QBrush(frontColor));
  p.drawChord(handleRect, turn + angle180, angle180);

  // draw onion skin dots
  p.setPen(Qt::red);
  for (int i = 0; i < mosCount; i++) {
    // mos : frame offset from the current frame
    int mos = osMask.getMos(i);
    // skip drawing if the frame is under the mouse cursor
    if (m_showOnionToSet == Mos && currentRow + mos == m_row) continue;

    if (osMask.isEnabled())
      p.setBrush(mos < 0 ? backColor : frontColor);
    else
      p.setBrush(Qt::NoBrush);
    QPoint topLeft = m_viewer->positionToXY(CellPosition(currentRow + mos, 0));
    QRect dotRect  = m_viewer->orientation()
                        ->rect(PredefinedRect::ONION_DOT)
                        .translated(topLeft);
    p.drawEllipse(dotRect);
  }

  //-- draw fixed onions
  for (int i = 0; i < osMask.getFosCount(); i++) {
    int fos = osMask.getFos(i);
    if (fos == currentRow) continue;
    // skip drawing if the frame is under the mouse cursor
    if (m_showOnionToSet == Fos && fos == m_row) continue;

    if (osMask.isEnabled())
      p.setBrush(QBrush(QColor(0, 255, 255, 128)));
    else
      p.setBrush(Qt::NoBrush);
    QPoint topLeft = m_viewer->positionToXY(CellPosition(fos, 0));
    QRect dotRect  = m_viewer->orientation()
                        ->rect(PredefinedRect::ONION_DOT_FIXED)
                        .translated(topLeft);
    p.drawEllipse(dotRect);
  }

  //-- onion placement hint under mouse
  if (m_showOnionToSet != None) {
    p.setPen(QColor(255, 128, 0));
    p.setBrush(QBrush(QColor(255, 255, 0)));
    QPoint topLeft = m_viewer->positionToXY(CellPosition(m_row, 0));
    QRect dotRect =
        m_viewer->orientation()
            ->rect(m_showOnionToSet == Fos ? PredefinedRect::ONION_DOT_FIXED
                                           : PredefinedRect::ONION_DOT)
            .translated(topLeft);
    p.drawEllipse(dotRect);
  }
}

//-----------------------------------------------------------------------------

namespace {

TStageObjectId getAncestor(TXsheet *xsh, TStageObjectId id) {
  assert(id.isColumn());
  TStageObjectId parentId;
  while (parentId = xsh->getStageObjectParent(id), parentId.isColumn())
    id = parentId;
  return id;
}

int getPinnedColumnId(int row, TXsheet *xsh, TStageObjectId ancestorId,
                      int columnCount) {
  int tmp_pinnedCol = -1;
  for (int c = 0; c < columnCount; c++) {
    TStageObjectId columnId(TStageObjectId::ColumnId(c));
    if (getAncestor(xsh, columnId) != ancestorId) continue;
    TStageObject *obj = xsh->getStageObject(columnId);

    if (obj->getPinnedRangeSet()->isPinned(row)) {
      tmp_pinnedCol = c;
      break;
    }
  }
  return tmp_pinnedCol;
}

}  // namespace

void RowArea::drawPinnedCenterKeys(QPainter &p, int r0, int r1) {
  // std::cout << "Skeleton Tool activated" << std::endl;
  int col      = m_viewer->getCurrentColumn();
  TXsheet *xsh = m_viewer->getXsheet();
  if (col < 0 || !xsh || xsh->isColumnEmpty(col)) return;

  TStageObjectId ancestorId = getAncestor(xsh, TStageObjectId::ColumnId(col));

  int columnCount    = xsh->getColumnCount();
  int prev_pinnedCol = -2;

  QRect keyRect =
      m_viewer->orientation()->rect(PredefinedRect::PINNED_CENTER_KEY);
  p.setPen(Qt::black);

  r1 = (r1 < xsh->getFrameCount() - 1) ? xsh->getFrameCount() - 1 : r1;

  for (int r = r0 - 1; r <= r1; r++) {
    if (r < 0) continue;

    int tmp_pinnedCol = getPinnedColumnId(r, xsh, ancestorId, columnCount);

    // Pinned Column is changed at this row
    if (tmp_pinnedCol != prev_pinnedCol) {
      prev_pinnedCol = tmp_pinnedCol;
      if (r != r0 - 1) {
        QPoint mouseInCell = m_pos - m_viewer->positionToXY(CellPosition(r, 0));
        if (keyRect.contains(mouseInCell))
          p.setBrush(QColor(30, 210, 255));
        else
          p.setBrush(QColor(0, 175, 255));

        QPoint topLeft = m_viewer->positionToXY(CellPosition(r, 0));
        QRect adjusted = keyRect.translated(topLeft);
        p.drawRect(adjusted);

        QFont font = p.font();
        font.setPixelSize(8);
        font.setBold(false);
        p.setFont(font);
        p.drawText(
            adjusted, Qt::AlignCenter,
            QString::number((tmp_pinnedCol == -1) ? ancestorId.getIndex() + 1
                                                  : tmp_pinnedCol + 1));
      }
    }
  }
}

//-----------------------------------------------------------------------------

void RowArea::paintEvent(QPaintEvent *event) {
  QRect toBeUpdated = event->rect();

  QPainter p(this);

  CellRange cellRange = m_viewer->xyRectToRange(toBeUpdated);
  int r0, r1;  // range of visible rows
  r0 = cellRange.from().frame();
  r1 = cellRange.to().frame();

  p.setClipRect(toBeUpdated);

  // fill background
  p.fillRect(toBeUpdated, m_viewer->getBGColor());

  if (TApp::instance()->getCurrentFrame()->isEditingScene())
    // current frame
    drawCurrentRowGadget(p, r0, r1);

  drawRows(p, r0, r1);

  if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
      Preferences::instance()->isOnionSkinEnabled())
    drawOnionSkinSelection(p);

  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Skeleton)
    drawPinnedCenterKeys(p, r0, r1);

  drawPlayRange(p, r0, r1);
  p.setPen(m_viewer->getVerticalLineColor());
  p.setBrush(Qt::NoBrush);
  if (m_viewer->orientation()->isVerticalTimeline())
    p.drawRect(toBeUpdated.adjusted(0, -1, -1, 0));
  else
    p.drawRect(toBeUpdated.adjusted(-1, 0, 0, -1));
}

//-----------------------------------------------------------------------------

void RowArea::mousePressEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();

  m_viewer->setQtModifiers(event->modifiers());
  if (event->button() == Qt::LeftButton) {
    bool frameAreaIsClicked = false;

    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
    TPoint pos(event->pos().x(), event->pos().y());
    int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();

    int row            = m_viewer->xyToPosition(pos).frame();
    QPoint topLeft     = m_viewer->positionToXY(CellPosition(row, 0));
    QPoint mouseInCell = event->pos() - topLeft;

    if (Preferences::instance()->isOnionSkinEnabled() &&
        o->rect(PredefinedRect::ONION_AREA).contains(mouseInCell)) {
      if (row == currentFrame) {
        setDragTool(
            XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
        frameAreaIsClicked = true;
      } else if (o->rect(PredefinedRect::ONION_FIXED_DOT_AREA)
                     .contains(mouseInCell))
        setDragTool(XsheetGUI::DragTool::makeKeyOnionSkinMaskModifierTool(
            m_viewer, true));
      else if (o->rect(PredefinedRect::ONION_DOT_AREA).contains(mouseInCell))
        setDragTool(XsheetGUI::DragTool::makeKeyOnionSkinMaskModifierTool(
            m_viewer, false));
      else {
        setDragTool(
            XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
        frameAreaIsClicked = true;
      }
    } else {
      int playR0, playR1, step;
      XsheetGUI::getPlayRange(playR0, playR1, step);

      bool playRangeEnabled = playR0 <= playR1;
      if (!playRangeEnabled) {
        TXsheet *xsh = m_viewer->getXsheet();
        playR1       = xsh->getFrameCount() - 1;
        playR0       = 0;
      }

      if (playR1 == -1) {  // getFrameCount = 0 i.e. xsheet is empty
        setDragTool(
            XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
        frameAreaIsClicked = true;
      } else if (o->rect(PredefinedRect::PLAY_RANGE).contains(mouseInCell) &&
                 (row == playR0 || row == playR1)) {
        if (!playRangeEnabled) XsheetGUI::setPlayRange(playR0, playR1, step);
        setDragTool(XsheetGUI::DragTool::makePlayRangeModifierTool(m_viewer));
      } else {
        setDragTool(
            XsheetGUI::DragTool::makeCurrentFrameModifierTool(m_viewer));
        frameAreaIsClicked = true;
      }
    }
    // when shift+click the row area, select a single row region in the cell
    // area
    if (frameAreaIsClicked && 0 != (event->modifiers() & Qt::ShiftModifier)) {
      int filledCol;
      for (filledCol = xsh->getColumnCount() - 1; filledCol >= 0; filledCol--) {
        TXshColumn *currentColumn = xsh->getColumn(filledCol);
        if (!currentColumn) continue;
        if (!currentColumn->isEmpty()) break;
      }

      m_viewer->getCellSelection()->selectNone();
      m_viewer->getCellSelection()->selectCells(row, 0, row,
                                                std::max(0, filledCol));
      m_viewer->updateCellRowAree();
    }

    m_viewer->dragToolClick(event);
    event->accept();
  }  // left-click
  // pan by middle-drag
  else if (event->button() == Qt::MidButton) {
    m_pos       = event->pos();
    m_isPanning = true;
  }
}

//-----------------------------------------------------------------------------

void RowArea::mouseMoveEvent(QMouseEvent *event) {
  const Orientation *o = m_viewer->orientation();
  m_viewer->setQtModifiers(event->modifiers());
  QPoint pos = event->pos();

  // pan by middle-drag
  if (m_isPanning) {
    QPoint delta = m_pos - pos;
    if (o->isVerticalTimeline())
      delta.setX(0);
    else
      delta.setY(0);
    m_viewer->scroll(delta);
    return;
  }

  m_row = m_viewer->xyToPosition(pos).frame();
  int x = pos.x();

  if ((event->buttons() & Qt::LeftButton) != 0 &&
      !visibleRegion().contains(pos)) {
    QRect bounds = visibleRegion().boundingRect();
    if (o->isVerticalTimeline())
      m_viewer->setAutoPanSpeed(bounds, QPoint(bounds.left(), pos.y()));
    else
      m_viewer->setAutoPanSpeed(bounds, QPoint(pos.x(), bounds.top()));
  } else
    m_viewer->stopAutoPan();

  m_pos = pos;

  m_viewer->dragToolDrag(event);

  m_showOnionToSet = None;

  if (getDragTool()) return;

  int currentRow = TApp::instance()->getCurrentFrame()->getFrame();
  int row        = m_viewer->xyToPosition(m_pos).frame();
  QPoint mouseInCell =
      event->pos() - m_viewer->positionToXY(CellPosition(row, 0));
  if (row < 0) return;
  // whether to show ability to set onion marks
  if (Preferences::instance()->isOnionSkinEnabled() && row != currentRow) {
    if (o->rect(PredefinedRect::ONION_FIXED_DOT_AREA).contains(mouseInCell))
      m_showOnionToSet = Fos;
    else if (o->rect(PredefinedRect::ONION_DOT_AREA).contains(mouseInCell))
      m_showOnionToSet = Mos;
  }

  /*- For showing the pinned center keys -*/
  bool isOnPinnedCenterKey = false;
  bool isRootBonePinned;
  int pinnedCenterColumnId = -1;
  if (TApp::instance()->getCurrentTool()->getTool()->getName() == T_Skeleton &&
      o->rect(PredefinedRect::PINNED_CENTER_KEY).contains(mouseInCell)) {
    int col      = m_viewer->getCurrentColumn();
    TXsheet *xsh = m_viewer->getXsheet();
    if (col >= 0 && xsh && !xsh->isColumnEmpty(col)) {
      TStageObjectId ancestorId =
          getAncestor(xsh, TStageObjectId::ColumnId(col));
      int columnCount = xsh->getColumnCount();
      /*- Check if the current row is the pinned center key-*/
      int prev_pinnedCol =
          (row == 0) ? -2
                     : getPinnedColumnId(row - 1, xsh, ancestorId, columnCount);
      int pinnedCol = getPinnedColumnId(row, xsh, ancestorId, columnCount);
      if (pinnedCol != prev_pinnedCol) {
        isOnPinnedCenterKey = true;
        isRootBonePinned    = (pinnedCol == -1);
        pinnedCenterColumnId =
            (isRootBonePinned) ? ancestorId.getIndex() : pinnedCol;
      }
    }
  }

  update();

  QPoint base0 = m_viewer->positionToXY(CellPosition(m_r0, 0));
  QPoint base1 = m_viewer->positionToXY(CellPosition(m_r1, 0));
  QPainterPath startArrow =
      o->path(PredefinedPath::BEGIN_PLAY_RANGE).translated(base0);
  QPainterPath endArrow =
      o->path(PredefinedPath::END_PLAY_RANGE).translated(base1);

  if (startArrow.contains(m_pos))
    m_tooltip = tr("Playback Start Marker");
  else if (endArrow.contains(m_pos))
    m_tooltip = tr("Playback End Marker");
  else if (isOnPinnedCenterKey)
    m_tooltip = tr("Pinned Center : Col%1%2")
                    .arg(pinnedCenterColumnId + 1)
                    .arg((isRootBonePinned) ? " (Root)" : "");
  else if (row == currentRow) {
    if (Preferences::instance()->isOnionSkinEnabled() &&
        o->rect(PredefinedRect::ONION).contains(mouseInCell))
      m_tooltip = tr("Double Click to Toggle Onion Skin");
    else
      m_tooltip = tr("Current Frame");
  } else if (m_showOnionToSet == Fos)
    m_tooltip = tr("Fixed Onion Skin Toggle");
  else if (m_showOnionToSet == Mos)
    m_tooltip = tr("Relative Onion Skin Toggle");
  else
    m_tooltip = tr("");
}

//-----------------------------------------------------------------------------

void RowArea::mouseReleaseEvent(QMouseEvent *event) {
  m_viewer->setQtModifiers(0);
  m_viewer->stopAutoPan();
  m_isPanning = false;
  m_viewer->dragToolRelease(event);

  TPoint pos(event->pos().x(), event->pos().y());

  int row = m_viewer->xyToPosition(pos).frame();
  if (m_playRangeActiveInMousePress && row == m_mousePressRow &&
      (13 <= pos.x && pos.x <= 26 && (row == m_r0 || row == m_r1)))
    onRemoveMarkers();
}

//-----------------------------------------------------------------------------

void RowArea::contextMenuEvent(QContextMenuEvent *event) {
  OnionSkinMask osMask =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();

  QMenu *menu             = new QMenu(this);
  QAction *setStartMarker = menu->addAction(tr("Set Start Marker"));
  connect(setStartMarker, SIGNAL(triggered()), SLOT(onSetStartMarker()));
  QAction *setStopMarker = menu->addAction(tr("Set Stop Marker"));
  connect(setStopMarker, SIGNAL(triggered()), SLOT(onSetStopMarker()));

  QAction *setAutoMarkers = menu->addAction(tr("Set Auto Markers"));
  connect(setAutoMarkers, SIGNAL(triggered()), SLOT(onSetAutoMarkers()));
  setAutoMarkers->setEnabled(canSetAutoMarkers());

  QAction *removeMarkers = menu->addAction(tr("Remove Markers"));
  connect(removeMarkers, SIGNAL(triggered()), SLOT(onRemoveMarkers()));

  // set both the from and to markers at the specified row
  QAction *previewThis = menu->addAction(tr("Preview This"));
  connect(previewThis, SIGNAL(triggered()), SLOT(onPreviewThis()));

  menu->addSeparator();

  if (Preferences::instance()->isOnionSkinEnabled()) {
    OnioniSkinMaskGUI::addOnionSkinCommand(menu);
    menu->addSeparator();
  }

  CommandManager *cmdManager = CommandManager::instance();
  menu->addAction(cmdManager->getAction(MI_InsertSceneFrame));
  menu->addAction(cmdManager->getAction(MI_RemoveSceneFrame));
  menu->addAction(cmdManager->getAction(MI_InsertGlobalKeyframe));
  menu->addAction(cmdManager->getAction(MI_RemoveGlobalKeyframe));
  menu->addAction(cmdManager->getAction(MI_DrawingSubForward));
  menu->addAction(cmdManager->getAction(MI_DrawingSubBackward));
  menu->addSeparator();
  menu->addAction(cmdManager->getAction(MI_ShiftTrace));
  menu->addAction(cmdManager->getAction(MI_EditShift));
  menu->addAction(cmdManager->getAction(MI_NoShift));
  menu->addAction(cmdManager->getAction(MI_ResetShift));

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------
// Checks if there is a cell non empty at current row and column to enable the
// auto markers item-menu.
bool RowArea::canSetAutoMarkers() {
  TXshCell cell =
      m_viewer->getXsheet()->getCell(m_row, m_viewer->getCurrentColumn());
  return cell.isEmpty() ? false : true;
}

//-----------------------------------------------------------------------------
int RowArea::getNonEmptyCell(int row, int column, Direction direction) {
  int currentPos = row;
  bool exit      = false;

  while (!exit) {
    TXshCell cell = m_viewer->getXsheet()->getCell(currentPos, column);
    if (cell.isEmpty()) {
      (direction == up) ? currentPos++ : currentPos--;
      exit = true;
    } else
      (direction == up) ? currentPos-- : currentPos++;
  }

  return currentPos;
}

//-----------------------------------------------------------------------------

void RowArea::mouseDoubleClickEvent(QMouseEvent *event) {
  int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  int row          = m_viewer->xyToPosition(event->pos()).frame();
  QPoint mouseInCell =
      event->pos() - m_viewer->positionToXY(CellPosition(row, 0));
  if (TApp::instance()->getCurrentFrame()->isEditingScene() &&
      event->buttons() & Qt::LeftButton &&
      Preferences::instance()->isOnionSkinEnabled() && row == currentFrame &&
      m_viewer->orientation()
          ->rect(PredefinedRect::ONION)
          .contains(mouseInCell)) {
    TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
    OnionSkinMask osm          = osmh->getOnionSkinMask();
    osm.enable(!osm.isEnabled());
    osmh->setOnionSkinMask(osm);
    osmh->notifyOnionSkinMaskChanged();
  }
}

//-----------------------------------------------------------------------------

bool RowArea::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    if (!m_tooltip.isEmpty())
      QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
    else
      QToolTip::hideText();
  } else if (event->type() == QEvent::Leave) {
    m_showOnionToSet = None;
  }
  return QWidget::event(event);
}

//-----------------------------------------------------------------------------

void RowArea::setMarker(int index) {
  assert(m_row >= 0);
  // I use only the step value..
  int unused0, unused1, step;
  getPlayRange(unused0, unused1, step);
  if (m_r0 > m_r1) {
    m_r0 = 0;
    m_r1 = TApp::instance()->getCurrentScene()->getScene()->getFrameCount() - 1;
    if (m_r1 < 1) m_r1 = 1;
  }
  if (index == 0) {
    m_r0                  = m_row;
    if (m_r1 < m_r0) m_r1 = m_r0;
  } else if (index == 1) {
    m_r1                  = m_row;
    if (m_r1 < m_r0) m_r0 = m_r1;
    m_r1 -= (step == 0) ? (m_r1 - m_r0) : (m_r1 - m_r0) % step;
  }
  setPlayRange(m_r0, m_r1, step);
}

//-----------------------------------------------------------------------------

void RowArea::onSetStartMarker() {
  setMarker(0);
  update();
}

//-----------------------------------------------------------------------------

void RowArea::onSetStopMarker() {
  setMarker(1);
  update();
}

//-----------------------------------------------------------------------------
// set both the from and to markers at the specified row
void RowArea::onPreviewThis() {
  assert(m_row >= 0);
  int r0, r1, step;
  getPlayRange(r0, r1, step);
  setPlayRange(m_row, m_row, step);
  update();
}

// Set the playing markers to the continuous block of the cell pointed by
// current row and column
void RowArea::onSetAutoMarkers() {
  int currentColumn = m_viewer->getCurrentColumn();

  int top    = getNonEmptyCell(m_row, currentColumn, Direction::up);
  int bottom = getNonEmptyCell(m_row, currentColumn, Direction::down);

  int r0, r1, step;
  getPlayRange(r0, r1, step);
  setPlayRange(top, bottom, step);
  update();
}

//-----------------------------------------------------------------------------

void RowArea::onRemoveMarkers() {
  int step;
  XsheetGUI::getPlayRange(m_r0, m_r1, step);
  XsheetGUI::setPlayRange(0, -1, step);
  update();
}

//-----------------------------------------------------------------------------

}  // namespace XsheetGUI;
