

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
#include "toonz/toonzfolders.h"

#include "toonzqt/selection.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/fontstyletranslation.h"
#include "trop.h"
#include "toonz/ttileset.h"
#include "toonz/glrasterpainter.h"
#include "toonz/stage.h"

#include "tfont.h"

// For Qt translation support
#include <QCoreApplication>
#include <QClipboard>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIntValidator>
#include <QMimeData>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QTextCodec>
#include <QTextBoundaryFinder>
#include <QVBoxLayout>

#include <functional>
#include <utility>

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

constexpr int cTextHistoryLimit         = 10;
constexpr qint64 cTextImportWarningSize = 1024 * 1024;

enum class TypeToolTextFormat { PlainText, Markdown };

struct TypeToolFontSettings final {
  QString family;
  QString style;
  QString size;
};

QString textFormatName(TypeToolTextFormat format) {
  return format == TypeToolTextFormat::Markdown ? QStringLiteral("Markdown")
                                                : QStringLiteral("PlainText");
}

TypeToolTextFormat textFormatFromName(const QString &name) {
  return name.compare(QStringLiteral("Markdown"), Qt::CaseInsensitive) == 0
             ? TypeToolTextFormat::Markdown
             : TypeToolTextFormat::PlainText;
}

struct TypeToolTextEntry final {
  QString source;
  TypeToolTextFormat format     = TypeToolTextFormat::PlainText;
  bool preserveSourceLineBreaks = true;
  TypeToolFontSettings font;
};

bool sameTextEntry(const TypeToolTextEntry &first,
                   const TypeToolTextEntry &second) {
  return first.source == second.source && first.format == second.format &&
         first.preserveSourceLineBreaks == second.preserveSourceLineBreaks &&
         first.font.family == second.font.family &&
         first.font.style == second.font.style &&
         first.font.size == second.font.size;
}

QString typeToolSettingsPath() {
  return toQString(ToonzFolder::getMyModuleDir() + TFilePath("typetool.ini"));
}

QString normalizedText(QString text) {
  text.replace("\r\n", "\n");
  text.replace('\r', '\n');
  return text;
}

QStringList graphemeClusters(const QString &text) {
  QStringList clusters;
  QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
  finder.toStart();
  int start = 0;
  int end;
  while ((end = finder.toNextBoundary()) >= 0) {
    if (end > start) clusters.append(text.mid(start, end - start));
    start = end;
  }
  return clusters;
}

class TypeToolTextHistory final {
  bool m_loaded;
  bool m_enabled;
  TypeToolTextEntry m_draft;
  QList<TypeToolTextEntry> m_entries;

  void configureSettings(QSettings &settings) const {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
  }

  void saveHistory() const {
    QSettings values(typeToolSettingsPath(), QSettings::IniFormat);
    configureSettings(values);
    values.beginGroup("General");
    values.setValue("Version", 3);
    values.setValue("HistoryLimit", cTextHistoryLimit);
    values.endGroup();
    values.beginWriteArray("History", m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
      values.setArrayIndex(i);
      const TypeToolTextEntry &entry = m_entries.at(i);
      values.remove("Text");
      values.setValue("Source", entry.source);
      values.setValue("Format", textFormatName(entry.format));
      values.setValue("PreserveLineBreaks", entry.preserveSourceLineBreaks);
      values.setValue("FontFamily", entry.font.family);
      values.setValue("FontStyle", entry.font.style);
      values.setValue("FontSize", entry.font.size);
    }
    values.endArray();
    values.sync();
  }

public:
  TypeToolTextHistory() : m_loaded(false), m_enabled(false) {}

  void load() {
    if (m_loaded) return;
    m_loaded = true;
    QSettings values(typeToolSettingsPath(), QSettings::IniFormat);
    configureSettings(values);
    values.beginGroup("General");
    m_enabled = values.value("EditorEnabled", false).toBool();
    values.endGroup();

    values.beginGroup("Draft");
    m_draft.source =
        normalizedText(values.value("Source", values.value("Text")).toString());
    m_draft.format = textFormatFromName(
        values.value("Format", QStringLiteral("PlainText")).toString());
    m_draft.preserveSourceLineBreaks =
        values.value("PreserveLineBreaks", true).toBool();
    m_draft.font.family = values.value("FontFamily").toString();
    m_draft.font.style  = values.value("FontStyle").toString();
    m_draft.font.size   = values.value("FontSize").toString();
    values.endGroup();

    int count = values.beginReadArray("History");
    for (int i = 0; i < count && m_entries.size() < cTextHistoryLimit; ++i) {
      values.setArrayIndex(i);
      TypeToolTextEntry entry;
      entry.source = normalizedText(
          values.value("Source", values.value("Text")).toString());
      entry.format = textFormatFromName(
          values.value("Format", QStringLiteral("PlainText")).toString());
      entry.preserveSourceLineBreaks =
          values.value("PreserveLineBreaks", true).toBool();
      entry.font.family = values.value("FontFamily").toString();
      entry.font.style  = values.value("FontStyle").toString();
      entry.font.size   = values.value("FontSize").toString();
      bool duplicate = false;
      for (const TypeToolTextEntry &savedEntry : m_entries) {
        if (sameTextEntry(savedEntry, entry)) {
          duplicate = true;
          break;
        }
      }
      if (!entry.source.isEmpty() && !duplicate) m_entries.append(entry);
    }
    values.endArray();
  }

