

#include "toonz/tlog.h"
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
#include "tmsgcore.h"

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

void notify(LEVEL level, const std::string &msg) {
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

  lpszStrings[0] = lpszStrings[1] = msg.c_str();
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
  syslog(Level2XPriority(level), "%s", msg.c_str());
#endif
}

static TThread::Mutex MyMutex;
}  // namespace

//------------------------------------------------------------------------------

void TSysLog::success(const std::string &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_SUCCESS, msg);
}

//------------------------------------------------------------------------------

void TSysLog::warning(const std::string &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_WARNING, msg);
}

//------------------------------------------------------------------------------

void TSysLog::error(const std::string &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_ERROR, msg);
}

//------------------------------------------------------------------------------

void TSysLog::info(const std::string &msg) {
  QMutexLocker sl(&MyMutex);
  notify(LEVEL_INFO, msg);
}

//==============================================================================

class TUserLogAppend::Imp {
public:
  Imp() : m_os(&std::cout), m_streamOwner(false) {}

  Imp(const TFilePath &fp) : m_streamOwner(true) {
    TFileStatus fs(fp);
    if (fs.doesExist())
      m_os = new Tofstream(fp, true);
    else
      m_os = new Tofstream(fp);
  }

  ~Imp() {
    if (m_streamOwner) delete m_os;
  }

  void write(const std::string &msg);

  TThread::Mutex m_mutex;
  std::ostream *m_os;
  bool m_streamOwner;
};

//------------------------------------------------------------------------------

void TUserLogAppend::Imp::write(const std::string &msg) {
  QMutexLocker sl(&m_mutex);
  *m_os << msg.c_str();
  m_os->flush();
}

//--------------------------------------------------------------------

namespace {

//--------------------------------------------------------------------

std::string myGetCurrentTime() {
  QString tmp = QTime::currentTime().toString("hh:mm:ss");
  return tmp.toStdString();
}

}  // namespace

//------------------------------------------------------------------------------

TUserLogAppend::TUserLogAppend() : m_imp(new Imp()) {}

//------------------------------------------------------------------------------

TUserLogAppend::TUserLogAppend(const TFilePath &fp) : m_imp(new Imp(fp)) {}

//------------------------------------------------------------------------------

TUserLogAppend::~TUserLogAppend() {}

//------------------------------------------------------------------------------

void TUserLogAppend::warning(const std::string &msg) {
  DVGui::warning(QString::fromStdString(msg));

  std::string fullMsg(myGetCurrentTime());
  fullMsg += " WRN:";
  fullMsg += "\n";
  fullMsg += msg;
  fullMsg += "\n";
  m_imp->write(fullMsg);
}

//------------------------------------------------------------------------------

void TUserLogAppend::error(const std::string &msg) {
  DVGui::error(QString::fromStdString(msg));
  std::string fullMsg(myGetCurrentTime());
  fullMsg += " ERR:";
  fullMsg += "\n";
  fullMsg += msg;
  fullMsg += "\n";
  m_imp->write(fullMsg);
}

//------------------------------------------------------------------------------

void TUserLogAppend::info(const std::string &msg) {
  std::string fullMsg("");
  // fullMsg += " INF:";
  // fullMsg += "\n";
  fullMsg += msg;
  fullMsg += "\n";
  m_imp->write(fullMsg);
}
