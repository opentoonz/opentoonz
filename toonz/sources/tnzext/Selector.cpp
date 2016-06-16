#include "ext/Selector.h"
#include "tgl.h"
//#include "drawutil.h"
#include "tmathutil.h"

using namespace ToonzExt;

//-----------------------------------------------------------------------------

namespace {
const GLfloat s_normalColor[]      = {250.0 / 255.0, 127 / 255.0, 240 / 255.0};
const GLfloat s_highlightedColor[] = {150.0 / 255.0, 255.0 / 255.0,
                                      140.0 / 255.0};
const double s_sqrt_2      = sqrt(2.0);
const double s_radius      = 5.0;
const double s_over_size   = 10;
const double s_length      = 15;
const double s_square_size = 5;
const double s_arrow_ratio = 2.5;

void drawArrow(const TPointD &from, const TPointD &direction, double length,
               const TPointD &arrowDirection = TPointD(0.0, -1.0),
               double arrowLength            = 0.0) {
  if (length <= 0 || arrowLength < 0) return;

  if (arrowLength == 0.0) arrowLength = 0.2 * length;

  TPointD pnt(from), myDir = normalize(direction);

  TPointD myArrowDirection(arrowDirection);

  if (arrowDirection.y <= 0.0) myArrowDirection = normalize(TPointD(-0.5, 0.8));

  if (direction * TPointD(1, 0) <= 0.0)
    myArrowDirection.x = -myArrowDirection.x;

  myDir            = myDir * length;
  myArrowDirection = myArrowDirection * arrowLength;

  glBegin(GL_LINES);
  tglVertex(pnt);
  pnt += myDir;
  tglVertex(pnt);
  glEnd();
  glBegin(GL_TRIANGLES);
  TPointD up = myDir + myArrowDirection;
  tglVertex(from + up);
  // tglVertex(pnt);
  tglVertex(pnt);
  myArrowDirection.y = -myArrowDirection.y;
  TPointD down       = myDir + myArrowDirection;
  tglVertex(from + down);
  glEnd();
}
}

//-----------------------------------------------------------------------------

Selector::Selector(double stroke_length, double min_val, double max_val) {
  original_stroke_length_ = stroke_length;
  range_                  = TPointD(min_val, max_val);
  isVisible_              = false;
  pixel_size_             = 1.0;
  w_                      = 0.5;
  this->init();
}

//-----------------------------------------------------------------------------

void Selector::init() {
  strokeRef_  = 0;
  signum_     = 1.0;
  isSelected_ = NONE;
}

//-----------------------------------------------------------------------------

Selector::~Selector() {}

//-----------------------------------------------------------------------------

TPointD Selector::getUp() const {
  if (!strokeRef_) return TPointD();

  TThickPoint pnt = strokeRef_->getPoint(w_);

  TPointD p = convert(pnt), p1;

  TPointD v0, v1, v = normalize(rotate90(strokeRef_->getSpeed(w_)));

  const double delta = TConsts::epsilon;

  if (w_ - delta >= 0.0)
    v0 = rotate90(strokeRef_->getSpeed(w_ - delta));
  else
    v0 = rotate90(strokeRef_->getSpeed(0));

  if (w_ + delta <= 1.0)
    v1 = rotate90(strokeRef_->getSpeed(w_ + delta));
  else
    v1 = rotate90(strokeRef_->getSpeed(1.0));

  if (!isAlmostZero(v * v0) || !isAlmostZero(v * v1)) v = normalize(v1 + v0);

  return v;
}

//-----------------------------------------------------------------------------

