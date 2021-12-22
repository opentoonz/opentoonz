

#include "tstopwatch.h"

#include <sstream>

#ifdef _WIN32
#include <stdlib.h>
#else  //_WIN32

#include <unistd.h>
#include <limits.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>

#ifndef STW_TICKS_PER_SECOND
#ifndef _WIN32
extern "C" long sysconf(int);
#define STW_TICKS_PER_SECOND sysconf(_SC_CLK_TCK)
#else
#define STW_TICKS_PER_SECOND CLK_TCK
#endif
#endif
#endif

#define MAXSWNAMELENGHT 40
#define MAXSWTIMELENGHT 12

TStopWatch TStopWatch::StopWatch[10];

enum TimerType { TTUUnknown, TTUHiRes, TTUTickCount };
static void determineTimer();

#ifdef _WIN32

static TimerType timerToUse = TTUUnknown;

static LARGE_INTEGER perfFreq;  // ticks per second
static int perfFreqAdjust = 0;  // in case Freq is too big
static int overheadTicks  = 0;  // overhead  in calling timer

#else
static TimerType timerToUse = TTUTickCount;
#endif

using namespace std;

//-----------------------------------------------------------

TStopWatch::TStopWatch(std::string name)
    : m_name(name), m_active(false), m_isRunning(false) {
  if (timerToUse == TTUUnknown) determineTimer();

  m_start = 0;
#ifdef _WIN32
  m_startUser.dwHighDateTime = m_startUser.dwLowDateTime = 0;
  m_startSystem.dwHighDateTime = m_startSystem.dwLowDateTime = 0;
#else
  m_startUser               = 0;
  m_startSystem             = 0;
#endif  //_WIN32
  m_tm       = 0;
  m_tmUser   = 0;
  m_tmSystem = 0;
}

//-----------------------------------------------------------

TStopWatch::~TStopWatch() { m_active = false; }

//-----------------------------------------------------------

void TStopWatch::setStartToCurrentTime() {
#ifdef _WIN32
  FILETIME creationTime, exitTime;
  BOOL ret =
      GetProcessTimes(GetCurrentProcess(),  // specifies the process of interest
                      &creationTime, &exitTime, &m_startSystem, &m_startUser);

  if (timerToUse == TTUTickCount) {
    m_start = GetTickCount();
  } else {
    QueryPerformanceCounter(&m_hrStart);
  }
#else
  struct tms clk;
  m_start       = times(&clk);
  m_startUser   = clk.tms_utime;
  m_startSystem = clk.tms_stime;
#endif  //_WIN32
}

//-----------------------------------------------------------

void TStopWatch::reset() {
  m_tm       = 0;
  m_tmUser   = 0;
  m_tmSystem = 0;
  setStartToCurrentTime();
}

//-----------------------------------------------------------

void TStopWatch::start(bool resetFlag) {
  if (resetFlag) reset();
  if (m_isRunning) return;
  m_active    = true;
  m_isRunning = true;
  setStartToCurrentTime();
}

//-----------------------------------------------------------

#ifdef _WIN32
inline __int64 FileTimeToInt64(LPFILETIME pFileTime) {
  __int64 val;
  val = pFileTime->dwHighDateTime;
  val <<= 32;
  val |= pFileTime->dwLowDateTime;
  return val;
}
#endif  //_WIN32

//-----------------------------------------------------------

//
// Aggiunge il tempo trascorso fra start(startUser, startSystem) e l'istante
// corrente
// a tm(tmUser, tmSystem)
//
static void checkTime(START start, START_USER startUser,
                      START_SYSTEM startSystem, TM_TOTAL &tm, TM_USER &tmUser,
                      TM_SYSTEM &tmSystem) {
  assert(timerToUse == TTUTickCount);

#ifdef _WIN32

  DWORD tm_stop;
  FILETIME creationTime, exitTime, stopSystem, stopUser;
  BOOL ret =
      GetProcessTimes(GetCurrentProcess(),  // specifies the process of interest
                      &creationTime, &exitTime, &stopSystem, &stopUser);
  tm_stop = GetTickCount();
  assert(tm_stop >= start);
  tm += tm_stop - start;  // total elapsed time

  tmUser += FileTimeToInt64(&stopUser) -
            FileTimeToInt64(&startUser);  // user elapsed time
  tmSystem += FileTimeToInt64(&stopSystem) -
              FileTimeToInt64(&startSystem);  // system elapsed time

#else  // _WIN32

  struct tms clk;
  clock_t tm_stop;
  tm_stop = times(&clk);
  assert(tm_stop >= start);
  tm += tm_stop - start;
  tmUser += clk.tms_utime - startUser;
  tmSystem += clk.tms_stime - startSystem;

#endif  // _WIN32
}

