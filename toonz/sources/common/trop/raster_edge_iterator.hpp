#pragma once

#ifndef RASTER_EDGE_ITERATOR_HPP
#define RASTER_EDGE_ITERATOR_HPP

#include "raster_edge_iterator.h"

namespace TRop {
namespace borders {

template <typename PixelSelector>
RasterEdgeIterator<PixelSelector>::RasterEdgeIterator(
    const raster_typeP &rin, const selector_type &selector, const TPoint &pos,
    const TPoint &dir, int adherence)
    : m_ras(rin)
    , m_lx_1(rin->getLx() - 1)
    , m_ly_1(rin->getLy() - 1)
    , m_wrap(rin->getWrap())
    , m_selector(selector)
    , m_pos(pos)
    , m_dir(dir)
    , m_elbowColor(selector.transparent())
    , m_rightSide(adherence == RIGHT)
    , m_turn(UNKNOWN) {
  pixels(m_leftPix, m_rightPix);
  colors(m_leftColor, m_rightColor);
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
void RasterEdgeIterator<PixelSelector>::setEdge(const TPoint &pos,
                                                const TPoint &dir) {
  m_pos = pos, m_dir = dir;
  pixels(m_leftPix, m_rightPix);
  colors(m_leftColor, m_rightColor);
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
inline void RasterEdgeIterator<PixelSelector>::pixels(pixel_type *&pixLeft,
                                                      pixel_type *&pixRight) {
  pixel_type *pix = m_ras->pixels(0) + m_pos.y * m_wrap + m_pos.x;
  if (m_dir.y)
    if (m_dir.y > 0)
      pixLeft = pix - 1, pixRight = pix;
    else
      pixLeft = pix - m_wrap, pixRight = pixLeft - 1;
  else if (m_dir.x > 0)
    pixLeft = pix, pixRight = pix - m_wrap;
  else
    pixRight = pix - 1, pixLeft = pixRight - m_wrap;
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
inline void RasterEdgeIterator<PixelSelector>::colors(value_type &leftColor,
                                                      value_type &rightColor) {
  if (m_dir.y)
    if (m_dir.y > 0) {
      if (m_pos.y > m_ly_1)
        leftColor = rightColor = m_selector.transparent();
      else {
        leftColor = (m_pos.x > 0) ? m_selector.value(*m_leftPix)
                                  : m_selector.transparent();
        rightColor = (m_pos.x <= m_lx_1) ? m_selector.value(*m_rightPix)
                                         : m_selector.transparent();
      }
    } else {
      if (m_pos.y < 1)
        leftColor = rightColor = m_selector.transparent();
      else {
        leftColor = (m_pos.x <= m_lx_1) ? m_selector.value(*m_leftPix)
                                        : m_selector.transparent();
        rightColor = (m_pos.x > 0) ? m_selector.value(*m_rightPix)
                                   : m_selector.transparent();
      }
    }
  else if (m_dir.x > 0) {
    if (m_pos.x > m_lx_1)
      leftColor = rightColor = m_selector.transparent();
    else {
      leftColor = (m_pos.y <= m_ly_1) ? m_selector.value(*m_leftPix)
                                      : m_selector.transparent();
      rightColor = (m_pos.y > 0) ? m_selector.value(*m_rightPix)
                                 : m_selector.transparent();
    }
  } else {
    if (m_pos.x < 1)
      leftColor = rightColor = m_selector.transparent();
    else {
      leftColor = (m_pos.y > 0) ? m_selector.value(*m_leftPix)
                                : m_selector.transparent();
      rightColor = (m_pos.y <= m_ly_1) ? m_selector.value(*m_rightPix)
                                       : m_selector.transparent();
    }
  }
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
inline void RasterEdgeIterator<PixelSelector>::turn(
    const value_type &newLeftColor, const value_type &newRightColor) {
  if (m_rightSide) {
    if (newLeftColor == m_rightColor) {
      if (newRightColor == m_leftColor)
        turnAmbiguous(newLeftColor, newRightColor);
      else
        turnLeft();
    } else {
      if (newRightColor != m_rightColor)
        turnRight();
      else
        m_turn = STRAIGHT;
    }

    m_elbowColor = newLeftColor;
  } else {
    if (newRightColor == m_leftColor) {
      if (newLeftColor == m_rightColor)
        turnAmbiguous(newLeftColor, newRightColor);
      else
        turnRight();
    } else {
      if (newLeftColor != m_leftColor)
        turnLeft();
      else
        m_turn = STRAIGHT;
    }

    m_elbowColor = newRightColor;
  }

  pixels(m_leftPix, m_rightPix);
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
inline void RasterEdgeIterator<PixelSelector>::turnAmbiguous(
    const value_type &newLeftColor, const value_type &newRightColor) {
  pixel_type *pix = m_ras->pixels(0) + m_pos.y * m_wrap + m_pos.x;
  UCHAR count1 = 0, count2 = 0;

  value_type val;

  // Check the 4x4 neighbourhood and connect the minority color
  if (m_pos.x > 2) {
    val = m_selector.value(*(pix - 2));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;

    val = m_selector.value(*(pix - 2 - m_wrap));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;
  }

  if (m_pos.x < m_lx_1) {
    val = m_selector.value(*(pix + 1));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;

    val = m_selector.value(*(pix + 1 - m_wrap));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;
  }

  if (m_pos.y > 2) {
    int wrap2 = m_wrap << 1;

    val = m_selector.value(*(pix - wrap2));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;

    val = m_selector.value(*(pix - wrap2 - 1));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;
  }

  if (m_pos.y < m_ly_1) {
    val = m_selector.value(*(pix + m_wrap));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;

    val = m_selector.value(*(pix + m_wrap - 1));
    if (val == m_leftColor)
      ++count1;
    else if (val == m_rightColor)
      ++count2;
  }

  // Minority connection - join the one with less count
  if (count1 < count2)
    turnRight();  // Join m_leftColor == newRightColor
  else if (count1 > count2)
    turnLeft();  // Join m_rightColor == newLeftColor
  else if (m_rightColor < m_leftColor)
    turnLeft();
  else
    turnRight();

  m_turn |= AMBIGUOUS;
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector>
RasterEdgeIterator<PixelSelector>
    &RasterEdgeIterator<PixelSelector>::operator++() {
  value_type newLeftColor = m_leftColor, newRightColor = m_rightColor;

  int pixAdd = m_dir.y * m_wrap + m_dir.x;
  if (m_rightSide) {
    do {
      m_pos.x += m_dir.x, m_pos.y += m_dir.y;
      m_leftPix += pixAdd, m_rightPix += pixAdd;
      m_leftColor = newLeftColor;

      colors(newLeftColor, newRightColor);
    } while ((newRightColor == m_rightColor) &&
             (newLeftColor != newRightColor) &&
             m_selector.skip(m_leftColor, newLeftColor));
  } else {
    do {
      m_pos.x += m_dir.x, m_pos.y += m_dir.y;
      m_leftPix += pixAdd, m_rightPix += pixAdd;
      m_rightColor = newRightColor;

      colors(newLeftColor, newRightColor);
    } while ((newLeftColor == m_leftColor) && (newLeftColor != newRightColor) &&
             m_selector.skip(m_rightColor, newRightColor));
  }

  turn(newLeftColor, newRightColor);
  colors(m_leftColor, m_rightColor);

  return *this;
}
}
}  //  namespace TRop::borders

#endif  // RASTER_EDGE_ITERATOR_HPP