void Selector::draw(Designer *designer) {
  if (!strokeRef_ || !isVisible_) return;

  pixel_size_ = designer ? sqrt(designer->getPixelSize2()) : 1.0;

  TPointD v = this->getUp(), n = normalize(rotate90(v));

  TThickPoint pnt = strokeRef_->getThickPoint(w_);

  height_ = (pnt.thick + s_over_size) * pixel_size_;

  TPointD p(pnt.x, pnt.y), up = p + v * height_, down = p - v * height_;

  glColor3fv(s_normalColor);
  glBegin(GL_LINES);
  tglVertex(down);
  tglVertex(up);
  glEnd();

  if (isSelected_ == POSITION)
    glColor3fv(s_highlightedColor);
  else
    glColor3fv(s_normalColor);

  double radius = s_radius * pixel_size_;

  if (isSelected_ == POSITION) tglDrawDisk(up + v * radius, radius);

  tglDrawCircle(up + v * radius, radius);

  if (isSelected_ == LENGTH)
    glColor3fv(s_highlightedColor);
  else
    glColor3fv(s_normalColor);

  {
    // the circle center is in
    TPointD down = -this->getUp(), center = pnt + down * (height_);
    const double length = s_square_size * 0.5 * pixel_size_;

    TPointD
        //  offset(length, 0.0),
        offset(length, length),
        bottom_left = center - offset, top_rigth = center + offset;

    TRectD rectangle(bottom_left, top_rigth);

    if (isSelected_ == LENGTH) tglFillRect(rectangle);

    tglDrawRect(rectangle);
  }
  /*
TPointD
arrowDirection(-0.5,0.8);

if( signum_ < 0.0 )
arrowDirection.x = -arrowDirection.x;
const double
length = s_length  * pixel_size_,
arrow_length = length/s_arrow_ratio;

drawArrow(down,
      TPointD(1,0),
      length,
      arrowDirection,
      arrow_length);
drawArrow(down,
      TPointD(-1,0),
      length,
      arrowDirection,
      arrow_length);
*/
  // draw stroke related information
  if (designer && this->isSelected()) designer->draw(this);

#if 0  // def  _DEBUG
  {
  TThickPoint
    pnt = strokeRef_->getThickPoint(w_);

  TPointD
      up = this->getUp(),
      down = -up;

  double
    radius = s_radius * pixel_size_;

  // the circle center is in 
  TPointD 
    center = pnt + up * ( height_ + radius);

  glColor3d(1.0,0.0,1.0);
  tglDrawCircle(center,radius+pixel_size_);

  // one pixel of tolerance
  center = pnt + down * ( height_ );

  const double
    length = s_length * pixel_size_;

  TPointD
    offset(length, 0.0),
    bottom_left = center - offset,
    top_rigth   = center + offset;

  TRectD
    rectangle(bottom_left,
              top_rigth);

  rectangle = rectangle.enlarge( 2.0* pixel_size_);

  tglDrawRect(rectangle);
  }
#endif
}

//-----------------------------------------------------------------------------

void Selector::mouseDown(const TPointD &pos) {
  click_ = curr_ = pos;
  if (!strokeRef_) return;
  stroke_length_ = original_stroke_length_;
  prev_          = curr_;
}

//-----------------------------------------------------------------------------

void Selector::mouseUp(const TPointD &pos) {
  curr_ = pos;
  if (!strokeRef_) return;
  original_stroke_length_ = stroke_length_;
  prev_                   = curr_;
}

//-----------------------------------------------------------------------------

void Selector::mouseMove(const TPointD &pos) {
  curr_ = pos;
  if (!strokeRef_) return;

  isSelected_ = this->getSelection(pos);
  prev_       = curr_;
}

//-----------------------------------------------------------------------------

void Selector::mouseDrag(const TPointD &pos) {
  curr_ = pos;
  if (!strokeRef_) return;

  double val = original_stroke_length_;

  const double stroke_length = strokeRef_->getLength();

  double max_val = std::min(stroke_length, range_.y);

  TPointD v = curr_ - prev_;

  signum_ = 1.0;

  switch (isSelected_) {
  case POSITION:
    w_ = strokeRef_->getW(pos);
    break;
  case LENGTH:
    signum_        = v * TPointD(1.0, 0.0) < 0.0 ? -1.0 : 1.0;
    val            = original_stroke_length_ + signum_ * pixel_size_ * norm(v);
    stroke_length_ = std::max(std::min(max_val, val), range_.x);
    break;
  default:
    assert(false);
  }
  prev_ = curr_;
}

//-----------------------------------------------------------------------------

void Selector::setStroke(const TStroke *ref) {
  // if(strokeRef_  &&
  //   strokeRef_ != ref )
  //  w_=strokeRef_->getParameterAtLength( strokeRef_->getLength()*0.5);
  // else
  //  w_=0.5;
  this->init();
  strokeRef_ = ref;
}

//-----------------------------------------------------------------------------

Selector::Selection Selector::getSelection(const TPointD &pos) {
  if (!strokeRef_ || !isVisible_) return NONE;

  TThickPoint pnt = strokeRef_->getThickPoint(w_);

  TPointD up = this->getUp(), down = -up;

  double radius = s_radius * pixel_size_;

  // the circle center is in
  TPointD center = pnt + up * (height_ + radius);

  // one pixel of tolerance
  if (tdistance2(center, pos) <= sq(radius + pixel_size_)) return POSITION;

  center = pnt + down * (height_);

  // const double  length = s_length * pixel_size_;
  const double length = s_square_size * 0.5 * pixel_size_;

  TPointD
      //  offset(length, 0.0),
      offset(length, length),
      bottom_left = center - offset, top_rigth = center + offset;

  TRectD rectangle(bottom_left, top_rigth);

  rectangle = rectangle.enlarge(2.0 * pixel_size_);

  if (rectangle.contains(pos)) return LENGTH;

  return NONE;
}

//-----------------------------------------------------------------------------

double Selector::getLength() const { return stroke_length_; }

//-----------------------------------------------------------------------------

void Selector::setLength(double length) {
  stroke_length_ = original_stroke_length_ = length;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
