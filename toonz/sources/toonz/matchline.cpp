

#include "matchline.h"

// Tnz6 includes
#include "tapp.h"

// TnzTools includes
#include "tools/toolutils.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/scenefx.h"
#include "toonz/dpiscale.h"
#include "toonz/txsheethandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tpalette.h"
#include "timagecache.h"
#include "tcolorstyles.h"
#include "tundo.h"
#include "tropcm.h"
#include "ttoonzimage.h"
#include "tenv.h"

// Qt includes
#include <QRadioButton>
#include <QPushButton>
#include <QApplication>
#include <QMainWindow>
#include <QProgressBar>
#include <QGroupBox>
#include <QLabel>
#include <QButtonGroup>

// STD includes
#include <map>

using namespace DVGui;

TEnv::IntVar MatchlineStyleIndex("MatchlineStyleIndex", -1);
TEnv::IntVar MatchlinePrevalence("MatchlinePrevalence", 0);
TEnv::IntVar MatchlineInkUsagePolicy("MatchlineInkUsagePolicy", -1);

namespace {

class MergeCmappedPair {
public:
  const TXshCell *m_cell;
  TAffine m_imgAff;
  const TXshCell *m_mcell;
  TAffine m_matchAff;

  MergeCmappedPair(const TXshCell &cell, const TAffine &imgAff,
                   const TXshCell &mcell, const TAffine &matchAff)
      : m_cell(&cell)
      , m_imgAff(imgAff)
      , m_mcell(&mcell)
      , m_matchAff(matchAff){};
};

void doMatchlines(const std::vector<MergeCmappedPair> &matchingLevels,
                  int inkIndex, int inkPrevalence) {
  if (matchingLevels.empty()) return;

  TPalette *palette = matchingLevels[0].m_cell->getImage(false)->getPalette();
  TPalette *matchPalette =
      matchingLevels[0].m_mcell->getImage(false)->getPalette();

  TPalette::Page *page;

  // upInkId -> downInkId
  std::map<int, int> usedInks;

  int i = 0;
  for (i = 0; i < (int)matchingLevels.size(); i++) {
    TToonzImageP img = (TToonzImageP)matchingLevels[i].m_cell->getImage(true);
    TToonzImageP match =
        (TToonzImageP)matchingLevels[i].m_mcell->getImage(false);
    if (!img || !match)
      throw TRopException("Can do matchlines only on cmapped raster images!");
    // img->lock();
    TRasterCM32P ras      = img->getRaster();    // img->getCMapped(false);
    TRasterCM32P matchRas = match->getRaster();  // match->getCMapped(true);
    if (!ras || !matchRas)
      throw TRopException("Can do matchlines only on cmapped images!");

    TAffine aff =
        matchingLevels[i].m_imgAff.inv() * matchingLevels[i].m_matchAff;
    int mlx = matchRas->getLx();
    int mly = matchRas->getLy();
    int rlx = ras->getLx();
    int rly = ras->getLy();

    TRectD in = convert(matchRas->getBounds()) - matchRas->getCenterD();

    TRectD out = aff * in;

    TPoint offs((rlx - mlx) / 2 + tround(out.getP00().x - in.getP00().x),
                (rly - mly) / 2 + tround(out.getP00().y - in.getP00().y));

    int lxout = tround(out.getLx()) + 1 + ((offs.x < 0) ? offs.x : 0);
    int lyout = tround(out.getLy()) + 1 + ((offs.y < 0) ? offs.y : 0);

    if (lxout <= 0 || lyout <= 0 || offs.x >= rlx || offs.y >= rly) {
      // tmsg_error("no intersections between matchline and level");
      continue;
    }

    aff = aff.place((double)(in.getLx() / 2.0), (double)(in.getLy() / 2.0),
                    (out.getLx()) / 2.0 + ((offs.x < 0) ? offs.x : 0),
                    (out.getLy()) / 2.0 + ((offs.y < 0) ? offs.y : 0));

    if (offs.x < 0) offs.x = 0;
    if (offs.y < 0) offs.y = 0;

    if (lxout + offs.x > rlx || lyout + offs.y > rly) {
      lxout = (lxout + offs.x > rlx) ? (rlx - offs.x) : lxout;
      lyout = (lyout + offs.y > rly) ? (rly - offs.y) : lyout;
    }

    if (!aff.isIdentity(1e-4)) {
      TRasterCM32P aux(lxout, lyout);
      TRop::resample(aux, matchRas, aff);
      matchRas = aux;
    }
    ras->lock();
    matchRas->lock();
    TRect raux = matchRas->getBounds() + offs;
    TRasterP r = ras->extract(raux);

    TRop::applyMatchLines(r, matchRas, palette, matchPalette, inkIndex,
                          inkPrevalence, usedInks);

    ras->unlock();
    matchRas->unlock();

    img->setSavebox(img->getSavebox() + (matchRas->getBounds() + offs));
  }

  if (inkIndex == -2) {
    std::map<int, int>::iterator it = usedInks.begin();
    while (it != usedInks.end()) {
      if (it->second < palette->getStyleCount())
        usedInks.erase(it++);
      else
        ++it;
    }
  }

  /*- UseMatchlinedInkの場合はここでreturnする -*/
  if (usedInks.empty()) return;

  std::wstring pageName = L"match lines";

  for (i = 0; i < palette->getPageCount(); i++)
    if (palette->getPage(i)->getName() == pageName) {
      page = palette->getPage(i);
      break;
    }
  if (i == palette->getPageCount()) page = palette->addPage(pageName);

  std::map<int, int>::iterator it = usedInks.begin(), it_e = usedInks.end();
  int count = 0;
  for (; it != it_e; ++it) {
    while (palette->getStyleCount() <= it->second)
      palette->addStyle(TPixel32::Red);
    palette->setStyle(it->second, matchPalette->getStyle(it->first)->clone());
    page->addStyle(it->second);
  }
  if (usedInks.size() > 0) {
    palette->setDirtyFlag(true);
    TApp::instance()
        ->getPaletteController()
        ->getCurrentPalette()
        ->notifyPaletteChanged();
  }
}

/*------------------------------------------------------------------------*/

void applyDeleteMatchline(TXshSimpleLevel *sl,
                          const std::vector<TFrameId> &fids,
                          const std::vector<int> &_inkIndexes) {
  TPalette::Page *page = 0;
  int i, j, pageIndex = 0;
  std::vector<int> inkIndexes = _inkIndexes;

  if (fids.empty()) return;

  TPalette *palette = 0;

  if (inkIndexes.empty()) {
    palette = sl->getFrame(fids[0], true)->getPalette();

    for (i = 0; i < palette->getPageCount(); i++)
      if (palette->getPage(i)->getName() == L"match lines") {
        page = palette->getPage(i);
        break;
      }

    if (!page) return;
    pageIndex = i;
  }

  for (i = 0; i < (int)fids.size(); i++) {
    TToonzImageP image = sl->getFrame(fids[i], true);
    assert(image);
    TRasterCM32P ras = image->getRaster();  // level[i]->getCMapped(false);
    ras->lock();
    if (inkIndexes.empty())
      for (j = 0; j < page->getStyleCount(); j++)
        inkIndexes.push_back(page->getStyleId(j));

    TRop::eraseColors(ras, &inkIndexes, true);

    ras->unlock();
    TRect savebox;
    TRop::computeBBox(ras, savebox);
    image->setSavebox(savebox);
  }
}

/*------------------------------------------------------------------------*/
}  // namespace
//-----------------------------------------------------------------------------

