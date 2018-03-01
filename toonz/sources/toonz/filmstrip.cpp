

#include "filmstrip.h"

// Tnz6 includes
#include "tapp.h"
#include "filmstripcommand.h"
#include "frameheadgadget.h"
#include "floatingpanelcommand.h"
#include "menubarcommandids.h"
#include "filmstripselection.h"
#include "onionskinmaskgui.h"
#include "comboviewerpane.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/trepetitionguard.h"
#include "toonzqt/gutil.h"
#include "toonzqt/tselectionhandle.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage2.h"
#include "toonz/levelproperties.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tpalette.h"

// Qt includes
#include <QPainter>
#include <QScrollBar>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QApplication>
#include <QMainWindow>
#include <QMimeData>
#include <QDrag>

namespace {
QString fidToFrameNumberWithLetter(int f) {
  QString str = QString::number((int)(f / 10));
  while (str.length() < 3) str.push_front("0");
  switch (f % 10) {
  case 1:
    str.append('A');
    break;
  case 2:
    str.append('B');
    break;
  case 3:
    str.append('C');
    break;
  case 4:
    str.append('D');
    break;
  case 5:
    str.append('E');
    break;
  case 6:
    str.append('F');
    break;
  case 7:
    str.append('G');
    break;
  case 8:
    str.append('H');
    break;
  case 9:
    str.append('I');
    break;
  default:
    str.append(' ');
    break;
  }
  return str;
}
}  // namespace

//=============================================================================
// Filmstrip
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
FilmstripFrames::FilmstripFrames(QScrollArea *parent, Qt::WindowFlags flags)
#else
FilmstripFrames::FilmstripFrames(QScrollArea *parent, Qt::WFlags flags)
#endif
    : QFrame(parent, flags)
    , m_scrollArea(parent)
    , m_selection(new TFilmstripSelection())
    , m_frameHeadGadget(0)
    , m_inbetweenDialog(0)
    , m_pos(0, 0)
    , m_iconSize(dimension2QSize(IconGenerator::instance()->getIconSize()))
    , m_frameLabelWidth(11)
    , m_selectingRange(0, 0)
    , m_scrollSpeed(0)
    , m_dragSelectionStartIndex(-1)
    , m_dragSelectionEndIndex(-1)
    , m_timerId(0)
    , m_isGhibli(false)
    , m_selecting(false)
    , m_dragDropArmed(false)
    , m_readOnly(false) {
  setObjectName("filmStripFrames");
  setFrameStyle(QFrame::StyledPanel);

  setFocusPolicy(Qt::StrongFocus);
  setFixedWidth(m_iconSize.width() + fs_leftMargin + fs_rightMargin +
                fs_iconMarginLR * 2);
  setFixedHeight(parentWidget()->height());

  // la testa mobile che indica il frame corrente (e gestisce la GUI dell'onion
  // skin)
  m_frameHeadGadget = new FilmstripFrameHeadGadget(this);
  installEventFilter(m_frameHeadGadget);

  m_selection->setView(this);
  setMouseTracking(true);

  std::string room =
      Preferences::instance()->getCurrentRoomChoice().toStdString();
  m_isGhibli = room == "StudioGhibli";
  m_viewer   = NULL;
}

//-----------------------------------------------------------------------------

FilmstripFrames::~FilmstripFrames() {
  delete m_selection;
  delete m_frameHeadGadget;
}

//-----------------------------------------------------------------------------

TXshSimpleLevel *FilmstripFrames::getLevel() const {
  TXshLevel *xl = TApp::instance()->getCurrentLevel()->getLevel();
  return xl ? xl->getSimpleLevel() : 0;
}

//-----------------------------------------------------------------------------

int FilmstripFrames::y2index(int y) const {
  const int dy = getIconSize().height() + fs_frameSpacing + fs_iconMarginTop +
                 fs_iconMarginBottom;
  return y / dy;
}

//-----------------------------------------------------------------------------

int FilmstripFrames::index2y(int index) const {
  const int dy = getIconSize().height() + fs_frameSpacing + fs_iconMarginTop +
                 fs_iconMarginBottom;
  return index * dy;
}

//-----------------------------------------------------------------------------

TFrameId FilmstripFrames::index2fid(int index) const {
  TXshSimpleLevel *sl = getLevel();
  if (!sl || index < 0) return TFrameId();
  return sl->index2fid(index);
}

//-----------------------------------------------------------------------------

int FilmstripFrames::fid2index(const TFrameId &fid) const {
  TXshSimpleLevel *sl = getLevel();
  if (!sl) return -1;
  return sl->guessIndex(fid);  // ATTENZIONE: dovrebbe usare fid2index()
}

//-----------------------------------------------------------------------------

int FilmstripFrames::getFramesHeight() const {
  TXshSimpleLevel *level = getLevel();
  int frameCount         = level ? level->getFrameCount() : 1;
  int frameHeight = m_iconSize.height() + fs_frameSpacing + fs_iconMarginTop +
                    fs_iconMarginBottom;
  return frameHeight * (frameCount + 1);
}

//-----------------------------------------------------------------------------

int FilmstripFrames::getOneFrameHeight() {
  return m_iconSize.height() + fs_frameSpacing + fs_iconMarginTop +
         fs_iconMarginBottom;
}

//-----------------------------------------------------------------------------
void FilmstripFrames::updateContentHeight(int minimumHeight) {
  if (minimumHeight < 0)
    minimumHeight   = visibleRegion().boundingRect().bottom();
  int contentHeight = getFramesHeight();
  if (contentHeight < minimumHeight) contentHeight = minimumHeight;
  int parentHeight                                 = parentWidget()->height();
  if (contentHeight < parentHeight) contentHeight  = parentHeight;
  if (contentHeight != height()) setFixedHeight(contentHeight);
}

//-----------------------------------------------------------------------------

void FilmstripFrames::showFrame(int index) {
  TXshSimpleLevel *level = getLevel();

  if (!level->isFid(index2fid(index))) return;
  int y0 = index2y(index);
  int y1 = y0 + m_iconSize.height() + fs_frameSpacing + fs_iconMarginTop +
           fs_iconMarginBottom;
  if (y1 > height()) setFixedHeight(y1);
  m_scrollArea->ensureVisible(0, (y0 + y1) / 2, 50, (y1 - y0) / 2);
}

//-----------------------------------------------------------------------------

