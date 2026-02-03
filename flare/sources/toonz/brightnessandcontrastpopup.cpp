

#include "brightnessandcontrastpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "filmstripselection.h"

// TnzQt includes
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
#include "timagecache.h"
#include "tpixelgr.h"
#include "trasterimage.h"
#include "tpixelutils.h"
#include "tundo.h"

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

void my_compute_lut(double contrast, double brightness, std::vector<int> &lut) {
  const int maxChannelValue          = lut.size() - 1;
  const double half_maxChannelValueD = 0.5 * maxChannelValue;
  const double maxChannelValueD      = maxChannelValue;

  int i;
  double value, nvalue, power;

  int lutSize = lut.size();
  for (i = 0; i < lutSize; i++) {
    value = i / maxChannelValueD;

    // brightness
    if (brightness < 0.0)
      value = value * (1.0 + brightness);
    else
      value = value + ((1.0 - value) * brightness);

    // contrast
    if (contrast < 0.0) {
      if (value > 0.5)
        nvalue = 1.0 - value;
      else
        nvalue                 = value;
      if (nvalue < 0.0) nvalue = 0.0;
      nvalue = 0.5 * pow(nvalue * 2.0, (double)(1.0 + contrast));
      if (value > 0.5)
        value = 1.0 - nvalue;
      else
        value = nvalue;
    } else {
      if (value > 0.5)
        nvalue = 1.0 - value;
      else
        nvalue                 = value;
      if (nvalue < 0.0) nvalue = 0.0;
      power =
          (contrast == 1.0) ? half_maxChannelValueD : 1.0 / (1.0 - contrast);
      nvalue = 0.5 * pow(2.0 * nvalue, power);
      if (value > 0.5)
        value = 1.0 - nvalue;
      else
        value = nvalue;
    }

    lut[i] = value * maxChannelValueD;
  }
}

//-----------------------------------------------------------------------------

template <typename PIX>
inline void doPix(PIX *outPix, const PIX *inPix, const std::vector<int> &lut) {
  if (inPix->m) {
    *outPix   = depremultiply(*inPix);
    outPix->r = lut[outPix->r];
    outPix->g = lut[outPix->g];
    outPix->b = lut[outPix->b];
    outPix->m = outPix->m;
    *outPix   = premultiply(*outPix);
  }
}

//-----------------------------------------------------------------------------

template <>
inline void doPix<TPixelGR8>(TPixelGR8 *outPix, const TPixelGR8 *inPix,
                             const std::vector<int> &lut) {
  outPix->value = lut[inPix->value];
}

//-----------------------------------------------------------------------------

template <>
inline void doPix<TPixelGR16>(TPixelGR16 *outPix, const TPixelGR16 *inPix,
                              const std::vector<int> &lut) {
  outPix->value = lut[inPix->value];
}

//-----------------------------------------------------------------------------

template <typename PIX>
void onChange(const TRasterPT<PIX> &in, const TRasterPT<PIX> &out, int contrast,
              int brightness) {
  assert(in->getSize() == out->getSize());

  double b      = brightness / 127.0;
  double c      = contrast / 127.0;
  if (c > 1) c  = 1;
  if (c < -1) c = -1;

  std::vector<int> lut(PIX::maxChannelValue + 1);
  my_compute_lut(c, b, lut);

  in->lock();
  out->lock();

  int lx = in->getLx(), y, ly = in->getLy();
  for (y = 0; y < ly; ++y) {
    const PIX *inPix = in->pixels(y), *endInPix = inPix + lx;
    PIX *outPix = out->pixels(y);

    while (inPix < endInPix) {
      doPix(outPix, inPix, lut);

      ++inPix;
      ++outPix;
    }
  }

  in->unlock();
  out->unlock();
}

//-----------------------------------------------------------------------------

void onChange(const TRasterP &in, const TRasterP &out, int contrast,
              int brightness) {
  TRaster32P in32(in);
  if (in32) {
    onChange<TPixel32>(in32, out, contrast, brightness);
    return;
  }

  TRaster64P in64(in);
  if (in64) {
    onChange<TPixel64>(in64, out, contrast, brightness);
    return;
  }

  TRasterGR8P inGR8(in);
  if (inGR8) {
    onChange<TPixelGR8>(inGR8, out, contrast, brightness);
    return;
  }

  TRasterGR16P inGR16(in);
  if (inGR16) onChange<TPixelGR16>(inGR16, out, contrast, brightness);
}

//-----------------------------------------------------------------------------

template <typename RAS>
inline void onChange(const RAS &ras, int contrast, int brightness) {
  onChange(ras, ras, contrast, brightness);
}

}  // namespace

