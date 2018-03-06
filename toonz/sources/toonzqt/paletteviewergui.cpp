

#include "toonzqt/paletteviewergui.h"

// TnzQt includes
#include "toonzqt/styleselection.h"
#include "toonzqt/trepetitionguard.h"
#include "toonzqt/gutil.h"
#include "toonzqt/paletteviewer.h"
#include "toonzqt/selectioncommandids.h"
#include "toonzqt/stylenameeditor.h"
#include "toonzqt/viewcommandids.h"
#include "palettedata.h"
#include "toonzqt/lutcalibrator.h"

// TnzLib includes
#include "toonz/palettecmd.h"
#include "toonz/txshlevel.h"
#include "toonz/studiopalette.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/imagestyles.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tcolorstyles.h"
#include "tundo.h"
#include "tenv.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenuBar>
#include <QToolTip>
#include <QDrag>
#include <QAction>
#include <QShortcut>

// enable to set font size for style name separately from other texts
TEnv::IntVar EnvSoftwareCurrentFontSize_StyleName(
    "SoftwareCurrentFontSize_StyleName", 11);

using namespace PaletteViewerGUI;
using namespace DVGui;

//=============================================================================
/*! \namespace PaletteViewerGUI
                \brief Provides a collection of widget to show \b palette.
                */

//=============================================================================
/*! \enum PaletteViewerGUI::PaletteViewType
                \brief Useful to mark different palette viewer type.

                There are three possible palette to view:
                \li level palette, palette of current level;
                \li cleanup palette, current scene cleanup palette;
                \li studio palette.
                */

//=============================================================================
/*! \class PaletteViewerGUI::PageViewer
                \brief The PageViewer class provides an object to view and
   manage palettes page.

                Inherits \b QFrame and \b TSelection::View.

                This object allows to show and manage a single palette page. An
   enum type
                \b PaletteViewerGUI::PageViewer::ViewMode, getViewMode(),
   specify visualization
                mode, it is possible set it using setViewMode(). Class manage
   all mouse event,
                move, press, drag and drop.
                */
/*! \enum PaletteViewerGUI::PageViewer::ViewMode
                Useful to mark different page view moed:
                \li SmallChips: displays styles as small squares; the name of
   the style is
                displayed as tooltip.
                \li SmallChipsWithName:
                displays styles as small squares with the name insde.
                \li LargeChips: displays the styles on top of the name of the
   style.
                \li List:				List displays style
   thumbnails next to their names in a list;
                if the palette styles are referring to a studio palette, its
                path will be displayed along with the style name.
                */
/*!	\fn TPalette::Page* PageViewer::getPage() const
                Return current page.
                */
/*!	\fn ViewMode PageViewer::getViewMode() const
                Return current page view mode \b
   PaletteViewerGUI::PageViewer::ViewMode.
                */
/*! \fn void TPalette::onSelectionChanged()
                Update current page view.
                */
PageViewer::PageViewer(QWidget *parent, PaletteViewType viewType,
                       bool hasPasteColors)
    : QFrame(parent)
    , m_page(0)
    , m_chipsOrigin(2, 2)
    , m_chipPerRow(0)
    , m_viewMode(SmallChips)
    , m_styleSelection(new TStyleSelection())
    , m_frameHandle(0)
    , m_renameTextField(new LineEdit(this))
    , m_dropPositionIndex(-1)
    , m_dropPageCreated(false)
    , m_startDrag(false)
    , m_viewType(viewType)
    , m_nameDisplayMode(Style)
    , m_hasPasteColors(hasPasteColors)
    , m_changeStyleCommand(0)
    , m_styleNameEditor(NULL) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("PageViewer");
  setFocusPolicy(Qt::StrongFocus);
  CommandManager *cmd    = CommandManager::instance();
  QAction *pasteValueAct = cmd->getAction(MI_PasteInto);
  addAction(pasteValueAct);
  if (m_hasPasteColors) {
    QAction *pasteColorsAct = cmd->getAction(MI_PasteColors);
    addAction(pasteColorsAct);
  }

  m_renameTextField->hide();
  m_renameTextField->setObjectName("RenameColorTextField");
  connect(m_renameTextField, SIGNAL(editingFinished()), this,
          SLOT(onStyleRenamed()));
  m_styleSelection->setView(this);
  setAcceptDrops(true);

  // applying default chip sizes
  ViewMode defaultChipSize;
  switch (m_viewType) {
  case LEVEL_PALETTE:
    defaultChipSize = (ViewMode)ChipSizeManager::instance()->chipSize_Palette;
    break;
  case CLEANUP_PALETTE:
    defaultChipSize = (ViewMode)ChipSizeManager::instance()->chipSize_Cleanup;
    break;
  case STUDIO_PALETTE:
    defaultChipSize = (ViewMode)ChipSizeManager::instance()->chipSize_Studio;
    break;
  default:
    defaultChipSize = (ViewMode)2;
    break;
  }
  setViewMode(defaultChipSize);
}

//-----------------------------------------------------------------------------

PageViewer::~PageViewer() { delete m_styleSelection; }

//-----------------------------------------------------------------------------

void PageViewer::setPaletteHandle(TPaletteHandle *paletteHandle) {
  TPaletteHandle *previousPalette = getPaletteHandle();
  if (previousPalette == paletteHandle) return;

  if (previousPalette)
    disconnect(previousPalette, SIGNAL(colorStyleChanged(bool)), this,
               SLOT(update()));

  m_styleSelection->setPaletteHandle(paletteHandle);
  connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), SLOT(update()));

  if (m_styleNameEditor) m_styleNameEditor->setPaletteHandle(paletteHandle);
}

//-----------------------------------------------------------------------------

TPaletteHandle *PageViewer::getPaletteHandle() const {
  return m_styleSelection->getPaletteHandle();
}

//-----------------------------------------------------------------------------

void PageViewer::setXsheetHandle(TXsheetHandle *xsheetHandle) {
  m_styleSelection->setXsheetHandle(xsheetHandle);
}

//-----------------------------------------------------------------------------

TXsheetHandle *PageViewer::getXsheetHandle() const {
  return m_styleSelection->getXsheetHandle();
}

//-----------------------------------------------------------------------------
/*! for clearing the cache when executing paste style command from
 * StyleSelection
*/
void PageViewer::setLevelHandle(TXshLevelHandle *levelHandle) {
  m_styleSelection->setLevelHandle(levelHandle);
}

//-----------------------------------------------------------------------------

void PageViewer::setFrameHandle(TFrameHandle *frameHandle) {
  m_frameHandle = frameHandle;
}

//-----------------------------------------------------------------------------

TFrameHandle *PageViewer::getFrameHandle() const { return m_frameHandle; }

//-----------------------------------------------------------------------------

void PageViewer::setCurrentStyleIndex(int index) {
  getPaletteHandle()->setStyleIndex(index);
}

//-----------------------------------------------------------------------------

int PageViewer::getCurrentStyleIndex() const {
  return getPaletteHandle()->getStyleIndex();
}

//-----------------------------------------------------------------------------
/*! Set current page to \b page and update view.
*/
void PageViewer::setPage(TPalette::Page *page) {
  m_page = page;
  computeSize();
  update();
}

//-----------------------------------------------------------------------------
/*! Return chip count contained in current page.
*/
int PageViewer::getChipCount() const {
  return m_page ? m_page->getStyleCount() : 0;
}

