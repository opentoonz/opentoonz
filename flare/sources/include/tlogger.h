#pragma once

#ifndef TLOGGER_INCLUDED
#define TLOGGER_INCLUDED

#include <memory>

//
// TLogger
//
// usage example:
//
//    TFilePath fp;
//    TLogger::info() << "Loading " << fp << " ...";
//    TLogger::error() << "File " << fp << "not found";
//
// N.B.
//    TLogger::debug() << ....
//    is automatically removed in the release
//

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

class TFilePath;

class DVAPI TLogger {  // singleton
  class Imp;
  std::unique_ptr<Imp> m_imp;

  TLogger();

public:
  ~TLogger();

  static TLogger *instance();

  enum MessageType { Debug = 1, Info, Warning, Error };

  class DVAPI Message {
    MessageType m_type;
    std::string m_timestamp;
    std::string m_text;

  public:
    Message(MessageType type, std::string text);
    MessageType getType() const { return m_type; }
    std::string getTimestamp() const { return m_timestamp; }
    std::string getText() const { return m_text; }
  };

  class Listener {
  public:
    virtual void onLogChanged() = 0;
    virtual ~Listener() {}
  };

  void addMessage(const Message &msg);
  void clearMessages();
  int getMessageCount() const;
  Message getMessage(int index) const;

  void addListener(Listener *listener);
  void removeListener(Listener *listener);

  class DVAPI Stream {
    MessageType m_type;
    std::string m_text;

  public:
    Stream(MessageType type);
    ~Stream();

    Stream &operator<<(std::string v);
    Stream &operator<<(int v);
    Stream &operator<<(double v);
    Stream &operator<<(const TFilePath &v);
  };

  class DVAPI NullStream {
  public:
    NullStream() {}
    ~NullStream() {}

    NullStream &operator<<(std::string) { return *this; }
    NullStream &operator<<(int) { return *this; }
    NullStream &operator<<(double) { return *this; }
    NullStream &operator<<(const TFilePath &) { return *this; }
  };

#ifdef NDEBUG
  static NullStream debug() { return NullStream(); }
#else
  static Stream debug() { return Stream(Debug); }
#endif
  static Stream info() { return Stream(Info); }
  static Stream warning() { return Stream(Warning); }
  static Stream error() { return Stream(Error); }
};

#endif
