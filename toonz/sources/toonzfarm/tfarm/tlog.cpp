

#include "tlog.h"
//#include "tfilepath.h"
#include "tfilepath_io.h"
#include <QDateTime>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#else
#include <syslog.h>
#include <unistd.h>
#endif

#include "tthreadmessage.h"
#include "tsystem.h"
#include <fstream>

using namespace TSysLog;
namespace {

enum LEVEL { LEVEL_SUCCESS, LEVEL_ERROR, LEVEL_WARNING, LEVEL_INFO };

#ifdef _WIN32
WORD Level2WinEventType(LEVEL level) {
  switch (level) {
  case LEVEL_SUCCESS:
    return EVENTLOG_SUCCESS;  // Success event
  case LEVEL_ERROR:
    return EVENTLOG_ERROR_TYPE;  // Error event
  case LEVEL_WARNING:
    return EVENTLOG_WARNING_TYPE;  // Warning event
  case LEVEL_INFO:
    return EVENTLOG_INFORMATION_TYPE;  // Information event
  //      case : return EVENTLOG_AUDIT_SUCCESS Success audit event
  //      case : return EVENTLOG_AUDIT_FAILURE Failure audit event
  default:
    return LEVEL_WARNING;
  }
}
#else
int Level2XPriority(LEVEL level) {
  switch (level) {
  case LEVEL_SUCCESS:
    return LOG_INFO;
  case LEVEL_ERROR:
    return LOG_ERR;  // Errors.
  case LEVEL_WARNING:
    return LOG_WARNING;  // Warning messages.
  case LEVEL_INFO:
    return LOG_INFO;  // Informational messages.
  default:
    return LEVEL_WARNING;
  }
}
#endif

void notify(LEVEL level, const QString &msg) {
#ifdef _WIN32
  TCHAR buf[_MAX_PATH + 1];

  GetModuleFileName(0, buf, _MAX_PATH);

  HANDLE handle =
      RegisterEventSource(NULL,  // uses local computer
                          TFilePath(buf).getName().c_str());  // source name

  LPCTSTR lpszStrings[2];
  TCHAR szMsg[256];
  DWORD dwErr = 1;
  _stprintf(szMsg, TEXT("%s error: %d"), "appname", dwErr);

  lpszStrings[0] = lpszStrings[1] = (LPCTSTR)msg.data();
  ReportEvent(handle,                     // event log handle
              Level2WinEventType(level),  // event type
              0,                          // category zero
              dwErr,                      // event identifier
              NULL,                       // no user security identifier
              1,                          // one substitution string
              0,                          // no data
              lpszStrings,                // pointer to string array
              NULL);                      // pointer to data

  DeregisterEventSource(handle);

#else
  std::string str = msg.toStdString();
  syslog(Level2XPriority(level), "%s", str.c_str());
#endif
}

static TThread::Mutex MyMutex;
}  // namespace

//------------------------------------------------------------------------------

void TSysLog::success(const QString &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_SUCCESS, msg);
}

//------------------------------------------------------------------------------

void TSysLog::warning(const QString &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_WARNING, msg);
}

//------------------------------------------------------------------------------

void TSysLog::error(const QString &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_ERROR, msg);
}

//------------------------------------------------------------------------------

void TSysLog::info(const QString &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_INFO, msg);
}

//==============================================================================

class TUserLog::Imp {
public:
  Imp() : m_os(&std::cout), m_streamOwner(false) {}

  Imp(const TFilePath &fp) : m_os(new Tofstream(fp)), m_streamOwner(true) {}

  ~Imp() {
    if (m_streamOwner) delete m_os;
  }

  void write(const QString &msg);

  TThread::Mutex m_mutex;
  std::ostream *m_os;
  bool m_streamOwner;
};

//------------------------------------------------------------------------------

void TUserLog::Imp::write(const QString &msg) {
  QMutexLocker sl(&MyMutex);
  *m_os << msg.toStdString();
  m_os->flush();
}

//--------------------------------------------------------------------

namespace {

//--------------------------------------------------------------------

QString myGetCurrentTime() { return QDateTime::currentDateTime().toString(); }

}  // namespace

//------------------------------------------------------------------------------

TUserLog::TUserLog() : m_imp(new Imp()) {}

//------------------------------------------------------------------------------

TUserLog::TUserLog(const TFilePath &fp) : m_imp(new Imp(fp)) {}

//------------------------------------------------------------------------------

TUserLog::~TUserLog() {}

//------------------------------------------------------------------------------

void TUserLog::warning(const QString &msg) {
  QString fullMsg(myGetCurrentTime());
  fullMsg += " WRN:";
  fullMsg += "\n";
  fullMsg += msg;
  fullMsg += "\n";
  m_imp->write(fullMsg);
}

//------------------------------------------------------------------------------

void TUserLog::error(const QString &msg) {
  QString fullMsg(myGetCurrentTime());
  fullMsg += " ERR:";
  fullMsg += "\n";
  fullMsg += msg;
  fullMsg += "\n";
  m_imp->write(fullMsg);
}

//------------------------------------------------------------------------------

void TUserLog::info(const QString &msg) {
  QString fullMsg(myGetCurrentTime());
  fullMsg += " INF:";
  fullMsg += "\n";
  fullMsg += msg;
  fullMsg += "\n";
  m_imp->write(fullMsg);
}
