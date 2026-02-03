

#include "filmstripcommand.h"
#include "tapp.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tframehandle.h"
#include "tinbetween.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "toonzqt/selection.h"
#include "toonzqt/dvdialog.h"
#include "drawingdata.h"
#include "toonzqt/strokesdata.h"
#include "toonzqt/toonzimagedata.h"
#include "timageCache.h"
#include "tools/toolutils.h"
#include "toonzqt/icongenerator.h"

#include "tundo.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzimageutils.h"
#include "trop.h"

#include "toonzqt/gutil.h"

#include <QApplication>
#include <QClipboard>

class MyInbetweener {
  TVectorImageP m_vi0, m_vi1;

public:
  MyInbetweener(const TVectorImageP &vi0, const TVectorImageP &vi1);
  TVectorImageP tween(double t);
  void stabilizeSegments(TVectorImageP &image);
  void stabilizeSegments(TStroke *stroke);
};

MyInbetweener::MyInbetweener(const TVectorImageP &vi0, const TVectorImageP &vi1)
    : m_vi0(vi0), m_vi1(vi1) {
  stabilizeSegments(m_vi0);
  stabilizeSegments(m_vi1);
}

void MyInbetweener::stabilizeSegments(TVectorImageP &image) {
  for (int i = 0; i < image->getStrokeCount(); i++)
    stabilizeSegments(image->getStroke(i));
}

void MyInbetweener::stabilizeSegments(TStroke *stroke) {
  for (int j = 0; j + 4 < stroke->getControlPointCount(); j += 4) {
    TThickPoint q0 = stroke->getControlPoint(j);
    TThickPoint q4 = stroke->getControlPoint(j + 4);
    TPointD p0     = convert(q0);
    TPointD p1     = convert(stroke->getControlPoint(j + 1));
    TPointD p2     = convert(stroke->getControlPoint(j + 2));
    TPointD p3     = convert(stroke->getControlPoint(j + 3));
    TPointD p4     = convert(q4);
    TPointD v      = normalize(p4 - p0);
    TPointD u      = rotate90(v);
    double eps     = tdistance(p0, p4) * 0.1;
    if (fabs(u * (p2 - p0)) < eps && fabs(u * (p1 - p0)) < eps &&
        fabs(u * (p3 - p0)) < eps) {
      double e                  = 0.001;
      double d2                 = norm2(p4 - p0);
      if (e * e * 6 * 6 > d2) e = sqrt(d2) / 6;
      TThickPoint q1(p0 + v * e, q0.thick);
      TThickPoint q3(p4 - v * e, q4.thick);
      stroke->setControlPoint(j + 1, q1);
      stroke->setControlPoint(j + 3, q3);
    }
  }
}

TVectorImageP MyInbetweener::tween(double t) {
  TVectorImageP vi = m_vi0->clone();
  int n            = tmin(m_vi0->getStrokeCount(), m_vi1->getStrokeCount());
  for (int i = 0; i < n; i++) {
    TStroke *stroke0 = m_vi0->getStroke(i);
    TStroke *stroke1 = m_vi1->getStroke(i);
    TStroke *stroke  = vi->getStroke(i);
    int m =
        tmin(stroke0->getControlPointCount(), stroke1->getControlPointCount());
    for (int j = 0; j < m; j++) {
      TThickPoint p0 = stroke0->getControlPoint(j);
      TThickPoint p1 = stroke1->getControlPoint(j);
      TThickPoint p  = (1 - t) * p0 + t * p1;
      stroke->setControlPoint(j, p);
    }
    /*
for(int j=2;j+2<m;j+=2)
{
TThickPoint p0 = stroke0->getControlPoint(j-2);
TThickPoint p1 = stroke0->getControlPoint(j-1);
TThickPoint p2 = stroke0->getControlPoint(j);
TThickPoint p3 = stroke0->getControlPoint(j+1);
TThickPoint p4 = stroke0->getControlPoint(j+2);
if(tdistance2(p0,p1)<0.001 && tdistance2(p3,p4)<0.001)
{
p2 = 0.5*(p1+p2);
stroke->setControlPoint(j,p2);
}
}
*/
  }
  return vi;
}

