#pragma once

#ifndef TARGETCOLORS_H
#define TARGETCOLORS_H

#include "tpixel.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//----------------------------------------------------

//  Forward declarations

class TPalette;

//----------------------------------------------------

//*******************************************************************************
//    TargetColor declaration
//*******************************************************************************

struct DVAPI TargetColor {
  TPixel32 m_color;
  int m_index;
  int m_brightness;
  int m_contrast;
  double m_hRange;
  double m_threshold;

public:
  TargetColor(const TPixel32 &color, int index, int brightness, int contrast,
              double hRange, double threshold)
      : m_color(color)
      , m_index(index)
      , m_brightness(brightness)
      , m_contrast(contrast)
      , m_hRange(hRange)
      , m_threshold(threshold) {}
};

//*******************************************************************************
//    TargetColors declaration
//*******************************************************************************

class DVAPI TargetColors {
  std::vector<TargetColor> m_colors;

public:
  TargetColors();
  ~TargetColors();

  void update(TPalette *palette, bool noAntialias);

  int getColorCount() const;
  const TargetColor &getColor(int i) const;

  TargetColors(const TargetColors &c) { m_colors = c.m_colors; }
  TargetColors operator=(const TargetColors &c) {
    m_colors = c.m_colors;
    return *this;
  }
};

#endif
