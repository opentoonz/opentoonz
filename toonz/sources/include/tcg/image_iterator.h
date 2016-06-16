

#ifndef TCG_IMAGE_ITERATOR_H
#define TCG_IMAGE_ITERATOR_H

// tcg includes
#include "tcg_ptr.h"
#include "tcg_image_ops.h"
#include "tcg_point.h"

// STD includes
#include <iterator>

namespace tcg {

//*********************************************************************************************************
//    Image Iterator  class
//*********************************************************************************************************

/*!
  The image_iterator class models an iterator accessing pixels of an image along
  its rows.
*/

template <typename It>
class image_iterator : public iterator_traits<It>::inheritable_iterator_type {
  typedef typename iterator_traits<It>::inheritable_iterator_type iter;

public:
  typedef typename iter::iterator_category iterator_category;
  typedef typename iter::value_type value_type;
  typedef typename iter::difference_type difference_type;
  typedef typename iter::pointer pointer;
  typedef typename iter::reference reference;

public:
  image_iterator() {}

  template <typename Img>
  image_iterator(const Img &img, int x, int y)
      : iter(image_traits<Img>::pixel(img, x, y))
      , m_base(image_traits<Img>::pixel(img, 0, 0))
      , m_lx(image_traits<Img>::width(img))
      , m_ly(image_traits<Img>::height(img))
      , m_wrap(image_traits<Img>::wrap(img))
      , m_skew(m_wrap - lx) {}

  int x() const { return (iter::operator-(m_base)) % m_wrap; }
  int y() const { return (iter::operator-(m_base)) / m_wrap; }

  image_iterator &operator++() {
    iter::operator++();
    if (x() >= m_lx) iter::operator+=(m_skew);
    return *this;
  }
  image_iterator operator++(int) {
    image_iterator it(*this);
    operator++();
    return it;
  }

  image_iterator &operator--() {
    iter::operator--();
    if (x() < 0) iter::operator-=(m_skew);
    return *this;
  }
  image_iterator operator--(int) {
    image_iterator it(*this);
    operator--();
    return it;
  }

  image_iterator &operator+=(difference_type d) {
    int yCount = (x() + d) / m_lx;
    iter::operator+=((d - yCount * m_lx) + yCount * m_wrap);
    return *this;
  }
  image_iterator operator+(difference_type d) const {
    image_iterator it(*this);
    it += d;
    return it;
  }

  image_iterator operator-(difference_type d) const { return operator+(-d); }
  image_iterator &operator-=(difference_type d) { return operator+=(-d); }

  difference_type operator-(const image_iterator &other) const {
    return (x() - other.x()) + m_lx * (y() - other.y());
  }

  reference operator[](difference_type d) const {
    const image_iterator &it = operator+(d);
    return *it;
  }

protected:
  iter m_base;
  int m_lx, m_ly, m_wrap, m_skew;
};

//*********************************************************************************************************
//    image_edge_iterator class
//*********************************************************************************************************

enum _iei_adherence_policy { LEFT_ADHERENCE, RIGHT_ADHERENCE };

/*!
  The image_edge_iterator class models a forward iterator following the contour
  of
  an image area of uniform color.
*/

template <typename It, _iei_adherence_policy _adherence = RIGHT_ADHERENCE>
class image_edge_iterator {
  typedef typename iterator_traits<It>::inheritable_iterator_type iter;

public:
  typedef std::forward_iterator_tag iterator_category;
  typedef typename iter::value_type value_type;
  typedef typename iter::difference_type difference_type;
  typedef typename iter::pointer pointer;
  typedef typename iter::reference reference;

public:
  enum { adherence = _adherence };

  enum Direction {
    STRAIGHT        = 0x0,
    LEFT            = 0x1,
    RIGHT           = 0x2,
    AMBIGUOUS       = 0x4,
    UNKNOWN         = 0x8,
    AMBIGUOUS_LEFT  = LEFT | AMBIGUOUS,
    AMBIGUOUS_RIGHT = RIGHT | AMBIGUOUS
  };

public:
  image_edge_iterator() {}

  template <typename Img>
  image_edge_iterator(const Img &img, int x, int y, int dirX, int dirY);

  const Point &pos() const { return m_pos; }
  const Point &dir() const { return m_dir; }

  const value_type &leftColor() const { return m_leftColor; }
  const value_type &rightColor() const { return m_rightColor; }

  const value_type &color() const { return color(policy<_adherence>()); }
  const value_type &oppositeColor() const {
    return oppositeColor(policy<_adherence>());
  }
  const value_type &elbowColor() const { return m_elbowColor; }

  iter leftPixel() const { return m_leftPix; }
  iter rightPixel() const { return m_rightPix; }

  iter pixel() const { return pixel(policy<_adherence>()); }
  iter oppositePixel() const { return oppositePixel(policy<_adherence>()); }

  Direction turn() const { return Direction(m_turn); }

public:
  // Iterator functions

  bool operator==(const image_edge_iterator &it) const {
    return (m_pos == it.m_pos) && (m_dir == it.m_dir);
  }
  bool operator!=(const image_edge_iterator &it) const {
    return !operator==(it);
  }

  image_edge_iterator &operator++() {
    advance(policy<_adherence>());
    return *this;
  }
  image_edge_iterator operator++(int) {
    image_edge_iterator temp(*this);
    operator++();
    return temp;
  }

private:
  void pixels(iter pixLeft, iter pixRight);
  void colors(value_type &leftColor, value_type &rightColor);

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

  void turn(const value_type &newLeftColor, const value_type &newRightColor) {
    turn(newLeftColor, newRightColor, policy<_adherence>());
  }
  void turnAmbiguous(const value_type &newLeftColor,
                     const value_type &newRightColor);

private:
  template <_iei_adherence_policy>
  struct policy {};

  const value_type &color(policy<LEFT_ADHERENCE>) const { return m_leftColor; }
  const value_type &color(policy<RIGHT_ADHERENCE>) const {
    return m_rightColor;
  }

  const value_type &oppositeColor(policy<LEFT_ADHERENCE>) const {
    return m_rightColor;
  }
  const value_type &oppositeColor(policy<RIGHT_ADHERENCE>) const {
    return m_leftColor;
  }

  iter pixel(policy<LEFT_ADHERENCE>) const { return m_leftPix; }
  iter pixel(policy<RIGHT_ADHERENCE>) const { return m_rightPix; }

  iter oppositePixel(policy<LEFT_ADHERENCE>) const { return m_rightPix; }
  iter oppositePixel(policy<RIGHT_ADHERENCE>) const { return m_leftPix; }

  void turn(const value_type &newLeftColor, const value_type &newRightColor,
            policy<LEFT_ADHERENCE>);
  void turn(const value_type &newLeftColor, const value_type &newRightColor,
            policy<RIGHT_ADHERENCE>);

  void advance(policy<LEFT_ADHERENCE>);
  void advance(policy<RIGHT_ADHERENCE>);

private:
  int m_lx_1, m_ly_1, m_wrap;

  Point m_pos, m_dir;

  value_type m_leftColor, m_rightColor, m_outsideColor, m_elbowColor;
  iter m_pix, m_leftPix, m_rightPix;

  int m_turn;
};

}  // namespace tcg

#endif  // TCG_IMAGE_ITERATOR_H

//=====================================================================================

#ifdef INCLUDE_HPP
#include "hpp/image_iterator.hpp"
#endif  // INCLUDE_HPP
