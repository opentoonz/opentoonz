

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "ext/StrokeDeformationImpl.h"
#include "ext/StrokeDeformation.h"
#include "ext/SquarePotential.h"
#include "ext/StrokeParametricDeformer.h"
// #include "ext/NotSymmetricBezierPotential.h"
#include "ext/ContextStatus.h"
#include "ext/Designer.h"
// #include "ext/TriParam.h"

#include <tcurves.h>
#include <tstrokeutil.h>
// #include <tvectorimage.h>
#include <tmathutil.h>

#include <algorithm>
#include <iterator>
#include <vector>
#include <memory>  // Added for smart pointers

#include "DeformationSelector.h"

namespace ToonzExt {

namespace {

/**
 * Compute reduction factor based on pixel size to avoid reducing curves with
 * great zoom out
 * @param pixelSize Pixel size
 * @param factor Base factor
 * @return Computed reduction factor
 */
double computeReductionFactor(double pixelSize, double factor) {
  assert(pixelSize > 0.0 && factor > 0.0);
  if (factor <= 0.0) factor = 1.0;
  if (pixelSize <= 0.0 || pixelSize > 1.0) pixelSize = 1.0;
  return pixelSize * factor;
}

//---------------------------------------------------------------------------

/**
 * Retrieve parameter at length with offset
 * @param stroke2change Stroke to process
 * @param length Target length
 * @param offset Offset parameter
 * @return Parameter at computed length
 */
double retrieveParamAtLengthWithOffset(const TStroke *stroke2change,
                                       double length,  // = 0.5 * strokeLength,
                                       double offset) {
  if (!isValid(stroke2change) || !isValid(offset) || length < 0) return -1;

  double strokeLength = stroke2change->getLength();
  assert(strokeLength >= 0.0 && "Not valid length");
  if (strokeLength < 0) {
    return -1;
  }

  double lengthAtW = stroke2change->getLength(offset);

  assert(strokeLength >= lengthAtW &&
         "Position of parameter is greater than stroke length!!!");

  if (strokeLength < lengthAtW) return -1;

  double newLength = -1;

  if (stroke2change->isSelfLoop()) {
    if (length >= 0)
      newLength = length > lengthAtW ? length + lengthAtW : lengthAtW - length;
  } else
    newLength = std::min(length + lengthAtW, strokeLength);

  return stroke2change->getParameterAtLength(newLength);
}

//---------------------------------------------------------------------------

/**
 * Rotate stroke to bring a specific parameter to position 0
 * @param stroke2change Original stroke
 * @param rotated Output rotated stroke
 * @param from From parameter (input/output)
 * @param to To parameter (input/output)
 * @param old_w0_pos Original position of w=0
 * @return true if rotation successful
 */
bool rotateStroke(const TStroke *stroke2change, TStroke *&rotated, double &from,
                  double &to, TPointD &old_w0_pos) {
  if (!stroke2change || !isValid(from) || !isValid(to)) return false;

  rotated = 0;

  // save position of w=0 (useful to retrieve this position after changes)
  old_w0_pos = convert(stroke2change->getControlPoint(0));

  double rotateAtLength = stroke2change->getLength(to);

  assert(rotateAtLength >= 0.0);
  if (rotateAtLength < 0) return false;

  // TStroke*
  rotated = ToonzExt::rotateControlPoint(stroke2change, ToonzExt::EvenInt(0),
                                         rotateAtLength);
  if (!rotated) return false;

  from = rotated->getW(stroke2change->getPoint(from));
  to   = rotated->getW(stroke2change->getPoint(to));

  // save some other information (style, etc..)
  ToonzExt::cloneStrokeStatus(stroke2change, rotated);
  return true;
}

//---------------------------------------------------------------------------

/**
 * Rotate control point to bring a specific point to position 0
 * @param stroke Stroke to rotate
 * @param pnt Point to bring to position 0
 * @return Rotated stroke or nullptr
 */
TStroke *rotateControlPointAtPoint(const TStroke *stroke, const TPointD &pnt) {
  if (!stroke || tdistance2(stroke->getPoint(0.0), pnt) < sq(TConsts::epsilon))
    return 0;

  double w = stroke->getW(pnt);
  // Removed unused variable: TPointD theSamePnt = stroke->getPoint(w);
  double length = stroke->getLength(w);
  return ToonzExt::rotateControlPoint(stroke, ToonzExt::EvenInt(0), length);
}

//---------------------------------------------------------------------------

/**
 * Find extremes based on tool action length
 * @param toolActionLength Length of tool action
 * @param stroke Stroke to process
 * @param w Parameter position
 * @param out Output interval
 * @return true if successful
 */
bool findExtremesFromActionLength(double toolActionLength,
                                  const TStroke *stroke, double w,
                                  ToonzExt::Interval &out) {
  out = ToonzExt::Interval(-1.0, -1.0);

  if (!stroke || 0.0 > w || w > 1.0) return false;

  double emiToolSize   = 0.5 * toolActionLength;
  double strokelength  = stroke->getLength();
  double lengthAtParam = stroke->getLength(w);

  assert(emiToolSize >= 0.0 && strokelength >= 0.0 && lengthAtParam >= 0.0);

  if (emiToolSize > strokelength * 0.5) {
    if (stroke->isSelfLoop()) {
      emiToolSize = strokelength * 0.5;

      // first and second are the same
      lengthAtParam += emiToolSize;

      if (lengthAtParam > strokelength) lengthAtParam -= strokelength;
      out.first = out.second = stroke->getParameterAtLength(lengthAtParam);
    } else {
      out.first  = 0.0;
      out.second = 1.0;
    }

    return true;
  }

  if (emiToolSize < 0.0 || strokelength < 0.0 || lengthAtParam < 0.0)
    return false;

  // [out] now is length, range 0,length
  out.first  = lengthAtParam - emiToolSize;
  out.second = lengthAtParam + emiToolSize;

  if (stroke->isSelfLoop()) {
    if (out.first < 0.0) {
      out.first = strokelength + out.first;
      assert(out.first < strokelength);
    }

    if (out.second > strokelength) {
      out.second = out.second - strokelength;
      assert(0.0 < out.second && out.second < strokelength);
    }
  } else {
    out.first  = std::max(out.first, 0.0);
    out.second = std::min(out.second, strokelength);
  }

  // [out] now is parameter, range 0,1
  out.first  = stroke->getParameterAtLength(out.first);
  out.second = stroke->getParameterAtLength(out.second);
#ifdef _DEBUG
  if (!stroke->isSelfLoop()) assert(out.first <= out.second);
#endif
  return true;
}

//---------------------------------------------------------------------------

/**
 * Check if two strokes are different
 * @param ref1 First stroke
 * @param ref2 Second stroke
 * @return true if strokes are different
 */
bool areDifferent(const TStroke *ref1, const TStroke *ref2) {
  if (!isValid(ref1) || !isValid(ref2)) return true;

  int cpCount1 = ref1->getControlPointCount();
  int cpCount2 = ref2->getControlPointCount();
  if (cpCount1 != cpCount2) return true;

  while (--cpCount1 >= 0) {
    if (ref1->getControlPoint(cpCount1) != ref2->getControlPoint(cpCount1))
      return true;
  }
  assert(cpCount1 == -1);
  return false;
}

//---------------------------------------------------------------------------
}  // namespace

//-----------------------------------------------------------------------------

TStroke *StrokeDeformationImpl::copyOfLastSelectedStroke_ =
    nullptr;  // deep copy stroke selected previously

//-----------------------------------------------------------------------------

const ContextStatus *&StrokeDeformationImpl::getImplStatus() {
  // if multi threading this datas need to be serialized
  // a ref to status
  static const ContextStatus *contextStatus_instance = nullptr;
  return contextStatus_instance;
}

//-----------------------------------------------------------------------------

TStroke *&StrokeDeformationImpl::getLastSelectedStroke() {
  static TStroke *lastSelectedStroke_instance = nullptr;
  return lastSelectedStroke_instance;
}

//-----------------------------------------------------------------------------

void StrokeDeformationImpl::setLastSelectedStroke(TStroke *stroke) {
  TStroke *&lastSelStroke = getLastSelectedStroke();
  lastSelStroke           = stroke;
  if (lastSelStroke) {
    delete copyOfLastSelectedStroke_;
    copyOfLastSelectedStroke_ = new TStroke(*lastSelStroke);
  }
}

//-----------------------------------------------------------------------------

int &StrokeDeformationImpl::getLastSelectedDegree() {
  static int lastSelectedDegree_instance = -1;
  return lastSelectedDegree_instance;
}

//-----------------------------------------------------------------------------

void StrokeDeformationImpl::setLastSelectedDegree(int degree) {
  getLastSelectedDegree() = degree;
}

//-----------------------------------------------------------------------------

ToonzExt::Intervals &StrokeDeformationImpl::getSpiresList() {
  static ToonzExt::Intervals listOfSpire_instance;
  return listOfSpire_instance;
}

//-----------------------------------------------------------------------------

ToonzExt::Intervals &StrokeDeformationImpl::getStraightsList() {
  static ToonzExt::Intervals listOfStraight_instance;
  return listOfStraight_instance;
}

//-----------------------------------------------------------------------------

StrokeDeformationImpl::StrokeDeformationImpl()
    : potential_(nullptr)
    , deformer_(nullptr)
    , stroke2manipulate_(nullptr)
    , old_w0_(-1)
    , old_w0_pos_(TConsts::napd)
    , shortcutKey_(ContextStatus::NONE)
    , cursorId_(-1) {
  // Direct initialization without virtual call
  old_w0_            = -1;
  old_w0_pos_        = TConsts::napd;
  deformer_          = nullptr;
  stroke2manipulate_ = nullptr;
}

//-----------------------------------------------------------------------------

StrokeDeformationImpl::~StrokeDeformationImpl() {
  clearPointerContainer(strokes_);
  delete potential_;
  potential_ = nullptr;
  delete deformer_;
  deformer_ = nullptr;
  delete copyOfLastSelectedStroke_;
  copyOfLastSelectedStroke_ = nullptr;
}

//-----------------------------------------------------------------------------

bool StrokeDeformationImpl::activate_impl(const ContextStatus *status) {
  assert(status && "Not status available");

  if (!status || !this->init(status)) return false;

  double w = status->w_;

  ToonzExt::Interval extremes = this->getExtremes();

  // Use unique_ptr for automatic cleanup
  std::unique_ptr<TStroke> stroke2transformPtr;
  TStroke *stroke2transform = nullptr;

  if (!this->computeStroke2Transform(status, stroke2transform, w, extremes)) {
    delete stroke2transform;  // Clean up if function failed
    return false;
  }

  // Wrap in unique_ptr for automatic cleanup
  stroke2transformPtr.reset(stroke2transform);

  // to avoid strange behaviour in limit's value
  if (areAlmostEqual(extremes.first, w)) w = extremes.first;

  if (areAlmostEqual(extremes.second, w)) w = extremes.second;

  assert(extremes.first <= w && w <= extremes.second);

  if (extremes.first > w || w > extremes.second) {
    return false;
  }

  std::vector<double> splitParameter;
  splitParameter.push_back(extremes.first);
  splitParameter.push_back(extremes.second);

  assert(strokes_.empty());
  if (!strokes_.empty()) clearPointerContainer(strokes_);
  splitStroke(*stroke2transform, splitParameter, strokes_);

  assert(strokes_.size() == 3);

  // I don't know how to manage properly this case
  if (strokes_.size() != 3) {
    clearPointerContainer(strokes_);
    return false;
  }

  // stroke to change
  stroke2manipulate_ = strokes_[1];
  assert(stroke2manipulate_ && " Not valid reference to stroke to move!!!");
  if (!stroke2manipulate_) return false;

  // remove empty stroke
  {
    TStroke *tmp_stroke = strokes_[2];
    if (isAlmostZero(tmp_stroke->getLength())) {
      std::vector<TStroke *>::iterator it = strokes_.begin();
      std::advance(it, 2);
      strokes_.erase(it);
      delete tmp_stroke;
    }
    tmp_stroke = strokes_[0];
    if (isAlmostZero(tmp_stroke->getLength())) {
      strokes_.erase(strokes_.begin());
      delete tmp_stroke;
    }
    // little movement of control points to have not empty
    //  length
    if (isAlmostZero(stroke2manipulate_->getLength())) {
      int count = stroke2manipulate_->getControlPointCount() - 1;
      assert(count > 0);

      TThickPoint pnt0  = stroke2manipulate_->getControlPoint(0),
                  pntn  = stroke2manipulate_->getControlPoint(count),
                  delta = TThickPoint(2.0 * TConsts::epsilon,
                                      -2.0 * TConsts::epsilon, 0.0);

      stroke2manipulate_->setControlPoint(0, pnt0 - delta);
      stroke2manipulate_->setControlPoint(count, pntn + delta);
    }
  }

  // simple check for empty chunk
  int cp_count = stroke2manipulate_->getChunkCount();
  const TThickQuadratic *tq;
  bool haveEmpty = false;
  while (--cp_count >= 0) {
    tq = stroke2manipulate_->getChunk(cp_count);
    //    assert( tq->getLength() != 0.0 );
    if (tq->getLength() == 0.0) haveEmpty = true;
  }

  if (haveEmpty) {
    const double reductionFactor =
        computeReductionFactor(getImplStatus()->pixelSize_, 1.0);
    stroke2manipulate_->reduceControlPoints(reductionFactor);
  }

  TPointD pntOnStroke = stroke2transform->getPoint(w);

  // parameter need to be recomputed to match
  // with new stroke
  w = stroke2manipulate_->getW(pntOnStroke);

  //  prevStyle_ = stroke2transform->getStyle();
  //  set a style for error
  //  stroke2manipulate_->setStyle( 4 );

  // in this deformer all stroke need to be moved
  double lengthOfAction = this->findActionLength();

  bool exception = false;

  try {
    // create a new stroke deformer
    delete deformer_;
    deformer_ = new StrokeParametricDeformer(
        lengthOfAction, w, stroke2manipulate_, potential_->clone());
  } catch (std::invalid_argument &) {
    exception = true;
    assert(!"Invalid argument");
  } catch (...) {
    exception = true;
  }

  assert(deformer_ && "Deformer is not available");

  if (exception) {
    deformer_ = nullptr;
    return false;
  }

  assert(getImplStatus() != nullptr && "ContextStatus is null???");

  if (!getImplStatus()) {
    delete deformer_;
    deformer_ = nullptr;
    this->reset();
    return false;
  }
  // change the threshold value of increser,
  //  if value is greater than diff add control points
  deformer_->setDiff(getImplStatus()->deformerSensitivity_);

  // just to be sure to have a control point where
  // stroke is selected
  stroke2manipulate_->insertControlPoints(w);

  //  two different case:
  //  (1) point at w=0 is not in stroke to transform
  //  (2) point at w=0 is in stroke to transform
  //  in case (2) it is mandatory to retrieve the
  //  parameter where stroke2transform is equal to
  //  w=0 and store it.
  //  old_w0_pos_ will be used in deactivate method
  if (old_w0_pos_ != TConsts::napd) {
    double w    = stroke2manipulate_->getW(old_w0_pos_);
    TPointD pnt = stroke2manipulate_->getPoint(w);

    if (tdistance2(pnt, old_w0_pos_) < sq(TConsts::epsilon)) {
      // the this position can be modified during drag
      old_w0_ = w;
    } else
      old_w0_ = -1;
  }

  bool test = increaseControlPoints(*stroke2manipulate_, *deformer_,
                                    getImplStatus()->pixelSize_);
  /*
// New Increaser behaviour
deformer_->setMouseMove(0,100);
extremes_ = Toonz::increase_cp (stroke2manipulate_,
deformer_);
//*/
  if (!test) return false;

  stroke2manipulate_->disableComputeOfCaches();
  return true;
}

//-----------------------------------------------------------------------------

void StrokeDeformationImpl::update_impl(const TPointD &delta) {
  assert(stroke2manipulate_ && deformer_ &&
         "Stroke and Deformer are available");

  if (!stroke2manipulate_ || !deformer_) return;

  // update delta in deformer
  deformer_->setMouseMove(delta.x, delta.y);

  modifyControlPoints(*stroke2manipulate_, *deformer_);
}

//-----------------------------------------------------------------------------

TStroke *StrokeDeformationImpl::deactivate_impl() {
  if (!stroke2manipulate_ || !getImplStatus()) {
    this->reset();
    return nullptr;
  }

  // retrieve the point
  if (old_w0_ != -1) old_w0_pos_ = stroke2manipulate_->getPoint(old_w0_);

  const double reductionFactor =
      computeReductionFactor(getImplStatus()->pixelSize_, 3.0);

  // stroke2manipulate_->enableComputeOfCaches ();
  // stroke2manipulate_->reduceControlPoints (reductionFactor);
  const int size = stroke2manipulate_->getControlPointCount();
  std::vector<TThickPoint> pnt(size);
  for (int i = 0; i < size; ++i)
    pnt[i] = stroke2manipulate_->getControlPoint(i);

  std::vector<TStroke *>::iterator it =
      std::find(strokes_.begin(), strokes_.end(), stroke2manipulate_);
  assert(it != strokes_.end());

  int pos            = std::distance(strokes_.begin(), it);
  stroke2manipulate_ = nullptr;
  delete strokes_[pos];

  TStroke *tmpStroke = new TStroke(pnt);
  assert((tmpStroke != nullptr) && "Not valid stroke!");
  strokes_[pos] = tmpStroke;
  strokes_[pos]->reduceControlPoints(reductionFactor);

  // strokes_[pos] = TStroke::interpolate(pnt,
  //                                   reductionFactor,
  //                                   false);

  TStroke *ref = Toonz::merge(strokes_);

  ToonzExt::cloneStrokeStatus(getImplStatus()->stroke2change_, ref);

  if (old_w0_pos_ != TConsts::napd) {
    // restore (if necessary) the initial position of
    // a stroke
    TStroke *rotated = rotateControlPointAtPoint(ref, old_w0_pos_);
    if (rotated) {
      delete ref;
      ref = rotated;
      if (getImplStatus())
        ToonzExt::cloneStrokeStatus(getImplStatus()->stroke2change_, ref);
      else {
        delete ref;
        ref = nullptr;
      }
    }
  }

  this->reset();
  return ref;
}

//-----------------------------------------------------------------------------

void StrokeDeformationImpl::reset() {
  old_w0_     = -1;
  old_w0_pos_ = TConsts::napd;

  deformer_       = nullptr;
  getImplStatus() = nullptr;
  this->setLastSelectedDegree(-1);
  this->setLastSelectedStroke(nullptr);
  this->getSpiresList().clear();
  this->getStraightsList().clear();

  stroke2manipulate_ = nullptr;
  clearPointerContainer(
      strokes_);  // stroke2move is deleted here (it is in vector)
}

//-----------------------------------------------------------------------------

TStroke *StrokeDeformationImpl::getTransformedStroke() {
  return this->stroke2manipulate_;
}

//-----------------------------------------------------------------------------

ToonzExt::Potential *StrokeDeformationImpl::getPotential() {
  return this->potential_;
}

//-----------------------------------------------------------------------------

void StrokeDeformationImpl::draw(Designer *designer) {}

//-----------------------------------------------------------------------------

void StrokeDeformationImpl::setPotential(Potential *potential) {
  assert(potential);
  potential_ = potential;
}

//-----------------------------------------------------------------------------

bool StrokeDeformationImpl::check(const ContextStatus *status) {
  if (!status || !this->init(status)) return false;

  return this->check_(status);
}

//-----------------------------------------------------------------------------

TPointD &StrokeDeformationImpl::oldW0() { return old_w0_pos_; }

//-----------------------------------------------------------------------------

ToonzExt::Interval StrokeDeformationImpl::getExtremes() {
  ToonzExt::Interval extremes = ToonzExt::Interval(-1.0, -1.0);

  if (!getImplStatus()) return extremes;

  // if it is manual
  if (getImplStatus()->isManual_ == true) {
    findExtremesFromActionLength(getImplStatus()->lengthOfAction_,
                                 getImplStatus()->stroke2change_,
                                 getImplStatus()->w_, extremes);
  } else {
    // if there are some special key down all will be managed in deformation
    this->findExtremes_(getImplStatus(), extremes);
  }

  return extremes;
}

//-----------------------------------------------------------------------------

bool StrokeDeformationImpl::init(const ContextStatus *status) {
  if (!status || !isValid(status->stroke2change_) || !isValid(status->w_)) {
    this->reset();
    return false;
  }

  getImplStatus() = status;

  if (!this->getLastSelectedStroke() ||
      this->getLastSelectedStroke() != status->stroke2change_ ||
      areDifferent(copyOfLastSelectedStroke_, status->stroke2change_) ||
      (this->getLastSelectedDegree() == -1) ||
      (this->getLastSelectedDegree() != status->cornerSize_)) {
    this->getSpiresList().clear();
    this->getStraightsList().clear();
    // recompute cache
    ToonzExt::findCorners(status->stroke2change_, this->getSpiresList(),
                          this->getStraightsList(), status->cornerSize_,
                          TConsts::epsilon);
    this->setLastSelectedStroke(status->stroke2change_);
    this->setLastSelectedDegree(status->cornerSize_);
  }
  return true;
}

//-----------------------------------------------------------------------------

bool StrokeDeformationImpl::computeStroke2Transform(
    const ContextStatus *status, TStroke *&stroke2transform, double &w,
    ToonzExt::Interval &extremes) {
  // Use unique_ptr for local cleanup
  std::unique_ptr<TStroke> localStrokePtr;

  if (!status || !isValid(w)) {
    // Clean up if stroke2transform was allocated before failure
    delete stroke2transform;
    stroke2transform = nullptr;
    return false;
  }

  // Set to null initially
  stroke2transform = nullptr;

  if (status->stroke2change_->isSelfLoop()) {
    if (extremes.first > extremes.second) {
      double new_w = (extremes.first + extremes.second) * 0.5;

      if (!rotateStroke(status->stroke2change_, stroke2transform, w, new_w,
                        old_w0_pos_)) {
        // Clean up on failure
        delete stroke2transform;
        stroke2transform = nullptr;
        return false;
      }

      // Save current status
      const ContextStatus *oldStatus = getImplStatus();

      // Create temporary status on stack but don't store pointer in static
      // variable
      ContextStatus tmpStatus  = *status;
      tmpStatus.stroke2change_ = stroke2transform;
      tmpStatus.w_             = w;

      // Temporarily set the status for check() function
      getImplStatus() = &tmpStatus;
      this->check(&tmpStatus);
      extremes = this->getExtremes();

      // Restore original status
      getImplStatus() = oldStatus;

      this->init(status);
      return true;
    } else if (extremes.first == extremes.second) {
      double positionOfFixPoint = -1;

      // there are two different cases
      //  a) only one corner
      //  b) only two corners
      switch (this->getSpiresList().size()) {
      case 0:
        assert(extremes.first == -1);
        positionOfFixPoint = retrieveParamAtLengthWithOffset(
            status->stroke2change_, status->stroke2change_->getLength() * 0.5,
            w);
        break;
      case 1:
        if (extremes.first != -1)
          positionOfFixPoint = retrieveParamAtLengthWithOffset(
              status->stroke2change_, status->stroke2change_->getLength() * 0.5,
              w);
        else
          positionOfFixPoint = this->getSpiresList()[0].first;
        break;
      case 2:
      // also forced mode
      default:
        assert(extremes.first != -1);
        positionOfFixPoint = extremes.first;
        break;
      }

      if (!rotateStroke(status->stroke2change_, stroke2transform, w,
                        positionOfFixPoint, old_w0_pos_)) {
        // Clean up on failure
        delete stroke2transform;
        stroke2transform = nullptr;
        return false;
      }

      extremes = ToonzExt::Interval(0.0, 1.0);
      return true;
    }
  }

  if (!isValid(extremes.first) || !isValid(extremes.second)) {
    // Clean up on failure
    delete stroke2transform;
    stroke2transform = nullptr;
    return false;
  }

  try {
    if (!stroke2transform) {
      stroke2transform = new TStroke(*status->stroke2change_);
    }
  } catch (...) {
    // Clean up on exception
    delete stroke2transform;
    stroke2transform = nullptr;
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
}  // namespace ToonzExt
