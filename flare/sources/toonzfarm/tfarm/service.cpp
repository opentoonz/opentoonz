

#include "service.h"
#include "tlog.h"
#include "tconvert.h"

#include "tfilepath.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <tchar.h>
#else
#include <string.h>
#include <errno.h>
#endif

#define SZDEPENDENCIES ""

#ifdef _WIN32

//------------------------------------------------------------------------------

static LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize) {
  DWORD dwRet;
  LPTSTR lpszTemp = NULL;

  dwRet = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_ARGUMENT_ARRAY,
      NULL, GetLastError(), LANG_NEUTRAL, (LPTSTR)&lpszTemp, 0, NULL);

  // supplied buffer is not long enough
  if (!dwRet || ((long)dwSize < (long)dwRet + 14))
    lpszBuf[0] = TEXT('\0');
  else {
    lpszTemp[lstrlen(lpszTemp) - 2] =
        TEXT('\0');  // remove cr and newline character
    _stprintf(lpszBuf, TEXT("%s (0x%x)"), lpszTemp, GetLastError());
  }

  if (lpszTemp) LocalFree((HLOCAL)lpszTemp);

  return lpszBuf;
}

#endif  // _WIN32

//------------------------------------------------------------------------------

std::string getLastErrorText() {
  std::string errText;
#ifdef _WIN32
  char errBuff[256];
  errText = GetLastErrorText(errBuff, sizeof(errBuff));
#else
  errText = strerror(errno);
#endif

  return errText;
}

//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================

class TService::Imp {
public:
  Imp() {}

#ifdef _WIN32
  static void WINAPI serviceMain(DWORD dwArgc, LPTSTR *lpszArgv);
  static void WINAPI serviceCtrl(DWORD dwCtrlCode);

  static BOOL WINAPI controlHandler(DWORD dwCtrlType);

  static bool reportStatusToSCMgr(long currentState, long win32ExitCode,
                                  long waitHint);

#endif

  std::string m_name;
  std::string m_displayName;
  static bool m_console;

#ifdef _WIN32
  static SERVICE_STATUS_HANDLE m_hService;
  static SERVICE_STATUS m_ssStatus;  // current status of the service
  static DWORD m_dwErr;

#endif
};

#ifdef _WIN32
SERVICE_STATUS_HANDLE TService::Imp::m_hService = 0;
SERVICE_STATUS TService::Imp::m_ssStatus;
DWORD TService::Imp::m_dwErr = 0;
#endif

bool TService::Imp::m_console = false;

//------------------------------------------------------------------------------

TService::TService(const std::string &name, const std::string &displayName)
    : m_imp(new Imp) {
  m_imp->m_name        = name;
  m_imp->m_displayName = displayName;
  m_instance           = this;
}

//------------------------------------------------------------------------------

TService::~TService() {}

//------------------------------------------------------------------------------

TService *TService::instance() { return m_instance; }

//------------------------------------------------------------------------------

TService *TService::m_instance = 0;

//------------------------------------------------------------------------------

void TService::setStatus(Status status, long exitCode, long waitHint) {
#ifdef _WIN32
  if (!isRunningAsConsoleApp())
    TService::Imp::reportStatusToSCMgr(status, exitCode, waitHint);
  else {
    if (status == Stopped) exit(1);
  }
#else
  if (status == Stopped) exit(1);
#endif
}

//------------------------------------------------------------------------------

std::string TService::getName() const { return m_imp->m_name; }

//------------------------------------------------------------------------------

std::string TService::getDisplayName() const { return m_imp->m_displayName; }

//------------------------------------------------------------------------------

#ifdef _WIN32