//-----------------------------------------------------------------------------
/*! Set current view mode \b PaletteViewerGUI::PageViewer::ViewMode to \b
 * viewMode and update view.
*/
void PageViewer::setViewMode(ViewMode viewMode) {
  if (m_viewMode == viewMode) return;
  m_viewMode = viewMode;
  computeSize();
  update();

  // keep new view mode for reproducing the same mode when a new palette is made
  switch (m_viewType) {
  case LEVEL_PALETTE:
    ChipSizeManager::instance()->chipSize_Palette = (int)m_viewMode;
    break;
  case CLEANUP_PALETTE:
    ChipSizeManager::instance()->chipSize_Cleanup = (int)m_viewMode;
    break;
  case STUDIO_PALETTE:
    ChipSizeManager::instance()->chipSize_Studio = (int)m_viewMode;
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------
/*! Return current chip index by position \b pos.
                N.B. Cannot return chip index > max.
                */
int PageViewer::posToIndex(const QPoint &pos) const {
  if (m_chipPerRow == 0) return -1;
  QSize chipSize = getChipSize();
  int j          = (pos.x() - m_chipsOrigin.x()) / chipSize.width();
  int i          = (pos.y() - m_chipsOrigin.y()) / chipSize.height();
  int index      = i * m_chipPerRow + j;
  // if(index<0 || index>=getChipCount()) return -1;
  return index;
}

//-----------------------------------------------------------------------------
/*! Return item rect of chip identified by \b index. Item rect is the rect of
                chip + name.
                */
QRect PageViewer::getItemRect(int index) const {
  if (m_chipPerRow == 0) return QRect();
  int row        = index / m_chipPerRow;
  int col        = index - (row * m_chipPerRow);
  int x0         = m_chipsOrigin.x();
  int y0         = m_chipsOrigin.y();
  QSize chipSize = getChipSize();
  int chipLx     = chipSize.width();
  int chipLy     = chipSize.height();
  QRect rect(x0 + chipLx * col, y0 + chipLy * row, chipLx, chipLy);
  return rect;
}

//-----------------------------------------------------------------------------
/*! Return rect of color area of chip identified by \b index.
*/
QRect PageViewer::getColorChipRect(int index) const {
  QRect rect = getItemRect(index);
  if (rect.isNull()) return rect;
  if (m_viewMode == LargeChips)
    rect.setHeight(20);
  else if (m_viewMode == List)
    rect.setWidth(rect.height());
  return rect;
}

//-----------------------------------------------------------------------------
/*! Return rect of chip identified by \b index name. (Not in \b SmallChips).
*/
QRect PageViewer::getColorNameRect(int index) const {
  QRect rect = getItemRect(index);
  if (rect.isNull()) return rect;
  if (m_viewMode == LargeChips)
    rect.adjust(-10, 14, 10, -14);
  else if (m_viewMode == MediumChips)
    rect.adjust(-15, 7, 15, -7);
  else if (m_viewMode == List)
    rect.setLeft(rect.left() + rect.height());
  else
    return rect;
  return rect;
}

//-----------------------------------------------------------------------------
/*! Add color to current page; if indexInPage == -1 add color at the bottom of
 * page.
*/
void PageViewer::drop(int dstIndexInPage, const QMimeData *mimeData) {
  assert(m_page);
  TPalette *palette = m_page->getPalette();
  if (!palette) return;
  int dstPageIndex = m_page->getIndex();
  if ((m_page->getStyleId(0) == 0 || m_page->getStyleId(1) == 1) &&
      dstIndexInPage < 2)
    dstIndexInPage                       = 2;
  if (dstIndexInPage < 0) dstIndexInPage = m_page->getStyleCount();

  const PaletteData *paletteData = dynamic_cast<const PaletteData *>(mimeData);

  if (!paletteData || !paletteData->hasStyleIndeces()) return;

  TPalette *srcPalette           = paletteData->getPalette();
  int srcPageIndex               = paletteData->getPageIndex();
  std::set<int> srcIndicesInPage = paletteData->getIndicesInPage();

  if (m_dropPageCreated) {
    // Rimuovo e reinserisco la pagina inizializzando l'undo.
    palette->setDirtyFlag(true);
    int pageIndex = palette->getPageCount() - 1;
    palette->erasePage(pageIndex);
    if (dstPageIndex != srcPageIndex && pageIndex == dstPageIndex) {
      TUndoManager::manager()->beginBlock();
      PaletteCmd::addPage(getPaletteHandle());
    } else
      m_dropPageCreated = false;
    getPaletteHandle()->notifyPaletteChanged();
  }
  if (srcPalette == palette) {
    PaletteCmd::arrangeStyles(getPaletteHandle(), dstPageIndex, dstIndexInPage,
                              srcPageIndex, srcIndicesInPage);

    // seleziono gli stili spostati
    clearSelection();
    std::set<int>::const_reverse_iterator i;
    int k = 0;
    for (i = srcIndicesInPage.rbegin(); i != srcIndicesInPage.rend();
         ++i, ++k) {
      int index = 0;
      if (*i <= dstIndexInPage)
        index = dstIndexInPage - k - 1;
      else
        index = dstIndexInPage + k;
      m_styleSelection->select(dstPageIndex, index, true);
    }
  } else  // when dropping the style into another palette
  {
    std::vector<TColorStyle *> styles;
    std::set<int>::iterator it;
    for (it = srcIndicesInPage.begin(); it != srcIndicesInPage.end(); ++it) {
      int indexInPage = *it;
      styles.push_back(
          srcPalette->getPage(srcPageIndex)->getStyle(indexInPage));
    }

    PaletteCmd::addStyles(getPaletteHandle(), dstPageIndex, dstIndexInPage,
                          styles);
  }

  if (m_dropPageCreated) {
    m_dropPageCreated = false;
    TUndoManager::manager()->endBlock();
  }
}

//-----------------------------------------------------------------------------
/*! Create an empty page to receive drop.
*/
void PageViewer::createDropPage() {
  if (m_dropPageCreated) return;
  m_dropPageCreated = true;
  assert(m_page);
  TPalette *palette = m_page->getPalette();
  if (palette) PaletteCmd::addPage(getPaletteHandle(), L"", false);
}

//-----------------------------------------------------------------------------

void PageViewer::clearSelection() { m_styleSelection->selectNone(); }

//-----------------------------------------------------------------------------
/*! Return page chip size, it depend from current \b
 * PaletteViewerGUI::PageViewer::ViewMode.
*/
QSize PageViewer::getChipSize() const {
  if (m_viewMode == SmallChips || m_viewMode == SmallChipsWithName)
    return QSize(48, 33);
  else if (m_viewMode == MediumChips)
    return QSize(98, 38);
  else if (m_viewMode == LargeChips)
    return QSize(155, 53);
  else
    return QSize(width(), 22);
}

//-----------------------------------------------------------------------------
/*! Draw a single chip style \b style in \b chipRect.
*/
void PageViewer::drawColorChip(QPainter &p, QRect &chipRect,
                               TColorStyle *style) {
  TRaster32P icon = style->getIcon(qsize2Dimension(chipRect.size()));
  p.drawPixmap(chipRect.left(), chipRect.top(), rasterToQPixmap(icon, false));
  p.drawRect(chipRect);
}

//-----------------------------------------------------------------------------
/*! Draw style \b style name in \b nameRect.
*/
void PageViewer::drawColorName(QPainter &p, QRect &nameRect, TColorStyle *style,
                               int styleIndex) {
  if (m_viewMode == SmallChips && style->getFlags() == 0) return;
  TPalette *palette = (m_page) ? m_page->getPalette() : 0;

  QString name = QString::fromStdWString(style->getName());
  if (m_viewMode == List) {
    p.setPen(getTextColor());

    std::pair<TFilePath, int> g =
        StudioPalette::instance()->getSourceStyle(style);
    if (g.first != TFilePath() && g.second >= 0)
      name += "  " + toQString(g.first) + ":" + QString::number(g.second);
    if (style->getFlags() != 0) name += "(autopaint)";

    TPoint pickedPos = style->getPickedPosition();
    if (pickedPos != TPoint())
      name += QString(" (%1,%2)").arg(pickedPos.x).arg(pickedPos.y);

    p.drawText(nameRect.adjusted(6, 4, -6, -4), name);

    QColor borderCol(getTextColor());
    borderCol.setAlphaF(0.3);
    p.setPen(borderCol);
  }

  if (m_viewMode == SmallChips && style->getFlags() != 0) {
    QRect rect(nameRect.left(), nameRect.top(), 9, 9);
    p.fillRect(rect, QBrush(Qt::white));
    p.drawRect(rect);
    p.drawText(rect, Qt::AlignCenter, "a");
  }
  if (m_viewMode == SmallChipsWithName && name != "" && name != "color") {
    QRect rect     = nameRect;
    QPen oldPen    = p.pen();
    TPixel32 color = style->getMainColor();
    int v          = (int)(0.299 * color.r + 0.587 * color.g + 0.114 * color.b);
    p.setPen(v > 127 ? Qt::black : Qt::white);
    int textWidth = QFontMetrics(p.font()).width(name);
    if (textWidth < rect.width() - 2)
      p.drawText(rect, Qt::AlignCenter, name);
    else
      p.drawText(rect.adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                 name);
    p.setPen(oldPen);
  }

  if (m_viewMode == LargeChips) {
    QString index                = QString::number(styleIndex);
    QFont font                   = p.font();
    int fontSize                 = font.pointSize();
    if (fontSize == -1) fontSize = font.pixelSize();
    int length                   = index.length() * fontSize;
    int w                        = (length > 11) ? (length) : 11;
    int h                        = 11;
    int x0                       = nameRect.right() - w + 1;
    int y0                       = nameRect.top() - h - 1;
    p.drawText(nameRect.adjusted(6, 1, -6, -1), name);
    QRect indexRect(x0, y0, w, h);
    p.fillRect(indexRect, QBrush(Qt::white));
    p.drawRect(indexRect);
    p.drawText(indexRect, Qt::AlignCenter, index);

    QFont f = p.font();
    f.setFamily("Calibri");
    p.setFont(f);
    if (style->getFlags() != 0) {
      QRect rect(nameRect.left(), y0, w, h);
      p.fillRect(rect, QBrush(Qt::white));
      p.drawRect(rect);
      p.drawText(rect.adjusted(0, +1, 0, +1), Qt::AlignCenter, "A");
    }
    TTextureStyle *ts = dynamic_cast<TTextureStyle *>(style);
    if (ts) {
      QRect rect(nameRect.left() + ((style->getFlags() != 0) ? w : 0), y0, w,
                 h);
      p.fillRect(rect, QBrush(Qt::white));
      p.drawRect(rect);
      p.drawText(rect.adjusted(0, +1, 0, +1), Qt::AlignCenter,
                 ts->isPattern() ? "P" : "T");
    }
  }

  p.drawRect(nameRect);
}

//-----------------------------------------------------------------------------
/*! Draw the toggle to know if \b style is linked to a studio palette.
*/
void PageViewer::drawToggleLink(QPainter &p, QRect &chipRect,
                                TColorStyle *style) {
  std::wstring globalName = style->getGlobalName();
  if (globalName != L"" && (globalName[0] == L'-' || globalName[0] == L'+')) {
    TPixel32 c = style->getMainColor();
    int x      = chipRect.topRight().x() - 6;
    int y      = chipRect.topRight().y();
    QRect rect(x, y, 7, 7);

    p.fillRect(rect, QBrush(Qt::white));
    p.setPen(Qt::black);
    p.drawRect(rect);

    if (globalName[0] == L'+') {
      QPointF a(x + 2, y + 2);
      QPointF b(x + 2, y + 5);
      QPointF c(x + 5, y + 2);
      QPointF d(x + 5, y + 5);
      p.drawLine(a, b);
      p.drawLine(a, c);
      p.drawLine(a, d);
    }
  }
}

//-----------------------------------------------------------------------------
/*! Pain current page styles using current view mode.
*/
void PageViewer::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  if (m_chipPerRow == 0) {
    p.drawText(QPoint(5, 25), tr("- No Styles -"));
    return;
  }

  // currentStyle e palette
  TPalette *palette = (m_page) ? m_page->getPalette() : 0;
  if (!palette) return;

  // [i0,i1] = range celle visibili
  QRect visibleRect            = e->rect();
  int i0                       = posToIndex(visibleRect.topLeft());
  if (i0 < 0) i0               = 0;
  int i1                       = posToIndex(visibleRect.bottomRight());
  if (i1 >= getChipCount()) i1 = getChipCount() - 1;

  if (m_viewMode == List) {
    // disegno le celle
    int i;
    int currentStyleIndexInPage = -1;
    for (i = i0; i <= i1; i++) {
      TColorStyle *style = m_page->getStyle(i);
      int styleIndex     = m_page->getStyleId(i);
      if (getCurrentStyleIndex() == styleIndex) currentStyleIndexInPage = i;

      QRect chipRect = getColorChipRect(i);
      p.setPen(Qt::black);
      drawColorChip(p, chipRect, style);

      // name, index and shortcut
      QRect nameRect = getColorNameRect(i);
      drawColorName(p, nameRect, style, styleIndex);

      // if numpad shortcut is activated, draw shortcut scope
      if (Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
          m_viewType == LEVEL_PALETTE &&
          palette->getStyleShortcut(styleIndex) >= 0) {
        p.setPen(QPen(QColor(0, 0, 0, 128), 2));
        p.drawLine(nameRect.topLeft() + QPoint(2, 1),
                   nameRect.bottomLeft() + QPoint(2, 0));
        p.setPen(QPen(QColor(255, 255, 255, 128), 2));
        p.drawLine(nameRect.topLeft() + QPoint(4, 1),
                   nameRect.bottomLeft() + QPoint(4, 0));
      }

      // selezione
      if (m_styleSelection->isSelected(m_page->getIndex(), i)) {
        p.setPen(Qt::white);
        QRect itemRect = getItemRect(i);
        p.drawRect(itemRect);
        p.drawRect(chipRect.adjusted(1, 1, -1, -1));
      }
      // stile corrente
      if (i == currentStyleIndexInPage && 0 <= i && i < getChipCount()) {
        QRect rect = getItemRect(i);

        p.setPen(QColor(180, 210, 255));
        p.drawRect(rect.adjusted(1, 1, -1, -1));

        p.setPen(Qt::white);
        p.drawRect(rect.adjusted(2, 2, -2, -2));
        p.setPen(Qt::black);
        p.drawRect(rect.adjusted(3, 3, -3, -3));

        if (!m_styleSelection->isSelected(m_page->getIndex(), i)) {
          p.setPen(QColor(225, 225, 225));
          p.drawRect(rect.adjusted(1, 1, -1, -1));
        }
      }

      // toggle link
      drawToggleLink(p, chipRect, m_page->getStyle(i));
    }
  } else {
    int currentStyleIndex = getCurrentStyleIndex();
    int i;
    for (i = i0; i <= i1; i++) {
      TColorStyle *style = m_page->getStyle(i);
      int styleIndex     = m_page->getStyleId(i);

      // if numpad shortcut is activated, draw shortcut scope
      if (Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
          m_viewType == LEVEL_PALETTE &&
          palette->getStyleShortcut(styleIndex) >= 0) {
        QRect itemRect = getItemRect(i);
        // paint dark
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 64));
        p.drawRect(itemRect);
        // check the neighbours and draw light lines
        p.setPen(QPen(QColor(255, 255, 255, 128), 2));
        // top
        if (!hasShortcut(i - m_chipPerRow))
          p.drawLine(itemRect.topLeft(), itemRect.topRight() - QPoint(1, 0));
        // left
        if (i % m_chipPerRow == 0 || !hasShortcut(i - 1))
          p.drawLine(itemRect.topLeft() + QPoint(0, 1), itemRect.bottomLeft());
        // bottom
        if (!hasShortcut(i + m_chipPerRow))
          p.drawLine(itemRect.bottomLeft() + QPoint(1, 0),
                     itemRect.bottomRight());
        // right
        if ((i + 1) % m_chipPerRow == 0 || !hasShortcut(i + 1))
          p.drawLine(itemRect.topRight(),
                     itemRect.bottomRight() - QPoint(0, 1));
      }

      // draw white frame if the style is selected or current
      if (m_styleSelection->isSelected(m_page->getIndex(), i) ||
          currentStyleIndex == styleIndex) {
        QRect itemRect = getItemRect(i).adjusted(0, -1, 0, 1);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundRect(itemRect, 7, 25);
      }
      // paint style
      QRect chipRect = getItemRect(i).adjusted(4, 4, -5, -5);

      QColor styleColor((int)style->getMainColor().r,
                        (int)style->getMainColor().g,
                        (int)style->getMainColor().b);
      // draw with MainColor for TSolidColorStyle(3), TColorCleanupStyle(2001)
      // and TBlackCleanupStyle(2002)
      if (style->getTagId() == 3 || style->getTagId() == 2001 ||
          style->getTagId() == 2002) {
        if (LutCalibrator::instance()->isValid())
          LutCalibrator::instance()->convert(styleColor);

        p.fillRect(chipRect, QBrush(styleColor));

        // if the style is not 100% opac, draw the bottom part of the chip with
        // the pixmap with alpha
        if ((int)style->getMainColor().m !=
            (int)style->getMainColor().maxChannelValue) {
          QRect bottomRect = chipRect;
          if (m_viewMode == LargeChips) {
            bottomRect.adjust(0, bottomRect.height() - 12, 0, 0);
          } else
            bottomRect.adjust(0, bottomRect.height() - 6, 0, 0);

          TRaster32P icon = style->getIcon(qsize2Dimension(bottomRect.size()));
          p.drawPixmap(bottomRect.left(), bottomRect.top(),
                       rasterToQPixmap(icon));
        }
        // Use white line for dark color. Use black line for light color.
        int val = (int)style->getMainColor().r * 30 +
                  (int)style->getMainColor().g * 59 +
                  (int)style->getMainColor().b * 11;
        if (val < 12800)
          p.setPen(Qt::white);
        else
          p.setPen(Qt::black);

      } else {
        TRaster32P icon = style->getIcon(qsize2Dimension(chipRect.size()));
        p.drawPixmap(chipRect.left(), chipRect.top(), rasterToQPixmap(icon));

        if (m_viewMode == LargeChips) {
          p.setPen(Qt::NoPen);
          p.setBrush(QColor(0, 0, 0, 140));
          QRect zabutonRect = chipRect.adjusted(0, 12, 0, -12);
          p.drawRect(zabutonRect);
        }
        p.setPen(Qt::white);
      }
      p.setBrush(Qt::NoBrush);

      // diagonal line indicates that the style color is changed after linked
      // from studio palette
      if (style->getIsEditedFlag()) {
        // draw diagonal line
        p.drawLine(chipRect.topRight(), chipRect.bottomLeft());

        // keep center blank for style name
        if (m_viewMode != SmallChips) {
          QRect colorNameRect = chipRect;
          if (m_viewMode == MediumChips)
            colorNameRect.adjust(2, 9, -2, -8);
          else  // LargeChip
            colorNameRect.adjust(2, 16, -2, -13);

          p.fillRect(colorNameRect, QBrush(styleColor));
        }
      }

      // draw style name
      QFont preFont = p.font();
      QFont tmpFont = p.font();

      if (m_viewMode != SmallChips) {
        if (m_viewMode == MediumChips) {
          tmpFont.setPixelSize(EnvSoftwareCurrentFontSize_StyleName);
          p.setFont(tmpFont);
        } else if (m_viewMode == LargeChips) {
          tmpFont.setPixelSize(EnvSoftwareCurrentFontSize_StyleName + 2);
          p.setFont(tmpFont);
        }

        std::wstring name     = style->getName();
        std::wstring origName = style->getOriginalName();

        // display the name (style name and original name) according to the name
        // display mode
        if (m_nameDisplayMode == Style)
          p.drawText(chipRect, Qt::AlignCenter, QString::fromStdWString(name));
        else if (m_nameDisplayMode == Original) {
          if (origName != L"") {
            tmpFont.setItalic(true);
            p.setFont(tmpFont);
            p.drawText(chipRect, Qt::AlignCenter,
                       QString::fromStdWString(origName));
          } else  // if there is no original name, then display the style name
                  // in brackets
            p.drawText(chipRect, Qt::AlignCenter,
                       QString::fromStdWString(L"( " + name + L" )"));
        } else if (m_nameDisplayMode == StyleAndOriginal) {
          p.drawText(chipRect, Qt::AlignCenter, QString::fromStdWString(name));

          // display original name only whne LargeChip view
          if (m_viewMode == LargeChips && origName != L"") {
            tmpFont.setItalic(true);
            tmpFont.setPixelSize(tmpFont.pixelSize() - 3);
            p.setFont(tmpFont);
            p.drawText(chipRect.adjusted(4, 4, -4, -4),
                       Qt::AlignLeft | Qt::AlignTop,
                       QString::fromStdWString(origName));
          }
        }
      }

      // draw the frame border if the style is selected or current
      if (m_styleSelection->isSelected(m_page->getIndex(), i) ||
          currentStyleIndex == styleIndex) {
        p.setBrush(Qt::NoBrush);
        p.drawRect(chipRect.adjusted(3, 3, -3, -3));
      }

      // draw border
      p.setPen(Qt::black);
      p.setBrush(Qt::NoBrush);
      p.drawRect(chipRect);

      // draw style index
      tmpFont.setPointSize(9);
      tmpFont.setItalic(false);
      p.setFont(tmpFont);
      int indexWidth = fontMetrics().width(QString().setNum(styleIndex)) + 4;
      QRect indexRect(chipRect.bottomRight() + QPoint(-indexWidth, -14),
                      chipRect.bottomRight());
      p.setPen(Qt::black);
      p.setBrush(Qt::white);
      p.drawRect(indexRect);
      p.drawText(indexRect, Qt::AlignCenter, QString().setNum(styleIndex));

      // draw "Autopaint for lines" indicator
      int offset = 0;
      if (style->getFlags() != 0) {
        QRect aflRect(chipRect.bottomLeft() + QPoint(0, -14), QSize(12, 15));
        p.drawRect(aflRect);
        p.drawText(aflRect, Qt::AlignCenter, "A");
        offset += 12;
      }

      // draw "Picked Position" indicator (not show on small chip mode)
      if (style->getPickedPosition() != TPoint() && m_viewMode != SmallChips) {
        QRect ppRect(chipRect.bottomLeft() + QPoint(offset, -14),
                     QSize(12, 15));
        p.drawRect(ppRect);
        QPoint markPos = ppRect.center() + QPoint(1, 0);
        p.drawEllipse(markPos, 3, 3);
        p.drawLine(markPos - QPoint(5, 0), markPos + QPoint(5, 0));
        p.drawLine(markPos - QPoint(0, 5), markPos + QPoint(0, 5));
      }

      // if numpad shortcut is activated, draw shortcut number on top
      if (Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
          m_viewType == LEVEL_PALETTE &&
          palette->getStyleShortcut(styleIndex) >= 0 &&
          m_viewMode != SmallChips) {
        int key      = palette->getStyleShortcut(styleIndex);
        int shortcut = key - Qt::Key_0;
        QRect ssRect(chipRect.center().x() - 8, chipRect.top() - 11, 16, 20);
        p.setBrush(Qt::gray);
        p.drawChord(ssRect, 0, -180 * 16);
        tmpFont.setPointSize(6);
        p.setFont(tmpFont);
        // make sure the text is visible with any font
        static int rectTopAdjust = 19 - QFontMetrics(tmpFont).overlinePos();
        p.drawText(ssRect.adjusted(0, rectTopAdjust, 0, 0), Qt::AlignCenter,
                   QString().setNum(shortcut));
      }

      // revert font set
      p.setFont(preFont);
      // revert brush
      p.setBrush(Qt::NoBrush);

      // draw link indicator
      drawToggleLink(p, chipRect, style);
    }
  }

  // indicatore di drop
  if (m_dropPositionIndex >= 0) {
    QRect itemRect = getItemRect(m_dropPositionIndex);
    QRect rect;
    if (m_viewMode == List)
      rect = QRect(itemRect.left(), itemRect.top() - 1, itemRect.width(), 2);
    else
      rect = QRect(itemRect.left() - 1, itemRect.top(), 2, itemRect.height());
    p.setPen(Qt::black);
    p.drawRect(rect);
    p.setPen(Qt::white);
    p.drawRect(rect.adjusted(-1, -1, 1, 1));
  }
}