  bool isEnabled() const { return m_enabled; }
  const TypeToolTextEntry &draft() const { return m_draft; }
  const QList<TypeToolTextEntry> &entries() const { return m_entries; }

  void setEnabled(bool enabled) {
    load();
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    QSettings values(typeToolSettingsPath(), QSettings::IniFormat);
    configureSettings(values);
    values.beginGroup("General");
    values.setValue("Version", 3);
    values.setValue("HistoryLimit", cTextHistoryLimit);
    values.setValue("EditorEnabled", m_enabled);
    values.endGroup();
    values.sync();
  }

  void setDraftSource(const QString &text) {
    m_draft.source = normalizedText(text);
  }

  void setDraftFont(const TypeToolFontSettings &font) { m_draft.font = font; }

  void setDraft(const TypeToolTextEntry &entry) {
    m_draft        = entry;
    m_draft.source = normalizedText(m_draft.source);
  }

  void saveDraft() const {
    QSettings values(typeToolSettingsPath(), QSettings::IniFormat);
    configureSettings(values);
    values.beginGroup("General");
    values.setValue("Version", 3);
    values.setValue("HistoryLimit", cTextHistoryLimit);
    values.endGroup();
    values.beginGroup("Draft");
    values.remove("Text");
    values.setValue("Source", m_draft.source);
    values.setValue("Format", textFormatName(m_draft.format));
    values.setValue("PreserveLineBreaks", m_draft.preserveSourceLineBreaks);
    values.setValue("FontFamily", m_draft.font.family);
    values.setValue("FontStyle", m_draft.font.style);
    values.setValue("FontSize", m_draft.font.size);
    values.endGroup();
    values.sync();
  }

