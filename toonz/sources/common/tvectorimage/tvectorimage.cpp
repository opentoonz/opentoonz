

#include "tcurves.h"
//#include "tpalette.h"
#include "tvectorimage.h"
#include "tvectorimageP.h"
#include "tstroke.h"
//#include "tgl.h"
#include "tvectorrenderdata.h"
#include "tmathutil.h"
//#include "tdebugmessage.h"
#include "tofflinegl.h"
//#include "tcolorstyles.h"
#include "tpaletteutil.h"
#include "tthreadmessage.h"
#include "tsimplecolorstyles.h"
#include "tcomputeregions.h"

#include <memory>

//=============================================================================
typedef TVectorImage::IntersectionBranch IntersectionBranch;

namespace {

typedef std::set<int> DisabledStrokeStyles;

// uso getDisabledStrokeStyleSet() invece che accedere direttamente alla
// variabile per assicurarmi che il tutto funzioni anche quando viene
// usato PRIMA del main (per iniziativa di un costruttore di una variabile
// globale, p.es.).
// per l'idioma: cfr. Modern C++ design, Andrei Alexandrescu, Addison Wesley
// 2001, p.133

inline DisabledStrokeStyles &getDisabledStrokeStyleSet() {
  static DisabledStrokeStyles disabledStokeStyles;
  return disabledStokeStyles;
}

inline bool isStrokeStyleEnabled__(int index) {
  DisabledStrokeStyles &disabledSet = getDisabledStrokeStyleSet();
  return disabledSet.find(index) == disabledSet.end();
}

}  // namespace

//=============================================================================

/*!
Permette di copiare effettuare delle copie delle curve
*/
/*
template <class Container>
class StrokeArrayInsertIterator
{
  Container& container;

public:
  explicit    StrokeArrayInsertIterator(Container& Line)
    :container(Line)
  {};

  StrokeArrayInsertIterator& operator=(const VIStroke* value )
  {
    TStroke *stroke = new TStroke(*(value->m_s));

    stroke->setId(value->m_s->getId());
    container.push_back(new VIStroke(stroke));
    return *this;
  };

  StrokeArrayInsertIterator&  operator*()       { return *this; }
  StrokeArrayInsertIterator&  operator++()       { return *this; }
  StrokeArrayInsertIterator   operator++(int val){ return *this; }
};
*/

//=============================================================================
/*!
TVectorImage::Imp: implementation of TVectorImage class
\relates  TVectorImage
*/

//=============================================================================

TVectorImage::Imp::Imp(TVectorImage *vi)
    : m_areValidRegions(false)
    , m_notIntersectingStrokes(false)
    , m_computeRegions(true)
    , m_autocloseTolerance(c_newAutocloseTolerance)
    , m_maxGroupId(1)
    , m_maxGhostGroupId(1)
    , m_mutex(new TThread::Mutex())
    , m_vi(vi)
    , m_intersectionData(0)
    , m_computedAlmostOnce(false)
    , m_justLoaded(false)
    , m_insideGroup(TGroupId())
    , m_minimizeEdges(true)
#ifdef NEW_REGION_FILL
    , m_regionFinder(0)
#endif
{
#ifdef NEW_REGION_FILL
  resetRegionFinder();
#endif
  initRegionsData();
}

TVectorImage::Imp::~Imp() {
  // delete m_regionFinder;
  deleteRegionsData();
  delete m_mutex;
}

//=============================================================================

TVectorImage::TVectorImage(bool loaded) : m_imp(new TVectorImage::Imp(this)) {
  if (loaded) m_imp->m_justLoaded = true;
}

//-----------------------------------------------------------------------------

TVectorImage::~TVectorImage() {}

//-----------------------------------------------------------------------------

int TVectorImage::isInsideGroup() const {
  return m_imp->m_insideGroup.getDepth();
}

//-----------------------------------------------------------------------------

int TVectorImage::addStrokeToGroup(TStroke *stroke, int strokeIndex) {
  if (!m_imp->m_strokes[strokeIndex]->m_groupId.isGrouped())
    return addStroke(stroke, true);
  for (int i = m_imp->m_strokes.size() - 1; i >= 0; i--)
    if (m_imp->m_strokes[i]->m_groupId ==
        m_imp->m_strokes[strokeIndex]->m_groupId) {
      m_imp->insertStrokeAt(
          new VIStroke(stroke, m_imp->m_strokes[i]->m_groupId), i + 1);
      return i + 1;
    }
  assert(false);
  return -1;
}

//-----------------------------------------------------------------------------

int TVectorImage::addStroke(TStroke *stroke, bool discardPoints) {
  if (discardPoints) {
    TRectD bBox = stroke->getBBox();
    if (bBox.x0 == bBox.x1 && bBox.y0 == bBox.y1)  // empty stroke: discard
      return -1;
  }

  if (m_imp->m_insideGroup != TGroupId()) {
    int i;
    for (i = m_imp->m_strokes.size() - 1; i >= 0; i--)
      if (m_imp->m_insideGroup.isParentOf(m_imp->m_strokes[i]->m_groupId)) {
        m_imp->insertStrokeAt(
            new VIStroke(stroke, m_imp->m_strokes[i]->m_groupId), i + 1);
        return i + 1;
      }
  }

  TGroupId gid;
  if (m_imp->m_strokes.empty() ||
      m_imp->m_strokes.back()->m_groupId.isGrouped() != 0)
    gid = TGroupId(this, true);
  else
    gid = m_imp->m_strokes.back()->m_groupId;

  m_imp->m_strokes.push_back(new VIStroke(stroke, gid));
  m_imp->m_areValidRegions = false;
  return m_imp->m_strokes.size() - 1;
}

//-----------------------------------------------------------------------------

