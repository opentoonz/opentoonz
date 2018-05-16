

//#include "tw/mainshell.h"

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"

#include "tstroke.h"
#include "tmathutil.h"
#include "tproperty.h"
#include "tenv.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "toonz/toonzimageutils.h"
#include "tools/cursors.h"
#include "tundo.h"
#include "tvectorgl.h"
#include "tgl.h"
#include "tregion.h"
#include "tvectorrenderdata.h"
#include "toonz/tpalettehandle.h"

#include "toonzqt/selection.h"
#include "toonzqt/imageutils.h"
#include "trop.h"
#include "toonz/ttileset.h"
#include "toonz/glrasterpainter.h"
#include "toonz/stage.h"

#include "tfont.h"

// For Qt translation support
#include <QCoreApplication>

//#include "tw/message.h"

//#include "tw/ime.h"

using namespace ToolUtils;

namespace {

//---------------------------------------------------------
//
// helper functions
//
//---------------------------------------------------------

void paintRegion(TRegion *region, int styleId, bool paint) {
  UINT j, regNum = region->getSubregionCount();
  if (paint) region->setStyle(styleId);

  for (j = 0; j < regNum; j++)
    paintRegion(region->getSubregion(j), styleId, !paint);
}

//---------------------------------------------------------

void paintChar(const TVectorImageP &image, int styleId) {
  // image->setPalette( getApplication()->getCurrentScene()->getPalette());
  // image->setPalette(getCurrentPalette());

  UINT j;
  UINT strokeNum = image->getStrokeCount();
  for (j = 0; j < strokeNum; j++) image->getStroke(j)->setStyle(styleId);

  image->enableRegionComputing(true, true);
  image->findRegions();

  UINT regNum = image->getRegionCount();
  for (j = 0; j < regNum; j++) paintRegion(image->getRegion(j), styleId, true);
}

//---------------------------------------------------------
// Type, vars, ecc.
//
//---------------------------------------------------------

TEnv::StringVar EnvCurrentFont("CurrentFont", "MS UI Gothic");

//! numero di pixel attorno al testo
const double cBorderSize = 15;

//---------------------------------------------------------
//
// UNDO
//
//---------------------------------------------------------

class UndoTypeTool final : public ToolUtils::TToolUndo {
  std::vector<TStroke *> m_strokes;
  std::vector<TFilledRegionInf> *m_fillInformationBefore,
      *m_fillInformationAfter;
  TVectorImageP m_image;

public:
  UndoTypeTool(std::vector<TFilledRegionInf> *fillInformationBefore,
               std::vector<TFilledRegionInf> *fillInformationAfter,
               TXshSimpleLevel *level, const TFrameId &frameId,
               bool isFrameCreated, bool isLevelCreated)
      : ToolUtils::TToolUndo(level, frameId, isFrameCreated, isLevelCreated)
      , m_fillInformationBefore(fillInformationBefore)
      , m_fillInformationAfter(fillInformationAfter) {}

  ~UndoTypeTool() {
    delete m_fillInformationBefore;
    delete m_fillInformationAfter;
    clearPointerContainer(m_strokes);
  }

  void addStroke(TStroke *stroke) {
    TStroke *s = new TStroke(*stroke);
    s->setId(stroke->getId());
    m_strokes.push_back(s);
  }

  void undo() const override {
    TTool::Application *application = TTool::getApplication();

    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(!!image);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    UINT i;
    for (i = 0; i < m_strokes.size(); i++) {
      VIStroke *stroke = image->getStrokeById(m_strokes[i]->getId());
      if (!stroke) return;
      image->deleteStroke(stroke);
    }

    if (m_fillInformationBefore) {
      UINT size = m_fillInformationBefore->size();
      TRegion *reg;
      for (i = 0; i < size; i++) {
        reg = image->getRegion((*m_fillInformationBefore)[i].m_regionId);
        assert(reg);
        if (reg) reg->setStyle((*m_fillInformationBefore)[i].m_styleId);
      }
    }
    removeLevelAndFrameIfNeeded();
    application->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(!!image);
    if (!image) return;

    TTool::Application *application = TTool::getApplication();

    QMutexLocker lock(image->getMutex());
    UINT i;
    for (i = 0; i < m_strokes.size(); i++) {
      TStroke *stroke = new TStroke(*m_strokes[i]);
      stroke->setId(m_strokes[i]->getId());
      image->addStroke(stroke);
    }

    if (image->isComputedRegionAlmostOnce()) image->findRegions();

    if (m_fillInformationAfter) {
      UINT size = m_fillInformationAfter->size();
      TRegion *reg;
      for (i = 0; i < size; i++) {
        reg = image->getRegion((*m_fillInformationAfter)[i].m_regionId);
        assert(reg);
        if (reg) reg->setStyle((*m_fillInformationAfter)[i].m_styleId);
      }
    }
    application->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    if (m_fillInformationAfter && m_fillInformationBefore)
      return sizeof(*this) +
             m_fillInformationBefore->capacity() * sizeof(TFilledRegionInf) +
             m_fillInformationAfter->capacity() * sizeof(TFilledRegionInf) +
             +m_strokes.capacity() * sizeof(TStroke) + 500;
    else
      return sizeof(*this) + m_strokes.capacity() * sizeof(TStroke) + 500;
  }

  QString getToolName() override { return QString("Type Tool"); }
};

//---------------------------------------------------------

class RasterUndoTypeTool final : public TRasterUndo {
  TTileSetCM32 *m_afterTiles;

public:
  RasterUndoTypeTool(TTileSetCM32 *beforeTiles, TTileSetCM32 *afterTiles,
                     TXshSimpleLevel *level, const TFrameId &id,
                     bool createdFrame, bool createdLevel)
      : TRasterUndo(beforeTiles, level, id, createdFrame, createdLevel, 0)
      , m_afterTiles(afterTiles) {}

  ~RasterUndoTypeTool() { delete m_afterTiles; }

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TToonzImageP image = getImage();
    if (!image) return;
    if (m_afterTiles) {
      ToonzImageUtils::paste(image, m_afterTiles);
      ToolUtils::updateSaveBox();
    }

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    if (m_afterTiles)
      return TRasterUndo::getSize() + m_afterTiles->getMemorySize();
    else
      return TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Type Tool"); }
};

//---------------------------------------------------------
//
// StrokeChar
//
// E' la TVectorImage che corrisponde ad un carattere
// (piu' le varie info che permettono di ricrearlo)
//
//---------------------------------------------------------

class StrokeChar {
public:
  // TVectorImageP m_char;

  TImageP m_char;

  double m_offset;
  TPointD m_charPosition;
  int m_key;
  int m_styleId;