//-----------------------------------------------------------------------------
/*! Recall computeSize().
*/
void PageViewer::resizeEvent(QResizeEvent *) { computeSize(); }

//-----------------------------------------------------------------------------
/*! If current page is empty return; otherwise select or unselect chip in regard
                to event position.
                */
void PageViewer::mousePressEvent(QMouseEvent *event) {
  if (!m_page) return;
  TPalette *palette = m_page->getPalette();
  QPoint pos        = event->pos();
  int indexInPage   = posToIndex(pos);
  m_startDrag       = false;
  if (!m_page) return;
  int pageIndex = m_page->getIndex();

  // disable style selection in the cleanup palette in order to avoid editing
  // the styles by mistake.
  if (m_viewType == CLEANUP_PALETTE) {
    if (0 <= indexInPage && indexInPage < getChipCount()) {
      // Right-click is available
      if (event->button() == Qt::RightButton) {
        m_styleSelection->makeCurrent();
        m_styleSelection->selectNone();
        m_styleSelection->select(pageIndex);
        m_styleSelection->select(pageIndex, indexInPage, true);
      }
      // Changing the current style is still available
      setCurrentStyleIndex(m_page->getStyleId(indexInPage));
    }
    update();
    return;
  }

  if (event->button() == Qt::RightButton) {
    m_styleSelection->makeCurrent();
    // if you are clicking on the color chip
    if (0 <= indexInPage && indexInPage < getChipCount()) {
      // Se pageIndex non e' selezionato lo seleziono
      if (!m_styleSelection->isSelected(pageIndex, indexInPage)) {
        m_styleSelection->select(pageIndex);
        m_styleSelection->select(pageIndex, indexInPage, true);
      }
      // Cambio l'indice corrente
      setCurrentStyleIndex(m_page->getStyleId(indexInPage));
    } else {
      m_styleSelection->selectNone();
      m_styleSelection->select(pageIndex);
    }
    update();
    // update locks when the styleSelection becomes current
    updateCommandLocks();
    return;
  }
  m_dragStartPosition = pos;
  if (indexInPage < 0 || indexInPage >= getChipCount()) {
    // l'utente ha fatto click fuori dai color chip. vuole deselezionare tutto
    // (lasciando la selezione attiva, per un eventuale paste)
    m_styleSelection->select(pageIndex);
    m_styleSelection->makeCurrent();

    update();
    // update locks when the styleSelection becomes current
    updateCommandLocks();
    return;
  } else {
    // O si sta selezonando un nuovo item O si vuole iniziare un drag
    if (m_styleSelection->isSelected(pageIndex, indexInPage) &&
        event->modifiers() == Qt::ControlModifier &&
        !m_page->getPalette()->isLocked())
      m_startDrag = true;
    else
      select(indexInPage, event);
  }
}

