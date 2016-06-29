

#include "binarizepopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "filmstripselection.h"

// TnzQt includes
#include "toonzqt/intfield.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/checkbox.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheet.h"
#include "toonz/tbinarizer.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "trasterimage.h"
#include "tpixelutils.h"
#include "tundo.h"
#include "timagecache.h"
#include "timage_io.h"

// Qt includes
#include <QSplitter>
#include <QScrollArea>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <QProgressBar>
#include <QMainWindow>

//**************************************************************************
//    TBinarizeUndo
//**************************************************************************

class TBinarizeUndo final : public TUndo {
  std::wstring m_levelName;
  TFrameId m_fid;
  bool m_alphaEnabled;
  QString m_rasId;
  int m_rasSize;  // used in TBinarizeUndo::getSize()

public:
  // note: create Undo BEFORE binarizing. Undo is keeping a copy of the current
  // raster
  TBinarizeUndo(TXshSimpleLevel *sl, const TFrameId &fid, bool alphaEnabled)
      : m_levelName(sl->getName()), m_fid(fid), m_alphaEnabled(alphaEnabled) {
    /* FIXME: clang
     * でコンパイルできなかったのでキャストにしたがこのダウンキャスト本当に大丈夫か?
     */
    TRasterImageP ri = (TRasterImageP)sl->getFrame(m_fid, false)->cloneImage();
    m_rasId = QString("BinarizeUndo") + QString::number((uintptr_t)this);
    TImageCache::instance()->add(m_rasId, ri);
    m_rasSize = ri->getRaster()->getLx() * ri->getRaster()->getLy() * 4;
  }

  ~TBinarizeUndo() { TImageCache::instance()->remove(m_rasId); }

  TXshSimpleLevel *getLevel() const {
    TXshLevel *xshLevel = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getLevelSet()
                              ->getLevel(m_levelName);
    return xshLevel ? xshLevel->getSimpleLevel() : 0;
  }
  void notify(TXshSimpleLevel *sl) const {
    sl->touchFrame(m_fid);
    IconGenerator::instance()->invalidate(sl, m_fid);
    if (m_isLastInBlock) {
      TApp::instance()->getCurrentLevel()->notifyLevelChange();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }
  }

  void undo() const override {
    TXshSimpleLevel *sl = getLevel();
    if (!sl) return;
    TRasterImageP ri = sl->getFrame(m_fid, true);
    if (!ri) return;

    TRasterImageP oldRaster = TImageCache::instance()->get(m_rasId, false);
    ri->getRaster()->copy(oldRaster->getRaster());
    notify(sl);
  }

  void redo() const override {
    TXshSimpleLevel *sl = getLevel();
    if (!sl) return;
    TRasterImageP ri = sl->getFrame(m_fid, true);
    if (!ri) return;
    TRaster32P ras = ri->getRaster();
    if (!ras) return;
    TBinarizer binarizer;
    binarizer.enableAlpha(m_alphaEnabled);
    binarizer.process(ras);
    notify(sl);
  }

  int getSize() const override { return sizeof(*this) + m_rasSize; }
};

//**************************************************************************
//    BinarizePopup Swatch
//**************************************************************************

class BinarizePopup::Swatch final : public PlaneViewer {
  TRasterP m_ras;

public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent) {}

  TRasterP raster() const { return m_ras; }
  TRasterP &raster() { return m_ras; }

  void paintGL() override {
    pushGLWorldCoordinates();
    drawBackground();

    if (m_ras) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // Note GL_ONE instead of GL_SRC_ALPHA: it's needed since the input
      // image is supposedly premultiplied - and it works because the
      // viewer's background is opaque.
      // See tpixelutils.h's overPixT function for comparison.

      draw(m_ras);
      glDisable(GL_BLEND);
    }
    popGLCoordinates();
  }
};

//**************************************************************************
//    BinarizePopup implementation
//**************************************************************************