//-----------------------------------------------------------

#ifdef _WIN32

//
// come checkTime, ma usa i timer ad alta risoluzione
//
namespace {

//-----------------------------------------------------------

void hrCheckTime(LARGE_INTEGER start, START_USER startUser,
                 START_SYSTEM startSystem, TM_TOTAL &tm, TM_USER &tmUser,
                 TM_SYSTEM &tmSystem) {
  assert(timerToUse != TTUTickCount);

  LARGE_INTEGER hrTm_stop;
  FILETIME creationTime, exitTime, stopSystem, stopUser;
  BOOL ret =
      GetProcessTimes(GetCurrentProcess(),  // specifies the process of interest
                      &creationTime, &exitTime, &stopSystem, &stopUser);

  QueryPerformanceCounter(&hrTm_stop);
  assert(hrTm_stop.HighPart > start.HighPart ||
         hrTm_stop.HighPart == start.HighPart &&
             hrTm_stop.LowPart >= start.LowPart);

  LARGE_INTEGER Freq = perfFreq;
  int Oht            = overheadTicks;

  LARGE_INTEGER dtime;
  // faccio "a mano" la differenza dtime = m_tStop - m_tStart
  dtime.HighPart = hrTm_stop.HighPart - start.HighPart;
  if (hrTm_stop.LowPart >= start.LowPart)
    dtime.LowPart = hrTm_stop.LowPart - start.LowPart;
  else {
    assert(dtime.HighPart > 0);
    dtime.HighPart--;
    dtime.LowPart = hrTm_stop.LowPart + ~start.LowPart + 1;
  }

  int shift = 0;
  if (Freq.HighPart > 0) {
    int h = Freq.HighPart;
    while (h > 0) {
      h >>= 1;
      shift++;
    }
  }
  if ((dtime.HighPart >> shift) > 0) {
    int h = dtime.HighPart >> shift;
    while (h > 0) {
      h >>= 1;
      shift++;
    }
  }
  if (shift > 0) {
    dtime.QuadPart = Int64ShrlMod32(dtime.QuadPart, shift);
    Freq.QuadPart  = Int64ShrlMod32(Freq.QuadPart, shift);
  }
  assert(Freq.HighPart == 0);
  assert(dtime.HighPart == 0);

  double totalTime = 1000.0 * dtime.LowPart / Freq.LowPart;
  tm += troundp(totalTime);

  tmUser += FileTimeToInt64(&stopUser) -
            FileTimeToInt64(&startUser);  // user elapsed time
  tmSystem += FileTimeToInt64(&stopSystem) -
              FileTimeToInt64(&startSystem);  // system elapsed time
}

//-----------------------------------------------------------

}  // namespace

#endif  // _WIN32

//-----------------------------------------------------------

void TStopWatch::stop() {
  if (!m_isRunning) return;
  m_isRunning = false;
#ifdef _WIN32
  if (timerToUse == TTUTickCount)
    checkTime(m_start, m_startUser, m_startSystem, m_tm, m_tmUser, m_tmSystem);
  else
    hrCheckTime(m_hrStart, m_startUser, m_startSystem, m_tm, m_tmUser,
                m_tmSystem);
#else
  checkTime(m_start, m_startUser, m_startSystem, m_tm, m_tmUser, m_tmSystem);
#endif
}

//-----------------------------------------------------------

