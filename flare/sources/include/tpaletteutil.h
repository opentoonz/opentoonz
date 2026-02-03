#pragma once

#ifndef TPALETTE_UTIL_H
#define TPALETTE_UTIL_H

#include "tpalette.h"

#include <set>

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//
// mergePalette
//   input: sourcePalette, sourceIndices
//   output: indexTable, modifies: targetPalette
//
// inserisce i colori di sourcePalette individuati da sourceIndices in
// targetPalette
// per ogni colore prima cerca un colore identico (TColorStyle::operator==()) in
// targetPalette
// se non c'e' ne crea uno nuovo (nella prima pagina di targetPalette)
// indexTable assicura la conversione <indici in sourcePalette> -> <indici in
// targetPalette>

DVAPI void mergePalette(const TPaletteP &targetPalette,
                        std::map<int, int> &indexTable,
                        const TPaletteP &sourcePalette,
                        const std::set<int> &sourceIndices);

// replace palette and lacking amount of styles will be copied from the other
// one
// return value will be true if the style amount is changed after the operation
DVAPI bool mergePalette_Overlap(const TPaletteP &dstPalette,
                                const TPaletteP &copiedPalette,
                                bool keepOriginalPalette);

#endif
