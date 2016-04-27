

#include "ttimer.h"
#include "tthreadmessage.h"
#include "texception.h"

#ifdef _WIN32

#include <windows.h>

//moto strano: se togliamo l'include della glut non linka
#include <GL/glut.h>

//------------------------------------------------------------------------------

namespace
{

void CALLBACK ElapsedTimeCB(UINT uID, UINT uMsg,
							DWORD dwUser, DWORD dw1,
							DWORD dw2);
};

//------------------------------------------------------------------------------

class TTimer::Imp
{
public:
	Imp(std::string name, UINT timerRes, TTimer::Type type, TTimer *timer);
	~Imp();

	void start(UINT delay)
	{
		if (m_started)
			throw TException("The timer is already started");

		m_timerID = timeSetEvent(delay, m_timerRes,
								 (LPTIMECALLBACK)ElapsedTimeCB, (DWORD) this,
								 m_type | TIME_CALLBACK_FUNCTION);

		m_delay = delay;
		m_ticks = 0;
		if (m_timerID == NULL)
			throw TException("Unable to start timer");

		m_started = true;
	}

	void stop()
	{
		if (m_started)
			timeKillEvent(m_timerID);
		m_started = false;
	}

	std::string getName() { return m_name; }
	TUINT64 getTicks() { return m_ticks; }
	UINT getDelay() { return m_delay; }

	std::string m_name;

	UINT m_timerRes;
	UINT m_type;
	TTimer *m_timer;

	UINT m_timerID;
	UINT m_delay;
	TUINT64 m_ticks;
	bool m_started;

	TGenericTimerAction *m_action;
};

//------------------------------------------------------------------------------

TTimer::Imp::Imp(std::string name, UINT timerRes, TTimer::Type type, TTimer *timer)
	: m_name(name), m_timerRes(timerRes), m_timer(timer), m_type(type), m_timerID(NULL), m_ticks(0), m_delay(0), m_started(false), m_action(0)
{

	TIMECAPS tc;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		throw TException("Unable to create timer");
	}

	m_timerRes = tmin((int)tmax((int)tc.wPeriodMin, (int)m_timerRes), (int)tc.wPeriodMax);
	timeBeginPeriod(m_timerRes);

	switch (type) {
	case TTimer::OneShot:
		m_type = TIME_ONESHOT;
		break;

	case TTimer::Periodic:
		m_type = TIME_PERIODIC;
		break;

	default:
		throw TException("Unexpected timer type");
		break;
	}
}

//------------------------------------------------------------------------------

TTimer::Imp::~Imp()
{
	stop();
	timeEndPeriod(m_timerRes);

	if (m_action)
		delete m_action;
}

//------------------------------------------------------------------------------

namespace
{

void CALLBACK ElapsedTimeCB(UINT uID, UINT uMsg,
							DWORD dwUser, DWORD dw1,
							DWORD dw2)
{
	TTimer::Imp *imp = reinterpret_cast<TTimer::Imp *>(dwUser);
	imp->m_ticks++;
	if (imp->m_action)
		imp->m_action->sendCommand(imp->m_ticks);
}
};
#elif LINUX

#include <SDL/SDL_timer.h>
#include <SDL/SDL.h>
#include "tthread.h"
namespace
{
Uint32 ElapsedTimeCB(Uint32 interval, void *param);
}

class TTimer::Imp
{
public:
	Imp(std::string name, UINT timerRes, TTimer::Type type, TTimer *timer)
		: m_action(0), m_ticks(0)
	{
	}
	~Imp() {}

	void start(UINT delay)
	{
		static bool first = true;
		if (first) {
			SDL_Init(SDL_INIT_TIMER);
			first = false;
		}
		m_timerID = SDL_AddTimer(delay, ElapsedTimeCB, this);
	}

	void stop()
	{
		SDL_RemoveTimer(m_timerID);
	}

	std::string getName() { return m_name; }
	TUINT64 getTicks() { return m_ticks; }
	UINT getDelay() { return m_delay; }

