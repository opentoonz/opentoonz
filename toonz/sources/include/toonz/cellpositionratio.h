#pragma once

#ifndef CELL_POSITION_RATIO_INCLUDED
#define CELL_POSITION_RATIO_INCLUDED

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "tcommon.h"

class DVAPI Ratio {
  int m_num, m_denom;

  void normalize();
  Ratio normalized() const;

public:
  Ratio(int num, int denom);

  friend Ratio operator+(const Ratio &a, const Ratio &b);
  friend Ratio operator-(const Ratio &a, const Ratio &b);
  friend Ratio operator*(const Ratio &a, const Ratio &b);
  friend Ratio operator/(const Ratio &a, const Ratio &b);

  friend int operator*(const Ratio &a, int b);

  bool operator!() const { return m_num == 0; }
};

class DVAPI CellPositionRatio {
  Ratio m_frame, m_layer;

public:
  CellPositionRatio(const Ratio &frame, const Ratio &layer)
      : m_frame(frame), m_layer(layer) {}

  Ratio frame() const { return m_frame; }
  Ratio layer() const { return m_layer; }
};

#endif
