#pragma once

#ifndef CLEANUPPALETTE_INCLUDED
#define CLEANUPPALETTE_INCLUDED

class TPalette;

TPalette *createStandardCleanupPalette();
TPalette *createToonzPalette(TPalette *cleanupPalette);
TPalette *createToonzPalette(TPalette *cleanupPalette, int colorParamIndex);

void convertToCleanupPalette(TPalette *palette);
void convertToLevelPalette(TPalette *palette);

#endif
