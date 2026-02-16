#pragma once

#ifndef PANE_H
#define PANE_H

// TODO: change the file name to tpanel.h

#include "../toonzqt/tdockwindows.h"

#include <QMap>
#include <QStringList>
#include <QtGlobal>
#include <QColor>
#include <vector>
#include <utility>

class TPanelTitleBarButtonSet;
class TPanelTitleBarButton;
class Room;

//-----------------------------------------------------------------------------
//! icon buttons placed on the panel titlebar (cfr. viewerpane.h)
class TPanelTitleBarButton : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QColor OffColor READ getOffColor WRITE setOffColor)
  Q_PROPERTY(QColor OverColor READ getOverColor WRITE setOverColor)
  Q_PROPERTY(QColor PressedColor READ getPressedColor WRITE setPressedColor)
  Q_PROPERTY(QColor FreezeColor READ getFreezeColor WRITE setFreezeColor)
  Q_PROPERTY(QColor PreviewColor READ getPreviewColor WRITE setPreviewColor)

public:
  explicit TPanelTitleBarButton(QWidget *parent,
                                const QString &standardPixmapName);
  ~TPanelTitleBarButton() override = default;

  // Prohibit copy and move (Qt doesn't support move for QWidget)
  TPanelTitleBarButton(const TPanelTitleBarButton &)            = delete;
  TPanelTitleBarButton &operator=(const TPanelTitleBarButton &) = delete;
  TPanelTitleBarButton(TPanelTitleBarButton &&)                 = delete;
  TPanelTitleBarButton &operator=(TPanelTitleBarButton &&)      = delete;

  //! call this method to make a radio button. id is the button identifier
  void setButtonSet(TPanelTitleBarButtonSet *buttonSet, int id);
  int getId() const noexcept { return m_id; }

  // Stylesheet properties
  void setOffColor(const QColor &color);
  QColor getOffColor() const;

  void setOverColor(const QColor &color);
  QColor getOverColor() const;

  void setPressedColor(const QColor &color);
  QColor getPressedColor() const;

  void setFreezeColor(const QColor &color);
  QColor getFreezeColor() const;

  void setPreviewColor(const QColor &color);
  QColor getPreviewColor() const;

public slots:
  // n.b. doesn't emit signals, only calls update()
  void setPressed(bool pressed);

signals:
  //! emitted when the user press the button
  //! n.b. the signal is not emitted if the button is part of a buttonset
  void toggled(bool pressed);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;
  void mousePressEvent(QMouseEvent *event) override;

  QString m_standardPixmapName;
  QColor m_offColor;
  QColor m_overColor;
  QColor m_pressedColor;
  QColor m_freezeColor;
  QColor m_previewColor;
  QSize m_baseSize;
  TPanelTitleBarButtonSet *m_buttonSet;
  int m_id;
  bool m_rollover;
  bool m_pressed;
};

//-----------------------------------------------------------------------------
/*! specialized button for Bind to Room feature with a simple circle indicator
 */
class TPanelTitleBarButtonForBindToRoom final : public TPanelTitleBarButton {
  Q_OBJECT
  Q_DISABLE_COPY(TPanelTitleBarButtonForBindToRoom)

public:
  explicit TPanelTitleBarButtonForBindToRoom(QWidget *parent);
  ~TPanelTitleBarButtonForBindToRoom() override = default;

protected:
  void paintEvent(QPaintEvent *) override;
};

//-----------------------------------------------------------------------------
/*! specialized button for safe area which enables to choose safe area size by
 * context menu
 */

class TPanelTitleBarButtonForLayoutGuide final : public TPanelTitleBarButton {
  Q_OBJECT
  Q_DISABLE_COPY(TPanelTitleBarButtonForLayoutGuide)

public:
  TPanelTitleBarButtonForLayoutGuide(QWidget *parent,
                                     const QString &standardPixmapName);

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

protected slots:
  void onSetLayout();
  void onEditLayouts();
  void onAddToXsheet();
};

//-----------------------------------------------------------------------------
/*! specialized button for preview which enables to choose preview behavior by
 * context menu
 */
class TPanelTitleBarButtonForPreview final : public TPanelTitleBarButton {
  Q_OBJECT
  Q_DISABLE_COPY(TPanelTitleBarButtonForPreview)

public:
  explicit TPanelTitleBarButtonForPreview(QWidget *parent,
                                          const QString &standardPixmapName);
  ~TPanelTitleBarButtonForPreview() override = default;

  bool isChecked() const noexcept { return m_pressed; }

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

protected slots:
  void onSetPreviewBehavior();
};

//-----------------------------------------------------------------------------
//! a buttonset can group different TPanelTitleBarButton
class TPanelTitleBarButtonSet final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(TPanelTitleBarButtonSet)

public:
  TPanelTitleBarButtonSet();
  ~TPanelTitleBarButtonSet() override = default;

  void add(TPanelTitleBarButton *button);
  void select(TPanelTitleBarButton *button);

signals:
  //! emitted when the current button changes. id is the button identifier
  void selected(int id);

private:
  std::vector<TPanelTitleBarButton *> m_buttons;
};

//-----------------------------------------------------------------------------
class TPanelTitleBar final : public QFrame {
  Q_OBJECT
  Q_DISABLE_COPY(TPanelTitleBar)