  void addEntry(const QString &sourceText) {
    TypeToolTextEntry entry = m_draft;
    entry.source            = normalizedText(sourceText);
    if (entry.source.isEmpty()) return;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
      if (sameTextEntry(m_entries.at(i), entry)) m_entries.removeAt(i);
    }
    m_entries.prepend(entry);
    while (m_entries.size() > cTextHistoryLimit) m_entries.removeLast();
    saveHistory();
  }

  void removeEntry(int index) {
    if (index < 0 || index >= m_entries.size()) return;
    m_entries.removeAt(index);
    saveHistory();
  }

  void clear() {
    m_entries.clear();
    saveHistory();
  }
};

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
  QString m_text;
  int m_styleId;

  StrokeChar(TImageP _char, double offset, const QString &text, int styleId)
      : m_char(_char)
      , m_offset(offset)
      , m_charPosition(TPointD(0, 0))
      , m_text(text)
      , m_styleId(styleId) {}

  bool isReturn() const { return m_text == QString(QChar('\r')); }
  bool isSpace() const {
    return !m_text.isEmpty() && m_text.at(0).isSpace();
  }

  void update(TAffine scale, const QString &nextText = QString()) {
    if (!isReturn()) {
      if (TVectorImageP vi = m_char) {
        vi = m_char = new TVectorImage;
        TPoint adv =
            TFontManager::instance()->drawText(vi, m_text, nextText);
        vi->transform(scale);
        paintChar(vi, m_styleId);
        m_offset = (scale * TPointD((double)(adv.x), (double)(adv.y))).x;
      } else {
        TRasterCM32P newRasterCM;
        TPoint p;
        TPoint adv = TFontManager::instance()->drawText(
            (TRasterCM32P &)newRasterCM, p, m_styleId, m_text, nextText);
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

class TypeToolTextHistoryPopup;

class TypeTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(TypeTool)

  // Properties
  TEnumProperty m_fontFamilyMenu;
  TEnumProperty m_typeFaceMenu;
  TBoolProperty m_vertical;
  TBoolProperty m_textHistoryEnabled;
  TEnumProperty m_size;
  TPropertyGroup m_prop[2];

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
  TypeToolTextHistory m_textHistory;
  TypeToolTextHistoryPopup *m_textHistoryPopup;

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
  TypeToolFontSettings currentFontSettings() const;
  QStringList availableFontFamilies() const;
  QStringList availableFontStyles() const;
  QStringList availableFontSizes() const;
  void applyFontSettings(const TypeToolFontSettings &font);
  void draw() override;

  void updateMouseCursor(const TPointD &pos);
  void updateStrokeChar();
  void updateCharPositions(int updateFrom = 0);
  void updateCursorPoint();
  void updateTextBox();

  void setCursorIndexFromPoint(TPointD point);

  void mouseMove(const TPointD &pos, const TMouseEvent &) override;
  bool preLeftButtonDown() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void rightButtonDown(const TPointD &pos, const TMouseEvent &) override;
  bool keyDown(QKeyEvent *event) override;

  void onInputText(const std::wstring &preedit, const std::wstring &commit,
                   int replacementStart, int replacementLen) override;

  // cancella gli StrokeChar fra from e to-1 e inserisce nuovi StrokeChar
  // corrispondenti a text a partire da from
  void replaceText(const QString &text, int from, int to);

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
  QString currentText() const;
  void setTextFromHistory(const QString &text);
  void syncTextHistoryDraft();
  void syncTextHistoryFontSettings();
  void showTextHistoryPopup();
  void hideTextHistoryPopup();

  void reset() override;

  void onActivate() override;
  void onDeactivate() override;
  void onImageChanged() override;

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return m_cursorId;
  }

  bool onPropertyChanged(std::string propertyName) override;

  TPropertyGroup *getProperties(int targetType) override {
    return &m_prop[targetType];
  }

  int getColorClass() const { return 1; }
};

//---------------------------------------------------------

class TypeToolTextHistoryPopup final : public DVGui::Dialog {
  Q_DECLARE_TR_FUNCTIONS(TypeToolTextHistoryPopup)

  TypeTool *m_tool;
  TypeToolTextHistory &m_history;
  QListWidget *m_historyList;
  QPlainTextEdit *m_editor;
  QComboBox *m_fontFamilyCombo;
  QComboBox *m_fontStyleCombo;
  QComboBox *m_fontSizeCombo;
  QComboBox *m_formatCombo;
  QCheckBox *m_preserveLineBreaks;
  QTimer *m_saveTimer;
  std::function<void(const QString &)> m_textChanged;

  TypeToolTextEntry editorEntry() const {
    TypeToolTextEntry entry;
    entry.source                   = m_editor->toPlainText();
    entry.format                   = m_formatCombo->currentIndex() == 1
                                         ? TypeToolTextFormat::Markdown
                                         : TypeToolTextFormat::PlainText;
    entry.preserveSourceLineBreaks = m_preserveLineBreaks->isChecked();
    entry.font.family              = m_fontFamilyCombo->currentText();
    entry.font.style               = m_fontStyleCombo->currentData().toString();
    entry.font.size                = m_fontSizeCombo->currentText();
    return entry;
  }

  void refreshFontControls() {
    TypeToolFontSettings font = m_tool->currentFontSettings();
    QSignalBlocker familyBlocker(m_fontFamilyCombo);
    QSignalBlocker styleBlocker(m_fontStyleCombo);
    QSignalBlocker sizeBlocker(m_fontSizeCombo);

    m_fontFamilyCombo->clear();
    m_fontFamilyCombo->addItems(m_tool->availableFontFamilies());
    m_fontFamilyCombo->setCurrentText(font.family);

    DVGui::populateFontStyleCombo(m_fontStyleCombo,
                                  m_tool->availableFontStyles(), font.style);

    m_fontSizeCombo->clear();
    m_fontSizeCombo->addItems(m_tool->availableFontSizes());
    m_fontSizeCombo->setCurrentText(font.size);
  }

  void applyFontControls(bool familyChanged = false) {
    TypeToolFontSettings font;
    font.family = m_fontFamilyCombo->currentText();
    if (!familyChanged) font.style = m_fontStyleCombo->currentData().toString();
    font.size = m_fontSizeCombo->currentText();
    m_tool->applyFontSettings(font);
    refreshFontControls();
    scheduleDraftSave();
  }

  void setEditorEntry(const TypeToolTextEntry &entry, bool notify = true) {
    TypeToolTextEntry resolvedEntry  = entry;
    TypeToolFontSettings currentFont = m_tool->currentFontSettings();
    if (resolvedEntry.font.family.isEmpty())
      resolvedEntry.font.family = currentFont.family;
    if (resolvedEntry.font.style.isEmpty())
      resolvedEntry.font.style = currentFont.style;
    if (resolvedEntry.font.size.isEmpty())
      resolvedEntry.font.size = currentFont.size;
    m_tool->applyFontSettings(resolvedEntry.font);

    QSignalBlocker editorBlocker(m_editor);
    QSignalBlocker formatBlocker(m_formatCombo);
    QSignalBlocker lineBreakBlocker(m_preserveLineBreaks);
    m_editor->setPlainText(resolvedEntry.source);
    m_formatCombo->setCurrentIndex(
        resolvedEntry.format == TypeToolTextFormat::Markdown ? 1 : 0);
    m_preserveLineBreaks->setChecked(resolvedEntry.preserveSourceLineBreaks);
    m_preserveLineBreaks->setEnabled(resolvedEntry.format ==
                                     TypeToolTextFormat::Markdown);
    refreshFontControls();
    resolvedEntry.font = m_tool->currentFontSettings();
    m_history.setDraft(resolvedEntry);
    if (notify) {
      m_saveTimer->start();
      if (m_textChanged) m_textChanged(resolvedEntry.source);
    }
  }

  void importTextDocument() {
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Import Text Document"), QString(),
        tr("Text documents (*.txt *.md *.markdown);;Plain text (*.txt);;"
           "Markdown (*.md *.markdown)"));
    if (fileName.isEmpty()) return;

    QFileInfo fileInfo(fileName);
    if (fileInfo.size() > cTextImportWarningSize &&
        QMessageBox::question(
            this, tr("Import Large Text Document"),
            tr("This file is larger than 1 MB and may be slow to convert to "
               "Type Tool geometry. Import it anyway?")) != QMessageBox::Yes)
      return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::warning(this, tr("Import Text Document"),
                           tr("The selected file could not be opened."));
      return;
    }

    QByteArray bytes = file.readAll();
    QTextCodec::ConverterState state;
    QString source = QTextCodec::codecForName("UTF-8")->toUnicode(
        bytes.constData(), bytes.size(), &state);
    if (state.invalidChars > 0 &&
        QMessageBox::question(
            this, tr("Invalid UTF-8 Text"),
            tr("The file contains invalid UTF-8 data. Invalid characters "
               "will be replaced. Continue?")) != QMessageBox::Yes)
      return;

    if (!source.isEmpty() && source.at(0) == QChar::ByteOrderMark)
      source.remove(0, 1);

    TypeToolTextEntry entry;
    entry.source   = normalizedText(source);
    QString suffix = fileInfo.suffix().toLower();
    entry.format =
        suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")
            ? TypeToolTextFormat::Markdown
            : TypeToolTextFormat::PlainText;
    entry.preserveSourceLineBreaks = true;
    setEditorEntry(entry);
    m_editor->setFocus();
  }

  void rebuildHistoryList() {
    int oldRow = m_historyList->currentRow();
    QSignalBlocker blocker(m_historyList);
    m_historyList->clear();
    for (const TypeToolTextEntry &entry : m_history.entries()) {
      QString label = entry.source;
      label.replace('\n', QChar(0x21B5));
      if (label.size() > 80) label = label.left(77) + QStringLiteral("...");
      if (entry.format == TypeToolTextFormat::Markdown)
        label.prepend(QStringLiteral("[MD] "));
      QString fontLabel =
          QStringLiteral("%1, %2, %3")
              .arg(entry.font.family,
                   DVGui::fontStyleDisplayName(entry.font.style),
                   entry.font.size);
      if (!entry.font.family.isEmpty())
        label.prepend(QStringLiteral("[%1] ").arg(fontLabel));
      QListWidgetItem *item = new QListWidgetItem(label, m_historyList);
      QString toolTip       = entry.source;
      if (!entry.font.family.isEmpty())
        toolTip += QStringLiteral("\n\n%1: %2\n%3: %4\n%5: %6")
                       .arg(tr("Font"), entry.font.family, tr("Style"),
                            DVGui::fontStyleDisplayName(entry.font.style),
                            tr("Size"), entry.font.size);
      item->setToolTip(toolTip);
    }
    if (oldRow >= 0 && oldRow < m_historyList->count())
      m_historyList->setCurrentRow(oldRow);
  }

  void scheduleDraftSave() {
    m_history.setDraft(editorEntry());
    m_saveTimer->start();
  }

