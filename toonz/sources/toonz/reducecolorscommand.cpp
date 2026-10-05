#include "reducecolorscommand.h"
#include "palettecolorreduction.h"

#include "tapp.h"
#include "tenv.h"
#include "menubarcommandids.h"
#include "tools/toolutils.h"
#include "toonz/levelset.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/ttilesaver.h"
#include "toonz/ttileset.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/styleselection.h"
#include "toonzqt/tselectionhandle.h"
#include "tsimplecolorstyles.h"
#include "ttoonzimage.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QProgressDialog>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <bitset>
#include <limits>
#include <memory>
#include <stdexcept>
#include <typeinfo>

namespace {

using namespace PaletteColorReduction;
using StyleMask = std::bitset<4096>;
bool reducing   = false;

// Follow the other conversion dialogs: last accepted options are retained
// across sessions, while context-dependent limits are applied only in the UI.
TEnv::IntVar ReduceColorsMode("ReduceColorsMode", 0);
TEnv::IntVar ReduceColorsTarget("ReduceColorsTarget", 16);
TEnv::IntVar ReduceColorsAutomaticTolerance("ReduceColorsAutomaticTolerance",
                                            1);
TEnv::DoubleVar ReduceColorsTolerance("ReduceColorsTolerance", 3.0);
TEnv::IntVar ReduceColorsCleanup("ReduceColorsCleanup", 1);
TEnv::IntVar ReduceColorsRenumber("ReduceColorsRenumber", 0);
TEnv::IntVar ReduceColorsSelectedScope("ReduceColorsSelectedScope", 1);

std::vector<std::vector<int>> palettePages(TPalette *palette) {
  std::vector<std::vector<int>> pages(palette->getPageCount());
  for (int p = 0; p < palette->getPageCount(); ++p) {
    TPalette::Page *page = palette->getPage(p);
    for (int i = 0; i < page->getStyleCount(); ++i)
      pages[p].push_back(page->getStyleId(i));
  }
  return pages;
}

StyleMask paletteScope(TPalette *palette) {
  StyleMask styles;
  // Removed chips keep their numeric slots in TPalette. Only visible palette
  // entries should count toward the dialog's scope and target limit.
  for (int p = 0; p < palette->getPageCount(); ++p) {
    TPalette::Page *page = palette->getPage(p);
    for (int i = 0; i < page->getStyleCount(); ++i) {
      int id = page->getStyleId(i);
      if (id > 0 && id < int(styles.size())) styles.set(id);
    }
  }
  return styles;
}

bool getTarget(TXshSimpleLevel *&level, StyleMask &styles, bool &allStyles) {
  TApp *app = TApp::instance();
  if (app->getCurrentFrame()->isPlaying()) return false;
  TXshCell cell = TTool::getImageCell();
  level         = cell.getSimpleLevel();
  if (!level || level->getType() != TZP_XSHLEVEL || level->isReadOnly() ||
      level->isSubsequence())
    return false;
  if (!app->getCurrentFrame()->isEditingLevel()) {
    TXshColumn *column = app->getCurrentXsheet()->getXsheet()->getColumn(
        app->getCurrentColumn()->getColumnIndex());
    if (!column || column->isLocked()) return false;
  }
  TPalette *palette = app->getCurrentPalette()->getPalette();
  if (!palette || palette != level->getPalette() ||
      palette->isCleanupPalette() || palette->isLocked())
    return false;

  TStyleSelection *selection = dynamic_cast<TStyleSelection *>(
      app->getCurrentSelection()->getSelection());
  int selectedCount = 0;
  if (selection && !selection->isEmpty()) {
    if (!selection->getPaletteHandle() || selection->getPalette() != palette)
      return false;
    int pageIndex = selection->getPageIndex();
    if (pageIndex < 0 || pageIndex >= palette->getPageCount()) return false;
    TPalette::Page *page = palette->getPage(pageIndex);
    for (int index : selection->getIndicesInPage()) {
      if (index < 0 || index >= page->getStyleCount()) continue;
      ++selectedCount;
      int id = page->getStyleId(index);
      if (id > 0 && id < int(styles.size())) styles.set(id);
    }
  }
  // Count the user's selection BEFORE filtering animated/protected styles.
  // Selecting two animated styles must not expand to the entire palette.
  allStyles = selectedCount < 2;
  if (allStyles) styles = paletteScope(palette);
  return styles.any();
}

bool eligible(TPalette *palette, int id) {
  TColorStyle *style = palette->getStyle(id);
  return style && typeid(*style) == typeid(TSolidColorStyle) &&
         palette->getKeyframeCount(id) == 0 && style->getGlobalName().empty() &&
         style->getOriginalName().empty();
}

// Pump events only outside raster locks and at a bounded frequency. The modal
// progress window prevents editing while the command retains its original
// target.
class Progress {
  QProgressDialog m_dialog;
  QElapsedTimer m_timer;

public:
  Progress()
      : m_dialog(QObject::tr("Analyzing colors..."), QObject::tr("Cancel"), 0,
                 1000, TApp::instance()->getMainWindow()) {
    m_dialog.setWindowTitle(QObject::tr("Reduce Colors"));
    m_dialog.setWindowModality(Qt::ApplicationModal);
    m_dialog.setMinimumDuration(0);
    m_dialog.setAutoClose(false);
    m_dialog.setAutoReset(false);
    m_dialog.show();
    m_timer.start();
  }

