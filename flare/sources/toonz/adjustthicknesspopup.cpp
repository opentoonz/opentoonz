

#include "adjustthicknesspopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "filmstripselection.h"
#include "columnselection.h"

// TnzTools includes
#include "tools/strokeselection.h"
#include "tools/levelselection.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/doublefield.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"

// TnzCore includes
#include "tundo.h"
#include "tstroke.h"
#include "tstrokeutil.h"

// Qt includes
#include <QTimer>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <QScrollArea>
#include <QMainWindow>

// tcg includes
#include "tcg/tcg_numeric_ops.h"

// boost includes
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>

//**************************************************************************
//    Local namespace stuff
//**************************************************************************

namespace {

static const double dmax = (std::numeric_limits<double>::max)(), dmin = -dmax;

//--------------------------------------------------------------

struct TransformFunc {
  TVectorImage &m_vi;
  const double (&m_transform)[2];

  std::vector<int> m_changedStrokeIdxs;
  std::vector<TStroke *> m_changedStrokes;

  ~TransformFunc()  //! Recalculates m_vi's regions.
  {
    m_vi.notifyChangedStrokes(m_changedStrokeIdxs, m_changedStrokes);
  }

  void operator()(
      int s)  //! Transforms the stroke thickness, and marks the specified
              //! stroke as \a modified.
  {
    TStroke *stroke = m_vi.getStroke(s);

    if (stroke->getMaxThickness() > 0.0) {
      ::transform_thickness(*stroke, m_transform, 2);

      m_changedStrokeIdxs.push_back(s);
      m_changedStrokes.push_back(stroke);
    }
  }
};

//--------------------------------------------------------------

void transformThickness_image(const TVectorImageP &vi,
                              const double (&transform)[2]) {
  assert(vi);
  if (transform[0] == 0.0 &&
      transform[1] == 1.0)  // Bail out if transform is the identity
    return;                 //

  TransformFunc transformer = {*vi, transform};

  std::for_each(boost::counting_iterator<int>(0),
                boost::counting_iterator<int>(vi->getStrokeCount()),
                transformer);
}

//--------------------------------------------------------------

void transformThickness_strokes(const TVectorImageP &vi,
                                const double (&transform)[2],
                                const int strokesSelection[],
                                int strokesSelectionCount) {
  assert(vi);
  if (transform[0] == 0.0 &&
      transform[1] == 1.0)  // Bail out if transform is the identity
    return;                 //

  TransformFunc transformer = {*vi, transform};

  std::for_each(strokesSelection, strokesSelection + strokesSelectionCount,
                transformer);
}

//--------------------------------------------------------------

namespace {

struct StylesFilter {
  const TVectorImage &m_vi;
  const int *m_stylesStart, *m_stylesEnd;

  bool operator()(int strokeIdx) const {
    int strokeStyle = m_vi.getStroke(strokeIdx)->getStyle();
    return std::binary_search(m_stylesStart, m_stylesEnd, strokeStyle);
  }
};

}  // namespace

void transformThickness_styles(const TVectorImageP &vi,
                               const double (&transform)[2],
                               const int stylesSelection[],
                               int stylesSelectionCount) {
  assert(vi);
  if (transform[0] == 0.0 &&
      transform[1] == 1.0)  // Bail out if transform is the identity
    return;                 //

  TransformFunc transformer = {*vi, transform};
  StylesFilter filter       = {*vi, stylesSelection,
                         stylesSelection + stylesSelectionCount};

  boost::counting_iterator<int> sBegin(0), sEnd(vi->getStrokeCount());

  std::for_each(boost::make_filter_iterator(filter, sBegin, sEnd),
                boost::make_filter_iterator(filter, sEnd, sEnd), transformer);
}

//------------------------------------------------------------------------

TXsheet *xsheet() { return TApp::instance()->getCurrentXsheet()->getXsheet(); }

//------------------------------------------------------------------------

double frame() { return TApp::instance()->getCurrentFrame()->getFrame(); }

//------------------------------------------------------------------------

int column() { return TApp::instance()->getCurrentColumn()->getColumnIndex(); }

//------------------------------------------------------------------------

TFrameId levelFid() { return TApp::instance()->getCurrentFrame()->getFid(); }

//--------------------------------------------------------------

TXshSimpleLevel *simpleLevel() {
  return TApp::instance()->getCurrentLevel()->getSimpleLevel();
}

//--------------------------------------------------------------

std::pair<TXshSimpleLevel *, int> currentLevelIndex() {
  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();

  TXshSimpleLevel *sl = 0;
  TFrameId fid;

  if (frameHandle->getFrameType() == TFrameHandle::SceneFrame) {
    const TXshCell &cell = xsheet()->getCell(frame(), column());

    sl  = cell.getSimpleLevel();
    fid = cell.getFrameId();
  } else {
    sl  = simpleLevel();
    fid = levelFid();
  }

  return std::make_pair(sl, sl ? sl->fid2index(fid) : -1);
}

//--------------------------------------------------------------

double relativePosition(int start, int end, int pos) {
  return tcrop((start == end) ? 0.0 : double(pos - start) / double(end - start),
               0.0, 1.0);
}

}  // namespace