  StrokeChar(TImageP _char, double offset, int key, int styleId)
      : m_char(_char)
      , m_offset(offset)
      , m_charPosition(TPointD(0, 0))
      , m_key(key)
      , m_styleId(styleId) {}

  bool isReturn() const { return m_key == (int)(QChar('\r').unicode()); }

  void update(TAffine scale, int nextCode = 0) {
    if (!isReturn()) {
      if (TVectorImageP vi = m_char) {
        vi = m_char = new TVectorImage;
        TPoint adv  = TFontManager::instance()->drawChar(vi, m_key, nextCode);
        vi->transform(scale);
        paintChar(vi, m_styleId);
        m_offset = (scale * TPointD((double)(adv.x), (double)(adv.y))).x;
      } else {
        TRasterCM32P newRasterCM;
        TPoint p;
        TPoint adv = TFontManager::instance()->drawChar(
            (TRasterCM32P &)newRasterCM, p, m_styleId, (wchar_t)m_key,
            (wchar_t)nextCode);
        // m_char->transform(scale);
        m_offset = (scale * TPointD((double)(adv.x), (double)(adv.y))).x;

        m_char = new TToonzImage(newRasterCM, newRasterCM->getBounds());
      }
    }
  }
};

}  //  namespace

//---------------------------------------------------------
//
// TypeTool
//
//---------------------------------------------------------

class TypeTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(TypeTool)

  // Properties
  TEnumProperty m_fontFamilyMenu;
  TEnumProperty m_typeFaceMenu;
  TBoolProperty m_vertical;
  TEnumProperty m_size;
  TPropertyGroup m_prop;

  // valori correnti di alcune Properties,
  // duplicati per permettere controlli sulla validita' o per ottimizzazione
  std::wstring m_fontFamily;
  std::wstring m_typeface;
  double m_dimension;

  bool m_validFonts;  // false iff there are problems with font loading
  bool m_initialized;

  int m_cursorId;
  int m_styleId;
  double m_pixelSize;

  std::vector<StrokeChar> m_string;

  int m_cursorIndex;  // indice del carattere successivo al cursore
  std::pair<int, int> m_preeditRange;
  // posizione (dentro m_string) della preeditString (cfr. IME)
  // n.b. la preeditString va da range.first a range.second-1
  TRectD m_textBox;  // bbox della stringa
  TScale m_scale;  // la dimensione dei char ottenuti dal text manager e' fissa
                   // perche' influisce sula qualita'. Quindi lo scalo
  TPointD m_cursorPoint;  // punto piu alto del cursore
  TPointD m_startPoint;   // punto dove si poggia la prima lettera (coincide col
                          // mouse click)
  double m_fontYOffset;   // spazio per le parti delle lettere sotto il rigo
                          // (dipende dal font)
  bool m_isVertical;      // text orientation

  TUndo *m_undo;

public:
  TypeTool();
  ~TypeTool();

  void updateTranslation() override;

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void init();
  void initTypeFaces();

  void loadFonts();
  void setFont(std::wstring fontFamily);
  void setTypeface(std::wstring typeface);
  void setSize(std::wstring size);
  void setVertical(bool vertical);
  void draw() override;

  void updateMouseCursor(const TPointD &pos);
  void updateStrokeChar();
  void updateCharPositions(int updateFrom = 0);
  void updateCursorPoint();
  void updateTextBox();

  void setCursorIndexFromPoint(TPointD point);

  void mouseMove(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void rightButtonDown(const TPointD &pos, const TMouseEvent &) override;
  bool keyDown(QKeyEvent *event) override;

  void onInputText(std::wstring preedit, std::wstring commit,
                   int replacementStart, int replacementLen) override;

  // cancella gli StrokeChar fra from e to-1 e inserisce nuovi StrokeChar
  // corrispondenti a text a partire da from
  void replaceText(std::wstring text, int from, int to);

  void addBaseChar(std::wstring text);
  void addReturn();
  void cursorUp();
  void cursorDown();
  void cursorLeft();
  void cursorRight();
  void deleteKey();
  void addTextToImage();
  void addTextToVectorImage(const TVectorImageP &currentImage,
                            std::vector<const TVectorImage *> &images);
  void addTextToToonzImage(const TToonzImageP &currentImage);
  void stopEditing();

  void reset() override;

  void onActivate() override;
  void onDeactivate() override;
  void onImageChanged() override;

  int getCursorId() const override { return m_cursorId; }

  bool onPropertyChanged(std::string propertyName) override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getColorClass() const { return 1; }

} typeTool;

//---------------------------------------------------------
//
// TypeTool methods
//
//---------------------------------------------------------

TypeTool::TypeTool()
    : TTool("T_Type")
    , m_textBox(TRectD(0, 0, 0, 0))
    , m_cursorPoint(TPointD(0, 0))
    , m_startPoint(TPointD(0, 0))
    , m_dimension(1)      // to be set the m_size value on the first-click
    , m_validFonts(true)  // false)
    , m_fontYOffset(0)
    , m_cursorId(ToolCursor::CURSOR_NO)
    , m_pixelSize(1)
    , m_cursorIndex(0)
    , m_preeditRange(0, 0)
    , m_isVertical(false)
    , m_initialized(false)
    , m_fontFamilyMenu("Font:")                  // W_ToolOptions_FontName
    , m_typeFaceMenu("Style:")                   // W_ToolOptions_TypeFace
    , m_vertical("Vertical Orientation", false)  // W_ToolOptions_Vertical
    , m_size("Size:")                            // W_ToolOptions_Size
    , m_undo(0) {
  bind(TTool::CommonLevels | TTool::EmptyTarget);
  m_prop.bind(m_fontFamilyMenu);
  // Su mac non e' visibile il menu dello style perche' e' stato inserito nel
  // nome
  // della font.
  //#ifndef MACOSX
  m_prop.bind(m_typeFaceMenu);
  //#endif
  m_prop.bind(m_size);
  m_prop.bind(m_vertical);
  m_vertical.setId("Orientation");
  m_fontFamilyMenu.setId("TypeFont");
  m_typeFaceMenu.setId("TypeStyle");
  m_size.setId("TypeSize");
}

//---------------------------------------------------------

TypeTool::~TypeTool() {}

//---------------------------------------------------------

void TypeTool::updateTranslation() {
  m_fontFamilyMenu.setQStringName(tr("Font:"));
  m_typeFaceMenu.setQStringName(tr("Style:"));
  m_vertical.setQStringName(tr("Vertical Orientation"));
  m_size.setQStringName(tr("Size:"));
}

//---------------------------------------------------------