void MatchlinesDialog::accept() {
  QSettings settings;
  bool ok;
  int value = m_inkIndex->text().toInt(&ok);

  MatchlineStyleIndex = (ok) ? value : -1;
  MatchlinePrevalence = getInkPrevalence();
  MatchlineInkUsagePolicy =
      m_button1->isChecked() ? 0 : (m_button2->isChecked() ? 1 : 2);

  QDialog::accept();
}

//----------------------------------------------------------------------------------

int MatchlinesDialog::exec(TPalette *plt) {
  m_pltHandle->setPalette(plt);

  int styleIndex                                    = MatchlineStyleIndex;
  if (styleIndex > plt->getStyleCount()) styleIndex = 1;
  if (styleIndex != -1) m_inkIndex->setText(QString::number(styleIndex));

  int prevalence = MatchlinePrevalence;
  m_inkPrevalence->setValue(prevalence);

  int inkUsagePolicy = MatchlineInkUsagePolicy;
  switch (inkUsagePolicy) {
  case 0:
    m_button1->setChecked(true);
    break;
  case -1:
  case 1:
    m_button2->setChecked(true);
    break;
  case 2:
    m_button3->setChecked(true);
    break;
  }
  m_inkIndex->setEnabled(inkUsagePolicy == 1);

  return QDialog::exec();
}

//-----------------------------------------------------------------------------

void MatchlinesDialog::onChooseInkClicked(bool value) {
  m_inkIndex->setEnabled(value);
}

//-----------------------------------------------------------------------------

int MatchlinesDialog::getInkPrevalence() { return m_inkPrevalence->getValue(); }

//-----------------------------------------------------------------------------

int MatchlinesDialog::getInkIndex() {
  if (m_button1->isChecked())
    return -1;
  else if (m_button3->isChecked())
    return -2;
  else {
    if (QString("current").contains(m_inkIndex->text()))
      return TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->getStyleIndex();
    else {
      bool ok;
      int value = m_inkIndex->text().toInt(&ok);
      if (ok) return value;
    }
    return 0;
  }
}

