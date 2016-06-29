

#include "adjustlevelspopup.h"

// Tnz6 includes
#include "tapp.h"
#include "cellselection.h"
#include "filmstripselection.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/histogram.h"
#include "toonzqt/marksbar.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/intfield.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"

// TnzCore includes
#include "trasterimage.h"
#include "trop.h"
#include "tundo.h"
#include "timagecache.h"

// Qt includes
#include <QPushButton>
#include <QSplitter>
#include <QScrollArea>
#include <QMainWindow>

//**************************************************************************
//    Local namespace stuff
//**************************************************************************

namespace {

void resetMarksBar(MarksBar *marksBar) {
  QVector<int> &values = marksBar->values();
  values[0]            = 0;
  values[1]            = 255;
}

//--------------------------------------------------------------

// Get the exact range for specified value
void getRange(const QVector<int> &values, int &min, int &max) {
  int size = values.size();

  for (min = 0; min < size; ++min) {
    if (values[min]) break;
  }

  for (max = size - 1; max >= 0; --max) {
    if (values[max]) break;
  }
}

//--------------------------------------------------------------

// Get the threshold-permissive range for values.
void getRange(const QVector<int> &values, int threshold, int &min, int &max) {
  int val, size = values.size(), count;

  count = values[0];  // Always Consent 1 value cut
  for (min = 1; min < size; ++min) {
    val = values[min];
    count += val;
    if (val && count > threshold)  // Then, stop before a positive value when
                                   // thresh is exceeded
      break;
  }

  count = values[size - 1];
  for (max = size - 2; max >= 0; --max) {
    val = values[max];
    count += val;
    if (val && count > threshold) break;
  }
}

}  // namespace

//**************************************************************************
//    Adjust Levels Swatch
//**************************************************************************