public:
  TypeToolTextHistoryPopup(QWidget *parent, TypeTool *tool,
                           TypeToolTextHistory &history,
                           std::function<void(const QString &)> textChanged)
      : DVGui::Dialog(parent, false, false, QStringLiteral("TypeToolHistory"))
      , m_tool(tool)
      , m_history(history)
      , m_historyList(new QListWidget(this))
      , m_editor(new QPlainTextEdit(this))
      , m_fontFamilyCombo(new QComboBox(this))
      , m_fontStyleCombo(new QComboBox(this))
      , m_fontSizeCombo(new QComboBox(this))
      , m_formatCombo(new QComboBox(this))
      , m_preserveLineBreaks(
            new QCheckBox(tr("Preserve source line breaks for Markdown"), this))
      , m_saveTimer(new QTimer(this))
      , m_textChanged(std::move(textChanged)) {
    setWindowTitle(tr("Type Tool Text History"));
    setMinimumSize(520, 460);

    m_historyList->setAlternatingRowColors(true);
    m_historyList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_editor->setPlaceholderText(tr("Enter text to place with the Type Tool."));
    m_fontSizeCombo->setEditable(true);
    m_fontSizeCombo->lineEdit()->setValidator(
        new QIntValidator(1, 1000, m_fontSizeCombo));
    m_formatCombo->addItem(tr("Plain Text"));
    m_formatCombo->addItem(tr("Markdown"));
    setEditorEntry(m_history.draft(), false);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(new QLabel(tr("Editable Text"), this));
    layout->addWidget(m_editor, 2);

    QHBoxLayout *fontLayout = new QHBoxLayout;
    fontLayout->addWidget(new QLabel(tr("Font:"), this));
    fontLayout->addWidget(m_fontFamilyCombo, 2);
    fontLayout->addWidget(new QLabel(tr("Style:"), this));
    fontLayout->addWidget(m_fontStyleCombo, 1);
    fontLayout->addWidget(new QLabel(tr("Size:"), this));
    fontLayout->addWidget(m_fontSizeCombo);
    layout->addLayout(fontLayout);

    QHBoxLayout *formatLayout = new QHBoxLayout;
    formatLayout->addWidget(new QLabel(tr("Document Format:"), this));
    formatLayout->addWidget(m_formatCombo);
    formatLayout->addWidget(m_preserveLineBreaks);
    formatLayout->addStretch(1);
    layout->addLayout(formatLayout);
    QLabel *markdownNote = new QLabel(
        tr("Markdown source is preserved for formatted generation; the "
           "current Type Tool places the editable source text."),
        this);
    markdownNote->setWordWrap(true);
    layout->addWidget(markdownNote);
    layout->addWidget(new QLabel(tr("Recent Text (last 10)"), this));
    layout->addWidget(m_historyList, 1);

    QPushButton *importButton = new QPushButton(tr("Import Text..."), this);
    QPushButton *deleteButton = new QPushButton(tr("Delete Entry"), this);
    QPushButton *clearButton  = new QPushButton(tr("Clear History"), this);
    QPushButton *closeButton  = new QPushButton(tr("Close"), this);
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
    addLayout(layout);

    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(300);

    connect(m_saveTimer, &QTimer::timeout, this,
            [this]() { m_history.saveDraft(); });
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
      scheduleDraftSave();
      if (m_textChanged) m_textChanged(m_editor->toPlainText());
    });
    connect(
        m_formatCombo,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          m_preserveLineBreaks->setEnabled(index == 1);
          scheduleDraftSave();
        });
    connect(m_preserveLineBreaks, &QCheckBox::toggled, this,
            [this]() { scheduleDraftSave(); });
    connect(m_fontFamilyCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [this](int) { applyFontControls(true); });
    connect(m_fontStyleCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [this](int) { applyFontControls(); });
    connect(m_fontSizeCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [this](int) { applyFontControls(); });
    connect(m_fontSizeCombo->lineEdit(), &QLineEdit::editingFinished, this,
            [this]() { applyFontControls(); });
    connect(m_historyList, &QListWidget::currentRowChanged, this,
            [this](int row) {
              if (row < 0 || row >= m_history.entries().size()) return;
              setEditorEntry(m_history.entries().at(row));
              m_editor->setFocus();
            });
    connect(importButton, &QPushButton::clicked, this,
            [this]() { importTextDocument(); });
    connect(deleteButton, &QPushButton::clicked, this, [this]() {
      int row = m_historyList->currentRow();
      if (row < 0) return;
      m_history.removeEntry(row);
      rebuildHistoryList();
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
      if (QMessageBox::question(
              this, tr("Clear Text History"),
              tr("Remove all saved Type Tool text entries?")) !=
          QMessageBox::Yes)
        return;
      m_history.clear();
      rebuildHistoryList();
    });
    connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);
    connect(this, &DVGui::Dialog::dialogClosed, this, [this]() {
      m_saveTimer->stop();
      m_history.setDraft(editorEntry());
      m_history.saveDraft();
    });

    rebuildHistoryList();
  }

  void setEditorText(const QString &text) {
    QString normalized = normalizedText(text);
    if (m_editor->toPlainText() == normalized) return;
    QSignalBlocker blocker(m_editor);
    m_editor->setPlainText(normalized);
    m_history.setDraftSource(normalized);
    m_saveTimer->start();
  }

  void syncFontControls() {
    refreshFontControls();
    m_history.setDraftFont(m_tool->currentFontSettings());
    m_saveTimer->start();
  }

  void refreshHistory() { rebuildHistoryList(); }

  void showAndRaise() {
    refreshHistory();
    show();
    raise();
    activateWindow();
  }
};

