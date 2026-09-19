

#include "canvassizepopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/levelproperties.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"
#include "tools/cursors.h"
#include "tools/toolutils.h"

// TnzBase includes
#include "tunit.h"

// TnzCore includes
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "trop.h"
#include "tundo.h"
#include "timagecache.h"
#include "tgl.h"
#include "tcurveutil.h"
#include "tcurves.h"

// Qt includes
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStyleOption>
#include <QPainter>
#include <QButtonGroup>
#include <QApplication>
#include <QMainWindow>
#include <QHideEvent>
#include <algorithm>

namespace {

class ResizeCanvasUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  TFrameId m_fid;
  std::string m_oldImageId, m_newImageId;
  TDimension m_oldDim, m_newDim;
  static int m_idCount;
  int m_undoSize;

public:
  ResizeCanvasUndo(const TXshSimpleLevelP &level, const TFrameId &fid,
                   const TImageP &oldImage, const TImageP &newImage,
                   const TDimension &oldDim, const TDimension &newDim)
      : TUndo()
      , m_level(level)
      , m_fid(fid)
      , m_oldDim(oldDim)
      , m_newDim(newDim) {
    m_oldImageId = "ResizeCanvasUndo_oldImage_" + std::to_string(m_idCount);
    m_newImageId = "ResizeCanvasUnd_newImage_" + std::to_string(m_idCount++);

    TImageCache::instance()->add(m_oldImageId, oldImage);
    TImageCache::instance()->add(m_newImageId, newImage);

    TRasterP oldRaster, newRaster;
    TToonzImageP ti(oldImage);
    TRasterImageP ri(oldImage);
    if (ti) {
      oldRaster = ti->getRaster();
      ti        = TToonzImageP(newImage);
      newRaster = ti->getRaster();
    } else if (ri) {
      oldRaster = ri->getRaster();
      ri        = TRasterImageP(newImage);
      newRaster = ri->getRaster();
    } else
      assert(0);
    int oldSize =
        oldRaster->getLx() * oldRaster->getLy() * oldRaster->getPixelSize();
    int newSize =
        newRaster->getLx() * newRaster->getLy() * newRaster->getPixelSize();
    m_undoSize = oldSize + newSize;
  }

  ~ResizeCanvasUndo() {
    TImageCache::instance()->remove(m_oldImageId);
    TImageCache::instance()->remove(m_newImageId);
  }

  void undo() const override {
    TImageP img = TImageCache::instance()->get(m_oldImageId, true);
    m_level->setFrame(m_fid, img);
    IconGenerator::instance()->invalidate(m_level.getPointer(), m_fid);
    m_level->touchFrame(m_fid);
    if (m_level->getFirstFid() == m_fid) {
      m_level->getProperties()->setImageRes(m_oldDim);
      IconGenerator::instance()->invalidateSceneIcon();
      TApp::instance()->getCurrentLevel()->notifyCanvasSizeChange();
    }
  }

  void redo() const override {
    TImageP img = TImageCache::instance()->get(m_newImageId, true);
    m_level->setFrame(m_fid, img);
    IconGenerator::instance()->invalidate(m_level.getPointer(), m_fid);
    m_level->touchFrame(m_fid);
    std::vector<TFrameId> fids;
    m_level->getFids(fids);
    if (fids.back() == m_fid) {
      m_level->getProperties()->setImageRes(m_newDim);
      IconGenerator::instance()->invalidateSceneIcon();
      TApp::instance()->getCurrentLevel()->notifyCanvasSizeChange();
    }
  }

  int getSize() const override { return m_undoSize; }
};

int ResizeCanvasUndo::m_idCount = 0;

//-----------------------------------------------------------------------------

class CanvasCameraUndo final : public TUndo {
  TDimensionD m_oldSize, m_newSize;
  TDimension m_oldRes, m_newRes;