void WINAPI TService::Imp::serviceCtrl(DWORD dwCtrlCode) {
  // Handle the requested control code.
  //
  switch (dwCtrlCode) {
  // Stop the service.
  //
  // SERVICE_STOP_PENDING should be reported before
  // setting the Stop Event - hServerStopEvent - in
  // ServiceStop().  This avoids a race condition
  // which may result in a 1053 - The Service did not respond...
  // error.
  case SERVICE_CONTROL_STOP:
  case SERVICE_CONTROL_SHUTDOWN: {
    TService::Imp::reportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR,
                                       3000 /*1*/ /*0*/);
    TService::instance()->onStop();

    TService::Imp::reportStatusToSCMgr(SERVICE_STOPPED, NO_ERROR, 3000);
    return;
  }

  // Update the service status.
  //
  case SERVICE_CONTROL_INTERROGATE:
    break;

  // invalid control code
  //
  default:
    break;
  }

  reportStatusToSCMgr(m_ssStatus.dwCurrentState, NO_ERROR, 0);
}

//------------------------------------------------------------------------------

void WINAPI TService::Imp::serviceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
  // register our service control handler:
  //
  m_hService = RegisterServiceCtrlHandler(
      TService::instance()->getName().c_str(), TService::Imp::serviceCtrl);

  if (m_hService == 0) goto cleanup;

  // SERVICE_STATUS members that don't change in example
  //
  m_ssStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
  m_ssStatus.dwServiceSpecificExitCode = 0;

  // report the status to the service control manager.
  //
  if (!reportStatusToSCMgr(SERVICE_START_PENDING,  // service state
                           NO_ERROR,               // exit code
                           3000))                  // wait hint
    goto cleanup;

  // AGGIUNGERE INIZIALIZZAZIONE QUI

  if (!reportStatusToSCMgr(SERVICE_RUNNING,  // service state
                           NO_ERROR,         // exit code
                           0))               // wait hint
    goto cleanup;

  TService::instance()->onStart(dwArgc, lpszArgv);

cleanup:

  // try to report the stopped status to the service control manager.
  //
  if (m_hService) reportStatusToSCMgr(SERVICE_STOPPED, m_dwErr, 0);

  return;
}

//------------------------------------------------------------------------------
//
//  TService::Imp::controlHandler( DWORD dwCtrlType )
//
//  PURPOSE: Handled console control events
//
//  PARAMETERS:
//    dwCtrlType - type of control event
//
//  RETURN VALUE:
//    True - handled
//    False - unhandled
//
//  COMMENTS:
//

BOOL WINAPI TService::Imp::controlHandler(DWORD dwCtrlType) {
  switch (dwCtrlType) {
  case CTRL_CLOSE_EVENT:
  case CTRL_LOGOFF_EVENT:
  case CTRL_SHUTDOWN_EVENT:

  case CTRL_BREAK_EVENT:  // use Ctrl+C or Ctrl+Break to simulate
  case CTRL_C_EVENT:      // SERVICE_CONTROL_STOP in debug mode
    _tprintf(TEXT("Stopping %s.\n"),
             TService::instance()->getDisplayName().c_str());

    TService::instance()->onStop();
    return TRUE;
    break;
  }

  return FALSE;
}

//------------------------------------------------------------------------------
//
//  FUNCTION: ReportStatusToSCMgr()
//
//  PURPOSE: Sets the current status of the service and
//           reports it to the Service Control Manager
//
//  PARAMETERS:
//    dwCurrentState - the state of the service
//    dwWin32ExitCode - error code to report
//    dwWaitHint - worst case estimate to next checkpoint
//
//  RETURN VALUE:
//    TRUE  - success
//    FALSE - failure
//
//  COMMENTS:
//
//------------------------------------------------------------------------------

