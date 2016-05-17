#pragma once

#ifndef TATOMICVAR_H
#define TATOMICVAR_H

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
#include <windows.h>
#elif defined(__sgi)
#include <sys/atomic_ops.h>
#elif defined(LINUX)

#define __sgi

// #include <asm/atomic.h>
// it's broken, either include the kernel header
//   /usr/src/linux/include/asm/atomic.h
// or copy it here
// #include <asm/atomic.h>

#elif defined(powerpc)

// from linux-2.4.20-20.9/include/linux/asm-ppc

typedef struct {
	volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i) \
	{                  \
		(i)            \
	}

#define atomic_read(v) ((v)->counter)
#define atomic_set(v, i) (((v)->counter) = (i))

extern void atomic_clear_mask(unsigned long mask, unsigned long *addr);
extern void atomic_set_mask(unsigned long mask, unsigned long *addr);

// we define it as we want to run on SMP dual processors...
#define CONFIG_SMP

#ifdef CONFIG_SMP
#define SMP_ISYNC "\n\tisync"
#else
#define SMP_ISYNC
#endif

static __inline__ void atomic_add(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%3		\n\
	add	%0,%2,%0\n\
	stwcx.	%0,0,%3\n\
	bne-	1b"
		: "=&r"(t), "=m"(v->counter)
		: "r"(a), "r"(&v->counter), "m"(v->counter)
		: "cc");
}

static __inline__ int atomic_add_return(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%2		\n\
	add	%0,%1,%0\n\
	stwcx.	%0,0,%2\n\
	bne-	1b" SMP_ISYNC
		: "=&r"(t)
		: "r"(a), "r"(&v->counter)
		: "cc", "memory");

	return t;
}

static __inline__ void atomic_sub(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%3		\n\
	subf	%0,%2,%0\n\
	stwcx.	%0,0,%3\n\
	bne-	1b"
		: "=&r"(t), "=m"(v->counter)
		: "r"(a), "r"(&v->counter), "m"(v->counter)
		: "cc");
}

static __inline__ int atomic_sub_return(int a, atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%2		\n\
	subf	%0,%1,%0\n\
	stwcx.	%0,0,%2\n\
	bne-	1b" SMP_ISYNC
		: "=&r"(t)
		: "r"(a), "r"(&v->counter)
		: "cc", "memory");

	return t;
}

static __inline__ void atomic_inc(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%2		\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b"
		: "=&r"(t), "=m"(v->counter)
		: "r"(&v->counter), "m"(v->counter)
		: "cc");
}

static __inline__ int atomic_inc_return(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%1		\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%1\n\
	bne-	1b" SMP_ISYNC
		: "=&r"(t)
		: "r"(&v->counter)
		: "cc", "memory");

	return t;
}

static __inline__ void atomic_dec(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%2		\n\
	addic	%0,%0,-1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b"
		: "=&r"(t), "=m"(v->counter)
		: "r"(&v->counter), "m"(v->counter)
		: "cc");
}

static __inline__ int atomic_dec_return(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
		"1:	lwarx	%0,0,%1		\n\
	addic	%0,%0,-1\n\
	stwcx.	%0,0,%1\n\
	bne-	1b" SMP_ISYNC
		: "=&r"(t)
		: "r"(&v->counter)
		: "cc", "memory");

	return t;
}

#elif defined(i386)

#include <atomic>

typedef std::atomic<int> atomic_t;

static __inline__ void atomic_set(atomic_t *v, const int value)
{
	v->store(value);
}

static __inline__ int atomic_inc_return(atomic_t *v)
{
	return v->fetch_add(1) + 1; // post increment atomic
}

static __inline__ int atomic_dec_return(atomic_t *v)
{
	return v->fetch_sub(1) - 1; // post decriment atomic
}

static __inline__ int atomic_read(const atomic_t *v)
{
	return v->load();
}

static __inline__ int atomic_add(int num, const atomic_t *v)
{
	return const_cast<atomic_t *>(v)->fetch_add(num) + num; /* なんで const つけた? */
}

#else
@ @PLATFORM NOT SUPPORTED !@ @
#endif

/*! Platform specific 
    Provides class with increment & decrement absolutely done in interlocked way 
*/

class DVAPI TAtomicVar
{
public:
#if defined(__sgi)
	TAtomicVar() : m_var(0)
	{
	}
#elif defined(LINUX) || defined(MACOSX)
	TAtomicVar()
	{
		atomic_set(&m_var, 0);
	}
#else
	TAtomicVar() : m_var(0)
	{
	}
#endif

	long operator++()
	{
#ifdef _WIN32
		return InterlockedIncrement(&m_var);
#elif defined(__sgi)
		return ++m_var;
#elif defined(LINUX)
		// atomic_inc(&m_var);
		// return atomic_read(&m_var);
		//  this is broken as it can return a value != from ++m_var
		atomic_inc(&m_var);
		return atomic_read(&m_var);
#elif defined(MACOSX)
		return atomic_inc_return(&m_var);
#endif
	}

	long operator+=(long value)
	{
#ifdef _WIN32
		InterlockedExchangeAdd(&m_var, value);
		return m_var;

#elif defined(__sgi)
		m_var += value;
		return m_var;
#elif defined(LINUX)
		// atomic_inc(&m_var);
		// return atomic_read(&m_var);
		//  this is broken as it can return a value != from ++m_var
		assert(false);

		return m_var;
#elif defined(MACOSX)
		atomic_add(value, &m_var);
		return atomic_read(&m_var);
#endif
	}

	long operator--()
	{
#ifdef _WIN32
		return InterlockedDecrement(&m_var);
#elif defined(__sgi)
		return --m_var;
#elif defined(LINUX)
		// atomic_dec(&m_var);
		// return atomic_read(&m_var);
		// broken as above...
		atomic_dec(&m_var);
		return atomic_read(&m_var);
#elif defined(MACOSX)
		// atomic_dec(&m_var);
		// return atomic_read(&m_var);
		// broken as above...
		return atomic_dec_return(&m_var);
#endif
	}
	bool operator<=(const long &rhs)
	{
#if defined(__sgi)
		return m_var <= rhs;
#elif defined(LINUX) || defined(MACOSX)
		return atomic_read(&m_var) <= rhs;
#else
		return m_var <= rhs;
#endif
	};
	operator long() const
	{
#if defined(__sgi)
		return m_var;
#elif defined(LINUX) || defined(MACOSX)
		return atomic_read(&m_var);
#else
		return m_var;
#endif
	};

#ifdef _WIN32
	long m_var;
#elif defined(__sgi)
	long m_var;
#elif defined(LINUX) || defined(MACOSX)
	atomic_t m_var;
#endif

#if !defined(LINUX) || defined(LINUX) && (__GNUC__ == 3) && (__GNUC_MINOR__ > 1)
private: // to avoid well known bug in gcc3 ... fixed in later versions..
#endif
	TAtomicVar &operator=(const TAtomicVar &); //not implemented
	TAtomicVar(const TAtomicVar &v);		   //not implemented
};

#endif

#undef __sgi