//**************************************************************************
//    Adjust Thickness Swatch
//**************************************************************************

class AdjustThicknessPopup::Swatch final : public PlaneViewer {
  TVectorImageP m_vi;

public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent) {
    setBgColor(TPixel32::White, TPixel32::White);
  }

  TVectorImageP image() const { return m_vi; }
  TVectorImageP &image() { return m_vi; }

  void paintGL() override {
    drawBackground();

    if (m_vi) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // Note GL_ONE instead of GL_SRC_ALPHA: it's needed since the input
      // image is supposedly premultiplied - and it works because the
      // viewer's background is opaque.
      // See tpixelutils.h's overPixT function for comparison.

      pushGLWorldCoordinates();
      draw(m_vi);
      popGLCoordinates();

      glDisable(GL_BLEND);
    }
  }
};

//**************************************************************************
//    FrameData  implementation
//**************************************************************************

AdjustThicknessPopup::FrameData::FrameData() : m_frameIdx(-1) {}

//--------------------------------------------------------------

AdjustThicknessPopup::FrameData::FrameData(const TXshSimpleLevelP &sl,
                                           int frameIdx)
    : m_sl(sl), m_frameIdx(frameIdx) {}

//--------------------------------------------------------------

AdjustThicknessPopup::FrameData AdjustThicknessPopup::FrameData::getCurrent() {
  const std::pair<TXshSimpleLevel *, int> &data = ::currentLevelIndex();

  FrameData fd;
  fd.m_sl = data.first, fd.m_frameIdx = data.second;

  return fd;
}

//--------------------------------------------------------------

bool AdjustThicknessPopup::FrameData::operator==(const FrameData &other) const {
  return (m_sl == other.m_sl) && (m_frameIdx == other.m_frameIdx);
}

//--------------------------------------------------------------

TVectorImageP AdjustThicknessPopup::FrameData::image() const {
  return m_sl ? TVectorImageP(m_sl->getFullsampledFrame(
                    m_sl->index2fid(m_frameIdx), false))
              : TVectorImageP();
}

//**************************************************************************
//    SelectionData  implementation
//**************************************************************************

AdjustThicknessPopup::SelectionData::SelectionData()
    : m_contentType(NONE), m_sl() {}

//--------------------------------------------------------------