bool TypeTool::onPropertyChanged(std::string propertyName) {
  if (!m_validFonts) return false;

  if (propertyName == m_fontFamilyMenu.getName()) {
    setFont(m_fontFamilyMenu.getValue());
    return true;
  } else if (propertyName == m_typeFaceMenu.getName()) {
    setTypeface(m_typeFaceMenu.getValue());
    return true;
  } else if (propertyName == m_size.getName()) {
    setSize(m_size.getValue());
    return true;
  } else if (propertyName == m_vertical.getName()) {
    setVertical(m_vertical.getValue());
    return true;
  }
  return false;
}

//---------------------------------------------------------

void TypeTool::init() {
  if (m_initialized) return;
  m_initialized = true;

  loadFonts();
  if (!m_validFonts) return;

  m_size.addValue(L"36");
  m_size.addValue(L"58");
  m_size.addValue(L"70");
  m_size.addValue(L"86");
  m_size.addValue(L"100");
  m_size.addValue(L"150");
  m_size.addValue(L"200");
  m_size.setValue(L"70");
}

//---------------------------------------------------------

void TypeTool::loadFonts() {
  TFontManager *instance = TFontManager::instance();
  try {
    instance->loadFontNames();
    m_validFonts = true;
  } catch (TFontLibraryLoadingError &) {
    m_validFonts = false;
    //    TMessage::error(toString(e.getMessage()));
  }

  if (!m_validFonts) return;

  std::vector<std::wstring> names;
  instance->getAllFamilies(names);

  for (std::vector<std::wstring>::iterator it = names.begin();
       it != names.end(); ++it)
    m_fontFamilyMenu.addValue(*it);

  std::string favFontApp     = EnvCurrentFont;
  std::wstring favouriteFont = ::to_wstring(favFontApp);
  if (m_fontFamilyMenu.isValue(favouriteFont)) {
    m_fontFamilyMenu.setValue(favouriteFont);
    setFont(favouriteFont);
  } else {
    setFont(m_fontFamilyMenu.getValue());
  }

  // not used for now
  m_scale = TScale();
  // m_scale = TScale(m_dimension /
  // (double)(TFontManager::instance()->getHeight()));
}

//---------------------------------------------------------

void TypeTool::initTypeFaces() {
  TFontManager *instance = TFontManager::instance();
  std::vector<std::wstring> typefaces;
  instance->getAllTypefaces(typefaces);
  std::wstring oldTypeface = m_typeFaceMenu.getValue();
  m_typeFaceMenu.deleteAllValues();
  for (std::vector<std::wstring>::iterator it = typefaces.begin();
       it != typefaces.end(); ++it)
    m_typeFaceMenu.addValue(*it);
  if (m_typeFaceMenu.isValue(oldTypeface)) m_typeFaceMenu.setValue(oldTypeface);
}

//---------------------------------------------------------

void TypeTool::setFont(std::wstring family) {
  if (m_fontFamily == family) return;
  TFontManager *instance = TFontManager::instance();
  try {
    instance->setFamily(family);

    m_fontFamily             = family;
    std::wstring oldTypeface = m_typeFaceMenu.getValue();
    initTypeFaces();
    if (oldTypeface != m_typeFaceMenu.getValue()) {
      if (m_typeFaceMenu.isValue(L"Regular")) {
        m_typeFaceMenu.setValue(L"Regular");
        m_typeface = L"Regular";
        instance->setTypeface(L"Regular");
      } else {
        m_typeface = m_typeFaceMenu.getValue();
        instance->setTypeface(m_typeface);
      }
    }

    // assert chiaramente vero se oldTypeface!=m_typeFaceMenu.getValue().
    // Assume che il TFontManager quando cambia family, si ricordi
    // il vecchio typeface e lo imposti se anche la nuova family lo ha
    assert(instance->getCurrentTypeface() == m_typeFaceMenu.getValue());

    updateStrokeChar();
    invalidate();
    EnvCurrentFont = ::to_string(m_fontFamily);
  } catch (TFontCreationError &) {
    //    TMessage::error(toString(e.getMessage()));
    assert(m_fontFamily == instance->getCurrentFamily());
    m_fontFamilyMenu.setValue(m_fontFamily);
  }
}

//---------------------------------------------------------

void TypeTool::setTypeface(std::wstring typeface) {
  if (m_typeface == typeface) return;
  TFontManager *instance = TFontManager::instance();
  try {
    instance->setTypeface(typeface);
    m_typeface = typeface;
    updateStrokeChar();
    invalidate();
  } catch (TFontCreationError &) {
    //    TMessage::error(toString(e.getMessage()));
    assert(m_typeface == instance->getCurrentTypeface());
    m_typeFaceMenu.setValue(m_typeface);
  }
}

//---------------------------------------------------------

void TypeTool::setSize(std::wstring strSize) {
  // font e tool fields update
  double dimension = std::stod(strSize);

  TImageP img      = getImage(true);
  TToonzImageP ti  = img;
  TVectorImageP vi = img;
  // for vector levels, adjust size according to the ratio between
  // the viewer dpi and the vector level's dpi
  if (vi) dimension *= Stage::inch / Stage::standardDpi;

  if (m_dimension == dimension) return;
  TFontManager::instance()->setSize((int)dimension);

  assert(m_dimension != 0);
  double ratio = dimension / m_dimension;
  m_dimension  = dimension;

  // not used for now
  m_scale = TScale();
  // m_scale = TScale(m_dimension /
  // (double)(TFontManager::instance()->getHeight()));

  // text update

  if (m_string.empty()) return;

  for (UINT i = 0; i < m_string.size(); i++) {
    if (TVectorImageP vi = m_string[i].m_char) vi->transform(TScale(ratio));
    m_string[i].m_offset *= ratio;
  }
  if (ti)
    updateStrokeChar();
  else
    updateCharPositions();

  invalidate();
}

//---------------------------------------------------------

void TypeTool::setVertical(bool vertical) {
  if (vertical == m_isVertical) return;

  m_isVertical        = vertical;
  bool oldHasVertical = TFontManager::instance()->hasVertical();
  TFontManager::instance()->setVertical(vertical);
  if (oldHasVertical != TFontManager::instance()->hasVertical())
    updateStrokeChar();
  else
    updateCharPositions();
  invalidate();
}

//---------------------------------------------------------

void TypeTool::stopEditing() {
  if (m_active == false) return;
  m_active = false;
  m_string.clear();
  m_cursorIndex = 0;
  m_textBox     = TRectD();
  //  closeImeWindow();
  //  if(m_viewer) m_viewer->enableIme(false);
  //  enableShortcuts(true);
  m_preeditRange = std::make_pair(0, 0);
  invalidate();
  if (m_undo) {
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }
}

//---------------------------------------------------------

