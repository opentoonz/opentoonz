// #pragma once // could not use by INCLUDE_HPP

#ifndef RASTER_EDGE_ITERATOR_H
#define RASTER_EDGE_ITERATOR_H

#include "traster.h"

namespace TRop {
namespace borders {

//*********************************************************************************************************
//    RasterEdgeIterator class
//*********************************************************************************************************

/*!
  The RasterEdgeIterator class models a forward iterator traversing a border of
  a
  raster image.
*/
template <typename PixelSelector>
class RasterEdgeIterator {
public:
  typedef PixelSelector selector_type;
  typedef typename PixelSelector::pixel_type pixel_type;
  typedef typename PixelSelector::value_type value_type;
  typedef TRasterT<pixel_type> raster_type;
  typedef TRasterPT<pixel_type> raster_typeP;

  enum {
    STRAIGHT        = 0x0,
    LEFT            = 0x1,
    RIGHT           = 0x2,
    AMBIGUOUS       = 0x4,
    AMBIGUOUS_LEFT  = LEFT | AMBIGUOUS,
    AMBIGUOUS_RIGHT = RIGHT | AMBIGUOUS,
    UNKNOWN         = 0x8
  };

private:
  raster_typeP m_ras;
  selector_type m_selector;

  int m_lx_1, m_ly_1, m_wrap;

  value_type m_leftColor, m_rightColor, m_elbowColor;
  pixel_type *m_leftPix, *m_rightPix;

  bool m_rightSide;
  int m_turn;

  TPoint m_pos, m_dir;

public:
  RasterEdgeIterator(const raster_typeP &rin, const selector_type &selector,
                     const TPoint &pos, const TPoint &dir,
                     int adherence = RIGHT);

  void setEdge(const TPoint &pos, const TPoint &dir);

  const raster_typeP &raster() const { return m_ras; }
  const selector_type &selector() const { return m_selector; }

  const TPoint &pos() const { return m_pos; }
  const TPoint &dir() const { return m_dir; }

  const value_type &leftColor() const { return m_leftColor; }
  const value_type &rightColor() const { return m_rightColor; }

  const value_type &color() const {
    return m_rightSide ? m_rightColor : m_leftColor;
  }
  const value_type &otherColor() const {
    return m_rightSide ? m_leftColor : m_rightColor;
  }
  const value_type &elbowColor() const { return m_elbowColor; }

  pixel_type *leftPix() const { return m_leftPix; }
  pixel_type *rightPix() const { return m_rightPix; }

  pixel_type *pix() const { return m_rightSide ? m_rightPix : m_leftPix; }
  pixel_type *otherPix() const { return m_rightSide ? m_leftPix : m_rightPix; }

  int turn() const { return m_turn; }

  void setAdherence(int side) { m_rightSide = (side == RIGHT); }
  int adherence() const { return m_rightSide ? RIGHT : LEFT; }

  RasterEdgeIterator &operator++();

  bool operator==(const RasterEdgeIterator &it) const {
    return m_pos == it.m_pos && m_dir == it.m_dir;
  }
  bool operator!=(const RasterEdgeIterator &it) const {
    return !operator==(it);
  }

private:
  void pixels(pixel_type *&pixLeft, pixel_type *&pixRight);
  void colors(value_type &leftColor, value_type &rightColor);
  void turn(const value_type &newLeftColor, const value_type &newRightColor);
  void turnLeft() {
    int temp = m_dir.x;
    m_dir.x  = -m_dir.y;
    m_dir.y  = temp;
    m_turn   = LEFT;
  }
  void turnRight() {
    int temp = m_dir.x;
    m_dir.x  = m_dir.y;
    m_dir.y  = -temp;
    m_turn   = RIGHT;
  }
  void turnAmbiguous(const value_type &newLeftColor,
                     const value_type &newRightColor);
};
}
}  // namespace TRop::borders

#endif  // RASTER_EDGE_ITERATOR_H

//=====================================================================================

#ifdef INCLUDE_HPP
#include "raster_edge_iterator.hpp"
#endif  // INCLUDE_HPP
