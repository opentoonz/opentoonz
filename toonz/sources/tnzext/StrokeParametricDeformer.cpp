

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "ext/StrokeParametricDeformer.h"
#include "ext/NotSimmetricExpPotential.h"
//#include "ext/NotSimmetricBezierPotential.h"
//#include "ext/SquarePotential.h"

#include <sstream>

#include <tstroke.h>

using namespace std;

//-----------------------------------------------------------------------------

ToonzExt::StrokeParametricDeformer::StrokeParametricDeformer(
    double actionLenght, double startParameter, TStroke *s, Potential *pot)
    : startParameter_(startParameter)
    , actionLenght_(actionLenght)
    , vx_(1.0)
    , vy_(1.0)
    , pot_(0) {
  diff_ = 0.001;

  try {
    ref_copy_ = new TStroke(*s);
  } catch (std::bad_alloc &) {
    throw std::invalid_argument("Not possible to have a copy of stroke!!!");
  }

  assert(0.0 <= startParameter_ && startParameter_ <= 1.0);

  // double
  //  max_length = s->getLength();

  //  if( actionLenght == -1 )
  //  {
  //    assert(false && "Rispristina la vecchia gestione");
  //    actionLenght_ = 2.0 * max_length;
  //  }
  //  else
  //  {
  assert(0.0 <= actionLenght_);
  // && actionLenght_ <= max_length );

  if (0.0 > actionLenght_) actionLenght_ = 0.0;
  // else if ( actionLenght_ > max_length )
  //  actionLenght_ = max_length;
  //}

  pot_ = pot;
  if (!pot_)
    throw std::invalid_argument("Not Possible to have a ref of Potential!!!");

  pot_->setParameters(ref_copy_, startParameter_, actionLenght_);
  assert(pot_);
  startLenght_ = startParameter_;
}

//-----------------------------------------------------------------------------

void ToonzExt::StrokeParametricDeformer::setMouseMove(double vx, double vy) {
  vx_ = vx;
  vy_ = vy;
}

//-----------------------------------------------------------------------------

ToonzExt::StrokeParametricDeformer::~StrokeParametricDeformer() {
  delete pot_;
  delete ref_copy_;
}

//-----------------------------------------------------------------------------

TThickPoint ToonzExt::StrokeParametricDeformer::getDisplacement(
    const TStroke &stroke, double w) const {
  // conversion in absolute system for shape deformation
  double val = pot_->value(w);

  assert(val >= 0);

  // amplify movement
  return TThickPoint(vx_ * val, vy_ * val, 0.0);
}

//-----------------------------------------------------------------------------

TThickPoint ToonzExt::StrokeParametricDeformer::getDisplacementForControlPoint(
    const TStroke &stroke, UINT n) const {
  double w = stroke.getParameterAtControlPoint(n);
  return this->getDisplacement(stroke, w);
}

//-----------------------------------------------------------------------------

TThickPoint
ToonzExt::StrokeParametricDeformer::getDisplacementForControlPointLen(
    const TStroke &stroke, double cpLen) const {
  return this->getDisplacement(stroke, cpLen);
}

//-----------------------------------------------------------------------------

double ToonzExt::StrokeParametricDeformer::getDelta(const TStroke &stroke,
                                                    double w) const {
#if 0
  double  w0 = w;
  if(w0 == 1.0)
    return norm( stroke.getSpeed(1.0));
  double tmp = stroke.getLength(w0);
  tmp+=1.0;
  double w1 = stroke.getParameterAtLength(tmp);

  /*
  double  w1 = w + 0.01;  
  if(w1>1.0)
  {
    w0 = 1.0 - 0.01;
    w1 = 1.0;
  }
  */

  const double  dx = 1.0; //stroke.getLength(w0,w1);
  TThickPoint pnt = this->getDisplacement(stroke,w1) - 
                    this->getDisplacement(stroke,w0);

  double  dy =  sqrt( sq(pnt.x) + sq(pnt.y) + sq(pnt.thick) );

  assert(dx!=0.0);
  return dy/dx;

  /*
  // the increaser is wrong than this value need to be the deformation
  //  value 

  // is norm 
  TThickPoint pnt = this->getDisplacement(stroke,
                                          w);
  return sqrt( sq(pnt.x) + sq(pnt.y) + sq(pnt.thick) );

  //return pot_->gradient(w);
  */
#endif
  return this->getDisplacement(stroke, w).y;
}

//-----------------------------------------------------------------------------

double ToonzExt::StrokeParametricDeformer::getMaxDiff() const { return diff_; }

//-----------------------------------------------------------------------------

void ToonzExt::StrokeParametricDeformer::getRange(double &from, double &to) {
  double x     = ref_copy_->getLength(startParameter_);
  double delta = x - actionLenght_ * 0.5;
  if (delta > 0)
    from = ref_copy_->getParameterAtLength(delta);
  else
    from = 0.0;

  delta = x + actionLenght_ * 0.5;
  if (delta < ref_copy_->getLength())
    to = ref_copy_->getParameterAtLength(delta);
  else
    to = 1.0;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