TypeTool typeTool;

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
    , m_textHistoryEnabled("Text History", false)
    , m_size("Size:")  // W_ToolOptions_Size
    , m_undo(0)
    , m_textHistoryPopup(nullptr) {
  bind(TTool::VectorImage | TTool::ToonzImage | TTool::EmptyTarget);
  m_prop[0].bind(m_fontFamilyMenu);
  // Su mac non e' visibile il menu dello style perche' e' stato inserito nel
  // nome
  // della font.
  //#ifndef MACOSX
  m_prop[1].bind(m_typeFaceMenu);
  //#endif
  m_prop[1].bind(m_size);
  m_prop[1].bind(m_vertical);
  m_prop[1].bind(m_textHistoryEnabled);
  m_vertical.setId("Orientation");
  m_textHistoryEnabled.setId("TextHistory");
  m_fontFamilyMenu.setId("TypeFont");
  m_typeFaceMenu.setId("TypeStyle");
  m_size.setId("TypeSize");
}

//---------------------------------------------------------

TypeTool::~TypeTool() {}

//---------------------------------------------------------

QString TypeTool::currentText() const {
  QString text;
  for (const StrokeChar &character : m_string) {
    if (character.isReturn())
      text.append('\n');
    else
      text.append(character.m_text);
  }
  return text;
}

//---------------------------------------------------------

void TypeTool::setTextFromHistory(const QString &sourceText) {
  if (!m_active || !m_validFonts || !getImage(false)) return;

  QString text = normalizedText(sourceText);
  if (currentText() == text) return;

  QString typeToolText = text;
  typeToolText.replace('\n', '\r');
  replaceText(typeToolText, 0, static_cast<int>(m_string.size()));
  m_cursorIndex  = static_cast<int>(m_string.size());
  m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
  updateCharPositions();
  invalidate();
}

//---------------------------------------------------------