  bool canceled(int value = -1) {
    if (m_timer.elapsed() >= 30 || value >= 0) {
      if (value >= 0) m_dialog.setValue(value);
      QApplication::processEvents();
      m_timer.restart();
    }
    return m_dialog.wasCanceled();
  }

  void label(const QString &text) { m_dialog.setLabelText(text); }
};

// RAII also unlocks the raster if tile allocation fails.
class RasterLock {
  TRasterCM32P m_raster;

public:
  explicit RasterLock(const TRasterCM32P &raster) : m_raster(raster) {
    m_raster->lock();
  }
  ~RasterLock() { m_raster->unlock(); }
};

bool mapRaster(const TRasterCM32P &raster, const StyleMap &styles,
               TTileSaverCM32 *saver = nullptr, Progress *progress = nullptr) {
  for (int start = 0; start < raster->getLy(); start += 32) {
    if (progress && progress->canceled()) return false;
    RasterLock lock(raster);
    for (int y = start; y < std::min(start + 32, raster->getLy()); ++y) {
      TPixelCM32 *row = raster->pixels(y);
      for (int x = 0; x < raster->getLx(); ++x) {
        TPixelCM32 pixel = row[x];
        if (!remap(pixel, styles)) continue;
        if (saver) saver->save(TPoint(x, y));
        row[x] = pixel;
      }
    }
  }
  return true;
}

TToonzImageP readFrame(TXshSimpleLevel *level, const TFrameId &fid,
                       bool modify) {
  if (!level->isFid(fid) || level->isFrameReadOnly(fid))
    throw std::runtime_error("Frame is no longer editable");
  TToonzImageP image = level->getFullsampledFrame(
      fid, modify ? ImageManager::toBeModified : ImageManager::none);
  if (!image || !image->getRaster() || image->getSubsampling() > 1)
    throw std::runtime_error("Full-resolution drawing is unavailable");
  return image;
}

std::vector<TXshSimpleLevelP> sharedPaletteLevels(TXshSimpleLevel *current) {
  // Include unexposed scene-cast levels, not just cells in the current Xsheet.
  TLevelSet *cast =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  std::vector<TXshSimpleLevelP> levels;
  for (int i = 0; i < cast->getLevelCount(); ++i) {
    TXshSimpleLevel *level = cast->getLevel(i)->getSimpleLevel();
    if (level && level != current &&
        level->getPalette() == current->getPalette())
      levels.push_back(TXshSimpleLevelP(level));
  }
  return levels;
}

bool collectSharedUsage(TXshSimpleLevel *current, Used &used,
                        Progress &progress) {
  const auto levels = sharedPaletteLevels(current);
  for (const auto &level : levels) {
    for (const TFrameId &fid : level->getFids()) {
      if (progress.canceled()) return false;
      TImageP image    = level->getFullsampledFrame(fid, ImageManager::none);
      TToonzImageP tlv = image;
      TVectorImageP vector = image;
      if (tlv && tlv->getRaster() && tlv->getSubsampling() <= 1) {
        TRasterCM32P raster = tlv->getRaster();
        for (int start = 0; start < raster->getLy(); start += 32) {
          if (progress.canceled()) return false;
          RasterLock lock(raster);
          for (int y = start; y < std::min(start + 32, raster->getLy()); ++y) {
            const TPixelCM32 *row = raster->pixels(y);
            for (int x = 0; x < raster->getLx(); ++x)
              used[row[x].getInk()] = used[row[x].getPaint()] = true;
          }
        }
      } else if (vector) {
        std::set<int> ids;
        vector->getUsedStyles(ids);
        for (int id : ids)
          if (id >= 0 && id < int(used.size())) used[id] = true;
      } else {
        // An unreadable/unsupported shared level is not evidence of disuse.
        throw std::runtime_error("Cannot verify shared palette usage");
      }
    }
  }
  return true;
}

class FrameUndo final : public ToolUtils::TRasterUndo {
  std::shared_ptr<const StyleMap> m_styles;

public:
  FrameUndo(TTileSetCM32 *tiles, TXshSimpleLevel *level, const TFrameId &fid,
            const std::shared_ptr<const StyleMap> &styles)
      : TRasterUndo(tiles, level, fid, false, false, nullptr, false)
      , m_styles(styles) {}