  static void apply(const TDimensionD &size, const TDimension &res) {
    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    if (!camera) return;
    camera->setSize(size);
    camera->setRes(res);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

public:
  CanvasCameraUndo(const TDimensionD &oldSize, const TDimension &oldRes,
                   const TDimensionD &newSize, const TDimension &newRes)
      : m_oldSize(oldSize)
      , m_newSize(newSize)
      , m_oldRes(oldRes)
      , m_newRes(newRes) {}

  void undo() const override { apply(m_oldSize, m_oldRes); }
  void redo() const override { apply(m_newSize, m_newRes); }
  int getSize() const override { return sizeof(*this); }
};

//-----------------------------------------------------------------------------

void applyCameraFromLevel(TXshSimpleLevel *sl) {
  if (!sl) return;
  TPointD dpi    = sl->getDpi();
  TDimension res = sl->getResolution();
  if (res.lx <= 0 || res.ly <= 0 || dpi.x <= 0 || dpi.y <= 0) return;

  TDimensionD size(res.lx / dpi.x, res.ly / dpi.y);
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  if (!camera) return;
  TDimensionD oldSize = camera->getSize();
  TDimension oldRes   = camera->getRes();
  if (oldRes == res && areAlmostEqual(oldSize.lx, size.lx) &&
      areAlmostEqual(oldSize.ly, size.ly))
    return;

  camera->setSize(size);
  camera->setRes(res);
  TUndoManager::manager()->add(
      new CanvasCameraUndo(oldSize, oldRes, size, res));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

double getMeasuredLength(int pixelLength, TMeasure *measure, double dpi,
                         const QString &unit) {
  if (unit == "pixel") return pixelLength;
  double inchValue     = (double)pixelLength / dpi;
  double measuredValue = measure->getCurrentUnit()->convertTo(inchValue);
  return unit == "field" ? measuredValue * 2 : measuredValue;
}

//-----------------------------------------------------------------------------

int getPixelLength(double measuredLength, TMeasure *measure, double dpi,
                   const QString &unit) {
  if (unit == "pixel") return tround(measuredLength);
  measuredLength   = unit == "field" ? measuredLength * 0.5 : measuredLength;
  double inchValue = measure->getCurrentUnit()->convertFrom(measuredLength);
  return tround(inchValue * dpi);
}

//-----------------------------------------------------------------------------

}  // namespace

bool isRasterCanvasLevel(TXshSimpleLevel *sl) {
  if (!sl) return false;
  int type = sl->getType();
  return type == TZP_XSHLEVEL || type == OVL_XSHLEVEL || type == TZI_XSHLEVEL;
}

void updateCanvasSizeCommandEnabled() {
  TApp *app = TApp::instance();
  if (!app || !app->getCurrentLevel()) {
    CommandManager::instance()->enable(MI_CanvasSize, false);
    return;
  }
  CommandManager::instance()->enable(
      MI_CanvasSize, isRasterCanvasLevel(app->getCurrentLevel()->getSimpleLevel()));
}

namespace {

//-----------------------------------------------------------------------------

TRectD worldRectFromDim(const TDimension &dim) {
  if (dim.lx < 1 || dim.ly < 1) return TRectD();
  return TRectD(0.0, 0.0, (double)dim.lx, (double)dim.ly) -
         TPointD(0.5 * dim.lx, 0.5 * dim.ly);
}

//-----------------------------------------------------------------------------

TRectD snapRectToPixelGrid(const TRectD &rect, const TRectD &grid) {
  TRectD out;
  out.x0 = grid.x0 + tround(rect.x0 - grid.x0);
  out.y0 = grid.y0 + tround(rect.y0 - grid.y0);
  out.x1 = grid.x0 + tround(rect.x1 - grid.x0);
  out.y1 = grid.y0 + tround(rect.y1 - grid.y0);
  if (out.x1 < out.x0 + 1.0) out.x1 = out.x0 + 1.0;
  if (out.y1 < out.y0 + 1.0) out.y1 = out.y0 + 1.0;
  return out;
}

//-----------------------------------------------------------------------------

TRectD proposedRectFromPeg(const TRectD &oldR, int newLx, int newLy,
                           PeggingPositions peg) {
  newLx = std::max(1, newLx);
  newLy = std::max(1, newLy);
  TRectD r;

  if (peg == e00 || peg == e10 || peg == e20) {
    r.x0 = oldR.x0;
    r.x1 = oldR.x0 + newLx;
  } else if (peg == e02 || peg == e12 || peg == e22) {
    r.x1 = oldR.x1;
    r.x0 = oldR.x1 - newLx;
  } else {
    double cx = 0.5 * (oldR.x0 + oldR.x1);
    r.x0      = cx - 0.5 * newLx;
    r.x1      = cx + 0.5 * newLx;
  }

  if (peg == e20 || peg == e21 || peg == e22) {
    r.y0 = oldR.y0;
    r.y1 = oldR.y0 + newLy;
  } else if (peg == e00 || peg == e01 || peg == e02) {
    r.y1 = oldR.y1;
    r.y0 = oldR.y1 - newLy;
  } else {
    double cy = 0.5 * (oldR.y0 + oldR.y1);
    r.y0      = cy - 0.5 * newLy;
    r.y1      = cy + 0.5 * newLy;
  }
  return r;
}

//-----------------------------------------------------------------------------

TDimension dimFromRect(const TRectD &rect) {
  return TDimension(std::max(1, tround(rect.x1 - rect.x0)),
                    std::max(1, tround(rect.y1 - rect.y0)));
}

//-----------------------------------------------------------------------------

TRectD rectUnion(const TRectD &a, const TRectD &b) {
  if (a.isEmpty()) return b;
  if (b.isEmpty()) return a;
  return TRectD(std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1),
                std::max(a.y1, b.y1));
}

//-----------------------------------------------------------------------------

bool isNavigationToolName(const QString &name) {
  return name == T_HandView || name == T_ZoomView || name == T_RotateView;
}
}  // namespace
//=============================================================================
// PeggingWidget
//-----------------------------------------------------------------------------

PeggingWidget::PeggingWidget(QWidget *parent)
    : QWidget(parent), m_pegging(e11), m_cutLx(false), m_cutLy(false) {
  setObjectName("PeggingWidget");
  setFixedSize(QSize(94, 94));

  m_topPix      = QPixmap(":Resources/pegging_top_arrow.png");
  m_topRightPix = QPixmap(":Resources/pegging_topright_arrow.png");

  QGridLayout *gridLayout = new QGridLayout(this);
  gridLayout->setSpacing(1);
  gridLayout->setContentsMargins(1, 1, 1, 1);

  m_buttonGroup = new QButtonGroup();
  m_buttonGroup->setExclusive(true);

  // second buttons line
  createButton(&m_00, e00);
  createButton(&m_01, e01);
  createButton(&m_02, e02);
  gridLayout->addWidget(m_00, 0, 0);
  gridLayout->addWidget(m_01, 0, 1);
  gridLayout->addWidget(m_02, 0, 2);

  bool ret = connect(m_00, SIGNAL(released()), this, SLOT(on00()));
  ret      = ret && connect(m_01, SIGNAL(released()), this, SLOT(on01()));
  ret      = ret && connect(m_02, SIGNAL(released()), this, SLOT(on02()));

  // second buttons line
  createButton(&m_10, e10);
  createButton(&m_11, e11);
  createButton(&m_12, e12);
  gridLayout->addWidget(m_10, 1, 0);
  gridLayout->addWidget(m_11, 1, 1);
  gridLayout->addWidget(m_12, 1, 2);
  ret = ret && connect(m_10, SIGNAL(released()), this, SLOT(on10()));
  ret = ret && connect(m_11, SIGNAL(released()), this, SLOT(on11()));
  ret = ret && connect(m_12, SIGNAL(released()), this, SLOT(on12()));

  // third buttons line
  createButton(&m_20, e20);
  createButton(&m_21, e21);
  createButton(&m_22, e22);
  gridLayout->addWidget(m_20, 2, 0);
  gridLayout->addWidget(m_21, 2, 1);
  gridLayout->addWidget(m_22, 2, 2);
  ret = ret && connect(m_20, SIGNAL(released()), this, SLOT(on20()));
  ret = ret && connect(m_21, SIGNAL(released()), this, SLOT(on21()));
  ret = ret && connect(m_22, SIGNAL(released()), this, SLOT(on22()));
}