void TypeTool::syncTextHistoryDraft() {
  if (!m_textHistoryEnabled.getValue()) return;
  QString text = currentText();
  m_textHistory.setDraftSource(text);
  if (m_textHistoryPopup)
    m_textHistoryPopup->setEditorText(text);
  else
    m_textHistory.saveDraft();
}

//---------------------------------------------------------

void TypeTool::syncTextHistoryFontSettings() {
  if (!m_textHistoryEnabled.getValue()) return;
  m_textHistory.setDraftFont(currentFontSettings());
  if (m_textHistoryPopup)
    m_textHistoryPopup->syncFontControls();
  else
    m_textHistory.saveDraft();
}

//---------------------------------------------------------

void TypeTool::showTextHistoryPopup() {
  if (!m_textHistoryEnabled.getValue()) return;
  if (!m_textHistoryPopup) {
    m_textHistoryPopup = new TypeToolTextHistoryPopup(
        QApplication::activeWindow(), this, m_textHistory,
        [this](const QString &text) { setTextFromHistory(text); });
    QObject::connect(m_textHistoryPopup, &QObject::destroyed,
                     [this]() { m_textHistoryPopup = nullptr; });
  }
  m_textHistoryPopup->showAndRaise();
}

//---------------------------------------------------------

void TypeTool::hideTextHistoryPopup() {
  if (m_textHistoryPopup) m_textHistoryPopup->hide();
}

//---------------------------------------------------------

void TypeTool::updateTranslation() {
  m_fontFamilyMenu.setQStringName(tr("Font:"));
  m_typeFaceMenu.setQStringName(tr("Style:"));
  m_vertical.setQStringName(tr("Vertical Orientation"));
  m_textHistoryEnabled.setQStringName(tr("Text History"));
  m_size.setQStringName(tr("Size:"));
  QStringList styles = availableFontStyles();
  QStringList labels = DVGui::fontStyleDisplayNames(styles);
  for (int i = 0; i < styles.size(); ++i)
    m_typeFaceMenu.setItemUIName(styles[i].toStdWString(), labels[i]);
}

//---------------------------------------------------------