bool TService::Imp::reportStatusToSCMgr(long currentState, long win32ExitCode,
                                        long waitHint) {
  /*
  SERVICE_STATUS srvStatus;		// Declare a SERVICE_STATUS structure,
  and fill it in

  srvStatus.dwServiceType  = SERVICE_WIN32_OWN_PROCESS;	// We're a service
  running in our own process

  // Set the state of the service from the argument, and save it away
  // for future use
  _dwPrevState=_dwCurrState;
  srvStatus.dwCurrentState = _dwCurrState = dwState;

  // Which commands will we accept from the SCM? All the common ones...
  srvStatus.dwControlsAccepted =	SERVICE_ACCEPT_STOP |
                                                                                                  SERVICE_ACCEPT_PAUSE_CONTINUE |
                                                                                                  SERVICE_ACCEPT_SHUTDOWN;

  srvStatus.dwWin32ExitCode = dwExitCode;	// Set the Win32 exit code for
  the service
  srvStatus.dwServiceSpecificExitCode = 0;	// Set the service-specific exit
  code
  srvStatus.dwCheckPoint = dwProgress;	// Set the checkpoint value
  srvStatus.dwWaitHint = __WAIT_TIME_FOR_SERVICE;	// 3 second timeout for
  waits

  return SetServiceStatus( _hService, &srvStatus); // pass the structure to the
  SCM
*/

  static DWORD dwCheckPoint = 1;
  BOOL fResult              = true;

  if (!m_console)  // when running as a console application we don't report to
                   // the SCM
  {
    if (currentState == SERVICE_START_PENDING)
      m_ssStatus.dwControlsAccepted = 0;
    else
      m_ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                      SERVICE_ACCEPT_PAUSE_CONTINUE |
                                      SERVICE_ACCEPT_SHUTDOWN;
    ;

    m_ssStatus.dwCurrentState  = currentState;
    m_ssStatus.dwWin32ExitCode = win32ExitCode;
    m_ssStatus.dwWaitHint      = waitHint;

    if ((currentState == SERVICE_RUNNING) || (currentState == SERVICE_STOPPED))
      m_ssStatus.dwCheckPoint = 0;
    else
      m_ssStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the service control manager.
    //
    if (!(fResult = SetServiceStatus(m_hService, &m_ssStatus))) {
      TService::addToMessageLog(QString("Failed to set the service status"));
    }
  }
  return !!fResult;
}

#endif

//------------------------------------------------------------------------------

void TService::run(int argc, char *argv[], bool console) {
  m_imp->m_console = console;

/*
#ifdef _DEBUG
  DebugBreak();
#endif
*/

#ifdef _WIN32
  if (console) {
    _tprintf(TEXT("Starting %s.\n"),
             TService::instance()->getDisplayName().c_str());

    SetConsoleCtrlHandler(TService::Imp::controlHandler, TRUE);
    TService::instance()->onStart(argc, argv);
  } else {
    SERVICE_TABLE_ENTRY dispatchTable[2];

    std::string name = TService::instance()->getName().c_str();

    dispatchTable[0].lpServiceName = (char *)name.c_str();
    dispatchTable[0].lpServiceProc =
        (LPSERVICE_MAIN_FUNCTION)TService::Imp::serviceMain;

    dispatchTable[1].lpServiceName = NULL;
    dispatchTable[1].lpServiceProc = NULL;

    if (!StartServiceCtrlDispatcher(dispatchTable))
      TService::addToMessageLog(QString(TEXT("Service start failed.")));
  }
#else
  TService::instance()->onStart(argc, argv);
#endif
}

//------------------------------------------------------------------------------

void TService::start(const std::string &name) {}

//------------------------------------------------------------------------------

void TService::stop(const std::string &name) {}

//------------------------------------------------------------------------------

bool TService::isRunningAsConsoleApp() const { return m_imp->m_console; }

//------------------------------------------------------------------------------

