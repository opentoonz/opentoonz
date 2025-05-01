#ifndef FLIPBOOKSETTINGS_H
#define FLIPBOOKSETTINGS_H
#include "tenv.h"

// Shared flipbook background toggle settings for UI and rendering
inline TEnv::IntVar FlipBookWhiteBgToggle("FlipBookWhiteBgToggle", 1);
inline TEnv::IntVar FlipBookBlackBgToggle("FlipBookBlackBgToggle", 0);
inline TEnv::IntVar FlipBookCheckBgToggle("FlipBookCheckBgToggle", 0);

#endif