  void notify() const {
    m_level->touchFrame(m_frameId);
    notifyImageChanged();
  }
  void undo() const override {
    if (!getImage()) return;
    TRasterUndo::undo();
    notify();
  }
  void redo() const override {
    TToonzImageP image = getImage();
    if (!image || !image->getRaster()) return;
    mapRaster(image->getRaster(), *m_styles);
    notify();
  }
};

class ReduceColorsUndo final : public TUndo {
  TPaletteP m_palette;

public:
  struct RemovedStyle {
    int page, index, id;
    std::unique_ptr<TColorStyle> style;
  };
  std::vector<std::unique_ptr<FrameUndo>> frames;
  std::vector<RemovedStyle> removedStyles;
  std::vector<int> renumber, originalNumbers;

  explicit ReduceColorsUndo(const TPaletteP &palette) : m_palette(palette) {}

  void removeStyles() const {
    // Original page indices stay valid when removed in reverse order.
    for (auto it = removedStyles.rbegin(); it != removedStyles.rend(); ++it)
      m_palette->getPage(it->page)->removeStyle(it->index);
  }

  void reorder(bool forward) const {
    if (!renumber.empty() &&
        !m_palette->reorderStyles(forward ? renumber : originalNumbers))
      throw std::runtime_error("Palette indices changed during reduction");
  }

  void notify(const std::vector<int> *indexMap = nullptr) const {
    if (!removedStyles.empty() || !renumber.empty()) {
      m_palette->setDirtyFlag(true);
      TPaletteHandle *handle = TApp::instance()->getCurrentPalette();
      if (handle->getPalette() == m_palette.getPointer()) {
        auto selection = dynamic_cast<TStyleSelection *>(
            TApp::instance()->getCurrentSelection()->getSelection());
        if (selection && selection->getPaletteHandle() &&
            selection->getPalette() == m_palette.getPointer())
          selection->selectNone();
        int id = handle->getStyleIndex();
        if (indexMap && id >= 0 && id < int(indexMap->size()))
          id = (*indexMap)[id];
        if (id < 0 || id >= m_palette->getStyleCount() ||
            !m_palette->getStylePage(id))
          id = 1;
        handle->setStyleIndex(id);
        handle->notifyPaletteChanged();
        handle->notifyColorStyleChanged(false, false);
      }
    }
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void undo() const override {
    reorder(false);
    for (const RemovedStyle &removed : removedStyles) {
      // Unpaged IDs may be reused by later style creation. Restore the saved
      // definition, including its name, before putting the chip back.
      m_palette->setStyle(removed.id, removed.style->clone());
      m_palette->getPage(removed.page)->insertStyle(removed.index, removed.id);
    }
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) (*it)->undo();
    notify(&originalNumbers);
  }
  void redo() const override {
    reorder(true);
    for (const auto &frame : frames) frame->redo();
    removeStyles();
    notify(&renumber);
  }
  int getSize() const override {
    size_t size = sizeof(*this) + sizeof(StyleMap);
    size += removedStyles.size() *
            (sizeof(RemovedStyle) + sizeof(TSolidColorStyle));
    size += (renumber.size() + originalNumbers.size()) * sizeof(int);
    for (const auto &frame : frames) size += frame->getSize();
    return int(std::min(size, size_t(std::numeric_limits<int>::max())));
  }
  QString getHistoryString() override { return QObject::tr("Reduce Colors"); }
};

struct PreparedFrame {
  TXshSimpleLevelP level;
  TFrameId fid;
  TToonzImageP image;
  TRasterCM32P raster;
  TRect savebox;
};

struct RenumberLevel {
  TXshSimpleLevelP level;
  std::vector<TFrameId> fids;
};

void executeReduction() {
  TXshSimpleLevel *level = nullptr;
  StyleMask scope;
  bool allStyles = false;
  if (!getTarget(level, scope, allStyles)) return;
  const StyleMask originalScope = scope;
  TXshSimpleLevelP keepLevel(level);
  TPaletteP palette(level->getPalette());
  const auto originalPages = palettePages(palette.getPointer());
  ToonzScene *scene        = TApp::instance()->getCurrentScene()->getScene();
  std::vector<TFrameId> fids;
  level->getFids(fids);
  if (fids.empty()) return;
  for (const TFrameId &fid : fids)
    if (level->isFrameReadOnly(fid)) {
      DVGui::warning(
          QObject::tr("This level contains read-only drawings. "
                      "Reduce Colors requires every drawing to be editable."));
      return;
    }

  QDialog dialog(TApp::instance()->getMainWindow());
  dialog.setWindowTitle(QObject::tr("Reduce Colors"));
  QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
  dialogLayout->setContentsMargins(12, 12, 12, 12);
  dialogLayout->setSpacing(12);
  QScrollArea *options = new QScrollArea(&dialog);
  options->setWidgetResizable(true);
  options->setFrameShape(QFrame::NoFrame);
  QWidget *contents   = new QWidget(options);
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);
  options->setWidget(contents);
  dialogLayout->addWidget(options);
  // Keep the action buttons and large-operation warning reachable on smaller
  // displays while allowing the explanatory text to wrap and scroll.
  dialog.resize(
      600,
      std::min(
          600,
          QApplication::primaryScreen()->availableGeometry().height() - 80));
  const auto addText = [](QVBoxLayout *box, const QString &text) {
    QLabel *label = new QLabel(text);
    label->setWordWrap(true);
    box->addWidget(label);
    return label;
  };
  const auto addGroup = [&](const QString &title) {
    QGroupBox *group = new QGroupBox(title, contents);
    QVBoxLayout *box = new QVBoxLayout(group);
    box->setContentsMargins(12, 16, 12, 12);
    box->setSpacing(8);
    layout->addWidget(group);
    return box;
  };

