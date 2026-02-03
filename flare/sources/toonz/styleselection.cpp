

#include "styleselection.h"
#include "menubarcommandids.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "tapp.h"
#include "toonzutil.h"

#include "tcolorstyles.h"
#include "tpixelutils.h"
#include "styledata.h"

#include "tundo.h"
#include "toonzqt/gutil.h"
#include "tconvert.h"

#include "toonzqt/dvdialog.h"

#include "toonz/studiopalette.h"

#include <QApplication>
#include <QClipboard>
#include <QByteArray>
#include <QBuffer>

using namespace DVGui;

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void copyStylesWithoutUndo(TPaletteP palette, int pageIndex,
                           std::set<int> *styleIndicesInPage) {
  if (!palette || pageIndex < 0) return;
  int n = styleIndicesInPage->size();
  if (n == 0) return;
  TPalette::Page *page = palette->getPage(pageIndex);
  assert(page);
  TPaletteHandle *cph = TApp::instance()->getCurrentPalette();
  StyleData *data     = new StyleData();
  std::set<int>::iterator it;
  for (it = styleIndicesInPage->begin(); it != styleIndicesInPage->end();
       ++it) {
    int j              = *it;
    int styleId        = page->getStyleId(j);
    TColorStyle *style = page->getStyle(j)->clone();
    data->addStyle(styleId, style);
  }
  QApplication::clipboard()->setMimeData(data);
}

//-----------------------------------------------------------------------------

bool pasteStylesWithoutUndo(TPaletteP palette, int pageIndex,
                            std::set<int> *styleIndicesInPage) {
  // page = pagina corrente
  TPalette::Page *page = palette->getPage(pageIndex);
  assert(page);
  // cerco il punto di inserimento (primo stile selezionato oppure dopo l'ultimo
  // stile della pagina
  // se nulla e' selezionato)
  int indexInPage                               = page->getStyleCount();
  if (!styleIndicesInPage->empty()) indexInPage = *styleIndicesInPage->begin();
  // prendo i dati dalla clipboard
  const StyleData *data =
      dynamic_cast<const StyleData *>(QApplication::clipboard()->mimeData());
  if (!data) return false;
  // cancello la selezione
  styleIndicesInPage->clear();
  // comincio a fare paste
  int i;
  for (i = 0; i < data->getStyleCount(); i++) {
    int styleId        = data->getStyleIndex(i);
    TColorStyle *style = data->getStyle(i)->clone();
    if (palette->getStylePage(styleId) < 0) {
      // styleId non e' utilizzato: uso quello
      // (cut/paste utilizzato per spostare stili)
      palette->setStyle(styleId, style);
    } else {
      // styleId e' gia' utilizzato. ne devo prendere un altro
      styleId = palette->getFirstUnpagedStyle();
      if (styleId >= 0)
        palette->setStyle(styleId, style);
      else
        styleId = palette->addStyle(style);
    }
    // inserisco lo stile nella pagina
    page->insertStyle(indexInPage + i, styleId);
    // e lo seleziono
    styleIndicesInPage->insert(indexInPage + i);
  }
  TApp::instance()->getCurrentPalette()->notifyColorStyleChanged();
  return true;
}

//-----------------------------------------------------------------------------

void deleteStylesWithoutUndo(TPaletteP palette, int pageIndex,
                             std::set<int> *styleIndicesInPage) {
  int n = styleIndicesInPage->size();
  if (n == 0) return;
  TPalette::Page *page = palette->getPage(pageIndex);
  assert(page);
  TPaletteHandle *cph         = TApp::instance()->getCurrentPalette();
  int currentStyleIndexInPage = page->search(cph->getStyleIndex());
  bool mustChangeCurrentStyle =
      currentStyleIndexInPage >= 0 &&
      styleIndicesInPage->count(currentStyleIndexInPage) > 0;
  std::set<int>::reverse_iterator it;
  for (it = styleIndicesInPage->rbegin(); it != styleIndicesInPage->rend();
       ++it) {
    int j       = *it;
    int styleId = page->getStyleId(j);
    if (styleId < 2) {
      error("Can't delete color #" + QString::number(styleId));
    } else {
      if (styleId == cph->getStyleIndex()) cph->setStyleIndex(1);
      palette->setStyle(styleId, TPixel32::Red);
      page->removeStyle(j);
    }
  }
  styleIndicesInPage->clear();
  if (mustChangeCurrentStyle) {
    // ho cancellato lo stile corrente
    if (currentStyleIndexInPage < page->getStyleCount()) {
      // posso fare in modo che lo stile selezionato sia nella stessa posizione
      cph->setStyleIndex(page->getStyleId(currentStyleIndexInPage));
    } else if (page->getStyleCount() > 0) {
      // almeno faccio in modo che sia nella stessa pagina
      int styleId = page->getStyleId(page->getStyleCount() - 1);
      cph->setStyleIndex(styleId);
    } else {
      // seleziono lo stile #1 (che c'e' sempre). n.b. questo puo' comportare un
      // cambio pagina
      cph->setStyleIndex(1);
    }
  }
  cph->notifyColorStyleChanged();
}

