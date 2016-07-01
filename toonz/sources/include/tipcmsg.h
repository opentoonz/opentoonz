#pragma once

#ifndef TIPC_MESSAGE_H
#define TIPC_MESSAGE_H

// Toonz includes
#include "tcommon.h"

// Qt includes
#include <QDataStream>
#include <QLocalSocket>

#include "tipc.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace tipc {

//********************************************************
//    TIPC Message Parser declaration
//********************************************************

class MessageParser {
  friend class Server;

  QLocalSocket *m_socket;
  tipc::Stream *m_stream;

public:
  virtual QString header() const        = 0;
  virtual void operator()(Message &msg) = 0;

  QLocalSocket *socket() { return m_socket; }
  tipc::Stream *stream() { return m_stream; }
};

//********************************************************
//    Default Message Types declaration
//********************************************************

enum DefMsgEnum {
  SHMEM_REQUEST,
  SHMEM_RELEASE,
  TMPFILE_REQUEST,
  TMPFILE_RELEASE,
  QUIT_ON_ERROR
};

template <DefMsgEnum msgType>
class DVAPI DefaultMessageParser final : public MessageParser {
public:
  QString header() const override;
  void operator()(Message &msg) override;
};

//------------------------------------------------------------------------------

/* Default commands syntaxes:

SHMEM_REQUEST

  Syntax: $shmem_request <shmem id> <nbytes>
  Reply: ok | err

SHMEM_RELEASE

  Syntax: $shmem_release <shmem id>
  Reply: ok | err

TMPFILE_REQUEST

  Syntax: $tmpfile_request <file id>
  Reply: ok <file path>\n | err

TMPFILE_RELEASE

  Syntax: $tmpfile_release <file id>
  Reply: ok | err

QUIT_ON_ERROR

  Syntax: $quit_on_error
  Reply: ok

*/

//------------------------------------------------------------------------------

}  // namespace tipc

#endif  // TIPC_MESSAGE_H