  QVBoxLayout *scopeLayout = addGroup(QObject::tr("Scope"));
  QComboBox *scopeChoice   = new QComboBox;
  scopeChoice->setAccessibleName(QObject::tr("Styles to process"));
  if (!allStyles) scopeChoice->addItem(QObject::tr("Selected styles"));
  scopeChoice->addItem(QObject::tr("All palette styles"));
  if (!allStyles && !int(ReduceColorsSelectedScope))
    scopeChoice->setCurrentIndex(1);
  scopeLayout->addWidget(scopeChoice);
  QLabel *scopeInfo = addText(scopeLayout, QString());

  QVBoxLayout *methodLayout = addGroup(QObject::tr("Color reduction"));
  QGridLayout *methodForm   = new QGridLayout;
  methodForm->setContentsMargins(0, 0, 0, 0);
  methodForm->setHorizontalSpacing(12);
  methodForm->setVerticalSpacing(8);
  methodForm->setColumnStretch(1, 1);
  methodLayout->addLayout(methodForm);
  QComboBox *method = new QComboBox;
  method->addItem(QObject::tr("Merge identical colors"));
  method->addItem(QObject::tr("Reduce to a color count"));
  method->addItem(QObject::tr("Merge similar colors (80/20)"));
  const int savedMode = int(ReduceColorsMode);
  method->setCurrentIndex(savedMode >= 0 && savedMode <= 2 ? savedMode : 0);
  method->setAccessibleName(QObject::tr("Reduction method"));
  QLabel *methodLabel = new QLabel(QObject::tr("Method:"));
  methodLabel->setBuddy(method);
  methodForm->addWidget(methodLabel, 0, 0);
  methodForm->addWidget(method, 0, 1);

  int preferredTarget = std::max(1, std::min(4095, int(ReduceColorsTarget)));
  QSpinBox *target    = new QSpinBox;
  target->setMaximumWidth(120);
  target->setAccessibleName(QObject::tr("Target color count"));
  target->setToolTip(QObject::tr(
      "The target is limited to eligible styles in this scope. A smaller "
      "palette temporarily limits the value without changing your saved "
      "preference. Increasing it cannot restore merged colors; use Undo."));
  QLabel *targetLabel = new QLabel(QObject::tr("Maximum colors:"));
  targetLabel->setBuddy(target);
  methodForm->addWidget(targetLabel, 1, 0);
  methodForm->addWidget(target, 1, 1, Qt::AlignLeft);