//-----------------------------------------------------------------------------

void PeggingWidget::resetWidget() {
  m_11->setChecked(true);
  on11();
}

//-----------------------------------------------------------------------------

void PeggingWidget::paintEvent(QPaintEvent *) {
  QStyleOption opt;
  opt.init(this);
  QPainter p(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

//-----------------------------------------------------------------------------

void PeggingWidget::createButton(QPushButton **button,
                                 PeggingPositions position) {
  *button = new QPushButton(this);
  (*button)->setObjectName("PeggingButton");
  (*button)->setCheckable(true);
  (*button)->setFixedSize(30, 30);
  m_buttonGroup->addButton(*button);
  if (position == e11) {
    (*button)->setChecked(true);
    return;
  }
  QPixmap pix(1, 1);
  switch (position) {
  case e00:
    pix = m_topRightPix.transformed(QTransform().rotate(-90), Qt::SmoothTransformation);
    break;
  case e01:
    pix = m_topPix;
    break;
  case e02:
    pix = m_topRightPix;
    break;
  case e10:
    pix = m_topPix.transformed(QTransform().rotate(-90), Qt::SmoothTransformation);
    break;
  case e12:
    pix = m_topPix.transformed(QTransform().rotate(90), Qt::SmoothTransformation);
    break;
  case e20:
    pix = m_topRightPix.transformed(QTransform().rotate(180), Qt::SmoothTransformation);
    break;
  case e21:
    pix = m_topPix.transformed(QTransform().rotate(180), Qt::SmoothTransformation);
    break;
  case e22:
    pix = m_topRightPix.transformed(QTransform().rotate(90), Qt::SmoothTransformation);
    break;
  default:
    break;
  }
  (*button)->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on00() {
  m_pegging = e00;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_00->setIcon(pix);
  m_01->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
  m_10->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));

  m_02->setIcon(pix);
  m_12->setIcon(pix);
  m_20->setIcon(pix);
  m_21->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on01() {
  m_pegging = e01;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_01->setIcon(pix);
  m_00->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_10->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));
  m_12->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
  m_02->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));

  m_20->setIcon(pix);
  m_21->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on02() {
  m_pegging = e02;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_02->setIcon(pix);
  m_01->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_12->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_10->setIcon(pix);
  m_20->setIcon(pix);
  m_21->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on10() {
  m_pegging = e10;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_10->setIcon(pix);
  m_00->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_01->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));
  m_21->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
  m_20->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));

  m_02->setIcon(pix);
  m_12->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on11() {
  m_pegging = e11;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_11->setIcon(pix);
  m_00->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_01->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_02->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_10->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_12->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));
  m_20->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_21->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));
  m_22->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
}

//-----------------------------------------------------------------------------