void TVectorImage::moveStrokes(int fromIndex, int count, int moveBefore) {
#ifdef _DEBUG
  m_imp->checkGroups();
#endif
  m_imp->moveStrokes(fromIndex, count, moveBefore, true);
#ifdef _DEBUG
  m_imp->checkGroups();
#endif
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::moveStrokes(int fromIndex, int count, int moveBefore,
                                    bool regroup) {
  assert(fromIndex >= 0 && fromIndex < (int)m_strokes.size());
  assert(moveBefore >= 0 && moveBefore <= (int)m_strokes.size());
  assert(count > 0);

  assert(fromIndex != moveBefore);

  for (int i = 0; i < count; i++)
    if (fromIndex < moveBefore)
      moveStroke(fromIndex, moveBefore);
    else
      moveStroke(fromIndex + i, moveBefore + i);

  std::vector<int> changedStrokes;
  if (regroup) regroupGhosts(changedStrokes);
  if (!changedStrokes.empty())
    notifyChangedStrokes(changedStrokes, std::vector<TStroke *>(), false);
}

//-----------------------------------------------------------------------------

void TVectorImage::insertStrokeAt(VIStroke *vs, int strokeIndex,
                                  bool recomputeRegions) {
  m_imp->insertStrokeAt(vs, strokeIndex, recomputeRegions);
}

/*
void TVectorImage::insertStrokeAt(TStroke *stroke, int strokeIndex, const
TGroupId& id)
{
VIStroke* vs;

vs = new VIStroke(stroke, id);

m_imp->insertStrokeAt(vs, strokeIndex);

}
*/

//-----------------------------------------------------------------------------

/*
TRectD TVectorImage::addStroke(const std::vector<TThickPoint> &points)
{
//  era:  TStroke *stroke = makeTStroke(points);
  TStroke *stroke = TStroke::interpolate(points, 5.0);

  m_imp->m_strokes.push_back(new VIStroke( stroke) );
  m_imp->m_areValidRegions = false;

  return stroke->getBBox();
}
*/

//-----------------------------------------------------------------------------

static bool isRegionWithStroke(TRegion *region, TStroke *s) {
  for (UINT i = 0; i < region->getEdgeCount(); i++)
    if (region->getEdge(i)->m_s == s) return true;
  return false;
}

//-----------------------------------------------------------------------------

static void deleteSubRegionWithStroke(TRegion *region, TStroke *s) {
  for (int i = 0; i < (int)region->getSubregionCount(); i++) {
    deleteSubRegionWithStroke(region->getSubregion(i), s);
    if (isRegionWithStroke(region->getSubregion(i), s)) {
      TRegion *r = region->getSubregion(i);
      r->moveSubregionsTo(region);
      assert(r->getSubregionCount() == 0);
      region->deleteSubregion(i);
      delete r;
      i--;
    }
  }
}

//-----------------------------------------------------------------------------

TStroke *TVectorImage::removeStroke(int index, bool doComputeRegions) {
  return m_imp->removeStroke(index, doComputeRegions);
}

TStroke *TVectorImage::Imp::removeStroke(int index, bool doComputeRegions) {
  assert(index >= 0 && index < (int)m_strokes.size());
  QMutexLocker sl(m_mutex);

  VIStroke *stroke = m_strokes[index];

  eraseIntersection(index);

  m_strokes.erase(m_strokes.begin() + index);

  if (m_computedAlmostOnce) {
    reindexEdges(index);
    if (doComputeRegions) computeRegions();
  }

  return stroke->m_s;
}

//-----------------------------------------------------------------------------

void TVectorImage::removeStrokes(const std::vector<int> &toBeRemoved,
                                 bool deleteThem, bool recomputeRegions) {
  m_imp->removeStrokes(toBeRemoved, deleteThem, recomputeRegions);
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::removeStrokes(const std::vector<int> &toBeRemoved,
                                      bool deleteThem, bool recomputeRegions) {
  QMutexLocker sl(m_mutex);

  for (int i = toBeRemoved.size() - 1; i >= 0; i--) {
    assert(i == 0 || toBeRemoved[i - 1] < toBeRemoved[i]);
    UINT index = toBeRemoved[i];

    eraseIntersection(index);
    if (deleteThem) delete m_strokes[index];
    m_strokes.erase(m_strokes.begin() + index);
  }

  if (m_computedAlmostOnce && !toBeRemoved.empty()) {
    reindexEdges(toBeRemoved, false);

    if (recomputeRegions)
      computeRegions();
    else
      m_areValidRegions = false;
  }
}

//-----------------------------------------------------------------------------

void TVectorImage::deleteStroke(int index) {
  TStroke *stroke = removeStroke(index);
  delete stroke;
}

//-----------------------------------------------------------------------------

void TVectorImage::deleteStroke(VIStroke *stroke) {
  UINT index = 0;
  for (; index < m_imp->m_strokes.size(); index++)
    if (m_imp->m_strokes[index] == stroke) {
      deleteStroke(index);
      return;
    }
}

//-----------------------------------------------------------------------------

/*
void TVectorImage::validateRegionEdges(TStroke* stroke, bool invalidate)
{
if (invalidate)
  for (UINT i=0; i<getRegionCount(); i++)
    {
    TRegion *r = getRegion(i);
//  if ((*cit)->getBBox().contains(stroke->getBBox()))
    for (UINT j=0; j<r->getEdgeCount(); j++)
      {
      TEdge* edge = r->getEdge(j);
      if (edge->m_s == stroke)
        edge->m_w0 = edge->m_w1 = -1;
      }
    }
else
 for (UINT i=0; i<getRegionCount(); i++)
    {
    TRegion *r = getRegion(i);
//  if ((*cit)->getBBox().contains(stroke->getBBox()))
    for (UINT j=0; j<r->getEdgeCount(); j++)
      {
      TEdge* edge = r->getEdge(j);
      if (edge->m_w0==-1)
        {
        int index;
        double t, dummy;
        edge->m_s->getNearestChunk(edge->m_p0, t, index, dummy);
        edge->m_w0 = getWfromChunkAndT(edge->m_s, index, t);
        edge->m_s->getNearestChunk(edge->m_p1, t, index, dummy);
        edge->m_w1 = getWfromChunkAndT(edge->m_s, index, t);
        }
      }
    }


}
*/
//-----------------------------------------------------------------------------

UINT TVectorImage::getStrokeCount() const { return m_imp->m_strokes.size(); }

//-----------------------------------------------------------------------------
/*
void  TVectorImage::addSeed(const TPointD& p, const TPixel& color)
{
m_imp->m_seeds.push_back(TFillSeed(color, p, NULL));
}
*/
//-----------------------------------------------------------------------------

UINT TVectorImage::getRegionCount() const {
  //  assert( m_imp->m_areValidRegions || m_imp->m_regions.empty());
  return m_imp->m_regions.size();
}

//-----------------------------------------------------------------------------

TRegion *TVectorImage::getRegion(UINT index) const {
  assert(index < m_imp->m_regions.size());
  //  assert( m_imp->m_areValidRegions );
  return m_imp->m_regions[index];
}

//-----------------------------------------------------------------------------

TRegion *TVectorImage::getRegion(TRegionId regId) const {
  int index = getStrokeIndexById(regId.m_strokeId);

  assert(m_imp->m_areValidRegions);

  TRegion *reg = m_imp->getRegion(regId, index);
  // assert( reg );
  return reg;
}

//-----------------------------------------------------------------------------

TRegion *TVectorImage::Imp::getRegion(TRegionId regId, int index) const {
  assert(index != -1);
  if (index == -1) return 0;

  assert(index < (int)m_strokes.size());
  if (index >= (int)m_strokes.size()) return 0;

  std::list<TEdge *> &edgeList = m_strokes[index]->m_edgeList;

  std::list<TEdge *>::iterator endList = edgeList.end();
  double w0;
  double w1;
  for (std::list<TEdge *>::iterator it = edgeList.begin(); it != endList;
       ++it) {
    w0 = (*it)->m_w0;
    w1 = (*it)->m_w1;

    if (w0 < w1) {
      if (w0 < regId.m_midW && regId.m_midW < w1 && regId.m_direction)
        return (*it)->m_r;
    } else {
      if (w1 < regId.m_midW && regId.m_midW < w0 && !regId.m_direction)
        return (*it)->m_r;
    }
  }

#ifdef _DEBUG
  TPointD cp1 = m_strokes[index]->m_s->getControlPoint(0);
  TPointD cp2 = m_strokes[index]->m_s->getControlPoint(
      m_strokes[index]->m_s->getControlPointCount() - 1);
#endif

  return 0;
}

/*
TRegion* TVectorImage::getRegion(TRegionId regId) const
{
  int index = getStrokeIndexById(regId.m_strokeId);
  assert(index!=-1);
  if( index == -1 )
    return 0;

  assert( index < (int)m_imp->m_strokes.size() );
  if( index >= (int)m_imp->m_strokes.size() )
    return 0;

  std::list<TEdge*> &edgeList = m_imp->m_strokes[index]->m_edgeList;

  std::list<TEdge*>::iterator endList = edgeList.end();
  double w0;
  double w1;
  for(list<TEdge*>::iterator it= edgeList.begin(); it!=endList; ++it)
  {
    w0 = (*it)->m_w0;
    w1 = (*it)->m_w1;

    if(w0<w1)
    {
      if( w0 < regId.m_midW && regId.m_midW < w1 && regId.m_direction )
        return (*it)->m_r;
    }
    else
    {
      if( w1 < regId.m_midW && regId.m_midW < w0 && !regId.m_direction )
        return (*it)->m_r;
    }
  }

  return 0;
}
*/
//-----------------------------------------------------------------------------

void TVectorImage::setEdgeColors(int strokeIndex, int leftColorIndex,
                                 int rightColorIndex) {
  std::list<TEdge *> &ll = m_imp->m_strokes[strokeIndex]->m_edgeList;

  std::list<TEdge *>::const_iterator l   = ll.begin();
  std::list<TEdge *>::const_iterator l_e = ll.end();
  for (; l != l_e; ++l) {
    // double w0 = (*l)->m_w0, w1 = (*l)->m_w1;
    if ((*l)->m_w0 > (*l)->m_w1) {
      if (leftColorIndex != -1)
        (*l)->m_styleId = leftColorIndex;
      else if (rightColorIndex != -1)
        (*l)->m_styleId = rightColorIndex;
    } else {
      if (rightColorIndex != -1)
        (*l)->m_styleId = rightColorIndex;
      else if (leftColorIndex != -1)
        (*l)->m_styleId = leftColorIndex;
    }
  }
}

//-----------------------------------------------------------------------------

TStroke *TVectorImage::getStroke(UINT index) const {
  assert(index < m_imp->m_strokes.size());
  return m_imp->m_strokes[index]->m_s;
}

VIStroke *TVectorImage::getVIStroke(UINT index) const {
  assert(index < m_imp->m_strokes.size());
  return m_imp->m_strokes[index];
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::getStrokeById(int id) const {
  int n = m_imp->m_strokes.size();
  for (int i = 0; i < n; i++)
    if (m_imp->m_strokes[i]->m_s->getId() == id) return m_imp->m_strokes[i];
  return 0;
}

//-----------------------------------------------------------------------------

int TVectorImage::getStrokeIndexById(int id) const {
  int n = m_imp->m_strokes.size();

  for (int i = 0; i < n; i++)
    if (m_imp->m_strokes[i]->m_s->getId() == id) return i;

  return -1;
}

//-----------------------------------------------------------------------------

int TVectorImage::getStrokeIndex(TStroke *stroke) const {
  int n = m_imp->m_strokes.size();

  for (int i = 0; i < n; i++)
    if (m_imp->m_strokes[i]->m_s == stroke) return i;

  return -1;
}

//-----------------------------------------------------------------------------

TRectD TVectorImage::getBBox() const {
  UINT strokeCount = m_imp->m_strokes.size();
  if (strokeCount == 0) return TRectD();

  TPalette *plt = getPalette();
  TRectD bbox;

  for (UINT i = 0; i < strokeCount; ++i) {
    TRectD r           = m_imp->m_strokes[i]->m_s->getBBox();
    TColorStyle *style = 0;
    if (plt) style     = plt->getStyle(m_imp->m_strokes[i]->m_s->getStyle());
    if (dynamic_cast<TRasterImagePatternStrokeStyle *>(style) ||
        dynamic_cast<TVectorImagePatternStrokeStyle *>(
            style))  // con i pattern style, il render a volte taglia sulla bbox
                     // dello stroke....
      // aumento la bbox della meta' delle sue dimensioni:pezzaccia.
      r  = r.enlarge(std::max(r.getLx() * 0.25, r.getLy() * 0.25));
    bbox = ((i == 0) ? r : bbox + r);
  }

  return bbox;
}

//-----------------------------------------------------------------------------

bool TVectorImage::getNearestStroke(const TPointD &p, double &outW,
                                    UINT &strokeIndex, double &dist2,
                                    bool onlyInCurrentGroup) const {
  dist2       = (std::numeric_limits<double>::max)();
  strokeIndex = getStrokeCount();
  outW        = -1;

  double tempdis2, tempPar;

  for (int i = 0; i < (int)m_imp->m_strokes.size(); ++i) {
    if (onlyInCurrentGroup && !inCurrentGroup(i)) continue;
    TStroke *s = m_imp->m_strokes[i]->m_s;
    tempPar    = s->getW(p);

    tempdis2 = tdistance2(TThickPoint(p, 0), s->getThickPoint(tempPar));

    if (tempdis2 < dist2) {
      outW        = tempPar;
      dist2       = tempdis2;
      strokeIndex = i;
    }
  }

  return dist2 < (std::numeric_limits<double>::max)();
}

//-----------------------------------------------------------------------------

#if defined(LINUX) || defined(MACOSX)
void TVectorImage::render(const TVectorRenderData &rd, TRaster32P &ras) {
  // hardRenderVectorImage(rd,ras,this);
}
#endif

//-----------------------------------------------------------------------------
//#include "timage_io.h"

TRaster32P TVectorImage::render(bool onlyStrokes) {
  TRect bBox = convert(getBBox());
  if (bBox.isEmpty()) return (TRaster32P)0;

  std::unique_ptr<TOfflineGL> offlineGlContext(new TOfflineGL(bBox.getSize()));
  offlineGlContext->clear(TPixel32(0, 0, 0, 0));
  offlineGlContext->makeCurrent();
  TVectorRenderData rd(TTranslation(-convert(bBox.getP00())),
                       TRect(bBox.getSize()), getPalette(), 0, true, true);
  rd.m_drawRegions = !onlyStrokes;
  offlineGlContext->draw(this, rd, false);

  return offlineGlContext->getRaster()->clone();

  // hardRenderVectorImage(rd,ras,this);
}

//-----------------------------------------------------------------------------

TRegion *TVectorImage::getRegion(const TPointD &p) {
#ifndef NEW_REGION_FILL
  if (!isComputedRegionAlmostOnce()) return 0;

  if (!m_imp->m_areValidRegions) m_imp->computeRegions();
#endif

  return m_imp->getRegion(p);
}

//-----------------------------------------------------------------------------

TRegion *TVectorImage::Imp::getRegion(const TPointD &p) {
  int strokeIndex = (int)m_strokes.size() - 1;

  while (strokeIndex >= 0) {
    for (UINT regionIndex = 0; regionIndex < m_regions.size(); regionIndex++)
      if (areDifferentGroup(strokeIndex, false, regionIndex, true) == -1 &&
          m_regions[regionIndex]->contains(p))
        return m_regions[regionIndex]->getRegion(p);
    int curr = strokeIndex;
    while (strokeIndex >= 0 &&
           areDifferentGroup(curr, false, strokeIndex, false) == -1)
      strokeIndex--;
  }

  return 0;
}

//-----------------------------------------------------------------------------

int TVectorImage::fillStrokes(const TPointD &p, int styleId) {
  UINT index;
  double outW, dist2;

  if (getNearestStroke(p, outW, index, dist2, true)) {
    double thick           = getStroke(index)->getThickPoint(outW).thick * 1.25;
    if (thick < 0.5) thick = 0.5;

    if (dist2 > thick * thick) return -1;
    assert(index >= 0 && index < m_imp->m_strokes.size());
    int ret = m_imp->m_strokes[index]->m_s->getStyle();
    m_imp->m_strokes[index]->m_s->setStyle(styleId);
    return ret;
  }

  return -1;
}

//-----------------------------------------------------------------------------

#ifdef NEW_REGION_FILL

void TVectorImage::resetRegionFinder() { m_imp->resetRegionFinder(); }
#else
//------------------------------------------------------------------

int TVectorImage::fill(const TPointD &p, int newStyleId, bool onlyEmpty) {
  TRegion *r = getRegion(p);
  if (onlyEmpty && r && r->getStyle() != 0) return -1;

  if (!m_imp->m_areValidRegions) m_imp->computeRegions();
  return m_imp->fill(p, newStyleId);
}
#endif

//-----------------------------------------------------------------------------
/*
void TVectorImage::autoFill(int styleId)
{
m_imp->autoFill(styleId, true);
}

void TVectorImage::Imp::autoFill(int styleId, bool oddLevel)
{
if (!m_areValidRegions)
  computeRegions();
for (UINT i = 0; i<m_regions.size(); i++)
  {
  if (oddLevel)
    m_regions[i]->setStyle(styleId);
  m_regions[i]->autoFill(styleId, !oddLevel);
  }
}
*/
//-----------------------------------------------------------------------------
/*
void TRegion::autoFill(int styleId, bool oddLevel)
{
for (UINT i = 0; i<getSubregionCount(); i++)
  {
  TRegion* r = getSubregion(i);
  if (oddLevel)
    r->setStyle(styleId);
  r->autoFill(styleId, !oddLevel);
  }

}
*/
//-----------------------------------------------------------------------------

int TVectorImage::Imp::fill(const TPointD &p, int styleId) {
  int strokeIndex = (int)m_strokes.size() - 1;

  while (strokeIndex >= 0) {
    if (!inCurrentGroup(strokeIndex)) {
      strokeIndex--;
      continue;
    }
    for (UINT regionIndex = 0; regionIndex < m_regions.size(); regionIndex++)
      if (areDifferentGroup(strokeIndex, false, regionIndex, true) == -1 &&
          m_regions[regionIndex]->contains(p))
        return m_regions[regionIndex]->fill(p, styleId);
    int curr = strokeIndex;
    while (strokeIndex >= 0 &&
           areDifferentGroup(curr, false, strokeIndex, false) == -1)
      strokeIndex--;
  }

  return -1;
}

//-----------------------------------------------------------------------------

bool TVectorImage::selectFill(const TRectD &selArea, TStroke *s, int newStyleId,
                              bool onlyUnfilled, bool fillAreas,
                              bool fillLines) {
  if (!m_imp->m_areValidRegions) m_imp->computeRegions();
  return m_imp->selectFill(selArea, s, newStyleId, onlyUnfilled, fillAreas,
                           fillLines);
}

//-----------------------------------------------------------------------------

bool TVectorImage::Imp::selectFill(const TRectD &selArea, TStroke *s,
                                   int newStyleId, bool onlyUnfilled,
                                   bool fillAreas, bool fillLines) {
  bool hitSome = false;

  if (s) {
    TVectorImage aux;
    aux.addStroke(s);
    aux.findRegions();
    for (UINT j = 0; j < aux.getRegionCount(); j++) {
      TRegion *r0 = aux.getRegion(j);
      if (fillAreas)
        for (UINT i = 0; i < m_regions.size(); i++) {
          TRegion *r1 = m_regions[i];

          if (m_insideGroup != TGroupId() &&
              !m_insideGroup.isParentOf(
                  m_strokes[r1->getEdge(0)->m_index]->m_groupId))
            continue;

          if ((!onlyUnfilled || r1->getStyle() == 0) && r0->contains(*r1)) {
            r1->setStyle(newStyleId);
            hitSome = true;
          }
        }
      if (fillLines)
        for (UINT i = 0; i < m_strokes.size(); i++) {
          if (!inCurrentGroup(i)) continue;

          TStroke *s1 = m_strokes[i]->m_s;
          if ((!onlyUnfilled || s1->getStyle() == 0) && r0->contains(*s1)) {
            s1->setStyle(newStyleId);
            hitSome = true;
          }
        }
    }
    aux.removeStroke(0);
    return hitSome;
  }

  // rect fill

  if (fillAreas)
#ifndef NEW_REGION_FILL
    for (UINT i = 0; i < m_regions.size(); i++) {
      int index, j = 0;

      do
        index = m_regions[i]->getEdge(j++)->m_index;
      while (index < 0 && j < (int)m_regions[i]->getEdgeCount());
      // if index<0, means that the region is purely of autoclose strokes!
      if (m_insideGroup != TGroupId() && index >= 0 &&
          !m_insideGroup.isParentOf(m_strokes[index]->m_groupId))
        continue;
      if (!onlyUnfilled || m_regions[i]->getStyle() == 0)
        hitSome |= m_regions[i]->selectFill(selArea, newStyleId);
    }
#else

    findRegions(selArea);

  for (UINT i = 0; i < m_regions.size(); i++) {
    if (m_insideGroup != TGroupId() &&
        !m_insideGroup.isParentOf(
            m_strokes[m_regions[i]->getEdge(0)->m_index]->m_groupId))
      continue;
    if (!onlyUnfilled || m_regions[i]->getStyle() == 0)
      hitSome |= m_regions[i]->selectFill(selArea, newStyleId);
  }
#endif

  if (fillLines)
    for (UINT i = 0; i < m_strokes.size(); i++) {
      if (!inCurrentGroup(i)) continue;

      TStroke *s = m_strokes[i]->m_s;

      if ((!onlyUnfilled || s->getStyle() == 0) &&
          selArea.contains(s->getBBox())) {
        s->setStyle(newStyleId);
        hitSome = true;
      }
    }
  return hitSome;
}

//-----------------------------------------------------------------------------

/*
void  TVectorImageImp::seedFill()
{
  std::vector<TFillSeed>::iterator it;
  TRegion*r;

  TFillStyleP app =NULL;

  for (it=m_seeds.begin(); it!=m_seeds.end(); )
    if ((r=fill(it->m_p, new TColorStyle(it->m_fillStyle.getPointer()) ))!=NULL)
    {
      it->m_r = r;
      it = m_seeds.erase(it);  // i seed provengono da immagini vecchie. non
servono piu'.
    }
  m_areValidRegions=true;
}
*/
//-----------------------------------------------------------------------------
/*
void  TVectorImage::seedFill()
{
  m_imp->seedFill();

}
*/
//-----------------------------------------------------------------------------

void TVectorImage::notifyChangedStrokes(
    const std::vector<int> &strokeIndexArray,
    const std::vector<TStroke *> &oldStrokeArray, bool areFlipped) {
  std::vector<TStroke *> aux;

  /*
if (oldStrokeArray.empty())
{
  for (int i=0; i<(int)strokeIndexArray.size(); i++)
    {
          TStroke *s = getStroke(strokeIndexArray[i]);
    aux.push_back(s);
          }
m_imp->notifyChangedStrokes(strokeIndexArray, aux, areFlipped);
  }
else*/
  m_imp->notifyChangedStrokes(strokeIndexArray, oldStrokeArray, areFlipped);
}

//-----------------------------------------------------------------------------

void TVectorImage::notifyChangedStrokes(int strokeIndexArray,
                                        TStroke *oldStroke, bool isFlipped) {
  std::vector<int> app(1);
  app[0] = strokeIndexArray;

  std::vector<TStroke *> oldStrokeArray(1);
  oldStrokeArray[0] = oldStroke ? oldStroke : getStroke(strokeIndexArray);
  m_imp->notifyChangedStrokes(app, oldStrokeArray, isFlipped);
}

//-----------------------------------------------------------------------------

// ofstream of("C:\\temp\\butta.txt");

void transferColors(const std::list<TEdge *> &oldList,
                    const std::list<TEdge *> &newList, bool isStrokeChanged,
                    bool isFlipped, bool overwriteColor) {
  if (newList.empty() || oldList.empty()) return;

  std::list<TEdge *>::const_iterator it;
// unused variable
#if 0 
list<TEdge*>::const_iterator it1;
#endif
  double totLength;
  if (isStrokeChanged) totLength = newList.front()->m_s->getLength();
  for (it = newList.begin(); it != newList.end(); ++it) {
    int newStyle = -1;  // ErrorStyle;
// unused variable
#if 0 
  int styleId = (*it)->m_styleId;
#endif
    if (!overwriteColor && (*it)->m_styleId != 0) continue;
    bool reversed;
    double deltaMax = 0.005;
    double l0, l1;
    if ((*it)->m_w0 > (*it)->m_w1) {
      reversed = !isFlipped;
      if (isStrokeChanged) {
        l0 = (*it)->m_s->getLength((*it)->m_w1) / totLength;
        l1 = (*it)->m_s->getLength((*it)->m_w0) / totLength;
      } else {
        l0 = (*it)->m_w1;
        l1 = (*it)->m_w0;
      }
    } else {
      reversed = isFlipped;
      if (isStrokeChanged) {
        l0 = (*it)->m_s->getLength((*it)->m_w0) / totLength;
        l1 = (*it)->m_s->getLength((*it)->m_w1) / totLength;
      } else {
        l0 = (*it)->m_w0;
        l1 = (*it)->m_w1;
      }
      // w0 = (*it)->m_w0;
      // w1 = (*it)->m_w1;
    }

    std::list<TEdge *>::const_iterator it1 = oldList.begin();
    for (; it1 != oldList.end(); ++it1) {
// unused variable
#if 0
      TEdge*e = *it1;
#endif
      if (/*(*it1)->m_styleId==0 ||*/
          (reversed && (*it1)->m_w0 < (*it1)->m_w1) ||
          (!reversed && (*it1)->m_w0 > (*it1)->m_w1))
        continue;
      double _l0, _l1;
      if (isStrokeChanged) {
        double totLength1 = (*it1)->m_s->getLength();

        _l0 = (*it1)->m_s->getLength(std::min((*it1)->m_w0, (*it1)->m_w1)) /
              totLength1;
        _l1 = (*it1)->m_s->getLength(std::max((*it1)->m_w0, (*it1)->m_w1)) /
              totLength1;
      } else {
        _l0 = std::min((*it1)->m_w0, (*it1)->m_w1);
        _l1 = std::max((*it1)->m_w0, (*it1)->m_w1);
      }
      double delta = std::min(l1, _l1) - std::max(l0, _l0);
      if (delta > deltaMax) {
        deltaMax = delta;
        newStyle = (*it1)->m_styleId;
      }
    }
    if (newStyle >= 0)  // !=ErrorStyle)
    {
      if ((*it)->m_r)
        (*it)->m_r->setStyle(newStyle);
      else
        (*it)->m_styleId = newStyle;
    }
  }
}

//-----------------------------------------------------------------------------

void TVectorImage::transferStrokeColors(TVectorImageP sourceImage,
                                        int sourceStroke,
                                        TVectorImageP destinationImage,
                                        int destinationStroke) {
  std::list<TEdge *> *sourceList =
      &(sourceImage->m_imp->m_strokes[sourceStroke]->m_edgeList);
  std::list<TEdge *> *destinationList =
      &(destinationImage->m_imp->m_strokes[destinationStroke]->m_edgeList);
  transferColors(*sourceList, *destinationList, true, false, false);
}

//-----------------------------------------------------------------------------

bool TVectorImage::Imp::areWholeGroups(const std::vector<int> &indexes) const {
  UINT i, j;
  for (i = 0; i < indexes.size(); i++) {
    if (m_strokes[indexes[i]]->m_isNewForFill) return false;
    if (!m_strokes[indexes[i]]->m_groupId.isGrouped() != 0) return false;
    for (j = 0; j < m_strokes.size(); j++) {
      int ret = areDifferentGroup(indexes[i], false, j, false);
      if (ret == -1 ||
          ret >= 1 && find(indexes.begin(), indexes.end(), j) == indexes.end())
        return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

//-------------------------------------------------------------------

void TVectorImage::Imp::notifyChangedStrokes(
    const std::vector<int> &strokeIndexArray,
    const std::vector<TStroke *> &oldStrokeArray, bool areFlipped) {
#ifdef _DEBUG
  checkIntersections();
#endif

  assert(oldStrokeArray.empty() ||
         strokeIndexArray.size() == oldStrokeArray.size());

  if (!m_computedAlmostOnce && !m_notIntersectingStrokes) return;

  typedef std::list<TEdge *> EdgeList;
  std::vector<EdgeList> oldEdgeListArray(strokeIndexArray.size());
  int i;

  // se si sono trasformati  interi gruppi (senza deformare le stroke) non c'e'
  // bisogno di ricalcolare le regioni!
  if (oldStrokeArray.empty() && areWholeGroups(strokeIndexArray)) {
    m_areValidRegions = true;
    for (i = 0; i < (int)m_regions.size(); i++)
      invalidateRegionPropAndBBox(m_regions[i]);
    return;
  }

  QMutexLocker sl(m_mutex);
  for (i = 0; i < (int)strokeIndexArray.size(); i++)  // ATTENZIONE! non si puo'
                                                      // fare eraseIntersection
                                                      // in questo stesso ciclo
  {
    VIStroke *s = m_strokes[strokeIndexArray[i]];
    // if (s->m_s->isSelfLoop())
    //  assert(s->m_edgeList.size()<=1);

    std::list<TEdge *>::iterator it = s->m_edgeList.begin();
    for (; it != s->m_edgeList.end(); it++) {
      TEdge *e                            = new TEdge(**it, false);
      if (!oldStrokeArray.empty()) e->m_s = oldStrokeArray[i];
      oldEdgeListArray[i].push_back(e);  // bisogna allocare nuovo edge,
                                         // perche'la eraseIntersection poi lo
                                         // cancella....
      if ((*it)->m_toBeDeleted) delete *it;
    }
    s->m_edgeList.clear();
  }

  for (i = 0; i < (int)strokeIndexArray.size(); i++) {
    eraseIntersection(strokeIndexArray[i]);
    if (!m_notIntersectingStrokes)
      m_strokes[strokeIndexArray[i]]->m_isNewForFill = true;
  }

  computeRegions();  // m_imp->m_strokes, m_imp->m_regions);

  for (i = 0; i < (int)strokeIndexArray.size(); i++) {
    transferColors(oldEdgeListArray[i],
                   m_strokes[strokeIndexArray[i]]->m_edgeList, true, areFlipped,
                   false);
    clearPointerContainer(oldEdgeListArray[i]);
  }

#ifdef _DEBUG
  checkIntersections();
#endif
}

//-----------------------------------------------------------------------------

void TVectorImage::findRegions(bool fromSwf) {
  // for (int i=0; i<(int)m_imp->m_strokes.size(); i++)
  //  {
  //  m_imp->eraseIntersection(i);
  //	  m_imp->m_strokes[i]->m_isNewForFill=true;
  //  }

  if (m_imp->m_areValidRegions) return;

  // m_imp->m_regions.clear();

  // compute regions...
  m_imp->computeRegions();  // m_imp->m_strokes, m_imp->m_regions);
}

//-----------------------------------------------------------------------------

void TVectorImage::putRegion(TRegion *region) {
  m_imp->m_regions.push_back(region);
}

//-----------------------------------------------------------------------------

//-------------------------------------------------------------------

void TVectorImage::Imp::cloneRegions(TVectorImage::Imp &out,
                                     bool doComputeRegions) {
  std::unique_ptr<IntersectionBranch[]> v;
  UINT size = getFillData(v);
  out.setFillData(v, size, doComputeRegions);
}

//-----------------------------------------------------------------------------

TVectorImageP TVectorImage::clone() const {
  return TVectorImageP(cloneImage());
}

//-----------------------------------------------------------------------------

TImage *TVectorImage::cloneImage() const {
  TVectorImage *out = new TVectorImage;

  out->m_imp->m_autocloseTolerance = m_imp->m_autocloseTolerance;
  out->m_imp->m_maxGroupId         = m_imp->m_maxGroupId;
  out->m_imp->m_maxGhostGroupId    = m_imp->m_maxGhostGroupId;

  for (int i = 0; i < (int)m_imp->m_strokes.size(); i++) {
    out->m_imp->m_strokes.push_back(new VIStroke(*(m_imp->m_strokes[i])));
    out->m_imp->m_strokes.back()->m_s->setId(m_imp->m_strokes[i]->m_s->getId());
  }

  m_imp->cloneRegions(*out->m_imp);

  out->setPalette(getPalette());
  out->m_imp->m_computedAlmostOnce = m_imp->m_computedAlmostOnce;
  out->m_imp->m_justLoaded         = m_imp->m_justLoaded;

  return out;
}

//-----------------------------------------------------------------------------
/*
TVectorImageP mergeAndClear(TVectorImageP v1, TVectorImageP v2 )
{
  TVectorImageP out = new TVectorImage;

  std::vector<VIStroke*>::iterator it_b =  v1->m_imp->m_strokes.begin();
  std::vector<VIStroke*>::iterator it_e =  v1->m_imp->m_strokes.end();

  std::copy( it_b, it_e, std::back_inserter( out->m_imp->m_strokes ) );

  it_b =  v2->m_imp->m_strokes.begin();
  it_e =  v2->m_imp->m_strokes.end();

  std::copy( it_b, it_e, std::back_inserter( out->m_imp->m_strokes ) );

  v1->m_imp->m_regions.clear();
  v1->m_imp->m_strokes.clear();
  v2->m_imp->m_regions.clear();
  v2->m_imp->m_strokes.clear();

  out->m_imp->m_areValidRegions = false;
  return out;
}
*/

//-----------------------------------------------------------------------------

VIStroke::VIStroke(const VIStroke &s, bool sameId)
    : m_isPoint(s.m_isPoint)
    , m_isNewForFill(s.m_isNewForFill)
    , m_groupId(s.m_groupId) {
  m_s                                     = new TStroke(*s.m_s);
  std::list<TEdge *>::const_iterator it   = s.m_edgeList.begin(),
                                     it_e = s.m_edgeList.end();
  for (; it != it_e; ++it) {
    m_edgeList.push_back(new TEdge(**it, true));
    m_edgeList.back()->m_s = m_s;
  }
  if (sameId) m_s->setId(s.m_s->getId());
}

//-----------------------------------------------------------------------------

void TVectorImage::mergeImage(const TVectorImageP &img, const TAffine &affine,
                              bool sameStrokeId) {
  QMutexLocker sl(m_imp->m_mutex);

#ifdef _DEBUG
  checkIntersections();
#endif

  TPalette *tarPlt = getPalette();
  TPalette *srcPlt = img->getPalette();

  assert(tarPlt);
  assert(tarPlt->getPageCount() > 0);

  // merge della palette
  std::map<int, int> styleTable;
  std::set<int> usedStyles;
  img->getUsedStyles(usedStyles);

  // gmt, 16/10/07. Quando si copia e incolla un path su uno stroke succede
  // che la palette dell'immagine sorgente sia vuota. Non mi sembra sbagliato
  // mettere comunque un test qui
  if (srcPlt) mergePalette(tarPlt, styleTable, srcPlt, usedStyles);

  mergeImage(img, affine, styleTable, sameStrokeId);
}

//-----------------------------------------------------------------------------

void TVectorImage::mergeImage(const TVectorImageP &img, const TAffine &affine,
                              const std::map<int, int> &styleTable,
                              bool sameStrokeId) {
  int imageSize = img->getStrokeCount();
  if (imageSize == 0) return;
  QMutexLocker sl(m_imp->m_mutex);

  m_imp->m_computedAlmostOnce |= img->m_imp->m_computedAlmostOnce;

  std::vector<int> changedStrokeArray(imageSize);

  img->m_imp->reindexGroups(*m_imp);

  int i;
  int insertAt = 0;

  if (m_imp->m_insideGroup !=
      TGroupId())  // if is inside a group, new image is put in that group.
  {
    TGroupId groupId;
    for (i = m_imp->m_strokes.size() - 1; i >= 0; i--)
      if (m_imp->m_insideGroup.isParentOf(m_imp->m_strokes[i]->m_groupId)) {
        insertAt = i + 1;
        groupId  = m_imp->m_strokes[i]->m_groupId;
        break;
      }
    if (insertAt != 0)
      for (i = 0; i < (int)img->m_imp->m_strokes.size(); i++)
        if (!img->m_imp->m_strokes[i]->m_groupId.isGrouped())
          img->m_imp->m_strokes[i]->m_groupId = groupId;
        else
          img->m_imp->m_strokes[i]->m_groupId =
              TGroupId(groupId, img->m_imp->m_strokes[i]->m_groupId);

  }

  // si fondono l'ultimo gruppo ghost della vecchia a e il primo della nuova

  else if (!m_imp->m_strokes.empty() &&
           m_imp->m_strokes.back()->m_groupId.isGrouped(true) != 0 &&
           img->m_imp->m_strokes[0]->m_groupId.isGrouped(true) != 0) {
    TGroupId idNew = m_imp->m_strokes.back()->m_groupId,
             idOld = img->m_imp->m_strokes[0]->m_groupId;
    for (i = 0; i < (int)img->m_imp->m_strokes.size() &&
                img->m_imp->m_strokes[i]->m_groupId == idOld;
         i++)
      img->m_imp->m_strokes[i]->m_groupId = idNew;
  }

  // merge dell'immagine
  std::map<int, int>::const_iterator styleTableIt;
  int oldSize = getStrokeCount();

  for (i = 0; i < imageSize; i++) {
    VIStroke *srcStroke = img->m_imp->m_strokes[i];
    VIStroke *tarStroke = new VIStroke(*srcStroke, sameStrokeId);

    int styleId;
    // cambio i colori delle regioni
    std::list<TEdge *>::const_iterator it   = tarStroke->m_edgeList.begin(),
                                       it_e = tarStroke->m_edgeList.end();
    for (; it != it_e; ++it) {
      int styleId  = (*it)->m_styleId;
      styleTableIt = styleTable.find(styleId);
      assert(styleTableIt != styleTable.end());
      if (styleTableIt != styleTable.end())
        (*it)->m_styleId = styleTableIt->second;
    }

    tarStroke->m_s->transform(affine, true);
    int strokeId = srcStroke->m_s->getId();
    if (getStrokeById(strokeId) == 0) tarStroke->m_s->setId(strokeId);

    // cambio i colori dello stroke
    styleId      = srcStroke->m_s->getStyle();
    styleTableIt = styleTable.find(styleId);
    assert(styleTableIt != styleTable.end());
    if (styleTableIt != styleTable.end())
      tarStroke->m_s->setStyle(styleTableIt->second);
    if (insertAt == 0) {
      m_imp->m_strokes.push_back(tarStroke);
      changedStrokeArray[i] = oldSize + i;
    } else {
      std::vector<VIStroke *>::iterator it = m_imp->m_strokes.begin();
      advance(it, insertAt + i);
      m_imp->m_strokes.insert(it, tarStroke);
      changedStrokeArray[i] = insertAt + i;
    }
  }
  if (insertAt > 0) {
    // for (i=changedStrokeArray.back()+1; i<m_imp->m_strokes.size(); i++)
    //  changedStrokeArray.push_back(i);
    m_imp->reindexEdges(changedStrokeArray, true);
  }

  notifyChangedStrokes(changedStrokeArray, std::vector<TStroke *>(), false);

#ifdef _DEBUG
  checkIntersections();
#endif
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::reindexGroups(TVectorImage::Imp &img) {
  UINT i, j;
  int newMax      = img.m_maxGroupId;
  int newMaxGhost = img.m_maxGhostGroupId;
  for (i = 0; i < m_strokes.size(); i++) {
    VIStroke *s = m_strokes[i];
    if (s->m_groupId.m_id.empty()) continue;
    if (s->m_groupId.m_id[0] > 0)
      for (j = 0; j < s->m_groupId.m_id.size(); j++) {
        s->m_groupId.m_id[j] += img.m_maxGroupId;
        newMax = std::max(newMax, s->m_groupId.m_id[j]);
      }
    else
      for (j = 0; j < s->m_groupId.m_id.size(); j++) {
        s->m_groupId.m_id[j] -= img.m_maxGhostGroupId;
        newMaxGhost = std::max(newMaxGhost, -s->m_groupId.m_id[j]);
      }
  }
  m_maxGroupId = img.m_maxGroupId = newMax;
  m_maxGhostGroupId = img.m_maxGhostGroupId = newMaxGhost;
}

//-----------------------------------------------------------------------------

void TVectorImage::mergeImage(const std::vector<const TVectorImage *> &images) {
  UINT oldSize = getStrokeCount();
  std::vector<int> changedStrokeArray;
  const TVectorImage *img;
  int index;

  if (m_imp->m_insideGroup != TGroupId()) {
    for (index = m_imp->m_strokes.size() - 1; index > -1; index--)
      if (m_imp->m_insideGroup.isParentOf(m_imp->m_strokes[index]->m_groupId))
        break;
    assert(index > -1);
  } else
    index = getStrokeCount() - 1;

  for (UINT j = 0; j < images.size(); ++j) {
    img = images[j];
    if (img->getStrokeCount() == 0) continue;

    img->m_imp->reindexGroups(*m_imp);

    int i = 0;
    /*if (!m_imp->m_strokes.empty() &&
m_imp->m_strokes[index-1]->m_groupId.isGrouped(true)!=0 &&
img->m_imp->m_strokes[0]->m_groupId.isGrouped(true)!=0)
{
assert(false);
TGroupId idNew = m_imp->m_strokes[index]->m_groupId, idOld =
img->m_imp->m_strokes[0]->m_groupId;
for  (;i<(int)img->m_imp->m_strokes.size() &&
img->m_imp->m_strokes[i]->m_groupId==idOld; i++)
img->m_imp->m_strokes[i]->m_groupId==idNew;
}*/

    int strokeCount = img->getStrokeCount();
    m_imp->m_computedAlmostOnce |= img->m_imp->m_computedAlmostOnce;
    for (i = 0; i < strokeCount; i++) {
      VIStroke *srcStroke = img->m_imp->m_strokes[i];
      VIStroke *tarStroke = new VIStroke(*srcStroke);
      int strokeId        = srcStroke->m_s->getId();
      if (getStrokeById(strokeId) == 0) tarStroke->m_s->setId(strokeId);

      index++;
      if (m_imp->m_insideGroup == TGroupId())
        m_imp->m_strokes.push_back(tarStroke);
      else  // if we are inside a group, the images must become part of that
            // group
      {
        tarStroke->m_groupId =
            TGroupId(m_imp->m_insideGroup, tarStroke->m_groupId);
        m_imp->insertStrokeAt(tarStroke, index);
      }

      changedStrokeArray.push_back(index);
    }
  }

  notifyChangedStrokes(changedStrokeArray, std::vector<TStroke *>(), false);
}
//-------------------------------------------------------------------

void TVectorImage::recomputeRegionsIfNeeded() {
  if (!m_imp->m_justLoaded) return;

  m_imp->m_justLoaded = false;

  std::vector<int> v(m_imp->m_strokes.size());
  int i;
  for (i = 0; i < (int)m_imp->m_strokes.size(); i++) v[i] = i;

  m_imp->notifyChangedStrokes(v, std::vector<TStroke *>(), false);
}

//-----------------------------------------------------------------------------

void TVectorImage::eraseStyleIds(const std::vector<int> styleIds) {
  int j;
  for (j = 0; j < (int)styleIds.size(); j++) {
    int styleId = styleIds[j];

    int strokeCount = getStrokeCount();
    int i;
    for (i = strokeCount - 1; i >= 0; i--) {
      TStroke *stroke = getStroke(i);
      if (stroke && stroke->getStyle() == styleId) removeStroke(i);
    }
    int regionCount = getRegionCount();
    for (i = 0; i < regionCount; i++) {
      TRegion *region = getRegion(i);
      if (!region || region->getStyle() != styleId) continue;
      TPointD p;
      if (region->getInternalPoint(p)) fill(p, 0);
    }
  }
}

//-------------------------------------------------------------------

void TVectorImage::insertImage(const TVectorImageP &img,
                               const std::vector<int> &dstIndices) {
  UINT i;
  UINT imageSize = img->getStrokeCount();
  assert(dstIndices.size() == imageSize);

  // img->m_imp->reindexGroups(*m_imp);
  std::vector<int> changedStrokeArray(imageSize);

  std::vector<VIStroke *>::iterator it = m_imp->m_strokes.begin();

  for (i = 0; i < imageSize; i++) {
    assert(i == 0 || dstIndices[i] > dstIndices[i - 1]);

    VIStroke *srcStroke = img->m_imp->m_strokes[i];
    VIStroke *tarStroke = new VIStroke(*srcStroke);
    int strokeId        = srcStroke->m_s->getId();
    if (getStrokeById(strokeId) == 0) tarStroke->m_s->setId(strokeId);
    advance(it, (i == 0) ? dstIndices[i] : dstIndices[i] - dstIndices[i - 1]);

    it = m_imp->m_strokes.insert(it, tarStroke);

    changedStrokeArray[i] = dstIndices[i];
  }
  m_imp->reindexEdges(changedStrokeArray, true);

  notifyChangedStrokes(changedStrokeArray, std::vector<TStroke *>(), false);
  // m_imp->computeRegions();
}

//-----------------------------------------------------------------------------

void TVectorImage::enableRegionComputing(bool enabled,
                                         bool notIntersectingStrokes) {
  m_imp->m_computeRegions         = enabled;
  m_imp->m_notIntersectingStrokes = notIntersectingStrokes;
}

//------------------------------------------------------------------------------

void TVectorImage::enableMinimizeEdges(bool enabled) {
  m_imp->m_minimizeEdges = enabled;
}

//-----------------------------------------------------------------------------

TVectorImageP TVectorImage::splitImage(const std::vector<int> &indices,
                                       bool removeFlag) {
  TVectorImageP out             = new TVectorImage;
  out->m_imp->m_maxGroupId      = m_imp->m_maxGroupId;
  out->m_imp->m_maxGhostGroupId = m_imp->m_maxGhostGroupId;

  std::vector<int> toBeRemoved;

  TPalette *vp = getPalette();
  if (vp) out->setPalette(vp->clone());

  for (UINT i = 0; i < indices.size(); ++i) {
    VIStroke *ref = m_imp->m_strokes[indices[i]];
    assert(ref);
    VIStroke *vs       = new VIStroke(*ref);
    vs->m_isNewForFill = true;
    out->m_imp->m_strokes.push_back(vs);
  }

  if (removeFlag) removeStrokes(indices, true, true);
  out->m_imp->m_areValidRegions    = false;
  out->m_imp->m_computedAlmostOnce = m_imp->m_computedAlmostOnce;
  return out;
}

//-----------------------------------------------------------------------------

TVectorImageP TVectorImage::splitSelected(bool removeFlag) {
  TVectorImageP out = new TVectorImage;
  std::vector<int> toBeRemoved;

  for (UINT i = 0; i < getStrokeCount(); ++i) {
    VIStroke *ref = m_imp->m_strokes[i];
    assert(ref);
    if (ref->m_s->getFlag(TStroke::c_selected_flag)) {
      VIStroke *stroke = new VIStroke(*ref);
      out->m_imp->m_strokes.push_back(stroke);
      if (removeFlag) {
        toBeRemoved.push_back(i);
        //  	  removeStroke(i);
        //			delete ref;
        //  	  i--;
      }
    }
  }
  removeStrokes(toBeRemoved, true, true);
  out->m_imp->m_areValidRegions = false;
  return out;
}

//-----------------------------------------------------------------------------

void TVectorImage::validateRegions(bool state) {
  m_imp->m_areValidRegions = state;
}

//-----------------------------------------------------------------------------

/*
void TVectorImage::invalidateBBox()
{
  for(UINT i=0; i<getRegionCount(); i++)
    getRegion(i)->invalidateBBox();
}
*/
//-----------------------------------------------------------------------------

void TVectorImage::setFillData(std::unique_ptr<IntersectionBranch[]> const &v,
                               UINT branchCount, bool doComputeRegions) {
  m_imp->setFillData(v, branchCount, doComputeRegions);
}

//-----------------------------------------------------------------------------

UINT TVectorImage::getFillData(std::unique_ptr<IntersectionBranch[]> &v) {
  return m_imp->getFillData(v);
}

//-----------------------------------------------------------------------------

void TVectorImage::enableStrokeStyle(int index, bool enable) {
  DisabledStrokeStyles &disabledSet = getDisabledStrokeStyleSet();
  if (enable)
    disabledSet.erase(index);
  else
    disabledSet.insert(index);
}

//-----------------------------------------------------------------------------

bool TVectorImage::isStrokeStyleEnabled(int index) {
  return isStrokeStyleEnabled__(index);
}

//-----------------------------------------------------------------------------

void TVectorImage::getUsedStyles(std::set<int> &styles) const {
  UINT strokeCount = getStrokeCount();
  UINT i           = 0;
  for (; i < strokeCount; ++i) {
    VIStroke *srcStroke = m_imp->m_strokes[i];
    int styleId         = srcStroke->m_s->getStyle();
    if (styleId != 0) styles.insert(styleId);
    std::list<TEdge *>::const_iterator it = srcStroke->m_edgeList.begin();
    for (; it != srcStroke->m_edgeList.end(); ++it) {
      styleId = (*it)->m_styleId;
      if (styleId != 0) styles.insert(styleId);
    }
  }
}

//-----------------------------------------------------------------------------

inline double recomputeW1(double oldW, const TStroke &oldStroke,
                          const TStroke &newStroke, double startW) {
  double oldLength = oldStroke.getLength();
  double newLength = newStroke.getLength();

  assert(startW <= oldW);
  assert(newLength < oldLength);

  double s = oldStroke.getLength(startW, oldW);
  assert(s <= newLength || areAlmostEqual(s, newLength, 1e-5));

  return newStroke.getParameterAtLength(s);
}

//-----------------------------------------------------------------------------
inline double recomputeW2(double oldW, const TStroke &oldStroke,
                          const TStroke &newStroke, double length) {
  double s = oldStroke.getLength(oldW);
  return newStroke.getParameterAtLength(length + s);
}

//-----------------------------------------------------------------------------

inline double recomputeW(double oldW, const TStroke &oldStroke,
                         const TStroke &newStroke, bool isAtBegin) {
  double oldLength = oldStroke.getLength();
  double newLength = newStroke.getLength();

  assert(newLength < oldLength);
  double s =
      oldStroke.getLength(oldW) - ((isAtBegin) ? 0 : oldLength - newLength);
  assert(s <= newLength || areAlmostEqual(s, newLength, 1e-5));

  return newStroke.getParameterAtLength(s);
}

//-----------------------------------------------------------------------------
#ifdef _DEBUG
void TVectorImage::checkIntersections() { m_imp->checkIntersections(); }
#endif

/*
void TVectorImage::reassignStyles()
{
  set<int> styles;
  UINT  strokeCount = getStrokeCount();
  UINT i=0;

  for( ; i< strokeCount; ++i)
    {
     int styleId = getStroke(i)->getStyle();
     if(styleId != 0) styles.insert(styleId);
    }
  UINT regionCount = getRegionCount();
  for( i = 0; i< regionCount; ++i)
    {
     int styleId = getRegion(i)->getStyle();
     if(styleId != 0) styles.insert(styleId);
    }

  map<int, int> conversionTable;
  for(set<int>::iterator it = styles.begin(); it != styles.end(); ++it)
    {
     int styleId = *it;
     conversionTable[styleId] = styleId + 13;
    }

  for( i = 0; i< strokeCount; ++i)
    {
     TStroke *stroke = getStroke(i);
     int styleId = stroke->getStyle();
     if(styleId != 0)
       {
        map<int, int>::iterator it = conversionTable.find(styleId);
        if(it != conversionTable.end())
          stroke->setStyle(it->second);
       }
    }

  for( i = 0; i< regionCount; ++i)
    {
     TRegion *region = getRegion(i);
     int styleId = region->getStyle();
     if(styleId != 0)
       {
        map<int, int>::iterator it = conversionTable.find(styleId);
        if(it != conversionTable.end())
          region->setStyle(it->second);
       }
    }
}

*/

//-----------------------------------------------------------------------------

bool TVectorImage::isComputedRegionAlmostOnce() const {
  return m_imp->m_computedAlmostOnce;
}

//-----------------------------------------------------------------------------

void TVectorImage::splitStroke(int strokeIndex,
                               const std::vector<DoublePair> &sortedWRanges) {
  m_imp->splitStroke(strokeIndex, sortedWRanges);
}

void TVectorImage::Imp::splitStroke(
    int strokeIndex, const std::vector<DoublePair> &sortedWRanges) {
  int i;
  VIStroke *subV = 0;

  if (strokeIndex >= (int)m_strokes.size() || sortedWRanges.empty()) return;

  VIStroke *vs     = m_strokes[strokeIndex];
  TGroupId groupId = vs->m_groupId;

  // se e' un self loop, alla fine non lo sara', e deve stare insieme
  // alle stroke non loopate. sposto lo stroke se serve
  /*
{
if (vs->m_s->isSelfLoop())
int up = strokeIndex+1;
while (up<(int)m_strokes.size() && m_strokes[up]->m_s->isSelfLoop())
up++;
int dn = strokeIndex-1;
while (dn>=0 && m_strokes[dn]->m_s->isSelfLoop())
dn--;
if ((up == m_strokes.size() || up!=strokeIndex+1) && (dn<0 ||
dn!=strokeIndex-1))
{
if (up>=(int)m_strokes.size())
{
assert(dn>=0);
moveStroke(strokeIndex, dn+1);
strokeIndex = dn+1;
}
else
{
moveStroke(strokeIndex, up-1);
strokeIndex = up-1;
}
}
}
*/
  assert(vs == m_strokes[strokeIndex]);

  bool toBeJoined =
      (vs->m_s->isSelfLoop() && sortedWRanges.front().first == 0.0 &&
       sortedWRanges.back().second == 1.0);

  int styleId = vs->m_s->getStyle();
  TStroke::OutlineOptions oOptions(vs->m_s->outlineOptions());

  m_regions.clear();

  std::list<TEdge *> origEdgeList;  // metto al pizzo la edge std::list della
                                    // stroke, perche' la erase intersection ne
                                    // fara' scempio
  std::list<TEdge *>::iterator it   = vs->m_edgeList.begin(),
                               it_e = vs->m_edgeList.end();
  for (; it != it_e; ++it) origEdgeList.push_back(new TEdge(**it, false));

  removeStroke(strokeIndex, false);

  std::vector<std::list<TEdge *>> edgeList(sortedWRanges.size());
  strokeIndex--;

  int wSize = (int)sortedWRanges.size();

  for (i = 0; i < wSize; i++) {
    assert(sortedWRanges[i].first < sortedWRanges[i].second);
    assert(i == wSize - 1 ||
           sortedWRanges[i].second <= sortedWRanges[i + 1].first);
    assert(sortedWRanges[i].first >= 0 && sortedWRanges[i].first <= 1);
    assert(sortedWRanges[i].second >= 0 && sortedWRanges[i].second <= 1);

    subV = new VIStroke(new TStroke(), groupId);
    TStroke s, dummy;

    if (areAlmostEqual(sortedWRanges[i].first, 0, 1e-4))
      s = *vs->m_s;
    else
      vs->m_s->split(sortedWRanges[i].first, dummy, s);

    double lenAtW0 = vs->m_s->getLength(sortedWRanges[i].first);
    double lenAtW1 = vs->m_s->getLength(sortedWRanges[i].second);
    double newW1   = s.getParameterAtLength(lenAtW1 - lenAtW0);

    if (areAlmostEqual(newW1, 1.0, 1e-4))
      *(subV->m_s) = s;
    else
      s.split(newW1, *(subV->m_s), dummy);

    strokeIndex++;
    /*assert(m_strokes[strokeIndex]->m_edgeList.empty());
assert(m_strokes[strokeIndex-wSize+1]->m_edgeList.empty());*/

    std::list<TEdge *>::const_iterator it   = origEdgeList.begin(),
                                       it_e = origEdgeList.end();
    for (; it != it_e; ++it) {
      double wMin = std::min((*it)->m_w0, (*it)->m_w1);
      double wMax = std::max((*it)->m_w0, (*it)->m_w1);

      if (wMin >= sortedWRanges[i].second || wMax <= sortedWRanges[i].first)
        continue;

      TEdge *e = new TEdge(**it, false);
      if (wMin < sortedWRanges[i].first)
        wMin = 0.0;
      else
        wMin =
            recomputeW1(wMin, *(vs->m_s), *(subV->m_s), sortedWRanges[i].first);

      if (wMax > sortedWRanges[i].second)
        wMax = 1.0;
      else
        wMax =
            recomputeW1(wMax, *(vs->m_s), *(subV->m_s), sortedWRanges[i].first);

      if (e->m_w0 < e->m_w1)
        e->m_w0 = wMin, e->m_w1 = wMax;
      else
        e->m_w1 = wMin, e->m_w0 = wMax;
      e->m_r     = 0;
      e->m_s     = subV->m_s;
      e->m_index = strokeIndex;
      edgeList[i].push_back(e);
    }
    subV->m_edgeList.clear();
    insertStrokeAt(subV, strokeIndex);
    subV->m_s->setStyle(styleId);
    subV->m_s->outlineOptions() = oOptions;
  }

  clearPointerContainer(origEdgeList);

  if (toBeJoined)  // la stroke e' un loop, quindi i due choncketti iniziali e
                   // finali vanno joinati
  {
    VIStroke *s0           = m_strokes[strokeIndex];
    VIStroke *s1           = m_strokes[strokeIndex - wSize + 1];
    std::list<TEdge *> &l0 = edgeList.back();
    std::list<TEdge *> &l1 = edgeList.front();

    // assert(s0->m_edgeList.empty());
    // assert(s1->m_edgeList.empty());
    removeStroke(strokeIndex - wSize + 1, false);

    strokeIndex--;
    removeStroke(strokeIndex, false);

    VIStroke *s = new VIStroke(joinStrokes(s0->m_s, s1->m_s), groupId);
    insertStrokeAt(s, strokeIndex);

    std::list<TEdge *>::iterator it = l0.begin(), it_e = l0.end();
    for (; it != it_e; ++it) {
      (*it)->m_s     = s->m_s;
      (*it)->m_index = strokeIndex;
      (*it)->m_w0    = recomputeW2((*it)->m_w0, *(s0->m_s), *(s->m_s), 0);
      (*it)->m_w1    = recomputeW2((*it)->m_w1, *(s0->m_s), *(s->m_s), 0);
    }
    it            = l1.begin();
    double length = s0->m_s->getLength();
    while (it != l1.end()) {
      (*it)->m_s     = s->m_s;
      (*it)->m_index = strokeIndex;
      (*it)->m_w0    = recomputeW2((*it)->m_w0, *(s1->m_s), *(s->m_s), length);
      (*it)->m_w1    = recomputeW2((*it)->m_w1, *(s1->m_s), *(s->m_s), length);
      l0.push_back(*it);
      it = l1.erase(it);
    }
    assert(l1.empty());
    edgeList.erase(edgeList.begin());

    std::vector<DoublePair> appSortedWRanges;

    wSize--;

    delete s0;
    delete s1;
  }

  // checkIntersections();

  // double len  = e->m_s->getLength();
  // if (recomputeRegions)

  if (m_computedAlmostOnce) {
    computeRegions();
    assert((int)edgeList.size() == wSize);
    assert((int)m_strokes.size() > strokeIndex);

    for (i = 0; i < wSize; i++)
      transferColors(edgeList[i],
                     m_strokes[strokeIndex - wSize + i + 1]->m_edgeList, false,
                     false, false);
  }

  for (i = 0; i < wSize; i++) clearPointerContainer(edgeList[i]);

  delete vs;
}

//-----------------------------------------------------------------------------

static void computeEdgeList(TStroke *newS, const std::list<TEdge *> &edgeList1,
                            bool join1AtBegin,
                            const std::list<TEdge *> &edgeList2,
                            bool join2AtBegin, std::list<TEdge *> &edgeList) {
  std::list<TEdge *>::const_iterator it;

  if (!edgeList1.empty()) {
    TStroke *s1    = edgeList1.front()->m_s;
    double length1 = s1->getLength();
    ;

    for (it = edgeList1.begin(); it != edgeList1.end(); ++it) {
      double l0 = s1->getLength((*it)->m_w0), l1 = s1->getLength((*it)->m_w1);
      if (join1AtBegin) l0 = length1 - l0, l1 = length1 - l1;

      TEdge *e         = new TEdge();
      e->m_toBeDeleted = true;
      e->m_index       = -1;
      e->m_s           = newS;
      e->m_styleId     = (*it)->m_styleId;
      e->m_w0          = newS->getParameterAtLength(l0);
      e->m_w1          = newS->getParameterAtLength(l1);
      edgeList.push_back(e);
    }
  }

  if (!edgeList2.empty()) {
    TStroke *s2    = edgeList2.front()->m_s;
    double offset  = newS->getLength(newS->getW(s2->getPoint(0.0)));
    double length2 = s2->getLength();
    for (it = edgeList2.begin(); it != edgeList2.end(); ++it) {
      double l0 = s2->getLength((*it)->m_w0), l1 = s2->getLength((*it)->m_w1);
      if (!join2AtBegin) l0 = length2 - l0, l1 = length2 - l1;

      TEdge *e         = new TEdge();
      e->m_toBeDeleted = true;
      e->m_index       = -1;
      e->m_s           = newS;
      e->m_styleId     = (*it)->m_styleId;
      e->m_w0          = newS->getParameterAtLength(offset + l0);
      e->m_w1          = newS->getParameterAtLength(offset + l1);
      edgeList.push_back(e);
    }
  }
}

//-----------------------------------------------------------------------------
#ifdef _DEBUG

//#include "tpalette.h"
#include "tcolorstyles.h"

void printEdges(std::ofstream &os, char *str, TPalette *plt,
                const std::list<TEdge *> &edges) {
  std::list<TEdge *>::const_iterator it;

  os << str << std::endl;

  for (it = edges.begin(); it != edges.end(); ++it) {
    TColorStyle *style = plt->getStyle((*it)->m_styleId);
    TPixel32 color     = style->getMainColor();
    os << "w0-w1:(" << (*it)->m_w0 << "-->" << (*it)->m_w1 << ")" << std::endl;
    os << "color=(" << color.r << "," << color.g << "," << color.b << ")"
       << std::endl;
  }
  os << std::endl << std::endl << std::endl;
}
#else
#define printEdges

#endif
//-----------------------------------------------------------------------------

#ifdef _DEBUG
void TVectorImage::Imp::printStrokes(std::ofstream &os) {
  for (int i = 0; i < (int)m_strokes.size(); i++) {
    os << "*****stroke #" << i << " *****";
    m_strokes[i]->m_s->print(os);
  }
}

#endif

//-----------------------------------------------------------------------------

TStroke *TVectorImage::removeEndpoints(int strokeIndex) {
  return m_imp->removeEndpoints(strokeIndex);
}

void TVectorImage::restoreEndpoints(int index, TStroke *oldStroke) {
  m_imp->restoreEndpoints(index, oldStroke);
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::Imp::extendStrokeSmoothly(int index,
                                                  const TThickPoint &pos,
                                                  int cpIndex) {
  TStroke *stroke  = m_strokes[index]->m_s;
  TGroupId groupId = m_strokes[index]->m_groupId;

  int cpCount = stroke->getControlPointCount();
  int styleId = stroke->getStyle();
  const TThickQuadratic *q =
      stroke->getChunk(cpIndex == 0 ? 0 : stroke->getChunkCount() - 1);

  double len    = q->getLength();
  double w      = exp(-len * 0.01);
  TThickPoint m = q->getThickP1();

  TThickPoint p1 =
      (cpIndex == 0 ? q->getThickP0() : q->getThickP2()) * (1 - w) + m * w;
  TThickPoint middleP = (p1 + pos) * 0.5;

  double angle = fabs(cross(normalize(m - middleP), normalize(pos - middleP)));
  if (angle < 0.05) middleP = (m + pos) * 0.5;

  stroke->setControlPoint(cpIndex, middleP);
  if (isAlmostZero(len)) {
    if (cpIndex == 0)
      stroke->setControlPoint(1,
                              middleP * 0.1 + stroke->getControlPoint(2) * 0.9);
    else
      stroke->setControlPoint(
          cpCount - 2,
          middleP * 0.1 + stroke->getControlPoint(cpCount - 3) * 0.9);
  }

  std::vector<TThickPoint> points(cpCount);
  for (int i  = 0; i < cpCount - 1; i++)
    points[i] = stroke->getControlPoint((cpIndex == 0) ? cpCount - i - 1 : i);
  points[cpCount - 1] = pos;

  TStroke *newStroke = new TStroke(points);
  newStroke->setStyle(styleId);
  newStroke->outlineOptions() = stroke->outlineOptions();
  std::list<TEdge *> oldEdgeList, emptyList;
  computeEdgeList(newStroke, m_strokes[index]->m_edgeList, cpIndex == 0,
                  emptyList, 0, oldEdgeList);

  std::vector<int> toBeDeleted;
  toBeDeleted.push_back(index);
  removeStrokes(toBeDeleted, true, false);

  insertStrokeAt(new VIStroke(newStroke, groupId), index, false);
  computeRegions();
  transferColors(oldEdgeList, m_strokes[index]->m_edgeList, true, false, true);

  return m_strokes[index];
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::Imp::extendStroke(int index, const TThickPoint &p,
                                          int cpIndex) {
  TGroupId groupId = m_strokes[index]->m_groupId;

  TStroke *stroke = m_strokes[index]->m_s;

  TStroke *ret;
  int cpCount = stroke->getControlPointCount();
  int count   = 0;
  std::vector<TThickPoint> points(cpCount + 2);
  int i, incr = (cpIndex == 0) ? -1 : 1;
  for (i = ((cpIndex == 0) ? cpCount - 1 : 0); i != cpIndex + incr; i += incr)
    points[count++] = stroke->getControlPoint(i);
  TThickPoint tp(p, points[count - 1].thick);
  points[count++] = 0.5 * (stroke->getControlPoint(cpIndex) + tp);
  points[count++] = tp;

  TStroke *newStroke = new TStroke(points);
  newStroke->setStyle(stroke->getStyle());
  newStroke->outlineOptions() = stroke->outlineOptions();
  ret                         = newStroke;
  std::list<TEdge *> oldEdgeList, emptyList;

  if (m_computedAlmostOnce)
    computeEdgeList(newStroke, m_strokes[index]->m_edgeList, cpIndex == 0,
                    emptyList, false, oldEdgeList);

  std::vector<int> toBeDeleted;
  toBeDeleted.push_back(index);
  removeStrokes(toBeDeleted, true, false);

  // removeStroke(index, false);

  insertStrokeAt(new VIStroke(newStroke, groupId), index, false);

  if (m_computedAlmostOnce) {
    computeRegions();
    transferColors(oldEdgeList, m_strokes[index]->m_edgeList, true, false,
                   true);
  }
  return m_strokes[index];
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::Imp::joinStroke(int index1, int index2, int cpIndex1,
                                        int cpIndex2) {
  assert(m_strokes[index1]->m_groupId == m_strokes[index2]->m_groupId);

  TGroupId groupId = m_strokes[index1]->m_groupId;

  TStroke *stroke1 = m_strokes[index1]->m_s;
  TStroke *stroke2 = m_strokes[index2]->m_s;
  // TStroke* ret;
  int cpCount1 = stroke1->getControlPointCount();
  int cpCount2 = stroke2->getControlPointCount();
  int styleId  = stroke1->getStyle();

  int count = 0;
  std::vector<TThickPoint> points(cpCount1 +
                                  ((index1 != index2) ? cpCount2 : 1) + 1);
  int i, incr = (cpIndex1 == 0) ? -1 : 1;
  for (i = ((cpIndex1 == 0) ? cpCount1 - 1 : 0); i != cpIndex1 + incr;
       i += incr)
    points[count++] = stroke1->getControlPoint(i);
  points[count++]   = 0.5 * (stroke1->getControlPoint(cpIndex1) +
                           stroke2->getControlPoint(cpIndex2));
  if (index1 != index2) {
    incr = (cpIndex2 == 0) ? 1 : -1;
    for (i = cpIndex2; i != ((cpIndex2 == 0) ? cpCount2 - 1 : 0) + incr;
         i += incr)
      points[count++] = stroke2->getControlPoint(i);
  } else
    points[count++] = stroke2->getControlPoint(cpIndex2);

  TStroke *newStroke = new TStroke(points);
  newStroke->setStyle(styleId);
  newStroke->outlineOptions() = stroke1->outlineOptions();
  // ret = newStroke;
  if (index1 == index2) newStroke->setSelfLoop();
  std::list<TEdge *> oldEdgeList, emptyList;

  computeEdgeList(
      newStroke, m_strokes[index1]->m_edgeList, cpIndex1 == 0,
      (index1 != index2) ? m_strokes[index2]->m_edgeList : emptyList,
      cpIndex2 == 0, oldEdgeList);

  std::vector<int> toBeDeleted;
  toBeDeleted.push_back(index1);
  if (index1 != index2) toBeDeleted.push_back(index2);
  removeStrokes(toBeDeleted, true, false);

  insertStrokeAt(new VIStroke(newStroke, groupId), index1, false);
  computeRegions();
  transferColors(oldEdgeList, m_strokes[index1]->m_edgeList, true, false, true);
  return m_strokes[index1];
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::Imp::joinStrokeSmoothly(int index1, int index2,
                                                int cpIndex1, int cpIndex2) {
  assert(m_strokes[index1]->m_groupId == m_strokes[index2]->m_groupId);

  TGroupId groupId = m_strokes[index1]->m_groupId;

  TStroke *stroke1 = m_strokes[index1]->m_s;
  TStroke *stroke2 = m_strokes[index2]->m_s;
  TStroke *ret;
  int cpCount1 = stroke1->getControlPointCount();
  int cpCount2 = stroke2->getControlPointCount();
  int styleId  = stroke1->getStyle();

  int qCount1 = stroke1->getChunkCount();
  int qCount2 = stroke2->getChunkCount();
  const TThickQuadratic *q1 =
      stroke1->getChunk(cpIndex1 == 0 ? 0 : qCount1 - 1);
  const TThickQuadratic *q2 =
      stroke2->getChunk(cpIndex2 == 0 ? 0 : qCount2 - 1);

  double len1 = q1->getLength();
  assert(len1 >= 0);
  if (len1 <= 0) len1 = 0;
  double w1           = exp(-len1 * 0.01);

  double len2 = q2->getLength();
  assert(len2 >= 0);
  if (len2 <= 0) len2 = 0;
  double w2           = exp(-len2 * 0.01);

  TThickPoint extreme1 = cpIndex1 == 0 ? q1->getThickP0() : q1->getThickP2();
  TThickPoint extreme2 = cpIndex2 == 0 ? q2->getThickP0() : q2->getThickP2();

  TThickPoint m1 = q1->getThickP1();
  TThickPoint m2 = q2->getThickP1();

  TThickPoint p1 = extreme1 * (1 - w1) + m1 * w1;
  TThickPoint p2 = extreme2 * (1 - w2) + m2 * w2;

  TThickPoint middleP = (p1 + p2) * 0.5;

  double angle = fabs(cross(normalize(m1 - middleP), normalize(m2 - middleP)));
  if (angle < 0.05) middleP = (m1 + m2) * 0.5;

  stroke1->setControlPoint(cpIndex1, middleP);
  if (isAlmostZero(len1)) {
    if (cpIndex1 == 0)
      stroke1->setControlPoint(
          1, middleP * 0.1 + stroke1->getControlPoint(2) * 0.9);
    else
      stroke1->setControlPoint(
          cpCount1 - 2,
          middleP * 0.1 + stroke1->getControlPoint(cpCount1 - 3) * 0.9);
  }

  stroke2->setControlPoint(cpIndex2, middleP);
  if (isAlmostZero(len2)) {
    if (cpIndex2 == 0)
      stroke2->setControlPoint(
          1, middleP * 0.1 + stroke2->getControlPoint(2) * 0.9);
    else
      stroke2->setControlPoint(
          cpCount2 - 2,
          middleP * 0.1 + stroke2->getControlPoint(cpCount2 - 3) * 0.9);
  }

  if (stroke1 == stroke2) {
    std::list<TEdge *> oldEdgeList, emptyList;
    computeEdgeList(stroke1, m_strokes[index1]->m_edgeList, cpIndex1 == 0,
                    emptyList, false, oldEdgeList);
    eraseIntersection(index1);
    m_strokes[index1]->m_isNewForFill = true;
    stroke1->setSelfLoop();
    computeRegions();
    transferColors(oldEdgeList, m_strokes[index1]->m_edgeList, true, false,
                   true);
    return m_strokes[index1];
    // nundo->m_newStroke=new TStroke(*stroke1);
    // nundo->m_newStrokeId=stroke1->getId();
  }

  std::vector<TThickPoint> points;
  points.reserve(cpCount1 + cpCount2 - 1);

  int incr = (cpIndex1) ? 1 : -1;
  int stop = cpIndex1;

  int i = cpCount1 - 1 - cpIndex1;
  for (; i != stop; i += incr) points.push_back(stroke1->getControlPoint(i));

  incr = (cpIndex2) ? -1 : 1;
  stop = cpCount2 - 1 - cpIndex2;

  for (i = cpIndex2; i != stop; i += incr)
    points.push_back(stroke2->getControlPoint(i));

  points.push_back(stroke2->getControlPoint(stop));

  TStroke *newStroke = new TStroke(points);
  newStroke->setStyle(styleId);
  newStroke->outlineOptions() = stroke1->outlineOptions();
  ret                         = newStroke;
  // nundo->m_newStroke=new TStroke(*newStroke);
  // nundo->m_newStrokeId=newStroke->getId();
  std::list<TEdge *> oldEdgeList;
  // ofstream os("c:\\temp\\edges.txt");

  // printEdges(os, "****edgelist1", getPalette(),
  // m_imp->m_strokes[index1]->m_edgeList);
  // printEdges(os, "****edgelist2", getPalette(),
  // m_imp->m_strokes[index2]->m_edgeList);

  computeEdgeList(newStroke, m_strokes[index1]->m_edgeList, cpIndex1 == 0,
                  m_strokes[index2]->m_edgeList, cpIndex2 == 0, oldEdgeList);
  // printEdges(os, "****edgelist", getPalette(), oldEdgeList);

  std::vector<int> toBeDeleted;
  toBeDeleted.push_back(index1);
  toBeDeleted.push_back(index2);
  removeStrokes(toBeDeleted, true, false);

  insertStrokeAt(new VIStroke(newStroke, groupId), index1);
  computeRegions();
  transferColors(oldEdgeList, m_strokes[index1]->m_edgeList, true, false, true);

  return m_strokes[index1];

  //  TUndoManager::manager()->add(nundo);
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::joinStroke(int index1, int index2, int cpIndex1,
                                   int cpIndex2, bool isSmooth) {
  int finalStyle = -1;

  if (index1 > index2) {
    finalStyle = getStroke(index1)->getStyle();
    std::swap(index1, index2);
    std::swap(cpIndex1, cpIndex2);
  }
  /*
if (index1==index2) //selfLoop!
{
if (index1>0 && index1<(int)getStrokeCount()-1 &&
  !getStroke(index1-1)->isSelfLoop() &&
  !getStroke(index1+1)->isSelfLoop())
{
for (UINT i = index1+2; i<getStrokeCount() && !getStroke(i)->isSelfLoop(); i++)
  ;
moveStroke(index1, i-1);
index1 = index2 = i-1;
}
}
*/
  VIStroke *ret;
  if (isSmooth)
    ret = m_imp->joinStrokeSmoothly(index1, index2, cpIndex1, cpIndex2);
  else
    ret = m_imp->joinStroke(index1, index2, cpIndex1, cpIndex2);

  if (finalStyle != -1) getStroke(index1)->setStyle(finalStyle);
  return ret;
}

//-----------------------------------------------------------------------------

VIStroke *TVectorImage::extendStroke(int index, const TThickPoint &p,
                                     int cpIndex, bool isSmooth) {
  if (isSmooth)
    return m_imp->extendStrokeSmoothly(index, p, cpIndex);
  else
    return m_imp->extendStroke(index, p, cpIndex);
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

TInputStreamInterface &TInputStreamInterface::operator>>(TPixel32 &pixel) {
  return *this >> pixel.r >> pixel.g >> pixel.b >> pixel.m;
}

//-------------------------------------------------------------------

TOutputStreamInterface &TOutputStreamInterface::operator<<(
    const TPixel32 &pixel) {
  return *this << pixel.r << pixel.g << pixel.b << pixel.m;
}

//-------------------------------------------------------------------

void TVectorImage::setAutocloseTolerance(double val) {
  m_imp->m_autocloseTolerance = val;
}

//-------------------------------------------------------------------

double TVectorImage::getAutocloseTolerance() const {
  return m_imp->m_autocloseTolerance;
}

//-------------------------------------------------------------------

TThread::Mutex *TVectorImage::getMutex() const { return m_imp->m_mutex; }

//-------------------------------------------------------------------

void TVectorImage::areaFill(TStroke *stroke, int index, bool m_onlyUnfilled) {
  TVectorImage v;
  v.addStroke(stroke);
  v.findRegions();

  for (UINT i = 0; i < v.getRegionCount(); i++)
    for (UINT j = 0; j < getRegionCount(); j++) {
      if (m_imp->m_insideGroup != TGroupId() &&
          !m_imp->m_insideGroup.isParentOf(
              m_imp->m_strokes[getRegion(j)->getEdge(0)->m_index]->m_groupId))
        continue;

      if (v.getRegion(i)->contains(*getRegion(j)))
        getRegion(j)->setStyle(index);
    }

  v.removeStroke(0);
}

VIStroke *cloneVIStroke(VIStroke *vs) { return new VIStroke(*vs); }

void deleteVIStroke(VIStroke *vs) {
  delete vs;
  vs = 0;
}

//-------------------------------------------------------------------

bool TVectorImage::sameSubGroup(int index0, int index1) const {
  if (index0 < 0 || index1 < 0) return 0;
  return m_imp->m_strokes[index0]->m_groupId.getCommonParentDepth(
             m_imp->m_strokes[index1]->m_groupId) >
         m_imp->m_insideGroup.getDepth();
}

//-------------------------------------------------------------------

int TVectorImage::getCommonGroupDepth(int index0, int index1) const {
  if (index0 < 0 || index1 < 0) return 0;
  return m_imp->m_strokes[index0]->m_groupId.getCommonParentDepth(
      m_imp->m_strokes[index1]->m_groupId);
}

//-------------------------------------------------------------------

int TVectorImage::ungroup(int fromIndex) {
  m_imp->m_insideGroup = TGroupId();

  assert(m_imp->m_strokes[fromIndex]->m_groupId.isGrouped() != 0);
  std::vector<int> changedStrokes;

  int toIndex = fromIndex + 1;

  while (toIndex < (int)m_imp->m_strokes.size() &&
         m_imp->m_strokes[fromIndex]->m_groupId.getCommonParentDepth(
             m_imp->m_strokes[toIndex]->m_groupId) >= 1)
    toIndex++;

  toIndex--;

  TGroupId groupId;

  if (fromIndex > 0 &&
      m_imp->m_strokes[fromIndex - 1]->m_groupId.isGrouped(true) != 0)
    groupId = m_imp->m_strokes[fromIndex - 1]->m_groupId;
  else if (toIndex < (int)m_imp->m_strokes.size() - 1 &&
           m_imp->m_strokes[toIndex + 1]->m_groupId.isGrouped(true) != 0)
    groupId = m_imp->m_strokes[toIndex + 1]->m_groupId;
  else
    groupId = TGroupId(this, true);

  for (int i = fromIndex;
       i <= toIndex || (i < (int)m_imp->m_strokes.size() &&
                        m_imp->m_strokes[i]->m_groupId.isGrouped(true) != 0);
       i++) {
    m_imp->m_strokes[i]->m_groupId.ungroup(groupId);
    changedStrokes.push_back(i);
  }

  notifyChangedStrokes(changedStrokes, std::vector<TStroke *>(), false);

  return toIndex - fromIndex + 1;
}

//-------------------------------------------------------------------

bool TVectorImage::isEnteredGroupStroke(int index) const {
  return m_imp->m_insideGroup.isParentOf(getVIStroke(index)->m_groupId);
}

//-------------------------------------------------------------------

bool TVectorImage::enterGroup(int index) {
  VIStroke *vs = getVIStroke(index);

  if (!vs->m_groupId.isGrouped()) return false;

  int newDepth = vs->m_groupId.getCommonParentDepth(m_imp->m_insideGroup) + 1;

  TGroupId newGroupId = vs->m_groupId;

  while (newGroupId.getDepth() > newDepth) newGroupId = newGroupId.getParent();

  if (newGroupId == m_imp->m_insideGroup) return false;

  m_imp->m_insideGroup = newGroupId;
  return true;
}

//-------------------------------------------------------------------

int TVectorImage::exitGroup() {
  if (m_imp->m_insideGroup == TGroupId()) return -1;

  int i, ret = -1;
  for (i = 0; i < (int)m_imp->m_strokes.size(); i++) {
    if (m_imp->m_strokes[i]->m_groupId.getCommonParentDepth(
            m_imp->m_insideGroup) >= m_imp->m_insideGroup.getDepth()) {
      ret = i;
      break;
    }
  }

  assert(i != m_imp->m_strokes.size());

  m_imp->m_insideGroup = m_imp->m_insideGroup.getParent();
  return ret;
}

//-------------------------------------------------------------------

void TVectorImage::group(int fromIndex, int count) {
  int i;
  assert(count >= 0);
  std::vector<int> changedStroke;

  TGroupId parent = TGroupId(this, false);

  for (i = 0; i < count; i++) {
    m_imp->m_strokes[fromIndex + i]->m_groupId =
        TGroupId(parent, m_imp->m_strokes[fromIndex + i]->m_groupId);
    changedStroke.push_back(fromIndex + i);
  }

  m_imp->rearrangeMultiGroup();  // see method's comment

  m_imp->regroupGhosts(changedStroke);

  notifyChangedStrokes(changedStroke, std::vector<TStroke *>(), false);

#ifdef _DEBUG
  m_imp->checkGroups();
#endif
}

//-------------------------------------------------------------------

int TVectorImage::getGroupDepth(UINT index) const {
  assert(index < m_imp->m_strokes.size());

  return m_imp->m_strokes[index]->m_groupId.isGrouped();
}

//-------------------------------------------------------------------

int TVectorImage::areDifferentGroup(UINT index1, bool isRegion1, UINT index2,
                                    bool isRegion2) const {
  return m_imp->areDifferentGroup(index1, isRegion1, index2, isRegion2);
}

//-------------------------------------------------------------------
/*this method is tricky.
it is not allow to have not-adiacent strokes  of same group.
but it can happen when you group  some already-grouped strokes creating
sub-groups.

example: vi made of 5 strokes, before grouping  (N=no group)
N
N
1
1
N
after grouping became:
2
2
2-1
2-1
2
not allowed!

this method moves strokes, so that  adiacent strokes have same group.
so after calling rearrangeMultiGroup the vi became:
2
2
2
2-1
2-1

*/

void TVectorImage::Imp::rearrangeMultiGroup() {
  UINT i, j, k;
  if (m_strokes.size() <= 0) return;
  for (i = 0; i < m_strokes.size() - 1; i++) {
    if (m_strokes[i]->m_groupId.isGrouped() &&
        m_strokes[i + 1]->m_groupId.isGrouped() &&
        m_strokes[i]->m_groupId != m_strokes[i + 1]->m_groupId) {
      TGroupId &prevId   = m_strokes[i]->m_groupId;
      TGroupId &idToMove = m_strokes[i + 1]->m_groupId;
      for (j = i + 1;
           j < m_strokes.size() && m_strokes[j]->m_groupId == idToMove; j++)
        ;
      if (j != m_strokes.size()) {
        j--;  // now range i+1-j contains the strokes to be moved.
        // let's compute where to move them (after last
        for (k = j; k < m_strokes.size() && m_strokes[k]->m_groupId != prevId;
             k++)
          ;
        if (k < m_strokes.size()) {
          for (; k < m_strokes.size() && m_strokes[k]->m_groupId == prevId; k++)
            ;
          moveStrokes(i + 1, j - i, k, false);
          rearrangeMultiGroup();
          return;
        }
      }
    }
  }
}

//-------------------------------------------------------------------

int TVectorImage::Imp::areDifferentGroup(UINT index1, bool isRegion1,
                                         UINT index2, bool isRegion2) const {
  TGroupId group1, group2;

  if (isRegion1) {
    TRegion *r = m_regions[index1];
    for (UINT i = 0; i < r->getEdgeCount(); i++)
      if (r->getEdge(i)->m_index >= 0) {
        group1 = m_strokes[r->getEdge(i)->m_index]->m_groupId;
        break;
      }
  } else
    group1 = m_strokes[index1]->m_groupId;
  if (isRegion2) {
    TRegion *r = m_regions[index2];
    for (UINT i = 0; i < r->getEdgeCount(); i++)
      if (r->getEdge(i)->m_index >= 0) {
        group2 = m_strokes[r->getEdge(i)->m_index]->m_groupId;
        break;
      }
  } else
    group2 = m_strokes[index2]->m_groupId;

  if (!group1 && !group2) return 0;

  if (group1 == group2)
    return -1;
  else
    return group1.getCommonParentDepth(group2);
}

//-------------------------------------------------------------------

int TGroupId::getCommonParentDepth(const TGroupId &id) const {
  int size1 = m_id.size();
  int size2 = id.m_id.size();
  int count;

  for (count = 0; count < std::min(size1, size2); count++)
    if (m_id[size1 - count - 1] != id.m_id[size2 - count - 1]) break;

  return count;
}

//-------------------------------------------------------------------

TGroupId::TGroupId(const TGroupId &parent, const TGroupId &id) {
  assert(parent.m_id[0] > 0);
  assert(id.m_id.size() > 0);

  if (id.isGrouped(true) != 0)
    m_id.push_back(parent.m_id[0]);
  else {
    m_id = id.m_id;
    int i;
    for (i = 0; i < (int)parent.m_id.size(); i++)
      m_id.push_back(parent.m_id[i]);
  }
}

/*
bool TGroupId::sameParent(const TGroupId& id) const
{
assert(!m_id.empty() || !id.m_id.empty());
return m_id.back()==id.m_id.back();
}
*/

TGroupId TGroupId::getParent() const {
  if (m_id.size() <= 1) return TGroupId();

  TGroupId ret = *this;
  ret.m_id.erase(ret.m_id.begin());
  return ret;
}

void TGroupId::ungroup(const TGroupId &id) {
  assert(id.isGrouped(true) != 0);
  assert(!m_id.empty());

  if (m_id.size() == 1)
    m_id[0] = id.m_id[0];
  else
    m_id.pop_back();
}

bool TGroupId::operator==(const TGroupId &id) const {
  if (m_id.size() != id.m_id.size()) return false;
  UINT i;
  for (i = 0; i < m_id.size(); i++)
    if (m_id[i] != id.m_id[i]) return false;

  return true;
}

bool TGroupId::operator<(const TGroupId &id) const {
  assert(!m_id.empty() && !id.m_id.empty());
  int size1 = m_id.size();
  int size2 = id.m_id.size();
  int i;
  for (i = 0; i < std::min(size1, size2); i++)
    if (m_id[size1 - i - 1] != id.m_id[size2 - i - 1])
      return m_id[size1 - i - 1] < id.m_id[size2 - i - 1];

  return size1 < size2;
}

//-------------------------------------------------------------------

int TGroupId::isGrouped(bool implicit) const {
  assert(!m_id.empty());
  assert(m_id[0] != 0);
  if (implicit)
    return (m_id[0] < 0) ? 1 : 0;
  else
    return (m_id[0] > 0) ? m_id.size() : 0;
}

//-------------------------------------------------------------------

TGroupId::TGroupId(TVectorImage *vi, bool isGhost) {
  m_id.push_back((isGhost) ? -(++vi->m_imp->m_maxGhostGroupId)
                           : ++vi->m_imp->m_maxGroupId);
}

#ifdef _DEBUG
void TVectorImage::Imp::checkGroups() {
  TGroupId currGroupId;
  std::set<TGroupId> groupSet;
  std::set<TGroupId>::iterator it;
  UINT i = 0;

  while (i < m_strokes.size()) {
    // assert(m_strokes[i]->m_groupId!=currGroupId);
    // assert(i==0 ||
    // m_strokes[i-1]->m_groupId.isGrouped()!=m_strokes[i]->m_groupId.isGrouped()!=0
    // ||
    //       (m_strokes[i]->m_groupId.isGrouped()!=0 &&
    //       m_strokes[i-1]->m_groupId!=m_strokes[i]->m_groupId));

    currGroupId = m_strokes[i]->m_groupId;
    it          = groupSet.find(currGroupId);
    if (it != groupSet.end())  // esisteva gia un gruppo con questo id!
      assert(!"errore: due gruppi con lo stesso id!");
    else
      groupSet.insert(currGroupId);

    while (i < m_strokes.size() && m_strokes[i]->m_groupId == currGroupId) i++;
  }
}
#endif

//-------------------------------------------------------------------

bool TVectorImage::canMoveStrokes(int strokeIndex, int count,
                                  int moveBefore) const {
  return m_imp->canMoveStrokes(strokeIndex, count, moveBefore);
}

//-------------------------------------------------------------------

// verifica se si possono spostare le stroke  da strokeindex a
// strokeindex+count-1 prima della posizione moveBefore;
// per fare questo fa un vettore in cui mette tutti i gruppi nella  posizione
// dopo lo
// spostamento e verifica che sia un configurazione di gruppi ammessa.

bool TVectorImage::Imp::canMoveStrokes(int strokeIndex, int count,
                                       int moveBefore) const {
  if (m_maxGroupId <= 1)  // non ci sono gruppi!
    return true;

  int i, j = 0;

  std::vector<TGroupId> groupsAfterMoving(m_strokes.size());
  if (strokeIndex < moveBefore) {
    for (i                   = 0; i < strokeIndex; i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;

    for (i                   = strokeIndex + count; i < moveBefore; i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;

    for (i                   = strokeIndex; i < strokeIndex + count; i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;

    for (i                   = moveBefore; i < (int)m_strokes.size(); i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;
  } else {
    for (i                   = 0; i < moveBefore; i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;

    for (i                   = strokeIndex; i < strokeIndex + count; i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;

    for (i                   = moveBefore; i < strokeIndex; i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;

    for (i = strokeIndex + count; i < (int)m_strokes.size(); i++)
      groupsAfterMoving[j++] = m_strokes[i]->m_groupId;
  }

  assert(j == (int)m_strokes.size());

  i = 0;
  TGroupId currGroupId;
  std::set<TGroupId> groupSet;

  while (i < (int)groupsAfterMoving.size()) {
    currGroupId = groupsAfterMoving[i];
    if (groupSet.find(currGroupId) !=
        groupSet.end())  // esisteva gia un gruppo con questo id!
    {
      if (!currGroupId.isGrouped(true))  // i gruppi impliciti non contano
        return false;
    } else
      groupSet.insert(currGroupId);

    while (i < (int)groupsAfterMoving.size() &&
           groupsAfterMoving[i] == currGroupId)
      i++;
  }

  return true;
}

//-----------------------------------------------------------------

void TVectorImage::Imp::regroupGhosts(std::vector<int> &changedStrokes) {
  TGroupId currGroupId;
  std::set<TGroupId> groupMap;
  std::set<TGroupId>::iterator it;
  UINT i = 0;

  while (i < m_strokes.size()) {
    assert(m_strokes[i]->m_groupId != currGroupId);
    assert(i == 0 ||
           m_strokes[i - 1]->m_groupId.isGrouped() !=
               m_strokes[i]->m_groupId.isGrouped() != 0 ||
           (m_strokes[i]->m_groupId.isGrouped() != 0 &&
            m_strokes[i - 1]->m_groupId != m_strokes[i]->m_groupId));

    currGroupId = m_strokes[i]->m_groupId;
    it          = groupMap.find(currGroupId);
    if (it != groupMap.end())  // esisteva gia un gruppo con questo id!
    {
      if (currGroupId.isGrouped() != 0)
        assert(!"errore: due gruppi con lo stesso id!");
      else  // gruppo ghost; gli do un nuovo id
      {
        TGroupId newGroup(m_vi, true);

        while (i < m_strokes.size() &&
               m_strokes[i]->m_groupId.isGrouped(true) != 0) {
          m_strokes[i]->m_groupId = newGroup;
          changedStrokes.push_back(i);
          i++;
        }
      }
    } else {
      groupMap.insert(currGroupId);
      while (i < m_strokes.size() &&
             ((currGroupId.isGrouped(false) != 0 &&
               m_strokes[i]->m_groupId == currGroupId) ||
              currGroupId.isGrouped(true) != 0 &&
                  m_strokes[i]->m_groupId.isGrouped(true) != 0)) {
        if (m_strokes[i]->m_groupId != currGroupId) {
          m_strokes[i]->m_groupId = currGroupId;
          changedStrokes.push_back(i);
        }
        i++;
      }
    }
  }
}

//--------------------------------------------------------------

bool TVectorImage::canEnterGroup(int strokeIndex) const {
  VIStroke *vs = m_imp->m_strokes[strokeIndex];

  if (!vs->m_groupId.isGrouped()) return false;

  return m_imp->m_insideGroup == TGroupId() ||
         vs->m_groupId != m_imp->m_insideGroup;
}

//--------------------------------------------------------------

bool TVectorImage::inCurrentGroup(int strokeIndex) const {
  return m_imp->inCurrentGroup(strokeIndex);
}

//----------------------------------------------------------------------------------

bool TVectorImage::Imp::inCurrentGroup(int strokeIndex) const {
  return m_insideGroup == TGroupId() ||
         m_insideGroup.isParentOf(m_strokes[strokeIndex]->m_groupId);
}

//--------------------------------------------------------------------------------------------------

bool TVectorImage::selectable(int strokeIndex) const {
  return (m_imp->m_insideGroup != m_imp->m_strokes[strokeIndex]->m_groupId &&
          inCurrentGroup(strokeIndex));
}

//--------------------------------------------------------------------------------------------------
namespace {

bool containsNoSubregion(const TRegion *r, const TPointD &p) {
  if (r->contains(p)) {
    for (unsigned int i = 0; i < r->getSubregionCount(); i++)
      if (r->getSubregion(i)->contains(p)) return false;
    return true;
  } else
    return false;
}
};

//------------------------------------------------------

int TVectorImage::getGroupByStroke(UINT index) const {
  VIStroke *viStroke = getVIStroke(index);
  return viStroke->m_groupId.m_id.back();
}

//------------------------------------------------------

int TVectorImage::getGroupByRegion(UINT index) const {
  TRegion *r = m_imp->m_regions[index];
  for (UINT i = 0; i < r->getEdgeCount(); i++)
    if (r->getEdge(i)->m_index >= 0) {
      return m_imp->m_strokes[r->getEdge(i)->m_index]->m_groupId.m_id.back();
    }

  return -1;
}

//------------------------------------------------------

int TVectorImage::pickGroup(const TPointD &pos, bool onEnteredGroup) const {
  if (onEnteredGroup && isInsideGroup() == 0) return -1;

  // double maxDist2 = 50*tglGetPixelSize2();

  int strokeIndex = getStrokeCount() - 1;

  while (strokeIndex >=
         0)  // ogni ciclo di while esplora un gruppo; ciclo sugli stroke
  {
    if (!isStrokeGrouped(strokeIndex)) {
      strokeIndex--;
      continue;
    }

    bool entered = isInsideGroup() > 0 && isEnteredGroupStroke(strokeIndex);

    if ((onEnteredGroup || entered) && (!onEnteredGroup || !entered)) {
      strokeIndex--;
      continue;
    }

    int currStrokeIndex = strokeIndex;

    while (strokeIndex >= 0 &&
           getCommonGroupDepth(strokeIndex, currStrokeIndex) > 0) {
      TStroke *s = getStroke(strokeIndex);
      double outT;
      int chunkIndex;
      double dist2;
      bool ret = s->getNearestChunk(pos, outT, chunkIndex, dist2);
      if (ret) {
        TThickPoint p = s->getChunk(chunkIndex)->getThickPoint(outT);
        if (p.thick < 0.1) p.thick = 1;
        if (sqrt(dist2) <= 1.5 * p.thick) return strokeIndex;
      }

      /*TThickPoint p = s->getThickPoint(s->getW(pos));

double dist = tdistance( TThickPoint(pos,0), p);
if (dist<1.2*p.thick/2.0)
return strokeIndex;*/
      strokeIndex--;
    }
  }

  strokeIndex = getStrokeCount() - 1;
  int ret     = -1;

  while (strokeIndex >=
         0)  // ogni ciclo di while esplora un gruppo; ciclo sulle regions
  {
    if (!isStrokeGrouped(strokeIndex)) {
      strokeIndex--;
      continue;
    }

    bool entered = isInsideGroup() > 0 && isEnteredGroupStroke(strokeIndex);

    if ((onEnteredGroup || entered) && (!onEnteredGroup || !entered)) {
      strokeIndex--;
      continue;
    }

    TRegion *currR = 0;
    for (UINT regionIndex = 0; regionIndex < getRegionCount(); regionIndex++) {
      TRegion *r = getRegion(regionIndex);

      int i, regionStrokeIndex = -1;
      for (i = 0; i < (int)r->getEdgeCount() && regionStrokeIndex < 0; i++)
        regionStrokeIndex = r->getEdge(i)->m_index;

      if (regionStrokeIndex >= 0 &&
          sameSubGroup(regionStrokeIndex, strokeIndex) &&
          containsNoSubregion(r, pos)) {
        if (!currR || currR->contains(*r)) {
          currR = r;
          ret   = regionStrokeIndex;
        }
      }
    }
    if (currR != 0) {
      assert(m_palette);
      const TSolidColorStyle *st = dynamic_cast<const TSolidColorStyle *>(
          m_palette->getStyle(currR->getStyle()));
      if (!st || st->getMainColor() != TPixel::Transparent) return ret;
    }

    while (strokeIndex > 0 &&
           getCommonGroupDepth(strokeIndex, strokeIndex - 1) > 0)
      strokeIndex--;
    strokeIndex--;
  }

  return -1;
}

//------------------------------------------------------------------------------------

int TVectorImage::pickGroup(const TPointD &pos) const {
  int index;
  if ((index = pickGroup(pos, true)) == -1) return pickGroup(pos, false);

  return index;
}

//--------------------------------------------------------------------------------------------------