  QCheckBox *automaticTolerance =
      new QCheckBox(QObject::tr("Calculate tolerance automatically"));
  automaticTolerance->setChecked(int(ReduceColorsAutomaticTolerance) != 0);
  methodForm->addWidget(automaticTolerance, 2, 0, 1, 2);
  QDoubleSpinBox *tolerance = new QDoubleSpinBox;
  tolerance->setRange(0, 200);
  tolerance->setDecimals(3);
  tolerance->setSingleStep(0.5);
  tolerance->setMaximumWidth(120);
  const double savedTolerance = double(ReduceColorsTolerance);
  tolerance->setValue(std::isfinite(savedTolerance)
                          ? std::max(0.0, std::min(200.0, savedTolerance))
                          : 3.0);
  tolerance->setAccessibleName(QObject::tr("Color similarity tolerance"));
  tolerance->setToolTip(QObject::tr(
      "Maximum color distance to the surviving style, measured as "
      "100 times the Oklab distance. Lower values keep more colors. "
      "Zero merges only identical colors."));
  QLabel *toleranceLabel = new QLabel(QObject::tr("Tolerance:"));
  toleranceLabel->setBuddy(tolerance);
  methodForm->addWidget(toleranceLabel, 3, 0);
  methodForm->addWidget(tolerance, 3, 1, Qt::AlignLeft);
  QLabel *methodInfo      = addText(methodLayout, QString());
  const auto updateMethod = [&]() {
    const bool useTarget     = method->currentIndex() == 1;
    const bool useSimilarity = method->currentIndex() == 2;
    targetLabel->setVisible(useTarget);
    target->setVisible(useTarget);
    automaticTolerance->setVisible(useSimilarity);
    const bool manual = useSimilarity && !automaticTolerance->isChecked();
    toleranceLabel->setVisible(manual);
    tolerance->setVisible(manual);
    if (useSimilarity)
      methodInfo->setText(QObject::tr(
          "Favors fewer styles over color accuracy. Protects about 20% of "
          "the distinct used colors (rounded up), chosen by pixel coverage "
          "and color separation. Automatic tolerance merges the rest; a "
          "lower manual tolerance keeps more colors."));
    else if (useTarget)
      methodInfo->setText(QObject::tr(
          "Merges identical colors first, then keeps at most the requested "
          "number of colors used in this scope."));
    else
      methodInfo->setText(QObject::tr(
          "Combines styles with identical colors and opacity, preserving "
          "the appearance of every drawing."));
  };
  QObject::connect(method, QOverload<int>::of(&QComboBox::currentIndexChanged),
                   &dialog, updateMethod);
  QObject::connect(automaticTolerance, &QCheckBox::toggled, &dialog,
                   updateMethod);
  QObject::connect(target, QOverload<int>::of(&QSpinBox::valueChanged), &dialog,
                   [&](int value) { preferredTarget = value; });
  updateMethod();

  QVBoxLayout *finishLayout = addGroup(QObject::tr("After reduction"));
  QCheckBox *cleanup =
      new QCheckBox(QObject::tr("Remove unused styles in this scope"));
  cleanup->setChecked(int(ReduceColorsCleanup) != 0);
  finishLayout->addWidget(cleanup);
  addText(
      finishLayout,
      QObject::tr("Keeps reserved styles and colors still used by other levels "
                  "sharing this palette."));
  finishLayout->addSpacing(4);
  QCheckBox *renumber =
      new QCheckBox(QObject::tr("Renumber remaining styles consecutively"));
  renumber->setChecked(int(ReduceColorsRenumber) != 0);
  finishLayout->addWidget(renumber);
  addText(
      finishLayout,
      QObject::tr(
          "Renumbers the entire palette in page order, keeping indices 0 and 1 "
          "fixed. Pixel assignments and animation are preserved. Shared levels "
          "must be editable Toonz Raster levels."));
  layout->addStretch();
  const TDimension resolution = level->getResolution();
  const double pixelCount = double(resolution.lx) * resolution.ly * fids.size();
  const auto sharedLevels = sharedPaletteLevels(level);
  size_t sharedDrawings   = 0;
  double sharedPixels     = 0;
  for (const auto &shared : sharedLevels) {
    const size_t drawings = shared->getFids().size();
    const TDimension size = shared->getResolution();
    sharedDrawings += drawings;
    sharedPixels += double(size.lx) * size.ly * drawings;
  }
  QLabel *largeOperation = new QLabel(
      QObject::tr("This is a large operation and may take considerable time "
                  "and memory. You can cancel during analysis or preparation; "
                  "no drawings are changed until preparation finishes."),
      &dialog);
  largeOperation->setWordWrap(true);
  dialogLayout->addWidget(largeOperation);
  QDialogButtonBox *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Reduce Colors"));
  buttons->button(QDialogButtonBox::Ok)->setDefault(true);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  dialogLayout->addWidget(buttons);

