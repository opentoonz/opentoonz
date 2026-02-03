#pragma once

#ifndef TSWEEPBOUNDARY_INCLUDED
#define TSWEEPBOUNDARY_INCLUDED

#include <vector>
//#include "tstroke.h"
//#include "tcurves.h"

// bool computeSweepBoundary(	const std::vector<TStroke*> &strokes,
//							std::vector<
// std::vector<TQuadratic*> > &outlines );

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

bool DVAPI
computeSweepBoundary(const std::vector<TStroke *> &strokes,
                     std::vector<std::vector<TQuadratic *>> &outlines);

#endif  // TSWEEPBOUNDARY_INCLUDED