void FilmstripFrames::scroll(int dy) {
  QScrollBar *sb = m_scrollArea->verticalScrollBar();
  int sbValue    = sb->value();

  updateContentHeight(getFramesHeight());
  if (sbValue + dy > getFramesHeight()) {
    sb->setValue(getFramesHeight());
    return;
  }
  sb->setValue(sbValue + dy);
}

//---------------------------------------------------------------------------

void FilmstripFrames::mouseDoubleClickEvent(QMouseEvent *event) {
  int index = y2index(event->pos().y());
  select(index, ONLY_SELECT);  // ONLY_SELECT
}

//-----------------------------------------------------------------------------

void FilmstripFrames::select(int index, SelectionMode mode) {
  TXshSimpleLevel *sl = getLevel();
  bool outOfRange     = !sl || index < 0 || index >= sl->getFrameCount();

  TFrameId fid;
  if (!outOfRange) fid = index2fid(index);

  switch (mode) {
  // select one frame only
  case ONLY_SELECT:
    m_selection->selectNone();
    if (!outOfRange) m_selection->select(fid);
    break;
  case SIMPLE_SELECT:
    // Bail out if fid is already selected
    if (!outOfRange && m_selection->isSelected(fid)) return;

    m_selection->selectNone();
    if (!outOfRange) m_selection->select(fid);
    break;

  case SHIFT_SELECT:
    if (outOfRange) return;

    // Bail out if fid is already selected
    if (m_selection->isSelected(fid)) return;

    if (m_selection->isEmpty())
      m_selection->select(fid);
    else {
      TXshSimpleLevel *sl = getLevel();
      if (!sl) return;

      // seleziono il range da fid al piu' vicino frame selezionato (in entrambe
      // le direzioni)
      int frameCount = sl->getFrameCount();

      // calcolo il limite inferiore della selezione
      int ia = index;
      while (ia > 0 && !m_selection->isSelected(sl->index2fid(ia - 1))) ia--;
      if (ia == 0) ia = index;

      // calcolo il limite superiore della selezione
      int ib = index;
      while (ib < frameCount - 1 &&
             !m_selection->isSelected(sl->index2fid(ib + 1)))
        ib++;
      if (ib == frameCount - 1) ib = index;

      // seleziono
      for (int i = ia; i <= ib; i++) m_selection->select(sl->index2fid(i));
    }
    break;

  case CTRL_SELECT:
    if (outOfRange) return;

    m_selection->select(fid, !m_selection->isSelected(fid));
    break;

  case START_DRAG_SELECT:
    m_selection->selectNone();

    if (outOfRange) {
      m_dragSelectionStartIndex = m_dragSelectionEndIndex = -1;

      break;
    }

    m_selection->select(fid);
    m_dragSelectionStartIndex = index;
    break;

  case DRAG_SELECT:
    if (outOfRange || m_dragSelectionStartIndex < 0 ||
        m_dragSelectionEndIndex == index)
      return;

    m_dragSelectionEndIndex = index;

    m_selection->selectNone();

    int ia = m_dragSelectionStartIndex;
    int ib = index;

    if (ia > ib) std::swap(ia, ib);

    for (int i = ia; i <= ib; ++i) m_selection->select(index2fid(i));

    break;
  }

  TObjectHandle *objectHandle = TApp::instance()->getCurrentObject();
  if (objectHandle->isSpline()) objectHandle->setIsSpline(false);

  // Update current selection
  m_selection->makeCurrent();

  TSelectionHandle *selHandle = TApp::instance()->getCurrentSelection();
  selHandle->notifySelectionChanged();
}

//-----------------------------------------------------------------------------

void FilmstripFrames::showEvent(QShowEvent *) {
  TApp *app = TApp::instance();

  // cambiamenti al livello
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(onLevelSwitched(TXshLevel *)));
  connect(levelHandle, SIGNAL(xshLevelChanged()), this, SLOT(onLevelChanged()));
  connect(levelHandle, SIGNAL(xshLevelViewChanged()), this,
          SLOT(onLevelChanged()));

  // al frame corrente
  connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
          SLOT(onFrameSwitched()));
  connect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
          SLOT(update()));

  // iconcine
  connect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
          SLOT(update()));

  // onion skin
  connect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
          SLOT(update()));

  getViewer();
}

//-----------------------------------------------------------------------------

void FilmstripFrames::hideEvent(QHideEvent *) {
  TApp *app = TApp::instance();

  // cambiamenti al livello
  disconnect(app->getCurrentLevel(), 0, this, 0);

  // al frame corrente
  disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(onFrameSwitched()));
  disconnect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
             SLOT(update()));

  // iconcine
  disconnect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
             SLOT(update()));

  // onion skin
  disconnect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
             SLOT(update()));

  if (m_viewer) {
    disconnect(m_viewer, SIGNAL(onZoomChanged()), this, SLOT(update()));
    disconnect(m_viewer, SIGNAL(refreshNavi()), this, SLOT(update()));
  }
}

//-----------------------------------------------------------------------------

void FilmstripFrames::getViewer() {
  bool viewerChanged = false;
  if (m_viewer != TApp::instance()->getActiveViewer()) {
    if (m_viewer) {
      disconnect(m_viewer, SIGNAL(onZoomChanged()), this, SLOT(update()));
      disconnect(m_viewer, SIGNAL(refreshNavi()), this, SLOT(update()));
    }
    viewerChanged = true;
  }
  ComboViewerPanel *inknPaintViewerPanel =
      TApp::instance()->getInknPaintViewerPanel();
  if (m_isGhibli && inknPaintViewerPanel) {
    m_viewer = inknPaintViewerPanel->getSceneViewer();
  } else {
    m_viewer = TApp::instance()->getActiveViewer();
  }
  if (m_viewer && viewerChanged) {
    connect(m_viewer, SIGNAL(onZoomChanged()), this, SLOT(update()));
    connect(m_viewer, SIGNAL(refreshNavi()), this, SLOT(update()));
  }
}

//-----------------------------------------------------------------------------