AdjustThicknessPopup::SelectionData::SelectionData(const TSelection *sel)
    : m_contentType(NONE), m_sl() {
  struct Locals {
    SelectionData *m_this;

    void resetIfInvalid()  // Resets to empty if thickness adjustment is
    {                      // not applicable:
      if (!m_this->m_sl)   //   1. The level is not a VECTOR level
      {                    //   2. There is no selected frame
        assert(!*m_this);
        return;
      }

      if (m_this->m_sl->getType() != PLI_XSHLEVEL) {
        *m_this = SelectionData();
        return;
      }

      switch (m_this->m_framesType) {
      case ALL_FRAMES:
        if (m_this->m_sl->getFrameCount() <= 0) *m_this = SelectionData();
        break;
      case SELECTED_FRAMES:
        // Since fid2index may return negative indexes, cut them out
        m_this->m_frameIdxs.erase(m_this->m_frameIdxs.begin(),
                                  m_this->m_frameIdxs.lower_bound(0));

        // Also cut indexes greater than m_sl's frames count
        m_this->m_frameIdxs.erase(
            m_this->m_frameIdxs.lower_bound(m_this->m_sl->getFrameCount()),
            m_this->m_frameIdxs.end());

        // Reset to empty in case no frame was selected
        if (m_this->m_frameIdxs.empty()) *m_this = SelectionData();

        // NOTE: This may notably happen whenever non-empty level cells refer to
        // frames
        //       not present in the level.
        break;
      }
    }

    void initialize(const TFilmstripSelection &selection) {
      TXshSimpleLevel *sl = simpleLevel();

      if (sl && sl->getType() == PLI_XSHLEVEL) {
        m_this->m_contentType = IMAGE;
        m_this->m_framesType  = SELECTED_FRAMES;
        m_this->m_sl          = sl;

        const std::set<TFrameId> &fids = selection.getSelectedFids();

        std::set<int> s;
        for (auto const &e : fids) {
          s.insert(m_this->m_sl->fid2index(e));
        }
        m_this->m_frameIdxs = std::move(s);

        resetIfInvalid();
      }
    }

    void initialize(int r0, int c0, int r1, int c1) {
      assert(!m_this->m_sl);

      const TXsheet *xsh = xsheet();
      for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
          const TXshCell &cell   = xsh->getCell(r, c);
          TXshSimpleLevel *newSl = cell.getSimpleLevel();

          if (newSl && newSl->getType() == PLI_XSHLEVEL) {
            if (m_this->m_sl) {
              if (m_this->m_sl != newSl)  // Only a single vector level
              {                           // is allowed in the selection
                *m_this = SelectionData();
                return;
              }
            } else {
              m_this->m_contentType = IMAGE;
              m_this->m_framesType  = SELECTED_FRAMES;
              m_this->m_sl          = newSl;
            }

            int idx = m_this->m_sl->fid2index(cell.getFrameId());
            if (idx >= 0) m_this->m_frameIdxs.insert(idx);
          }
        }
      }

      resetIfInvalid();
    }

    void initialize(const TCellSelection &selection) {
      int r0, c0, r1, c1;
      selection.getSelectedCells(r0, c0, r1, c1);

      initialize(r0, c0, r1, c1);
    }

    void initialize(const TColumnSelection &selection) {
      if (selection.getIndices().size() != 1)  // Bail out if we don't have a
        return;                                // specific column

      int c = *selection.getIndices().begin();

      TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
      TXshColumn *col = xsh->getColumn(c);

      assert(col);

      // Retrieve the first level in the column
      if (TXshCellColumn *cCol = dynamic_cast<TXshCellColumn *>(col)) {
        int r0, r1;
        cCol->getRange(r0, r1);

        initialize(r0, c, r1, c);
      }
    }

    void initialize(const StrokeSelection &selection) {
      const std::pair<TXshSimpleLevel *, int> &pair = currentLevelIndex();

      TXshSimpleLevel *sl = pair.first;

      if (sl && sl->getType() == PLI_XSHLEVEL) {
        assert(selection.getImage() ==
               sl->getFullsampledFrame(sl->index2fid(pair.second), false));

        m_this->m_contentType = STROKES;
        m_this->m_framesType  = SELECTED_FRAMES;
        m_this->m_sl          = sl;

        m_this->m_frameIdxs.insert(pair.second);
        resetIfInvalid();

        if (*m_this) {
          const std::set<int> &strokeIdxs = selection.getSelection();

          m_this->m_idxs =
              std::vector<int>(strokeIdxs.begin(), strokeIdxs.end());

          // Reset to empty in case no stroke was selected
          if (m_this->m_idxs.empty()) *m_this = SelectionData();
        }
      }
    }

    void initialize(const LevelSelection &selection) {
      if (TXshSimpleLevel *sl = simpleLevel()) {
        assert(sl->getType() == PLI_XSHLEVEL);
        m_this->m_sl = sl;

        // Discriminate styles selection modes
        switch (selection.filter()) {
        case LevelSelection::WHOLE:
          m_this->m_contentType = IMAGE;
          break;

        case LevelSelection::SELECTED_STYLES:
          m_this->m_contentType = STYLES;
          m_this->m_idxs        = std::vector<int>(selection.styles().begin(),
                                            selection.styles().end());

          // Reset to empty in case no style was selected
          if (m_this->m_idxs.empty()) {
            *m_this = SelectionData();
            return;
          }

          break;
        case LevelSelection::BOUNDARY_STROKES:
          m_this->m_contentType = BOUNDARIES;
          break;
        default:
          break;
        }

        // Discriminate frames selection modes
        switch (selection.framesMode()) {
        case LevelSelection::FRAMES_ALL:
          m_this->m_framesType = ALL_FRAMES;
          break;

        case LevelSelection::FRAMES_SELECTED: {
          m_this->m_framesType = SELECTED_FRAMES;

          const std::set<TFrameId> &fids = TTool::getSelectedFrames();

          std::set<int> s;
          for (auto const &e : fids) {
            s.insert(m_this->m_sl->fid2index(e));
          }
          m_this->m_frameIdxs = std::move(s);
          break;
        }
        default:
          break;
        }

        resetIfInvalid();
      }
    }

  } locals = {this};

  if (sel && !sel->isEmpty()) {
    if (const TCellSelection *cSel = dynamic_cast<const TCellSelection *>(sel))
      locals.initialize(*cSel);
    else if (const TFilmstripSelection *fSel =
                 dynamic_cast<const TFilmstripSelection *>(sel))
      locals.initialize(*fSel);
    else if (const TColumnSelection *cSel =
                 dynamic_cast<const TColumnSelection *>(sel))
      locals.initialize(*cSel);
    else if (const StrokeSelection *sSel =
                 dynamic_cast<const StrokeSelection *>(sel))
      locals.initialize(*sSel);
    else if (const LevelSelection *vlSel =
                 dynamic_cast<const LevelSelection *>(sel))
      locals.initialize(*vlSel);
  }
}