	std::string m_name;

	UINT m_timerRes;
	UINT m_type;
	TTimer *m_timer;

	SDL_TimerID m_timerID;
	UINT m_delay;
	TUINT64 m_ticks;
	bool m_started;

	TGenericTimerAction *m_action;
};

class SendCommandMSG : public TThread::Message
{
	TTimer::Imp *m_ztimp;

public:
	SendCommandMSG(TTimer::Imp *ztimp) : TThread::Message(), m_ztimp(ztimp)
	{
	}
	~SendCommandMSG() {}
	TThread::Message *clone() const { return new SendCommandMSG(*this); }
	void onDeliver()
	{
		if (m_ztimp->m_action)
			m_ztimp->m_action->sendCommand(m_ztimp->m_ticks);
	}
};

namespace
{
Uint32 ElapsedTimeCB(Uint32 interval, void *param)
{
	TTimer::Imp *imp = reinterpret_cast<TTimer::Imp *>(param);
	imp->m_ticks++;
	SendCommandMSG(imp).send();
	return interval;
}
}

#elif __sgi
class TTimer::Imp
{
public:
	Imp(std::string name, UINT timerRes, TTimer::Type type, TTimer *timer)
		: m_action(0) {}
	~Imp() {}

	void start(UINT delay)
	{
		if (m_started)
			throw TException("The timer is already started");

		m_started = true;
	}

	void stop()
	{
		m_started = false;
	}

	std::string getName() { return m_name; }
	TUINT64 getTicks() { return m_ticks; }
	UINT getDelay() { return m_delay; }

	std::string m_name;

	UINT m_timerRes;
	UINT m_type;
	TTimer *m_timer;

	UINT m_timerID;
	UINT m_delay;
	TUINT64 m_ticks;
	bool m_started;

	TGenericTimerAction *m_action;
};
#elif MACOSX
class TTimer::Imp
{
public:
	Imp(std::string name, UINT timerRes, TTimer::Type type, TTimer *timer)
		: m_action(0) {}
	~Imp() {}

	void start(UINT delay)
	{
		if (m_started)
			throw TException("The timer is already started");
		throw TException("The timer is not yet available under MAC :(");
		m_started = true;
	}

	void stop()
	{
		m_started = false;
	}

	std::string getName() { return m_name; }
	TUINT64 getTicks() { return m_ticks; }
	UINT getDelay() { return m_delay; }

	std::string m_name;

	UINT m_timerRes;
	UINT m_type;
	TTimer *m_timer;

	UINT m_timerID;
	UINT m_delay;
	TUINT64 m_ticks;
	bool m_started;

	TGenericTimerAction *m_action;
};

#endif

//===============================================================================
//
//  TTimer
//
//===============================================================================

TTimer::TTimer(const std::string &name, UINT timerRes, Type type)
	: m_imp(new TTimer::Imp(name, timerRes, type, this))
{
}

//--------------------------------------------------------------------------------

TTimer::~TTimer()
{
}

//--------------------------------------------------------------------------------

void TTimer::start(UINT delay)
{
	m_imp->start(delay);
}

//--------------------------------------------------------------------------------

bool TTimer::isStarted() const
{
	return m_imp->m_started;
}

//--------------------------------------------------------------------------------

void TTimer::stop()
{
	m_imp->stop();
}

//--------------------------------------------------------------------------------

std::string TTimer::getName() const
{
	return m_imp->getName();
}

//--------------------------------------------------------------------------------

TUINT64 TTimer::getTicks() const
{
	return m_imp->getTicks();
}

//--------------------------------------------------------------------------------

UINT TTimer::getDelay() const
{
	return m_imp->getDelay();
}

//--------------------------------------------------------------------------------

void TTimer::setAction(TGenericTimerAction *action)
{
	if (m_imp->m_action)
		delete m_imp->m_action;

	m_imp->m_action = action;
}