void FilmstripFrames::paintEvent(QPaintEvent *evt) {
  QPainter p(this);

  // p.setRenderHint(QPainter::Antialiasing, true);

  QRect clipRect = evt->rect();

  p.fillRect(clipRect, Qt::black);
  // thumbnail rect, including offsets
  QRect iconImgRect = QRect(QPoint(fs_leftMargin + fs_iconMarginLR,
                                   fs_frameSpacing / 2 + fs_iconMarginTop),
                            m_iconSize);
  // frame size with margins
  QSize frameSize = m_iconSize + QSize(fs_iconMarginLR * 2,
                                       fs_iconMarginTop + fs_iconMarginBottom);
  //  .. and with offset
  QRect frameRect =
      QRect(QPoint(fs_leftMargin, fs_frameSpacing / 2), frameSize);

  int oneFrameHeight = frameSize.height() + fs_frameSpacing;

  // visible frame index range
  int i0 = y2index(clipRect.top());
  int i1 = y2index(clipRect.bottom());

  // fids, frameCount <- frames del livello
  std::vector<TFrameId> fids;
  TXshSimpleLevel *sl = getLevel();
  if (sl)
    sl->getFids(fids);
  else {
    for (int i = i0; i <= i1; i++) {
      // draw white rectangles if obtaining the level is failed
      QRect iconRect = frameRect.translated(QPoint(0, oneFrameHeight * i));
      p.setBrush(QColor(192, 192, 192));
      p.setPen(Qt::NoPen);
      p.drawRect(iconRect);
    }
    return;
  }

  //--- compute navigator rect ---

  QRect naviRect;

  if ((sl->getType() == TZP_XSHLEVEL || sl->getType() == OVL_XSHLEVEL) &&
      m_viewer && m_viewer->isVisible()) {
    // imgSize: image's pixel size
    QSize imgSize(sl->getProperties()->getImageRes().lx,
                  sl->getProperties()->getImageRes().ly);
    // Viewer affine
    TAffine viewerAff = m_viewer->getViewMatrix();
    // pixel size which will be displayed with 100% scale in Viewer Stage
    TFrameId currentId = TApp::instance()->getCurrentFrame()->getFid();
    double imgPixelWidth =
        (double)(imgSize.width()) / sl->getDpi(currentId).x * Stage::inch;
    double imgPixelHeight =
        (double)(imgSize.height()) / sl->getDpi(currentId).y * Stage::inch;

    // get the image's corner positions in viewer matrix (with current zoom
    // scale)
    TPointD imgTopRight =
        viewerAff * TPointD(imgPixelWidth / 2.0f, imgPixelHeight / 2.0f);
    TPointD imgBottomLeft =
        viewerAff * TPointD(-imgPixelWidth / 2.0f, -imgPixelHeight / 2.0f);

    // pixel size in viewer matrix ( with current zoom scale )
    QSizeF imgSizeInViewer(imgTopRight.x - imgBottomLeft.x,
                           imgTopRight.y - imgBottomLeft.y);

    // ratio of the Viewer frame's position and size
    QRectF naviRatio(
        (-(float)m_viewer->width() * 0.5f - (float)imgBottomLeft.x) /
            imgSizeInViewer.width(),
        1.0f -
            ((float)m_viewer->height() * 0.5f - (float)imgBottomLeft.y) /
                imgSizeInViewer.height(),
        (float)m_viewer->width() / imgSizeInViewer.width(),
        (float)m_viewer->height() / imgSizeInViewer.height());

    naviRect = QRect(iconImgRect.left() +
                         (int)(naviRatio.left() * (float)iconImgRect.width()),
                     iconImgRect.top() +
                         (int)(naviRatio.top() * (float)iconImgRect.height()),
                     (int)((float)iconImgRect.width() * naviRatio.width()),
                     (int)((float)iconImgRect.height() * naviRatio.height()));
    // for drag move
    m_naviRectPos = naviRect.center();

    naviRect = naviRect.intersected(frameRect);

    m_icon2ViewerRatio.setX(imgSizeInViewer.width() /
                            (float)iconImgRect.width());
    m_icon2ViewerRatio.setY(imgSizeInViewer.height() /
                            (float)iconImgRect.height());
  }

  //--- compute navigator rect end ---

  int frameCount = (int)fids.size();

  std::set<TFrameId> editableFrameRange;

  if (sl) editableFrameRange = sl->getEditableRange();

  bool isReadOnly    = false;
  if (sl) isReadOnly = sl->isReadOnly();

  int i;
  int iconWidth   = m_iconSize.width();
  int x0          = m_frameLabelWidth;
  int x1          = x0 + iconWidth;
  int frameHeight = m_iconSize.height();

  // linee orizzontali che separano i frames
  p.setPen(getLightLineColor());
  for (i = i0; i <= i1; i++) {
    int y = index2y(i) + frameHeight;
    p.drawLine(0, y, x1, y);
  }

  TFilmstripSelection::InbetweenRange range = m_selection->getInbetweenRange();

  // draw for each frames
  for (i = i0; i <= i1; i++) {
    QRect tmp_iconImgRect =
        iconImgRect.translated(QPoint(0, oneFrameHeight * i));
    QRect tmp_frameRect = frameRect.translated(QPoint(0, oneFrameHeight * i));
    bool isCurrentFrame =
        (i == sl->fid2index(TApp::instance()->getCurrentFrame()->getFid()));
    bool isSelected =
        (0 <= i && i < frameCount && m_selection->isSelected(fids[i]));
    TFrameId fid;
    if (0 <= i && i < frameCount) {
      fid = fids[i];
      // normal or inbetween (for vector levels)
      int flags = (sl->getType() == PLI_XSHLEVEL && range.first < fid &&
                   fid < range.second)
                      ? F_INBETWEEN_RANGE
                      : F_NORMAL;

      // draw icons
      drawFrameIcon(p, tmp_iconImgRect, i, fid, flags);

      p.setPen(Qt::NoPen);
      p.setBrush(Qt::NoBrush);
      p.drawRect(tmp_iconImgRect);

      // Frame number
      if (m_selection->isSelected(fids[i])) {
        if (TApp::instance()->getCurrentFrame()->isEditingLevel() &&
            isCurrentFrame)
          p.setPen(Qt::red);
        else
          p.setPen(Qt::white);
      } else
        p.setPen(QColor(192, 192, 192));

      p.setBrush(Qt::NoBrush);
      // for single frame
      QString text;
      if (fid.getNumber() == TFrameId::EMPTY_FRAME ||
          fid.getNumber() == TFrameId::NO_FRAME) {
        text = QString("Single Frame");
      }
      // for sequencial frame (with letter)
      else if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
        text = fidToFrameNumberWithLetter(fid.getNumber());
      }
      // for sequencial frame
      else {
        char letter = fid.getLetter();
        text        = QString::number(fid.getNumber()).rightJustified(4, '0') +
               (letter != '\0' ? QString(letter) : "");
      }
      p.drawText(tmp_frameRect.adjusted(0, 0, -3, 2), text,
                 QTextOption(Qt::AlignRight | Qt::AlignBottom));
      p.setPen(Qt::NoPen);

      // Read-only frames (lock)
      if (0 <= i && i < frameCount) {
        if ((editableFrameRange.empty() && isReadOnly) ||
            (isReadOnly && (!editableFrameRange.empty() &&
                            editableFrameRange.count(fids[i]) == 0))) {
          static QPixmap lockPixmap(":Resources/forbidden.png");
          p.drawPixmap(tmp_frameRect.bottomLeft() + QPoint(3, -13), lockPixmap);
        }
      }
    }

    // navigator rect
    if (naviRect.isValid() && fid >= 0 && fid == getCurrentFrameId()) {
      p.setPen(QPen(Qt::red, 1));
      p.drawRect(naviRect.translated(0, oneFrameHeight * i));
      p.setPen(Qt::NoPen);
    }

    // red frame for the current frame
    if (TApp::instance()->getCurrentFrame()->isEditingLevel() &&
        (isCurrentFrame || isSelected)) {
      QPen pen;
      pen.setColor(Qt::red);
      pen.setWidth(2);
      pen.setJoinStyle(Qt::RoundJoin);
      p.setPen(pen);

      p.drawRect(tmp_frameRect.adjusted(-1, -1, 2, 2));
      p.setPen(Qt::NoPen);
    }
  }

  // se sono in modalita' level edit faccio vedere la freccia che indica il
  // frame corrente
  if (TApp::instance()->getCurrentFrame()->isEditingLevel())
    m_frameHeadGadget->draw(p, QColor(Qt::white), QColor(Qt::black));
}