//-----------------------------------------------------------------------------

MatchlinesDialog::MatchlinesDialog()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "Matchlines")
    , m_pltHandle(new TPaletteHandle())
    , m_currentXsheet(0) {
  setWindowTitle(tr("Apply Match Lines"));

  m_button1       = new QRadioButton(tr("Add Match Line Inks"), this);
  m_button2       = new QRadioButton(tr("Use Ink: "), this);
  m_button3       = new QRadioButton(tr("Merge Inks"), this);
  m_inkIndex      = new StyleIndexLineEdit();
  m_inkPrevalence = new IntField(this);

  QPushButton *okBtn     = new QPushButton(tr("Apply"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);

  QGroupBox *inkUsageGroupBox  = new QGroupBox(tr("Ink Usage"), this);
  QGroupBox *lineOrderGroupBox = new QGroupBox(tr("Line Stacking Order"), this);

  m_lup_noGapButton = new QPushButton(this);
  m_rup_noGapButton = new QPushButton(this);
  m_lup_gapButton   = new QPushButton(this);
  m_rup_gapButton   = new QPushButton(this);

  m_button1->setCheckable(true);
  m_button2->setCheckable(true);
  m_button3->setCheckable(true);
  m_button2->setChecked(true);

  m_button3->setToolTip(
      tr("Merge Inks : If the target level has the same style as the match "
         "line ink\n"
         "(i.e. with the same index and the same color), the existing style "
         "will be used.\n"
         "Otherwise, a new style will be added to \"match lines\" page."));

  m_inkIndex->setPaletteHandle(m_pltHandle);

  m_inkPrevalence->setRange(0, 100);

  okBtn->setDefault(true);

  QIcon lup_noGapIcon(
      QPixmap(":Resources/match_lup_nogap.png").scaled(104, 104));
  QIcon rup_noGapIcon(
      QPixmap(":Resources/match_rup_nogap.png").scaled(104, 104));
  QIcon lup_gapIcon(QPixmap(":Resources/match_lup_gap.png").scaled(104, 104));
  QIcon rup_gapIcon(QPixmap(":Resources/match_rup_gap.png").scaled(104, 104));

  m_lup_noGapButton->setObjectName("MatchLineButton");
  m_rup_noGapButton->setObjectName("MatchLineButton");
  m_lup_gapButton->setObjectName("MatchLineButton");
  m_rup_gapButton->setObjectName("MatchLineButton");

  m_lup_noGapButton->setCheckable(true);
  m_rup_noGapButton->setCheckable(true);
  m_lup_gapButton->setCheckable(true);
  m_rup_gapButton->setCheckable(true);

  m_lup_gapButton->setChecked(true);

  m_lup_noGapButton->setIcon(lup_noGapIcon);
  m_rup_noGapButton->setIcon(rup_noGapIcon);
  m_lup_gapButton->setIcon(lup_gapIcon);
  m_rup_gapButton->setIcon(rup_gapIcon);

  m_lup_noGapButton->setIconSize(QSize(104, 104));
  m_rup_noGapButton->setIconSize(QSize(104, 104));
  m_lup_gapButton->setIconSize(QSize(104, 104));
  m_rup_gapButton->setIconSize(QSize(104, 104));

  QButtonGroup *lineStackButtonGroup = new QButtonGroup(this);
  lineStackButtonGroup->addButton(m_lup_gapButton, 0);
  lineStackButtonGroup->addButton(m_rup_gapButton, 100);
  lineStackButtonGroup->addButton(m_lup_noGapButton, 30);
  lineStackButtonGroup->addButton(m_rup_noGapButton, 70);

  //----layout

  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(5);
  {
    QGridLayout *inkUsageLay = new QGridLayout();
    inkUsageLay->setMargin(5);
    inkUsageLay->setSpacing(5);
    {
      inkUsageLay->addWidget(m_button1, 0, 0, 1, 2);
      inkUsageLay->addWidget(m_button2, 1, 0);
      inkUsageLay->addWidget(m_inkIndex, 1, 1, Qt::AlignLeft);
      inkUsageLay->addWidget(m_button3, 2, 0, 1, 2);
    }
    inkUsageLay->setColumnStretch(0, 0);
    inkUsageLay->setColumnStretch(1, 1);
    inkUsageGroupBox->setLayout(inkUsageLay);
    m_topLayout->addWidget(inkUsageGroupBox);

    QVBoxLayout *lineOrderLay = new QVBoxLayout();
    lineOrderLay->setMargin(5);
    lineOrderLay->setSpacing(5);
    {
      QGridLayout *buttonsLay = new QGridLayout();
      buttonsLay->setMargin(0);
      buttonsLay->setSpacing(5);
      {
        buttonsLay->addWidget(new QLabel(tr("L-Up R-Down"), this), 0, 1);
        buttonsLay->addWidget(new QLabel(tr("L-Down R-Up"), this), 0, 2);

        buttonsLay->addWidget(new QLabel(tr("Keep\nHalftone"), this), 1, 0);
        buttonsLay->addWidget(m_lup_gapButton, 1, 1);
        buttonsLay->addWidget(m_rup_gapButton, 1, 2);

        buttonsLay->addWidget(new QLabel(tr("Fill\nGaps"), this), 2, 0);
        buttonsLay->addWidget(m_lup_noGapButton, 2, 1);
        buttonsLay->addWidget(m_rup_noGapButton, 2, 2);
      }
      buttonsLay->setColumnStretch(0, 0);
      buttonsLay->setColumnStretch(1, 1);
      buttonsLay->setColumnStretch(2, 1);
      buttonsLay->setRowStretch(0, 0);
      buttonsLay->setRowStretch(1, 1);
      buttonsLay->setRowStretch(2, 1);
      lineOrderLay->addLayout(buttonsLay, 1);

      QHBoxLayout *inkPrevalenceLay = new QHBoxLayout();
      inkPrevalenceLay->setMargin(0);
      inkPrevalenceLay->setSpacing(5);
      {
        inkPrevalenceLay->addWidget(new QLabel(tr("Line Prevalence"), this), 0);
        inkPrevalenceLay->addWidget(m_inkPrevalence, 1);
      }
      lineOrderLay->addLayout(inkPrevalenceLay, 0);
    }
    lineOrderGroupBox->setLayout(lineOrderLay);
    m_topLayout->addWidget(lineOrderGroupBox);

    m_topLayout->addStretch();
  }

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(10);
  {
    m_buttonLayout->addWidget(okBtn);
    m_buttonLayout->addWidget(cancelBtn);
  }

  //----signal-slot connections

  connect(m_button2, SIGNAL(toggled(bool)), this,
          SLOT(onChooseInkClicked(bool)));
  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  connect(lineStackButtonGroup, SIGNAL(buttonPressed(int)),
          SLOT(onLineStackButtonPressed(int)));
  connect(m_inkPrevalence, SIGNAL(valueChanged(bool)),
          SLOT(onInkPrevalenceChanged(bool)));
}

//-----------------------------------------------------------------------------

void MatchlinesDialog::showEvent(QShowEvent *e) {
  /*-- Xsheetが切り替わったらInkIndexを１に戻す --*/
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (xsh != m_currentXsheet) {
    m_inkIndex->setText("1");
    m_currentXsheet = xsh;
  }
}

//-----------------------------------------------------------------------------
/*-- ボタンを押したらPreveranceの値を変える。ボタンのIDが値に対応している --*/
void MatchlinesDialog::onLineStackButtonPressed(int id) {
  m_inkPrevalence->setValue(id);
}

//-----------------------------------------------------------------------------
/*-
 * スライダを動かしたらボタンを解除する。対応する値がある場合は対応するボタンを押される状態にする
 * -*/
void MatchlinesDialog::onInkPrevalenceChanged(bool isDragging) {
  if (isDragging) return;

  m_lup_noGapButton->setChecked(false);
  m_rup_noGapButton->setChecked(false);
  m_lup_gapButton->setChecked(false);
  m_rup_gapButton->setChecked(false);

  if (m_inkPrevalence->getValue() == 0)
    m_lup_gapButton->setChecked(true);
  else if (m_inkPrevalence->getValue() == 100)
    m_rup_gapButton->setChecked(true);
  else if (m_inkPrevalence->getValue() < 50)
    m_lup_noGapButton->setChecked(true);
  else
    m_rup_noGapButton->setChecked(true);
}

//-----------------------------------------------------------------------------

class DeleteMatchlineUndo final : public TUndo {
public:
  TXshLevel *m_xl;
  TXshSimpleLevel *m_sl;
  std::vector<TFrameId> m_fids;
  std::vector<int> m_indexes;
  TPaletteP m_matchlinePalette;

  DeleteMatchlineUndo(
      TXshLevel *xl, TXshSimpleLevel *sl, const std::vector<TFrameId> &fids,
      const std::vector<int> &indexes)  //, TPalette*matchPalette)
      : TUndo(),
        m_xl(xl),
        m_sl(sl),
        m_fids(fids),
        m_indexes(indexes) {
    int i;
    for (i = 0; i < fids.size(); i++) {
      QString id = "DeleteMatchlineUndo" + QString::number((uintptr_t)this) +
                   "-" + QString::number(i);
      TToonzImageP image = sl->getFrame(fids[i], false);
      assert(image);
      TImageCache::instance()->add(id, image->clone());
    }
  }

  void undo() const override {
    int i;

    for (i = 0; i < m_fids.size(); i++) {
      QString id = "DeleteMatchlineUndo" + QString::number((uintptr_t)this) +
                   "-" + QString::number(i);
      TImageP img = TImageCache::instance()->get(id, false)->cloneImage();

      m_sl->setFrame(m_fids[i], img);
      ToolUtils::updateSaveBox(m_sl, m_fids[i]);
    }

    if (m_xl) invalidateIcons(m_xl, m_fids);
    m_sl->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int i;

    applyDeleteMatchline(m_sl, m_fids, m_indexes);
    for (i = 0; i < m_fids.size(); i++) {
      ToolUtils::updateSaveBox(m_sl, m_fids[i]);
    }
    if (m_xl) invalidateIcons(m_xl, m_fids);
    m_sl->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  ~DeleteMatchlineUndo() {
    int i;
    for (i = 0; i < m_fids.size(); i++)
      TImageCache::instance()->remove("DeleteMatchlineUndo" +
                                      QString::number((uintptr_t)this) + "-" +
                                      QString::number(i));
  }

  QString getHistoryString() override {
    return QObject::tr("Delete Matchline  : Level %1")
        .arg(QString::fromStdWString(m_sl->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

//-----------------------------------------------------------------------------

class MatchlineUndo final : public TUndo {
  TXshLevelP m_xl;
  int m_mergeCmappedSessionId;
  std::map<TFrameId, QString> m_images;
  TXshSimpleLevel *m_level;
  TPalette *m_palette;
  int m_column, m_mColumn;
  int m_index, m_prevalence;

public:
  MatchlineUndo(TXshLevelP xl, int mergeCmappedSessionId, int index,
                int prevalence, int column, TXshSimpleLevel *level,
                const std::map<TFrameId, QString> &images, int mColumn,
                TPalette *palette)
      : TUndo()
      , m_xl(xl)
      , m_mergeCmappedSessionId(mergeCmappedSessionId)
      , m_palette(palette->clone())
      , m_level(level)
      , m_column(column)
      , m_mColumn(mColumn)
      , m_index(index)
      , m_prevalence(prevalence)
      , m_images(images) {}

  void undo() const override {
    std::map<TFrameId, QString>::const_iterator it = m_images.begin();

    m_level->getPalette()->assign(m_palette);

    std::vector<TFrameId> fids;
    for (; it != m_images.end(); ++it)  //, ++mit)
    {
      QString id = "MatchlinesUndo" + QString::number(m_mergeCmappedSessionId) +
                   "-" + QString::number(it->first.getNumber());
      TImageP img = TImageCache::instance()->get(id, false)->cloneImage();

      img->setPalette(m_level->getPalette());

      m_level->setFrame(it->first, img);
      fids.push_back(it->first);
    }

    if (m_xl) invalidateIcons(m_xl.getPointer(), fids);

    m_level->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_index == -1)
      TApp::instance()
          ->getPaletteController()
          ->getCurrentPalette()
          ->notifyPaletteChanged();
  }

  void redo() const override {
    doMatchlines(m_column, m_mColumn, m_index, m_prevalence,
                 m_mergeCmappedSessionId);
  }

  int getSize() const override { return sizeof(*this); }

  ~MatchlineUndo() {
    std::map<TFrameId, QString>::const_iterator it = m_images.begin();
    for (; it != m_images.end(); ++it)  //, ++mit)
    {
      QString id = "MatchlineUndo" + QString::number(m_mergeCmappedSessionId) +
                   "-" + QString::number(it->first.getNumber());
      TImageCache::instance()->remove(id);
    }
    delete m_palette;
  }

  QString getHistoryString() override {
    return QObject::tr("Apply Matchline  : Column%1 < Column%2")
        .arg(QString::number(m_column + 1))
        .arg(QString::number(m_mColumn + 1));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

//-----------------------------------------------------------------------------

static int LastMatchlineIndex = -1;

void doMatchlines(int column, int mColumn, int index, int inkPrevalence,
                  int MergeCmappedSessionId) {
  static int increment_MergeCmappedSessionId = 0;
  if (MergeCmappedSessionId == 0) {
    increment_MergeCmappedSessionId++;
    MergeCmappedSessionId = increment_MergeCmappedSessionId;
  }

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int start, end;
  xsh->getCellRange(column, start, end);

  if (start > end) return;
  std::vector<TXshCell> cell(end - start + 1);
  std::vector<TXshCell> mCell(end - start + 1);

  xsh->getCells(start, column, cell.size(), &(cell[0]));

  if (mColumn != -1) xsh->getCells(start, mColumn, cell.size(), &(mCell[0]));

  TXshColumn *col  = xsh->getColumn(column);
  TXshColumn *mcol = xsh->getColumn(mColumn);

  std::vector<MergeCmappedPair> matchingLevels;

  std::map<TFrameId, TFrameId> table;

  TXshSimpleLevel *level = 0, *mLevel = 0;
  TXshLevelP xl;

  std::map<TFrameId, QString> images;

  QProgressBar *getImageProgressBar = new QProgressBar(0);
  getImageProgressBar->setAttribute(Qt::WA_DeleteOnClose);
  getImageProgressBar->setWindowFlags(Qt::SubWindow | Qt::WindowStaysOnTopHint);

  getImageProgressBar->setMinimum(0);
  getImageProgressBar->setMaximum(cell.size() - 1);
  getImageProgressBar->setValue(0);
  getImageProgressBar->move(600, 500);
  getImageProgressBar->setWindowTitle("Storing matchline pairs");
  getImageProgressBar->show();

  /*- 左カラムの各セルについてループ -*/
  for (int i = 0; i < (int)cell.size(); i++) {
    getImageProgressBar->setValue(i);
    std::map<TFrameId, TFrameId>::iterator it;
    /*- 両セルに中身が無い場合は次のセルへ -*/
    if (cell[i].isEmpty() || mCell[i].isEmpty()) continue;
    if (!level) {
      level = cell[i].getSimpleLevel();
      xl    = cell[i].m_level;
    }
    /*-- 左カラムに複数のLevelが入っている場合、警告を出して抜ける --*/
    else if (level != cell[i].getSimpleLevel()) {
      getImageProgressBar->close();
      DVGui::warning(
          QObject::tr("It is not possible to apply match lines to a column "
                      "containing more than one level."));
      /*-- 前に遡ってキャッシュを消去 --*/
      i--;
      for (; i >= 0; i--) {
        TFrameId fid = cell[i].m_frameId;
        QString id = "MatchlinesUndo" + QString::number(MergeCmappedSessionId) +
                     "-" + QString::number(fid.getNumber());
        if (TImageCache::instance()->isCached(id.toStdString()))
          TImageCache::instance()->remove(id);
      }
      return;
    }

    if (!mLevel)
      mLevel = mCell[i].getSimpleLevel();
    else if (mLevel != mCell[i].getSimpleLevel()) {
      getImageProgressBar->close();
      DVGui::warning(
          QObject::tr("It is not possible to use a match lines column "
                      "containing more than one level."));
      /*-- 前に遡ってキャッシュを消去 --*/
      i--;
      for (; i >= 0; i--) {
        TFrameId fid = cell[i].m_frameId;
        QString id = "MatchlinedUndo" + QString::number(MergeCmappedSessionId) +
                     "-" + QString::number(fid.getNumber());
        if (TImageCache::instance()->isCached(id.toStdString()))
          TImageCache::instance()->remove(id);
      }
      return;
    }
    TImageP img   = cell[i].getImage(true);
    TImageP match = mCell[i].getImage(true);
    TFrameId fid  = cell[i].m_frameId;
    TFrameId mFid = mCell[i].m_frameId;

    //画像が取得できなかったら次へ
    if (!img || !match) continue;

    if ((it = table.find(fid)) == table.end()) {
      TToonzImageP timg   = (TToonzImageP)img;
      TToonzImageP tmatch = (TToonzImageP)match;

      //ラスタLevelじゃないとき、エラーを返す
      if (!timg || !tmatch) {
        getImageProgressBar->close();
        DVGui::warning(QObject::tr(
            "Match lines can be applied to Toonz raster levels only."));
        /*-- 前に遡ってキャッシュを消去 --*/
        i--;
        for (; i >= 0; i--) {
          TFrameId fid = cell[i].m_frameId;
          QString id   = "MatchlinedUndo" +
                       QString::number(MergeCmappedSessionId) + "-" +
                       QString::number(fid.getNumber());
          if (TImageCache::instance()->isCached(id.toStdString()))
            TImageCache::instance()->remove(id);
        }
        return;
      }
      /*- Matchline前の画像をUndoに格納 -*/
      QString id = "MatchlinesUndo" + QString::number(MergeCmappedSessionId) +
                   "-" + QString::number(fid.getNumber());
      TImageCache::instance()->add(id, timg->clone(), false);
      images[fid] = id;
      TAffine imgAff, matchAff;
      getColumnPlacement(imgAff, xsh, start + i, column, false);
      getColumnPlacement(matchAff, xsh, start + i, mColumn, false);
      TAffine dpiAff  = getDpiAffine(level, fid);
      TAffine mdpiAff = getDpiAffine(mLevel, mFid);
      matchingLevels.push_back(MergeCmappedPair(cell[i], imgAff * dpiAff,
                                                mCell[i], matchAff * mdpiAff));
      table[fid] = mFid;
    }
  }
  getImageProgressBar->close();

  if (matchingLevels.empty()) {
    DVGui::warning(
        QObject::tr("Match lines can be applied to Toonz raster levels only."));
    return;
  }

  if (inkPrevalence == -1)  // we are not in the redo
  {
    TPalette *plt = level->getPalette();
    if (!plt) {
      DVGui::warning(
          QObject::tr("The level you are using has not a valid palette."));
      return;
    }

    static MatchlinesDialog *md = new MatchlinesDialog();

    int styleCount = plt->getStyleCount();
    index          = styleCount;
    while (index >= styleCount) {
      if (md->exec(plt) != QDialog::Accepted) {
        /*- キャッシュを消す -*/
        for (int i = 0; i < (int)cell.size(); i++) {
          TFrameId fid  = cell[i].m_frameId;
          TFrameId mFid = mCell[i].m_frameId;
          QString id    = "MatchlinedUndo" +
                       QString::number(MergeCmappedSessionId) + "-" +
                       QString::number(fid.getNumber());
          QString mid = "MatchlinerUndo" +
                        QString::number(MergeCmappedSessionId) + "-" +
                        QString::number(mFid.getNumber());
          if (TImageCache::instance()->isCached(id.toStdString()))
            TImageCache::instance()->remove(id);
          if (TImageCache::instance()->isCached(mid.toStdString()))
            TImageCache::instance()->remove(mid);
        }
        return;
      }

      index = md->getInkIndex();
      if (index >= styleCount)
        DVGui::warning(
            QObject::tr("The style index you specified is not available in the "
                        "palette of the destination level."));
      if (index != -1) LastMatchlineIndex = index;
    }

    inkPrevalence = md ? md->getInkPrevalence() : 0;

    TUndoManager::manager()->add(
        new MatchlineUndo(xl, MergeCmappedSessionId, index, inkPrevalence,
                          column, level, images, mColumn, plt));
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);
  doMatchlines(matchingLevels, index, inkPrevalence);
  QApplication::restoreOverrideCursor();

  for (int i = 0; i < (int)cell.size(); i++)  // the saveboxes must be updated
  {
    if (cell[i].isEmpty() || mCell[i].isEmpty()) continue;

    if (!cell[i].getImage(false) || !mCell[i].getImage(false)) continue;

    TXshSimpleLevel *sl = cell[i].getSimpleLevel();
    const TFrameId &fid = cell[i].m_frameId;

    ToolUtils::updateSaveBox(sl, fid);
    IconGenerator::instance()->invalidate(sl, fid);
    sl->setDirtyFlag(true);
  }

  level->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

namespace {

const TXshCell *findCell(int column, const TFrameId &fid) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int i;
  for (i = 0; i < xsh->getColumn(column)->getMaxFrame(); i++)
    if (xsh->getCell(i, column).getFrameId() == fid)
      return &(xsh->getCell(i, column));
  return 0;
}

bool contains(const std::vector<TFrameId> &v, const TFrameId &val) {
  int i;
  for (i = 0; i < (int)v.size(); i++)
    if (v[i] == val) return true;
  return false;
}

//-----------------------------------------------------------------------------

QString indexes2string(const std::set<TFrameId> fids) {
  if (fids.empty()) return "";

  QString str;

  std::set<TFrameId>::const_iterator it = fids.begin();

  str = QString::number(it->getNumber());

  while (it != fids.end()) {
    std::set<TFrameId>::const_iterator it1 = it;
    it1++;

    int lastVal = it->getNumber();
    while (it1 != fids.end() && it1->getNumber() == lastVal + 1) {
      lastVal = it1->getNumber();
      it1++;
    }

    if (lastVal != it->getNumber()) str += "-" + QString::number(lastVal);
    if (it1 == fids.end()) return str;

    str += ", " + QString::number(it1->getNumber());

    it = it1;
  }

  return str;
}

}  // namespace

//-----------------------------------------------------------------------------

std::vector<int> string2Indexes(const QString &values) {
  std::vector<int> ret;
  int i, j;
  bool ok;
  QStringList vals = values.split(',', QString::SkipEmptyParts);
  for (i = 0; i < vals.size(); i++) {
    if (vals.at(i).contains('-')) {
      QStringList vals1 = vals.at(i).split('-', QString::SkipEmptyParts);
      if (vals1.size() != 2) return std::vector<int>();
      int from = vals1.at(0).toInt(&ok);
      if (!ok) return std::vector<int>();
      int to = vals1.at(1).toInt(&ok);
      if (!ok) return std::vector<int>();

      for (j = std::min(from, to); j <= std::max(from, to); j++)
        ret.push_back(j);
    } else {
      int val = vals.at(i).toInt(&ok);
      if (!ok) return std::vector<int>();
      ret.push_back(val);
    }
  }
  std::sort(ret.begin(), ret.end());
  return ret;
}

//-----------------------------------------------------------------------------

std::vector<int> DeleteInkDialog::getInkIndexes() {
  return string2Indexes(m_inkIndex->text());
}

std::vector<TFrameId> DeleteInkDialog::getFrames() {
  std::vector<TFrameId> ret;
  std::vector<int> ret1 = string2Indexes(m_frames->text());
  int i;
  for (i = 0; i < (int)ret1.size(); i++) ret.push_back(ret1[i]);
  return ret;
}

DeleteInkDialog::DeleteInkDialog(const QString &str, int inkIndex)
    : Dialog(TApp::instance()->getMainWindow(), true,
             Preferences::instance()->getCurrentLanguage() == "English",
             "DeleteInk") {
  setWindowTitle(tr("Delete Lines"));

  m_inkIndex             = new LineEdit(QString::number(inkIndex));
  m_frames               = new LineEdit(str);
  QPushButton *okBtn     = new QPushButton(tr("Delete"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);

  okBtn->setDefault(true);

  //--- layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(10);
  {
    QGridLayout *upperLay = new QGridLayout();
    upperLay->setMargin(0);
    upperLay->setSpacing(5);
    {
      upperLay->addWidget(new QLabel(tr("Style Index:"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
      upperLay->addWidget(m_inkIndex, 0, 1);

      upperLay->addWidget(new QLabel(tr("Apply to Frames:"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
      upperLay->addWidget(m_frames, 1, 1);
    }
    m_topLayout->addLayout(upperLay);
  }

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(10);
  {
    m_buttonLayout->addWidget(okBtn);
    m_buttonLayout->addWidget(cancelBtn);
  }

  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

void DeleteInkDialog::setRange(const QString &str) { m_frames->setText(str); }

//-----------------------------------------------------------------------------
/*--
         DeleteMatchLineコマンドから呼ばれる場合：chooseInkがfalse
         DeleteLinesコマンドから呼ばれる場合：chooseInkがtrue
--*/

static void doDeleteMatchlines(TXshSimpleLevel *sl,
                               const std::set<TFrameId> &fids, bool chooseInk) {
  std::vector<int> indexes;
  // vector<TToonzImageP> images;
  std::vector<TFrameId> frames;
  std::vector<TFrameId> fidsToProcess;
  int i;
  if (chooseInk) {
    TPaletteHandle *ph =
        TApp::instance()->getPaletteController()->getCurrentLevelPalette();
    if (LastMatchlineIndex == -1) LastMatchlineIndex = ph->getStyleIndex();

    static DeleteInkDialog *md = 0;

    if (!md)
      md = new DeleteInkDialog(indexes2string(fids), LastMatchlineIndex);
    else
      md->setRange(indexes2string(fids));

    bool ok = false;
    while (!ok) {
      if (md->exec() != QDialog::Accepted) return;
      indexes = md->getInkIndexes();
      if (indexes.empty()) {
        DVGui::warning(QObject::tr(
            "The style index range you specified is not valid: please separate "
            "values with a comma (e.g. 1,2,5) or with a dash (e.g. 4-7 will "
            "refer to indexes 4, 5, 6 and 7)."));
        continue;
      }

      frames = md->getFrames();
      if (frames.empty()) {
        DVGui::warning(
            QObject::tr("The frame range you specified is not valid: please "
                        "separate values with a comma (e.g. 1,2,5) or with a "
                        "dash (e.g. 4-7 will refer to frames 4, 5, 6 and 7)."));
        continue;
      }
      for (i = 0; i < frames.size(); i++) {
        if (!sl->isFid(frames[i])) continue;
        // images.push_back(sl->getFrame(frames[i], true));
        if (sl->getFrame(frames[i], false)) fidsToProcess.push_back(frames[i]);
      }

      break;
    }
  } else {
    std::set<TFrameId>::const_iterator it = fids.begin();
    for (; it != fids.end(); ++it) {
      // images.push_back(sl->getFrame(*it, true));
      if (sl->getFrame(*it, false)) fidsToProcess.push_back(*it);
    }
  }

  if (fidsToProcess.empty()) {
    DVGui::warning(QObject::tr(
        "No drawing is available in the frame range you specified."));
    return;
  }

  TXshLevel *xl = TApp::instance()->getCurrentLevel()->getLevel();

  TUndoManager::manager()->add(new DeleteMatchlineUndo(
      xl, sl, fidsToProcess, indexes));  //, images[0]->getPalette()));

  applyDeleteMatchline(sl, fidsToProcess, indexes);

  for (int i = 0; i < fidsToProcess.size();
       i++)  // the saveboxes must be updated
    ToolUtils::updateSaveBox(sl, fidsToProcess[i]);

  std::vector<TFrameId> fidsss;
  xl->getFids(fidsss);
  invalidateIcons(xl, fidsss);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  sl->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void deleteMatchlines(TXshSimpleLevel *sl, const std::set<TFrameId> &fids) {
  doDeleteMatchlines(sl, fids, false);
}

void deleteInk(TXshSimpleLevel *sl, const std::set<TFrameId> &fids) {
  doDeleteMatchlines(sl, fids, true);
}

//-----------------------------------------------------------------------------
