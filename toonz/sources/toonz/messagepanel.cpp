

#include "messagepanel.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "mainwindow.h"

#include <QTextEdit>
#include <QPainter>
#include <QFontMetrics>

//==================================
//    Centered Text Widget class
//----------------------------------

class CenteredTextWidget final : public QWidget {
  QString m_text;

public:
  CenteredTextWidget(QWidget *parent = 0) : QWidget(parent) {}

  void setText(const QString &text) { m_text = text; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);

    QFontMetrics fm    = p.fontMetrics();
    QString elidedText = fm.elidedText(m_text, Qt::ElideRight, width());
    qreal textWidth    = fm.width(elidedText);
    p.drawText((width() - textWidth) * 0.5, (height() - fm.height()) * 0.5,
               elidedText);
  }
};

//==============================
//    MessagePanel methods
//------------------------------

/*! \class MessagePanel
                \brief MessagePanel class provides a pane to view a single,
   centered text message.

                Inherits \b TPanel.
*/

MessagePanel::MessagePanel(QWidget *parent) : TPanel(parent) {
  setPanelType("Message");
  setWindowTitle("Message");

  m_messageBox = new CenteredTextWidget(this);
  setWidget(m_messageBox);
}

//-----------------------------------------------------------------------------

MessagePanel::~MessagePanel() {}

//-----------------------------------------------------------------------------

void MessagePanel::setViewType(int viewType) { m_viewType = viewType; }

//-----------------------------------------------------------------------------

int MessagePanel::getViewType() { return m_viewType; }

//-----------------------------------------------------------------------------

void MessagePanel::setPoolIndex(int poolIndex) { m_poolIndex = poolIndex; }

//-----------------------------------------------------------------------------

int MessagePanel::getPoolIndex() const { return m_poolIndex; }

//-----------------------------------------------------------------------------

void MessagePanel::setMessage(QString text) { m_messageBox->setText(text); }

//=============================================================================
/*! \class LogPanel
                \brief The LogPanel class provide a pane to log text messages in
   a QTextEdit document.

                Inherits \b TPanel.
*/
#if QT_VERSION >= 0x050500
LogPanel::LogPanel(QWidget *parent, Qt::WindowFlags flags)
#else
LogPanel::LogPanel(QWidget *parent, Qt::WFlags flags)
#endif
    : TPanel(parent), TLogger::Listener() {
  TLogger::instance()->addListener(this);
  TLogger::instance()->clearMessages();
  setPanelType("Log");
  setIsMaximizable(false);
  setWindowTitle("Log");

  m_messageBox = new QTextEdit(this);
  m_messageBox->setObjectName("LogPanel");
  m_messageBox->setReadOnly(true);
  m_messageBox->setMinimumSize(200, 150);

  setWidget(m_messageBox);

  connect(TApp::instance()->getCurrentScene(), SIGNAL(sceneSwitched()), this,
          SLOT(clear()));
}

//-----------------------------------------------------------------------------

LogPanel::~LogPanel() { TLogger::instance()->removeListener(this); }

//-----------------------------------------------------------------------------

void LogPanel::onLogChanged() {
  TLogger *logger  = TLogger::instance();
  int messageCount = logger->getMessageCount();
  if (!messageCount) return;
  TLogger::Message message  = logger->getMessage(messageCount - 1);
  TLogger::MessageType type = message.getType();

  m_messageBox->moveCursor(QTextCursor::End);

  QString strCount;
  ;
  m_messageBox->insertPlainText(strCount.setNum(messageCount) + QString("> "));

  if (type == TLogger::Debug) {
    m_messageBox->setTextColor(Qt::blue);
    m_messageBox->insertPlainText(QString("Debug: "));
  } else if (type == TLogger::Info) {
    m_messageBox->setTextColor(QColor(0, 200, 0));
    m_messageBox->insertPlainText(QString("Info: "));
  } else if (type == TLogger::Warning) {
    m_messageBox->setTextColor(QColor(255, 200, 0));
    m_messageBox->insertPlainText(QString("Warning: "));
  } else if (type == TLogger::Error) {
    m_messageBox->setTextColor(Qt::red);
    m_messageBox->insertPlainText(QString("Error: "));
  }

  m_messageBox->setTextColor(Qt::black);
  m_messageBox->insertPlainText(QString::fromStdString(message.getText()) +
                                QString("\n"));
}

//-----------------------------------------------------------------------------
/*! Clear panel text box from all messages.
*/
void LogPanel::clear() {
  TLogger::instance()->clearMessages();
  m_messageBox->clear();
}

//=============================================================================
// OpenFloatingLogPanel

class OpenFloatingLogPanel final : public MenuItemHandler {
public:
  OpenFloatingLogPanel() : MenuItemHandler(MI_OpenMessage) {}
  void execute() override {
    TMainWindow *currentRoom = TApp::instance()->getCurrentRoom();
    if (currentRoom) {
      QList<TPanel *> list = currentRoom->findChildren<TPanel *>();
      for (int i = 0; i < list.size(); i++) {
        TPanel *pane = list.at(i);
        // If the pane is hidden, floating and have the same name
        if (pane->isHidden() && pane->isFloating() &&
            pane->getPanelType() == "Message") {
          pane->show();
          pane->raise();
          return;
        }
      }

      TPanel *pane = TPanelFactory::createPanel(currentRoom, "Message");
      pane->setFloating(true);
      pane->show();
      pane->raise();
    }
  }
} openFloatingLogPanelCommand;
