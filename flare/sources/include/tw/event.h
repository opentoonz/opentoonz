#pragma once

#ifndef T_EVENT_INCLUDED
#define T_EVENT_INCLUDED

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TMouseEvent {
public:
  TPoint m_pos;
  int m_pressure;
  int m_status;
  bool isCtrlPressed() const;
  bool isShiftPressed() const;
  bool isAltPressed() const;
#ifdef MACOSX
  bool isLockPressed() const;
#endif
  TMouseEvent() : m_pos(), m_pressure(0), m_status(0) {}
  TMouseEvent(int x, int y) : m_pos(x, y), m_pressure(0), m_status(0) {}
  TMouseEvent(const TPoint &p) : m_pos(p), m_pressure(0), m_status(0) {}

  TMouseEvent(const TPoint &p, int pressure, int status)
      : m_pos(p), m_pressure(pressure), m_status(status) {}
};

#endif
