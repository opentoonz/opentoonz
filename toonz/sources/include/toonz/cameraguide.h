#pragma once

#ifndef CAMERA_GUIDE_INCLUDED
#define CAMERA_GUIDE_INCLUDED

#include "tgeometry.h"

namespace CameraGuide {

// frame is in physical, camera-local coordinates; resolution is applied later.
inline TRectD centeredRect(const TRectD &frame, double aspectRatio) {
  double width = frame.getLx(), height = frame.getLy();
  if (!std::isfinite(frame.x0) || !std::isfinite(frame.y0) ||
      !std::isfinite(width) || !std::isfinite(height) || width <= 0.0 ||
      height <= 0.0 || !std::isfinite(aspectRatio) || aspectRatio <= 0.0)
    return TRectD();

  if (width / height > aspectRatio)
    width = height * aspectRatio;
  else
    height = width / aspectRatio;

  const TPointD center =
      frame.getP00() + TPointD(frame.getLx() * 0.5, frame.getLy() * 0.5);
  return TRectD(center.x - width * 0.5, center.y - height * 0.5,
                center.x + width * 0.5, center.y + height * 0.5);
}

}  // namespace CameraGuide

#endif
