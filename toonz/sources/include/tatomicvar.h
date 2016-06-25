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

#include <atomic>

class DVAPI TAtomicVar {
public:
  using value_type = long;

public:
  TAtomicVar() : m_var(0) {}

public:
  value_type operator++() { return ++m_var; }
  value_type operator--() { return --m_var; }

  value_type operator+=(value_type value) { return m_var += value; }

  bool operator<=(value_type rhs) { return m_var <= rhs; };

  operator value_type() const { return m_var; };

#if !defined(LINUX) || defined(LINUX) && (__GNUC__ == 3) && (__GNUC_MINOR__ > 1)
private:  // to avoid well known bug in gcc3 ... fixed in later versions..
#endif
  TAtomicVar &operator=(const TAtomicVar &) = delete;  // not implemented
  TAtomicVar(const TAtomicVar &v)           = delete;  // not implemented

private:
  std::atomic<value_type> m_var;
};

#endif