//--------------------------------------------------------------

AdjustThicknessPopup::SelectionData::SelectionData(const FrameData &fd)
    : m_contentType(IMAGE)
    , m_framesType(SELECTED_FRAMES)
    , m_sl(fd.m_sl)
    , m_frameIdxs(&fd.m_frameIdx, &fd.m_frameIdx + 1) {
  if (!m_sl || m_sl->getType() != PLI_XSHLEVEL ||
      m_sl->index2fid(*m_frameIdxs.begin()) < 0)
    *this = SelectionData();
}

//--------------------------------------------------------------

AdjustThicknessPopup::SelectionData::operator bool() const {
  return (m_contentType != NONE);
}

//--------------------------------------------------------------

void AdjustThicknessPopup::SelectionData::getRange(int &startIdx,
                                                   int &endIdx) const {
  if (m_contentType == NONE) return;

  switch (m_framesType) {
  case ALL_FRAMES:
    assert(m_sl);

    startIdx = 0;
    endIdx   = m_sl->getFrameCount() - 1;
    break;

  case SELECTED_FRAMES:
    assert(!m_frameIdxs.empty());

    startIdx = *m_frameIdxs.begin();
    endIdx   = *--m_frameIdxs.end();
    break;
  }
}

//**************************************************************************
//    Adjust Thickness  operation
//**************************************************************************