//-----------------------------------------------------------------------------

void cutStylesWithoutUndo(TPaletteP palette, int pageIndex,
                          std::set<int> *styleIndicesInPage) {
  copyStylesWithoutUndo(palette, pageIndex, styleIndicesInPage);
  deleteStylesWithoutUndo(palette, pageIndex, styleIndicesInPage);
}

//=============================================================================
// CopyStylesUndo
//-----------------------------------------------------------------------------

class CopyStylesUndo : public TUndo {
  QMimeData *m_oldData;
  QMimeData *m_newData;

public:
  CopyStylesUndo(QMimeData *oldData, QMimeData *newData)
      : m_oldData(oldData), m_newData(newData) {}

  void undo() const {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);
  }

  void redo() const {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_newData), QClipboard::Clipboard);
  }

  int getSize() const { return sizeof(*this); }
};

//=============================================================================
// PasteStylesUndo
//-----------------------------------------------------------------------------

class PasteStylesUndo : public TUndo {
  TStyleSelection *m_selection;

public:
  PasteStylesUndo(TStyleSelection *selection) : m_selection(selection) {}

  ~PasteStylesUndo() { delete m_selection; }

  void undo() const {
    std::set<int> styleIndicesInPage = m_selection->getIndicesInPage();
    cutStylesWithoutUndo(m_selection->getPalette(), m_selection->getPageIndex(),
                         &styleIndicesInPage);
  }

  void redo() const {
    std::set<int> styleIndicesInPage = m_selection->getIndicesInPage();
    pasteStylesWithoutUndo(m_selection->getPalette(),
                           m_selection->getPageIndex(), &styleIndicesInPage);
  }

  int getSize() const { return sizeof(*this); }
};

//=============================================================================
// DeleteStylesUndo
//-----------------------------------------------------------------------------

class DeleteStylesUndo : public TUndo {
  TStyleSelection *m_selection;
  QMimeData *m_data;

public:
  DeleteStylesUndo(TStyleSelection *selection, QMimeData *data)
      : m_selection(selection), m_data(data) {}

  ~DeleteStylesUndo() { delete m_selection; }

  void undo() const {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *oldData    = cloneData(clipboard->mimeData());

    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);

    std::set<int> styleIndicesInPage = m_selection->getIndicesInPage();
    pasteStylesWithoutUndo(m_selection->getPalette(),
                           m_selection->getPageIndex(), &styleIndicesInPage);

    clipboard->setMimeData(oldData, QClipboard::Clipboard);
  }

  void redo() const {
    std::set<int> styleIndicesInPage = m_selection->getIndicesInPage();
    deleteStylesWithoutUndo(m_selection->getPalette(),
                            m_selection->getPageIndex(), &styleIndicesInPage);
  }

  int getSize() const { return sizeof(*this); }
};

//=============================================================================
// CutStylesUndo
//-----------------------------------------------------------------------------

class CutStylesUndo : public TUndo {
  TStyleSelection *m_selection;
  QMimeData *m_oldData;

public:
  CutStylesUndo(TStyleSelection *selection, QMimeData *data)
      : m_selection(selection), m_oldData(data) {}

  ~CutStylesUndo() { delete m_selection; }

  void undo() const {
    std::set<int> styleIndicesInPage = m_selection->getIndicesInPage();
    pasteStylesWithoutUndo(m_selection->getPalette(),
                           m_selection->getPageIndex(), &styleIndicesInPage);
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);
  }

  void redo() const {
    std::set<int> styleIndicesInPage = m_selection->getIndicesInPage();
    cutStylesWithoutUndo(m_selection->getPalette(), m_selection->getPageIndex(),
                         &styleIndicesInPage);
  }

  int getSize() const { return sizeof(*this); }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TStyleSelection