//-----------------------------------------------------------------------------
/*! If left botton is pressed start drag.
*/
void PageViewer::mouseMoveEvent(QMouseEvent *event) {
  if (!m_page) return;
  if (m_viewType == CLEANUP_PALETTE) return;

  // continuo solo se e' un drag (bottone sinistro premuto)
  if (!(event->buttons() & Qt::LeftButton)) return;

  if (m_page->getPalette()->isLocked()) return;

  // Se spingo ctrl mentre mi sto muovendo con almeno un item selezionato
  // abilito la possibilita' di fare drag.
  if (!m_startDrag && event->modifiers() == Qt::ControlModifier &&
      !m_styleSelection->isEmpty() &&
      (event->pos() - m_dragStartPosition).manhattanLength() > 12)
    m_startDrag = true;
  // faccio partire il drag&drop solo se mi sono mosso di una certa quantita'
  if ((event->pos() - m_dragStartPosition).manhattanLength() > 20 &&
      m_startDrag) {
    assert(m_styleSelection && !m_styleSelection->isEmpty());
    startDragDrop();
  }
}

//-----------------------------------------------------------------------------

void PageViewer::mouseReleaseEvent(QMouseEvent *event) {
  if (!m_page) return;
  TPalette *palette = m_page->getPalette();
  QPoint pos        = event->pos();
  int indexInPage   = posToIndex(pos);
  if (m_startDrag && m_dropPositionIndex == -1 &&
      event->modifiers() == Qt::ControlModifier)
    select(indexInPage, event);
  m_startDrag = false;
}