namespace {

typedef AdjustThicknessPopup::SelectionData SelectionData;

TVectorImageP processFrame(const SelectionData &selData, int slFrameIndex,
                           const double (&fromTransform)[2],
                           const double (&toTransform)[2]) {
  struct locals {
    static void makeTransform(double (&transform)[2],
                              const double (&fromTransform)[2],
                              const double (&toTransform)[2], int startIdx,
                              int endIdx, int curIdx) {
      double relPos = ::relativePosition(startIdx, endIdx, curIdx);

      transform[0] =
          tcg::numeric_ops::lerp(fromTransform[0], toTransform[0], relPos);
      transform[1] =
          tcg::numeric_ops::lerp(fromTransform[1], toTransform[1], relPos);
    }

  };  // locals

  if (!selData || !selData.m_sl) return TVectorImageP();

  // Retrieve input image
  if (TVectorImageP viIn = selData.m_sl->getFullsampledFrame(
          selData.m_sl->index2fid(slFrameIndex), false)) {
    // Retrieve operations range
    int startIdx, endIdx;
    selData.getRange(startIdx, endIdx);

    if (startIdx <= endIdx) {
      // Allocate a conformant output, if necessary
      TVectorImageP viOut = viIn->clone();

      // Perform the operation preview
      switch (selData.m_contentType) {
      case SelectionData::IMAGE: {
        double transform[2];
        locals::makeTransform(transform, fromTransform, toTransform, startIdx,
                              endIdx, slFrameIndex);

        ::transformThickness_image(viOut, transform);
        break;
      }

      case SelectionData::STYLES: {
        double transform[2];
        locals::makeTransform(transform, fromTransform, toTransform, startIdx,
                              endIdx, slFrameIndex);

        ::transformThickness_styles(viOut, transform, selData.m_idxs.data(),
                                    selData.m_idxs.size());
        break;
      }

      case SelectionData::BOUNDARIES: {
        double transform[2];
        locals::makeTransform(transform, fromTransform, toTransform, startIdx,
                              endIdx, slFrameIndex);

        std::vector<int> strokes = getBoundaryStrokes(*viOut);

        ::transformThickness_strokes(viOut, transform, strokes.data(),
                                     strokes.size());
        break;
      }

      case SelectionData::STROKES:
        ::transformThickness_strokes(
            viOut, fromTransform, selData.m_idxs.data(), selData.m_idxs.size());
        break;

      default:
        assert(false);
      }

      return viOut;
    }
  }

  return TVectorImageP();
}

}  // namespace

//**************************************************************************
//    AdjustThicknessPopup implementation
//**************************************************************************