void PeggingWidget::on12() {
  m_pegging = e12;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_12->setIcon(pix);
  m_02->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_01->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_21->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_22->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_10->setIcon(pix);
  m_20->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on20() {
  m_pegging = e20;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_20->setIcon(pix);
  m_10->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_21->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_01->setIcon(pix);
  m_02->setIcon(pix);
  m_12->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on21() {
  m_pegging = e21;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_21->setIcon(pix);
  m_20->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_10->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_12->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_22->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_01->setIcon(pix);
  m_02->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on22() {
  m_pegging = e22;
  emit peggingChanged();
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_22->setIcon(pix);
  m_12->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QTransform().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_21->setIcon(m_topPix.transformed(QTransform().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_01->setIcon(pix);
  m_02->setIcon(pix);
  m_10->setIcon(pix);
  m_20->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::updateAnchor() {
  switch (m_pegging) {
  case e00:
    on00();
    break;
  case e01:
    on01();
    break;
  case e02:
    on02();
    break;
  case e10:
    on10();
    break;
  case e11:
    on11();
    break;
  case e12:
    on12();
    break;
  case e20:
    on20();
    break;
  case e21:
    on21();
    break;
  case e22:
    on22();
    break;
  }
}

//-----------------------------------------------------------------------------

void PeggingWidget::setPeggingPosition(PeggingPositions position) {
  if (m_pegging == position) return;
  switch (position) {
  case e00:
    m_00->setChecked(true);
    on00();
    break;
  case e01:
    m_01->setChecked(true);
    on01();
    break;
  case e02:
    m_02->setChecked(true);
    on02();
    break;
  case e10:
    m_10->setChecked(true);
    on10();
    break;
  case e11:
    m_11->setChecked(true);
    on11();
    break;
  case e12:
    m_12->setChecked(true);
    on12();
    break;
  case e20:
    m_20->setChecked(true);
    on20();
    break;
  case e21:
    m_21->setChecked(true);
    on21();
    break;
  case e22:
    m_22->setChecked(true);
    on22();
    break;
  }
}

//=============================================================================
// CanvasSizeTool - viewer overlay for Canvas Size
//-----------------------------------------------------------------------------

class CanvasSizeTool final : public TTool {
  TPointD m_firstPos;
  TRectD m_dragStartRect, m_lastInvalidateRect;
  int m_handle;
  bool m_dragging;

  enum {
    hNone,
    h00,
    h01,
    h10,
    h11,
    hM0,
    h1M,
    hM1,
    h0M
  };

public:
  static CanvasSizeTool *instance() { return s_instance; }

  CanvasSizeTool()
      : TTool(T_CanvasSize)
      , m_firstPos(-1, -1)
      , m_handle(hNone)
      , m_dragging(false) {
    s_instance = this;
    bind(TTool::ToonzImage | TTool::RasterImage);
  }

  ToolType getToolType() const override { return TTool::LevelReadTool; }
  unsigned int getToolHints() const override { return 0; }
  bool isDragging() const override { return m_dragging; }

  QString updateEnabled(int, int) override {
    TTool::Application *app = getApplication();
    if (!app) return (enable(false), QString());
    TXshSimpleLevel *sl = app->getCurrentLevel()->getSimpleLevel();
    if (isRasterCanvasLevel(sl)) return (enable(true), QString());
    return (enable(false), QString());
  }

  void draw() override;
  void mouseMove(const TPointD &p, const TMouseEvent &) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &, const TMouseEvent &) override;
  int getCursorId() const override;
  void onActivate() override;
  void onDeactivate() override;

  void refresh(bool wholeViewer);

private:
  int pickHandle(const TPointD &p, const TRectD &r) const;
  PeggingPositions pegForHandle(int handle) const;
  TRectD dragRect(const TPointD &pos, const TMouseEvent &e) const;

  static CanvasSizeTool *s_instance;
};

CanvasSizeTool *CanvasSizeTool::s_instance = 0;
CanvasSizeTool canvasSizeTool;

//-----------------------------------------------------------------------------

int CanvasSizeTool::pickHandle(const TPointD &p, const TRectD &r) const {
  double pixelSize = getPixelSize();
  TPointD size(10 * pixelSize, 10 * pixelSize);
  double maxDist = 5 * pixelSize;

  if (TRectD(r.getP00() - size, r.getP00() + size).contains(p)) return h00;
  if (TRectD(r.getP01() - size, r.getP01() + size).contains(p)) return h01;
  if (TRectD(r.getP11() - size, r.getP11() + size).contains(p)) return h11;
  if (TRectD(r.getP10() - size, r.getP10() + size).contains(p)) return h10;
  if (isCloseToSegment(p, TSegment(r.getP00(), r.getP10()), maxDist)) return hM0;
  if (isCloseToSegment(p, TSegment(r.getP10(), r.getP11()), maxDist)) return h1M;
  if (isCloseToSegment(p, TSegment(r.getP11(), r.getP01()), maxDist)) return hM1;
  if (isCloseToSegment(p, TSegment(r.getP01(), r.getP00()), maxDist)) return h0M;
  return hNone;
}

//-----------------------------------------------------------------------------

PeggingPositions CanvasSizeTool::pegForHandle(int handle) const {
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (!popup) return ::e11;
  PeggingPositions cur = popup->peggingPosition();

  int row = (int)cur / 3;
  int col = (int)cur % 3;

  switch (handle) {
  case h00:
    return ::e02;
  case h01:
    return ::e22;
  case h10:
    return ::e00;
  case h11:
    return ::e20;
  case h0M:
    col = 2;
    break;
  case h1M:
    col = 0;
    break;
  case hM0:
    row = 0;
    break;
  case hM1:
    row = 2;
    break;
  default:
    return cur;
  }
  return (PeggingPositions)(row * 3 + col);
}

//-----------------------------------------------------------------------------

TRectD CanvasSizeTool::dragRect(const TPointD &pos, const TMouseEvent &e) const {
  TRectD r             = m_dragStartRect;
  TPointD delta        = pos - m_firstPos;
  const bool uniform   = e.isShiftPressed();
  const bool fromCenter = e.isAltPressed();
  double W             = r.x1 - r.x0;
  double H             = r.y1 - r.y0;
  if (W < 1.0) W = 1.0;
  if (H < 1.0) H = 1.0;
  const double ar = W / H;
  const double cx = 0.5 * (r.x0 + r.x1);
  const double cy = 0.5 * (r.y0 + r.y1);

  const bool moveL = m_handle == h00 || m_handle == h01 || m_handle == h0M;
  const bool moveR = m_handle == h10 || m_handle == h11 || m_handle == h1M;
  const bool moveB = m_handle == h00 || m_handle == h10 || m_handle == hM0;
  const bool moveT = m_handle == h01 || m_handle == h11 || m_handle == hM1;

  double dW = 0, dH = 0;
  if (moveR) dW += delta.x;
  if (moveL) dW -= delta.x;
  if (moveT) dH += delta.y;
  if (moveB) dH -= delta.y;
  if (fromCenter) {
    dW *= 2.0;
    dH *= 2.0;
  }

  if (uniform) {
    const bool isCorner = (moveL || moveR) && (moveB || moveT);
    if (isCorner) {
      if (fabs(dW) * H >= fabs(dH) * W)
        dH = dW / ar;
      else
        dW = dH * ar;
    } else if (moveL || moveR)
      dH = dW / ar;
    else
      dW = dH * ar;
  }

  double newW = std::max(1.0, W + dW);
  double newH = std::max(1.0, H + dH);
  if (uniform) {
    if (moveL || moveR)
      newH = std::max(1.0, newW / ar);
    else
      newW = std::max(1.0, newH * ar);
  }

  if (fromCenter) {
    r.x0 = cx - 0.5 * newW;
    r.x1 = cx + 0.5 * newW;
    r.y0 = cy - 0.5 * newH;
    r.y1 = cy + 0.5 * newH;
    return r;
  }

  if (moveL && !moveR)
    r.x0 = r.x1 - newW;
  else if (moveR && !moveL)
    r.x1 = r.x0 + newW;
  else {
    r.x0 = cx - 0.5 * newW;
    r.x1 = cx + 0.5 * newW;
  }

  if (moveB && !moveT)
    r.y0 = r.y1 - newH;
  else if (moveT && !moveB)
    r.y1 = r.y0 + newH;
  else {
    r.y0 = cy - 0.5 * newH;
    r.y1 = cy + 0.5 * newH;
  }
  return r;
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::draw() {
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (!popup || !popup->isSessionActive()) return;

  TRectD currentR  = popup->currentCanvasRect();
  TRectD proposedR = popup->proposedCanvasRect();
  if (currentR.isEmpty() && proposedR.isEmpty()) return;

  double pixelSize = getPixelSize();
  double handleR   = 4.0 * pixelSize;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4d(0.15, 0.65, 0.95, 0.12);
  glBegin(GL_QUADS);
  glVertex2d(proposedR.x0, proposedR.y0);
  glVertex2d(proposedR.x1, proposedR.y0);
  glVertex2d(proposedR.x1, proposedR.y1);
  glVertex2d(proposedR.x0, proposedR.y1);
  glEnd();
  glDisable(GL_BLEND);

  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, 0xCCCC);
  glColor3d(0.85, 0.85, 0.85);
  glBegin(GL_LINE_LOOP);
  glVertex2d(currentR.x0, currentR.y0);
  glVertex2d(currentR.x1, currentR.y0);
  glVertex2d(currentR.x1, currentR.y1);
  glVertex2d(currentR.x0, currentR.y1);
  glEnd();
  glDisable(GL_LINE_STIPPLE);

  glColor3d(0.2, 0.85, 1.0);
  glBegin(GL_LINE_LOOP);
  glVertex2d(proposedR.x0, proposedR.y0);
  glVertex2d(proposedR.x1, proposedR.y0);
  glVertex2d(proposedR.x1, proposedR.y1);
  glVertex2d(proposedR.x0, proposedR.y1);
  glEnd();

  TPixel32 handleColor(70, 210, 255);
  ToolUtils::drawSquare(proposedR.getP00(), handleR, handleColor);
  ToolUtils::drawSquare(proposedR.getP01(), handleR, handleColor);
  ToolUtils::drawSquare(proposedR.getP10(), handleR, handleColor);
  ToolUtils::drawSquare(proposedR.getP11(), handleR, handleColor);
  TPointD center(0.5 * (proposedR.x0 + proposedR.x1),
                 0.5 * (proposedR.y0 + proposedR.y1));
  ToolUtils::drawSquare(TPointD(center.x, proposedR.y0), handleR, handleColor);
  ToolUtils::drawSquare(TPointD(proposedR.x1, center.y), handleR, handleColor);
  ToolUtils::drawSquare(TPointD(center.x, proposedR.y1), handleR, handleColor);
  ToolUtils::drawSquare(TPointD(proposedR.x0, center.y), handleR, handleColor);

  TPointD labelPos = proposedR.getP01() + TPointD(0, 4 * pixelSize);
  glPushMatrix();
  glTranslated(labelPos.x, labelPos.y, 0);
  glScaled(2, 2, 2);
  tglDrawText(TPointD(), "Canvas");
  glPopMatrix();
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::mouseMove(const TPointD &p, const TMouseEvent &) {
  if (m_dragging) return;
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (!popup || !popup->isSessionActive()) {
    m_handle = hNone;
    return;
  }
  m_handle = pickHandle(p, popup->proposedCanvasRect());
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (!popup || !popup->isSessionActive()) return;

  m_firstPos       = pos;
  m_dragStartRect  = popup->proposedCanvasRect();
  m_handle         = pickHandle(pos, m_dragStartRect);
  m_dragging       = m_handle != hNone;
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_dragging) return;
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (!popup || !popup->isSessionActive()) return;

  TRectD next = dragRect(pos, e);
  PeggingPositions peg =
      e.isAltPressed() ? ::e11 : pegForHandle(m_handle);
  popup->setProposedRectFromTool(next, peg);
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::leftButtonUp(const TPointD &, const TMouseEvent &) {
  m_dragging      = false;
  m_firstPos      = TPointD(-1, -1);
  m_dragStartRect = TRectD();
}

//-----------------------------------------------------------------------------

int CanvasSizeTool::getCursorId() const {
  switch (m_handle) {
  case h11:
  case h00:
    return ToolCursor::ScaleCursor;
  case h10:
  case h01:
    return ToolCursor::ScaleInvCursor;
  case h1M:
  case h0M:
    return ToolCursor::ScaleHCursor;
  case hM1:
  case hM0:
    return ToolCursor::ScaleVCursor;
  default:
    return ToolCursor::CURSOR_ARROW;
  }
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::onActivate() { refresh(true); }

//-----------------------------------------------------------------------------

void CanvasSizeTool::onDeactivate() {
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (!popup || !popup->isSessionActive()) return;

  TTool::Application *app = getApplication();
  if (!app) return;
  QString next = app->getCurrentTool()->getRequestedToolName();
  if (isNavigationToolName(next) || next == T_CanvasSize) return;
  popup->cancelFromOutside();
}

//-----------------------------------------------------------------------------

void CanvasSizeTool::refresh(bool wholeViewer) {
  CanvasSizePopup *popup = CanvasSizePopup::instance();
  if (wholeViewer || !popup) {
    m_lastInvalidateRect = TRectD();
    invalidate();
    return;
  }

  TRectD box = rectUnion(popup->currentCanvasRect(), popup->proposedCanvasRect());
  box        = box.enlarge(24.0 * getPixelSize());
  TRectD dirty = rectUnion(m_lastInvalidateRect, box);
  if (!dirty.isEmpty()) invalidate(dirty);
  m_lastInvalidateRect = box;
}

//=============================================================================
// CanvasSizePopup
//-----------------------------------------------------------------------------

namespace {
CanvasSizePopup *s_canvasSizePopup = 0;
}

CanvasSizePopup *CanvasSizePopup::instance() { return s_canvasSizePopup; }

PeggingPositions CanvasSizePopup::peggingPosition() const {
  return m_pegging->getPeggingPosition();
}

//-----------------------------------------------------------------------------

CanvasSizePopup::CanvasSizePopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, true, "CanvasSize")
    , m_currentDim(1, 1)
    , m_sessionActive(false)
    , m_ignoreSync(false)
    , m_fromTool(false) {
  s_canvasSizePopup = this;
  m_sl              = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  TDimension dim    = m_sl ? m_sl->getResolution() : TDimension(1, 1);
  m_currentDim      = dim;
  m_currentRect = m_proposedRect = worldRectFromDim(dim);

  setModal(false);
  setWindowModality(Qt::NonModal);
  setWindowFlags(windowFlags() | Qt::Tool);
  setWindowTitle(tr("Canvas Size"));
  setMinimumSize(QSize(300, 350));

  beginVLayout();

  addSeparator(tr("Current Size"));

  m_currentXSize = new QLabel(QString::number(dim.lx));
  m_currentXSize->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_currentXSize->setFixedHeight(DVGui::WidgetHeight);
  addWidget(tr("Width:"), m_currentXSize);

  m_currentYSize = new QLabel(QString::number(dim.ly));
  m_currentYSize->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_currentYSize->setFixedHeight(DVGui::WidgetHeight);
  addWidget(tr("Height:"), m_currentYSize);

  addSeparator(tr("New Size"));

  m_unit = new QComboBox(this);
  m_unit->addItem(tr("pixel"), "pixel");
  m_unit->addItem(tr("mm"), "mm");
  m_unit->addItem(tr("cm"), "cm");
  m_unit->addItem(tr("field"), "field");
  m_unit->addItem(tr("inch"), "inch");
  m_unit->setFixedHeight(DVGui::WidgetHeight);
  addWidget(tr("Unit:"), m_unit);
  connect(m_unit, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onUnitChanged(int)));

  m_xSizeFld = new DVGui::DoubleLineEdit(this, dim.lx);
  m_xSizeFld->setFixedSize(80, DVGui::WidgetHeight);
  addWidget(tr("Width:"), m_xSizeFld);
  connect(m_xSizeFld, SIGNAL(textChanged(const QString &)), this,
          SLOT(onSizeChanged()));

  m_ySizeFld = new DVGui::DoubleLineEdit(this, dim.ly);
  m_ySizeFld->setFixedSize(80, DVGui::WidgetHeight);
  addWidget(tr("Height:"), m_ySizeFld);
  connect(m_ySizeFld, SIGNAL(textChanged(const QString &)), this,
          SLOT(onSizeChanged()));

  m_relative = new DVGui::CheckBox(tr("Relative"), this);
  m_relative->setFixedHeight(DVGui::WidgetHeight);
  m_relative->setChecked(false);
  connect(m_relative, SIGNAL(toggled(bool)), this, SLOT(onRelative(bool)));

  m_updateCamera = new DVGui::CheckBox(tr("Set Camera"), this);
  m_updateCamera->setFixedHeight(DVGui::WidgetHeight);
  m_updateCamera->setChecked(false);
  addWidgets(m_updateCamera, m_relative);

  addSeparator(tr("Anchor"));

  m_pegging = new PeggingWidget(this);
  addWidget("", m_pegging);
  connect(m_pegging, SIGNAL(peggingChanged()), this, SLOT(onPeggingChanged()));

  endVLayout();

  QPushButton *okBtn = new QPushButton(tr("Resize"), this);
  okBtn->setDefault(true);
  QPushButton *resetBtn  = new QPushButton(tr("Reset"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtn()));
  connect(resetBtn, SIGNAL(clicked()), this, SLOT(onReset()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, resetBtn, cancelBtn);
  setButtonBarSpacing(8);
  okBtn->setMinimumSize(50, 25);
  resetBtn->setMinimumSize(50, 25);
  cancelBtn->setMinimumSize(50, 25);

  m_xMeasure = TMeasureManager::instance()->get("canvas.lx");
  m_yMeasure = TMeasureManager::instance()->get("canvas.ly");

  connect(TApp::instance()->getCurrentLevel(),
          SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(onLevelSwitched(TXshLevel *)));
  updateCanvasSizeCommandEnabled();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::openSession() {
  if (isVisible()) {
    hide();
    return;
  }
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!isRasterCanvasLevel(sl)) return;
  show();
  raise();
  activateWindow();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::cancelFromOutside() {
  m_sessionActive = false;
  hide();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::refreshOverlay(bool wholeViewer) {
  if (CanvasSizeTool::instance()) CanvasSizeTool::instance()->refresh(wholeViewer);
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::initFromLevel() {
  m_sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (!isRasterCanvasLevel(m_sl.getPointer())) return;

  m_currentDim     = m_sl->getResolution();
  m_currentRect    = worldRectFromDim(m_currentDim);
  m_proposedRect   = m_currentRect;

  TPointD dpi  = m_sl->getDpi();
  QString unit = m_unit->currentData().toString();
  double dimLx =
      getMeasuredLength(m_currentDim.lx, m_xMeasure, dpi.x, unit);
  double dimLy =
      getMeasuredLength(m_currentDim.ly, m_yMeasure, dpi.y, unit);

  m_ignoreSync = true;
  m_currentXSize->setText(QString::number(dimLx));
  m_currentYSize->setText(QString::number(dimLy));
  m_relative->setChecked(false);
  m_xSizeFld->setValue(dimLx);
  m_ySizeFld->setValue(dimLy);
  m_pegging->resetWidget();
  m_ignoreSync = false;
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::endSession() {
  if (!m_sessionActive) return;
  m_sessionActive = false;
  ToolHandle *th  = TApp::instance()->getCurrentTool();
  if (th && th->getRequestedToolName() == T_CanvasSize) th->unsetPseudoTool();
  refreshOverlay(true);
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::showEvent(QShowEvent *e) {
  DVGui::Dialog::showEvent(e);
  initFromLevel();
  if (!isRasterCanvasLevel(m_sl.getPointer())) {
    hide();
    return;
  }
  m_sessionActive = true;
  ToolHandle *th  = TApp::instance()->getCurrentTool();
  if (th && th->getRequestedToolName() != T_CanvasSize)
    th->setPseudoTool(T_CanvasSize);
  refreshOverlay(true);
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::hideEvent(QHideEvent *e) {
  endSession();
  DVGui::Dialog::hideEvent(e);
}

//-----------------------------------------------------------------------------

TDimension CanvasSizePopup::proposedPixelSize() const {
  if (!m_sl) return TDimension(1, 1);
  TPointD dpi  = m_sl->getDpi();
  QString unit = m_unit->currentData().toString();
  int nx = getPixelLength(m_xSizeFld->getValue(), m_xMeasure, dpi.x, unit);
  int ny = getPixelLength(m_ySizeFld->getValue(), m_yMeasure, dpi.y, unit);
  if (m_relative->isChecked()) {
    nx += m_currentDim.lx;
    ny += m_currentDim.ly;
  }
  return TDimension(std::max(1, nx), std::max(1, ny));
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::updateProposedFromFields() {
  if (!m_sl) return;
  TDimension newDim  = proposedPixelSize();
  m_proposedRect     = proposedRectFromPeg(
      m_currentRect, newDim.lx, newDim.ly, m_pegging->getPeggingPosition());
  m_pegging->cutLx(m_currentDim.lx > newDim.lx);
  m_pegging->cutLy(m_currentDim.ly > newDim.ly);
  m_ignoreSync = true;
  m_pegging->updateAnchor();
  m_ignoreSync = false;
  refreshOverlay();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::syncFieldsFromRect() {
  if (!m_sl) return;
  TDimension newDim = dimFromRect(m_proposedRect);
  TPointD dpi       = m_sl->getDpi();
  QString unit      = m_unit->currentData().toString();

  int xVal = m_relative->isChecked() ? newDim.lx - m_currentDim.lx : newDim.lx;
  int yVal = m_relative->isChecked() ? newDim.ly - m_currentDim.ly : newDim.ly;

  m_ignoreSync = true;
  m_xSizeFld->blockSignals(true);
  m_ySizeFld->blockSignals(true);
  m_xSizeFld->setValue(getMeasuredLength(xVal, m_xMeasure, dpi.x, unit));
  m_ySizeFld->setValue(getMeasuredLength(yVal, m_yMeasure, dpi.y, unit));
  m_xSizeFld->blockSignals(false);
  m_ySizeFld->blockSignals(false);
  m_pegging->cutLx(m_currentDim.lx > newDim.lx);
  m_pegging->cutLy(m_currentDim.ly > newDim.ly);
  m_pegging->updateAnchor();
  m_ignoreSync = false;
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::setProposedRectFromTool(const TRectD &rect,
                                              PeggingPositions peg) {
  m_proposedRect = snapRectToPixelGrid(rect, m_currentRect);
  m_fromTool     = true;
  m_ignoreSync   = true;
  m_pegging->setPeggingPosition(peg);
  syncFieldsFromRect();
  m_ignoreSync = false;
  m_fromTool   = false;
  refreshOverlay();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onSizeChanged() {
  if (m_ignoreSync || !m_sl) return;
  updateProposedFromFields();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onPeggingChanged() {
  if (m_ignoreSync || m_fromTool || !m_sl) return;
  updateProposedFromFields();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onRelative(bool toggled) {
  if (m_ignoreSync || !m_sl) return;
  TDimension prop = dimFromRect(m_proposedRect);
  TPointD dpi     = m_sl->getDpi();
  QString unit    = m_unit->currentData().toString();
  int xVal        = toggled ? prop.lx - m_currentDim.lx : prop.lx;
  int yVal        = toggled ? prop.ly - m_currentDim.ly : prop.ly;
  m_ignoreSync    = true;
  m_xSizeFld->setValue(getMeasuredLength(xVal, m_xMeasure, dpi.x, unit));
  m_ySizeFld->setValue(getMeasuredLength(yVal, m_yMeasure, dpi.y, unit));
  m_ignoreSync = false;
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onReset() {
  if (!m_sl) return;
  m_proposedRect = m_currentRect;
  m_ignoreSync   = true;
  m_relative->setChecked(false);
  m_pegging->resetWidget();
  m_ignoreSync = false;
  syncFieldsFromRect();
  refreshOverlay(true);
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onUnitChanged(int index) {
  if (!m_sl) return;
  QString unit = m_unit->itemData(index).toString();
  if (unit != "pixel") {
    TUnit *measureUnit = m_xMeasure->getUnit(unit.toStdWString());
    m_xMeasure->setCurrentUnit(measureUnit);
    measureUnit = m_yMeasure->getUnit(unit.toStdWString());
    m_yMeasure->setCurrentUnit(measureUnit);
  }
  TPointD dpi  = m_sl->getDpi();
  double dimLx =
      getMeasuredLength(m_currentDim.lx, m_xMeasure, dpi.x, unit);
  double dimLy =
      getMeasuredLength(m_currentDim.ly, m_yMeasure, dpi.y, unit);
  m_ignoreSync = true;
  m_currentXSize->setText(QString::number(dimLx));
  m_currentYSize->setText(QString::number(dimLy));
  m_ignoreSync = false;
  syncFieldsFromRect();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onLevelSwitched(TXshLevel *) {
  updateCanvasSizeCommandEnabled();
  if (!m_sessionActive) return;
  TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  if (sl == m_sl.getPointer() && isRasterCanvasLevel(sl)) return;
  hide();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onOkBtn() {
  if (!m_sl) {
    hide();
    return;
  }

  TDimension dim    = m_currentDim;
  TDimension newDim = dimFromRect(m_proposedRect);
  newDim.lx         = std::max(1, newDim.lx);
  newDim.ly         = std::max(1, newDim.ly);

  bool canvasChanged = dim.lx != newDim.lx || dim.ly != newDim.ly;
  bool updateCamera  = m_updateCamera->isChecked();

  if (!canvasChanged && !updateCamera) {
    hide();
    return;
  }

  if (canvasChanged && (dim.lx > newDim.lx || dim.ly > newDim.ly)) {
    int ret = DVGui::MsgBox(tr("The new canvas size is smaller than the "
                               "current one.\nDo you want to crop the canvas?"),
                            tr("Crop"), tr("Cancel"));
    if (ret == 2) return;
  }

  TPoint pos;
  pos.x = tround(m_currentRect.x0 - m_proposedRect.x0);
  pos.y = tround(m_currentRect.y0 - m_proposedRect.y0);

  hide();

  QApplication::setOverrideCursor(Qt::WaitCursor);
  TUndoManager::manager()->beginBlock();

  if (canvasChanged) {
    int i;
    std::vector<TFrameId> fids;
    m_sl->getFids(fids);
    for (i = 0; i < (int)fids.size(); i++) {
      TImageP img = m_sl->getFrame(fids[i], true);
      TRasterImageP ri(img);
      TToonzImageP ti(img);

      if (ri) {
        TRasterP ras    = ri->getRaster();
        TRasterP newRas = ras->create(newDim.lx, newDim.ly);
        if (newRas->getPixelSize() < 4)
          memset(newRas->getRawData(), 255,
                 newRas->getPixelSize() * newRas->getWrap() * newRas->getLy());
        else
          newRas->clear();
        newRas->copy(ras, pos);
        TRasterImageP newImg(newRas);
        double xdpi, ydpi;
        ri->getDpi(xdpi, ydpi);
        newImg->setDpi(xdpi, ydpi);
        m_sl->setFrame(fids[i], newImg);
        TUndoManager::manager()->add(
            new ResizeCanvasUndo(m_sl, fids[i], img, newImg, dim, newDim));
      } else if (ti) {
        TRasterP ras    = ti->getRaster();
        TRasterP newRas = ras->create(newDim.lx, newDim.ly);
        newRas->clear();
        newRas->copy(ras, pos);
        TRect box;
        TRop::computeBBox(newRas, box);
        TToonzImageP newImg(newRas, box);
        double xdpi, ydpi;
        ti->getDpi(xdpi, ydpi);
        newImg->setDpi(xdpi, ydpi);
        m_sl->setFrame(fids[i], newImg);
        TUndoManager::manager()->add(
            new ResizeCanvasUndo(m_sl, fids[i], img, newImg, dim, newDim));
      } else
        assert(0);
      IconGenerator::instance()->invalidate(m_sl.getPointer(), fids[i]);
      m_sl->touchFrame(fids[i]);
    }
    m_sl->getProperties()->setImageRes(newDim);
    IconGenerator::instance()->invalidateSceneIcon();
  }
  if (updateCamera) applyCameraFromLevel(m_sl.getPointer());
  TUndoManager::manager()->endBlock();
  QApplication::restoreOverrideCursor();
  if (canvasChanged)
    TApp::instance()->getCurrentLevel()->notifyCanvasSizeChange();
}

//=============================================================================

class CanvasSizeCommandHandler final : public MenuItemHandler {
public:
  CanvasSizeCommandHandler() : MenuItemHandler(MI_CanvasSize) {}
  void execute() override {
    CanvasSizePopup *popup = CanvasSizePopup::instance();
    if (!popup) popup = new CanvasSizePopup();
    popup->openSession();
  }
} canvasSizeCommandHandler;