//-----------------------------------------------------------------------------
/*! If double click position is not in a chip rect return; otherwise if is in
                name rect open a text field, if is in color rect open a style
   editor.
                */
void PageViewer::mouseDoubleClickEvent(QMouseEvent *e) {
  int index = posToIndex(e->pos());
  if (index < 0 || index >= getChipCount()) return;
  TColorStyle *style = m_page->getStyle(index);
  if (!style) return;

  if (m_page->getPalette()->isLocked()) return;

  if (m_viewMode != SmallChips) {
    QRect nameRect = getColorNameRect(index);
    if (nameRect.contains(e->pos())) {
      std::wstring styleName = style->getName();
      LineEdit *fld          = m_renameTextField;
      fld->setText(QString::fromStdWString(styleName));
      fld->setGeometry(nameRect);
      fld->show();
      fld->selectAll();
      fld->setFocus(Qt::OtherFocusReason);
      fld->setAlignment((m_viewMode == List) ? Qt::AlignLeft : Qt::AlignCenter);
      return;
    }
  }

  CommandManager::instance()->execute("MI_OpenStyleControl");
}

//-----------------------------------------------------------------------------
void PageViewer::createMenuAction(QMenu &menu, const char *id, QString name,
                                  const char *slot) {
  bool ret     = true;
  QAction *act = menu.addAction(name);
  std::string slotName(slot);
  slotName = std::string("1") + slotName;
  ret      = connect(act, SIGNAL(triggered()), slotName.c_str());
  assert(ret);
}

//-----------------------------------------------------------------------------

void PageViewer::addNewColor() {
  PaletteCmd::createStyle(getPaletteHandle(), getPage());
  computeSize();
  update();
}

//-----------------------------------------------------------------------------

void PageViewer::addNewPage() {
  PaletteCmd::addPage(getPaletteHandle());
  update();
}