AdjustThicknessPopup::AdjustThicknessPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "AdjustThickness")
    , m_validPreview(false) {
  setWindowTitle(tr("Adjust Thickness"));
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

  QGridLayout *topLayout = new QGridLayout(topWidget);
  topWidget->setLayout(topLayout);

  //-------------------------- Options ----------------------------

  int row = 0;

  topLayout->setColumnStretch(0, 1);

  topLayout->addWidget(new QLabel(tr("Mode:")), row, 1, Qt::AlignRight);

  m_thicknessMode = new QComboBox;
  topLayout->addWidget(m_thicknessMode, row++, 2, Qt::AlignLeft);

  m_thicknessMode->addItems(QStringList() << tr("Scale Thickness")
                                          << tr("Add Thickness")
                                          << tr("Constant Thickness"));

  topLayout->addWidget(new QLabel(tr("Start:")), row, 1, Qt::AlignRight);

  QHBoxLayout *paramsLayout = new QHBoxLayout;
  topLayout->addLayout(paramsLayout, row++, 2, Qt::AlignLeft);
  {
    m_fromScale        = new DVGui::MeasuredDoubleField;
    m_fromDisplacement = new DVGui::MeasuredDoubleField;
    m_toScale          = new DVGui::MeasuredDoubleField;
    m_toDisplacement   = new DVGui::MeasuredDoubleField;

    paramsLayout->addWidget(m_fromScale);
    paramsLayout->addWidget(m_fromDisplacement);

    paramsLayout->addWidget(new QLabel(tr("End:")));
    paramsLayout->addWidget(m_toScale);
    paramsLayout->addWidget(m_toDisplacement);

    paramsLayout->addStretch(1);

    m_fromScale->setMeasure("percentage");
    m_toScale->setMeasure("percentage");

    m_fromScale->setRange(0, dmax);
    m_fromDisplacement->setRange(dmin, dmax);
    m_toScale->setRange(0, dmax);
    m_toDisplacement->setRange(dmin, dmax);

    m_fromScale->setValue(1.0);
    m_toScale->setValue(1.0);

    m_fromDisplacement->setValue(0.0);
    m_toDisplacement->setValue(0.0);
  }

  topLayout->setColumnStretch(3, 1);
  topLayout->setRowStretch(2, 1);  // Needed to justify at top

  //------------------------- View Widget -------------------------

  // NOTE: It's IMPORTANT that parent widget is supplied. It's somewhat
  // used by QSplitter to decide the initial widget sizes...

  m_viewer = new Swatch(splitter);
  m_viewer->setMinimumHeight(150);
  m_viewer->setFocusPolicy(Qt::WheelFocus);
  splitter->addWidget(m_viewer);

  m_okBtn = new QPushButton(tr("Apply"));
  addButtonBarWidget(m_okBtn);

  // Establish connections
  bool ret = true;
  ret      = connect(m_thicknessMode, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onModeChanged())) &&
        ret;
  ret = connect(m_fromScale, SIGNAL(valueChanged(bool)), this,
                SLOT(onParamsChanged())) &&
        ret;
  ret = connect(m_fromDisplacement, SIGNAL(valueChanged(bool)), this,
                SLOT(onParamsChanged())) &&
        ret;
  ret = connect(m_toScale, SIGNAL(valueChanged(bool)), this,
                SLOT(onParamsChanged())) &&
        ret;
  ret = connect(m_toDisplacement, SIGNAL(valueChanged(bool)), this,
                SLOT(onParamsChanged())) &&
        ret;
  ret = connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply())) && ret;
  assert(ret);

  m_viewer->resize(0, 350);
  resize(600, 500);
}

//--------------------------------------------------------------

void AdjustThicknessPopup::showEvent(QShowEvent *se) {
  TApp *app = TApp::instance();

  TSelectionHandle *selectionHandle = app->getCurrentSelection();
  TXsheetHandle *xsheetHandle       = app->getCurrentXsheet();
  TFrameHandle *frameHandle         = app->getCurrentFrame();
  TColumnHandle *columnHandle       = app->getCurrentColumn();

  bool ret = true;
  ret = connect(selectionHandle, SIGNAL(selectionChanged(TSelection *)), this,
                SLOT(onSelectionChanged())) &&
        ret;
  ret = connect(selectionHandle,
                SIGNAL(selectionSwitched(TSelection *, TSelection *)), this,
                SLOT(onSelectionChanged())) &&
        ret;
  ret = connect(xsheetHandle, SIGNAL(xsheetChanged()), this,
                SLOT(onXsheetChanged())) &&
        ret;
  ret = connect(xsheetHandle, SIGNAL(xsheetSwitched()), this,
                SLOT(onXsheetChanged())) &&
        ret;
  ret = connect(frameHandle, SIGNAL(frameSwitched()), this,
                SLOT(onFrameChanged())) &&
        ret;
  ret = connect(columnHandle, SIGNAL(columnIndexSwitched()), this,
                SLOT(onFrameChanged())) &&
        ret;
  assert(ret);

  onModeChanged();
  onXsheetChanged();
}

//--------------------------------------------------------------

void AdjustThicknessPopup::hideEvent(QHideEvent *he) {
  Dialog::hideEvent(he);

  TApp *app = TApp::instance();
  app->getCurrentSelection()->disconnect(this);
  app->getCurrentXsheet()->disconnect(this);
  app->getCurrentFrame()->disconnect(this);
  app->getCurrentColumn()->disconnect(this);

  // Empty cached data
  m_selectionData    = SelectionData();
  m_currentFrameData = FrameData();

  m_previewedFrameData = FrameData();
  m_viewer->image()    = TVectorImageP();  // This in particular
}

