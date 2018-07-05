

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

// TnzBase includes
#include "tunit.h"

// TnzCore includes
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "trop.h"
#include "tundo.h"
#include "tconvert.h"
#include "timagecache.h"

// Qt includes
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStyleOption>
#include <QPainter>
#include <QButtonGroup>
#include <QApplication>
#include <QMainWindow>

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
}
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
  gridLayout->setMargin(1);

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
    pix = m_topRightPix.transformed(QMatrix().rotate(-90),
                                    Qt::SmoothTransformation);
    break;
  case e01:
    pix = m_topPix;
    break;
  case e02:
    pix = m_topRightPix;
    break;
  case e10:
    pix = m_topPix.transformed(QMatrix().rotate(-90), Qt::SmoothTransformation);
    break;
  case e12:
    pix = m_topPix.transformed(QMatrix().rotate(90), Qt::SmoothTransformation);
    break;
  case e20:
    pix = m_topRightPix.transformed(QMatrix().rotate(180),
                                    Qt::SmoothTransformation);
    break;
  case e21:
    pix = m_topPix.transformed(QMatrix().rotate(180), Qt::SmoothTransformation);
    break;
  case e22:
    pix = m_topRightPix.transformed(QMatrix().rotate(90),
                                    Qt::SmoothTransformation);
    break;
  }
  (*button)->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on00() {
  m_pegging = e00;
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_00->setIcon(pix);
  m_01->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLx ? -90 : 90),
                                Qt::SmoothTransformation));
  m_10->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 0 : 180),
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
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_01->setIcon(pix);
  m_00->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_10->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));
  m_12->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
  m_02->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));

  m_20->setIcon(pix);
  m_21->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on02() {
  m_pegging = e02;
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_02->setIcon(pix);
  m_01->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_12->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 0 : 180),
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
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_10->setIcon(pix);
  m_00->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_01->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));
  m_21->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
  m_20->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));

  m_02->setIcon(pix);
  m_12->setIcon(pix);
  m_22->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on11() {
  m_pegging = e11;
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_11->setIcon(pix);
  m_00->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_01->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_02->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_10->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_12->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));
  m_20->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_21->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));
  m_22->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? -90 : 90),
                                Qt::SmoothTransformation));
}

//-----------------------------------------------------------------------------

void PeggingWidget::on12() {
  m_pegging = e12;
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_12->setIcon(pix);
  m_02->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_01->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_21->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 0 : 180),
                                Qt::SmoothTransformation));
  m_22->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 0 : 180),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_10->setIcon(pix);
  m_20->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on20() {
  m_pegging = e20;
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_20->setIcon(pix);
  m_10->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_21->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? -90 : 90),
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
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_21->setIcon(pix);
  m_20->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? 90 : -90),
                                     Qt::SmoothTransformation));
  m_10->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_11->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_12->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 180 : 0),
                                Qt::SmoothTransformation));
  m_22->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? -90 : 90),
                                     Qt::SmoothTransformation));

  m_00->setIcon(pix);
  m_01->setIcon(pix);
  m_02->setIcon(pix);
}

//-----------------------------------------------------------------------------