//-----------------------------------------------------------------------------

void FilmstripFrames::drawFrameIcon(QPainter &p, const QRect &r, int index,
                                    const TFrameId &fid, int flags) {
  QPixmap pm;
  TXshSimpleLevel *sl = getLevel();
  if (sl) {
    pm = IconGenerator::instance()->getIcon(sl, fid);
  }
  if (!pm.isNull()) {
    p.drawPixmap(r.left(), r.top(), pm);

    if (sl && sl->getType() == PLI_XSHLEVEL && flags & F_INBETWEEN_RANGE) {
      int x1 = r.right();
      int x0 = x1 - 12;
      int y0 = r.top();
      int y1 = r.bottom();
      p.fillRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, QColor(180, 180, 180, 255));
      p.setPen(Qt::black);
      p.drawLine(x0 - 1, y0, x0 - 1, y1);

      QPixmap inbetweenPixmap(
          svgToPixmap(":Resources/filmstrip_inbetween.svg"));
      p.drawPixmap(
          x0 + 2,
          y1 - inbetweenPixmap.height() / inbetweenPixmap.devicePixelRatio() -
              3,
          inbetweenPixmap);
    }
  } else {
    // non riesco (per qualche ragione) a visualizzare l'icona
    p.fillRect(r, QColor(255, 200, 200));
    p.setPen(Qt::black);
    p.drawText(r, tr("no icon"), QTextOption(Qt::AlignCenter));
  }
}

void FilmstripFrames::enterEvent(QEvent *event) { getViewer(); }

//-----------------------------------------------------------------------------

TFrameId FilmstripFrames::getCurrentFrameId() {
  TApp *app        = TApp::instance();
  TFrameHandle *fh = app->getCurrentFrame();
  TFrameId currFid;
  if (fh->isEditingLevel())
    currFid = fh->getFid();
  else {
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int col      = app->getCurrentColumn()->getColumnIndex();
    int row      = fh->getFrame();
    if (row < 0 || col < 0) return TFrameId();
    TXshCell cell = xsh->getCell(row, col);
    // if (cell.isEmpty()) return;
    currFid = cell.getFrameId();
  }
  return currFid;
}

//-----------------------------------------------------------------------------

void FilmstripFrames::mousePressEvent(QMouseEvent *event) {
  m_selecting  = false;
  int index    = y2index(event->pos().y());
  TFrameId fid = index2fid(index);

  TXshSimpleLevel *sl = getLevel();
  int i0              = y2index(0);
  int frameHeight = m_iconSize.height() + fs_frameSpacing + fs_iconMarginTop +
                    fs_iconMarginBottom;
  QPoint clickedPos = event->pos() - QPoint(0, (index - i0) * frameHeight);
  bool actualIconClicked =
      QRect(QPoint(fs_leftMargin + fs_iconMarginLR,
                   fs_frameSpacing / 2 +
                       fs_iconMarginTop)  //<- top-left position of the icon
            ,
            m_iconSize)
          .contains(clickedPos);

  if (event->button() == Qt::LeftButton ||
      event->button() == Qt::MiddleButton) {
    // navigator pan
    // make sure the viewer is visible and that a toonz raster or raster level
    // is current
    if (fid.getNumber() >= 0 && fid == getCurrentFrameId() &&
        (sl->getType() == TZP_XSHLEVEL || sl->getType() == OVL_XSHLEVEL) &&
        m_viewer && m_viewer->isVisible() && actualIconClicked &&
        (m_isGhibli != (event->button() == Qt::MiddleButton))) {
      m_isNavigatorPanning = true;
      execNavigatorPan(event->pos());
      QApplication::setOverrideCursor(Qt::ClosedHandCursor);
    } else
      m_isNavigatorPanning = false;
    // end of navigator section

    // return if frame empty or middle button pressed
    if (fid == TFrameId() || event->button() == Qt::MiddleButton) {
      m_justStartedSelection = false;
      return;
    }

    // with shift or control
    if (event->modifiers() & Qt::ShiftModifier) {
      select(index, SHIFT_SELECT);
      if (m_selection->isSelected(fid)) {
        // If the frame is already selected enable
        // drag'n'drop
        m_dragDropArmed = true;
        m_pos           = event->pos();
      }
    } else if (event->modifiers() & Qt::ControlModifier)
      select(index, CTRL_SELECT);

    // was the inbetween button clicked?
    else if (sl->getType() == PLI_XSHLEVEL &&
             m_selection->isInInbetweenRange(fid) &&
             event->pos().x() > width() - 20 - fs_rightMargin) {
      inbetween();
    } else {
      // move current frame when clicked without modifier
      TApp *tapp = TApp::instance();
      std::vector<TFrameId> fids;
      TXshLevel *level = tapp->getCurrentLevel()->getLevel();
      level->getFids(fids);

      tapp->getCurrentFrame()->setFrameIds(fids);
      tapp->getCurrentFrame()->setFid(fid);

      if (actualIconClicked &&
          (!m_selection->isSelected(fid) || m_justStartedSelection)) {
        // click on a non-selected frame
        m_selecting = true;  // allow drag-select
        select(index, START_DRAG_SELECT);
      } else if (m_selection->isSelected(fid)) {
        // if it's already selected - it can be drag and dropped
        m_dragDropArmed = true;
        m_pos           = event->pos();
        // allow a the frame to be reselected if the mouse isn't moved far
        // this is to enable a group selection to become a single selection
        m_allowResetSelection    = true;
        m_indexForResetSelection = index;
      } else if (!actualIconClicked) {
        // this allows clicking the frame number to trigger an instant drag
        select(index, ONLY_SELECT);
        m_dragDropArmed = true;
        m_pos           = event->pos();
      }
    }
    update();
  } else if (event->button() == Qt::RightButton) {
    select(index);
  }
  m_justStartedSelection = false;
}