class AdjustLevelsPopup::Swatch final : public PlaneViewer {
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
//    EditableMarksBar implementation
//**************************************************************************

EditableMarksBar::EditableMarksBar(QWidget *parent) : QFrame(parent) {
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setMargin(0);
  layout->setSpacing(2);
  setLayout(layout);

  // Add MarksBar
  m_marksBar = new MarksBar;
  m_marksBar->setContentsMargins(5, 0, 6, 0);
  layout->addWidget(m_marksBar);

  // Customize it
  m_marksBar->setRange(0, 256, 2);

  QVector<int> &values = m_marksBar->values();
  values.push_back(0);
  values.push_back(256);

  QVector<QColor> &colors = m_marksBar->colors();
  colors.fill(Qt::black, 2);

  // MarksBar dominates values change notifications
  bool ret =
      connect(m_marksBar, SIGNAL(marksUpdated()), this, SLOT(updateFields()));
  ret = ret && connect(m_marksBar, SIGNAL(marksUpdated()), this,
                       SIGNAL(paramsChanged()));

  // Add fields layout
  QHBoxLayout *hLayout = new QHBoxLayout;
  hLayout->setMargin(0);
  hLayout->setContentsMargins(4, 0, 5, 0);
  layout->addLayout(hLayout);

  m_fields[0] = new DVGui::IntLineEdit;

  hLayout->addWidget(m_fields[0]);
  ret = ret && connect(m_fields[0], SIGNAL(editingFinished()), this,
                       SLOT(onFieldEdited()));

  hLayout->addStretch(1);

  m_fields[1] = new DVGui::IntLineEdit;
  hLayout->addWidget(m_fields[1]);
  ret = ret && connect(m_fields[1], SIGNAL(editingFinished()), this,
                       SLOT(onFieldEdited()));

  updateFields();
}

//--------------------------------------------------------------

EditableMarksBar::~EditableMarksBar() {}

//--------------------------------------------------------------

void EditableMarksBar::getValues(int *values) const {
  const QVector<int> &marks = m_marksBar->values();

  values[0] = marks[0];
  values[1] = marks[1] - 1;
}

//--------------------------------------------------------------

void EditableMarksBar::updateFields() {
  const QVector<int> &values = m_marksBar->values();

  m_fields[0]->setValue(values[0]);
  m_fields[1]->setValue(values[1] - 1);

  // No emission - as it's the marksbar that dominate signal emission
}

//--------------------------------------------------------------

void EditableMarksBar::onFieldEdited() {
  QVector<int> &values = m_marksBar->values();

  // Copy the values to the marksBar
  values[0] = m_fields[0]->getValue();
  values[1] = m_fields[1]->getValue() + 1;

  m_marksBar->conformValues();

  emit paramsChanged();
}

//**************************************************************************
//    Adjust Levels Popup implementation
//**************************************************************************

AdjustLevelsPopup::AdjustLevelsPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "AdjustLevels")
    , m_thresholdD(0.005)  // 0.5% of the image size
{
  int i, j;

  setWindowTitle(tr("Adjust Levels"));
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
  scrollArea->setMinimumWidth(450);
  splitter->addWidget(scrollArea);
  splitter->setStretchFactor(0, 1);

  QFrame *topWidget = new QFrame(scrollArea);
  scrollArea->setWidget(topWidget);

  QVBoxLayout *topVLayout =
      new QVBoxLayout(topWidget);  // Needed to justify at top
  topWidget->setLayout(topVLayout);

  QHBoxLayout *topLayout = new QHBoxLayout(topWidget);
  topVLayout->addLayout(topLayout);
  topVLayout->addStretch(1);

  //------------------------- Histogram ---------------------------

  m_histogram = new Histogram(topWidget);
  topLayout->addWidget(m_histogram);

  //------------------------- Mark Bars ---------------------------

  QVBoxLayout *histogramViewLayout;

  for (i = 0; i < 5; ++i) {
    HistogramView *view = m_histogram->getHistograms()->getHistogramView(i);
    histogramViewLayout = static_cast<QVBoxLayout *>(view->layout());

    // Don't draw channel numbers
    view->channelBar()->setDrawNumbers(false);

    for (j = 0; j < 2; ++j) {
      EditableMarksBar *editableMarksBar = m_marksBar[j + (i << 1)] =
          new EditableMarksBar;
      MarksBar *marksBar = editableMarksBar->marksBar();

      // Set margins up to cover the histogram
      editableMarksBar->layout()->setContentsMargins(6, 0, 5, 0);
      connect(editableMarksBar, SIGNAL(paramsChanged()), this,
              SLOT(onParamsChanged()));

      histogramViewLayout->insertWidget(1 + (j << 1), editableMarksBar);
    }
  }

  //------------------------- View Widget -------------------------

  // NOTE: It's IMPORTANT that parent widget is supplied. It's somewhat
  // used by QSplitter to decide the initial widget sizes...

  m_viewer = new Swatch(splitter);
  m_viewer->setMinimumHeight(150);
  m_viewer->setFocusPolicy(Qt::WheelFocus);
  splitter->addWidget(m_viewer);

  //--------------------------- Buttons ---------------------------

  QVBoxLayout *buttonsLayout = new QVBoxLayout(topWidget);
  topLayout->addLayout(buttonsLayout);

  buttonsLayout->addSpacing(50);

  QPushButton *clampRange = new QPushButton(tr("Clamp"), topWidget);
  clampRange->setMinimumSize(65, 25);
  buttonsLayout->addWidget(clampRange);
  connect(clampRange, SIGNAL(clicked(bool)), this, SLOT(clampRange()));

  QPushButton *autoAdjust = new QPushButton(tr("Auto"), topWidget);
  autoAdjust->setMinimumSize(65, 25);
  buttonsLayout->addWidget(autoAdjust);
  connect(autoAdjust, SIGNAL(clicked(bool)), this, SLOT(autoAdjust()));

  QPushButton *resetBtn = new QPushButton(tr("Reset"), topWidget);
  resetBtn->setMinimumSize(65, 25);
  buttonsLayout->addWidget(resetBtn);
  connect(resetBtn, SIGNAL(clicked(bool)), this, SLOT(reset()));

  buttonsLayout->addStretch(1);

  m_okBtn = new QPushButton(tr("Apply"));
  addButtonBarWidget(m_okBtn);

  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply()));

  // Finally, acquire current selection
  acquireRaster();

  m_viewer->resize(0, 350);
  resize(600, 700);
}