  std::vector<Color> colors;
  const auto updateScope = [&]() {
    scope = (allStyles || scopeChoice->currentIndex() == 1)
                ? paletteScope(palette.getPointer())
                : originalScope;
    colors.clear();
    int skipped = 0;
    for (int id = 1; id < palette->getStyleCount() && id < int(scope.size());
         ++id) {
      if (!scope[id]) continue;
      if (eligible(palette.getPointer(), id))
        colors.push_back({id, palette->getStyle(id)->getMainColor()});
      else
        ++skipped;
    }
    QString summary = QObject::tr("Eligible styles: %1    Drawings: %2")
                          .arg(int(colors.size()))
                          .arg(int(fids.size()));
    if (skipped > 0)
      summary +=
          QObject::tr("\nSkipped styles: %1 (animated, linked or non-solid).")
              .arg(skipped);
    summary += QObject::tr(
        "\nOpacity is preserved and may require additional colors.");
    scopeInfo->setText(summary);
    // Do not treat a context-dependent cap as a newly chosen target. Switching
    // back to a larger scope restores the user's preferred value.
    const QSignalBlocker blockTarget(target);
    target->setRange(1, std::max(1, int(colors.size())));
    target->setValue(preferredTarget);
    buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(!colors.empty() || renumber->isChecked());
    const bool checksShared = cleanup->isChecked() || renumber->isChecked();
    largeOperation->setVisible(
        colors.size() >= 256 ||
        fids.size() + (checksShared ? sharedDrawings : 0) >= 100 ||
        pixelCount + (checksShared ? sharedPixels : 0) >= 50000000.0);
  };
  QObject::connect(scopeChoice,
                   QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
                   updateScope);
  QObject::connect(renumber, &QCheckBox::toggled, &dialog, updateScope);
  QObject::connect(cleanup, &QCheckBox::toggled, &dialog, updateScope);
  updateScope();
  if (dialog.exec() != QDialog::Accepted) return;
  // Canceling the dialog leaves all remembered options unchanged. A forced
  // all-styles scope (fewer than two selected chips) must not overwrite the
  // user's preference for occasions when a real multi-selection exists.
  ReduceColorsMode               = method->currentIndex();
  ReduceColorsTarget             = preferredTarget;
  ReduceColorsAutomaticTolerance = automaticTolerance->isChecked() ? 1 : 0;
  ReduceColorsTolerance          = tolerance->value();
  ReduceColorsCleanup            = cleanup->isChecked() ? 1 : 0;
  ReduceColorsRenumber           = renumber->isChecked() ? 1 : 0;
  if (!allStyles)
    ReduceColorsSelectedScope = scopeChoice->currentIndex() == 0 ? 1 : 0;
  const int requested      = method->currentIndex() == 1 ? target->value() : 0;
  const bool useSimilarity = method->currentIndex() == 2;
  const double requestedTolerance =
      automaticTolerance->isChecked() ? -1 : tolerance->value() / 100.0;
  std::vector<RenumberLevel> renumberLevels;
  if (renumber->isChecked()) {
    for (const auto &shared : sharedLevels) {
      if (shared->getType() != TZP_XSHLEVEL || shared->isReadOnly() ||
          shared->isSubsequence()) {
        DVGui::warning(QObject::tr(
            "Renumbering requires every level sharing this palette to be "
            "an editable Toonz Raster level. No drawings have been changed."));
        return;
      }
      renumberLevels.push_back({shared, shared->getFids()});
      for (const auto &fid : renumberLevels.back().fids)
        if (shared->isFrameReadOnly(fid)) {
          DVGui::warning(QObject::tr(
              "A level sharing this palette contains read-only drawings. "
              "Renumbering cannot proceed. No drawings have been changed."));
          return;
        }
    }
  }