//-----------------------------------------------------------------------------

void FilmstripFrames::execNavigatorPan(const QPoint &point) {
  int index    = y2index(point.y());
  TFrameId fid = index2fid(index);
  int i0       = y2index(0);

  int frameHeight = m_iconSize.height() + fs_frameSpacing + fs_iconMarginTop +
                    fs_iconMarginBottom;
  QPoint clickedPos = point - QPoint(0, (index - i0) * frameHeight);

  if (fid != getCurrentFrameId()) return;

  QRect iconRect =
      QRect(QPoint(fs_leftMargin + fs_iconMarginLR,
                   fs_frameSpacing / 2 +
                       fs_iconMarginTop)  //<- top-left position of the icon
            ,
            m_iconSize);

  QPointF delta = m_naviRectPos - clickedPos;

  if (iconRect.left() > clickedPos.x() || iconRect.right() < clickedPos.x())
    delta.setX(0.0);
  if (iconRect.top() > clickedPos.y() || iconRect.bottom() < clickedPos.y())
    delta.setY(0.0);
  if (delta.x() == 0.0 && delta.y() == 0.0) return;

  delta.setX(delta.x() * m_icon2ViewerRatio.x());
  delta.setY(delta.y() * m_icon2ViewerRatio.y());

  if (m_viewer) m_viewer->navigatorPan(delta.toPoint());
}

//-----------------------------------------------------------------------------

void FilmstripFrames::mouseReleaseEvent(QMouseEvent *e) {
  stopAutoPanning();
  m_selecting          = false;
  m_dragDropArmed      = false;
  m_isNavigatorPanning = false;
  if (m_allowResetSelection) {
    select(m_indexForResetSelection, ONLY_SELECT);
    update();
  }
  m_allowResetSelection    = false;
  m_indexForResetSelection = -1;
  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------

void FilmstripFrames::mouseMoveEvent(QMouseEvent *e) {
  QPoint pos = e->pos();
  int index  = y2index(e->pos().y());
  if (e->buttons() & Qt::LeftButton || e->buttons() & Qt::MiddleButton) {
    // navigator pan
    if (m_isNavigatorPanning) {
      execNavigatorPan(e->pos());
      e->accept();
      return;
    }
    if (e->buttons() & Qt::MiddleButton) return;
    if (m_dragDropArmed) {
      if ((m_pos - e->pos()).manhattanLength() > 10) {
        startDragDrop();
        m_dragDropArmed       = false;
        m_allowResetSelection = false;
      }
    } else if (m_selecting) {
      m_pos = e->globalPos();
      select(index, DRAG_SELECT);
    }

    // autopan
    int speed = getOneFrameHeight() / 64;

    QRect visibleRect = visibleRegion().boundingRect();
    int visibleTop    = visibleRect.top();
    int visibleBottom = visibleRect.bottom();
    if (pos.y() < visibleRect.top()) {
      m_scrollSpeed = -speed;
      if (visibleRect.top() - pos.y() > 30) {
        m_timerInterval = 50;
      } else if (visibleRect.top() - pos.y() > 15) {
        m_timerInterval = 150;
      } else {
        m_timerInterval = 300;
      }
    } else if (pos.y() > visibleRect.bottom()) {
      m_scrollSpeed = speed;
      if (pos.y() - visibleRect.bottom() > 30) {
        m_timerInterval = 50;
      } else if (pos.y() - visibleRect.bottom() > 15) {
        m_timerInterval = 150;
      } else {
        m_timerInterval = 300;
      }
    } else
      m_scrollSpeed = 0;
    if (m_scrollSpeed != 0) {
      startAutoPanning();
    } else
      stopAutoPanning();
    update();
  } else if (e->buttons() & Qt::MidButton) {
    // scroll con il tasto centrale
    pos = e->globalPos();
    scroll((m_pos.y() - pos.y()) * 10);
    m_pos = pos;
  }
}

//-----------------------------------------------------------------------------

void FilmstripFrames::keyPressEvent(QKeyEvent *event) {
  TFrameHandle *fh       = TApp::instance()->getCurrentFrame();
  TXshSimpleLevel *level = getLevel();
  if (!level) return;
  std::vector<TFrameId> fids;
  level->getFids(fids);
  if (fids.empty()) return;

  // If on a level frame pass the frame id after the last frame to allow
  // creating a new frame with the down arrow key
  TFrameId newId = 0;
  if (Preferences::instance()->getDownArrowLevelStripNewFrame() &&
      fh->getFrameType() == TFrameHandle::LevelFrame) {
    int frameCount = (int)fids.size();
    newId          = index2fid(frameCount);
  }

  fh->setFrameIds(fids);
  if (event->key() == Qt::Key_Up)
    fh->prevFrame();
  else if (event->key() == Qt::Key_Down)
    fh->nextFrame(newId);
  else if (event->key() == Qt::Key_Home)
    fh->firstFrame();
  else if (event->key() == Qt::Key_End)
    fh->lastFrame();
  else if (event->key() == Qt::Key_PageDown) {
    int frameHeight   = m_iconSize.height();
    int visibleHeight = visibleRegion().rects()[0].height();
    int visibleFrames = double(visibleHeight) / double(frameHeight);
    scroll(visibleFrames * frameHeight);
    return;
  } else if (event->key() == Qt::Key_PageUp) {
    int frameHeight   = m_iconSize.height();
    int visibleHeight = visibleRegion().rects()[0].height();
    int visibleFrames = double(visibleHeight) / double(frameHeight);
    scroll(-visibleFrames * frameHeight);
    return;
  } else
    return;

  m_selection->selectNone();
  if (getLevel()) m_selection->select(fh->getFid());
  int index = fid2index(fh->getFid());
  if (index >= 0) showFrame(index);
}

//-----------------------------------------------------------------------------

void FilmstripFrames::wheelEvent(QWheelEvent *event) {
  scroll(-event->delta());
}

//-----------------------------------------------------------------------------

void FilmstripFrames::startAutoPanning() {
  if (m_timerId == 0) m_timerId = startTimer(m_timerInterval);
}

//-----------------------------------------------------------------------------

void FilmstripFrames::stopAutoPanning() {
  if (m_timerId != 0) {
    killTimer(m_timerId);
    m_timerId     = 0;
    m_scrollSpeed = 0;
  }
}

//-----------------------------------------------------------------------------

void FilmstripFrames::timerEvent(QTimerEvent *) {
  scroll(m_scrollSpeed);
  // reset the timer in case m_scroll speed has changed
  killTimer(m_timerId);
  m_timerId = 0;
  m_timerId = startTimer(m_timerInterval);
  if (m_selecting) {
    QPoint pos = mapFromGlobal(m_pos);
    int index  = y2index(pos.y());
    select(index, DRAG_SELECT);
    showFrame(index);
    update();
  }
}

//-----------------------------------------------------------------------------

void FilmstripFrames::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu             = new QMenu();
  TXshSimpleLevel *sl     = getLevel();
  bool isSubsequenceLevel = (sl && sl->isSubsequence());
  bool isReadOnly         = (sl && sl->isReadOnly());
  CommandManager *cm      = CommandManager::instance();

  menu->addAction(cm->getAction(MI_SelectAll));
  menu->addAction(cm->getAction(MI_InvertSelection));
  menu->addSeparator();
  if (!isSubsequenceLevel && !isReadOnly) {
    menu->addAction(cm->getAction(MI_Cut));
  }
  menu->addAction(cm->getAction(MI_Copy));

  if (!isSubsequenceLevel && !isReadOnly) {
    menu->addAction(cm->getAction(MI_Paste));
    menu->addAction(cm->getAction(MI_PasteInto));
    menu->addAction(cm->getAction(MI_Insert));
    menu->addAction(cm->getAction(MI_Clear));
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_Reverse));
    menu->addAction(cm->getAction(MI_Swing));
    menu->addAction(cm->getAction(MI_Step2));
    menu->addAction(cm->getAction(MI_Step3));
    menu->addAction(cm->getAction(MI_Step4));
    menu->addAction(cm->getAction(MI_Each2));
    menu->addAction(cm->getAction(MI_Each3));
    menu->addAction(cm->getAction(MI_Each4));
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_Duplicate));
    menu->addAction(cm->getAction(MI_MergeFrames));
  }
  menu->addAction(cm->getAction(MI_ExposeResource));
  if (!isSubsequenceLevel && !isReadOnly) {
    menu->addAction(cm->getAction(MI_AddFrames));
    menu->addAction(cm->getAction(MI_Renumber));
    if (sl && sl->getType() == TZP_XSHLEVEL)
      menu->addAction(cm->getAction(MI_RevertToCleanedUp));
    if (sl &&
        (sl->getType() == TZP_XSHLEVEL || sl->getType() == PLI_XSHLEVEL ||
         (sl->getType() == OVL_XSHLEVEL && sl->getPath().getType() != "psd")))
      menu->addAction(cm->getAction(MI_RevertToLastSaved));
  }

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void FilmstripFrames::onLevelChanged() {
  updateContentHeight();
  update();
}