void PeggingWidget::on22() {
  m_pegging = e22;
  QPixmap pix(30, 30);
  pix.fill(Qt::transparent);
  m_22->setIcon(pix);
  m_12->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLy ? 180 : 0),
                                     Qt::SmoothTransformation));
  m_11->setIcon(
      m_topRightPix.transformed(QMatrix().rotate(m_cutLx || m_cutLy ? 90 : -90),
                                Qt::SmoothTransformation));
  m_21->setIcon(m_topPix.transformed(QMatrix().rotate(m_cutLx ? 90 : -90),
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

//=============================================================================
// CanvasSizePopup
//-----------------------------------------------------------------------------

CanvasSizePopup::CanvasSizePopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, true,
                    "CanvasSize") {
  m_sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  assert(m_sl);
  setWindowTitle(tr("Canvas Size"));
  setMinimumSize(QSize(300, 350));

  beginVLayout();

  addSeparator(tr("Current Size"));

  TDimension dim = m_sl->getResolution();

  // Current Xsize
  m_currentXSize = new QLabel(QString::number(dim.lx));
  m_currentXSize->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_currentXSize->setFixedHeight(DVGui::WidgetHeight);
  addWidget(tr("Width:"), m_currentXSize);

  // Current Ysize
  m_currentYSize = new QLabel(QString::number(dim.ly));
  m_currentYSize->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_currentYSize->setFixedHeight(DVGui::WidgetHeight);
  addWidget(tr("Height:"), m_currentYSize);

  addSeparator(tr("New Size"));

  // Unit
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

  // New Xsize
  m_xSizeFld = new DVGui::DoubleLineEdit(this, dim.lx);
  m_xSizeFld->setFixedSize(80, DVGui::WidgetHeight);
  addWidget(tr("Width:"), m_xSizeFld);
  connect(m_xSizeFld, SIGNAL(textChanged(const QString &)), this,
          SLOT(onSizeChanged()));

  // New Ysize
  m_ySizeFld = new DVGui::DoubleLineEdit(this, dim.ly);
  m_ySizeFld->setFixedSize(80, DVGui::WidgetHeight);
  addWidget(tr("Height:"), m_ySizeFld);
  connect(m_ySizeFld, SIGNAL(textChanged(const QString &)), this,
          SLOT(onSizeChanged()));

  // Relative
  m_relative = new DVGui::CheckBox(tr("Relative"), this);
  m_relative->setFixedHeight(DVGui::WidgetHeight);
  m_relative->setChecked(false);
  connect(m_relative, SIGNAL(toggled(bool)), this, SLOT(onRelative(bool)));
  addWidget(m_relative);

  addSeparator(tr("Anchor"));

  m_pegging = new PeggingWidget(this);
  addWidget("", m_pegging);

  endVLayout();

  // add buttons
  QPushButton *okBtn = new QPushButton(tr("Resize"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtn()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);

  m_xMeasure = TMeasureManager::instance()->get("canvas.lx");
  m_yMeasure = TMeasureManager::instance()->get("canvas.ly");
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::showEvent(QShowEvent *e) {
  m_sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
  assert(m_sl);
  TDimension dim = m_sl->getResolution();
  TPointD dpi    = m_sl->getDpi();
  double dimLx   = getMeasuredLength(dim.lx, m_xMeasure, dpi.x,
                                   m_unit->currentData().toString());
  double dimLy = getMeasuredLength(dim.ly, m_yMeasure, dpi.y,
                                   m_unit->currentData().toString());
  m_currentXSize->setText(QString::number(dimLx));
  m_currentYSize->setText(QString::number(dimLy));
  m_relative->setChecked(false);
  m_xSizeFld->setValue(dimLx);
  m_ySizeFld->setValue(dimLy);
  m_pegging->resetWidget();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onSizeChanged() {
  TDimension dim        = m_sl->getResolution();
  TPointD dpi           = m_sl->getDpi();
  QString unit          = m_unit->currentData().toString();
  double measuredXValue = m_relative->isChecked()
                              ? dim.lx + m_xSizeFld->getValue()
                              : m_xSizeFld->getValue();
  double measuredYValue = m_relative->isChecked()
                              ? dim.ly + m_ySizeFld->getValue()
                              : m_ySizeFld->getValue();
  double newXValue = getPixelLength(measuredXValue, m_xMeasure, dpi.x, unit);
  double newYValue = getPixelLength(measuredYValue, m_yMeasure, dpi.y, unit);
  m_pegging->cutLx(dim.lx > newXValue);
  m_pegging->cutLy(dim.ly > newYValue);
  m_pegging->updateAnchor();
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onRelative(bool toggled) {
  if (toggled) {
    m_xSizeFld->setValue(0);
    m_ySizeFld->setValue(0);
  } else {
    TDimension dim = m_sl->getResolution();
    TPointD dpi    = m_sl->getDpi();
    QString unit   = m_unit->currentData().toString();
    double dimLx   = getMeasuredLength(dim.lx, m_xMeasure, dpi.x, unit);
    double dimLy   = getMeasuredLength(dim.ly, m_yMeasure, dpi.y, unit);
    m_xSizeFld->setValue(dimLx);
    m_ySizeFld->setValue(dimLy);
  }
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onUnitChanged(int index) {
  if (m_relative->isChecked()) {
    m_xSizeFld->setValue(0);
    m_ySizeFld->setValue(0);
    return;
  }
  QString unit = m_unit->itemData(index).toString();
  if (unit != "pixel") {
    TUnit *measureUnit = m_xMeasure->getUnit(unit.toStdWString());
    m_xMeasure->setCurrentUnit(measureUnit);
    measureUnit = m_yMeasure->getUnit(unit.toStdWString());
    m_yMeasure->setCurrentUnit(measureUnit);
  }
  TDimension dim = m_sl->getResolution();
  TPointD dpi    = m_sl->getDpi();
  double dimLx   = getMeasuredLength(dim.lx, m_xMeasure, dpi.x, unit);
  double dimLy   = getMeasuredLength(dim.ly, m_yMeasure, dpi.y, unit);
  m_currentXSize->setText(QString::number(dimLx));
  m_currentYSize->setText(QString::number(dimLy));
  m_xSizeFld->setValue(dimLx);
  m_ySizeFld->setValue(dimLy);
}

//-----------------------------------------------------------------------------

void CanvasSizePopup::onOkBtn() {
  TDimension dim = m_sl->getResolution();
  TPointD dpi    = m_sl->getDpi();
  QString unit   = m_unit->currentData().toString();
  int newXValue =
      getPixelLength(m_xSizeFld->getValue(), m_xMeasure, dpi.x, unit);
  int newYValue =
      getPixelLength(m_ySizeFld->getValue(), m_yMeasure, dpi.y, unit);
  bool relative = m_relative->isChecked();
  newXValue     = relative ? dim.lx + newXValue : newXValue;
  newYValue     = relative ? dim.ly + newYValue : newYValue;
  newXValue     = newXValue > 0 ? newXValue : 1;
  newYValue     = newYValue > 0 ? newYValue : 1;

  if ((relative && newXValue == 0 && newYValue == 0) ||
      (!relative && dim.lx == newXValue && dim.ly == newYValue)) {
    close();
    return;
  }

  if (dim.lx > newXValue || dim.ly > newYValue) {
    int ret = DVGui::MsgBox(tr("The new canvas size is smaller than the "
                               "current one.\nDo you want to crop the canvas?"),
                            tr("Crop"), tr("Cancel"));
    if (ret == 2) return;
  }
  close();

  QApplication::setOverrideCursor(Qt::WaitCursor);
  PeggingPositions pegPos = m_pegging->getPeggingPosition();
  TDimension newDim(newXValue, newYValue);

  TPoint pos;
  if (pegPos == e00 || pegPos == e10 || pegPos == e20) pos.x = 0;
  if (pegPos == e01 || pegPos == e11 || pegPos == e21)
    pos.x = (newDim.lx - dim.lx) * 0.5;
  if (pegPos == e02 || pegPos == e12 || pegPos == e22)
    pos.x = newDim.lx - dim.lx;

  if (pegPos == e20 || pegPos == e21 || pegPos == e22) pos.y = 0;
  if (pegPos == e10 || pegPos == e11 || pegPos == e12)
    pos.y = (newDim.ly - dim.ly) * 0.5;
  if (pegPos == e00 || pegPos == e01 || pegPos == e02)
    pos.y = newDim.ly - dim.ly;

  int i;
  std::vector<TFrameId> fids;
  m_sl->getFids(fids);
  TUndoManager::manager()->beginBlock();
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
      // TRop::addBackground(newRas,TPixel32::White);
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
  TUndoManager::manager()->endBlock();
  m_sl->getProperties()->setImageRes(newDim);
  IconGenerator::instance()->invalidateSceneIcon();
  QApplication::restoreOverrideCursor();
  TApp::instance()->getCurrentLevel()->notifyCanvasSizeChange();
}

//=============================================================================

OpenPopupCommandHandler<CanvasSizePopup> openCanvasSizePopup(MI_CanvasSize);