void TypeTool::updateStrokeChar() {
  TFontManager *instance = TFontManager::instance();

  // not used for now
  m_scale = TScale();
  // m_scale = TScale(m_dimension / (double)(instance->getHeight()));

  bool hasKerning = instance->hasKerning();
  for (UINT i = 0; i < m_string.size(); i++) {
    if (hasKerning && i + 1 < m_string.size() && !m_string[i + 1].isReturn())
      m_string[i].update(/*m_font,*/ m_scale, m_string[i + 1].m_key);
    else
      m_string[i].update(/*m_font,*/ m_scale);
  }

  updateCharPositions();
}

//---------------------------------------------------------

void TypeTool::updateCharPositions(int updateFrom) {
  if (updateFrom < 0) updateFrom = 0;
  UINT size                      = m_string.size();
  TPointD currentOffset;
  TFontManager *instance = TFontManager::instance();
  m_fontYOffset          = (double)(instance->getLineSpacing()) * m_scale.a11;
  double descent         = (double)(instance->getLineDescender()) * m_scale.a11;
  double height          = (double)(instance->getHeight()) * m_scale.a11;
  double vLineSpacing =
      (double)(instance->getAverageCharWidth()) * 2.0 * m_scale.a11;

  // Update from "updateFrom"
  if (updateFrom > 0) {
    if ((int)m_string.size() <= updateFrom - 1) return;
    currentOffset = m_string[updateFrom - 1].m_charPosition - m_startPoint;
    // Vertical case
    if (m_isVertical && !instance->hasVertical()) {
      if (m_string[updateFrom - 1].isReturn())
        currentOffset = TPointD(currentOffset.x - vLineSpacing, -height);
      else
        currentOffset = currentOffset + TPointD(0, -height);
    }
    // Horizontal case
    else {
      if (m_string[updateFrom - 1].isReturn())
        currentOffset = TPointD(0, currentOffset.y - m_fontYOffset);
      else
        currentOffset =
            currentOffset + TPointD(m_string[updateFrom - 1].m_offset, 0);
    }
  }
  // Update whole characters
  else {
    if (m_isVertical && !instance->hasVertical())
      currentOffset = currentOffset + TPointD(0, -height);
    else
      currentOffset = currentOffset + TPointD(0, -descent);
  }

  for (UINT j = updateFrom; j < size; j++) {
    m_string[j].m_charPosition = m_startPoint + currentOffset;
    // Vertical case
    if (m_isVertical && !instance->hasVertical()) {
      if (m_string[j].isReturn() || m_string[j].m_key == ' ')
        currentOffset = TPointD(currentOffset.x - vLineSpacing, -height);
      else
        currentOffset = currentOffset + TPointD(0, -height);
    }
    // Horizontal case
    else {
      if (m_string[j].isReturn())
        currentOffset = TPointD(0, currentOffset.y - m_fontYOffset);
      else
        currentOffset = currentOffset + TPointD(m_string[j].m_offset, 0);
    }
  }

  // To be sure
  if (m_cursorIndex <= (int)m_string.size()) {
    updateCursorPoint();
    updateTextBox();
  }
}

//---------------------------------------------------------

void TypeTool::updateCursorPoint() {
  assert(0 <= m_cursorIndex && m_cursorIndex <= (int)m_string.size());
  TFontManager *instance = TFontManager::instance();
  double descent         = (double)(instance->getLineDescender()) * m_scale.a11;
  double height          = (double)(instance->getHeight()) * m_scale.a11;
  double vLineSpacing =
      (double)(instance->getAverageCharWidth()) * 2.0 * m_scale.a11;
  m_fontYOffset          = (double)(instance->getLineSpacing()) * m_scale.a11;
  double scaledDimension = m_dimension * m_scale.a11;

  if (m_string.empty()) {
    if (!m_isVertical || instance->hasVertical())
      m_cursorPoint = m_startPoint + TPointD(0, scaledDimension);
    else
      m_cursorPoint = m_startPoint;
  } else if (m_cursorIndex == (int)m_string.size()) {
    // Horizontal case
    if (!m_isVertical || instance->hasVertical()) {
      if (m_string.back().isReturn())
        m_cursorPoint = TPointD(
            m_startPoint.x, m_string.back().m_charPosition.y - m_fontYOffset +
                                scaledDimension + descent);
      else
        m_cursorPoint = m_string.back().m_charPosition +
                        TPointD(m_string.back().m_offset, 0) +
                        TPointD(0, scaledDimension + descent);
    }
    // Vertical case
    else {
      if (m_string.back().isReturn())
        m_cursorPoint = TPointD(m_string.back().m_charPosition.x - vLineSpacing,
                                m_startPoint.y);
      else
        m_cursorPoint = m_string.back().m_charPosition;
    }
  } else {
    if (!m_isVertical || instance->hasVertical())
      m_cursorPoint = m_string[m_cursorIndex].m_charPosition +
                      TPointD(0, scaledDimension + descent);
    else
      m_cursorPoint =
          m_string[m_cursorIndex].m_charPosition + TPointD(0, height);
  }
}

//---------------------------------------------------------

void TypeTool::updateTextBox() {
  UINT size                = m_string.size();
  UINT returnNumber        = 0;
  double currentLineLength = 0;
  double maxXLength        = 0;

  TFontManager *instance = TFontManager::instance();
  double descent         = (double)(instance->getLineDescender()) * m_scale.a11;
  double height          = (double)(instance->getHeight()) * m_scale.a11;
  double vLineSpacing =
      (double)(instance->getAverageCharWidth()) * 2.0 * m_scale.a11;
  m_fontYOffset = (double)(instance->getLineSpacing()) * m_scale.a11;

  for (UINT j = 0; j < size; j++) {
    if (m_string[j].isReturn()) {
      if (currentLineLength > maxXLength) {
        maxXLength = currentLineLength;
      }
      currentLineLength = 0;
      returnNumber++;
    } else {
      currentLineLength += (m_isVertical && !instance->hasVertical())
                               ? height
                               : m_string[j].m_offset;
    }
  }

  if (currentLineLength > maxXLength)  // last line
    maxXLength = currentLineLength;

  if (m_isVertical && !instance->hasVertical())
    m_textBox = TRectD(m_startPoint.x - vLineSpacing * returnNumber,
                       m_startPoint.y - maxXLength,
                       m_startPoint.x + vLineSpacing, m_startPoint.y)
                    .enlarge(cBorderSize * m_pixelSize);
  else
    m_textBox =
        TRectD(m_startPoint.x,
               m_startPoint.y - (m_fontYOffset * returnNumber + descent),
               m_startPoint.x + maxXLength, m_startPoint.y + height)
            .enlarge(cBorderSize * m_pixelSize);
}

//---------------------------------------------------------

