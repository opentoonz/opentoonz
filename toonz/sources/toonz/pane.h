#pragma once

#ifndef PANE_H
#define PANE_H

// TODO: cambiare il nome del file in tpanel.h

//#include <QDockWidget>
#include "../toonzqt/tdockwindows.h"

class TPanelTitleBarButtonSet;
class Room;

//! icon buttons placed on the panel titlebar (cfr. viewerpane.h)
class TPanelTitleBarButton : public QWidget {
  Q_OBJECT
  QPixmap m_standardPixmap, m_rolloverPixmap, m_pressedPixmap;
  bool m_rollover;
  TPanelTitleBarButtonSet *m_buttonSet;
  int m_id;

protected:
  bool m_pressed;

public:
  TPanelTitleBarButton(QWidget *parent, const QString &standardPixmapName,
                       const QString &rolloverPixmapName,
                       const QString &pressedPixmapName);

  //! call this method to make a radio button. id is the button identifier
  void setButtonSet(TPanelTitleBarButtonSet *buttonSet, int id);
  int getId() const { return m_id; }

public slots:
  void setPressed(bool pressed);  // n.b. doesn't emit signals. calls update()

protected:
  void paintEvent(QPaintEvent *event);

  void mouseMoveEvent(QMouseEvent *event);
  void enterEvent(QEvent *);
  void leaveEvent(QEvent *);
  virtual void mousePressEvent(QMouseEvent *event);

signals:
  //! emitted when the user press the button
  //! n.b. the signal is not emitted if the button is part of a buttonset
  void toggled(bool pressed);
};

//-----------------------------------------------------------------------------
/*! specialized button for sage area which enables to choose safe area size by
 * context menu
*/

class TPanelTitleBarButtonForSafeArea : public TPanelTitleBarButton {
  Q_OBJECT
public:
  TPanelTitleBarButtonForSafeArea(QWidget *parent,
                                  const QString &standardPixmapName,
                                  const QString &rolloverPixmapName,
                                  const QString &pressedPixmapName)
      : TPanelTitleBarButton(parent, standardPixmapName, rolloverPixmapName,
                             pressedPixmapName) {}
  void getSafeAreaNameList(QList<QString> &nameList);

protected:
  void contextMenuEvent(QContextMenuEvent *event);
  void mousePressEvent(QMouseEvent *event);
protected slots:
  void onSetSafeArea();
};

//-----------------------------------------------------------------------------

//! a buttonset can group different TPanelTitleBarButton
class TPanelTitleBarButtonSet : public QObject {
  Q_OBJECT
  std::vector<TPanelTitleBarButton *> m_buttons;

public:
  TPanelTitleBarButtonSet();
  ~TPanelTitleBarButtonSet();

  void add(TPanelTitleBarButton *button);
  void select(TPanelTitleBarButton *button);

signals:
  //! emitted when the current button changes. id is the button identifier
  void selected(int id);
};

//-----------------------------------------------------------------------------

class TPanelTitleBar : public QFrame {
  Q_OBJECT

  bool m_isActive;
  bool m_closeButtonHighlighted;
  std::vector<std::pair<QPoint, QWidget *>> m_buttons;

public:
  TPanelTitleBar(QWidget *parent                      = 0,
                 TDockWidget::Orientation orientation = TDockWidget::vertical);

  QSize sizeHint() const { return minimumSizeHint(); }
  QSize minimumSizeHint() const;

  void setIsActive(bool value);
  bool isActive() { return m_isActive; }

  // pos = widget position. n.b. if pos.x()<0 then origin is topright corner
  void add(const QPoint &pos, QWidget *widget);

protected:
  void resizeEvent(QResizeEvent *e);

  // To Disable the default context Menu
  void contextMenuEvent(QContextMenuEvent *) {}

  void paintEvent(QPaintEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseDoubleClickEvent(QMouseEvent *);

signals:

  void closeButtonPressed();
  void doubleClick(QMouseEvent *me);
};

//-----------------------------------------------------------------------------

class TPanel : public TDockWidget {
  Q_OBJECT

  QColor
      m_bgcolor;  // overrides palette background color in paint event - Mac fix

  Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)

  QColor getBGColor() const { return m_bgcolor; }
  void setBGColor(const QColor &color) { m_bgcolor = color; }

  std::string m_panelType;
  bool m_isMaximizable;
  bool m_isMaximized;
  bool m_isActive;
  bool m_multipleInstancesAllowed;

  TPanelTitleBar *m_panelTitleBar;

  QList<TPanel *> m_hiddenDockWidgets;
  QByteArray m_currentRoomOldState;

public:
  TPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0,
         TDockWidget::Orientation orientation = TDockWidget::vertical);
  ~TPanel();

  void setPanelType(const std::string &panelType) { m_panelType = panelType; }
  std::string getPanelType() { return m_panelType; }

  void setIsMaximizable(bool value) { m_isMaximizable = value; }
  bool isMaximizable() { return m_isMaximizable; }
  // bool isMaximized() { return m_isMaximized; }
  // void setMaximized(bool isMaximized, Room *room = 0);

  QList<TPanel *> getHiddenDockWidget() const { return m_hiddenDockWidgets; }
  QByteArray getSavedOldState() const { return m_currentRoomOldState; }

  bool isActive() { return m_isActive; }
  void setActive(bool value);

  // void setTitleBarWidget(TPanelTitleBar *newTitleBar);

  // si riferisce a istanze multiple dei pannelli floating; default = true
  void allowMultipleInstances(bool allowed) {
    m_multipleInstancesAllowed = allowed;
  }
  bool areMultipleInstancesAllowed() const {
    return m_multipleInstancesAllowed;
  }

  TPanelTitleBar *getTitleBar() const { return m_panelTitleBar; }

  virtual void reset(){};

  virtual int getViewType() { return -1; };
  virtual void setViewType(int viewType){};

  virtual bool widgetInThisPanelIsFocused() {
    // by default, chech if the panel content itself has focus
    if (widget())
      return widget()->hasFocus();
    else
      return false;
  };

protected:
  void paintEvent(QPaintEvent *);
  void showEvent(QShowEvent *);
  void hideEvent(QHideEvent *);
  void enterEvent(QEvent *);
  void leaveEvent(QEvent *);

  virtual bool isActivatableOnEnter() { return false; }

protected slots:

  void onCloseButtonPressed();
  virtual void widgetFocusOnEnter() {
    // by default, focus the panel content
    if (widget()) widget()->setFocus();
  };
  virtual void widgetClearFocusOnLeave() {
    if (widget()) widget()->clearFocus();
  };

signals:

  void doubleClick(QMouseEvent *me);
  void closeButtonPressed();
};

//-----------------------------------------------------------------------------

class TPanelFactory {
  QString m_panelType;
  static QMap<QString, TPanelFactory *> m_table;

public:
  TPanelFactory(QString panelType);
  ~TPanelFactory();

  QString getPanelType() const { return m_panelType; }

  virtual void initialize(TPanel *panel) = 0;
  virtual TPanel *createPanel(QWidget *parent);
  static TPanel *createPanel(QWidget *parent, QString panelType);
};

//-----------------------------------------------------------------------------

#endif  // PANE_H
