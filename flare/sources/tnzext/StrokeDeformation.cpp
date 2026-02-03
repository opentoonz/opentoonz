

// StrokeDeformation.cpp: implementation of the StrokeDeformation class.
//
//-----------------------------------------------------------------------------
#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif

#include "ext/StrokeDeformation.h"
#include "ext/SquarePotential.h"
#include "ext/StrokeParametricDeformer.h"
#include "ext/NotSymmetricBezierPotential.h"
#include "ext/SmoothDeformation.h"
#include "ext/ContextStatus.h"
#include "ext/Designer.h"
//#include "ext/TriParam.h"

#include "ext/StraightCornerDeformation.h"
#include "ext/CornerDeformation.h"

#include "ext/StrokeDeformationImpl.h"
#include "DeformationSelector.h"

#include <tcurves.h>
#include <tstrokeutil.h>
#include <tmathutil.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "tthreadmessage.h"

using namespace ToonzExt;
using namespace std;

/**
 * @brief Manage a mutable context.
 */
static TThread::Mutex s_mutex;

//-----------------------------------------------------------------------------

StrokeDeformation::StrokeDeformation() {
  state_           = CREATED;
  deformationImpl_ = SmoothDeformation::instance();
}

//-----------------------------------------------------------------------------

StrokeDeformation::~StrokeDeformation() {}

//-----------------------------------------------------------------------------

void StrokeDeformation::activate(const ContextStatus *status) {
  QMutexLocker sl(&s_mutex);

  assert(status && "Status is null!!!");
  if (!status) return;

  if (state_ == RESETTED) delete this->deactivate();

  if (state_ == CREATED || state_ == DEACTIVE) {
    deformationImpl_ = this->retrieveDeformator(status);
  } else {
    // if it is a not valid entry state_ recover
    assert(deformationImpl_ && "Can not find a valid deformator!!!");
    if (!deformationImpl_) return;
    deformationImpl_->reset();
    deformationImpl_ = DeformationSelector::instance()->getDeformation(status);
  }

  assert(deformationImpl_ && "Deformation is null!!!");
  if (!deformationImpl_) return;

  if (!deformationImpl_->activate_impl(status)) {
    deformationImpl_->reset();
    state_ = DEACTIVE;
    return;
  }

  state_ = ACTIVE;
}

//-----------------------------------------------------------------------------

void StrokeDeformation::update(const TPointD &delta) {
  QMutexLocker sl(&s_mutex);

  assert(deformationImpl_ && "Deformation is null!!!");
  if (!deformationImpl_) {
    state_ = DEACTIVE;
    return;
  }

  if (state_ != ACTIVE && state_ != UPDATING) {
    deformationImpl_->reset();
    state_ = ACTIVE;
    return;
  }

  deformationImpl_->update_impl(delta);
  state_ = UPDATING;
}

//-----------------------------------------------------------------------------

TStroke *StrokeDeformation::deactivate() {
  QMutexLocker sl(&s_mutex);

  if (!deformationImpl_) {
    state_ = DEACTIVE;
    return 0;
  }

  if (state_ != ACTIVE && state_ != UPDATING) {
    state_ = DEACTIVE;
    deformationImpl_->reset();
    return 0;
  }

  state_           = DEACTIVE;
  TStroke *out     = deformationImpl_->deactivate_impl();
  deformationImpl_ = 0;
  return out;
}

//-----------------------------------------------------------------------------

void StrokeDeformation::reset() {
  QMutexLocker sl(&s_mutex);

  state_ = RESETTED;
}

//-----------------------------------------------------------------------------

void StrokeDeformation::draw(Designer *designer) {
  QMutexLocker sl(&s_mutex);

  assert(designer && "Designer is null!!!");
  if (!deformationImpl_ || !designer) return;

  // this is to draw the icon
  deformationImpl_->draw(designer);

  // this is to draw stroke when change something
  designer->draw(this);
}

//-----------------------------------------------------------------------------

void StrokeDeformation::check(const ContextStatus *status) {
  QMutexLocker sl(&s_mutex);
  if (state_ != UPDATING) deformationImpl_ = this->retrieveDeformator(status);
}

//-----------------------------------------------------------------------------

const TStroke *StrokeDeformation::getCopiedStroke() const {
  QMutexLocker sl(&s_mutex);
  if (deformationImpl_) return deformationImpl_->getCopiedStroke();
  return 0;
}

//-----------------------------------------------------------------------------

const TStroke *StrokeDeformation::getStroke() const {
  QMutexLocker sl(&s_mutex);
  if (deformationImpl_) return deformationImpl_->getStrokeSelected();
  return 0;
}

//-----------------------------------------------------------------------------

const TStroke *StrokeDeformation::getTransformedStroke() const {
  QMutexLocker sl(&s_mutex);
  if (deformationImpl_) return deformationImpl_->getTransformedStroke();
  return 0;
}

//-----------------------------------------------------------------------------

void StrokeDeformation::recover() {
  QMutexLocker sl(&s_mutex);
  assert(deformationImpl_ && "Deformation is null!!!");
  if (!deformationImpl_) return;

  deformationImpl_->reset();
  //  this->clearStatus();

  // deformationImpl_ = DeformationSelector::instance()->getDeformation(status);
  // assert(deformationImpl_ &&  "Deformation is null!!!" );
  // if( !deformationImpl_ )
  //  return;
  // deformationImpl_->activate_impl(status);
}

//-----------------------------------------------------------------------------

const ContextStatus *StrokeDeformation::getStatus() const {
  QMutexLocker sl(&s_mutex);
  if (deformationImpl_) return deformationImpl_->getImplStatus();
  return 0;
}

//-----------------------------------------------------------------------------

ToonzExt::Interval StrokeDeformation::getExtremes() const {
  QMutexLocker sl(&s_mutex);
  if (!deformationImpl_) return Interval(-1, -1);
  return deformationImpl_->getExtremes();
}

//-----------------------------------------------------------------------------

StrokeDeformationImpl *StrokeDeformation::retrieveDeformator(
    const ContextStatus *status) {
  QMutexLocker sl(&s_mutex);
  return DeformationSelector::instance()->getDeformation(status);
}

//-----------------------------------------------------------------------------

int StrokeDeformation::getCursorId() const {
  QMutexLocker sl(&s_mutex);
  if (!deformationImpl_) return -1;
  return deformationImpl_->getCursorId();
}

//-----------------------------------------------------------------------------

#ifdef _DEBUG

const Potential *StrokeDeformation::getPotential() const {
  assert(deformationImpl_ && "Deformation is null!!!");
  if (!deformationImpl_) return 0;
  return deformationImpl_->getPotential();
}

//-----------------------------------------------------------------------------

const StrokeDeformationImpl *StrokeDeformation::getDeformationImpl() const {
  return deformationImpl_;
}
#endif

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