//-----------------------------------------------------------------------------

/*! Create and open the Right-click menu in page.
*/
void PageViewer::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  CommandManager *cmd = CommandManager::instance();

  menu.addAction(cmd->getAction(MI_Copy));
  QAction *pasteValueAct = cmd->getAction(MI_PasteValues);
  menu.addAction(pasteValueAct);
  QAction *pasteColorsAct = cmd->getAction(MI_PasteColors);
  menu.addAction(pasteColorsAct);
  QAction *pasteNamesAct = cmd->getAction(MI_PasteNames);
  menu.addAction(pasteNamesAct);
  QAction *pasteAct = cmd->getAction(MI_Paste);
  menu.addAction(pasteAct);
  QAction *cutAct = cmd->getAction(MI_Cut);
  menu.addAction(cutAct);

  menu.addSeparator();
  QAction *clearAct = cmd->getAction(MI_Clear);
  menu.addAction(clearAct);

  menu.addSeparator();
  QAction *openPltGizmoAct = cmd->getAction("MI_OpenPltGizmo");
  menu.addAction(openPltGizmoAct);
  QAction *openStyleControlAct = cmd->getAction("MI_OpenStyleControl");
  menu.addAction(openStyleControlAct);
  QAction *openStyleNameEditorAct = menu.addAction(tr("Name Editor"));
  connect(openStyleNameEditorAct, &QAction::triggered, [&]() {
    if (!m_styleNameEditor) {
      m_styleNameEditor = new StyleNameEditor(this);
      m_styleNameEditor->setPaletteHandle(getPaletteHandle());
    }
    m_styleNameEditor->show();
    m_styleNameEditor->raise();
    m_styleNameEditor->activateWindow();
  });

  // Verifica se lo stile e' link.
  // Abilita e disabilita le voci di menu' in base a dove si e' cliccato.
  int index     = posToIndex(event->pos());
  int indexPage = m_page ? m_page->getIndex() : -1;

  bool isLocked = m_page ? m_page->getPalette()->isLocked() : false;

  // remove links from studio palette
  if (m_viewType == LEVEL_PALETTE && m_styleSelection &&
      !m_styleSelection->isEmpty() && !isLocked &&
      m_styleSelection->hasLinkedStyle()) {
    menu.addSeparator();
    QAction *toggleStyleLink = cmd->getAction("MI_ToggleLinkToStudioPalette");
    menu.addAction(toggleStyleLink);
    QAction *removeStyleLink =
        cmd->getAction("MI_RemoveReferenceToStudioPalette");
    menu.addAction(removeStyleLink);
    QAction *getBackOriginalAct =
        cmd->getAction("MI_GetColorFromStudioPalette");
    menu.addAction(getBackOriginalAct);
  }

  if (((indexPage == 0 && index > 0) || (indexPage > 0 && index >= 0)) &&
      index < getChipCount() && !isLocked) {
    if (pasteValueAct) pasteValueAct->setEnabled(true);
    if (pasteColorsAct) pasteColorsAct->setEnabled(true);

    pasteNamesAct->setEnabled(true);
    pasteAct->setEnabled(true);
    cutAct->setEnabled(true);
    clearAct->setEnabled(true);
  } else {
    if (pasteValueAct) pasteValueAct->setEnabled(false);
    if (pasteColorsAct) pasteColorsAct->setEnabled(false);

    pasteNamesAct->setEnabled(false);
    pasteAct->setEnabled(!isLocked);
    cutAct->setEnabled(false);
    clearAct->setEnabled(false);
  }

  if (m_page) {
    menu.addSeparator();
    QAction *newStyle = menu.addAction(tr("New Style"));
    connect(newStyle, SIGNAL(triggered()), SLOT(addNewColor()));
    QAction *newPage = menu.addAction(tr("New Page"));
    connect(newPage, SIGNAL(triggered()), SLOT(addNewPage()));
  }

  /*
  if (m_viewType != STUDIO_PALETTE) {
          menu.addAction(cmd->getAction(MI_EraseUnusedStyles));
  }
  */

  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------
/*! Accept drag enter event if evant data ha format \b
 * TStyleSelection::getMimeType().
*/
void PageViewer::dragEnterEvent(QDragEnterEvent *event) {
  if (!m_page) return;
  const PaletteData *paletteData =
      dynamic_cast<const PaletteData *>(event->mimeData());
  if (paletteData && paletteData->hasStyleIndeces()) {
    if ((m_viewType == CLEANUP_PALETTE &&
         !paletteData->getPalette()->isCleanupPalette()) ||
        (m_viewType == LEVEL_PALETTE &&
         paletteData->getPalette()->isCleanupPalette())) {
      event->ignore();
      return;
    }
    int index = posToIndex(event->pos());
    // non si puo' spostare qualcosa nelle prime due posizioni di pagina 0
    // (occupate da BG e FG)
    if (m_page->getIndex() == 0 && index < 2) index = 2;
    if (index < 0)
      index = 0;
    else if (index > m_page->getStyleCount())
      index             = m_page->getStyleCount();
    m_dropPositionIndex = index;
    update();
    event->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------
/*! If current page exist and index in event position is different from dropped
                index set right drop position index and accept event.
                */
void PageViewer::dragMoveEvent(QDragMoveEvent *event) {
  if (!m_page) return;
  int index = posToIndex(event->pos());
  if (index != m_dropPositionIndex) {
    if ((m_page->getStyleId(0) == 0 || m_page->getStyleId(1) == 1) && index < 2)
      index = 2;
    if (index < 0)
      index = 0;
    else if (index > m_page->getStyleCount())
      index             = m_page->getStyleCount();
    m_dropPositionIndex = index;
    update();
    event->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------
/*! If event data has correct format drop it in current drop position index.
*/
void PageViewer::dropEvent(QDropEvent *event) {
  int dstIndexInPage  = m_dropPositionIndex;
  m_dropPositionIndex = -1;
  update();
  if (!dynamic_cast<const PaletteData *>(event->mimeData())) return;
  drop(dstIndexInPage, event->mimeData());
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------
/*! Set to -1 drag position index and update view.
*/
void PageViewer::dragLeaveEvent(QDragLeaveEvent *event) {
  m_dropPositionIndex = -1;
  update();
}

//-----------------------------------------------------------------------------
/*! Start drag and drop; if current page exist set drag and drop event data.
*/
void PageViewer::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  assert(m_page);
  TPalette *palette = m_page->getPalette();
  if (!palette || !m_page || !m_styleSelection) return;
  int pageIndex = m_page->getIndex();

  if (!m_styleSelection->canHandleStyles()) return;

  PaletteData *paletteData = new PaletteData();
  paletteData->setPaletteData(palette, pageIndex,
                              m_styleSelection->getIndicesInPage());
  QDrag *drag = new QDrag(this);
  drag->setMimeData(paletteData);
  Qt::DropAction dropAction = drag->exec(Qt::MoveAction);
  if (m_dropPageCreated) {
    m_dropPageCreated = false;
    int pageIndex     = palette->getPageCount() - 1;
    if (palette->getPage(pageIndex)->getStyleCount() == 0) {
      palette->erasePage(pageIndex);
      getPaletteHandle()->notifyPaletteChanged();
    } else
      palette->setDirtyFlag(true);
  }
}

//-----------------------------------------------------------------------------

void PageViewer::keyPressEvent(QKeyEvent *e) {
  int key = e->key();

  if (key == Qt::Key_Up) {  // Row = frame precedente a quello settato
    int frameIndex = m_frameHandle->getFrameIndex() - 1;
    if (frameIndex < 0) return;
    m_frameHandle->setFrameIndex(frameIndex);
  } else if (key == Qt::Key_Down) {  // Row = frame successivo a quello settato
    int frameIndex = m_frameHandle->getFrameIndex() + 1;
    m_frameHandle->setFrameIndex(frameIndex);
  } else {
    CommandManager *cManager = CommandManager::instance();

    if (key ==
        cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_ZoomIn)))
      zoomInChip();
    else if (key ==
             cManager->getKeyFromShortcut(
                 cManager->getShortcutFromId(V_ZoomOut)))
      zoomOutChip();
    else
      e->ignore();
  }
}

//-----------------------------------------------------------------------------

void PageViewer::showEvent(QShowEvent *) {
  TPaletteHandle *paletteHandle = getPaletteHandle();
  if (!paletteHandle) return;
  connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), SLOT(update()),
          Qt::UniqueConnection);
}