//-----------------------------------------------------------------------------

void FilmstripFrames::onLevelSwitched(TXshLevel *) {
  updateContentHeight(0);
  onFrameSwitched();  // deve visualizzare il frame corrente nella levelstrip
}

//-----------------------------------------------------------------------------

void FilmstripFrames::onFrameSwitched() {
  // no. interferische con lo shift-click per la selezione.
  // m_selection->selectNone();
  TApp *app        = TApp::instance();
  TFrameHandle *fh = app->getCurrentFrame();
  TFrameId fid     = getCurrentFrameId();

  int index = fid2index(fid);
  if (index >= 0) {
    showFrame(index);

    TFilmstripSelection *fsSelection =
        dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());

    // don't select if already selected - may be part of a group selection
    if (!m_selection->isSelected(index2fid(index)) && fsSelection) {
      select(index, ONLY_SELECT);
      m_justStartedSelection = true;
    }
  }
  update();
}

//-----------------------------------------------------------------------------

void FilmstripFrames::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  TXshSimpleLevel *sl = getLevel();
  if (!sl) return;
  const std::set<TFrameId> &fids = m_selection->getSelectedFids();
  if (fids.empty()) return;
  QByteArray byteArray;

  QMimeData *mimeData = new QMimeData;
  mimeData->setData("application/vnd.toonz.drawings", byteArray);
  QDrag *drag           = new QDrag(this);
  QPixmap dropThumbnail = IconGenerator::instance()->getIcon(sl, *fids.begin());
  if (!dropThumbnail.isNull()) drag->setPixmap(dropThumbnail);
  drag->setMimeData(mimeData);
  Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
}

//-----------------------------------------------------------------------------

void FilmstripFrames::inbetween() {
  TFilmstripSelection::InbetweenRange range = m_selection->getInbetweenRange();
  if (range.first >= range.second || !getLevel()) return;

  QSettings settings;
  QString keyName("InbetweenInterpolation");
  QString currentItem = settings.value(keyName, tr("Linear")).toString();
  int index;

  {
    if (!m_inbetweenDialog) m_inbetweenDialog = new InbetweenDialog(this);

    // Default -> l'ultimo valore usato

    m_inbetweenDialog->setValue(currentItem);

    int ret = m_inbetweenDialog->exec();
    if (!ret) return;

    currentItem = m_inbetweenDialog->getValue();
    if (currentItem.isEmpty()) return;
    index = m_inbetweenDialog->getIndex(currentItem);
    if (index < 0) return;

    // registro il nuovo valore
  }
  settings.setValue(keyName, currentItem);

  // lo converto nella notazione di FilmstripCmd
  const FilmstripCmd::InbetweenInterpolation codes[] = {
      FilmstripCmd::II_Linear, FilmstripCmd::II_EaseIn,
      FilmstripCmd::II_EaseOut, FilmstripCmd::II_EaseInOut};

  FilmstripCmd::InbetweenInterpolation interpolation = codes[index];

  // inbetween
  FilmstripCmd::inbetween(getLevel(), range.first, range.second, interpolation);
}