void TypeTool::updateMouseCursor(const TPointD &pos) {
  int oldCursor = m_cursorId;

  if (!m_validFonts)
    m_cursorId = ToolCursor::CURSOR_NO;
  else {
    TPointD clickPoint =
        (TFontManager::instance()->hasVertical() && m_isVertical)
            ? TRotation(m_startPoint, 90) * pos
            : pos;
    if (m_textBox == TRectD(0, 0, 0, 0) || m_string.empty() ||
        !m_textBox.contains(clickPoint))
      m_cursorId = ToolCursor::TypeOutCursor;
    else
      m_cursorId = ToolCursor::TypeInCursor;
  }

  //  if(oldCursor != m_cursorId)
  //    TNotifier::instance()->notify(TToolChange());
}

//---------------------------------------------------------

void TypeTool::draw() {
  if (!m_active || !getImage(false)) return;

  TFontManager *instance = TFontManager::instance();

  /*TAffine viewMatrix = getViewer()->getViewMatrix();
glPushMatrix();
  tglMultMatrix(viewMatrix);*/

  if (instance->hasVertical() && m_isVertical) {
    glPushMatrix();
    tglMultMatrix(TRotation(m_startPoint, -90));
  }

  // draw text
  UINT size = m_string.size();

  TPoint descenderP(0, TFontManager::instance()->getLineDescender());

  for (int j = 0; j < (int)size; j++) {
    if (m_string[j].isReturn()) continue;

    TImageP img = TImageP(getImage(false));
    assert(!!img);
    if (!img) return;

    TPalette *vPalette = img->getPalette();
    assert(vPalette);

    double charWidth = 0;
    if (TVectorImageP vi = m_string[j].m_char) {
      TTranslation transl(convert(descenderP) + m_string[j].m_charPosition);
      const TVectorRenderData rd(transl, TRect(), vPalette, 0, false);
      tglDraw(rd, vi.getPointer());
      charWidth = vi->getBBox().getLx();
    } else if (TToonzImageP ti = m_string[j].m_char) {
      TDimension dim = ti->getSize();
      ti->setPalette(vPalette);

      TPoint rasterCenter(dim.lx / 2, dim.ly / 2);
      TTranslation transl1(convert(rasterCenter));
      TTranslation transl2(m_string[j].m_charPosition);
      GLRasterPainter::drawRaster(transl2 * m_scale * transl1, ti, false);

      TPointD adjustedDim = m_scale * TPointD(dim.lx, dim.ly);
      charWidth           = adjustedDim.x;
    }

    // sottolineo i caratteri della preedit string
    if (m_preeditRange.first <= j && j < m_preeditRange.second) {
      TPointD a(m_string[j].m_charPosition);
      TPointD b = a + TPointD(charWidth, 0);
      glColor3d(1, 0, 0);
      tglDrawSegment(a, b);
    }
  }

  // draw textbox
  double pixelSize = sqrt(tglGetPixelSize2());
  if (!isAlmostZero(pixelSize - m_pixelSize)) {
    m_textBox   = m_textBox.enlarge((pixelSize - m_pixelSize) * cBorderSize);
    m_pixelSize = pixelSize;
  }
  TPixel32 boxColor = TPixel32::Black;
  ToolUtils::drawRect(m_textBox, boxColor, 0x5555);

  if (m_active) {
    // draw cursor
    tglColor(TPixel32::Black);
    if (!m_isVertical || instance->hasVertical())
      tglDrawSegment(m_cursorPoint,
                     m_cursorPoint + m_scale * TPointD(0, -m_dimension));
    else
      tglDrawSegment(m_cursorPoint,
                     m_cursorPoint + m_scale * TPointD(m_dimension, 0));
  }

  TPointD drawableCursor = m_cursorPoint;
  if (instance->hasVertical() && m_isVertical) {
    drawableCursor = TRotation(m_startPoint, -90) * drawableCursor;
    glPopMatrix();
  }

  // glPopMatrix();

  //  TPoint ipos = m_viewer->toolToMouse(drawableCursor);
  //  setImePosition(ipos.x, ipos.y, (int)m_dimension);
}

//---------------------------------------------------------

void TypeTool::addTextToVectorImage(const TVectorImageP &currentImage,
                                    std::vector<const TVectorImage *> &images) {
  UINT oldSize = currentImage->getStrokeCount();

  std::vector<TFilledRegionInf> *fillInformationBefore =
      new std::vector<TFilledRegionInf>;
  ImageUtils::getFillingInformationOverlappingArea(
      currentImage, *fillInformationBefore, m_textBox);

  currentImage->mergeImage(images);

  std::vector<TFilledRegionInf> *fillInformationAfter =
      new std::vector<TFilledRegionInf>;
  ImageUtils::getFillingInformationOverlappingArea(
      currentImage, *fillInformationAfter, m_textBox);

  UINT newSize = currentImage->getStrokeCount();

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  UndoTypeTool *undo =
      new UndoTypeTool(fillInformationBefore, fillInformationAfter, level,
                       getCurrentFid(), m_isFrameCreated, m_isLevelCreated);

  for (UINT j = oldSize; j < newSize; j++)
    undo->addStroke(currentImage->getStroke(j));
  TUndoManager::manager()->add(undo);
  if (m_undo) {
    delete m_undo;
    m_undo = 0;
  }
}
//---------------------------------------------------------

void TypeTool::addTextToToonzImage(const TToonzImageP &currentImage) {
  UINT size = m_string.size();
  if (size == 0) return;

  // TPalette *palette  = currentImage->getPalette();
  // currentImage->lock();

  TRasterCM32P targetRaster = currentImage->getRaster();
  TRect changedArea;

  UINT j;
  for (j = 0; j < size; j++) {
    if (m_string[j].isReturn()) continue;

    if (TToonzImageP ti = m_string[j].m_char) {
      TRectD srcBBox  = ti->getBBox() + m_string[j].m_charPosition;
      TDimensionD dim = srcBBox.getSize();
      TDimensionD enlargeAmount(dim.lx * (m_scale.a11 - 1.0),
                                dim.ly * (m_scale.a22 - 1.0));
      changedArea += ToonzImageUtils::convertWorldToRaster(
          srcBBox.enlarge(enlargeAmount), currentImage);
      /*
if( instance->hasVertical() && m_isVertical)
vi->transform( TRotation(m_startPoint,-90) );
*/
    }
  }

  if (!changedArea.isEmpty()) {
    TTileSetCM32 *beforeTiles = new TTileSetCM32(targetRaster->getSize());
    beforeTiles->add(targetRaster, changedArea);

    for (j = 0; j < size; j++) {
      if (m_string[j].isReturn()) continue;

      if (TToonzImageP srcTi = m_string[j].m_char) {
        TRasterCM32P srcRaster = srcTi->getRaster();
        TTranslation transl2(m_string[j].m_charPosition +
                             convert(targetRaster->getCenter()));
        TRop::over(targetRaster, srcRaster, transl2 * m_scale);
      }
    }

    TTileSetCM32 *afterTiles = new TTileSetCM32(targetRaster->getSize());
    afterTiles->add(targetRaster, changedArea);

    TXshSimpleLevel *sl =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TFrameId id = getCurrentFid();

    TUndoManager::manager()->add(new RasterUndoTypeTool(
        beforeTiles, afterTiles, sl, id, m_isFrameCreated, m_isLevelCreated));
    if (m_undo) {
      delete m_undo;
      m_undo = 0;
    }

    ToolUtils::updateSaveBox();
  }
}

