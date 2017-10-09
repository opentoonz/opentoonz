#pragma once

#ifndef TFXUTIL_INCLUDED
#define TFXUTIL_INCLUDED

#include "tfx.h"

class TImage;

namespace TFxUtil {

DVAPI void setParam(const TFxP &fx, std::string paramName, double value);
DVAPI void setParam(const TFxP &fx, std::string paramName, TPixel32 value);

DVAPI TFxP makeColorCard(TPixel32 color);

DVAPI TFxP makeCheckboard();
DVAPI TFxP makeCheckboard(TPixel32 c0, TPixel32 c1, double squareSize);

/*--
 * Preferenceオプションにより、Xsheetノードに繋がった素材を「比較暗」合成して表示する
 * --*/
DVAPI TFxP makeDarken(const TFxP &dn, const TFxP &up);

DVAPI TFxP makeOver(const TFxP &dn, const TFxP &up);
DVAPI TFxP makeAffine(const TFxP &arg, const TAffine &aff);

DVAPI TFxP makeBlur(const TFxP &arg, double value);

DVAPI TFxP makeColumnColorFilter(const TFxP &arg, TPixel32 colorScale);

DVAPI TFxP makeRadialGradient(/*TPixel32 color*/);

enum { NO_KEYFRAMES = 0, ALL_KEYFRAMES = 1, SOME_KEYFRAMES = -1 };

DVAPI int getKeyframeStatus(const TFxP &fx, int frame);
DVAPI void deleteKeyframes(const TFxP &fx, int frame);

DVAPI void setKeyframe(const TFxP &dstFx, int dstFrame, const TFxP &srcFx,
                       int srcFrame, bool changedOnly = false);
}

#endif
