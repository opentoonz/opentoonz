#pragma once

#include <QObject>
#include <set>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QLocalServer;
class QLocalSocket;
class QTcpServer;
class QTcpSocket;

#undef ERROR

namespace DVGui {

enum MsgType {
  INFORMATION,
  WARNING,   // this one opens  a popup only if tmsg not visible
  CRITICAL,  // this one opens always a popup
  QUESTION
};

void DVAPI MsgBox(MsgType type, const QString &text);

void DVAPI error(const QString &msg);
void DVAPI warning(const QString &msg);
void DVAPI info(const QString &msg);
};

class DVAPI TMsgCore final : public QObject {
  Q_OBJECT

  QTcpServer *m_tcpServer;
  QTcpSocket *m_clientSocket;
  std::set<QTcpSocket *> m_sockets;
  void readFromSocket(QTcpSocket *socket);

public:
  TMsgCore();
  ~TMsgCore();
  static TMsgCore *instance();

  // client side
  // 'send' returns false if the tmessage is not active in the application
  // (tipically, in console applications such as tcomposer)
  bool send(DVGui::MsgType type, const QString &message);
  void connectTo(const QString &address = "");

  // server side
  bool openConnection();
  QString getConnectionName();

Q_SIGNALS:

  void sendMessage(int type, const QString &message);

public Q_SLOTS:

  void OnNewConnection();
  void OnReadyRead();
  void OnDisconnected();
};