//-----------------------------------------------------------------------------

void PageViewer::hideEvent(QHideEvent *) {
  TPaletteHandle *paletteHandle = getPaletteHandle();
  if (!paletteHandle) return;
  disconnect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
             SLOT(update()));
}

//-----------------------------------------------------------------------------
/*! Manage page tooltip.
*/
bool PageViewer::event(QEvent *e) {
  if (m_page && e->type() == QEvent::ToolTip) {
    QHelpEvent *helpEvent = dynamic_cast<QHelpEvent *>(e);
    QString toolTip;
    QPoint pos      = helpEvent->pos();
    int indexInPage = posToIndex(pos);
    if (0 <= indexInPage && indexInPage < m_page->getStyleCount()) {
      TColorStyle *style = m_page->getStyle(indexInPage);
      if (style) {
        int styleIndex = m_page->getStyleId(indexInPage);
        toolTip        = "#" + QString::number(styleIndex) + " " +
                  QString::fromStdWString(style->getName());

        int shortcutKey = m_page->getPalette()->getStyleShortcut(styleIndex);
        if (shortcutKey > 0)
          toolTip += QString::fromStdWString(std::wstring(L" (") +
                                             (wchar_t)shortcutKey + L")");
      }
    }
    if (toolTip != "")
      QToolTip::showText(helpEvent->globalPos(), toolTip);
    else
      QToolTip::hideText();
    e->accept();
  }
  return QFrame::event(e);
}

//-----------------------------------------------------------------------------
/*! Add chip identified by \b indexInPage to style selection if it was not
                selected; remove it from style selection otherwise. Manage CTRL
   SHIFT
                pressed case.
                */
void PageViewer::select(int indexInPage, QMouseEvent *event) {
  bool selected = false;
  assert(m_page);
  TPalette *palette = m_page->getPalette();
  int pageIndex     = m_page->getIndex();
  bool on           = true;

  bool wasSelected = m_styleSelection->isSelected(pageIndex, indexInPage);
  if (event->modifiers() == Qt::NoModifier)
    m_styleSelection->selectNone();
  else if (event->modifiers() == Qt::CTRL && wasSelected)
    on = false;
  else if (event->modifiers() == Qt::SHIFT && !m_styleSelection->isEmpty()) {
    // premuto shift. la selezione si estende fino ai piu' vicini colori
    // selezionati (prima e dopo)
    // a e' b diventeranno gli estremi della selezione
    int a = -1, b = -1, i = 0;
    for (i = 0; i < m_page->getStyleCount(); i++)
      if (m_styleSelection->isSelected(pageIndex, i)) {
        if (i < indexInPage)
          a = i;
        else if (i > indexInPage) {
          b = i;
          break;
        }
      }
    if (a >= 0 && b >= 0) {
      assert(a < indexInPage && indexInPage < b);
      a++;
      b--;
    } else if (a >= 0) {
      assert(b < 0 && a < indexInPage);
      a++;
      b = indexInPage;
    } else if (b >= 0) {
      assert(a < 0 && indexInPage < b);
      b--;
      a = indexInPage;
    } else
      a = b = indexInPage;
    for (i = a; i <= b; i++) {
      m_styleSelection->select(pageIndex, i, true);
      selected = true;
    }
  }

  bool isStyleChanged = false;
  if (on) selected    = true;
  int styleIndex      = m_page->getStyleId(indexInPage);
  if (selected) {
    setCurrentStyleIndex(styleIndex);

    ChangeStyleCommand *changeStyleCommand = getChangeStyleCommand();
    if (changeStyleCommand)
      isStyleChanged = changeStyleCommand->onStyleChanged();
  }
  if (!isStyleChanged) {
    m_styleSelection->select(pageIndex, indexInPage, on);
    m_styleSelection->makeCurrent();
    updateCommandLocks();
  }
  update();
}

//-----------------------------------------------------------------------------
/*! Compute page size in regard to chip count.
*/
void PageViewer::computeSize() {
  if (!m_page) {
    m_chipPerRow = 0;
    return;
  }
  int w          = parentWidget()->width();
  int chipCount  = getChipCount();
  QSize chipSize = getChipSize();
  m_chipPerRow   = m_viewMode == List ? 1 : (w - 8) / chipSize.width();
  if (m_chipPerRow == 0) m_chipPerRow = 1;
  int rowCount = (chipCount + m_chipPerRow - 1) / m_chipPerRow;
  setMinimumSize(w, rowCount * chipSize.height() + 10);
}

//-----------------------------------------------------------------------------
/*! If palette, referred to current page, exist and is animated set it to
                current frame.
                */
void PageViewer::onFrameChanged() {
  TPalette *palette = (m_page) ? m_page->getPalette() : 0;
  if (palette && palette->isAnimated()) update();
}

//-----------------------------------------------------------------------------
/*! Rename current style and update its view in current page.
*/
// recall from m_renameTextField
void PageViewer::onStyleRenamed() {
  m_renameTextField->hide();
  std::wstring newName = m_renameTextField->text().toStdWString();
  assert(getPaletteHandle());
  PaletteCmd::renamePaletteStyle(getPaletteHandle(), newName);
}

//-----------------------------------------------------------------------------

bool PageViewer::hasShortcut(int indexInPage) {
  if (!m_page) return false;
  if (indexInPage < 0 || indexInPage >= m_page->getStyleCount()) return false;
  int styleIndex = m_page->getStyleId(indexInPage);
  return (m_page->getPalette()->getStyleShortcut(styleIndex) >= 0);
}

//=============================================================================
/*! \class PaletteViewerGUI::PaletteTabBar
                \brief The PaletteTabBar class provides a bar with tab to show
   and manage palette
                page.

                Inherits \b QTabBar.
                This object allows to move a tab with mouseMoveEvent, rename a
   tab in
                doubleClickevent and create a new tab in drag and drop.
                */
/*!	\fn void PaletteTabBar::setPageViewer(PageViewer *pageViewer)
                Set current tab bar page to \b pageViewer.

                \fn void PaletteTabBar::tabTextChanged(int index)
                This signal is emitted when tab associated text change.

                \fn void PaletteTabBar::movePage(int srcIndex, int dstIndex)
                This signal is emitted to notify tab position change.
                */
PaletteTabBar::PaletteTabBar(QWidget *parent, bool hasPageCommand)
    : QTabBar(parent)
    , m_renameTextField(new LineEdit(this))
    , m_renameTabIndex(-1)
    , m_pageViewer(0)
    , m_hasPageCommand(hasPageCommand) {
  setObjectName("PaletteTabBar");
  setDrawBase(false);
  m_renameTextField->hide();
  connect(m_renameTextField, SIGNAL(editingFinished()), this,
          SLOT(updateTabName()));
  if (m_hasPageCommand) setAcceptDrops(true);
}

