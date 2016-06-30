#pragma once

#ifndef FRAMENVIGATOR_H
#define FRAMENVIGATOR_H

// TnzCore includes
#include "tcommon.h"

// TnzQt includes
#include "toonzqt/intfield.h"

// Qt includes
#include <QWidget>
#include <QToolBar>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//======================================================

//    Forward declarations

class TFrameHandle;

//======================================================

//*****************************************************************************
//    KeyframeNavigator  declaration
//*****************************************************************************

/*!
  \brief    The FrameNavigator is a simple toolbar widget showing a numerical
            representation of an integer timeline.

  \details  A FrameNavigator is a TFrameHandle viewer composed of a line edit
            widget showing the current frame number, and a pair of directional
            arrow buttons that move to adjacent frames.

  \remark   The text field actually visualizes the internal integer \p m_frame
  \a +1
            (e.g. when \p m_frame is 0 then 1 is visualized).
*/

class DVAPI FrameNavigator final : public QToolBar {
  Q_OBJECT

  TFrameHandle *m_frameHandle;

  int m_frame;
  DVGui::IntLineEdit *m_lineEdit;

public:
  FrameNavigator(QWidget *parent = 0);
  ~FrameNavigator() {}

  int getFrame() const { return m_frame; }

  void setFrameHandle(TFrameHandle *);  //!< Attaches the navigator to the
                                        //! specified frameHandle.
  //!  \remark    Detaches from any previously attached frame handle.

  bool anyWidgetHasFocus();
signals:

  void frameSwitched();

public slots:

  void setFrame(
      int frame,
      bool notifyFrameHandler);  //!< Sets the navigator's current frame.
                                 //!  \deprecated  Remove the bool.
  void prevFrame() {
    setFrame(m_frame - 1, true);
  }  //!< Move to previous frame.
  void nextFrame() { setFrame(m_frame + 1, true); }  //!< Move to next frame.

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  void updateFrame(int frame);  //!< Changes frame without emitting any signal
                                //! and notifying the frameHandle.

protected slots:

  /*!
\details  Copies the value in the frame's line edit widget to the internal
current frame value.
*/
  void onEditingFinished();  //!< Slot invoked whenever current frame's text
                             //! editing is finished.
  void onFrameSwitched();
};

#endif  // FRAMENVIGATORTOOLBAR_H