//---------------------------------------------------------

void TypeTool::addTextToImage() {
  if (!m_validFonts) return;
  TFontManager *instance = TFontManager::instance();

  UINT size = m_string.size();
  if (size == 0) return;

  TImageP img      = getImage(true);
  TVectorImageP vi = img;
  TToonzImageP ti  = img;

  if (!vi && !ti) return;

  if (vi) {
    QMutexLocker lock(vi->getMutex());
    std::vector<const TVectorImage *> images;

    UINT j;
    for (j = 0; j < size; j++) {
      if (m_string[j].isReturn()) continue;

      TPoint descenderP(0, TFontManager::instance()->getLineDescender());
      if (TVectorImageP vi = m_string[j].m_char) {
        vi->transform(
            TTranslation(convert(descenderP) + m_string[j].m_charPosition));
        if (instance->hasVertical() && m_isVertical)
          vi->transform(TRotation(m_startPoint, -90));
        images.push_back(vi.getPointer());
      }
    }
    addTextToVectorImage(vi, images);
  } else if (ti)
    addTextToToonzImage(ti);

  notifyImageChanged();
  //  getApplication()->notifyImageChanges();

  m_string.clear();
  m_cursorIndex = 0;
  m_textBox     = TRectD();
}

//---------------------------------------------------------

void TypeTool::setCursorIndexFromPoint(TPointD point) {
  UINT size = m_string.size();
  int line  = 0;
  int retNum;

  if (!m_isVertical)
    retNum =
        tround((m_startPoint.y + m_dimension - point.y) / m_dimension - 0.5);
  else
    retNum = tround((m_startPoint.x - point.x) / m_dimension + 0.5);

  UINT j = 0;

  for (; line < retNum && j < size; j++)
    if (m_string[j].isReturn()) line++;

  if (j == size) {
    m_cursorIndex  = size;
    m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    return;
  }

  double currentDispl = !m_isVertical ? m_startPoint.x : m_startPoint.y;

  for (; j < size; j++) {
    if (m_string[j].isReturn()) {
      m_cursorIndex  = j;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
      return;
    } else {
      if (!m_isVertical) {
        currentDispl += m_string[j].m_offset;

        if (currentDispl > point.x) {
          if (fabs(currentDispl - m_string[j].m_offset - point.x) <
              fabs(currentDispl - point.x))
            m_cursorIndex = j;
          else
            m_cursorIndex = j + 1;
          m_preeditRange  = std::make_pair(m_cursorIndex, m_cursorIndex);
          return;
        }
      } else {
        if (!TFontManager::instance()->hasVertical()) {
          currentDispl -= m_dimension;
          if (currentDispl < point.y) {
            if (fabs(currentDispl + m_dimension - point.y) <
                fabs(currentDispl - point.y))
              m_cursorIndex = j;
            else
              m_cursorIndex = j + 1;
            m_preeditRange  = std::make_pair(m_cursorIndex, m_cursorIndex);
            return;
          }
        } else {
          currentDispl -= m_string[j].m_offset;

          if (currentDispl < point.y) {
            if (fabs(currentDispl + m_string[j].m_offset - point.y) <
                fabs(currentDispl - point.y))
              m_cursorIndex = j;
            else
              m_cursorIndex = j + 1;
            m_preeditRange  = std::make_pair(m_cursorIndex, m_cursorIndex);
            return;
          }
        }
      }
    }
  }

  if (j == size) {
    m_cursorIndex  = j;
    m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    return;
  }
}

//---------------------------------------------------------

void TypeTool::mouseMove(const TPointD &pos, const TMouseEvent &) {
  updateMouseCursor(pos);
}

//---------------------------------------------------------

void TypeTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TSelection::setCurrent(0);

  if (!m_validFonts) return;

  TImageP img;
  if (!m_active)
    img = touchImage();
  else
    img            = getImage(true);
  TVectorImageP vi = img;
  TToonzImageP ti  = img;

  if (!vi && !ti) return;

  setSize(m_size.getValue());

  if (m_isFrameCreated) {
    if (vi)
      m_undo = new UndoTypeTool(
          0, 0, getApplication()->getCurrentLevel()->getSimpleLevel(),
          getCurrentFid(), m_isFrameCreated, m_isLevelCreated);
    else
      m_undo = new RasterUndoTypeTool(
          0, 0, getApplication()->getCurrentLevel()->getSimpleLevel(),
          getCurrentFid(), m_isFrameCreated, m_isLevelCreated);
  }

  //  closeImeWindow();
  //  if(m_viewer) m_viewer->enableIme(true);
  m_active = true;

  if (!m_string.empty()) {
    TPointD clickPoint =
        (TFontManager::instance()->hasVertical() && m_isVertical)
            ? TRotation(m_startPoint, 90) * pos
            : pos;
    if (m_textBox.contains(clickPoint)) {
      setCursorIndexFromPoint(pos);
      updateCursorPoint();
      invalidate();
      return;
    } else {
      resetInputMethod();
      addTextToImage();
    }
  }

  m_startPoint = pos;
  updateTextBox();
  updateCursorPoint();
  updateMouseCursor(pos);
  //  enableShortcuts(false);
  invalidate();
}

//---------------------------------------------------------

void TypeTool::rightButtonDown(const TPointD &pos, const TMouseEvent &) {
  if (!m_validFonts) return;

  if (!m_string.empty())
    addTextToImage();
  else
    stopEditing();
  m_cursorIndex = 0;
  updateMouseCursor(pos);
  invalidate();
}

//-----------------------------------------------------------------------------

