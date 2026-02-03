

#include "antialiaspopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "filmstripselection.h"

// ToonzQt includes
#include "toonzqt/intfield.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"

// TnzCore includes
#include "tpixelgr.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tpixelutils.h"
#include "tundo.h"
#include "trop.h"
#include "timagecache.h"

// Qt includes
#include <QSplitter>
#include <QScrollArea>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>

//**************************************************************************
//    Local namespace stuff
//**************************************************************************

namespace {

void onChange(const TRasterP &in, const TRasterP &out, int threshold,
              int softness) {
  assert(in->getSize() == out->getSize());

  in->lock();
  out->lock();

  TRop::antialias(in, out, threshold, softness);

  in->unlock();
  out->unlock();
}

inline void onChange(const TRasterP &ras, int threshold, int softness) {
  onChange(ras->clone(), ras, threshold, softness);
}

}  // namespace

//**************************************************************************
//    AntialiasPopup Swatch
//**************************************************************************

class AntialiasPopup::Swatch final : public PlaneViewer {
  TImageP m_img;

public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent) {
    setBgColor(TPixel32::White, TPixel32::White);
  }

  TRasterP getRaster() const { return m_img->raster(); }

  void setImage(const TImageP &img) { m_img = img; }

  void paintGL() override {
    drawBackground();

    if (m_img) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // Note GL_ONE instead of GL_SRC_ALPHA: it's needed since the input
      // image is supposedly premultiplied - and it works because the
      // viewer's background is opaque.
      // See tpixelutils.h's overPixT function for comparison.

      pushGLWorldCoordinates();
      draw(m_img);
      popGLCoordinates();

      glDisable(GL_BLEND);
    }
  }
};

//**************************************************************************
//    AntialiasPopup implementation
//**************************************************************************

AntialiasPopup::AntialiasPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false, "Antialias")
    , m_startRas(0) {
  setWindowTitle(tr("Apply Antialias"));
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

  //------------------------- Top Layout --------------------------

  QScrollArea *scrollArea = new QScrollArea(splitter);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  splitter->addWidget(scrollArea);
  splitter->setStretchFactor(0, 1);

  QFrame *topWidget = new QFrame(scrollArea);
  topWidget->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                       QSizePolicy::MinimumExpanding));
  scrollArea->setWidget(topWidget);

  QGridLayout *topLayout = new QGridLayout(this);
  topWidget->setLayout(topLayout);

  //------------------------- Parameters --------------------------

  // Brightness
  QLabel *brightnessLabel = new QLabel(tr("Threshold:"));
  topLayout->addWidget(brightnessLabel, 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);

  m_thresholdField = new DVGui::IntField(topWidget);
  m_thresholdField->setRange(0, 255);
  m_thresholdField->setValue(40);
  topLayout->addWidget(m_thresholdField, 0, 1);

  // Contrast
  QLabel *contrastLabel = new QLabel(tr("Softness:"));
  topLayout->addWidget(contrastLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);

  m_softnessField = new DVGui::IntField(topWidget);
  m_softnessField->setRange(0, 100);
  m_softnessField->setValue(70);
  topLayout->addWidget(m_softnessField, 1, 1);

  topLayout->setRowStretch(2, 1);

  //--------------------------- Swatch ----------------------------

  m_viewer = new Swatch(splitter);
  m_viewer->setMinimumHeight(150);
  m_viewer->setFocusPolicy(Qt::WheelFocus);
  splitter->addWidget(m_viewer);

  //--------------------------- Button ----------------------------

  m_okBtn = new QPushButton(QString(tr("Apply")), this);
  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply()));

  addButtonBarWidget(m_okBtn);

  //------------------------ Connections --------------------------

  TApp *app = TApp::instance();

  bool ret = true;

  ret = ret && connect(m_thresholdField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onValuesChanged(bool)));
  ret = ret && connect(m_softnessField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onValuesChanged(bool)));

  assert(ret);

  m_viewer->resize(0, 350);
  resize(600, 500);
}

//-----------------------------------------------------------------------------

void AntialiasPopup::setCurrentSampleRaster() {
  TRasterP sampleRas;

  m_startRas = TRasterP();
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
  TFilmstripSelection *filmstripSelection =
      dynamic_cast<TFilmstripSelection *>(selection);
  TImageP image;
  if (cellSelection) {
    TApp *app     = TApp::instance();
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(app->getCurrentFrame()->getFrameIndex(),
                                 app->getCurrentColumn()->getColumnIndex());
    TImageP aux    = cell.getImage(true);
    if (aux) image = aux->cloneImage();
  } else if (filmstripSelection) {
    TApp *app                    = TApp::instance();
    TXshSimpleLevel *simpleLevel = app->getCurrentLevel()->getSimpleLevel();
    if (simpleLevel) {
      TImageP imageAux =
          simpleLevel->getFrame(app->getCurrentFrame()->getFid(), true);
      if (imageAux) image = imageAux->cloneImage();
    }
  }

  if (!image || !(sampleRas = image->raster())) {
    m_viewer->setImage(TImageP());
    m_viewer->update();
    m_okBtn->setEnabled(false);
    return;
  }

  m_okBtn->setEnabled(true);
  m_startRas = sampleRas->clone();
  onChange(m_startRas, sampleRas, m_thresholdField->getValue(),
           m_softnessField->getValue());

  m_viewer->setImage(image);
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void AntialiasPopup::showEvent(QShowEvent *se) {
  TApp *app = TApp::instance();
  bool ret  = true;
  ret = ret && connect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
                       SLOT(setCurrentSampleRaster()));
  ret = ret && connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
                       SLOT(setCurrentSampleRaster()));
  ret = ret && connect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()),
                       this, SLOT(setCurrentSampleRaster()));
  assert(ret);
  setCurrentSampleRaster();
}