void TService::install(const std::string &name, const std::string &displayName,
                       const TFilePath &appPath) {
#ifdef _WIN32
  SC_HANDLE schService;
  SC_HANDLE schSCManager;

  schSCManager = OpenSCManager(NULL,  // machine (NULL == local)
                               NULL,  // database (NULL == default)
                               SC_MANAGER_ALL_ACCESS);  // access required

  if (schSCManager) {
    schService = CreateService(
        schSCManager,                                  // SCManager database
        name.c_str(),                                  // name of service
        displayName.c_str(),                           // name to display
        SERVICE_ALL_ACCESS,                            // desired access
        SERVICE_WIN32_OWN_PROCESS,                     // service type
        SERVICE_DEMAND_START,                          // start type
        SERVICE_ERROR_NORMAL,                          // error control type
        ::to_string(appPath.getWideString()).c_str(),  // service's binary
        NULL,                                          // no load ordering group
        NULL,                                          // no tag identifier
        TEXT(SZDEPENDENCIES),                          // dependencies
        NULL,                                          // LocalSystem account
        NULL);                                         // no password

    if (schService) {
      _tprintf(TEXT("%s installed.\n"), displayName.c_str());
      CloseServiceHandle(schService);
    } else {
      _tprintf(TEXT("CreateService failed - %s\n"), getLastErrorText().c_str());
    }

    CloseServiceHandle(schSCManager);
  } else
    _tprintf(TEXT("OpenSCManager failed - %s\n"), getLastErrorText().c_str());
#endif
}

//------------------------------------------------------------------------------

void TService::remove(const std::string &name) {
#ifdef _WIN32
  std::string displayName = name;

  SC_HANDLE schService;
  SC_HANDLE schSCManager;

  schSCManager = OpenSCManager(NULL,  // machine (NULL == local)
                               NULL,  // database (NULL == default)
                               SC_MANAGER_ALL_ACCESS);  // access required

  if (schSCManager) {
    schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);

    if (schService) {
      SERVICE_STATUS ssStatus;  // current status of the service

      // try to stop the service
      if (ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus)) {
        _tprintf(TEXT("Stopping %s."), displayName.c_str());
        Sleep(1000);

        while (QueryServiceStatus(schService, &ssStatus)) {
          if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING) {
            _tprintf(TEXT("."));
            Sleep(1000);
          } else
            break;
        }

        if (ssStatus.dwCurrentState == SERVICE_STOPPED)
          _tprintf(TEXT("\n%s stopped.\n"), displayName.c_str());
        else
          _tprintf(TEXT("\n%s failed to stop.\n"), displayName.c_str());
      }

      // now remove the service
      if (DeleteService(schService))
        _tprintf(TEXT("%s removed.\n"), displayName.c_str());
      else
        _tprintf(TEXT("DeleteService failed - %s\n"),
                 getLastErrorText().c_str());

      CloseServiceHandle(schService);
    } else
      _tprintf(TEXT("OpenService failed - %s\n"), getLastErrorText().c_str());

    CloseServiceHandle(schSCManager);
  } else
    _tprintf(TEXT("OpenSCManager failed - %s\n"), getLastErrorText().c_str());
#endif
}

//------------------------------------------------------------------------------

void TService::addToMessageLog(const QString &msg) {
  addToMessageLog(msg.toStdString());
}

void TService::addToMessageLog(const std::string &msg) {
#ifdef _WIN32
  TCHAR szMsg[256];
  HANDLE hEventSource;
  LPCTSTR lpszStrings[2];

  if (!TService::Imp::m_console) {
    TService::Imp::m_dwErr = GetLastError();

    // Use event logging to log the error.
    //
    hEventSource =
        RegisterEventSource(NULL, TService::instance()->getName().c_str());

    _stprintf(szMsg, TEXT("%s error: %d"),
              TService::instance()->getName().c_str(), TService::Imp::m_dwErr);
    lpszStrings[0] = szMsg;
    lpszStrings[1] = msg.c_str();

    if (hEventSource != NULL) {
      ReportEvent(hEventSource,         // handle of event source
                  EVENTLOG_ERROR_TYPE,  // event type
                  0,                    // event category
                  0,                    // event ID
                  NULL,                 // current user's SID
                  2,                    // strings in lpszStrings
                  0,                    // no bytes of raw data
                  lpszStrings,          // array of error strings
                  NULL);                // no raw data

      (VOID) DeregisterEventSource(hEventSource);
    }
  } else {
    std::cout << msg.c_str();
  }
#else
  if (!TService::Imp::m_console) {
    QString str(msg.c_str());
    TSysLog::error(str);
  } else {
    std::cout << msg.c_str();
  }
#endif
}