bool TypeTool::onPropertyChanged(std::string propertyName) {
  if (propertyName == m_textHistoryEnabled.getName()) {
    m_textHistory.load();
    bool enabled = m_textHistoryEnabled.getValue();
    m_textHistory.setEnabled(enabled);
    if (enabled)
      showTextHistoryPopup();
    else
      hideTextHistoryPopup();
    return true;
  }

  if (!m_validFonts) return false;

  if (propertyName == m_fontFamilyMenu.getName()) {
    setFont(m_fontFamilyMenu.getValue());
    syncTextHistoryFontSettings();
    return true;
  } else if (propertyName == m_typeFaceMenu.getName()) {
    setTypeface(m_typeFaceMenu.getValue());
    syncTextHistoryFontSettings();
    return true;
  } else if (propertyName == m_size.getName()) {
    setSize(m_size.getValue());
    syncTextHistoryFontSettings();
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

  m_textHistory.load();
  m_textHistoryEnabled.setValue(m_textHistory.isEnabled());

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
  QStringList styles;
  for (const std::wstring &typeface : typefaces)
    styles.append(QString::fromStdWString(typeface));
  const QStringList labels = DVGui::fontStyleDisplayNames(styles);
  for (int i = 0; i < styles.size(); ++i)
    m_typeFaceMenu.addValueWithUIName(typefaces[i], labels[i]);
  if (m_typeFaceMenu.isValue(oldTypeface)) m_typeFaceMenu.setValue(oldTypeface);

  TTool::getApplication()->getCurrentTool()->notifyToolComboBoxListChanged(
      m_typeFaceMenu.getName());
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

TypeToolFontSettings TypeTool::currentFontSettings() const {
  TypeToolFontSettings font;
  font.family = QString::fromStdWString(m_fontFamilyMenu.getValue());
  font.style  = QString::fromStdWString(m_typeFaceMenu.getValue());
  font.size   = QString::fromStdWString(m_size.getValue());
  return font;
}

//---------------------------------------------------------

QStringList TypeTool::availableFontFamilies() const {
  QStringList families;
  for (const std::wstring &family : m_fontFamilyMenu.getRange())
    families.append(QString::fromStdWString(family));
  return families;
}

//---------------------------------------------------------

QStringList TypeTool::availableFontStyles() const {
  QStringList styles;
  for (const std::wstring &style : m_typeFaceMenu.getRange())
    styles.append(QString::fromStdWString(style));
  return styles;
}

//---------------------------------------------------------

QStringList TypeTool::availableFontSizes() const {
  QStringList sizes;
  for (const std::wstring &size : m_size.getRange())
    sizes.append(QString::fromStdWString(size));
  return sizes;
}

//---------------------------------------------------------

void TypeTool::applyFontSettings(const TypeToolFontSettings &font) {
  bool changed = false;

  std::wstring family  = font.family.toStdWString();
  bool familyAvailable = family.empty() || m_fontFamilyMenu.isValue(family);
  if (!family.empty() && familyAvailable) {
    if (m_fontFamilyMenu.getValue() != family) {
      m_fontFamilyMenu.setValue(family);
      setFont(family);
      changed = true;
    }
  }

  std::wstring style = font.style.toStdWString();
  if (familyAvailable && !style.empty() && m_typeFaceMenu.isValue(style) &&
      m_typeFaceMenu.getValue() != style) {
    m_typeFaceMenu.setValue(style);
    setTypeface(style);
    changed = true;
  }

  bool validSize = false;
  int size       = font.size.toInt(&validSize);
  if (validSize && size > 0 && size <= 1000) {
    std::wstring sizeValue = QString::number(size).toStdWString();
    if (!m_size.isValue(sizeValue)) {
      m_size.addValue(sizeValue);
      TTool::getApplication()->getCurrentTool()->notifyToolComboBoxListChanged(
          m_size.getName());
    }
    if (m_size.getValue() != sizeValue) {
      m_size.setValue(sizeValue);
      setSize(sizeValue);
      changed = true;
    }
  }

  if (changed) TTool::getApplication()->getCurrentTool()->notifyToolChanged();
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
      m_string[i].update(/*m_font,*/ m_scale, m_string[i + 1].m_text);
    else
      m_string[i].update(/*m_font,*/ m_scale);
  }

  updateCharPositions();
}

//---------------------------------------------------------

void TypeTool::updateCharPositions(int updateFrom) {
  if (updateFrom < 0) updateFrom = 0;
  UINT size = m_string.size();
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
      if (m_string[j].isReturn() || m_string[j].isSpace())
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
  double ascent          = (double)(instance->getLineAscender()) * m_scale.a11;
  double descent         = (double)(instance->getLineDescender()) * m_scale.a11;
  double height          = (double)(instance->getHeight()) * m_scale.a11;
  double vLineSpacing =
      (double)(instance->getAverageCharWidth()) * 2.0 * m_scale.a11;
  m_fontYOffset          = (double)(instance->getLineSpacing()) * m_scale.a11;

  if (m_string.empty()) {
    if (!m_isVertical || instance->hasVertical())
      m_cursorPoint = m_startPoint + TPointD(0, ascent);
    else
      m_cursorPoint = m_startPoint;
  } else if (m_cursorIndex == (int)m_string.size()) {
    // Horizontal case
    if (!m_isVertical || instance->hasVertical()) {
      if (m_string.back().isReturn())
        m_cursorPoint =
            TPointD(m_startPoint.x, m_string.back().m_charPosition.y -
                                        m_fontYOffset + ascent + descent);
      else
        m_cursorPoint = m_string.back().m_charPosition +
                        TPointD(m_string.back().m_offset, 0) +
                        TPointD(0, ascent + descent);
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
      m_cursorPoint =
          m_string[m_cursorIndex].m_charPosition + TPointD(0, ascent + descent);
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
  double ascent          = (double)(instance->getLineAscender()) * m_scale.a11;
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
               m_startPoint.x + maxXLength, m_startPoint.y + ascent)
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
      tglDrawSegment(
          m_cursorPoint,
          m_cursorPoint + TPointD(0, -instance->getHeight() * m_scale.a11));
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
  QString committedText = currentText();

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

  if (m_textHistoryEnabled.getValue()) {
    m_textHistory.setDraftFont(currentFontSettings());
    m_textHistory.addEntry(committedText);
    m_textHistory.setDraftSource(committedText);
    m_textHistory.saveDraft();
    if (m_textHistoryPopup) {
      m_textHistoryPopup->setEditorText(committedText);
      m_textHistoryPopup->refreshHistory();
    }
  }

  m_string.clear();
  m_cursorIndex = 0;
  m_textBox     = TRectD();
}

//---------------------------------------------------------

void TypeTool::setCursorIndexFromPoint(TPointD point) {
  UINT size = m_string.size();
  int line  = 0;
  int retNum;

  if (!m_isVertical) {
    TFontManager *instance = TFontManager::instance();
    double ascent          = instance->getLineAscender() * m_scale.a11;
    double lineSpacing     = instance->getLineSpacing() * m_scale.a11;
    retNum = tfloor((m_startPoint.y + ascent - point.y) / lineSpacing);
    if (retNum < 0) retNum = 0;
  } else
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
          m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
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
            m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
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
            m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
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

bool TypeTool::preLeftButtonDown() {
  if (getViewer() && getViewer()->getGuidedStrokePickerMode()) return false;

  if (m_validFonts && !m_active) touchImage();
  return true;
}

//---------------------------------------------------------

void TypeTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TSelection::setCurrent(0);

  if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
    getViewer()->doPickGuideStroke(pos);
    return;
  }

  if (!m_validFonts) return;

  TImageP img      = getImage(true);
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
  // The click marks the top of the first horizontal line. Internally the
  // character positions remain baseline-based.
  if (!m_isVertical)
    m_startPoint.y -= TFontManager::instance()->getLineAscender() * m_scale.a11;
  updateTextBox();
  updateCursorPoint();
  if (m_textHistoryEnabled.getValue()) {
    showTextHistoryPopup();
    setTextFromHistory(m_textHistory.draft().source);
  }
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
void TypeTool::replaceText(const QString &text, int from, int to) {
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

  QStringList clusters = graphemeClusters(text);
  for (int i = 0; i < clusters.size(); i++) {
    const QString &character = clusters.at(i);

    // line break case. This can happen when pasting text including the line
    // break
    if (character == QString(QChar('\r'))) {
      TVectorImageP vi(new TVectorImage);
      unsigned int index = from + i;
      m_string.insert(m_string.begin() + index,
                      StrokeChar(vi, -1., QString(QChar('\r')), 0));
    }

    else if (vi) {
      TVectorImageP characterImage(new TVectorImage());
      unsigned int index = from + i;
      // se il font ha kerning bisogna tenere conto del carattere successivo
      // (se c'e' e non e' un CR) per calcolare correttamente la distanza adv
      TPoint adv;
      if (instance->hasKerning() && index < m_string.size() &&
          !m_string[index].isReturn())
        adv = instance->drawText(characterImage, character,
                                 m_string[index].m_text);
      else
        adv = instance->drawText(characterImage, character);
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
          index < m_string.size() - 1 && !m_string[index].isReturn())
        adv = instance->drawText((TRasterCM32P &)newRasterCM, p, styleId,
                                 character, m_string[index].m_text);
      else
        adv = instance->drawText((TRasterCM32P &)newRasterCM, p, styleId,
                                 character);

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
        instance->getDistance(m_string[from - 1].m_text,
                              m_string[from].m_text);
    TPointD advD = m_scale * TPointD((double)(adv.x), (double)(adv.y));
    m_string[from - 1].m_offset = advD.x;
  }
}

//---------------------------------------------------------

void TypeTool::addReturn() {
  TVectorImageP vi(new TVectorImage);
  if ((UINT)m_cursorIndex == m_string.size())
    m_string.push_back(StrokeChar(vi, -1., QString(QChar('\r')), 0));
  else
    m_string.insert(m_string.begin() + m_cursorIndex,
                    StrokeChar(vi, -1., QString(QChar('\r')), 0));

  m_cursorIndex++;
  m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
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
      adv = instance->getDistance(m_string[m_cursorIndex - 1].m_text,
                                  m_string[m_cursorIndex].m_text);
    } else {
      adv = instance->getDistance(m_string[m_cursorIndex - 1].m_text);
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
  if ((event->modifiers() & Qt::ShiftModifier)) text = text.toUpper();

  if (QKeySequence(event->key() + event->modifiers()) == QKeySequence::Paste) {
    QClipboard *clipboard     = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData->hasText()) return true;
    text = mimeData->text().replace('\n', '\r');
  }

  // return if only ALT, SHIFT or CTRL key is pressed
  if (event->modifiers() != Qt::NoModifier && text.isEmpty())
    return true;

  // per sicurezza
  m_preeditRange = std::make_pair(0, 0);

  if (!m_validFonts || !m_active) return true;

  bool textChanged = false;

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
    textChanged = true;
    break;

  case Qt::Key_Backspace:
    if (m_cursorIndex > 0) {
      m_cursorIndex--;
      m_preeditRange = std::make_pair(m_cursorIndex, m_cursorIndex);
      deleteKey();
      textChanged = true;
    }
    break;

  case Qt::Key_Return:
  case Qt::Key_Enter:
    addReturn();
    textChanged = true;
    break;

  default:
    if (text.isEmpty()) return false;
    replaceText(text, m_cursorIndex, m_cursorIndex);
    int startIndex = m_cursorIndex + 1;
    m_cursorIndex += graphemeClusters(text).size();
    m_preeditRange = std::make_pair(startIndex, m_cursorIndex);
    updateCharPositions(startIndex - 1);
    textChanged = true;
  }

  if (textChanged) syncTextHistoryDraft();
  invalidate();
  return true;
}