//--------------------------------------------------------------

void AdjustThicknessPopup::updateSelectionData() {
  m_selectionData =
      SelectionData(TApp::instance()->getCurrentSelection()->getSelection());
}

//--------------------------------------------------------------

void AdjustThicknessPopup::updateCurrentFrameData() {
  m_currentFrameData = FrameData::getCurrent();
}

//--------------------------------------------------------------

void AdjustThicknessPopup::updateSelectionGui() {
  m_okBtn->setEnabled(bool(m_selectionData) ||
                      bool(SelectionData(m_currentFrameData)));
}

//--------------------------------------------------------------

void AdjustThicknessPopup::onModeChanged() {
  bool scale = (m_thicknessMode->currentIndex() == 0);

  m_fromScale->setVisible(scale);
  m_toScale->setVisible(scale);

  m_fromDisplacement->setVisible(!scale);
  m_toDisplacement->setVisible(!scale);

  schedulePreviewUpdate();
}

//--------------------------------------------------------------

void AdjustThicknessPopup::onXsheetChanged() {
  updateSelectionData();
  updateCurrentFrameData();

  updateSelectionGui();
  schedulePreviewUpdate();
}

//--------------------------------------------------------------

void AdjustThicknessPopup::onSelectionChanged() {
  updateSelectionData();

  updateSelectionGui();
  schedulePreviewUpdate();
}

//--------------------------------------------------------------

void AdjustThicknessPopup::onFrameChanged() {
  updateCurrentFrameData();

  if (m_currentFrameData != m_previewedFrameData) {
    updateSelectionGui();
    schedulePreviewUpdate();
  }
}

//--------------------------------------------------------------

void AdjustThicknessPopup::onParamsChanged() { schedulePreviewUpdate(); }

//--------------------------------------------------------------

void AdjustThicknessPopup::schedulePreviewUpdate() {
  m_validPreview = false;
  QTimer::singleShot(0, this, SLOT(updatePreview()));
}

//--------------------------------------------------------------

void AdjustThicknessPopup::getTransformParameters(double (&fromTransform)[2],
                                                  double (&toTransform)[2]) {
  enum { SCALE, ADD, CONSTANT };

  switch (m_thicknessMode->currentIndex()) {
  case SCALE:
    fromTransform[0] = 0.0;
    fromTransform[1] = m_fromScale->getValue();

    toTransform[0] = 0.0;
    toTransform[1] = m_toScale->getValue();
    break;

  case ADD:
    fromTransform[0] = m_fromDisplacement->getValue() * Stage::inch;
    fromTransform[1] = 1.0;

    toTransform[0] = m_toDisplacement->getValue() * Stage::inch;
    toTransform[1] = 1.0;
    break;

  case CONSTANT:
    fromTransform[0] = m_fromDisplacement->getValue() * Stage::inch;
    fromTransform[1] = 0.0;

    toTransform[0] = m_toDisplacement->getValue() * Stage::inch;
    toTransform[1] = 0.0;
    break;
  }
}

//--------------------------------------------------------------

void AdjustThicknessPopup::updatePreview() {
  if (!m_validPreview) {
    m_validPreview = true;

    // Re-process preview source
    double fromTransform[2], toTransform[2];
    getTransformParameters(fromTransform, toTransform);

    if (m_selectionData) {
      m_previewedFrameData =
          FrameData(m_selectionData.m_sl, m_currentFrameData.m_frameIdx);
      m_viewer->image() =
          ::processFrame(m_selectionData, m_currentFrameData.m_frameIdx,
                         fromTransform, toTransform);
    } else {
      m_previewedFrameData = m_currentFrameData;
      m_viewer->image() =
          ::processFrame(m_currentFrameData, m_currentFrameData.m_frameIdx,
                         fromTransform, toTransform);
    }

    m_viewer->update();
  }
}

//**************************************************************************
//    AdjustThickness Undo
//**************************************************************************

