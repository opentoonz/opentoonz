#pragma once

#ifndef T_STOPWATCH_INCLUDED
#define T_STOPWATCH_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _WIN32
#include "windows.h"
#endif

//===============================================================
/**
  * This is an example of how to use stopwatch.
*/
/*! The TStopWatch class allows you to recording time (in milliseconds)
    taken by your process or by system to perform various tasks.
    It is possible to record these times:
    Total Time, User Time and System Time.
    A stop watch is identified by a name that describes meaningfully its use.
*/
#ifdef _WIN32

#define TM_TOTAL DWORD
#define TM_USER                                                                \
  __int64  // user time (time associated to the process calling stop
           // watch)(unit=100-nanosecond)
#define TM_SYSTEM __int64  // system time (unit=100-nanosecond)
#define START DWORD        // total starting reference time in milliseconds
#define START_USER                                                             \
  FILETIME  // process starting reference time (unit=100-nanosecond)
#define START_SYSTEM                                                           \
  FILETIME  // system starting reference time  (unit=100-nanosecond)
#else

#define TM_TOTAL clock_t
#define TM_USER clock_t
#define TM_SYSTEM clock_t
#define START clock_t
#define START_USER clock_t
#define START_SYSTEM clock_t
#endif

class DVAPI TStopWatch {
  std::string m_name;  // stopwatch name

  TM_TOTAL m_tm;         // elapsed total time (in milliseconds)
  TM_USER m_tmUser;      // elapsed user time (time associated to the process
                         // calling stop watch)(unit=100-nanosecond)
  TM_SYSTEM m_tmSystem;  // elapsed system time (unit=100-nanosecond)
  START m_start;         // total starting reference time in milliseconds
  START_USER
  m_startUser;  // process starting reference time (unit=100-nanosecond)
  START_SYSTEM
  m_startSystem;  // system starting reference time  (unit=100-nanosecond)

#ifdef _WIN32
  LARGE_INTEGER m_hrStart;  // high resolution starting reference (total) time
#endif

  bool m_active;     // sw e' stato usato (e' stato invocato il metodo start())
  bool m_isRunning;  // fra start() e stop()

  void setStartToCurrentTime();
  void getElapsedTime(TM_TOTAL &tm, TM_USER &user, TM_SYSTEM &system);

public:
  TStopWatch(std::string name = "");
  ~TStopWatch();

  void reset();

  /*!Start the stop watch. If reset=true the stop watch is firstly reset.*/
  void start(bool resetFlag = false);
  void stop();

  /*!Returns the number of milliseconds that have elapsed in all the
start()-stop() intervals since
the stopWatch was reset up to the getTotalTime() call. The method can be
called during a start()-stop() interval */
  TUINT32 getTotalTime();

  /*!Returns the amount of time that the process has spent executing application
   * code. see getTotalTime() */
  TUINT32 getUserTime();

  /*!Returns the amount of time that the process has spent executing operating
   * system code. see getTotalTime() */
  TUINT32 getSystemTime();

  const std::string &getName() { return m_name; };
  void setName(std::string name) { m_name = name; };

  /*!Returns a string containing the recorded times.*/
  operator std::string();

  /*!Print (to cout) the name and the relative total,user and system times of
   * the stop watch. see getTotalTime()*/
  void print(std::ostream &out);
  void print();

private:
  static TStopWatch StopWatch[10];

public:
  /*!
TStopWatch::global(index) is a global array of 10 stop watches.
*/
  static TStopWatch &global(int index) {
    assert(0 <= index && index < 10);
    return StopWatch[index];
  };

  /*!
Allows you to print the name and the relative total,
user and system times of all the active stop watches.
*/
  static void printGlobals(std::ostream &out);
  static void printGlobals();
};

//-----------------------------------------------------------

#endif  //  __T_STOPWATCH_INCLUDED