//-----------------------------------------------------------------------------

void AntialiasPopup::hideEvent(QHideEvent *he) {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
             SLOT(setCurrentSampleRaster()));
  disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(setCurrentSampleRaster()));
  disconnect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
             SLOT(setCurrentSampleRaster()));

  Dialog::hideEvent(he);

  m_viewer->setImage(TImageP());
  m_startRas = TRasterP();
}

//-----------------------------------------------------------------------------

class TRasterAntialiasUndo final : public TUndo {
  int m_r, m_c, m_threshold, m_softness;

  QString m_rasId;
  int m_rasSize;

public:
  TRasterAntialiasUndo(int threshold, int softness, int r, int c, TRasterP ras)
      : m_r(r)
      , m_c(c)
      , m_rasId()
      , m_threshold(threshold)
      , m_softness(softness)
      , m_rasSize(ras->getLx() * ras->getLy() * ras->getPixelSize()) {
    m_rasId = QString("AntialiasUndo_") + QString::number(uintptr_t(this));
    if ((TRasterCM32P)ras)
      TImageCache::instance()->add(m_rasId,
                                   TToonzImageP(ras, ras->getBounds()));
    else
      TImageCache::instance()->add(m_rasId, TRasterImageP(ras));
  }

  ~TRasterAntialiasUndo() { TImageCache::instance()->remove(m_rasId); }

  void undo() const override {
    TXsheet *xsheet    = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshCell cell      = xsheet->getCell(m_r, m_c);
    TImageP image      = cell.getImage(true);
    TRasterImageP rimg = (TRasterImageP)image;
    TToonzImageP timg  = (TToonzImageP)image;
    if (!rimg && !timg) return;
    if (rimg)
      rimg->setRaster(
          ((TRasterImageP)TImageCache::instance()->get(m_rasId, true))
              ->getRaster()
              ->clone());
    else
      timg->setCMapped(
          ((TToonzImageP)TImageCache::instance()->get(m_rasId, true))
              ->getRaster()
              ->clone());
    TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
    assert(simpleLevel);
    simpleLevel->touchFrame(cell.getFrameId());
    simpleLevel->setDirtyFlag(false);
    IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());

    if (m_isLastInBlock) {
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }
  }

  void redo() const override {
    TXsheet *xsheet    = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshCell cell      = xsheet->getCell(m_r, m_c);
    TImageP image      = cell.getImage(true);
    TRasterImageP rimg = (TRasterImageP)image;
    TToonzImageP timg  = (TToonzImageP)image;
    if (!rimg && !timg) return;
    TRasterP ras = image->raster();
    if (!ras) return;

    onChange(ras, m_threshold, m_softness);
    TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
    assert(simpleLevel);
    simpleLevel->touchFrame(cell.getFrameId());
    simpleLevel->setDirtyFlag(false);
    IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());
    if (m_isLastInBlock) {
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }
  }

  int getSize() const override { return sizeof(*this) + m_rasSize; }

  QString getHistoryString() override { return QObject::tr("Apply Antialias"); }
};

//-----------------------------------------------------------------------------

void AntialiasPopup::apply() {
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  int threshold = m_thresholdField->getValue();
  int softness  = m_softnessField->getValue();
  if (cellSelection) {
    std::set<TImage *> images;
    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsheet      = TApp::instance()->getCurrentXsheet()->getXsheet();
    bool oneImageChanged = false;
    int c, r;
    TUndoManager::manager()->beginBlock();
    for (c = c0; c <= c1; c++)
      for (r = r0; r <= r1; r++) {
        TXshCell cell = xsheet->getCell(r, c);
        TImageP image = cell.getImage(true);
        if (!image) continue;
        if (images.find(image.getPointer()) != images.end()) continue;
        TRasterP ras = image->raster();
        if (!ras) continue;
        images.insert(image.getPointer());
        oneImageChanged = true;
        TUndoManager::manager()->add(
            new TRasterAntialiasUndo(threshold, softness, r, c, ras->clone()));
        onChange(ras, threshold, softness);
        TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
        assert(simpleLevel);
        simpleLevel->touchFrame(cell.getFrameId());
        simpleLevel->setDirtyFlag(true);
        IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());
      }
    TUndoManager::manager()->endBlock();
    if (oneImageChanged) {
      close();
      return;
    }
  }
  TFilmstripSelection *filmstripSelection =
      dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
  if (filmstripSelection) {
    TXshSimpleLevel *simpleLevel =
        TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (simpleLevel) {
      std::set<TFrameId> fids = filmstripSelection->getSelectedFids();
      bool oneImageChanged    = false;
      for (auto const fid : fids) {
        TImageP image = simpleLevel->getFrame(fid, true);
        if (!image) continue;
        TRasterP ras = image->raster();
        if (!ras) continue;
        oneImageChanged = true;
        onChange(ras, threshold, softness);
        simpleLevel->touchFrame(fid);
        simpleLevel->setDirtyFlag(true);
        IconGenerator::instance()->invalidate(simpleLevel, fid);
      }
      if (oneImageChanged) {
        close();
        return;
      }
    }
  }

  DVGui::error(QObject::tr("The current selection is invalid."));
  return;
}

//-----------------------------------------------------------------------------

void AntialiasPopup::onValuesChanged(bool dragging) {
  if (dragging || !m_startRas || !m_viewer->getRaster()) return;

  onChange(m_startRas, m_viewer->getRaster(), m_thresholdField->getValue(),
           m_softnessField->getValue());
  m_viewer->update();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AntialiasPopup> openAntialiasPopup(MI_Antialias);
