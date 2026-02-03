

#include "toonz/tobjecthandle.h"

#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/txsheet.h"
#include "tvectorimage.h"
#include "tstroke.h"

namespace {

void clearImage(TVectorImage *img) {
  while (img->getStrokeCount()) img->deleteStroke(0);
}

// obj -> image
void getSpline(TVectorImage *dstImg, TStageObjectId objId,
               TStageObjectSpline *currentSpline) {
  QMutexLocker lock(dstImg->getMutex());
  clearImage(dstImg);
  if (currentSpline)
    dstImg->addStroke(new TStroke(*currentSpline->getStroke()));
}

// image -> obj
void setSpline(TVectorImage *srcImg, TStageObjectId objId,
               TStageObjectSpline *currentSpline) {
  if (!currentSpline) return;
  if (srcImg->getStrokeCount() == 0) {
    double d = 30;
    std::vector<TThickPoint> points;
    points.push_back(TPointD(-d, 0));
    points.push_back(TPointD(0, 0));
    points.push_back(TPointD(d, 0));
    srcImg->addStroke(new TStroke(points), false);
  }
  TStroke *stroke = srcImg->getStroke(0);
  currentSpline->setStroke(new TStroke(*stroke));
}

}  // namespace

//=============================================================================
// TXshLevelHandle
//-----------------------------------------------------------------------------

TObjectHandle::TObjectHandle()
    : m_objectId(TStageObjectId::NoneId)
    , m_isSpline(false)
    , m_splineImage(0)
    , m_currentSpline(0) {
  m_splineImage = new TVectorImage();
  m_splineImage->addRef();
}

//-----------------------------------------------------------------------------

TObjectHandle::~TObjectHandle() { m_splineImage->release(); }

//-----------------------------------------------------------------------------

TStageObjectId TObjectHandle::getObjectId() { return m_objectId; }

//-----------------------------------------------------------------------------

void TObjectHandle::setObjectId(TStageObjectId objectId) {
  if (m_objectId != objectId) {
    m_objectId = objectId;
    m_isSpline = false;
    clearImage(m_splineImage);
    emit objectSwitched();
  }
}

//-----------------------------------------------------------------------------

void TObjectHandle::setIsSpline(bool isSpline) {
  if (m_isSpline != isSpline) {
    m_isSpline = isSpline;
    emit objectSwitched();
  }
}

//-----------------------------------------------------------------------------

void TObjectHandle::commitSplineChanges() {
  setSpline(m_splineImage, m_objectId, m_currentSpline);
  emit splineChanged();
}

//-----------------------------------------------------------------------------

void TObjectHandle::setSplineObject(TStageObjectSpline *splineObject) {
  m_currentSpline = splineObject;
  if (m_isSpline && m_currentSpline)
    getSpline(m_splineImage, m_objectId, m_currentSpline);
}