// cancella [from,to[ da m_string e lo rimpiazza con text
// n.b. NON fa updateCharPositions()
void TypeTool::replaceText(std::wstring text, int from, int to) {
  int stringLength = m_string.size();
  from             = tcrop(from, 0, stringLength);
  to               = tcrop(to, from, stringLength);

  // cancello i vecchi caratteri
  m_string.erase(m_string.begin() + from, m_string.begin() + to);

  // aggiungo i nuovi
  TFontManager *instance = TFontManager::instance();
  int styleId            = TTool::getApplication()->getCurrentLevelStyleIndex();

  TImageP img      = getImage(true);
  TToonzImageP ti  = img;
  TVectorImageP vi = img;

  TPoint adv;
  TPointD d_adv;

  for (unsigned int i = 0; i < (unsigned int)text.size(); i++) {
    wchar_t character = text[i];

    if (vi) {
      TVectorImageP characterImage(new TVectorImage());
      unsigned int index = from + i;
      // se il font ha kerning bisogna tenere conto del carattere successivo
      // (se c'e' e non e' un CR) per calcolare correttamente la distanza adv
      TPoint adv;
      if (instance->hasKerning() && index < m_string.size() &&
          !m_string[index].isReturn())
        adv = instance->drawChar(characterImage, character,
                                 m_string[index].m_key);
      else
        adv        = instance->drawChar(characterImage, character);
      TPointD advD = m_scale * TPointD(adv.x, adv.y);

      characterImage->transform(m_scale);
      // colora le aree chiuse
      paintChar(characterImage, styleId);

      m_string.insert(m_string.begin() + index,
                      StrokeChar(characterImage, advD.x, character, styleId));
    } else if (ti) {
      TRasterCM32P newRasterCM;
      TPoint p;
      unsigned int index = from + i;

      if (instance->hasKerning() && (UINT)m_cursorIndex < m_string.size() &&
          !m_string[index].isReturn())
        adv = instance->drawChar((TRasterCM32P &)newRasterCM, p, styleId,
                                 (wchar_t)character,
                                 (wchar_t)m_string[index].m_key);
      else
        adv = instance->drawChar((TRasterCM32P &)newRasterCM, p, styleId,
                                 (wchar_t)character, (wchar_t)0);

      d_adv = m_scale * TPointD((double)(adv.x), (double)(adv.y));

      TToonzImageP newTImage(
          new TToonzImage(newRasterCM, newRasterCM->getBounds()));

      TPalette *vPalette = img->getPalette();
      assert(vPalette);
      newTImage->setPalette(vPalette);

      m_string.insert(m_string.begin() + index,
                      StrokeChar(newTImage, d_adv.x, character, styleId));
    }
  }

  // se necessario ricalcolo il kerning del carattere precedente
  // all'inserimento/cancellazione
  if (instance->hasKerning() && from - 1 >= 0 &&
      !m_string[from - 1].isReturn() && from < (int)m_string.size() &&
      !m_string[from].isReturn()) {
    TPoint adv =
        instance->getDistance(m_string[from - 1].m_key, m_string[from].m_key);
    TPointD advD = m_scale * TPointD((double)(adv.x), (double)(adv.y));
    m_string[from - 1].m_offset = advD.x;
  }
}

//---------------------------------------------------------

void TypeTool::addReturn() {
  TVectorImageP vi(new TVectorImage);
  if ((UINT)m_cursorIndex == m_string.size())
    m_string.push_back(StrokeChar(vi, -1., (int)(QChar('\r').unicode()), 0));
  else
    m_string.insert(m_string.begin() + m_cursorIndex,
                    StrokeChar(vi, -1., (int)(QChar('\r').unicode()), 0));

  m_cursorIndex++;
  m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
  updateCharPositions(m_cursorIndex - 1);
  invalidate();
}

//---------------------------------------------------------

void TypeTool::addBaseChar(std::wstring text) {
  TFontManager *instance = TFontManager::instance();

  TImageP img      = getImage(true);
  TToonzImageP ti  = img;
  TVectorImageP vi = img;

  int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
  TPoint adv;
  TPointD d_adv;

  wchar_t character = text[0];

  if (vi) {
    TVectorImageP newVImage(new TVectorImage);

    if (instance->hasKerning() && (UINT)m_cursorIndex < m_string.size() &&
        !m_string[m_cursorIndex].isReturn())
      adv = instance->drawChar(newVImage, character,
                               m_string[m_cursorIndex].m_key);
    else
      adv = instance->drawChar(newVImage, character);

    newVImage->transform(m_scale);
    paintChar(newVImage, styleId);
    // if(isSketchStyle(styleId))
    //    enableSketchStyle();

    d_adv = m_scale * TPointD((double)(adv.x), (double)(adv.y));

    if ((UINT)m_cursorIndex == m_string.size())
      m_string.push_back(StrokeChar(newVImage, d_adv.x, character, styleId));
    else
      m_string.insert(m_string.begin() + m_cursorIndex,
                      StrokeChar(newVImage, d_adv.x, character, styleId));
  } else if (ti) {
    TRasterCM32P newRasterCM;
    TPoint p;

    if (instance->hasKerning() && (UINT)m_cursorIndex < m_string.size() &&
        !m_string[m_cursorIndex].isReturn())
      adv = instance->drawChar((TRasterCM32P &)newRasterCM, p, styleId,
                               (wchar_t)character,
                               (wchar_t)m_string[m_cursorIndex].m_key);
    else
      adv = instance->drawChar((TRasterCM32P &)newRasterCM, p, styleId,
                               (wchar_t)character, (wchar_t)0);

    // textImage->transform(m_scale);

    d_adv = m_scale * TPointD((double)(adv.x), (double)(adv.y));

    TToonzImageP newTImage(
        new TToonzImage(newRasterCM, newRasterCM->getBounds()));

    TPalette *vPalette = img->getPalette();
    assert(vPalette);
    newTImage->setPalette(vPalette);

    // newTImage->updateRGBM(newRasterCM->getBounds(),vPalette);
    // assert(0);

    if ((UINT)m_cursorIndex == m_string.size())
      m_string.push_back(StrokeChar(newTImage, d_adv.x, character, styleId));
    else
      m_string.insert(m_string.begin() + m_cursorIndex,
                      StrokeChar(newTImage, d_adv.x, character, styleId));
  }

  if (instance->hasKerning() && m_cursorIndex > 0 &&
      !m_string[m_cursorIndex - 1].isReturn()) {
    adv   = instance->getDistance(m_string[m_cursorIndex - 1].m_key, character);
    d_adv = m_scale * TPointD((double)(adv.x), (double)(adv.y));
    m_string[m_cursorIndex - 1].m_offset = d_adv.x;
  }

  m_cursorIndex++;
  updateCharPositions(m_cursorIndex - 1);
  invalidate();
}

//---------------------------------------------------------

void TypeTool::cursorUp() {
  setCursorIndexFromPoint(m_cursorPoint + TPointD(0, 0.5 * m_dimension));
}

//---------------------------------------------------------

void TypeTool::cursorDown() {
  setCursorIndexFromPoint(m_cursorPoint + TPointD(0, -1.5 * m_dimension));
}

//---------------------------------------------------------

