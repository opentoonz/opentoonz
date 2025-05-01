#include "layerfooterpanel.h"

#include <QPainter>
#include <QMenu>
#include "xsheetviewer.h"
#include "xshcolumnviewer.h"
#include "toonzqt/gutil.h"

//-----------------------------------------------------------------------------

LayerFooterPanel::LayerFooterPanel(XsheetViewer* viewer, QWidget* parent,
                                   Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_viewer(viewer)
    , m_frameZoomSlider(nullptr)
    , m_zoomInButton(nullptr)
    , m_zoomOutButton(nullptr)
    , isCtrlPressed(false) {
  setObjectName("layerFooterPanel");

  const Orientation* o = viewer->orientation();

  // Create and configure the zoom slider
  Qt::Orientation ori = o->isVerticalTimeline() ? Qt::Vertical : Qt::Horizontal;
  bool invDirection   = o->isVerticalTimeline();
  QString tooltipStr  = o->isVerticalTimeline() ? tr("Zoom in/out of xsheet")
                                                : tr("Zoom in/out of timeline");

  m_frameZoomSlider = new QSlider(ori, this);
  m_frameZoomSlider->setMinimum(20);
  m_frameZoomSlider->setMaximum(100);
  m_frameZoomSlider->setValue(m_viewer->getFrameZoomFactor());
  m_frameZoomSlider->setToolTip(tooltipStr);
  m_frameZoomSlider->setInvertedAppearance(invDirection);
  m_frameZoomSlider->setInvertedControls(invDirection);

  // Create and configure zoom buttons
  m_zoomInButton = new QToolButton(this);
  m_zoomInButton->setIcon(createQIcon("zoom_in"));
  m_zoomInButton->setToolTip(tr("Zoom in (Ctrl-click to zoom in all the way)"));
  m_zoomInButton->setAutoRaise(true);
  m_zoomInButton->setFocusPolicy(Qt::NoFocus);

  // Remove padding by making icon size match button size
  QSize buttonSize = o->rect(PredefinedRect::ZOOM_IN).size();
  m_zoomInButton->setIconSize(buttonSize);
  m_zoomInButton->setFixedSize(buttonSize);

  m_zoomOutButton = new QToolButton(this);
  m_zoomOutButton->setIcon(createQIcon("zoom_out"));
  m_zoomOutButton->setToolTip(
      tr("Zoom out (Ctrl-click to zoom out all the way)"));
  m_zoomOutButton->setAutoRaise(true);
  m_zoomOutButton->setFocusPolicy(Qt::NoFocus);

  // Apply same sizing to zoom out button
  m_zoomOutButton->setIconSize(buttonSize);
  m_zoomOutButton->setFixedSize(buttonSize);

  // Connect signals
  connect(m_frameZoomSlider, SIGNAL(valueChanged(int)), this,
          SLOT(onFrameZoomSliderValueChanged(int)));
  connect(m_zoomInButton, SIGNAL(clicked()), this, SLOT(onZoomInClicked()));
  connect(m_zoomOutButton, SIGNAL(clicked()), this, SLOT(onZoomOutClicked()));

  // Hide slider if needed based on orientation settings
  if (o->rect(PredefinedRect::ZOOM_SLIDER).isEmpty()) {
    m_frameZoomSlider->hide();
  }

  // Initial layout setup
  showOrHide(o);
}

//-----------------------------------------------------------------------------

LayerFooterPanel::~LayerFooterPanel() {}

//-----------------------------------------------------------------------------

void LayerFooterPanel::showOrHide(const Orientation* o) {
  QRect rect = o->rect(PredefinedRect::LAYER_FOOTER_PANEL);
  setFixedSize(rect.size());

  // Update slider configuration
  Qt::Orientation ori = o->isVerticalTimeline() ? Qt::Vertical : Qt::Horizontal;
  bool invDirection   = o->isVerticalTimeline();
  QString tooltipStr  = o->isVerticalTimeline() ? tr("Zoom in/out of xsheet")
                                                : tr("Zoom in/out of timeline");

  m_frameZoomSlider->setOrientation(ori);
  m_frameZoomSlider->setToolTip(tooltipStr);
  m_frameZoomSlider->setInvertedAppearance(invDirection);
  m_frameZoomSlider->setInvertedControls(invDirection);

  // Position widgets according to orientation
  QRect sliderRect = o->rect(PredefinedRect::ZOOM_SLIDER);
  m_frameZoomSlider->setGeometry(sliderRect);

  m_zoomInButton->setGeometry(o->rect(PredefinedRect::ZOOM_IN));
  m_zoomOutButton->setGeometry(o->rect(PredefinedRect::ZOOM_OUT));

  // Handle slider visibility
  if (o->rect(PredefinedRect::ZOOM_SLIDER).isEmpty()) {
    m_frameZoomSlider->hide();
  } else {
    m_frameZoomSlider->show();
  }

  show();
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::setZoomSliderValue(int val) {
  val = qBound(m_frameZoomSlider->minimum(), val, m_frameZoomSlider->maximum());

  m_frameZoomSlider->blockSignals(true);
  m_frameZoomSlider->setValue(val);
  m_frameZoomSlider->blockSignals(false);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onFrameZoomSliderValueChanged(int val) {
  m_viewer->zoomOnFrame(m_viewer->getCurrentRow(), val);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onZoomInClicked() {
  int newFactor = isCtrlPressed ? m_frameZoomSlider->maximum()
                                : m_frameZoomSlider->value() + 10;
  m_frameZoomSlider->setValue(newFactor);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onZoomOutClicked() {
  int newFactor = isCtrlPressed ? m_frameZoomSlider->minimum()
                                : m_frameZoomSlider->value() - 10;
  m_frameZoomSlider->setValue(newFactor);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::paintEvent(QPaintEvent* event) {
  // Only needed for drawing the separator line in horizontal mode
  if (!m_viewer->orientation()->isVerticalTimeline()) {
    QPainter p(this);
    p.setPen(m_viewer->getVerticalLineColor());

    // Draw separator line
    QRect zoomOutRect = m_viewer->orientation()->rect(PredefinedRect::ZOOM_OUT);
    QLine line        = QLine(zoomOutRect.topLeft(), zoomOutRect.bottomLeft())
                     .translated(-2, 0);
    p.drawLine(line);
  }

  QWidget::paintEvent(event);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::contextMenuEvent(QContextMenuEvent* event) {
  if (!m_viewer->orientation()->isVerticalTimeline()) {
    event->ignore();
    return;
  }

  QList<int> frames = m_viewer->availableFramesPerPage();
  if (frames.isEmpty()) {
    return;
  }

  QMenu* menu = new QMenu(this);
  for (auto frame : frames) {
    QAction* action = menu->addAction(tr("%1 frames per page").arg(frame));
    action->setData(frame);
    connect(action, SIGNAL(triggered()), this, SLOT(onFramesPerPageSelected()));
  }
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onFramesPerPageSelected() {
  QAction* action = qobject_cast<QAction*>(sender());
  if (action) {
    int frame = action->data().toInt();
    m_viewer->zoomToFramesPerPage(frame);
  }
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onControlPressed(bool pressed) {
  isCtrlPressed = pressed;
}

//-----------------------------------------------------------------------------

bool LayerFooterPanel::isControlPressed() const { return isCtrlPressed; }
