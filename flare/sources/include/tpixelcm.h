#pragma once

#ifndef T_PIXELCM_INCLUDED
#define T_PIXELCM_INCLUDED

#include "tcommon.h"
#include "tnztypes.h"
//#include "tspecialstyleid.h"
#undef DVAPI
#undef DVVAR
#ifdef TCOLOR_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

//! Toonz5.0 color-map, 12+12+8 bits (ink,paint,tone)
class DVAPI TPixelCM32 {
  TUINT32 m_value;

public:
  TPixelCM32() : m_value(255) {}
  explicit TPixelCM32(TUINT32 v) : m_value(v) {}
  TPixelCM32(int ink, int paint, int tone)
      : m_value(ink << 20 | paint << 8 | tone) {
    assert(0 <= ink && ink < 4096);
    assert(0 <= paint && paint < 4096);
    assert(0 <= tone && tone < 256);
  }
  TPixelCM32(const TPixelCM32 &pix) : m_value(pix.m_value){};

  inline bool operator==(const TPixelCM32 &p) const {
    return m_value == p.m_value;
  };

  inline bool operator<(const TPixelCM32 &p) const {
    return m_value < p.m_value;
  };

  TUINT32 getValue() const { return m_value; }

  int getInk() const { return m_value >> 20; };
  int getPaint() const { return (m_value >> 8) & 0xFFF; };
  int getTone() const { return m_value & 0xFF; };

  void setInk(int ink) {
    assert(0 <= ink && ink < 4096);
    m_value = (m_value & 0x000FFFFF) | (ink << 20);
  }
  void setPaint(int paint) {
    assert(0 <= paint && paint < 4096);
    m_value = (m_value & 0xFFF000FF) | (paint << 8);
  }
  void setTone(int tone) {
    assert(0 <= tone && tone < 256);
    m_value = (m_value & 0xFFFFFF00) | tone;
  }

  inline static int getMaxInk() { return 4095; }
  inline static int getMaxPaint() { return 4095; }
  inline static int getMaxTone() { return 255; }

  inline static int getToneMask() { return 0xff; }
  inline static int getPaintMask() { return 0xfff00; }
  inline static int getInkMask() { return 0xfff00000; }

  // inline bool isInk()   const {return getTone()<64;} queste funzioni erano
  // folli!
  // inline bool isPaint() const {return getTone()>192;}

  inline bool isPureInk() const { return getTone() == 0; }
  inline bool isPurePaint() const { return getTone() == getMaxTone(); }
};

//-----------------------------------------------------------------------------

#endif  //__T_PIXEL_INCLUDED