//-----------------------------------------------------------------------------

void TypeTool::onInputText(const std::wstring &preedit,
                           const std::wstring &commit, int replacementStart,
                           int replacementLen) {
  QString preeditText = QString::fromStdWString(preedit);
  QString commitText  = QString::fromStdWString(commit);
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
  replaceText(commitText, a, b);
  int index = a + graphemeClusters(commitText).size();

  // inserisco la nuova preedit string
  if (!preeditText.isEmpty()) replaceText(preeditText, index, index);
  m_preeditRange.first  = index;
  m_preeditRange.second = index + graphemeClusters(preeditText).size();

  // aggiorno la posizione del cursore
  m_cursorIndex = m_preeditRange.second;
  updateCharPositions(a);
  syncTextHistoryDraft();
  invalidate();
}

//---------------------------------------------------------

void TypeTool::onActivate() {
  init();
  //  getApplication()->editImage();
  m_string.clear();
  m_textBox     = TRectD(0, 0, 0, 0);
  m_cursorIndex = 0;
  if (m_textHistoryEnabled.getValue()) showTextHistoryPopup();
}

//---------------------------------------------------------

void TypeTool::onDeactivate() {
  resetInputMethod();
  if (!m_string.empty())
    addTextToImage();  // call internally stopEditing()
  else
    stopEditing();
  hideTextHistoryPopup();
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