//-----------------------------------------------------------------------------

TStyleSelection::TStyleSelection() : m_palette(0), m_pageIndex(-1) {}

//-------------------------------------------------------------------

TStyleSelection::~TStyleSelection() {}

//-----------------------------------------------------------------------------

void TStyleSelection::enableCommands() {
  enableCommand(this, MI_Cut, &TStyleSelection::cutStyles);
  enableCommand(this, MI_Copy, &TStyleSelection::copyStyles);
  enableCommand(this, MI_Paste, &TStyleSelection::pasteStyles);
  enableCommand(this, MI_PasteValues, &TStyleSelection::pasteStylesValue);
  enableCommand(this, MI_Clear, &TStyleSelection::deleteStyles);
  enableCommand(this, MI_BlendColors, &TStyleSelection::blendStyles);
}

//-------------------------------------------------------------------

void TStyleSelection::select(const TPaletteP &palette, int pageIndex) {
  m_palette   = palette;
  m_pageIndex = pageIndex;
  m_styleIndicesInPage.clear();
}

//-------------------------------------------------------------------

void TStyleSelection::select(const TPaletteP &palette, int pageIndex,
                             int styleIndexInPage, bool on) {
  if (on) {
    if (m_palette.getPointer() != palette.getPointer() ||
        pageIndex != m_pageIndex)
      m_styleIndicesInPage.clear();
    m_palette   = palette;
    m_pageIndex = pageIndex;
    m_styleIndicesInPage.insert(styleIndexInPage);
  } else if (m_palette.getPointer() == palette.getPointer() &&
             pageIndex == m_pageIndex)
    m_styleIndicesInPage.erase(styleIndexInPage);
}

//-------------------------------------------------------------------

bool TStyleSelection::isSelected(const TPaletteP &palette, int pageIndex,
                                 int id) const {
  return m_palette.getPointer() == palette.getPointer() &&
         m_pageIndex == pageIndex &&
         m_styleIndicesInPage.find(id) != m_styleIndicesInPage.end();
}

//------------------------------------------------------------------

bool TStyleSelection::isPageSelected(const TPaletteP &palette,
                                     int pageIndex) const {
  return m_palette.getPointer() == palette.getPointer() &&
         m_pageIndex == pageIndex;
}
//-------------------------------------------------------------------

void TStyleSelection::selectNone() {
  m_palette   = 0;
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
  QClipboard *clipboard      = QApplication::clipboard();
  QMimeData *oldData         = cloneData(clipboard->mimeData());
  TStyleSelection *selection = new TStyleSelection(this);
  if (m_pageIndex == 0 &&
      (isSelected(m_palette, 0, 0) || isSelected(m_palette, 0, 1))) {
    MsgBox(CRITICAL, "Can't delete colors #0 and #1");
    return;
  }
  cutStylesWithoutUndo(m_palette, m_pageIndex, &m_styleIndicesInPage);

  TUndoManager::manager()->add(new CutStylesUndo(selection, oldData));
}

//-------------------------------------------------------------------

void TStyleSelection::copyStyles() {
  if (isEmpty()) return;
  QClipboard *clipboard = QApplication::clipboard();
  QMimeData *oldData    = cloneData(clipboard->mimeData());
  copyStylesWithoutUndo(m_palette, m_pageIndex, &m_styleIndicesInPage);
  QMimeData *newData = cloneData(clipboard->mimeData());

  TUndoManager::manager()->add(new CopyStylesUndo(oldData, newData));
}

//-------------------------------------------------------------------

void TStyleSelection::pasteStyles() {
  // se non c'e' palette o pagina corrente esco
  if (!m_palette || m_pageIndex < 0) return;

  if (m_pageIndex == 0 && !m_styleIndicesInPage.empty() &&
      *m_styleIndicesInPage.begin() < 2) {
    MsgBox(CRITICAL, "Can't paste styles there");
    return;
  }

  pasteStylesWithoutUndo(m_palette, m_pageIndex, &m_styleIndicesInPage);
  TUndoManager::manager()->add(new PasteStylesUndo(new TStyleSelection(this)));
}

//-------------------------------------------------------------------

