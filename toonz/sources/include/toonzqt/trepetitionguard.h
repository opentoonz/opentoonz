#pragma once

#ifndef TREPETITION_GUARD
#define TREPETITION_GUARD

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TRepetitionGuard {
  static int m_counter;
  bool m_counterCheck;

public:
  TRepetitionGuard() : m_counterCheck(m_counter == 0) { ++m_counter; }

  ~TRepetitionGuard() { --m_counter; }

  bool hasLock() { return m_counterCheck; }
};

#endif  // TREPETITION_GUARD
