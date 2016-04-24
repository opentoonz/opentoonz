#ifndef TTIMER_INCLUDED
#define TTIMER_INCLUDED

#include <memory>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TAPPTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class DVAPI TGenericTimerAction
{
public:
	virtual ~TGenericTimerAction() {}
	virtual void sendCommand(TUINT64 tick) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TTimerAction : public TGenericTimerAction
{
public:
	typedef void (T::*Method)(TUINT64 tick);
	TTimerAction(T *target, Method method) : m_target(target), m_method(method) {}
	void sendCommand(TUINT64 tick) { (m_target->*m_method)(tick); }
private:
	T *m_target;
	Method m_method;
};

//------------------------------------------------------------------------------
//! THis class is manages general time events.
/*!
		This class defines a timer, 
		i.e a system which, at user defined time steps, sends events through a callback function.
	*/
class DVAPI TTimer
{
public:
	/*!
		Specifies which is the type of timer of this object.
	*/
	enum Type {
		OneShot, /*!< This type of timer sends timer events only at a single time. */
		Periodic /*!< This type of timer sends timer events periodically. */
	};

	/*!
		Creates a timer with name \p name, resolution \p timerRes and type \p type.
		Resolution is expressed in milliseconds.
	*/
	TTimer(const string &name, UINT timerRes, Type type);
	/*!
		Deletes the timer.
	*/
	~TTimer();

	/*!
		Starts the timer after \p delay milliseconds.
	*/
	void start(UINT delay); // delay expressed in milliseconds
							/*!
		Stops the timer immediately. 
		Doesn't delete the timer.
	*/
	void stop();
	/*!
		Returns \p true if the timer is started.
	*/
	bool isStarted() const;
	/*!
		Returns the name of the timer.
	*/
	string getName() const;
	/*!
		Asks the timer for number of events so far.
	*/
	TUINT64 getTicks() const;
	/*!
		Returns the initial start delay of the timer. 
	*/
	UINT getDelay() const;
	/*!
		Sets the callback function, i.e. the function to be called every timer's shot.
	*/
	void setAction(TGenericTimerAction *action);

	class Imp;

private:
	std::unique_ptr<Imp> m_imp;

	TTimer(const TTimer &);
	void operator=(const TTimer &);
};

#endif
