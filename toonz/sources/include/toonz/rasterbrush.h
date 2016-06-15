#pragma once

#ifndef RASTERBRUSH_INCLUDED
#define RASTERBRUSH_INCLUDED

#include "trastercm.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

/*!
Presa un' immagine raster "completamente trasparente", la funzione "rasterBrush"
disegna uno stroke raster di colore "color" seguendo i punti contenuti nel
vettore
"points". La funzione rasterBrush non inserisce punti aggiuntivi nel vettore per
gestire la
continuita' dello stroke percio' il vettore "points" dovra' gia' contenerli.
*/

DVAPI void rasterBrush(const TRasterCM32P &raster,
                       const std::vector<TThickPoint> &points, int styleId,
                       bool doAntialias);

#endif
