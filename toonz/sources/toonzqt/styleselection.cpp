

#include "toonzqt/styleselection.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/selectioncommandids.h"
#include "styledata.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/studiopalette.h"
#include "toonz/palettecmd.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/cleanupcolorstyles.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/levelproperties.h"

// TnzCore includes
#include "tcolorstyles.h"
#include "tpixelutils.h"
#include "tundo.h"
#include "tconvert.h"

#include "../toonz/menubarcommandids.h"
#include "historytypes.h"

// Qt includes
#include <QApplication>
#include <QClipboard>
#include <QByteArray>
#include <QBuffer>

using namespace DVGui;

enum StyleType { NORMALSTYLE, STUDIOSTYLE, LINKEDSTYLE };

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void copyStylesWithoutUndo(TPalette *palette, TPaletteHandle *pltHandle,
                           int pageIndex, std::set<int> *styleIndicesInPage) {
  if (!palette || pageIndex < 0) return;
  int n = styleIndicesInPage->size();
  if (n == 0) return;
  TPalette::Page *page = palette->getPage(pageIndex);
  assert(page);
  StyleData *data = new StyleData();
  std::set<int>::iterator it;
  for (it = styleIndicesInPage->begin(); it != styleIndicesInPage->end();
       ++it) {
    int indexInPage    = *it;
    int styleId        = page->getStyleId(indexInPage);
    TColorStyle *style = page->getStyle(indexInPage);
    if (!style) continue;
    TColorStyle *newStyle = style->clone();
    data->addStyle(styleId, newStyle);
  }
  QApplication::clipboard()->setMimeData(data);
}

//-----------------------------------------------------------------------------
/*! Paste styles contained in \b data in the end of current palette page.
                Add to styleIndicesInPage pasted styles index.
*/
bool pasteStylesDataWithoutUndo(TPalette *palette, TPaletteHandle *pltHandle,
                                const StyleData *data, int indexInPage,
                                int pageIndex,
                                std::set<int> *styleIndicesInPage) {
  if (!palette) palette = pltHandle->getPalette();
  if (!palette) return false;
  // page = pagina corrente
  TPalette::Page *page = palette->getPage(pageIndex);

  if (!data) return false;

  // comincio a fare paste
  int i;
  int styleId = 0;
  for (i = 0; i < data->getStyleCount(); i++) {
    styleId            = data->getStyleIndex(i);
    TColorStyle *style = data->getStyle(i)->clone();

    // Se la palette e' di cleanup gli stili devono essere 8.
    if (palette->isCleanupPalette() && palette->getStyleInPagesCount() >= 8) {
      delete style;
      break;
    }

    // For now styles will be inserted regardless the styleId of copied styles
    // are already used in the target palette or not.
    styleId = palette->getFirstUnpagedStyle();
    if (styleId >= 0)
      palette->setStyle(styleId, style);
    else
      styleId = palette->addStyle(style);

    // check the type of the original(copied) style
    // If the original is NormalStyle
    if (style->getGlobalName() == L"") {
      // 1. If pasting normal style to level palette, do nothing
      if (palette->getGlobalName() == L"") {
      }
      // 2. If pasting normal style to studio palette, add a new link and make
      // it linkable
      else {
        std::wstring gname =
            L"-" + palette->getGlobalName() + L"-" + std::to_wstring(styleId);
        style->setGlobalName(gname);
      }
    }
    // If the original is StudioPaletteStyle
    else if (style->getOriginalName() == L"") {
      // 3. If pasting StudioPaletteStyle to level palette, set the style name
      // into the original name.
      // 4. If pasting StudioPaletteStyle to studio palette, set the style name
      // into the original name and keep the link.
      style->setOriginalName(style->getName());
    }
    // If the original is linked style, do nothing
    else {
    }

    // move in the page
    // inserisco lo stile nella pagina
    int index = indexInPage + i;
    page->insertStyle(index, styleId);
    // e lo seleziono
    if (styleIndicesInPage) styleIndicesInPage->insert(index);
  }
  if (palette == pltHandle->getPalette()) pltHandle->setStyleIndex(styleId);
  pltHandle->notifyPaletteChanged();
  pltHandle->notifyColorStyleSwitched();
  return true;
}

//-----------------------------------------------------------------------------
/*! Paste styles contained in application data after current style. Clear
   styleIndicesInPage
                and add to it pasted styles. If currentStyleIndex == -1 take
   paletteHandle current style.
*/
bool pasteStylesWithoutUndo(TPalette *palette, TPaletteHandle *pltHandle,
                            int pageIndex, std::set<int> *styleIndicesInPage) {
  if (!palette) palette = pltHandle->getPalette();
  // page = pagina corrente
  TPalette::Page *page = palette->getPage(pageIndex);
  assert(page);
  // cerco il punto di inserimento (dopo lo stile corrente)
  int currentStyleIndex = pltHandle->getStyleIndex();
  int indexInPage       = page->search(currentStyleIndex) + 1;
  const StyleData *data =
      dynamic_cast<const StyleData *>(QApplication::clipboard()->mimeData());
  if (!data) return false;

  // cancello la selezione
  if (styleIndicesInPage) styleIndicesInPage->clear();
  return pasteStylesDataWithoutUndo(palette, pltHandle, data, indexInPage,
                                    pageIndex, styleIndicesInPage);
}

//-----------------------------------------------------------------------------