BinarizePopup::BinarizePopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false, "Binarize")
    , m_frameIndex(-1)
    , m_progressBar(0)
    , m_timerId(0) {
  setWindowTitle(tr("Binarize"));
  setLabelWidth(0);
  setModal(false);

  setTopMargin(0);
  setTopSpacing(0);

  beginVLayout();

  QSplitter *splitter = new QSplitter(Qt::Vertical);
  splitter->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                      QSizePolicy::MinimumExpanding));
  addWidget(splitter);

  endVLayout();

  QWidget *parametersArea = new QFrame();
  splitter->addWidget(parametersArea);

  QGridLayout *parametersLayout = new QGridLayout();

  // parametersArea->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
  // QSizePolicy::Fixed));

  // QFrame* topWidget = new QFrame(scrollArea);
  // topWidget->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Fixed));
  // scrollArea->setWidget(topWidget);

  // parametersLayout->setSizeConstraint(QLayout::SetFixedSize);

  //------------------------- Parameters --------------------------

  /*
//Brightness
  QLabel* brightnessLabel = new QLabel(tr("Brightness:"));
  topLayout->addWidget(brightnessLabel, 0, 0, Qt::AlignRight |
Qt::AlignVCenter);

m_brightnessField = new DVGui::IntField(topWidget);
m_brightnessField->setRange(-127, 127);
m_brightnessField->setValue(0);
  topLayout->addWidget(m_brightnessField, 0, 1);

//Contrast
  QLabel* contrastLabel = new QLabel(tr("Contrast:"));
  topLayout->addWidget(contrastLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);

m_contrastField = new DVGui::IntField(topWidget);
m_contrastField->setRange(-127, 127);
m_contrastField->setValue(0);
  topLayout->addWidget(m_contrastField, 1, 1);
*/

  m_alphaChk   = new DVGui::CheckBox(tr("Alpha"));
  m_previewChk = new DVGui::CheckBox(tr("Preview"));
  parametersLayout->addWidget(m_alphaChk, 0, 0);
  parametersLayout->addWidget(m_previewChk, 0, 1);

  parametersArea->setLayout(parametersLayout);

  //--------------------------- Swatch ----------------------------

  m_viewer = new Swatch();
  m_viewer->setMinimumHeight(150);
  m_viewer->setFocusPolicy(Qt::WheelFocus);
  splitter->addWidget(m_viewer);

  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);

  //--------------------------- Button ----------------------------

  m_okBtn = new QPushButton(tr("Apply"), this);
  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply()));

  addButtonBarWidget(m_okBtn);

  //------------------------ Connections --------------------------

  bool ret = true;

  // ret = ret && connect(m_brightnessField, SIGNAL(valueChanged(bool)), this,
  // SLOT(onValuesChanged(bool)));
  // ret = ret && connect(m_contrastField,   SIGNAL(valueChanged(bool)), this,
  // SLOT(onValuesChanged(bool)));
  ret = ret && connect(m_previewChk, SIGNAL(stateChanged(int)), this,
                       SLOT(onPreviewCheckboxChanged(int)));

  ret = ret && connect(m_alphaChk, SIGNAL(stateChanged(int)), this,
                       SLOT(onAlphaCheckboxChanged(int)));

  assert(ret);

  m_viewer->resize(0, 350);
  resize(600, 500);
}

void BinarizePopup::setSample(const TRasterP &sampleRas) {
  m_inRas = sampleRas;
  if (m_inRas) {
    m_okBtn->setEnabled(true);
  } else {
    m_okBtn->setEnabled(false);
  }
  m_outRas = TRasterP();
  m_previewChk->setChecked(false);
  m_viewer->raster() = m_inRas;
  m_viewer->update();
}

void BinarizePopup::showEvent(QShowEvent *e) {
  TApp *app = TApp::instance();

  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  m_viewer->setBgColor(col1, col2);

  bool ret = true;
  ret = ret && connect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
                       SLOT(fetchSample()));
  ret = ret && connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
                       SLOT(fetchSample()));
  ret = ret && connect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()),
                       this, SLOT(fetchSample()));
  ret = ret &&
        connect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel *)),
                this, SLOT(onLevelSwitched(TXshLevel *)));
  ret = ret && connect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()), this,
                       SLOT(fetchSample()));
  assert(ret);
  m_previewChk->setChecked(false);
  fetchSample();
}

void BinarizePopup::hideEvent(QHideEvent *e) {
  TApp *app = TApp::instance();
  bool ret  = true;
  ret       = ret && disconnect(app->getCurrentFrame(), 0, this, 0);
  ret       = ret && disconnect(app->getCurrentColumn(), 0, this, 0);
  ret       = ret && disconnect(app->getCurrentLevel(), 0, this, 0);
  assert(ret);
}

