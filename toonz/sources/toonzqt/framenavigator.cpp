

// TnzLib includes
#include "toonz/tframehandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// Qt includes
#include <QAction>

#include "toonzqt/framenavigator.h"

using namespace DVGui;

//=============================================================================
// FrameNavigator
//-----------------------------------------------------------------------------

FrameNavigator::FrameNavigator(QWidget *parent)
    : QToolBar(parent), m_frame(0), m_lineEdit(0), m_frameHandle(0) {
  setMaximumWidth(130);
  setIconSize(QSize(17, 17));
  setObjectName("WidePaddingToolBar");
  QAction *prevButton =
      new QAction(createQIcon("frameprev"), tr("Previous Frame"), this);
  connect(prevButton, SIGNAL(triggered()), this, SLOT(prevFrame()));
  addAction(prevButton);

  m_lineEdit = new DVGui::IntLineEdit(this);

  m_lineEdit->setFixedHeight(16);
  bool ret = connect(m_lineEdit, SIGNAL(editingFinished()), this,
                     SLOT(onEditingFinished()));
  addWidget(m_lineEdit);

  QAction *nextButton =
      new QAction(createQIcon("framenext"), tr("Next Frame"), this);
  ret =
      ret && connect(nextButton, SIGNAL(triggered()), this, SLOT(nextFrame()));
  addAction(nextButton);
  assert(ret);
}

//-----------------------------------------------------------------------------

bool FrameNavigator::anyWidgetHasFocus() {
  return hasFocus() || m_lineEdit->hasFocus() || m_lineEdit->hasFocus();
}

//-----------------------------------------------------------------------------

void FrameNavigator::setFrameHandle(TFrameHandle *fh) {
  if (fh == m_frameHandle) return;
  if (isVisible() && m_frameHandle)
    disconnect(m_frameHandle, SIGNAL(frameSwitched()), this,
               SLOT(onFrameSwitched()));
  m_frameHandle = fh;
  if (m_frameHandle) {
    if (isVisible())
      connect(m_frameHandle, SIGNAL(frameSwitched()), this,
              SLOT(onFrameSwitched()));
    updateFrame(m_frameHandle->getFrame());
  }
}

//-----------------------------------------------------------------------------

void FrameNavigator::updateFrame(int frame) {
  m_frame = frame;
  m_lineEdit->setValue(m_frame + 1);
}

//-----------------------------------------------------------------------------

void FrameNavigator::setFrame(int frame, bool notifyFrameHandler) {
  if (m_frame == frame) return;

  updateFrame(frame);

  if (m_frameHandle && notifyFrameHandler) m_frameHandle->setFrame(frame);

  if (notifyFrameHandler) emit frameSwitched();
}

//-----------------------------------------------------------------------------

void FrameNavigator::onEditingFinished() {
  setFrame(m_lineEdit->getValue() - 1, true);
}

//-----------------------------------------------------------------------------

void FrameNavigator::onFrameSwitched() {
  if (m_frameHandle) updateFrame(m_frameHandle->getFrame());
}

//-----------------------------------------------------------------------------

void FrameNavigator::showEvent(QShowEvent *) {
  onFrameSwitched();
  if (m_frameHandle)
    connect(m_frameHandle, SIGNAL(frameSwitched()), this,
            SLOT(onFrameSwitched()));
}

//-----------------------------------------------------------------------------

void FrameNavigator::hideEvent(QHideEvent *) {
  if (m_frameHandle)
    disconnect(m_frameHandle, SIGNAL(frameSwitched()), this,
               SLOT(onFrameSwitched()));
}
