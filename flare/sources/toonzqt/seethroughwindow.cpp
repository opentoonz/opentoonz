#include "toonzqt/seethroughwindow.h"

// Tnz6 includes
#include "tenv.h"

// TnzQt includes
#include "toonzqt/gutil.h"

#include <QApplication>
#include <QMainWindow>
#include <QDialog>

TEnv::IntVar SeeThroughWindowOpacity("SeeThroughWindowOpacity", 50);

//-----------------------------------------------------------------------------

SeeThroughWindowPopup::SeeThroughWindowPopup(SeeThroughWindowMode *mode,
                                             QWidget *mainWindow)
    : m_mode(mode), Dialog(mainWindow, true, false, "SeeThroughWindow") {
  setWindowTitle(tr("See Through Mode (Main Window)"));
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  setModal(false);

  m_suffixTxtSlider = "% " + tr("Opacity");

  m_mainWindow  = mainWindow;
  m_backupValue = 50;

  m_layout = new QBoxLayout(QBoxLayout::LeftToRight);

  m_opacitySlider = new QSlider(Qt::Horizontal);
  m_opacitySlider->setRange(1, 50);
  m_opacitySlider->setSingleStep(1);
  m_opacitySlider->setPageStep(5);
  setOpacitySlider(SeeThroughWindowOpacity);
  m_opacitySlider->setMinimumHeight(25);
  m_layout->addWidget(m_opacitySlider);

  m_seeThroughIcon_off = createQIcon("toggle_seethroughwin_off");
  m_seeThroughIcon_on  = createQIcon("toggle_seethroughwin_on");

  QString tooltip =
      tr("Quickly toggle main window semi-transparency and full opacity.") +
      "\n" + tr("Hold ALT while clicking to use full transparency instead.") +
      "\n" + tr("When slider is at 100% it acts as ALT is held.");

  m_opacityBtn = new QPushButton(m_seeThroughIcon_on, "");
  m_opacityBtn->setCheckable(true);
  m_opacityBtn->setToolTip(tooltip);
  m_opacityBtn->setFocusPolicy(Qt::NoFocus);
  m_opacityBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_opacityBtn->setFixedSize(25, 25);
  m_layout->addWidget(m_opacityBtn);

  m_closeBtn = new QPushButton(tr("Close"));
  m_closeBtn->setToolTip("Disable See-Through Window Mode and Close");
  m_closeBtn->setDefault(true);
  m_closeBtn->setFocusPolicy(Qt::NoFocus);

  beginHLayout();
  addLayout(m_layout);
  endHLayout();
  addButtonBarWidget(m_closeBtn);
  resizeEvent(nullptr);  // set proper orientation

  bool ret = true;

  ret = ret && connect(m_opacitySlider, SIGNAL(valueChanged(int)), this,
                       SLOT(sliderChanged(int)));
  ret = ret &&
        connect(m_opacityBtn, SIGNAL(clicked()), this, SLOT(opacityToggle()));
  ret = ret && connect(m_closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
  assert(ret);
}

//-----------------------------------------------------------------------------

QWidget *SeeThroughWindowPopup::getMainWindow() { return m_mainWindow; }

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::toggleMode() { setVisible(!isVisible()); }

//-----------------------------------------------------------------------------

int SeeThroughWindowPopup::getOpacitySlider() {
  return m_opacitySlider->value() * 2;
}

//-----------------------------------------------------------------------------

int SeeThroughWindowPopup::setOpacitySlider(int opacity) {
  int value = std::min(std::max((int)opacity / 2, 1), 50);
  m_opacitySlider->setValue(value);
  return value;
}

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::changeOpacity(int value) {
  bool hideMain = false;
  emit m_mode->opacityChanged(value, hideMain);
  if (hideMain)
    m_mainWindow->setWindowOpacity(0.0);
  else
    m_mainWindow->setWindowOpacity((double)value / 100.0);
}

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::showEvent(QShowEvent *e) {
  sliderChanged(m_opacitySlider->value());
}

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::hideEvent(QHideEvent *e) {
  SeeThroughWindowOpacity = getOpacitySlider();
  changeOpacity(100);
}

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::resizeEvent(QResizeEvent *e) {
  bool portrait = width() > height();
  m_layout->setDirection(portrait ? QBoxLayout::LeftToRight
                                  : QBoxLayout::TopToBottom);
  m_opacitySlider->setOrientation(portrait ? Qt::Orientation::Horizontal
                                           : Qt::Orientation::Vertical);
}

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::sliderChanged(int value) {
  int opacity = getOpacitySlider();
  changeOpacity(opacity);
  m_opacitySlider->setToolTip(QString::number(opacity) + m_suffixTxtSlider);
  if (m_opacityBtn->isChecked()) {
    m_opacityBtn->setChecked(false);
    m_opacityBtn->setIcon(m_seeThroughIcon_on);
  }
}

//-----------------------------------------------------------------------------

void SeeThroughWindowPopup::opacityToggle() {
  if (m_opacityBtn->isChecked()) {
    bool altMod = (QApplication::keyboardModifiers() & Qt::AltModifier);
    if (m_opacitySlider->value() >= m_opacitySlider->maximum()) altMod = true;
    changeOpacity(altMod ? 0 : 100);
    m_opacityBtn->setIcon(altMod ? QIcon() : m_seeThroughIcon_off);
  } else {
    sliderChanged(m_opacitySlider->value());
    m_opacityBtn->setIcon(m_seeThroughIcon_on);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

SeeThroughWindowMode::SeeThroughWindowMode() : m_dialog(nullptr) {}

//-----------------------------------------------------------------------------

void SeeThroughWindowMode::toggleMode(QWidget *mainWindow) {
  if (!m_dialog) m_dialog = new SeeThroughWindowPopup(this, mainWindow);
  m_dialog->setVisible(!m_dialog->isVisible());
}

//-----------------------------------------------------------------------------

QWidget *SeeThroughWindowMode::getMainWindow() {
  return m_dialog->getMainWindow();
}

//-----------------------------------------------------------------------------

int SeeThroughWindowMode::getOpacity() {
  if (m_dialog) return m_dialog->getOpacitySlider();
  return 100;
}

//-----------------------------------------------------------------------------

void SeeThroughWindowMode::refreshOpacity() {
  if (m_dialog) {
    if (m_dialog->isVisible())
      m_dialog->changeOpacity(m_dialog->getOpacitySlider());
    else
      m_dialog->changeOpacity(100);
  }
}