namespace {

class AdjustThicknessUndo final : public TUndo {
public:
  AdjustThicknessUndo(const SelectionData &selData, double (&fromTransform)[2],
                      double (&toTransform)[2]);

  void redo() const override;
  void undo() const override;

  int getSize() const override {
    return (10 << 20);
  }  // 10 MB, flat - ie, at max 10 of these for a standard 100MB
     // undo cache size.
private:
  struct ImageBackup {
    TFrameId m_fid;
    TVectorImageP m_vi;

  public:
    ImageBackup(const TFrameId &fid, const TVectorImageP &vi)
        : m_fid(fid), m_vi(vi) {}
  };

private:
  SelectionData m_selData;  //!< Selection to be processed.

  double m_fromTransform[2],  //!< Thickness transform start parameters.
      m_toTransform[2];       //!< Thickness transform end parameters.

  mutable std::vector<ImageBackup> m_originalImages;  //!< Original images.
};

//==============================================================

AdjustThicknessUndo::AdjustThicknessUndo(const SelectionData &selData,
                                         double (&fromTransform)[2],
                                         double (&toTransform)[2])
    : m_selData(selData) {
  std::copy(fromTransform, fromTransform + 2, m_fromTransform);
  std::copy(toTransform, toTransform + 2, m_toTransform);

  assert(m_selData && m_selData.m_sl);
}

//--------------------------------------------------------------

void AdjustThicknessUndo::redo() const {
  auto const processFrame = [this](int frameIdx) {
    TXshSimpleLevel *sl = m_selData.m_sl.getPointer();
    assert(sl);

    const TFrameId &fid = sl->index2fid(frameIdx);

    // Backup input frame
    TVectorImageP viIn = sl->getFullsampledFrame(fid, false);
    if (!viIn) return;

    m_originalImages.push_back(ImageBackup(fid, viIn));

    // Process required frame
    TVectorImageP viOut = ::processFrame(
        m_selData, frameIdx, m_fromTransform, m_toTransform);

    sl->setFrame(fid, viOut);

    // Ensure the level data is invalidated suitably
    sl->setDirtyFlag(true);
    IconGenerator::instance()->invalidate(sl, fid);
  };

  m_originalImages.clear();

  // Iterate selected frames
  switch (m_selData.m_framesType) {
  case SelectionData::ALL_FRAMES:
    std::for_each(
        boost::make_counting_iterator(0),
        boost::make_counting_iterator(m_selData.m_sl->getFrameCount()),
        processFrame);
    break;
  case SelectionData::SELECTED_FRAMES:
    std::for_each(m_selData.m_frameIdxs.begin(), m_selData.m_frameIdxs.end(),
                  processFrame);
    break;
  }
}

//--------------------------------------------------------------

void AdjustThicknessUndo::undo() const {
  // Copy the backup images back to the level
  TXshSimpleLevel *sl = m_selData.m_sl.getPointer();

  std::vector<ImageBackup>::const_iterator bt, bEnd = m_originalImages.end();
  for (bt = m_originalImages.begin(); bt != bEnd; ++bt) {
    sl->setFrame(bt->m_fid, bt->m_vi.getPointer());

    sl->setDirtyFlag(true);
    IconGenerator::instance()->invalidate(sl, bt->m_fid);
  }
}

}  // namespace

//**************************************************************************
//    Apply stuff
//**************************************************************************

void AdjustThicknessPopup::apply() {
  if (!m_selectionData) {
    DVGui::error(QObject::tr("The current selection is invalid."));
    return;
  }

  // Retrieve parameters
  double fromTransform[2], toTransform[2];
  getTransformParameters(fromTransform, toTransform);

  std::unique_ptr<TUndo> undo(
      new AdjustThicknessUndo(m_selectionData, fromTransform, toTransform));

  undo->redo();

  TUndoManager::manager()->add(undo.release());

  close();
}

//**************************************************************************
//    Open Popup Command Handler instantiation
//**************************************************************************

OpenPopupCommandHandler<AdjustThicknessPopup> openAdjustThicknessPopup(
    MI_AdjustThickness);
