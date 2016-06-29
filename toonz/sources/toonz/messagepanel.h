#pragma once

#ifndef MESSAGEPANEL_H
#define MESSAGEPANEL_H

#include "pane.h"

#include "tlogger.h"

// forward declaration
class QTextEdit;
class CenteredTextWidget;
class MainWindow;

//=============================================================================
// MessagePanel
//-----------------------------------------------------------------------------

class MessagePanel final : public TPanel {
  friend class MainWindow;

  CenteredTextWidget *m_messageBox;

  int m_viewType;
  int m_poolIndex;

public:
  MessagePanel(QWidget *parent = 0);
  ~MessagePanel();

  void setMessage(QString text);

private:
  // These methods are used to remember special panel infos
  // when a MessagePanel substitutes a panel due to license
  // limitations.
  void setViewType(int viewType) override;
  int getViewType() override;

  void setPoolIndex(int poolIndex);
  int getPoolIndex() const;
};

//=============================================================================
// MessagePanel
//-----------------------------------------------------------------------------

class LogPanel final : public TPanel, public TLogger::Listener {
  Q_OBJECT

  QTextEdit *m_messageBox;

  int m_viewType;
  int m_poolIndex;

public:
#if QT_VERSION >= 0x050500
  LogPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  LogPanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~LogPanel();

  void onLogChanged() override;

public slots:
  void clear();
};

#endif  // MESSAGEPANEL_H
