#pragma once

#ifndef DVSCROLLWIDGET_H
#define DVSCROLLWIDGET_H

// TnzCore includes
#include "tcommon.h"

// Qt includes
#include <QFrame>
#include <QEasingCurve>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class FreeLayout;
class QPropertyAnimation;
class QPushButton;
class QTimer;

//====================================================

//****************************************************************************
//    DvScrollWidget class
//****************************************************************************

/*!
  \brief    The DvScrollWidget class is a QScrollArea-like container
            widget for monodirectional scrolling.

  \details  Unlike a QScrollArea, scrolling is enforced using directional
  buttons
            at the side of the content whose activation performs animated
  scrolling
            by half a widget in the represented direction.
            Alternatively, free-scroll is available by default where mouse
  events
            are un-accepted by the content's child widgets.

            The push buttons at the side of the content appear only when there
  is
            available content space in that direction. They are \a empty by
  default,
            but can be filled in or styled with a style sheet.
            In the latter case, they are given an object name dependant on the
            scroll orientation - in the form
  <TT>Scroll<Left/Right/Up/Down>Button</TT>.

  \note     The default size QSizePolicy for DvScrollWidget is \p Preferred in
            the orientation direction, and \p Fixed in the other.

  \sa       The FreeLayout class.
*/
class DVAPI DvScrollWidget final : public QFrame {
  Q_OBJECT

  QWidget *m_content;
  QPushButton *m_scrollBackward, *m_scrollForward;

  QPropertyAnimation *m_animation;
  QEasingCurve m_clickEase, m_holdEase;
  QTimer *m_backwardTimer, *m_forwardTimer;

  int m_mousePos;

  bool m_horizontal, m_pressed, m_heldRelease, m_heldClick;

public:
  DvScrollWidget(QWidget *parent = 0, Qt::Orientation = Qt::Horizontal);
  ~DvScrollWidget() {}

  /*! \remark Ownership is \a transferred to this class. Any previously set
     widget
        is deleted before being replaced. */

  void setWidget(QWidget *widget);  //!< Sets the scrollable content widget.

  /*! \note   Function setOrientation() restores the widget's <I>size policy</I>
     to
        \p Preferred in the specified direction, and \p Fixed in the other. */
  void setOrientation(Qt::Orientation orientation);
  Qt::Orientation getOrientation() const;

  void setEasing(QEasingCurve clickEase, QEasingCurve holdPressEase);

  void scroll(int dx, int duration = 0,
              QEasingCurve easing = QEasingCurve(QEasingCurve::OutCubic));
  void scrollTo(int pos, int duration = 0,
                QEasingCurve easing = QEasingCurve(QEasingCurve::OutCubic));

public slots:

  void scrollBackward();
  void scrollForward();

protected:
  void resizeEvent(QResizeEvent *re) override;
  void mousePressEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;
  void showEvent(QShowEvent *se) override;

protected slots:

  void updateButtonsVisibility();

  void holdBackward();
  void holdForward();

  void releaseBackward();
  void releaseForward();
};

#endif  // DVSCROLLWIDGET_H