void TStopWatch::getElapsedTime(TM_TOTAL &tm, TM_USER &user,
                                TM_SYSTEM &system) {
  if (m_isRunning) {
    TM_TOTAL cur_tm        = 0;
    TM_USER cur_tmUser     = 0;
    TM_SYSTEM cur_tmSystem = 0;

#ifdef _WIN32
    if (timerToUse == TTUTickCount)
      checkTime(m_start, m_startUser, m_startSystem, cur_tm, cur_tmUser,
                cur_tmSystem);
    else
      hrCheckTime(m_hrStart, m_startUser, m_startSystem, cur_tm, cur_tmUser,
                  cur_tmSystem);
#else
    checkTime(m_start, m_startUser, m_startSystem, cur_tm, cur_tmUser,
              cur_tmSystem);
#endif

    tm     = m_tm + cur_tm;
    user   = m_tmUser + cur_tmUser;
    system = m_tmSystem + cur_tmSystem;
  } else {
    tm     = m_tm;
    user   = m_tmUser;
    system = m_tmSystem;
  }
}

//-----------------------------------------------------------

TUINT32 TStopWatch::getTotalTime() {
  TM_TOTAL tm;
  TM_USER user;
  TM_SYSTEM system;
  getElapsedTime(tm, user, system);
#ifdef _WIN32
  return tm;
#else
  return (TINT32)(tm * 1000) / STW_TICKS_PER_SECOND;
#endif  //_WIN32
}

//-----------------------------------------------------------

TUINT32 TStopWatch::getUserTime() {
  TM_TOTAL tm;
  TM_USER user;
  TM_SYSTEM system;
  getElapsedTime(tm, user, system);
#ifdef _WIN32
  return (TINT32)(user / 10000);
#else
  return (TINT32)(user * 1000) / STW_TICKS_PER_SECOND;
#endif  //_WIN32
}

//-----------------------------------------------------------

TUINT32 TStopWatch::getSystemTime() {
  TM_TOTAL tm;
  TM_USER user;
  TM_SYSTEM system;
  getElapsedTime(tm, user, system);
#ifdef _WIN32
  return (TINT32)(system / 10000);
#else
  return (TINT32)(system * 1000) / STW_TICKS_PER_SECOND;
#endif  //_WIN32
}

//-----------------------------------------------------------

TStopWatch::operator string() {
  ostringstream out;
  out << m_name.c_str() << ": " << (int)getTotalTime() << " u"
      << (int)getUserTime() << " s" << (TINT32)getSystemTime();
  return out.str();
}

//------------------------------------------------------------

void TStopWatch::print() { print(cout); }

//-------------------------------------------------------------------------------------------

void TStopWatch::print(ostream &out) {
  string s(*this);
  out << s.c_str() << endl;
}

//-------------------------------------------------------------------------------------------

void TStopWatch::printGlobals(ostream &out) {
  const int n = sizeof(StopWatch) / sizeof(StopWatch[0]);
  for (int i = 0; i < n; i++)
    if (StopWatch[i].m_active) StopWatch[i].print(out);
}

//-------------------------------------------------------------------------------------------

void TStopWatch::printGlobals() { printGlobals(cout); }

//-----------------------------------------------------------
#ifdef _WIN32

static void dummyFunction() {
  // It's used just to calculate the overhead
  return;
}

void determineTimer() {
  void (*pFunc)() = dummyFunction;
  // cout << "DETERMINE TIMER" << endl;
  // Assume the worst
  timerToUse = TTUTickCount;
  if (QueryPerformanceFrequency(&perfFreq)) {
    // We can use hires timer, determine overhead
    timerToUse    = TTUHiRes;
    overheadTicks = 200;
    for (int i = 0; i < 20; i++) {
      LARGE_INTEGER b, e;
      int Ticks;
      QueryPerformanceCounter(&b);
      (*pFunc)();
      QueryPerformanceCounter(&e);
      Ticks = e.LowPart - b.LowPart;
      if (Ticks >= 0 && Ticks < overheadTicks) overheadTicks = Ticks;
    }
    // See if Freq fits in 32 bits; if not lose some precision
    perfFreqAdjust = 0;
    int High32     = perfFreq.HighPart;
    while (High32) {
      High32 >>= 1;
      perfFreqAdjust++;
    }
  }
}
#else
void determineTimer() {}
#endif
