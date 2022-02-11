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

class DVAPI CellPositionRatio {
  double m_frameRatio, m_layerRatio;

public:
  CellPositionRatio(const double frameRatio, const double layerRatio)
      : m_frameRatio(frameRatio), m_layerRatio(layerRatio) {}

  double frame() const { return m_frameRatio; }
  double layer() const { return m_layerRatio; }
};

#endif