//=============================================================================
// UndoInbetween
//-----------------------------------------------------------------------------

namespace {

class UndoInbetween : public TUndo {
  TXshSimpleLevelP m_level;
  vector<TFrameId> m_fids;
  vector<TVectorImageP> m_images;
  FilmstripCmd::InbetweenInterpolation m_interpolation;

public:
  UndoInbetween(TXshSimpleLevel *xl, std::vector<TFrameId> fids,
                FilmstripCmd::InbetweenInterpolation interpolation)
      : m_level(xl), m_fids(fids), m_interpolation(interpolation) {
    std::vector<TFrameId>::iterator it = fids.begin();
    // mi salvo tutte le immagine
    for (; it != fids.end(); ++it)
      m_images.push_back(m_level->getFrame(
          *it, false));  // non si fa clone perche' il livello subito dopo
                         // rilascia queste immagini a causa dell'inbetweener
  }

  void undo() const {
    UINT levelSize = m_fids.size() - 1;
    for (UINT count = 1; count != levelSize; count++) {
      TVectorImageP vImage = m_images[count];
      m_level->setFrame(m_fids[count], vImage);
      IconGenerator::instance()->invalidate(m_level.getPointer(),
                                            m_fids[count]);
    }

    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const {
    TFrameId fid0 = *m_fids.begin();
    TFrameId fid1 = *(--m_fids.end());
    FilmstripCmd::inbetweenWithoutUndo(m_level.getPointer(), fid0, fid1,
                                       m_interpolation);
  }

  int getSize() const {
    assert(!m_images.empty());
    return m_images.size() * m_images.front()->getStrokeCount() * 100;
  }
};

}  // namespace

//=============================================================================
// inbetween
//-----------------------------------------------------------------------------

void FilmstripCmd::inbetweenWithoutUndo(
    TXshSimpleLevel *sl, const TFrameId &fid0, const TFrameId &fid1,
    FilmstripCmd::InbetweenInterpolation interpolation) {
  if (!sl) return;
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  std::vector<TFrameId>::iterator it;
  it = std::find(fids.begin(), fids.end(), fid0);
  if (it == fids.end()) return;
  int ia = std::distance(fids.begin(), it);
  it     = std::find(fids.begin(), fids.end(), fid1);
  if (it == fids.end()) return;
  int ib = std::distance(fids.begin(), it);
  if (ib - ia < 2) return;

  TVectorImageP img0 = sl->getFrame(fid0, false);
  TVectorImageP img1 = sl->getFrame(fid1, false);
  if (!img0 || !img1) return;

  MyInbetweener inbetween(img0, img1);
  int i;
  for (i = ia + 1; i < ib; i++) {
    double t = (double)(i - ia) / (double)(ib - ia);
    double s = t;
    // in tutte le interpolazioni : s(0) = 0, s(1) = 1
    switch (interpolation) {
    case II_Linear:
      break;
    case II_EaseIn:
      s = t * t;
      break;  // s'(0) = 0
    case II_EaseOut:
      s = t * (2 - t);
      break;  // s'(1) = 0
    case II_EaseInOut:
      s = t * t * (3 - 2 * t);
      break;  // s'(0) = s'(1) = 0
    }

    TVectorImageP vi = inbetween.tween(s);
    sl->setFrame(fids[i], vi);
    IconGenerator::instance()->invalidate(sl, fids[i]);
  }
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void FilmstripCmd::inbetween(
    TXshSimpleLevel *sl, const TFrameId &fid0, const TFrameId &fid1,
    FilmstripCmd::InbetweenInterpolation interpolation) {
  std::vector<TFrameId> fids;
  std::vector<TFrameId> levelFids;
  sl->getFids(levelFids);
  std::vector<TFrameId>::iterator it = levelFids.begin();
  for (it; it != levelFids.end(); it++) {
    int curFid = it->getNumber();
    if (fid0.getNumber() <= curFid && curFid <= fid1.getNumber())
      fids.push_back(*it);
  }

  TUndoManager::manager()->add(new UndoInbetween(sl, fids, interpolation));

  inbetweenWithoutUndo(sl, fid0, fid1, interpolation);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}