  auto undo = std::make_unique<ReduceColorsUndo>(palette);
  Plan plan;
  {
    Progress progress;
    Usage usage{};
    Used used{};
    for (size_t f = 0; f < fids.size(); ++f) {
      if (progress.canceled(int(400 * f / fids.size()))) return;
      TToonzImageP image  = readFrame(level, fids[f], false);
      TRasterCM32P raster = image->getRaster();
      for (int start = 0; start < raster->getLy(); start += 32) {
        if (progress.canceled()) return;
        RasterLock lock(raster);
        for (int y = start; y < std::min(start + 32, raster->getLy()); ++y) {
          const TPixelCM32 *row = raster->pixels(y);
          for (int x = 0; x < raster->getLx(); ++x) count(row[x], usage, used);
        }
      }
    }
    progress.label(QObject::tr("Choosing surviving colors..."));
    const auto cancel = [&]() { return progress.canceled(); };
    plan              = useSimilarity ? makeSimilarityPlan(colors, usage, used,
                                                           requestedTolerance, cancel)
                                      : makePlan(colors, usage, used, requested, cancel);
    if (plan.canceled || progress.canceled()) return;
    if (requested > 0 && requested < plan.minimum) {
      DVGui::warning(
          QObject::tr("At least %1 colors are needed to preserve the "
                      "opacity values used in this scope. Choose a target "
                      "of %1 or more. No drawings have been changed.")
              .arg(plan.minimum));
      return;
    }
    if (cleanup->isChecked()) {
      progress.label(
          QObject::tr("Checking unused styles and shared palettes..."));
      Used remaining = remappedUsage(used, plan.styles);
      if (!collectSharedUsage(level, remaining, progress)) return;
      for (int p = 0; p < palette->getPageCount(); ++p) {
        TPalette::Page *page = palette->getPage(p);
        for (int i = 0; i < page->getStyleCount(); ++i) {
          const int id = page->getStyleId(i);
          // Match Delete Unused Styles' protection for the first two chips.
          // Keep ID 1 protected even if the user has moved it to another page.
          if ((p == 0 && i < 2) || id <= 1 || id >= int(scope.size()) ||
              !scope[id] || remaining[id] ||
              !eligible(palette.getPointer(), id))
            continue;
          undo->removedStyles.push_back(
              {p, i, id,
               std::unique_ptr<TColorStyle>(palette->getStyle(id)->clone())});
        }
      }
    }

    StyleMap numbers;
    std::iota(numbers.begin(), numbers.end(), 0);
    if (renumber->isChecked()) {
      Used removed{};
      for (const auto &style : undo->removedStyles) removed[style.id] = true;
      const StyleMap identity = numbers;
      numbers =
          makeRenumberMap(originalPages, palette->getStyleCount(), removed);
      if (numbers != identity) {
        const int count = palette->getStyleCount();
        undo->renumber.assign(numbers.begin(), numbers.begin() + count);
        undo->originalNumbers.resize(count);
        for (int id = 0; id < count; ++id)
          undo->originalNumbers[numbers[id]] = id;
        for (int &id : plan.styles) id = numbers[id];
      }
    }

    progress.label(QObject::tr("Preparing drawings and undo data..."));
    const auto mapping       = std::make_shared<const StyleMap>(plan.styles);
    const auto numberMapping = std::make_shared<const StyleMap>(numbers);
    std::vector<PreparedFrame> prepared;
    size_t totalFrames = fids.size(), preparedCount = 0;
    for (const auto &shared : renumberLevels) totalFrames += shared.fids.size();
    const auto prepare = [&](TXshSimpleLevel *targetLevel,
                             const std::vector<TFrameId> &targetFids,
                             const std::shared_ptr<const StyleMap> &map) {
      for (const TFrameId &fid : targetFids) {
        if (progress.canceled(400 + int(600 * preparedCount++ / totalFrames)))
          return false;
        TToonzImageP image  = readFrame(targetLevel, fid, false);
        TRasterCM32P raster = image->getRaster()->clone();
        auto tiles          = std::make_unique<TTileSetCM32>(raster->getSize());
        TTileSaverCM32 saver(raster, tiles.get());
        if (!mapRaster(raster, *map, &saver, &progress)) return false;
        if (tiles->getTileCount() == 0) continue;
        // Prepare copies and original tiles for both the reduced level and
        // shared levels that only need a lossless index permutation.
        auto frameUndo =
            std::make_unique<FrameUndo>(tiles.get(), targetLevel, fid, map);
        tiles.release();
        undo->frames.push_back(std::move(frameUndo));
        prepared.push_back({TXshSimpleLevelP(targetLevel), fid, image, raster,
                            image->getSavebox()});
      }
      return true;
    };
    if ((plan.before != plan.after || !undo->renumber.empty()) &&
        !prepare(level, fids, mapping))
      return;
    if (!undo->renumber.empty())
      for (const auto &shared : renumberLevels)
        if (!prepare(shared.level.getPointer(), shared.fids, numberMapping))
          return;
    if (progress.canceled(999)) return;

    TXshSimpleLevel *currentLevel = nullptr;
    StyleMask currentScope;
    bool currentAll = false;
    if (!getTarget(currentLevel, currentScope, currentAll) ||
        currentLevel != level || currentScope != originalScope ||
        level->getPalette() != palette.getPointer() ||
        level->getFids() != fids ||
        palettePages(palette.getPointer()) != originalPages ||
        TApp::instance()->getCurrentScene()->getScene() != scene)
      throw std::runtime_error("Reduction target changed");
    for (const Color &color : colors)
      if (color.id >= palette->getStyleCount() ||
          !eligible(palette.getPointer(), color.id) ||
          palette->getStyle(color.id)->getMainColor() != color.rgba)
        throw std::runtime_error("Palette changed during reduction");
    if (!undo->renumber.empty()) {
      if (sharedPaletteLevels(level) != sharedLevels)
        throw std::runtime_error("Shared palette levels changed");
      for (const auto &shared : renumberLevels)
        if (shared.level->getPalette() != palette.getPointer() ||
            shared.level->getFids() != shared.fids ||
            shared.level->isReadOnly() || shared.level->isSubsequence())
          throw std::runtime_error("Shared palette level changed");
    }

    // Mark cached images as editable before the commit. Retained image pointers
    // keep them resident. The commit itself only swaps prepared raster
    // pointers.
    for (const PreparedFrame &frame : prepared)
      if (readFrame(frame.level.getPointer(), frame.fid, true) != frame.image)
        throw std::runtime_error("Drawing changed during reduction");
    // reorderStyles allocates before making any change. Once it succeeds,
    // swapping rasters and removing page entries cannot allocate or cancel.
    undo->reorder(true);
    for (PreparedFrame &frame : prepared) {
      frame.image->setCMapped(frame.raster);
      frame.image->setSavebox(frame.savebox);
    }
    undo->removeStyles();
  }
  if (undo->frames.empty() && undo->removedStyles.empty() &&
      undo->renumber.empty()) {
    DVGui::info(
        QObject::tr("No colors need to be combined, removed or renumbered."));
    return;
  }
  const int removedCount = int(undo->removedStyles.size());
  const bool renumbered  = !undo->renumber.empty();
  for (const auto &frame : undo->frames) frame->notify();
  undo->notify(&undo->renumber);
  TUndoManager::manager()->add(undo.release());
  QString result =
      QObject::tr(
          "Reduced %1 used styles to %2 colors in the chosen scope. "
          "Removed %3 unused styles.")
          .arg(plan.before)
          .arg(plan.after)
          .arg(removedCount);
  if (useSimilarity)
    result += QObject::tr("\nTolerance: %1. Protected colors: %2.")
                  .arg(plan.tolerance * 100.0, 0, 'f', 3)
                  .arg(int(plan.protectedStyles.size()));
  if (renumbered)
    result += QObject::tr(
        "\nRemaining palette styles were renumbered. "
        "Pixel assignments in shared levels were preserved.");
  DVGui::info(result);
}