//--------------------------------------------------------------

void AdjustLevelsPopup::showEvent(QShowEvent *se) {
  TSelectionHandle *selectionHandle = TApp::instance()->getCurrentSelection();
  connect(selectionHandle, SIGNAL(selectionChanged(TSelection *)), this,
          SLOT(onSelectionChanged()));

  acquireRaster();
}

//--------------------------------------------------------------

void AdjustLevelsPopup::hideEvent(QHideEvent *he) {
  Dialog::hideEvent(he);

  TSelectionHandle *selectionHandle = TApp::instance()->getCurrentSelection();
  disconnect(selectionHandle, SIGNAL(selectionChanged(TSelection *)), this,
             SLOT(onSelectionChanged()));

  m_inputRas         = TRasterP();
  m_viewer->raster() = TRasterP();
}

//--------------------------------------------------------------

void AdjustLevelsPopup::acquireRaster() {
  // Retrieve current selection
  TApp *app                     = TApp::instance();
  TSelection *selection         = app->getCurrentSelection()->getSelection();
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
  TFilmstripSelection *filmstripSelection =
      dynamic_cast<TFilmstripSelection *>(selection);

  // Retrieve the input raster
  m_inputRas = TRasterP();
  if (cellSelection) {
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(app->getCurrentFrame()->getFrameIndex(),
                                 app->getCurrentColumn()->getColumnIndex());
    TRasterImageP rasImage                            = cell.getImage(true);
    if (rasImage && rasImage->getRaster()) m_inputRas = rasImage->getRaster();
  } else if (filmstripSelection) {
    TXshSimpleLevel *simpleLevel = app->getCurrentLevel()->getSimpleLevel();
    if (simpleLevel) {
      TRasterImageP rasImage = (TRasterImageP)simpleLevel->getFrame(
          app->getCurrentFrame()->getFid(), true);
      if (rasImage && rasImage->getRaster()) m_inputRas = rasImage->getRaster();
    }
  }

  if (m_inputRas) {
    m_threshold = m_inputRas->getLx() * m_inputRas->getLy() * m_thresholdD;
    m_okBtn->setEnabled(true);
  } else {
    m_inputRas = TRasterP();
    m_okBtn->setEnabled(false);
  }

  // Build histograms
  m_histogram->setRaster(m_inputRas);

  // Update the corresponding processed image in the viewer
  updateProcessedImage();
}

//--------------------------------------------------------------

void AdjustLevelsPopup::setThreshold(double t) {
  m_thresholdD = t;
  if (m_inputRas)
    m_threshold = m_inputRas->getLx() * m_inputRas->getLy() * m_thresholdD;
}

//--------------------------------------------------------------

void AdjustLevelsPopup::getParameters(int *in0, int *in1, int *out0,
                                      int *out1) {
  int p[2];

  int i, j;
  for (i = j = 0; i < 10; i += 2, ++j) {
    m_marksBar[i]->getValues(p);
    in0[j] = p[0], in1[j] = p[1];

    m_marksBar[i + 1]->getValues(p);
    out0[j] = p[0], out1[j] = p[1];
  }
}

//--------------------------------------------------------------

void AdjustLevelsPopup::updateProcessedImage() {
  if (!m_inputRas) {
    m_viewer->raster() = TRasterP();
    m_viewer->update();
    return;
  }

  // Allocate a conformant output, if necessary
  TRasterP &outRas = m_viewer->raster();
  if (!outRas || outRas->getPixelSize() != m_inputRas->getPixelSize() ||
      outRas->getSize() != m_inputRas->getSize())
    outRas = m_inputRas->create(m_inputRas->getLx(), m_inputRas->getLy());

  // Perform the operation preview
  int in0[5], in1[5], out0[5], out1[5];
  getParameters(in0, in1, out0, out1);

  TRop::rgbmAdjust(outRas, m_inputRas, in0, in1, out0, out1);

  // Update the swatch
  m_viewer->update();
}