int BinarizePopup::getSelectedFrames() {
  m_frames.clear();
  TSelection *selection = TSelection::getCurrent();
  TCellSelection *cellSelection;
  TFilmstripSelection *filmstripSelection;
  int count = 0;
  if ((cellSelection = dynamic_cast<TCellSelection *>(selection))) {
    std::set<TRasterImage *> images;
    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
    int c, r;
    for (c = c0; c <= c1; c++) {
      for (r = r0; r <= r1; r++) {
        TXshCell cell          = xsheet->getCell(r, c);
        TRasterImageP rasImage = cell.getImage(false);
        if (!rasImage || !rasImage->getRaster()) continue;
        Frames::value_type item(cell.getSimpleLevel(), cell.getFrameId());
        Frames::iterator it;
        it = std::lower_bound(m_frames.begin(), m_frames.end(), item);
        if (it == m_frames.end() || *it != item) {
          m_frames.insert(it, item);
          count++;
        }
      }
    }
  } else if ((filmstripSelection =
                  dynamic_cast<TFilmstripSelection *>(selection))) {
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (sl) {
      std::set<TFrameId> fids = filmstripSelection->getSelectedFids();
      std::set<TFrameId>::iterator it;
      for (it = fids.begin(); it != fids.end(); ++it) {
        TRasterImageP rasImage = sl->getFrame(*it, false);
        if (!!rasImage && !!rasImage->getRaster()) {
          m_frames.push_back(std::make_pair(sl, *it));
          count++;
        }
      }
    }
  } else {
  }
  m_frameIndex = 0;
  return count;
}

void BinarizePopup::computePreview() {
  if (!m_inRas) return;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  if (m_outRas)
    m_outRas->copy(m_inRas);
  else
    m_outRas = m_inRas->clone();
  TBinarizer binarizer;
  binarizer.enableAlpha(!!m_alphaChk->checkState());
  binarizer.process(m_outRas);
  QApplication::restoreOverrideCursor();
}

void BinarizePopup::onPreviewCheckboxChanged(int state) {
  if (!m_inRas) return;
  if (state) {
    if (!m_outRas) computePreview();
    m_viewer->raster() = m_outRas;
  } else {
    m_viewer->raster() = m_inRas;
  }
  m_viewer->update();
}

void BinarizePopup::onAlphaCheckboxChanged(int state) {
  if (!!m_outRas && m_previewChk->checkState()) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    computePreview();
    QApplication::restoreOverrideCursor();
    m_viewer->update();
  }
}

void BinarizePopup::fetchSample() {
  TApp *app = TApp::instance();
  TImageP img;
  if (app->getCurrentFrame()->isEditingLevel()) {
    TXshLevel *xshLevel = app->getCurrentLevel()->getLevel();
    if (xshLevel && xshLevel->getSimpleLevel()) {
      TXshSimpleLevel *sl = xshLevel->getSimpleLevel();
      img = sl->getFrame(app->getCurrentFrame()->getFid(), false);
    }
  } else {
    TXsheet *xsh  = app->getCurrentScene()->getScene()->getXsheet();
    TXshCell cell = xsh->getCell(app->getCurrentFrame()->getFrame(),
                                 app->getCurrentColumn()->getColumnIndex());
    img = cell.getImage(false);
  }
  TRasterImageP ri = img;
  if (ri) {
    setSample(ri->getRaster());
  } else {
    setSample(TRasterP());
  }
}

void BinarizePopup::apply() {
  if (getSelectedFrames() <= 0) {
    DVGui::error(tr("No raster frames selected"));
    return;
  }

  DVGui::ProgressDialog pd(tr("Binarizing images"), tr("Cancel"), 0,
                           (int)m_frames.size(), 0);
  pd.show();
  qApp->processEvents();

  TBinarizer binarizer;
  binarizer.enableAlpha(!!m_alphaChk->checkState());

  TUndoManager::manager()->beginBlock();
  int count = 0;
  Frames::iterator it;
  for (it = m_frames.begin(); it != m_frames.end(); ++it) {
    TXshSimpleLevel *sl = it->first;
    if (!!m_alphaChk->checkState()) sl->getProperties()->setHasAlpha(true);
    TFrameId fid = it->second;
    TBinarizeUndo *undo =
        new TBinarizeUndo(sl, fid, binarizer.isAlphaEnabled());
    TUndoManager::manager()->add(undo);
    TRasterImageP ri = sl->getFrame(fid, true);
    if (!ri) continue;  // should never happen
    TRaster32P ras32 = ri->getRaster();
    if (!ras32) continue;  // not yet handled
    binarizer.process(ras32);
    pd.setValue(count++);
    qApp->processEvents();
    sl->touchFrame(fid);
    sl->setDirtyFlag(true);
    IconGenerator::instance()->invalidate(sl, fid);
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

OpenPopupCommandHandler<BinarizePopup> openBinarizePopup(MI_Binarize);