void deleteStylesWithoutUndo(TPalette *palette, TPaletteHandle *pltHandle,
                             int pageIndex, std::set<int> *styleIndicesInPage,
                             int fir = 0) {
  if (!palette) palette = pltHandle->getPalette();
  int n                 = styleIndicesInPage->size();
  if (n == 0) return;
  TPalette::Page *page = palette->getPage(pageIndex);
  assert(page);
  int currentStyleIndexInPage = page->search(pltHandle->getStyleIndex());
  bool mustChangeCurrentStyle =
      currentStyleIndexInPage >= 0 &&
      styleIndicesInPage->count(currentStyleIndexInPage) > 0;
  std::set<int>::reverse_iterator it;
  for (it = styleIndicesInPage->rbegin(); it != styleIndicesInPage->rend();
       ++it) {
    int j       = *it;
    int styleId = page->getStyleId(j);
    if (styleId < 2) {
      error(QObject::tr("It is not possible to delete the style #") +
            QString::number(styleId));
    } else {
      if (styleId == pltHandle->getStyleIndex()) pltHandle->setStyleIndex(1);
      palette->setStyle(styleId, TPixel32::Red);
      page->removeStyle(j);
    }
  }
  styleIndicesInPage->clear();
  if (mustChangeCurrentStyle) {
    // ho cancellato lo stile corrente
    if (currentStyleIndexInPage < page->getStyleCount()) {
      // posso fare in modo che lo stile selezionato sia nella stessa posizione
      pltHandle->setStyleIndex(page->getStyleId(currentStyleIndexInPage));
    } else if (page->getStyleCount() > 0) {
      // almeno faccio in modo che sia nella stessa pagina
      int styleId = page->getStyleId(page->getStyleCount() - 1);
      pltHandle->setStyleIndex(styleId);
    } else {
      // seleziono lo stile #1 (che c'e' sempre). n.b. questo puo' comportare un
      // cambio pagina
      pltHandle->setStyleIndex(1);
    }
  }
  // This function is used in undo and redo. So do not activate dirtyflag.
  pltHandle->notifyColorStyleChanged(false, false);
  pltHandle->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void cutStylesWithoutUndo(TPalette *palette, TPaletteHandle *pltHandle,
                          int pageIndex, std::set<int> *styleIndicesInPage) {
  copyStylesWithoutUndo(palette, pltHandle, pageIndex, styleIndicesInPage);
  deleteStylesWithoutUndo(palette, pltHandle, pageIndex, styleIndicesInPage);
}

//-----------------------------------------------------------------------------

void insertStylesWithoutUndo(TPalette *palette, TPaletteHandle *pltHandle,
                             int pageIndex, std::set<int> *styleIndicesInPage) {
  if (!palette) palette = pltHandle->getPalette();
  if (!palette) return;
  TPalette::Page *page = palette->getPage(pageIndex);
  if (!page) return;

  const StyleData *data =
      dynamic_cast<const StyleData *>(QApplication::clipboard()->mimeData());
  if (!data) return;

  int styleId = 0;
  // comincio a fare paste
  std::set<int>::iterator it = styleIndicesInPage->begin();
  int i;
  for (i = 0; i < data->getStyleCount(); i++, it++) {
    styleId            = data->getStyleIndex(i);
    TColorStyle *style = data->getStyle(i)->clone();
    palette->setStyle(styleId, style);

    // inserisco lo stile nella pagina
    int index = *it;
    page->insertStyle(index, styleId);
  }
  if (palette == pltHandle->getPalette()) pltHandle->setStyleIndex(styleId);

  pltHandle->notifyColorStyleChanged(false);
  pltHandle->notifyPaletteChanged();
}

//=============================================================================
// PasteStylesUndo
//-----------------------------------------------------------------------------

class PasteStylesUndo final : public TUndo {
  TStyleSelection *m_selection;
  int m_oldStyleIndex;
  int m_pageIndex;
  std::set<int> m_styleIndicesInPage;
  TPaletteP m_palette;
  QMimeData *m_data;

public:
  PasteStylesUndo(TStyleSelection *selection, int oldStyleIndex,
                  QMimeData *data)
      : m_selection(selection), m_oldStyleIndex(oldStyleIndex), m_data(data) {
    m_pageIndex          = m_selection->getPageIndex();
    m_styleIndicesInPage = m_selection->getIndicesInPage();
    m_palette            = m_selection->getPaletteHandle()->getPalette();
  }

  ~PasteStylesUndo() { delete m_data; }

  void undo() const override {
    TPaletteHandle *paletteHandle    = m_selection->getPaletteHandle();
    std::set<int> styleIndicesInPage = m_styleIndicesInPage;
    cutStylesWithoutUndo(m_palette.getPointer(), paletteHandle, m_pageIndex,
                         &styleIndicesInPage);

    m_selection->selectNone();
    m_selection->makeCurrent();

    // Setto l'indice corrente.
    if (m_palette.getPointer() == paletteHandle->getPalette())
      paletteHandle->setStyleIndex(m_oldStyleIndex);
  }

  void redo() const override {
    // Se e' la paletta corrente setto l'indice corrente.
    TPaletteHandle *paletteHandle = m_selection->getPaletteHandle();
    if (m_palette.getPointer() == paletteHandle->getPalette())
      paletteHandle->setStyleIndex(m_oldStyleIndex);

    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    assert(page);
    int indexInPage       = page->search(m_oldStyleIndex) + 1;
    const StyleData *data = dynamic_cast<const StyleData *>(m_data);
    assert(data);
    std::set<int> styleIndicesInPage;
    pasteStylesDataWithoutUndo(m_palette.getPointer(), paletteHandle, data,
                               indexInPage, m_pageIndex, &styleIndicesInPage);

    // Se e' la paletta corrente aggiorno la selezione
    if (m_selection && m_palette.getPointer() == paletteHandle->getPalette()) {
      m_selection->selectNone();
      m_selection->select(m_pageIndex);
      std::set<int>::iterator it;
      for (it = styleIndicesInPage.begin(); it != styleIndicesInPage.end();
           ++it)
        m_selection->select(m_pageIndex, *it, true);
      m_selection->makeCurrent();
    }
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Paste Style  in Palette : %1")
        .arg(QString::fromStdWString(m_palette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// DeleteStylesUndo
//-----------------------------------------------------------------------------

class DeleteStylesUndo final : public TUndo {
  TStyleSelection *m_selection;
  int m_pageIndex;
  std::set<int> m_styleIndicesInPage;
  QMimeData *m_data;
  TPaletteP m_palette;

public:
  DeleteStylesUndo(TStyleSelection *selection, QMimeData *data)
      : m_selection(selection), m_data(data) {
    m_pageIndex          = m_selection->getPageIndex();
    m_styleIndicesInPage = m_selection->getIndicesInPage();
    m_palette            = m_selection->getPaletteHandle()->getPalette();
  }

  void setStyleIndicesInPage(const std::set<int> &styleIndicesInPage) {
    m_styleIndicesInPage = styleIndicesInPage;
  }

  void setPageIndex(int pageIndex) { m_pageIndex = pageIndex; }

  ~DeleteStylesUndo() { delete m_data; }

  void undo() const override {
    TPaletteHandle *paletteHandle = m_selection->getPaletteHandle();
    // Prendo il data corrente
    QClipboard *clipboard  = QApplication::clipboard();
    QMimeData *currentData = cloneData(clipboard->mimeData());
    // Setto il vecchio data
    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);

    std::set<int> styleIndicesInPage = m_styleIndicesInPage;
    insertStylesWithoutUndo(m_palette.getPointer(), paletteHandle, m_pageIndex,
                            &styleIndicesInPage);

    if (m_selection && m_palette.getPointer() == paletteHandle->getPalette()) {
      m_selection->selectNone();
      m_selection->select(m_pageIndex);
      std::set<int>::iterator it;
      for (it = styleIndicesInPage.begin(); it != styleIndicesInPage.end();
           ++it)
        m_selection->select(m_pageIndex, *it, true);

      m_selection->makeCurrent();
    }
    // Rimetto il data corrente.
    clipboard->setMimeData(currentData, QClipboard::Clipboard);
    // do not activate dirty flag here in case that the m_palette is not current
    // when undo
    paletteHandle->notifyColorStyleChanged(false, false);
  }

  void redo() const override {
    std::set<int> styleIndicesInPage = m_styleIndicesInPage;
    deleteStylesWithoutUndo(m_palette.getPointer(),
                            m_selection->getPaletteHandle(), m_pageIndex,
                            &styleIndicesInPage);
    if (m_selection) {
      m_selection->selectNone();
      m_selection->makeCurrent();
    }
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Delete Style  from Palette : %1")
        .arg(QString::fromStdWString(m_palette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// CutStylesUndo
//-----------------------------------------------------------------------------

class CutStylesUndo final : public TUndo {
  TStyleSelection *m_selection;
  int m_pageIndex;
  std::set<int> m_styleIndicesInPage;
  QMimeData *m_oldData;  //!< data before cut
  QMimeData *m_data;     //!< data containing cut styles
  TPaletteP m_palette;

public:
  CutStylesUndo(TStyleSelection *selection, QMimeData *data, QMimeData *oldData)
      : m_selection(selection), m_oldData(oldData), m_data(data) {
    m_pageIndex          = m_selection->getPageIndex();
    m_styleIndicesInPage = m_selection->getIndicesInPage();
    m_palette            = m_selection->getPaletteHandle()->getPalette();
  }

  ~CutStylesUndo() {
    delete m_oldData;
    delete m_data;
  }

  void undo() const override {
    TPaletteHandle *paletteHandle = m_selection->getPaletteHandle();

    // Setto il data del cut
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);

    std::set<int> styleIndicesInPage = m_styleIndicesInPage;
    insertStylesWithoutUndo(m_palette.getPointer(), paletteHandle, m_pageIndex,
                            &styleIndicesInPage);

    if (m_selection && m_palette.getPointer() == paletteHandle->getPalette()) {
      m_selection->selectNone();
      m_selection->select(m_pageIndex);
      std::set<int>::iterator it;
      for (it = styleIndicesInPage.begin(); it != styleIndicesInPage.end();
           ++it)
        m_selection->select(m_pageIndex, *it, true);
      m_selection->makeCurrent();
    }

    // Setto il che c'era prima del cut
    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);
  }

  void redo() const override {
    std::set<int> styleIndicesInPage = m_styleIndicesInPage;
    cutStylesWithoutUndo(m_palette.getPointer(),
                         m_selection->getPaletteHandle(), m_pageIndex,
                         &styleIndicesInPage);

    m_selection->selectNone();
    m_selection->makeCurrent();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Cut Style  from Palette : %1")
        .arg(QString::fromStdWString(m_palette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TStyleSelection
//-----------------------------------------------------------------------------

TStyleSelection::TStyleSelection()
    : m_paletteHandle(0)
    , m_xsheetHandle(0)
    , m_pageIndex(-1)
    , m_levelHandle(0) {}

//-------------------------------------------------------------------

TStyleSelection::~TStyleSelection() {}

//-----------------------------------------------------------------------------

void TStyleSelection::enableCommands() {
  if (m_paletteHandle && m_paletteHandle->getPalette() &&
      !m_paletteHandle->getPalette()->isCleanupPalette()) {
    enableCommand(this, MI_Cut, &TStyleSelection::cutStyles);
    enableCommand(this, MI_Copy, &TStyleSelection::copyStyles);
    enableCommand(this, MI_Paste, &TStyleSelection::pasteStyles);
    enableCommand(this, MI_PasteValues, &TStyleSelection::pasteStylesValue);
    enableCommand(this, MI_PasteColors, &TStyleSelection::pasteStylesColor);
    enableCommand(this, MI_PasteNames, &TStyleSelection::pasteStylesName);

    // available only for level palette
    if (m_paletteHandle->getPalette()->getGlobalName() == L"") {
      enableCommand(this, MI_GetColorFromStudioPalette,
                    &TStyleSelection::getBackOriginalStyle);
      enableCommand(this, MI_ToggleLinkToStudioPalette,
                    &TStyleSelection::toggleLink);
      enableCommand(this, MI_RemoveReferenceToStudioPalette,
                    &TStyleSelection::removeLink);
    }
  }
  enableCommand(this, MI_Clear, &TStyleSelection::deleteStyles);
  enableCommand(this, MI_EraseUnusedStyles, &TStyleSelection::eraseUnsedStyle);
  enableCommand(this, MI_BlendColors, &TStyleSelection::blendStyles);
}

//-------------------------------------------------------------------

void TStyleSelection::select(int pageIndex) {
  m_pageIndex = pageIndex;
  m_styleIndicesInPage.clear();
}

//-------------------------------------------------------------------

void TStyleSelection::select(int pageIndex, int styleIndexInPage, bool on) {
  if (on) {
    if (pageIndex != m_pageIndex) m_styleIndicesInPage.clear();
    m_pageIndex = pageIndex;
    m_styleIndicesInPage.insert(styleIndexInPage);
  } else if (pageIndex == m_pageIndex)
    m_styleIndicesInPage.erase(styleIndexInPage);
}

//-------------------------------------------------------------------

bool TStyleSelection::isSelected(int pageIndex, int id) const {
  return m_pageIndex == pageIndex &&
         m_styleIndicesInPage.find(id) != m_styleIndicesInPage.end();
}

//------------------------------------------------------------------

bool TStyleSelection::isPageSelected(int pageIndex) const {
  return m_pageIndex == pageIndex;
}

//------------------------------------------------------------------

bool TStyleSelection::canHandleStyles() {
  TPalette *palette = getPalette();
  if (!palette) return false;
  TPalette::Page *page = palette->getPage(m_pageIndex);
  if (!page) return false;
  if ((isSelected(m_pageIndex, 0) && page->getStyleId(0) == 0) ||
      (isSelected(m_pageIndex, 1) && page->getStyleId(1) == 1))
    return false;
  return true;
}

//-------------------------------------------------------------------

void TStyleSelection::selectNone() {
  m_pageIndex = -1;
  m_styleIndicesInPage.clear();
  notifyView();
}
//-------------------------------------------------------------------

bool TStyleSelection::isEmpty() const {
  return m_pageIndex < 0 && m_styleIndicesInPage.empty();
}

//-------------------------------------------------------------------

int TStyleSelection::getStyleCount() const {
  return m_styleIndicesInPage.size();
}

//-------------------------------------------------------------------

void TStyleSelection::cutStyles() {
  if (isEmpty()) return;
  QClipboard *clipboard = QApplication::clipboard();
  QMimeData *oldData    = cloneData(clipboard->mimeData());
  if (!canHandleStyles()) {
    error(QObject::tr("It is not possible to delete styles #0 and #1."));
    return;
  }
  TPalette *palette = m_paletteHandle->getPalette();
  if (!palette) return;
  if (palette->isLocked()) return;

  StyleData *data = new StyleData();
  std::set<int>::iterator it;
  TPalette::Page *page = palette->getPage(m_pageIndex);
  std::vector<int> styleIds;
  for (it = m_styleIndicesInPage.begin(); it != m_styleIndicesInPage.end();
       ++it) {
    int j       = *it;
    int styleId = page->getStyleId(j);
    if (styleId < 0) continue;
    TColorStyle *style = page->getStyle(j)->clone();
    data->addStyle(styleId, style);
    styleIds.push_back(page->getStyleId(*it));
  }

  std::unique_ptr<TUndo> undo(new CutStylesUndo(this, data, oldData));

  if (m_xsheetHandle) {
    if (eraseStylesInDemand(palette, styleIds, m_xsheetHandle) == 0) return;
  }
  palette->setDirtyFlag(true);
  cutStylesWithoutUndo(palette, m_paletteHandle, m_pageIndex,
                       &m_styleIndicesInPage);
  TUndoManager::manager()->add(undo.release());
}

//-------------------------------------------------------------------

void TStyleSelection::copyStyles() {
  if (isEmpty()) return;
  copyStylesWithoutUndo(m_paletteHandle->getPalette(), m_paletteHandle,
                        m_pageIndex, &m_styleIndicesInPage);
}

//-------------------------------------------------------------------

void TStyleSelection::pasteStyles() {
  TPalette *palette = getPalette();
  // se non c'e' palette o pagina corrente esco
  if (!palette || m_pageIndex < 0) return;
  if (palette->isLocked()) return;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  if (!page) return;
  if (isSelected(m_pageIndex, 0) && page->getStyleId(0) == 0) {
    error(QObject::tr("Can't paste styles there"));
    return;
  }

  int oldStyleIndex     = m_paletteHandle->getStyleIndex();
  QClipboard *clipboard = QApplication::clipboard();
  QMimeData *oldData    = cloneData(clipboard->mimeData());
  pasteStylesWithoutUndo(m_paletteHandle->getPalette(), m_paletteHandle,
                         m_pageIndex, &m_styleIndicesInPage);
  palette->setDirtyFlag(true);
  TUndoManager::manager()->add(
      new PasteStylesUndo(this, oldStyleIndex, oldData));
}

//-------------------------------------------------------------------

void TStyleSelection::deleteStyles() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;
  if (palette->isLocked()) return;
  if (!canHandleStyles()) {
    error(QObject::tr("It is not possible to delete styles #0 and #1."));
    return;
  }
  if (getStyleCount() == 0) return;

  StyleData *data = new StyleData();
  std::set<int>::iterator it;
  TPalette::Page *page = palette->getPage(m_pageIndex);
  std::vector<int> styleIds;
  for (it = m_styleIndicesInPage.begin(); it != m_styleIndicesInPage.end();
       ++it) {
    int j       = *it;
    int styleId = page->getStyleId(j);
    if (styleId < 0) continue;
    TColorStyle *style = page->getStyle(j)->clone();
    data->addStyle(styleId, style);
    styleIds.push_back(page->getStyleId(*it));
  }

  TUndoScopedBlock undoBlock;

  if (m_xsheetHandle) {
    if (eraseStylesInDemand(palette, styleIds, m_xsheetHandle) ==
        0)  // Could add an undo
      return;
  }

  std::unique_ptr<TUndo> undo(new DeleteStylesUndo(this, data));

  deleteStylesWithoutUndo(m_paletteHandle->getPalette(), m_paletteHandle,
                          m_pageIndex, &m_styleIndicesInPage);
  palette->setDirtyFlag(true);

  TUndoManager::manager()->add(undo.release());
}

//-------------------------------------------------------------------

void TStyleSelection::eraseUnsedStyle() {
  std::set<TXshSimpleLevel *> levels;
  int row, column, i, j;
  TPalette *palette = m_paletteHandle->getPalette();
  findPaletteLevels(levels, row, column, palette, m_xsheetHandle->getXsheet());

  // Verifico quali stili sono usati e quali no
  std::map<int, bool> usedStyleIds;
  int pageCount = palette->getPageCount();
  for (auto const level : levels) {
    std::vector<TFrameId> fids;
    level->getFids(fids);
    int m, i, j;
    for (m = 0; m < (int)fids.size(); m++) {
      TImageP image = level->getFrame(fids[m], false);
      if (!image) continue;
      for (i = 0; i < pageCount; i++) {
        TPalette::Page *page = palette->getPage(i);
        assert(page);
        for (j = 0; j < page->getStyleCount(); j++) {
          int styleId = page->getStyleId(j);
          if (m != 0 && usedStyleIds[styleId]) continue;
          if (i == 0 && j == 0)  // Il primo stile della prima pagina non deve
                                 // essere mai cancellato
          {
            usedStyleIds[styleId] = true;
            continue;
          }
          usedStyleIds[styleId] = isStyleUsed(image, styleId);
        }
      }
    }
  }

  TUndoManager::manager()->beginBlock();

  // Butto gli stili non usati
  for (i = 0; i < pageCount; i++) {
    // Variabili usate per l'undo
    std::set<int> styleIndicesInPage;
    StyleData *data      = new StyleData();
    TPalette::Page *page = palette->getPage(i);
    assert(page);
    for (j = 0; j < page->getStyleCount(); j++) {
      int styleId = page->getStyleId(j);
      if (usedStyleIds[styleId]) continue;
      styleIndicesInPage.insert(j);
      data->addStyle(styleId, page->getStyle(j)->clone());
    }
    // Se styleIndicesInPage e' vuoto ci sono stili da cancellare.
    if (styleIndicesInPage.empty()) {
      delete data;
      continue;
    }
    // Cancello gli stili
    std::set<int>::reverse_iterator it;
    for (it = styleIndicesInPage.rbegin(); it != styleIndicesInPage.rend();
         ++it)
      page->removeStyle(*it);
    // Undo
    DeleteStylesUndo *undo = new DeleteStylesUndo(this, data);
    undo->setPageIndex(i);
    undo->setStyleIndicesInPage(styleIndicesInPage);
    TUndoManager::manager()->add(undo);
  }
  TUndoManager::manager()->endBlock();
  m_paletteHandle->setStyleIndex(1);
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoPasteValues
//-----------------------------------------------------------------------------

class UndoPasteValues final : public TUndo {
  TStyleSelection *m_selection;
  TPaletteHandle *m_paletteHandle;
  int m_pageIndex;
  TPaletteP m_palette;

  bool m_pasteName;
  bool m_pasteColor;

  class Item {
  public:
    int m_index;
    TColorStyle *m_oldStyle;
    TColorStyle *m_newStyle;
    Item(int index, const TColorStyle *oldStyle, const TColorStyle *newStyle)
        : m_index(index)
        , m_oldStyle(oldStyle->clone())
        , m_newStyle(newStyle->clone()) {}
    Item(int index, const TColorStyle *newStyle)
        : m_index(index), m_oldStyle(0), m_newStyle(newStyle->clone()) {}
    ~Item() {
      delete m_oldStyle;
      delete m_newStyle;
    }
  };

  std::vector<Item *> m_items;
  std::vector<Item *> m_itemsInserted;

  TPalette *getPalette() const { return m_palette.getPointer(); }

public:
  UndoPasteValues(TStyleSelection *selection, bool pasteName,
                  bool pasteColor = true)
      : m_selection(selection)
      , m_pasteName(pasteName)
      , m_pasteColor(pasteColor) {
    m_pageIndex     = m_selection->getPageIndex();
    m_paletteHandle = m_selection->getPaletteHandle();
    m_palette       = m_paletteHandle->getPalette();
  }

  ~UndoPasteValues() {
    clearPointerContainer(m_items);
    clearPointerContainer(m_itemsInserted);
  }

  void addItem(int index, const TColorStyle *oldStyle,
               const TColorStyle *newStyle) {
    m_items.push_back(new Item(index, oldStyle, newStyle));
  }

  void addItemToInsert(const std::set<int> styleIndicesInPage) {
    TPalette::Page *page = getPalette()->getPage(m_pageIndex);
    std::set<int>::const_iterator it;
    for (it = styleIndicesInPage.begin(); it != styleIndicesInPage.end(); it++)
      m_itemsInserted.push_back(new Item(*it, page->getStyle(*it)));
  }

  void pasteValue(int styleId, const TColorStyle *newStyle) const {
    // preserva il vecchio nome se m_pasteOnlyColor e' falso
    std::wstring str = newStyle->getName();
    if (m_pasteColor) {
      getPalette()->setStyle(styleId, newStyle->clone());
      if (!m_pasteName) getPalette()->getStyle(styleId)->setName(str);
    } else if (m_pasteName)
      getPalette()->getStyle(styleId)->setName(newStyle->getName());
  }

  void undo() const override {
    m_selection->selectNone();
    TPalette::Page *page = getPalette()->getPage(m_pageIndex);

    int i;
    for (i = 0; i < (int)m_items.size(); i++) {
      int indexInPage = m_items[i]->m_index;
      int styleId     = page->getStyleId(indexInPage);
      pasteValue(styleId, m_items[i]->m_oldStyle);
      m_selection->select(m_pageIndex, indexInPage, true);
    }

    // Se avevo aggiunto degli stili devo rimuoverli.
    int j;
    for (j = (int)m_itemsInserted.size() - 1; j >= 0; j--) {
      int indexInPage = m_itemsInserted[j]->m_index;
      int styleId     = page->getStyleId(indexInPage);
      if (getPalette() == m_paletteHandle->getPalette() &&
          styleId == m_paletteHandle->getStyleIndex())
        m_paletteHandle->setStyleIndex(page->getStyleId(indexInPage - 1));
      getPalette()->setStyle(styleId, TPixel32::Red);
      page->removeStyle(indexInPage);
    }

    m_selection->makeCurrent();
    // do not activate a dirty flag here in case that m_palette is not
    // currentpalette
    m_paletteHandle->notifyColorStyleChanged(false, false);
    m_paletteHandle->notifyColorStyleSwitched();
  }

  void redo() const override {
    m_selection->selectNone();

    TPalette::Page *page = getPalette()->getPage(m_pageIndex);

    int i;
    int indexInPage = 0;
    for (i = 0; i < (int)m_items.size(); i++) {
      std::wstring oldName = m_items[i]->m_oldStyle->getName();
      TColorStyle *style   = m_items[i]->m_newStyle;
      indexInPage          = m_items[i]->m_index;
      int styleId          = page->getStyleId(indexInPage);

      // first, check out the types of copied and pasted styles
      StyleType srcType, dstType;
      // copied (source)
      if (style->getGlobalName() == L"")
        srcType = NORMALSTYLE;
      else if (style->getOriginalName() == L"")
        srcType = STUDIOSTYLE;
      else
        srcType = LINKEDSTYLE;
      // pasted (destination)
      TColorStyle *dstStyle        = m_items[i]->m_oldStyle;
      std::wstring dstGlobalName   = dstStyle->getGlobalName();
      std::wstring dstOriginalName = dstStyle->getOriginalName();
      if (dstGlobalName == L"")
        dstType = NORMALSTYLE;
      else if (dstOriginalName == L"")
        dstType = STUDIOSTYLE;
      else
        dstType = LINKEDSTYLE;

      pasteValue(styleId, style);

      // Process the global and original names according to the pasted style
      TColorStyle *pastedStyle = getPalette()->getStyle(styleId);
      if (srcType == NORMALSTYLE) {
        if (dstType == NORMALSTYLE) {
        }  // 1. From normal to normal. Do nothing.
        else if (dstType == STUDIOSTYLE)  // 2. From normal to studio. Restore
                                          // the global name
          pastedStyle->setGlobalName(dstGlobalName);
        else  // dstType == LINKEDSTYLE		//3. From normal to linked.
              // Restore both the global and the original name. Activate the
              // edited flag.
        {
          pastedStyle->setGlobalName(dstGlobalName);
          pastedStyle->setOriginalName(dstOriginalName);
          pastedStyle->setIsEditedFlag(true);
        }
      } else if (srcType == STUDIOSTYLE) {
        if (dstType == NORMALSTYLE)  // 4. From studio to normal. Set the studio
                                     // style's name to the original name.
          pastedStyle->setOriginalName(style->getName());
        else if (dstType == STUDIOSTYLE)  // 5. From studio to studio. Restore
                                          // the global name.
          pastedStyle->setGlobalName(dstGlobalName);
        else  // dstStyle == LINKEDSTYLE		//6. From studio to
              // linked. Set the studio style's name to the original name, and
              // set the edited flag to off.
        {
          pastedStyle->setOriginalName(style->getName());
          pastedStyle->setIsEditedFlag(false);
        }
      } else  // srcType == LINKEDSTYLE
      {
        if (dstType == NORMALSTYLE) {
        }  // 7. From linked to normal. Do nothing.
        else if (dstType == STUDIOSTYLE)  // 8. From linked to studio. Restore
                                          // the global name. Delete the
                                          // original name. Set the edited flag
                                          // to off.
        {
          pastedStyle->setGlobalName(dstGlobalName);
          pastedStyle->setOriginalName(L"");
          pastedStyle->setIsEditedFlag(false);
        } else  // dstStyle == LINKEDSTYLE		//9. From linked to
                // linked.
                // Do nothing (bring all information from the original).
        {
        }
      }

      if (!m_pasteName)  // devo settare al nuovo stile il vecchio nome
        getPalette()->getStyle(styleId)->setName(oldName);
      m_selection->select(m_pageIndex, indexInPage, true);
    }

    if (m_itemsInserted.size() != 0) {
      StyleData *newData = new StyleData();
      int j;
      for (j = 0; j < (int)m_itemsInserted.size(); j++)
        newData->addStyle(m_itemsInserted[j]->m_index,
                          m_itemsInserted[j]->m_newStyle->clone());

      std::set<int> styleIndicesInPage;
      pasteStylesDataWithoutUndo(getPalette(), m_paletteHandle, newData,
                                 indexInPage + 1, m_pageIndex,
                                 &styleIndicesInPage);

      std::set<int>::iterator styleIt;
      for (styleIt = styleIndicesInPage.begin();
           styleIt != styleIndicesInPage.end(); ++styleIt)
        m_selection->select(m_pageIndex, *styleIt, true);

      delete newData;
    }

    m_selection->makeCurrent();
    // do not activate a dirty flag here in case that m_palette is not
    // currentpalette
    m_paletteHandle->notifyColorStyleChanged(false, false);
    m_paletteHandle->notifyColorStyleSwitched();
  }

  int getSize() const override {
    return sizeof(*this) + (int)m_items.size() * 100;  // forfait
  }

  QString getHistoryString() override {
    QString palNameStr =
        QObject::tr("  to Palette : %1")
            .arg(QString::fromStdWString(m_palette->getPaletteName()));
    if (m_pasteName && m_pasteColor)
      return QObject::tr("Paste Color && Name%1").arg(palNameStr);
    else if (m_pasteName && !m_pasteColor)
      return QObject::tr("Paste Name%1").arg(palNameStr);
    else if (!m_pasteName && m_pasteColor)
      return QObject::tr("Paste Color%1").arg(palNameStr);
    else
      return QObject::tr("Paste%1").arg(palNameStr);
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void TStyleSelection::pasteStylesValues(bool pasteName, bool pasteColor) {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;
  if (palette->isLocked()) return;
  TPalette::Page *page = palette->getPage(m_pageIndex);
  if (!page) return;
  if (isSelected(m_pageIndex, 0) && page->getStyleId(0) == 0) {
    error(QObject::tr("Can't modify color #0"));
    return;
  }

  const StyleData *data =
      dynamic_cast<const StyleData *>(QApplication::clipboard()->mimeData());
  if (!data) return;

  int dataStyleCount = data->getStyleCount();
  if (dataStyleCount > (int)m_styleIndicesInPage.size()) {
    QString question = QObject::tr(
        "There are more cut/copied styles than selected. Paste anyway (adding "
        "styles)?");
    int ret =
        DVGui::MsgBox(question, QObject::tr("Paste"), QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return;
  }
  int i                 = 0;
  UndoPasteValues *undo = new UndoPasteValues(this, pasteName, pasteColor);
  std::set<int>::iterator it;
  int indexInPage = 0;
  for (it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end() && i < data->getStyleCount();
       ++it, i++) {
    if (page->getStyleCount() < i) break;
    indexInPage = *it;
    int styleId = page->getStyleId(indexInPage);
    undo->addItem(indexInPage, palette->getStyle(styleId), data->getStyle(i));
    TColorStyle *colorStyle = page->getStyle(indexInPage);
    std::wstring styleName  = colorStyle->getName();
    unsigned int flags      = colorStyle->getFlags();

    if (pasteColor) {
      // first, check out the types of copied and pasted styles
      StyleType srcType, dstType;
      // copied (source)
      if (data->getStyle(i)->getGlobalName() == L"")
        srcType = NORMALSTYLE;
      else if (data->getStyle(i)->getOriginalName() == L"")
        srcType = STUDIOSTYLE;
      else
        srcType = LINKEDSTYLE;
      // pasted (destination)
      TColorStyle *dstStyle        = page->getStyle(indexInPage);
      std::wstring dstGlobalName   = dstStyle->getGlobalName();
      std::wstring dstOriginalName = dstStyle->getOriginalName();
      if (dstGlobalName == L"")
        dstType = NORMALSTYLE;
      else if (dstOriginalName == L"")
        dstType = STUDIOSTYLE;
      else
        dstType = LINKEDSTYLE;
      //---- 追加ここまで iwasawa

      // For cleanup styles, do not duplicate the cleanup information. Just
      // paste color information.
      TCleanupStyle *cleanupStyle =
          dynamic_cast<TCleanupStyle *>(data->getStyle(i));
      if (cleanupStyle && !palette->isCleanupPalette())
        palette->setStyle(styleId, cleanupStyle->getMainColor());
      else
        palette->setStyle(styleId, data->getStyle(i)->clone());

      // Process the global and original names according to the pasted style
      TColorStyle *pastedStyle = getPalette()->getStyle(styleId);
      if (srcType == NORMALSTYLE) {
        if (dstType == NORMALSTYLE) {
        }  // 1. From normal to normal. Do nothing.
        else if (dstType == STUDIOSTYLE)  // 2. From normal to studio. Restore
                                          // the global name
          pastedStyle->setGlobalName(dstGlobalName);
        else  // dstType == LINKEDSTYLE		//3. From normal to linked.
              // Restore both the global and the original name. Activate the
              // edited flag.
        {
          pastedStyle->setGlobalName(dstGlobalName);
          pastedStyle->setOriginalName(dstOriginalName);
          pastedStyle->setIsEditedFlag(true);
        }
      } else if (srcType == STUDIOSTYLE) {
        if (dstType == NORMALSTYLE)  // 4. From studio to normal. Set the studio
                                     // style's name to the original name.
          pastedStyle->setOriginalName(data->getStyle(i)->getName());
        else if (dstType == STUDIOSTYLE)  // 5. From studio to studio. Restore
                                          // the global name.
          pastedStyle->setGlobalName(dstGlobalName);
        else  // dstStyle == LINKEDSTYLE		//6. From studio to
              // linked. Set the studio style's name to the original name, and
              // set the edited flag to off.
        {
          pastedStyle->setOriginalName(data->getStyle(i)->getName());
          pastedStyle->setIsEditedFlag(false);
        }
      } else  // srcType == LINKEDSTYLE
      {
        if (dstType == NORMALSTYLE) {
        }  // 7. From linked to normal. Do nothing.
        else if (dstType == STUDIOSTYLE)  // 8. From linked to studio. Restore
                                          // the global name. Delete the
                                          // original name. Set the edited flag
                                          // to off.
        {
          pastedStyle->setGlobalName(dstGlobalName);
          pastedStyle->setOriginalName(L"");
          pastedStyle->setIsEditedFlag(false);
        } else  // dstStyle == LINKEDSTYLE		//9. From linked to
                // linked.
                // Do nothing (bring all information from the original).
        {
        }
      }

      // put back the name when "paste color"
      if (!pasteName) {
        page->getStyle(indexInPage)->setName(styleName);
        page->getStyle(indexInPage)->setFlags(flags);
      }
    }
    // when "paste name"
    if (pasteName)
      page->getStyle(indexInPage)->setName(data->getStyle(i)->getName());
  }

  // Se gli stili del data sono piu' di quelli selezionati faccio un paste degli
  // stili che restano,
  // inserisco i nuovi stili dopo indexInPage.
  if (i < dataStyleCount) {
    StyleData *newData = new StyleData();
    int j;
    for (j = i; j < dataStyleCount; j++)
      newData->addStyle(data->getStyleIndex(j), data->getStyle(j)->clone());

    std::set<int> styleIndicesInPage;
    pasteStylesDataWithoutUndo(m_paletteHandle->getPalette(), m_paletteHandle,
                               newData, indexInPage + 1, m_pageIndex,
                               &styleIndicesInPage);
    undo->addItemToInsert(styleIndicesInPage);

    std::set<int>::iterator styleIt;
    for (styleIt = styleIndicesInPage.begin();
         styleIt != styleIndicesInPage.end(); ++styleIt)
      m_styleIndicesInPage.insert(*styleIt);

    delete newData;
  }
  TUndoManager::manager()->add(undo);

  // clear cache(invalidate) of the level
  if (m_levelHandle && m_levelHandle->getSimpleLevel() &&
      !m_levelHandle->getSimpleLevel()->getProperties()->getDirtyFlag())
    m_levelHandle->getSimpleLevel()->invalidateFrames();

  m_paletteHandle->notifyColorStyleChanged(false);
  m_paletteHandle->notifyColorStyleSwitched();

  // this is to redraw the style editor with colorStyleSwitched() signal
  m_paletteHandle->setStyleIndex(m_paletteHandle->getStyleIndex());
  palette->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void TStyleSelection::pasteStylesValue() { pasteStylesValues(true, true); }

//-----------------------------------------------------------------------------

void TStyleSelection::pasteStylesColor() { pasteStylesValues(false, true); }

//-----------------------------------------------------------------------------

void TStyleSelection::pasteStylesName() { pasteStylesValues(true, false); }

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoBlendColor
//-----------------------------------------------------------------------------

class UndoBlendColor final : public TUndo {
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_palette;
  int m_pageIndex;
  std::vector<std::pair<int, TColorStyle *>> m_colorStyles;
  TPixel32 m_c0, m_c1;

public:
  UndoBlendColor(TPaletteHandle *paletteHandle, int pageIndex,
                 std::vector<std::pair<int, TColorStyle *>> colorStyles,
                 const TPixel32 &c0, const TPixel32 &c1)
      : m_paletteHandle(paletteHandle)
      , m_pageIndex(pageIndex)
      , m_colorStyles(colorStyles)
      , m_c0(c0)
      , m_c1(c1) {
    m_palette = m_paletteHandle->getPalette();
  }

  ~UndoBlendColor() {}

  void undo() const override {
    if (!m_palette) return;
    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    if (!page) return;
    int count = 0;

    for (UINT i = 0; i < m_colorStyles.size(); i++) {
      TColorStyle *st = page->getStyle(m_colorStyles[i].first);
      QString gname   = QString::fromStdWString(st->getGlobalName());
      if (!gname.isEmpty() && gname[0] != L'-') continue;
      m_palette->setStyle(page->getStyleId(m_colorStyles[i].first),
                          m_colorStyles[i].second->clone());
      m_colorStyles[i].second->invalidateIcon();
    }
    // do not activate a dirty flag here in case that m_palette is not
    // currentpalette
    m_paletteHandle->notifyColorStyleChanged(false, false);
    m_paletteHandle->notifyColorStyleSwitched();
  }

  void redo() const override {
    if (!m_palette) return;
    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    if (!page) return;
    for (UINT i = 0; i < m_colorStyles.size(); i++) {
      TColorStyle *cs = page->getStyle(m_colorStyles[i].first);
      QString gname   = QString::fromStdWString(cs->getGlobalName());
      if (!gname.isEmpty() && gname[0] != L'-') continue;
      assert(cs);
      if (!cs) continue;
      cs->setMainColor(
          blend(m_c0, m_c1, (double)i / (double)(m_colorStyles.size() - 1)));
      cs->invalidateIcon();
    }
    // do not activate a dirty flag here in case that m_palette is not
    // currentpalette
    m_paletteHandle->notifyColorStyleChanged(false, false);
    m_paletteHandle->notifyColorStyleSwitched();
  }

  int getSize() const override {
    return sizeof(
        *this);  //+500*m_oldStyles.size(); //dipende da che stile ha salvato
  }

  QString getHistoryString() override {
    return QObject::tr("Blend Colors  in Palette : %1")
        .arg(QString::fromStdWString(m_palette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void TStyleSelection::blendStyles() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;

  if (palette->isLocked()) return;

  int n = m_styleIndicesInPage.size();
  if (n < 2) return;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);

  std::vector<TColorStyle *> styles;
  std::vector<std::pair<int, TColorStyle *>> oldStyles;
  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    TColorStyle *cs = page->getStyle(*it);
    assert(cs);
    styles.push_back(cs);
    oldStyles.push_back(std::pair<int, TColorStyle *>(*it, cs->clone()));
  }
  assert(styles.size() >= 2);

  TPixel32 c0 = styles.front()->getMainColor();
  TPixel32 c1 = styles.back()->getMainColor();

  bool areAllStyleLincked = true;
  int i;
  for (i = 1; i < n - 1; i++) {
    QString gname = QString::fromStdWString(styles[i]->getGlobalName());
    if (!gname.isEmpty() && gname[0] != L'-') continue;
    areAllStyleLincked = false;
    styles[i]->setMainColor(blend(c0, c1, (double)i / (double)(n - 1)));
    styles[i]->invalidateIcon();
  }
  if (areAllStyleLincked) return;
  m_paletteHandle->notifyColorStyleChanged(false);
  m_paletteHandle->notifyColorStyleSwitched();

  UndoBlendColor *undo =
      new UndoBlendColor(m_paletteHandle, m_pageIndex, oldStyles, c0, c1);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoLinkToStudioPalette
//-----------------------------------------------------------------------------

class UndoLinkToStudioPalette final : public TUndo {
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_palette;
  int m_pageIndex;
  struct ColorStyleData {
    int m_indexInPage;
    TColorStyle *m_oldStyle;
    std::wstring m_newName;
  };
  std::vector<ColorStyleData> m_styles;
  bool m_updateLinkedColors;

public:
  UndoLinkToStudioPalette(TPaletteHandle *paletteHandle, int pageIndex)
      : m_paletteHandle(paletteHandle)
      , m_pageIndex(pageIndex)
      , m_updateLinkedColors(false) {
    m_palette = m_paletteHandle->getPalette();
  }

  ~UndoLinkToStudioPalette() {}

  void setUpdateLinkedColors(bool updateLinkedColors) {
    m_updateLinkedColors = updateLinkedColors;
  }

  void setColorStyle(int indexInPage, TColorStyle *oldStyle,
                     std::wstring newName) {
    ColorStyleData data;
    data.m_indexInPage = indexInPage;
    data.m_newName     = newName;
    data.m_oldStyle    = oldStyle;
    m_styles.push_back(data);
  }

  void undo() const override {
    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    assert(page);
    int i;
    for (i = 0; i < (int)m_styles.size(); i++) {
      ColorStyleData data   = m_styles[i];
      int styleId           = page->getStyleId(m_styles[i].m_indexInPage);
      TColorStyle *oldStyle = m_styles[i].m_oldStyle;
      m_palette->setStyle(styleId, oldStyle->clone());
      m_palette->getStyle(styleId)->assignNames(oldStyle);
    }
    m_paletteHandle->notifyColorStyleChanged(false, false);
    m_paletteHandle->notifyColorStyleSwitched();
  }

  void redo() const override {
    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    assert(page);
    int i;
    for (i = 0; i < (int)m_styles.size(); i++) {
      ColorStyleData data = m_styles[i];
      TColorStyle *cs     = page->getStyle(data.m_indexInPage);
      cs->setGlobalName(data.m_newName);
    }
    m_paletteHandle->notifyColorStyleChanged(false, false);
    m_paletteHandle->notifyColorStyleSwitched();
    if (m_updateLinkedColors)
      StudioPalette::instance()->updateLinkedColors(m_palette.getPointer());
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Toggle Link  in Palette : %1")
        .arg(QString::fromStdWString(m_palette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void TStyleSelection::toggleLink() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;
  if (isEmpty() || palette->isLocked()) return;
  int n = m_styleIndicesInPage.size();
  if (n <= 0) return;

  std::vector<std::pair<int, std::wstring>> oldColorStylesLinked;
  std::vector<std::pair<int, std::wstring>> newColorStylesLinked;

  bool somethingHasBeenLinked    = false;
  bool somethingChanged          = false;
  bool currentStyleIsInSelection = false;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);

  UndoLinkToStudioPalette *undo =
      new UndoLinkToStudioPalette(m_paletteHandle, m_pageIndex);

  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    int index       = *it;
    TColorStyle *cs = page->getStyle(index);
    assert(cs);
    std::wstring name  = cs->getGlobalName();
    TColorStyle *oldCs = cs->clone();
    if (name != L"" && (name[0] == L'-' || name[0] == L'+')) {
      name[0] = name[0] == L'-' ? L'+' : L'-';
      cs->setGlobalName(name);
      if (name[0] == L'+') somethingHasBeenLinked = true;
      somethingChanged                            = true;
    }
    undo->setColorStyle(index, oldCs, name);

    if (*it ==
        palette->getPage(m_pageIndex)->search(m_paletteHandle->getStyleIndex()))
      currentStyleIsInSelection = true;
  }

  // if nothing changed, do not set dirty flag, nor register undo
  if (!somethingChanged) {
    delete undo;
    return;
  }

  if (somethingHasBeenLinked)
    StudioPalette::instance()->updateLinkedColors(palette);

  m_paletteHandle->notifyColorStyleChanged(false);

  if (currentStyleIsInSelection) m_paletteHandle->notifyColorStyleSwitched();

  palette->setDirtyFlag(true);

  undo->setUpdateLinkedColors(somethingHasBeenLinked);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void TStyleSelection::eraseToggleLink() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;
  if (isEmpty() || palette->isLocked()) return;
  int n = m_styleIndicesInPage.size();
  if (n <= 0) return;

  bool currentStyleIsInSelection = false;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);

  std::vector<std::pair<int, std::wstring>> oldColorStylesLinked;
  std::vector<std::pair<int, std::wstring>> newColorStylesLinked;

  UndoLinkToStudioPalette *undo =
      new UndoLinkToStudioPalette(m_paletteHandle, m_pageIndex);

  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    int index       = *it;
    TColorStyle *cs = page->getStyle(index);
    assert(cs);
    TColorStyle *oldCs = cs->clone();
    std::wstring name  = cs->getGlobalName();
    if (name != L"" && (name[0] == L'-' || name[0] == L'+'))
      cs->setGlobalName(L"");
    undo->setColorStyle(index, oldCs, L"");
    if (*it ==
        palette->getPage(m_pageIndex)->search(m_paletteHandle->getStyleIndex()))
      currentStyleIsInSelection = true;
  }

  m_paletteHandle->notifyColorStyleChanged(false);

  if (currentStyleIsInSelection) m_paletteHandle->notifyColorStyleSwitched();

  palette->setDirtyFlag(true);

  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void TStyleSelection::toggleKeyframe(int frame) {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;
  int n                = m_styleIndicesInPage.size();
  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);
  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    int styleId = page->getStyleId(*it);
    palette->setKeyframe(styleId, frame);
  }
}

//-----------------------------------------------------------------------------

class UndoRemoveLink final : public TUndo {
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_palette;
  int m_pageIndex;
  struct ColorStyleData {
    int m_indexInPage;
    std::wstring m_oldGlobalName;
    std::wstring m_oldOriginalName;
    bool m_oldEdittedFlag;
  };
  std::vector<ColorStyleData> m_styles;

public:
  UndoRemoveLink(TPaletteHandle *paletteHandle, int pageIndex)
      : m_paletteHandle(paletteHandle), m_pageIndex(pageIndex) {
    m_palette = m_paletteHandle->getPalette();
  }

  ~UndoRemoveLink() {}

  void setColorStyle(int indexInPage, TColorStyle *oldStyle) {
    ColorStyleData data;
    data.m_indexInPage     = indexInPage;
    data.m_oldGlobalName   = oldStyle->getGlobalName();
    data.m_oldOriginalName = oldStyle->getOriginalName();
    data.m_oldEdittedFlag  = oldStyle->getIsEditedFlag();
    m_styles.push_back(data);
  }

  void undo() const override {
    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    assert(page);
    int i;
    for (i = 0; i < (int)m_styles.size(); i++) {
      ColorStyleData data = m_styles[i];
      int styleId         = page->getStyleId(m_styles[i].m_indexInPage);
      m_palette->getStyle(styleId)->setGlobalName(data.m_oldGlobalName);
      m_palette->getStyle(styleId)->setOriginalName(data.m_oldOriginalName);
      m_palette->getStyle(styleId)->setIsEditedFlag(data.m_oldEdittedFlag);
    }
    m_paletteHandle->notifyColorStyleChanged(false, false);
  }

  void redo() const override {
    TPalette::Page *page = m_palette->getPage(m_pageIndex);
    assert(page);
    int i;
    for (i = 0; i < (int)m_styles.size(); i++) {
      ColorStyleData data = m_styles[i];
      TColorStyle *cs     = page->getStyle(data.m_indexInPage);
      cs->setGlobalName(L"");
      cs->setOriginalName(L"");
      cs->setIsEditedFlag(false);
    }
    m_paletteHandle->notifyColorStyleChanged(false, false);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Remove Reference  in Palette : %1")
        .arg(QString::fromStdWString(m_palette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-----------------------------------------------------------------------------
/*! remove link from studio palette. Delete the global and the orginal names.
 * return true if something changed
*/
void TStyleSelection::removeLink() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;
  int n = m_styleIndicesInPage.size();
  if (n <= 0) return;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);

  bool somethingChanged = false;

  UndoRemoveLink *undo = new UndoRemoveLink(m_paletteHandle, m_pageIndex);

  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    TColorStyle *cs = page->getStyle(*it);
    assert(cs);

    if (cs->getGlobalName() != L"" || cs->getOriginalName() != L"") {
      undo->setColorStyle(*it, cs);

      cs->setGlobalName(L"");
      cs->setOriginalName(L"");
      cs->setIsEditedFlag(false);
      somethingChanged = true;
    }
  }

  if (somethingChanged) {
    m_paletteHandle->notifyColorStyleChanged(false);
    TUndoManager::manager()->add(undo);
  } else
    delete undo;
}

//=============================================================================
class getBackOriginalStyleUndo final : public TUndo {
  TStyleSelection m_selection;
  std::vector<TPixel32> m_oldColors, m_newColors;

  // remember the edited flags
  std::vector<bool> m_oldEditedFlags, m_newEditedFlags;

public:
  getBackOriginalStyleUndo(const TStyleSelection &selection)
      : m_selection(selection) {
    getColors(m_oldColors, m_oldEditedFlags);
  }

  ~getBackOriginalStyleUndo() {}

  void getStyles(std::vector<TColorStyle *> &styles,
                 const TStyleSelection &selection) const {
    styles.clear();
    int pageIndex        = selection.getPageIndex();
    TPaletteP palette    = selection.getPalette();
    TPalette::Page *page = palette->getPage(pageIndex);
    if (!page) return;
    std::set<int> indices = selection.getIndicesInPage();
    // non si puo' modificare il BG
    if (pageIndex == 0) indices.erase(0);
    styles.reserve(indices.size());
    for (std::set<int>::iterator it = indices.begin(); it != indices.end();
         ++it)
      styles.push_back(page->getStyle(*it));
  }

  int getSize() const override {
    return sizeof *this +
           (m_oldColors.size() + m_newColors.size()) * sizeof(TPixel32);
  }

  void onAdd() override { getColors(m_newColors, m_newEditedFlags); }

  void getColors(std::vector<TPixel32> &colors,
                 std::vector<bool> &flags) const {
    std::vector<TColorStyle *> styles;
    getStyles(styles, m_selection);
    colors.resize(styles.size());
    flags.resize(styles.size());
    for (int i = 0; i < (int)styles.size(); i++) {
      colors[i] = styles[i]->getMainColor();
      flags[i]  = styles[i]->getIsEditedFlag();
    }
  }

  void setColors(const std::vector<TPixel32> &colors,
                 const std::vector<bool> &flags) const {
    std::vector<TColorStyle *> styles;
    getStyles(styles, m_selection);
    int n = std::min(styles.size(), colors.size());
    for (int i = 0; i < n; i++) {
      QString gname = QString::fromStdWString(styles[i]->getGlobalName());
      if (!gname.isEmpty() && gname[0] != L'-') continue;
      styles[i]->setMainColor(colors[i]);
      styles[i]->setIsEditedFlag(flags[i]);
      styles[i]->invalidateIcon();
    }

    m_selection.getPaletteHandle()->notifyColorStyleChanged(false, false);
  }

  void undo() const override { setColors(m_oldColors, m_oldEditedFlags); }

  void redo() const override { setColors(m_newColors, m_newEditedFlags); }

  QString getHistoryString() override {
    return QObject::tr("Get Color from Studio Palette");
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-----------------------------------------------------------------------------
/*! get the color from the linked style of the studio palette
*/
void TStyleSelection::getBackOriginalStyle() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0) return;

  if (isEmpty() || palette->isLocked()) return;  // 110804 iwasawa

  int n = m_styleIndicesInPage.size();
  if (n <= 0) return;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);

  bool somethingChanged = false;

  std::map<std::wstring, TPaletteP> table;

  TUndo *undo = new getBackOriginalStyleUndo(this);

  // for each selected style
  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    TColorStyle *cs = page->getStyle(*it);
    assert(cs);
    std::wstring gname = cs->getGlobalName();

    // if the style has no link
    if (gname == L"") continue;

    // Find the palette from the table
    int k = gname.find_first_of(L'-', 1);
    if (k == (int)std::wstring::npos) continue;

    std::wstring paletteId = gname.substr(1, k - 1);

    std::map<std::wstring, TPaletteP>::iterator palIt;
    palIt = table.find(paletteId);

    TPalette *spPalette = 0;

    // If not found in the table, then search for a new studio palette.
    if (palIt == table.end()) {
      spPalette = StudioPalette::instance()->getPalette(paletteId);
      if (!spPalette) continue;
      table[paletteId] = spPalette;
      assert(spPalette->getRefCount() == 1);
    } else
      spPalette = palIt->second.getPointer();

    // j is StudioPaletteID
    int j = std::stoi(gname.substr(k + 1));

    if (spPalette && 0 <= j && j < spPalette->getStyleCount()) {
      TColorStyle *spStyle = spPalette->getStyle(j);
      assert(spStyle);

      // edit flag is also copied here
      spStyle = spStyle->clone();
      spStyle->setGlobalName(gname);

      // get the original name from the studio palette
      spStyle->setOriginalName(spStyle->getName());
      // do not change the style name
      spStyle->setName(cs->getName());

      palette->setStyle(page->getStyleId(*it), spStyle);

      somethingChanged = true;
    }
  }

  if (somethingChanged) {
    palette->setDirtyFlag(true);
    TUndoManager::manager()->add(undo);

    m_paletteHandle->notifyColorStyleChanged(false);
    m_paletteHandle->notifyColorStyleSwitched();
  } else
    delete undo;
}

//-----------------------------------------------------------------------------
/*! return true if there is at least one linked style in the selection
*/

bool TStyleSelection::hasLinkedStyle() {
  TPalette *palette = getPalette();
  if (!palette || m_pageIndex < 0 || isEmpty()) return false;
  if (m_styleIndicesInPage.size() <= 0) return false;

  TPalette::Page *page = palette->getPage(m_pageIndex);
  assert(page);

  // for each selected style
  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    TColorStyle *cs    = page->getStyle(*it);
    std::wstring gname = cs->getGlobalName();
    // if the style has link, return true
    if (gname != L"" && (gname[0] == L'-' || gname[0] == L'+')) return true;
  }
  return false;
}