void TypeTool::cursorLeft() {
  if (TFontManager::instance()->hasVertical()) {
    m_cursorPoint = TRotation(m_startPoint, -90) * m_cursorPoint;
    setCursorIndexFromPoint(m_cursorPoint + TPointD(-1.5 * m_dimension, 0));
  } else
    setCursorIndexFromPoint(m_cursorPoint + TPointD(-0.5 * m_dimension, 0));
}

//---------------------------------------------------------

void TypeTool::cursorRight() {
  if (TFontManager::instance()->hasVertical()) {
    m_cursorPoint = TRotation(m_startPoint, -90) * m_cursorPoint;
    setCursorIndexFromPoint(m_cursorPoint + TPointD(0.5 * m_dimension, 0));
  } else
    setCursorIndexFromPoint(m_cursorPoint + TPointD(1.5 * m_dimension, 0));
}

//---------------------------------------------------------

void TypeTool::deleteKey() {
  if ((UINT)m_cursorIndex >= m_string.size()) return;
  TFontManager *instance = TFontManager::instance();
  m_string.erase(m_string.begin() + m_cursorIndex);

  if (instance->hasKerning() && m_cursorIndex > 0 &&
      !m_string[m_cursorIndex - 1].isReturn()) {
    TPoint adv;
    if ((UINT)m_cursorIndex < m_string.size() &&
        !m_string[m_cursorIndex].isReturn()) {
      adv = instance->getDistance(m_string[m_cursorIndex - 1].m_key,
                                  m_string[m_cursorIndex].m_key);
    } else {
      adv = instance->getDistance(m_string[m_cursorIndex - 1].m_key, 0);
    }
    TPointD d_adv = m_scale * TPointD((double)(adv.x), (double)(adv.y));
    m_string[m_cursorIndex - 1].m_offset = d_adv.x;
  }
  m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
  updateCharPositions(m_cursorIndex);
  invalidate();
}

//---------------------------------------------------------

bool TypeTool::keyDown(QKeyEvent *event) {
  QString text = event->text();
  if ((event->modifiers() & Qt::ShiftModifier)) text.toUpper();
  std::wstring unicodeChar = text.toStdWString();

  // per sicurezza
  m_preeditRange = std::make_pair(0, 0);

  if (!m_validFonts || !m_active) return true;

  // Se ho premuto solo i tasti ALT, SHIFT e CTRL (da soli) esco
  if (event->modifiers() != Qt::NoModifier && (unicodeChar.empty()))
    return true;

  switch (event->key()) {
  case Qt::Key_Insert:
  case Qt::Key_CapsLock:
    return true;

  case Qt::Key_Home:
  case Qt::Key_PageUp:
    m_cursorIndex  = 0;
    m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    updateCursorPoint();
    invalidate();
    break;

  case Qt::Key_End:
  case Qt::Key_PageDown:
    m_cursorIndex  = (int)m_string.size();
    m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    updateCursorPoint();
    invalidate();
    break;

  /////////////////// cursors
  case Qt::Key_Up:
    if (!m_isVertical)
      cursorUp();
    else if (m_cursorIndex > 0) {
      m_cursorIndex--;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    }
    updateCursorPoint();
    invalidate();
    break;

  case Qt::Key_Down:
    if (!m_isVertical)
      cursorDown();
    else if ((UINT)m_cursorIndex < m_string.size()) {
      m_cursorIndex++;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    }
    updateCursorPoint();
    invalidate();
    break;

  case Qt::Key_Left:
    if (m_isVertical)
      cursorLeft();
    else if (m_cursorIndex > 0) {
      m_cursorIndex--;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    }
    updateCursorPoint();
    invalidate();
    break;

  case Qt::Key_Right:
    if (m_isVertical)
      cursorRight();
    else if ((UINT)m_cursorIndex < m_string.size()) {
      m_cursorIndex++;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    }
    updateCursorPoint();
    invalidate();
    break;

  /////////////////// end cursors

  case Qt::Key_Escape:
    resetInputMethod();
    if (!m_string.empty())
      addTextToImage();
    else
      stopEditing();
    break;

  case Qt::Key_Delete:
    deleteKey();
    break;

  case Qt::Key_Backspace:
    if (m_cursorIndex > 0) {
      m_cursorIndex--;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
      deleteKey();
    }
    break;

  case Qt::Key_Return:
  case Qt::Key_Enter:
    addReturn();
    break;

  default:
    replaceText(unicodeChar, m_cursorIndex, m_cursorIndex);
    m_cursorIndex++;
    m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
    updateCharPositions(m_cursorIndex - 1);
  }

  invalidate();
  return true;
}

//-----------------------------------------------------------------------------

void TypeTool::onInputText(std::wstring preedit, std::wstring commit,
                           int replacementStart, int replacementLen) {
  // butto la vecchia preedit string
  m_preeditRange.first  = std::max(0, m_preeditRange.first);
  m_preeditRange.second = std::min((int)m_string.size(), m_preeditRange.second);
  if (m_preeditRange.first < m_preeditRange.second)
    m_string.erase(m_string.begin() + m_preeditRange.first,
                   m_string.begin() + m_preeditRange.second);

  // inserisco la commit string
  int stringLength = m_string.size();
  int a = tcrop(m_preeditRange.first + replacementStart, 0, stringLength);
  int b = tcrop(m_preeditRange.first + replacementStart + replacementLen, a,
                stringLength);
  replaceText(commit, a, b);
  int index = a + commit.length();

  // inserisco la nuova preedit string
  if (preedit.length() > 0) replaceText(preedit, index, index);
  m_preeditRange.first  = index;
  m_preeditRange.second = index + preedit.length();

  // aggiorno la posizione del cursore
  m_cursorIndex = m_preeditRange.second;
  updateCharPositions(a);
  invalidate();
}

//---------------------------------------------------------

void TypeTool::onActivate() {
  init();
  //  getApplication()->editImage();
  m_string.clear();
  m_textBox     = TRectD(0, 0, 0, 0);
  m_cursorIndex = 0;
}

//---------------------------------------------------------

void TypeTool::onDeactivate() {
  resetInputMethod();
  if (!m_string.empty())
    addTextToImage();  // call internally stopEditing()
  else
    stopEditing();
}

//---------------------------------------------------------

void TypeTool::reset() {
  m_string.clear();
  m_textBox     = TRectD(0, 0, 0, 0);
  m_cursorIndex = 0;
}

//---------------------------------------------------------

void TypeTool::onImageChanged() { stopEditing(); }

//=========================================================

static TTool *getTypeTool() { return &typeTool; }

/*void resetTypetTool(TTool*tool)
{

if (!tool) return;
TypeTool*tt = dynamic_cast<TypeTool*>(tool);
  if (tt)
    tt->stopEditing();
}*/