//-----------------------------------------------------------------------------
/*! Hide rename text field and recall \b QTabBar::mousePressEvent().
*/
void PaletteTabBar::mousePressEvent(QMouseEvent *event) {
  m_renameTextField->hide();
  QTabBar::mousePressEvent(event);
  m_pageViewer->getSelection()->select(currentIndex());
  m_pageViewer->getSelection()->makeCurrent();
}

//-----------------------------------------------------------------------------
/*! If left button is pressed and a tab is selected emit signal \b movePage.
                In any case recall \b QTabBar::mousePressEvent().
                */
void PaletteTabBar::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() == Qt::LeftButton &&
      event->modifiers() == Qt::ControlModifier &&
      !m_pageViewer->getPage()->getPalette()->isLocked()) {
    int srcIndex = currentIndex();
    int dstIndex = tabAt(event->pos());
    if (dstIndex >= 0 && dstIndex < count() && dstIndex != srcIndex) {
      QRect rect = tabRect(srcIndex);
      int x      = event->pos().x();
      if (x < rect.left() || x > rect.right()) {
        emit movePage(srcIndex, dstIndex);
      }
    }
  }
  QTabBar::mouseMoveEvent(event);
}

//-----------------------------------------------------------------------------
/*! Set a text field with focus in event position to edit tab name.
*/
void PaletteTabBar::mouseDoubleClickEvent(QMouseEvent *event) {
  if (!m_hasPageCommand) return;
  if (m_pageViewer->getPage()->getPalette()->isLocked()) return;
  int index = tabAt(event->pos());
  if (index < 0) return;
  m_renameTabIndex = index;
  LineEdit *fld    = m_renameTextField;
  fld->setText(tabText(index));
  fld->setGeometry(tabRect(index));
  fld->setAlignment(Qt::AlignCenter);
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------
/*! If event data is a paletteData accept drag event; otherwise return.
*/
void PaletteTabBar::dragEnterEvent(QDragEnterEvent *event) {
  if (!m_hasPageCommand) return;
  const PaletteData *paletteData =
      dynamic_cast<const PaletteData *>(event->mimeData());
  if (!paletteData) return;
  if ((m_pageViewer->getViewType() == CLEANUP_PALETTE &&
       !paletteData->getPalette()->isCleanupPalette()) ||
      (m_pageViewer->getViewType() == LEVEL_PALETTE &&
       paletteData->getPalette()->isCleanupPalette())) {
    event->ignore();
    return;
  }
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------
/*! If tab in event position exist set it as current, otherwise create a new tab
                recalling PageViewer::createDropPage().
                */
void PaletteTabBar::dragMoveEvent(QDragMoveEvent *event) {
  if (!m_hasPageCommand) return;
  const PaletteData *paletteData =
      dynamic_cast<const PaletteData *>(event->mimeData());
  if (!paletteData) return;
  if (paletteData->getPalette() == m_pageViewer->getPage()->getPalette() &&
      paletteData->hasOnlyPalette())
    return;
  int tabIndex = tabAt(event->pos());
  if (0 <= tabIndex && tabIndex < count())
    setCurrentIndex(tabIndex);
  else
    m_pageViewer->createDropPage();
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------
/*! Recall PageViewer::drop().
*/
void PaletteTabBar::dropEvent(QDropEvent *event) {
  if (!m_hasPageCommand) return;
  if (!dynamic_cast<const PaletteData *>(event->mimeData())) return;
  m_pageViewer->drop(-1, event->mimeData());
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------
/*! If current rename tab index is a valid index, set tab text to rename text
                field text; hide the rename text field and emit tabTextChanged()
   signal.
                */
void PaletteTabBar::updateTabName() {
  int index = m_renameTabIndex;
  if (index < 0) return;
  m_renameTabIndex = -1;
  if (m_renameTextField->text() != "")
    setTabText(index, m_renameTextField->text());
  m_renameTextField->hide();
  emit tabTextChanged(index);
}

//=============================================================================

/*!
  \class  PaletteViewerGUI::PaletteIconWidget

  \brief  PaletteIconWidget class provides a widget to show a palette icon.
  */

#if QT_VERSION >= 0x050500
PaletteIconWidget::PaletteIconWidget(QWidget *parent, Qt::WindowFlags flags)
#else
PaletteIconWidget::PaletteIconWidget(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent, flags), m_isOver(false) {
  setFixedSize(30, 20);
  setToolTip(QObject::tr("Palette"));
}

//-----------------------------------------------------------------------------

PaletteIconWidget::~PaletteIconWidget() {}

//-----------------------------------------------------------------------------

void PaletteIconWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);

  if (m_isOver) {
    static QPixmap dragPaletteIconPixmapOver(
        svgToPixmap(":Resources/dragpalette_over.svg"));
    p.drawPixmap(5, 1, dragPaletteIconPixmapOver);
  } else {
    static QPixmap dragPaletteIconPixmap(
        svgToPixmap(":Resources/dragpalette.svg"));
    p.drawPixmap(5, 1, dragPaletteIconPixmap);
  }
}

//-----------------------------------------------------------------------------

void PaletteIconWidget::mousePressEvent(QMouseEvent *me) {
  if (me->button() != Qt::LeftButton) {
    me->ignore();
    return;
  }

  m_mousePressPos = me->pos();
  m_dragged       = false;

  me->accept();
}

//-----------------------------------------------------------------------------

void PaletteIconWidget::mouseMoveEvent(QMouseEvent *me) {
  if ((me->pos() - m_mousePressPos).manhattanLength() > 20 && !m_dragged) {
    m_dragged = true;
    emit startDrag();
  }

  me->accept();
}

//-----------------------------------------------------------------------------

void PaletteIconWidget::enterEvent(QEvent *event) { m_isOver = true; }

//-----------------------------------------------------------------------------

void PaletteIconWidget::leaveEvent(QEvent *event) { m_isOver = false; }

//-----------------------------------------------------------------------------

void PageViewer::zoomInChip() {
  ViewMode currentView = getViewMode();
  if (currentView == List || currentView == LargeChips)
    return;
  else if (currentView == SmallChips)
    setViewMode(MediumChips);
  else if (currentView == MediumChips)
    setViewMode(LargeChips);
}
void PageViewer::zoomOutChip() {
  ViewMode currentView = getViewMode();
  if (currentView == List || currentView == SmallChips)
    return;
  else if (currentView == LargeChips)
    setViewMode(MediumChips);
  else if (currentView == MediumChips)
    setViewMode(SmallChips);
}

//-----------------------------------------------------------------------------

void PageViewer::setNameDisplayMode(NameDisplayMode mode) {
  if (m_nameDisplayMode == mode) return;
  m_nameDisplayMode = mode;

  update();
}

//-----------------------------------------------------------------------------
/*! lock the commands when the styleSelection set to current
*/
void PageViewer::updateCommandLocks() {
  if (!m_page) return;
  // iwasawa

  bool isLocked = m_page->getPalette()->isLocked();

  CommandManager *cmd = CommandManager::instance();

  cmd->getAction("MI_Paste")->setEnabled(!isLocked);
  cmd->getAction("MI_PasteValues")->setEnabled(!isLocked);
  cmd->getAction("MI_PasteColors")->setEnabled(!isLocked);
  cmd->getAction("MI_Clear")->setEnabled(!isLocked);
  cmd->getAction("MI_BlendColors")->setEnabled(!isLocked);
  cmd->getAction("MI_PasteNames")->setEnabled(!isLocked);
  cmd->getAction("MI_GetColorFromStudioPalette")->setEnabled(!isLocked);
  cmd->getAction("MI_ToggleLinkToStudioPalette")->setEnabled(!isLocked);
  cmd->getAction("MI_RemoveReferenceToStudioPalette")->setEnabled(!isLocked);
}