class ReduceColorsCommand final : public MenuItemHandler {
public:
  ReduceColorsCommand() : MenuItemHandler(MI_ReduceColors) {}
  void execute() override {
    if (reducing) return;
    QScopedValueRollback<bool> guard(reducing, true);
    try {
      executeReduction();
    } catch (...) {
      DVGui::error(QObject::tr(
          "Reduce Colors could not be completed. Check that "
          "all drawings can be loaded and that enough memory is available."));
    }
  }
} reduceColorsCommand;

}  // namespace

void initReduceColorsCommand(QAction *action) {
  TApp *app = TApp::instance();
  action->setToolTip(
      QObject::tr("Reduce colors in every drawing of the current Toonz Raster "
                  "level. Select two or more styles to limit the operation. "
                  "Animated colors are never merged."));
  const auto update = []() {
    TXshSimpleLevel *level = nullptr;
    StyleMask scope;
    bool allStyles = false;
    CommandManager::instance()->enable(MI_ReduceColors,
                                       getTarget(level, scope, allStyles));
  };
  const auto schedule = [action, update]() {
    QTimer::singleShot(0, action, update);
  };
  QObject::connect(action, &QAction::triggered, action, schedule);
  QObject::connect(app->getCurrentSelection(),
                   &TSelectionHandle::selectionSwitched, action, schedule);
  QObject::connect(app->getCurrentSelection(),
                   &TSelectionHandle::selectionChanged, action, schedule);
  QObject::connect(app->getCurrentLevel(), &TXshLevelHandle::xshLevelSwitched,
                   action, schedule);
  QObject::connect(app->getCurrentLevel(), &TXshLevelHandle::xshLevelChanged,
                   action, schedule);
  QObject::connect(app->getCurrentFrame(), &TFrameHandle::frameSwitched, action,
                   schedule);
  QObject::connect(app->getCurrentFrame(), &TFrameHandle::frameTypeChanged,
                   action, schedule);
  QObject::connect(app->getCurrentFrame(),
                   &TFrameHandle::isPlayingStatusChanged, action, schedule);
  QObject::connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
                   action, schedule);
  QObject::connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
                   action, schedule);
  QObject::connect(app->getCurrentPalette(), &TPaletteHandle::paletteSwitched,
                   action, schedule);
  QObject::connect(app->getCurrentPalette(), &TPaletteHandle::paletteChanged,
                   action, schedule);
  QObject::connect(app->getCurrentPalette(),
                   &TPaletteHandle::paletteLockChanged, action, schedule);
  QObject::connect(app->getCurrentPalette(), &TPaletteHandle::colorStyleChanged,
                   action, schedule);
  QObject::connect(app->getCurrentColumn(), &TColumnHandle::columnIndexSwitched,
                   action, schedule);
  update();
}