  Q_PROPERTY(QColor TitleColor READ getTitleColor WRITE setTitleColor)
  Q_PROPERTY(QColor ActiveTitleColor READ getActiveTitleColor WRITE
                 setActiveTitleColor)
  Q_PROPERTY(
      QColor CloseOverColor READ getCloseOverColor WRITE setCloseOverColor)

public:
  explicit TPanelTitleBar(
      QWidget *parent                      = nullptr,
      TDockWidget::Orientation orientation = TDockWidget::vertical);
  ~TPanelTitleBar() override = default;

  QSize sizeHint() const override { return minimumSizeHint(); }
  QSize minimumSizeHint() const override;

  // pos = widget position. n.b. if pos.x()<0 then origin is top‑right corner
  void add(const QPoint &pos, QWidget *widget);

  QColor getTitleColor() const noexcept { return m_titleColor; }
  void setTitleColor(const QColor &color) noexcept { m_titleColor = color; }

  QColor getActiveTitleColor() const noexcept { return m_activeTitleColor; }
  void setActiveTitleColor(const QColor &color) noexcept {
    m_activeTitleColor = color;
  }

  QColor getCloseOverColor() const;
  void setCloseOverColor(const QColor &color);

signals:
  void closeButtonPressed();
  void doubleClick(QMouseEvent *me);

protected:
  void resizeEvent(QResizeEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *) override {
  }  // disable default menu
  void paintEvent(QPaintEvent *event) override;
  void leaveEvent(QEvent *) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;

private:
  bool m_closeButtonHighlighted;
  std::vector<std::pair<QPoint, QWidget *>> m_buttons;
  QColor m_titleColor, m_activeTitleColor, m_closeOverColor;
};

//-----------------------------------------------------------------------------
class TPanel : public TDockWidget {
  Q_OBJECT
  Q_DISABLE_COPY(TPanel)

  Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)

public:
  explicit TPanel(QWidget *parent                      = nullptr,
                  Qt::WindowFlags flags                = Qt::WindowFlags(),
                  TDockWidget::Orientation orientation = TDockWidget::vertical);
  ~TPanel() override;

  // Panel type identification
  void setPanelType(const std::string &panelType) { m_panelType = panelType; }
  std::string getPanelType() const { return m_panelType; }

  // Maximize support
  void setIsMaximizable(bool value) noexcept { m_isMaximizable = value; }
  bool isMaximizable() const noexcept { return m_isMaximizable; }

  // Hidden dock widgets management
  QList<TPanel *> getHiddenDockWidget() const { return m_hiddenDockWidgets; }
  QByteArray getSavedOldState() const { return m_currentRoomOldState; }

  // Multiple instances of floating panels; default = true
  void allowMultipleInstances(bool allowed) noexcept {
    m_multipleInstancesAllowed = allowed;
  }
  bool areMultipleInstancesAllowed() const noexcept {
    return m_multipleInstancesAllowed;
  }

  // Title bar access
  TPanelTitleBar *getTitleBar() const noexcept { return m_panelTitleBar; }

  // Room binding methods (for all panels)
  bool isRoomBound() const noexcept { return m_isRoomBound; }
  void setRoomBound(bool bound);
  QString getBoundRoomName() const { return m_boundRoomName; }
  void setBoundRoomName(const QString &roomName);
  void setRoomBindButton(TPanelTitleBarButton *button) noexcept {
    m_roomBindButton = button;
  }

  // Add room binding toggle button to the title bar
  // This enables the "Bind to Room" feature for any panel
  void addRoomBindButton();

  // Virtuals that may be overridden
  virtual void reset() {}
  virtual int getViewType() { return -1; }
  virtual void setViewType(int viewType) {}
  virtual bool widgetInThisPanelIsFocused() {
    return widget() ? widget()->hasFocus() : false;
  }
  virtual void restoreFloatingPanelState();
  virtual void zoomContentsAndFitGeometry(bool forward);

protected:
  void paintEvent(QPaintEvent *) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;
  virtual bool isActivatableOnEnter() { return false; }

protected slots:
  void onCloseButtonPressed();
  void onCustomContextMenuRequested(const QPoint &pos);
  virtual void widgetFocusOnEnter() {
    if (widget()) widget()->setFocus();
  }
  virtual void widgetClearFocusOnLeave() {
    if (widget()) widget()->clearFocus();
  }

signals:
  void doubleClick(QMouseEvent *me);
  void closeButtonPressed();

private:
  QColor
      m_bgcolor;  // overrides palette background color in paint event - Mac fix
  std::string m_panelType;
  bool m_isMaximizable;
  bool m_isMaximized;  // currently unused
  bool m_multipleInstancesAllowed;
  TPanelTitleBar *m_panelTitleBar;
  QList<TPanel *> m_hiddenDockWidgets;
  QByteArray m_currentRoomOldState;

  // Room binding for custom panels
  bool m_isRoomBound;
  QString m_boundRoomName;
  TPanelTitleBarButton *m_roomBindButton;

  QColor getBGColor() const { return m_bgcolor; }
  void setBGColor(const QColor &color) { m_bgcolor = color; }
};

//-----------------------------------------------------------------------------
class TPanelFactory {
  Q_DISABLE_COPY(TPanelFactory)

public:
  explicit TPanelFactory(const QString &panelType);
  virtual ~TPanelFactory();

  QString getPanelType() const { return m_panelType; }

  virtual void initialize(TPanel *panel) = 0;
  virtual TPanel *createPanel(QWidget *parent);
  static TPanel *createPanel(QWidget *parent, const QString &panelType);

private:
  static QMap<QString, TPanelFactory *> &tableInstance();
  QString m_panelType;
};

//-----------------------------------------------------------------------------

#endif  // PANE_H
