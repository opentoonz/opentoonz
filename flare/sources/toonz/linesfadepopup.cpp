

#include "linesfadepopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "filmstripselection.h"
#include "imageviewer.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/intfield.h"

// TnzCore includes
#include "tpixelgr.h"
#include "trasterimage.h"
#include "tundo.h"
#include "timagecache.h"

// Qt includes
#include <QSplitter>
#include <QScrollArea>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>

using namespace DVGui;

//**************************************************************************
//    Local namespace stuff
//**************************************************************************

namespace {

void onChange(TRaster32P &rasIn, const TRaster32P &rasOut, TPixel32 col,
              int _intensity) {
  double intensity = _intensity / 100.0;
  col              = premultiply(col);

  int j;
  rasIn->lock();
  rasOut->lock();
  for (j = 0; j < rasIn->getLy(); j++) {
    TPixel32 *pixIn    = rasIn->pixels(j);
    TPixel32 *pixOut   = rasOut->pixels(j);
    TPixel32 *endPixIn = pixIn + rasIn->getLx();
    while (pixIn < endPixIn) {
      double factor = pixIn->m / (double)255;

      int val;
      val       = troundp(pixIn->r + intensity * (col.r * factor - pixIn->r));
      pixOut->r = (val > 255) ? 255 : val;
      val       = troundp(pixIn->g + intensity * (col.g * factor - pixIn->g));
      pixOut->g = (val > 255) ? 255 : val;
      val       = troundp(pixIn->b + intensity * (col.b * factor - pixIn->b));
      pixOut->b = (val > 255) ? 255 : val;
      val       = troundp(pixIn->m + intensity * (col.m * factor - pixIn->m));
      pixOut->m = (val > 255) ? 255 : val;

      /*  Fading matte here
pix->r=(UCHAR)(pix->r+intensity*(col.r-pix->r));
pix->g=(UCHAR)(pix->g+intensity*(col.g-pix->g));
pix->b=(UCHAR)(pix->b+intensity*(col.b-pix->b));
pix->m=(UCHAR)(pix->m+intensity*(col.m-pix->m));*/

      ++pixOut;
      ++pixIn;
    }
  }

  rasIn->unlock();
  rasOut->unlock();
}

}  // namespace

//**************************************************************************
//    BrightnessAndContrastPopup Swatch
//**************************************************************************

class LinesFadePopup::Swatch final : public PlaneViewer {
  TRasterP m_ras;

public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent) {
    setBgColor(TPixel32::White, TPixel32::White);
  }

  TRasterP raster() const { return m_ras; }
  TRasterP &raster() { return m_ras; }

  void paintGL() override {
    drawBackground();

    if (m_ras) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // Note GL_ONE instead of GL_SRC_ALPHA: it's needed since the input
      // image is supposedly premultiplied - and it works because the
      // viewer's background is opaque.
      // See tpixelutils.h's overPixT function for comparison.

      pushGLWorldCoordinates();
      draw(m_ras);
      popGLCoordinates();

      glDisable(GL_BLEND);
    }
  }
};

//**************************************************************************
//    LinesFadePopup implementation
//**************************************************************************

LinesFadePopup::LinesFadePopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false, "LinesFade")
    , m_startRas(0) {
  setWindowTitle(tr("Color Fade"));
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

  // Fade
  QLabel *fadeLabel = new QLabel(tr("Fade:"));
  topLayout->addWidget(fadeLabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);

  m_linesColorField = new DVGui::ColorField(this, true, TPixel32::Black, 40);
  m_linesColorField->setFixedHeight(40);

  topLayout->addWidget(m_linesColorField, 0, 1);

  // Intensity
  QLabel *intensityLabel = new QLabel(tr("Intensity:"));
  topLayout->addWidget(intensityLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);

  m_intensity = new IntField(this);
  m_intensity->setValues(100, 0, 100);
  topLayout->addWidget(m_intensity, 1, 1);

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

  ret = ret &&
        connect(m_linesColorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(onLinesColorChanged(const TPixel32 &, bool)));
  ret = ret && connect(m_intensity, SIGNAL(valueChanged(bool)), this,
                       SLOT(onIntensityChanged(bool)));

  assert(ret);

  m_viewer->resize(0, 350);
  resize(600, 500);
}

//-----------------------------------------------------------------------------