//=============================================================================
// Filmstrip
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
Filmstrip::Filmstrip(QWidget *parent, Qt::WindowFlags flags)
#else
Filmstrip::Filmstrip(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent) {
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  m_frameArea        = new QScrollArea(this);
  m_chooseLevelCombo = new QComboBox(this);
  m_frames           = new FilmstripFrames(m_frameArea);

  //----

  m_frameArea->setObjectName("filmScrollArea");
  m_frameArea->setFrameStyle(QFrame::StyledPanel);
  m_frameArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_frameArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_frameArea->verticalScrollBar()->setObjectName("LevelStripScrollBar");
  m_frameArea->setWidget(m_frames);

  m_chooseLevelCombo->setMaxVisibleItems(50);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(3);
  {
    mainLayout->addWidget(m_chooseLevelCombo, 0);
    mainLayout->addWidget(m_frameArea, 1);
  }
  setLayout(mainLayout);

  setFocusProxy(m_frames);

  onLevelSwitched(0);

  // signal-slot connections
  // switch the current level when the current index of m_chooseLevelCombo is
  // changed
  connect(m_chooseLevelCombo, SIGNAL(activated(int)), this,
          SLOT(onChooseLevelComboChanged(int)));
}

//-----------------------------------------------------------------------------
/*! switch the current level when the current index of m_chooseLevelCombo is
 * changed
*/
void Filmstrip::onChooseLevelComboChanged(int index) {
  TApp *tapp = TApp::instance();
  // empty level
  if (index == m_chooseLevelCombo->findText(tr("- No Current Level -")))
    tapp->getCurrentLevel()->setLevel(0);
  else {
    std::vector<TFrameId> fids;
    m_levels[index]->getFids(fids);
    tapp->getCurrentFrame()->setFrameIds(fids);

    // retrieve to the current working frame of the level
    TFrameId WF;
    std::map<TXshSimpleLevel *, TFrameId>::iterator WFit;
    WFit = m_workingFrames.find(m_levels[index]);
    if (WFit != m_workingFrames.end())
      WF = WFit->second;
    else
      WF = fids[0];

    // this function emits xshLevelSwitched() signal and eventually calls
    // FlipConsole::UpdateRange
    // it may move the current frame so we need to keep the current frameId
    // before calling setLevel.
    tapp->getCurrentLevel()->setLevel(m_levels[index]);

    if (tapp->getCurrentSelection()->getSelection())
      tapp->getCurrentSelection()->getSelection()->selectNone();

    // move to the current working frame
    tapp->getCurrentFrame()->setFid(WF);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    invalidateIcons(m_levels[index], fids);

    QApplication::restoreOverrideCursor();
  }
}

//-----------------------------------------------------------------------------
/*! update combo items when the contents of scene cast are changed
*/
void Filmstrip::updateChooseLevelComboItems() {
  // clear items
  m_chooseLevelCombo->clear();
  m_levels.clear();

  std::map<TXshSimpleLevel *, TFrameId> new_workingFrames;

  // correct and register items
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene) {
    std::vector<TXshLevel *> levels;
    scene->getLevelSet()->listLevels(levels);
    std::vector<TXshLevel *>::iterator it;

    for (it = levels.begin(); it != levels.end(); ++it) {
      // register only TLV and PLI
      TXshSimpleLevel *sl = (*it)->getSimpleLevel();
      if (sl) {
        // register only used level in xsheet
        if (!scene->getTopXsheet()->isLevelUsed(sl)) continue;

        m_levels.push_back(sl);

        // create new m_workingFrames map with the new levelset
        TFrameId fId;
        std::map<TXshSimpleLevel *, TFrameId>::iterator WFit =
            m_workingFrames.find(sl);

        if (WFit != m_workingFrames.end())
          fId = WFit->second;
        else
          fId = sl->getFirstFid();

        new_workingFrames.insert(std::make_pair(sl, fId));

        QString levelName = QString::fromStdWString(sl->getName());
        if (sl->getProperties()->getDirtyFlag()) levelName += " *";

        // append the current working frame number to the item name
        if (fId != sl->getFirstFid() && fId.getNumber() >= 0)
          levelName +=
              QString("  [#") + QString::number(fId.getNumber()) + QString("]");

        m_chooseLevelCombo->addItem(levelName);
      }
    }
  }

  m_chooseLevelCombo->addItem(tr("- No Current Level -"));

  // swap the list
  m_workingFrames.clear();
  m_workingFrames = new_workingFrames;

  // synchronize the current index of combo to the current level
  updateCurrentLevelComboItem();
}

//-----------------------------------------------------------------------------
/*! synchronize the current index of combo to the current level
*/
void Filmstrip::updateCurrentLevelComboItem() {
  if (m_chooseLevelCombo->count() == 1) {
    m_chooseLevelCombo->setCurrentIndex(0);
    return;
  }

  TXshSimpleLevel *currentLevel =
      TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!currentLevel) {
    int noLevelIndex = m_chooseLevelCombo->findText(tr("- No Current Level -"));
    m_chooseLevelCombo->setCurrentIndex(noLevelIndex);
    return;
  }

  for (int i = 0; i < m_levels.size(); i++) {
    if (currentLevel->getName() == m_levels[i]->getName()) {
      m_chooseLevelCombo->setCurrentIndex(i);
      return;
    }
  }

  int noLevelIndex = m_chooseLevelCombo->findText(tr("- No Current Level -"));
  m_chooseLevelCombo->setCurrentIndex(noLevelIndex);
}

//-----------------------------------------------------------------------------

Filmstrip::~Filmstrip() {}

//-----------------------------------------------------------------------------

void Filmstrip::showEvent(QShowEvent *) {
  TApp *app                    = TApp::instance();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  bool ret = connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)),
                     SLOT(onLevelSwitched(TXshLevel *)));
  ret = ret &&
        connect(levelHandle, SIGNAL(xshLevelChanged()), SLOT(onLevelChanged()));

  // updateWindowTitle is called in the onLevelChanged
  ret = ret && connect(app->getPaletteController()->getCurrentLevelPalette(),
                       SIGNAL(colorStyleChangedOnMouseRelease()),
                       SLOT(onLevelChanged()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelTitleChanged()),
                       SLOT(onLevelChanged()));

  ret =
      ret && connect(m_frameArea->verticalScrollBar(),
                     SIGNAL(valueChanged(int)), this, SLOT(onSliderMoved(int)));

  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(updateChooseLevelComboItems()));
  ret = ret && connect(sceneHandle, SIGNAL(castChanged()), this,
                       SLOT(updateChooseLevelComboItems()));

  ret = ret &&
        connect(TApp::instance()->getCurrentXsheet(), SIGNAL(xsheetChanged()),
                this, SLOT(updateChooseLevelComboItems()));

  ret = ret && connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
                       SLOT(onFrameSwitched()));

  assert(ret);

  updateChooseLevelComboItems();
  onFrameSwitched();
  onLevelSwitched(0);
}

