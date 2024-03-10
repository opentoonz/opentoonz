

#include <tools/tooltimer.h>


//*****************************************************************************************
//    TToolTimer static members
//*****************************************************************************************

const TTimerTicks TToolTimer::frequency = 1000000000;
const double TToolTimer::step = 1e-9;
const double TToolTimer::epsilon = 1e-10;


//*****************************************************************************************
//    TToolTimer  implementation
//*****************************************************************************************

TToolTimer::TToolTimer()
  { m_timer.start(); }

 
TTimerTicks TToolTimer::ticks() {
  static TToolTimer timer;
  return timer.m_timer.nsecsElapsed();
}