void LinesFadePopup::setCurrentSampleRaster() {
  TRaster32P sampleRas;

  m_startRas = TRaster32P();
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
  TFilmstripSelection *filmstripSelection =
      dynamic_cast<TFilmstripSelection *>(selection);
  if (cellSelection) {
    TApp *app     = TApp::instance();
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(app->getCurrentFrame()->getFrameIndex(),
                                 app->getCurrentColumn()->getColumnIndex());
    TRasterImageP rasImage = cell.getImage(true);
    if (rasImage && rasImage->getRaster())
      sampleRas = rasImage->getRaster()->clone();
  } else if (filmstripSelection) {
    TApp *app                    = TApp::instance();
    TXshSimpleLevel *simpleLevel = app->getCurrentLevel()->getSimpleLevel();
    if (simpleLevel) {
      TRasterImageP rasImage = (TRasterImageP)simpleLevel->getFrame(
          app->getCurrentFrame()->getFid(), true);
      if (rasImage && rasImage->getRaster())
        sampleRas = rasImage->getRaster()->clone();
    }
  }
  if (!sampleRas) {
    m_viewer->raster() = TRasterP();
    m_viewer->update();
    m_okBtn->setEnabled(false);
    return;
  }

  m_okBtn->setEnabled(true);
  m_startRas = sampleRas->clone();
  onChange(m_startRas, sampleRas, m_linesColorField->getColor(),
           m_intensity->getValue());

  m_viewer->raster() = sampleRas;
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void LinesFadePopup::showEvent(QShowEvent *se) {
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

void LinesFadePopup::hideEvent(QHideEvent *he) {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
             SLOT(setCurrentSampleRaster()));
  disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(setCurrentSampleRaster()));
  disconnect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
             SLOT(setCurrentSampleRaster()));
  Dialog::hideEvent(he);

  m_viewer->raster() = TRasterP();
  m_startRas         = TRaster32P();
}

//-----------------------------------------------------------------------------

class TLineFadeUndo final : public TUndo {
  int m_r, m_c;
  TPixel32 m_color;
  int m_intensity;

  QString m_rasId;
  int m_rasSize;

public:
  TLineFadeUndo(const TPixel32 &color, int intensity, int r, int c,
                TRaster32P ras)
      : m_r(r)
      , m_c(c)
      , m_rasId()
      , m_color(color)
      , m_intensity(intensity)
      , m_rasSize(ras->getLx() * ras->getLy() * ras->getPixelSize()) {
    m_rasId = QString("LineFadeUndo") + QString::number((uintptr_t)this);
    TImageCache::instance()->add(m_rasId, TRasterImageP(ras));
  }

  ~TLineFadeUndo() { TImageCache::instance()->remove(m_rasId); }

  void undo() const override {
    TXsheet *xsheet        = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshCell cell          = xsheet->getCell(m_r, m_c);
    TRasterImageP rasImage = (TRasterImageP)cell.getImage(true);
    if (!rasImage) return;
    rasImage->setRaster(
        ((TRasterImageP)TImageCache::instance()->get(m_rasId, true))
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
    TXsheet *xsheet        = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshCell cell          = xsheet->getCell(m_r, m_c);
    TRasterImageP rasImage = (TRasterImageP)cell.getImage(true);
    if (!rasImage) return;
    TRaster32P ras = rasImage->getRaster();
    if (!ras) return;

    onChange(ras, ras, m_color, m_intensity);
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
};

//-----------------------------------------------------------------------------

void LinesFadePopup::apply() {
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  TPixel32 color = m_linesColorField->getColor();
  int intensity  = m_intensity->getValue();

  if (cellSelection) {
    std::set<TRasterImage *> images;
    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsheet      = TApp::instance()->getCurrentXsheet()->getXsheet();
    bool oneImageChanged = false;
    int c, r;
    TUndoManager::manager()->beginBlock();
    for (c = c0; c <= c1; c++)
      for (r = r0; r <= r1; r++) {
        TXshCell cell          = xsheet->getCell(r, c);
        TRasterImageP rasImage = (TRasterImageP)cell.getImage(true);
        if (!rasImage) continue;
        if (images.find(rasImage.getPointer()) != images.end()) continue;
        TRaster32P ras = rasImage->getRaster();
        if (!ras) continue;
        images.insert(rasImage.getPointer());
        TUndoManager::manager()->add(
            new TLineFadeUndo(color, intensity, r, c, ras->clone()));
        oneImageChanged = true;
        onChange(ras, ras, color, intensity);
        TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
        assert(simpleLevel);
        simpleLevel->touchFrame(cell.getFrameId());
        simpleLevel->setDirtyFlag(true);
        IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());
      }
    TUndoManager::manager()->endBlock();
    images.clear();
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
      for (auto const &fid : fids) {
        TRasterImageP rasImage =
            (TRasterImageP)simpleLevel->getFrame(fid, true);
        ;
        if (!rasImage) continue;
        TRaster32P ras = rasImage->getRaster();
        if (!ras) continue;
        oneImageChanged = true;
        onChange(ras, ras, color, intensity);
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

void LinesFadePopup::onLinesColorChanged(const TPixel32 &color,
                                         bool isDragging) {
  if (!m_startRas || !m_viewer->raster()) return;
  onChange(m_startRas, m_viewer->raster(), color, m_intensity->getValue());
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void LinesFadePopup::onIntensityChanged(bool isDragging) {
  if (!m_startRas || !m_viewer->raster()) return;
  onChange(m_startRas, m_viewer->raster(), m_linesColorField->getColor(),
           m_intensity->getValue());
  m_viewer->update();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<LinesFadePopup> openLinesFadePopup(MI_LinesFade);