//--------------------------------------------------------------

void AdjustLevelsPopup::onSelectionChanged() { acquireRaster(); }

//--------------------------------------------------------------

// Params were changed. Content must be updated.
void AdjustLevelsPopup::onParamsChanged() { updateProcessedImage(); }

//--------------------------------------------------------------

// Reset ALL channels
void AdjustLevelsPopup::reset() {
  int i;
  for (i = 0; i < 10; ++i) {
    EditableMarksBar *editableMarksBar = m_marksBar[i];

    QVector<int> &marks = editableMarksBar->marksBar()->values();
    marks[0] = 0, marks[1] = 256;

    editableMarksBar->updateFields();
  }

  onParamsChanged();
  update();
}

//--------------------------------------------------------------

void AdjustLevelsPopup::clampRange() {
  if (!m_inputRas) return;

  Histograms *histograms = m_histogram->getHistograms();
  int channelIdx         = histograms->currentIndex();
  int inputBarIdx        = (channelIdx << 1);

  EditableMarksBar *editableMarksBar = m_marksBar[inputBarIdx];

  int min, max;

  // Clamp histogram
  const QVector<int> &values =
      histograms->getHistogramView(channelIdx)->values();
  ::getRange(values, min, max);

  QVector<int> &marks = editableMarksBar->marksBar()->values();
  if (min < max)
    marks[0] = min, marks[1] = max + 1;
  else
    marks[0] = 0, marks[1] = 256;

  editableMarksBar->updateFields();
  onParamsChanged();
  update();
}

//--------------------------------------------------------------

void AdjustLevelsPopup::autoAdjust() {
  if (!m_inputRas) return;

  Histograms *histograms = m_histogram->getHistograms();
  int channelIdx         = histograms->currentIndex();
  int inputBarIdx        = (channelIdx << 1);

  EditableMarksBar *editableMarksBar = m_marksBar[inputBarIdx];

  int min, max;

  // Clamp histogram
  const QVector<int> &values =
      histograms->getHistogramView(channelIdx)->values();
  if (channelIdx == 0) {
    int minR, maxR, minG, maxG, minB, maxB;

    ::getRange(histograms->getHistogramView(1)->values(), m_threshold, minR,
               maxR);
    ::getRange(histograms->getHistogramView(2)->values(), m_threshold, minG,
               maxG);
    ::getRange(histograms->getHistogramView(3)->values(), m_threshold, minB,
               maxB);

    min = std::min({minR, minG, minB});
    max = std::max({maxR, maxG, maxB});
  } else
    ::getRange(values, m_threshold, min, max);

  QVector<int> &marks = editableMarksBar->marksBar()->values();
  if (min < max)
    marks[0] = min, marks[1] = max + 1;
  else
    marks[0] = 0, marks[1] = 256;

  editableMarksBar->updateFields();
  onParamsChanged();
  update();
}

//**************************************************************************
//    TGBMScale Undo
//**************************************************************************

class AdjustLevelsUndo final : public TUndo {
  int m_in0[5], m_in1[5], m_out0[5], m_out1[5];
  int m_r, m_c;

  QString m_rasId;
  int m_rasSize;

public:
  AdjustLevelsUndo(int *in0, int *in1, int *out0, int *out1, int r, int c,
                   TRasterP ras);
  ~AdjustLevelsUndo();

  void undo() const override;
  void redo() const override;

  int getSize() const override { return sizeof(*this) + m_rasSize; }
};

//--------------------------------------------------------------