//-----------------------------------------------------------------------------

void Filmstrip::hideEvent(QHideEvent *) {
  TApp *app                    = TApp::instance();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  disconnect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
             SLOT(onLevelSwitched(TXshLevel *)));
  disconnect(levelHandle, SIGNAL(xshLevelChanged()), this,
             SLOT(onLevelChanged()));
  disconnect(TApp::instance()->getPaletteController()->getCurrentLevelPalette(),
             SIGNAL(colorStyleChangedOnMouseRelease()), this,
             SLOT(onLevelChanged()));

  disconnect(levelHandle, SIGNAL(xshLevelTitleChanged()), this,
             SLOT(onLevelChanged()));

  disconnect(m_frameArea->verticalScrollBar(), SIGNAL(valueChanged(int)), this,
             SLOT(onSliderMoved(int)));

  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
             SLOT(updateChooseLevelComboItems()));
  disconnect(sceneHandle, SIGNAL(castChanged()), this,
             SLOT(updateChooseLevelComboItems()));

  disconnect(TApp::instance()->getCurrentXsheet(), SIGNAL(xsheetChanged()),
             this, SLOT(updateChooseLevelComboItems()));

  disconnect(TApp::instance()->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(onFrameSwitched()));
}

//-----------------------------------------------------------------------------

void Filmstrip::resizeEvent(QResizeEvent *e) {
  m_frames->updateContentHeight();
  m_frameArea->verticalScrollBar()->setSingleStep(
      m_frames->getOneFrameHeight());
}

//-----------------------------------------------------------------------------

void Filmstrip::onLevelChanged() { updateWindowTitle(); }

//-----------------------------------------------------------------------------

void Filmstrip::updateWindowTitle() {
  updateCurrentLevelComboItem();

  TXshSimpleLevel *level = m_frames->getLevel();

  QString levelName;

  if (!level) {
    parentWidget()->setWindowTitle(tr("Level Strip"));
    return;
  } else {
    levelName = QString::fromStdWString(level->getName());
    if (level->getProperties()->getDirtyFlag()) levelName += " *";
  }

  // parentWidget() is TPanel
  parentWidget()->setWindowTitle(tr("Level:  ") + levelName);

  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  if (fh->isEditingLevel() && fh->getFid().getNumber() >= 0)
    levelName += QString("  [#") + QString::number(fh->getFid().getNumber()) +
                 QString("]");

  m_chooseLevelCombo->setItemText(m_chooseLevelCombo->currentIndex(),
                                  levelName);
}

//-----------------------------------------------------------------------------

void Filmstrip::onLevelSwitched(TXshLevel *oldLevel) {
  updateWindowTitle();

  int tc = ToonzCheck::instance()->getChecks();
  if (tc & (ToonzCheck::eInk | ToonzCheck::ePaint)) {
    TXshLevel *sl = TApp::instance()->getCurrentLevel()->getLevel();
    if (!sl) return;
    std::vector<TFrameId> fids;
    sl->getFids(fids);
    removeIcons(sl, fids, true);
  }
  update();
}

//-----------------------------------------------------------------------------

void Filmstrip::onSliderMoved(int val) {
  int oneFrameHeight = m_frames->getIconSize().height() + fs_frameSpacing +
                       fs_iconMarginTop + fs_iconMarginBottom;
  int tmpVal =
      (int)((float)val / (float)oneFrameHeight + 0.5f) * oneFrameHeight;
  m_frameArea->verticalScrollBar()->setValue(tmpVal);
}

//-----------------------------------------------------------------------------

void Filmstrip::onFrameSwitched() {
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  if (!fh->isEditingLevel()) return;

  TXshSimpleLevel *level = m_frames->getLevel();

  std::map<TXshSimpleLevel *, TFrameId>::iterator WFit;
  WFit = m_workingFrames.find(level);
  if (WFit == m_workingFrames.end()) return;

  WFit->second = fh->getFid();

  QString levelName = QString::fromStdWString(level->getName());
  if (level->getProperties()->getDirtyFlag()) levelName += " *";
  if (fh->getFid().getNumber() >= 0)
    levelName += QString("  [#") + QString::number(fh->getFid().getNumber()) +
                 QString("]");

  m_chooseLevelCombo->setItemText(m_chooseLevelCombo->currentIndex(),
                                  levelName);
}

//=============================================================================
// inbetweenDialog
//-----------------------------------------------------------------------------

InbetweenDialog::InbetweenDialog(QWidget *parent)
    : Dialog(TApp::instance()->getMainWindow(), true, "InBeetween") {
  setWindowTitle(tr("Inbetween"));

  QString linear(tr("Linear"));
  QString easeIn(tr("Ease In"));
  QString easeOut(tr("Ease Out"));
  QString easeInOut(tr("Ease In / Ease Out"));
  QStringList items;
  items << linear << easeIn << easeOut << easeInOut;

  beginHLayout();
  m_comboBox = new QComboBox(this);
  m_comboBox->addItems(items);
  addWidget(tr("Interpolation:"), m_comboBox);
  endHLayout();

  QPushButton *okBtn     = new QPushButton(tr("Inbetween"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);
}

//-----------------------------------------------------------------------------

QString InbetweenDialog::getValue() { return m_comboBox->currentText(); }

//-----------------------------------------------------------------------------

void InbetweenDialog::setValue(const QString &value) {
  int currentIndex                   = m_comboBox->findText(value);
  if (currentIndex < 0) currentIndex = 0;
  m_comboBox->setCurrentIndex(currentIndex);
}

//-----------------------------------------------------------------------------

int InbetweenDialog::getIndex(const QString &text) {
  return m_comboBox->findText(text);
}

//=============================================================================

OpenFloatingPanel openFilmstripCommand(MI_OpenFilmStrip, "FilmStrip",
                                       QObject::tr("Level: "));