//**************************************************************************
//    BrightnessAndContrastPopup Swatch
//**************************************************************************

class BrightnessAndContrastPopup::Swatch final : public PlaneViewer {
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
//    BrightnessAndContrastPopup implementation
//**************************************************************************

BrightnessAndContrastPopup::BrightnessAndContrastPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "BrightnessAndContrast")
    , m_startRas(0) {
  setWindowTitle(tr("Brightness and Contrast"));
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
  QLabel *brightnessLabel = new QLabel(tr("Brightness:"));
  topLayout->addWidget(brightnessLabel, 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);

  m_brightnessField = new DVGui::IntField(topWidget);
  m_brightnessField->setRange(-127, 127);
  m_brightnessField->setValue(0);
  topLayout->addWidget(m_brightnessField, 0, 1);

  // Contrast
  QLabel *contrastLabel = new QLabel(tr("Contrast:"));
  topLayout->addWidget(contrastLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);

  m_contrastField = new DVGui::IntField(topWidget);
  m_contrastField->setRange(-127, 127);
  m_contrastField->setValue(0);
  topLayout->addWidget(m_contrastField, 1, 1);

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

  ret = ret && connect(m_brightnessField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onValuesChanged(bool)));
  ret = ret && connect(m_contrastField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onValuesChanged(bool)));

  assert(ret);

  m_viewer->resize(0, 350);
  resize(600, 500);
}

//-----------------------------------------------------------------------------

void BrightnessAndContrastPopup::setCurrentSampleRaster() {
  TRasterP sampleRas;

  m_startRas = TRasterP();
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
  onChange(m_startRas, sampleRas, m_contrastField->getValue(),
           m_brightnessField->getValue());

  m_viewer->raster() = sampleRas;
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void BrightnessAndContrastPopup::showEvent(QShowEvent *se) {
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

void BrightnessAndContrastPopup::hideEvent(QHideEvent *he) {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentFrame(), SIGNAL(frameTypeChanged()), this,
             SLOT(setCurrentSampleRaster()));
  disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(setCurrentSampleRaster()));
  disconnect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
             SLOT(setCurrentSampleRaster()));

  Dialog::hideEvent(he);

  m_viewer->raster() = TRasterP();
  m_startRas         = TRasterP();
}

//-----------------------------------------------------------------------------

class TRasterBrightnessUndo final : public TUndo {
  int m_r, m_c, m_brightness, m_contrast;

  QString m_rasId;
  int m_rasSize;

public:
  TRasterBrightnessUndo(int brightness, int contrast, int r, int c,
                        TRasterP ras)
      : m_r(r)
      , m_c(c)
      , m_rasId()
      , m_brightness(brightness)
      , m_contrast(contrast)
      , m_rasSize(ras->getLx() * ras->getLy() * ras->getPixelSize()) {
    m_rasId = QString("BrightnessUndo") + QString::number((uintptr_t)this);
    TImageCache::instance()->add(m_rasId, TRasterImageP(ras));
  }

  ~TRasterBrightnessUndo() { TImageCache::instance()->remove(m_rasId); }

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
    TRasterP ras = rasImage->getRaster();
    if (!ras) return;

    onChange(ras, m_contrast, m_brightness);
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

void BrightnessAndContrastPopup::apply() {
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  int brightness = m_brightnessField->getValue();
  int contrast   = m_contrastField->getValue();
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
        TRasterP ras = rasImage->getRaster();
        if (!ras) continue;
        images.insert(rasImage.getPointer());
        oneImageChanged = true;
        TUndoManager::manager()->add(new TRasterBrightnessUndo(
            brightness, contrast, r, c, ras->clone()));
        onChange(ras, contrast, brightness);
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
      std::set<TFrameId> fids         = filmstripSelection->getSelectedFids();
      std::set<TFrameId>::iterator it = fids.begin();
      bool oneImageChanged            = false;
      for (auto const &fid : fids) {
        TRasterImageP rasImage =
            (TRasterImageP)simpleLevel->getFrame(fid, true);
        ;
        if (!rasImage) continue;
        TRasterP ras = rasImage->getRaster();
        if (!ras) continue;
        oneImageChanged = true;
        onChange(ras, contrast, brightness);
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

void BrightnessAndContrastPopup::onValuesChanged(bool dragging) {
  if (!m_startRas || !m_viewer->raster()) return;

  onChange(m_startRas, m_viewer->raster(), m_contrastField->getValue(),
           m_brightnessField->getValue());
  m_viewer->update();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<BrightnessAndContrastPopup>
    openBrightnessAndContrastPopup(MI_BrightnessAndContrast);