void TStyleSelection::deleteStyles() {
  if (!m_palette || m_pageIndex < 0) return;
  if (m_pageIndex == 0 &&
      (isSelected(m_palette, 0, 0) || isSelected(m_palette, 0, 1))) {
    MsgBox(CRITICAL, "Can't delete colors #0 and #1");
    return;
  }

  StyleData *data = new StyleData();
  std::set<int>::iterator it;
  TPalette::Page *page = m_palette->getPage(m_pageIndex);
  for (it = m_styleIndicesInPage.begin(); it != m_styleIndicesInPage.end();
       ++it) {
    int j       = *it;
    int styleId = page->getStyleId(j);
    if (styleId < 0) continue;
    TColorStyle *style = page->getStyle(j)->clone();
    data->addStyle(styleId, style);
  }
  TStyleSelection *selection = new TStyleSelection(this);

  deleteStylesWithoutUndo(m_palette, m_pageIndex, &m_styleIndicesInPage);

  TUndoManager::manager()->add(new DeleteStylesUndo(selection, data));
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoPasteValues
//-----------------------------------------------------------------------------

class UndoPasteValues : public TUndo {
  TPaletteP m_palette;
  class Item {
  public:
    int m_index;
    TColorStyle *m_oldStyle;
    TColorStyle *m_newStyle;
    Item(int index, const TColorStyle *oldStyle, const TColorStyle *newStyle)
        : m_index(index)
        , m_oldStyle(oldStyle->clone())
        , m_newStyle(newStyle->clone()) {}
    ~Item() {
      delete m_oldStyle;
      delete m_newStyle;
    }
  };

  std::vector<Item *> m_items;

public:
  UndoPasteValues(const TPaletteP &palette) : m_palette(palette) {}

  ~UndoPasteValues() { clearPointerContainer(m_items); }

  void addItem(int index, const TColorStyle *oldStyle,
               const TColorStyle *newStyle) {
    m_items.push_back(new Item(index, oldStyle, newStyle));
  }

  void pasteValue(int styleId, const TColorStyle *newStyle) const {
    // preserva il vecchio nome
    wstring str = m_palette->getStyle(styleId)->getName();
    m_palette->setStyle(styleId, newStyle->clone());
    m_palette->getStyle(styleId)->setName(str);
  }

  void undo() const {
    int i;
    for (i = 0; i < (int)m_items.size(); i++)
      pasteValue(m_items[i]->m_index, m_items[i]->m_oldStyle);
    TApp::instance()->getCurrentPalette()->notifyColorStyleChanged();
  }

  void redo() const {
    int i;
    for (i = 0; i < (int)m_items.size(); i++)
      pasteValue(m_items[i]->m_index, m_items[i]->m_newStyle);
    TApp::instance()->getCurrentPalette()->notifyColorStyleChanged();
  }

  int getSize() const {
    return sizeof(*this) + (int)m_items.size() * 100;  // forfait
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void TStyleSelection::pasteStylesValue() {
  if (!m_palette || m_pageIndex < 0) return;
  if (m_pageIndex == 0 && isSelected(m_palette, 0, 0)) {
    MsgBox(CRITICAL, "Can't modify color #0");
    return;
  }

  TPalette::Page *page = m_palette->getPage(m_pageIndex);
  assert(page);
  const StyleData *data =
      dynamic_cast<const StyleData *>(QApplication::clipboard()->mimeData());
  if (!data) return;
  int i                 = 0;
  UndoPasteValues *undo = new UndoPasteValues(m_palette);
  std::set<int>::iterator it;
  for (it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end() && i < data->getStyleCount();
       ++it, i++) {
    int styleId = page->getStyleId(*it);
    undo->addItem(styleId, m_palette->getStyle(styleId), data->getStyle(i));
    m_palette->setStyle(styleId, data->getStyle(i)->clone());
  }
  TUndoManager::manager()->add(undo);

  TPaletteHandle *ph = TApp::instance()->getCurrentPalette();
  ph->notifyColorStyleChanged();
  ph->updateColor();
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoBlendColor
//-----------------------------------------------------------------------------

class UndoBlendColor : public TUndo {
  TPaletteP m_palette;
  int m_pageIndex;
  std::vector<std::pair<int, TColorStyle *>> m_colorStyles;
  TPixel32 m_c0, m_c1;

public:
  UndoBlendColor(TPaletteP palette, int pageIndex,
                 std::vector<std::pair<int, TColorStyle *>> colorStyles,
                 const TPixel32 &c0, const TPixel32 &c1)
      : m_palette(palette)
      , m_pageIndex(pageIndex)
      , m_colorStyles(colorStyles)
      , m_c0(c0)
      , m_c1(c1) {}

  ~UndoBlendColor() {}

  void undo() const {
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
    TApp::instance()->getCurrentPalette()->notifyColorStyleChanged();
  }

  void redo() const {
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
    TApp::instance()->getCurrentPalette()->notifyColorStyleChanged();
  }

  int getSize() const {
    return sizeof(
        *this);  //+500*m_oldStyles.size(); //dipende da che stile ha salvato
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void TStyleSelection::blendStyles() {
  if (!m_palette || m_pageIndex < 0) return;
  int n = m_styleIndicesInPage.size();
  if (n < 2) return;

  TPalette::Page *page = m_palette->getPage(m_pageIndex);
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
  TApp::instance()->getCurrentPalette()->notifyColorStyleChanged();

  UndoBlendColor *undo =
      new UndoBlendColor(m_palette, m_pageIndex, oldStyles, c0, c1);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void TStyleSelection::toggleLink() {
  if (!m_palette || m_pageIndex < 0) return;
  int n = m_styleIndicesInPage.size();
  if (n <= 0) return;

  bool somethingHasBeenLinked = false;

  bool currentStyleIsInSelection = false;
  TApp *app                      = TApp::instance();
  TPaletteHandle *ph             = app->getCurrentPalette();

  TPalette::Page *page = m_palette->getPage(m_pageIndex);
  assert(page);

  for (std::set<int>::iterator it = m_styleIndicesInPage.begin();
       it != m_styleIndicesInPage.end(); ++it) {
    TColorStyle *cs = page->getStyle(*it);
    assert(cs);
    wstring name = cs->getGlobalName();
    if (name != L"" && (name[0] == L'-' || name[0] == L'+')) {
      name[0] = name[0] == L'-' ? L'+' : L'-';
      cs->setGlobalName(name);
      if (name[0] == L'+') somethingHasBeenLinked = true;
    }

    if (*it == m_palette->getPage(m_pageIndex)->search(ph->getStyleIndex()))
      currentStyleIsInSelection = true;
  }
  if (somethingHasBeenLinked)
    StudioPalette::instance()->updateLinkedColors(m_palette.getPointer());

  ph->notifyPaletteChanged();

  if (currentStyleIsInSelection) ph->notifyColorStyleSwitched();

  // DA FARE: e' giusto mettere la nofica del dirty flag a current scene
  // o e' meglio farlo al livello corrente!?
  app->getCurrentScene()->setDirtyFlag(true);
  //  extern void setPaletteDirtyFlag();
  //  setPaletteDirtyFlag();
}

//-----------------------------------------------------------------------------

QByteArray TStyleSelection::toByteArray(int pageIndex,
                                        const std::set<int> &indicesInPage,
                                        const QString paletteGlobalName) {
  QByteArray data;
  QBuffer dataBuffer(&data);
  dataBuffer.open(QIODevice::WriteOnly);
  QDataStream out(&dataBuffer);
  out << (int)pageIndex << (int)indicesInPage.size() << paletteGlobalName;

  std::set<int>::const_iterator it;
  for (it = indicesInPage.begin(); it != indicesInPage.end(); ++it)
    out << (int)*it;
  return data;
}

//-----------------------------------------------------------------------------

void TStyleSelection::fromByteArray(QByteArray &byteArray, int &pageIndex,
                                    std::set<int> &indicesInPage,
                                    QString &paletteGlobalName) {
  QBuffer dataBuffer(&byteArray);
  dataBuffer.open(QIODevice::ReadOnly);
  QDataStream in(&dataBuffer);
  int i, count = 0;
  in >> pageIndex >> count >> paletteGlobalName;
  for (i = 0; i < count; i++) {
    int indexInPage;
    in >> indexInPage;
    indicesInPage.insert(indexInPage);
  }
}

//-----------------------------------------------------------------------------

QByteArray TStyleSelection::toByteArray() const {
  QString globalName =
      QString::fromStdWString(m_palette->getGlobalName().c_str());
  return toByteArray(m_pageIndex, m_styleIndicesInPage, globalName);
}

//-----------------------------------------------------------------------------

const char *TStyleSelection::getMimeType() {
  return "application/vnd.toonz.style";
}
