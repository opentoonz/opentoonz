#pragma once

#ifndef PIXEL_SELECTORS_H
#define PIXEL_SELECTORS_H

#include "tpixel.h"
#include "tpixelgr.h"
#include "tpixelcm.h"

namespace TRop {
namespace borders {

//****************************************************************
//    Standard Pixel Selectors
//****************************************************************

template <typename Pix>
class PixelSelector {
  bool m_skip;

public:
  typedef Pix pixel_type;
  typedef Pix value_type;

public:
  PixelSelector(bool onlyCorners = true) : m_skip(onlyCorners) {}

  value_type transparent() const { return pixel_type::Transparent; }
  bool transparent(const pixel_type &pix) const { return (pix.m == 0); }

  value_type value(const pixel_type &pix) const { return pix; }
  bool equal(const pixel_type &a, const pixel_type &b) const { return a == b; }

  void setSkip(bool skip) { m_skip = skip; }
  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return m_skip;
  }
};

//--------------------------------------------------------------------------------

template <>
class PixelSelector<TPixelGR8> {
  bool m_skip;
  TPixelGR8 m_transpColor;

public:
  typedef TPixelGR8 pixel_type;
  typedef TPixelGR8 value_type;

public:
  PixelSelector(bool onlyCorners            = true,
                pixel_type transparentColor = pixel_type::White)
      : m_skip(onlyCorners), m_transpColor(transparentColor) {}

  value_type transparent() const { return m_transpColor; }
  bool transparent(const pixel_type &pix) const {
    return (pix == m_transpColor);
  }

  value_type value(const pixel_type &pix) const { return pix; }
  bool equal(const pixel_type &a, const pixel_type &b) const { return a == b; }

  void setSkip(bool skip) { m_skip = skip; }
  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return m_skip;
  }
};

//--------------------------------------------------------------------------------

template <>
class PixelSelector<TPixelGR16> {
  bool m_skip;
  TPixelGR16 m_transpColor;

public:
  typedef TPixelGR16 pixel_type;
  typedef TPixelGR16 value_type;

public:
  PixelSelector(bool onlyCorners            = true,
                pixel_type transparentColor = pixel_type::White)
      : m_skip(onlyCorners), m_transpColor(transparentColor) {}

  value_type transparent() const { return m_transpColor; }
  bool transparent(const pixel_type &pix) const {
    return (pix == m_transpColor);
  }

  value_type value(const pixel_type &pix) const { return pix; }
  bool equal(const pixel_type &a, const pixel_type &b) const { return a == b; }

  void setSkip(bool skip) { m_skip = skip; }
  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return m_skip;
  }
};

//--------------------------------------------------------------------------------

template <>
class PixelSelector<TPixelCM32> {
  int m_tone;
  bool m_skip;

public:
  typedef TPixelCM32 pixel_type;
  typedef TUINT32 value_type;

public:
  PixelSelector(bool onlyCorners = true, int tone = 128)
      : m_tone(tone), m_skip(onlyCorners) {}

  value_type transparent() const { return 0; }
  bool transparent(const pixel_type &pix) const { return value(pix) == 0; }

  value_type value(const pixel_type &pix) const {
    return (pix.getTone() < m_tone) ? pix.getInk() : pix.getPaint();
  }
  bool equal(const pixel_type &a, const pixel_type &b) const {
    return value(a) == value(b);
  }

  void setSkip(bool skip) { m_skip = skip; }
  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return m_skip;
  }
};
}
}  // namespace TRop::borders

#endif  // PIXEL_SELECTORS_H