AdjustLevelsUndo::AdjustLevelsUndo(int *in0, int *in1, int *out0, int *out1,
                                   int r, int c, TRasterP ras)
    : m_r(r)
    , m_c(c)
    , m_rasSize(ras->getLx() * ras->getLy() * ras->getPixelSize()) {
  memcpy(m_in0, in0, sizeof(m_in0));
  memcpy(m_in1, in1, sizeof(m_in1));
  memcpy(m_out0, out0, sizeof(m_out0));
  memcpy(m_out1, out1, sizeof(m_out1));

  static int counter = 0;
  m_rasId            = QString("AdjustLevelsUndo") + QString::number(++counter);

  TImageCache::instance()->add(m_rasId, TRasterImageP(ras));
}

//--------------------------------------------------------------

AdjustLevelsUndo::~AdjustLevelsUndo() {
  TImageCache::instance()->remove(m_rasId);
}

//--------------------------------------------------------------

void AdjustLevelsUndo::undo() const {
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshCell cell   = xsheet->getCell(m_r, m_c);

  TRasterImageP rasImage = (TRasterImageP)cell.getImage(true);
  if (!rasImage) return;  //...Should never happen, though...

  rasImage->setRaster(
      ((TRasterImageP)TImageCache::instance()->get(m_rasId, true))
          ->getRaster()
          ->clone());

  TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
  assert(simpleLevel);
  simpleLevel->touchFrame(cell.getFrameId());
  simpleLevel->setDirtyFlag(true);
  IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());

  if (m_isLastInBlock)
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//--------------------------------------------------------------

void AdjustLevelsUndo::redo() const {
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshCell cell   = xsheet->getCell(m_r, m_c);

  TRasterImageP rasImage = (TRasterImageP)cell.getImage(true);
  if (!rasImage) return;

  TRasterP ras = rasImage->getRaster();
  if (!ras) return;

  TRop::rgbmAdjust(ras, ras, m_in0, m_in1, m_out0, m_out1);

  TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
  assert(simpleLevel);
  simpleLevel->touchFrame(cell.getFrameId());
  simpleLevel->setDirtyFlag(true);
  IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());

  if (m_isLastInBlock)
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//**************************************************************************
//    Apply stuff
//**************************************************************************

void AdjustLevelsPopup::apply() {
  // Retrieve parameters
  int in0[5], in1[5], out0[5], out1[5];
  getParameters(in0, in1, out0, out1);

  // Operate depending on the selection kind
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  if (cellSelection) {
    std::set<TRasterImage *>
        images;  // Multiple cells may yield the same image...

    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsheet      = TApp::instance()->getCurrentXsheet()->getXsheet();
    bool oneImageChanged = false;

    TUndoManager::manager()->beginBlock();
    {
      int c, r;
      for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
          const TXshCell &cell = xsheet->getCell(r, c);

          TRasterImageP rasImage = (TRasterImageP)cell.getImage(true);
          if (!rasImage) continue;

          if (images.find(rasImage.getPointer()) != images.end()) continue;

          TRasterP ras = rasImage->getRaster();
          if (!ras) continue;

          images.insert(rasImage.getPointer());
          oneImageChanged = true;

          TUndoManager::manager()->add(
              new AdjustLevelsUndo(in0, in1, out0, out1, r, c, ras->clone()));
          TRop::rgbmAdjust(ras, ras, in0, in1, out0, out1);

          TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
          assert(simpleLevel);
          simpleLevel->touchFrame(cell.getFrameId());
          simpleLevel->setDirtyFlag(true);

          IconGenerator::instance()->invalidate(simpleLevel, cell.getFrameId());
        }
      }
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

      for (auto const &fid : fids) {
        TRasterImageP rasImage =
            (TRasterImageP)simpleLevel->getFrame(fid, true);
        if (!rasImage) continue;

        TRasterP ras = rasImage->getRaster();
        if (!ras) continue;

        oneImageChanged = true;
        TRop::rgbmAdjust(ras, ras, in0, in1, out0, out1);

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

//**************************************************************************
//    Open Popup Command Handler instantiation
//**************************************************************************

OpenPopupCommandHandler<AdjustLevelsPopup> openAdjustLevelsPopup(
    MI_AdjustLevels);
