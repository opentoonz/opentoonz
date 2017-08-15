

//#include "tsystem.h"
#include "tmachine.h"
#include "tcurves.h"
#include "tcommon.h"
#include "tregion.h"
//#include "tregionutil.h"
#include "tstopwatch.h"

#include "tstroke.h"
#include "tstrokeutil.h"
#include "tvectorimageP.h"
#include "tdebugmessage.h"
#include "tthreadmessage.h"
#include "tl2lautocloser.h"
#include "tcomputeregions.h"
#include <vector>

#include "tcurveutil.h"

#include <algorithm>

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

//-----------------------------------------------------------------------------

#ifdef IS_DOTNET
#define NULL_ITER list<IntersectedStroke>::iterator()
#else
#define NULL_ITER 0
#endif

    using namespace std;

typedef TVectorImage::IntersectionBranch IntersectionBranch;
//-----------------------------------------------------------------------------

inline double myRound(double x) {
  return (1.0 / REGION_COMPUTING_PRECISION) *
         ((TINT32)(x * REGION_COMPUTING_PRECISION));
}

inline TThickPoint myRound(const TThickPoint &p) {
  return TThickPoint(myRound(p.x), myRound(p.y), p.thick);
}

static void roundStroke(TStroke *s) {
  int size = s->getControlPointCount();

  for (int j = 0; j < (int)s->getControlPointCount(); j++) {
    TThickPoint p = s->getControlPoint(j);
    s->setControlPoint(j, myRound(p));
  }
  if (size > 3)
  //! it can happen that a stroke has a first or last quadratic degenerated:(3
  //! equal control points).
  // in that case, if the stroke has an intersection in an endpoint, the
  // resulting w could not be  0 or 1 as expected.
  // since the w==0 and w==1 are used in the region computing to determine if
  // the intersection is an endpoint,
  //

  {
    if (s->getControlPoint(0) == s->getControlPoint(1) &&
        s->getControlPoint(0) == s->getControlPoint(2)) {
      s->setControlPoint(2, s->getControlPoint(3));
      s->setControlPoint(1, s->getControlPoint(3));
    }
    if (s->getControlPoint(size - 1) == s->getControlPoint(size - 2) &&
        s->getControlPoint(size - 1) == s->getControlPoint(size - 3)) {
      s->setControlPoint(size - 2, s->getControlPoint(size - 4));
      s->setControlPoint(size - 3, s->getControlPoint(size - 4));
    }
  }
}

//-----------------------------------------------------------------------------
class VIListElem {
public:
  VIListElem *m_prev;
  VIListElem *m_next;

  VIListElem() : m_prev(0), m_next(0) {}
  inline VIListElem *next() { return m_next; }
  inline VIListElem *prev() { return m_prev; }
};

template <class T>
class VIList {
  int m_size;
  T *m_begin, *m_end;

public:
  VIList() : m_begin(0), m_end(0), m_size(0) {}

  inline T *first() const { return m_begin; };
  inline T *last() const { return m_end; };

  void clear();
  void pushBack(T *elem);
  void insert(T *before, T *elem);
  T *erase(T *element);
  T *getElemAt(int pos);
  int getPosOf(T *elem);
  inline int size() { return m_size; }
  inline bool empty() { return size() == 0; }
};

class Intersection final : public VIListElem {
public:
  // Intersection* m_prev, *m_next;
  TPointD m_intersection;
  int m_numInter;
  // bool m_isNotErasable;
  VIList<IntersectedStroke> m_strokeList;

  Intersection() : m_numInter(0), m_strokeList() {}

  inline Intersection *next() { return (Intersection *)VIListElem::next(); };
  inline Intersection *prev() { return (Intersection *)VIListElem::prev(); };
  // inline Intersection* operator++() {return next();}
};

class IntersectedStrokeEdges {
public:
  int m_index;
  list<TEdge *> m_edgeList;
  IntersectedStrokeEdges(int index) : m_index(index), m_edgeList() {}
  ~IntersectedStrokeEdges() {
    assert(m_index >= 0); /*clearPointerContainer(m_edgeList);*/
    m_edgeList.clear();
    m_index = -1;
  }
};

class IntersectionData {
public:
  UINT maxAutocloseId;
  VIList<Intersection> m_intList;
  map<int, VIStroke *> m_autocloseMap;
  vector<IntersectedStrokeEdges> m_intersectedStrokeArray;

  IntersectionData() : maxAutocloseId(1), m_intList() {}

  ~IntersectionData();
};

//-----------------------------------------------------------------------------

class IntersectedStroke final : public VIListElem {
  /*double m_w;
TStroke *m_s;
UINT m_index;*/
  // IntersectedStroke* m_prev, *m_next;
public:
  TEdge m_edge;
  Intersection *m_nextIntersection;
  IntersectedStroke *m_nextStroke;
  bool m_visited, m_gettingOut;  //, m_dead;

  IntersectedStroke()
      : m_visited(false), m_nextIntersection(0), m_nextStroke(0){};

  IntersectedStroke(Intersection *nextIntersection,
                    IntersectedStroke *nextStroke)
      /*: m_w(-1.0)
, m_s(NULL)
, m_index(0)*/
      : m_edge(),
        m_nextIntersection(nextIntersection),
        m_nextStroke(nextStroke),
        m_visited(false)
  //, m_dead(false)
  {}

  IntersectedStroke(const IntersectedStroke &s)
      : m_edge(s.m_edge, false)
      , m_nextIntersection(s.m_nextIntersection)
      , m_nextStroke(s.m_nextStroke)
      , m_visited(s.m_visited)
      , m_gettingOut(s.m_gettingOut)
  //, m_dead(s.m_dead)
  {}

  inline IntersectedStroke *next() {
    return (IntersectedStroke *)VIListElem::next();
  };
};

//=============================================================================

template <class T>
void VIList<T>::clear() {
  while (m_begin) {
    T *aux  = m_begin;
    m_begin = m_begin->next();
    delete aux;
  }
  m_end  = 0;
  m_size = 0;
}

template <class T>
void VIList<T>::pushBack(T *elem) {
  if (!m_begin) {
    assert(!m_end);
    elem->m_next = elem->m_prev = 0;
    m_begin = m_end = elem;
  } else {
    assert(m_end);
    assert(m_end->m_next == 0);
    m_end->m_next = elem;
    elem->m_prev  = m_end;
    elem->m_next  = 0;
    m_end         = elem;
  }
  m_size++;
}

template <class T>
void VIList<T>::insert(T *before, T *elem) {
  assert(before && elem);

  elem->m_prev = before->m_prev;
  elem->m_next = before;

  if (!before->m_prev)
    before->m_prev = m_begin = elem;
  else {
    before->m_prev->m_next = elem;
    before->m_prev         = elem;
  }
  m_size++;
}

template <class T>
T *VIList<T>::erase(T *element) {
  T *ret;

  assert(m_size > 0);
  if (!element->m_prev) {
    assert(m_begin == element);
    if (!element->m_next)
      ret = m_begin = m_end = 0;
    else {
      m_begin         = (T *)m_begin->m_next;
      m_begin->m_prev = 0;
      ret             = m_begin;
    }
  } else if (!element->m_next) {
    assert(m_end == element);
    m_end         = (T *)m_end->m_prev;
    m_end->m_next = 0;
    ret           = 0;
  } else {
    element->m_prev->m_next = element->m_next;
    element->m_next->m_prev = element->m_prev;
    ret                     = (T *)element->m_next;
  }
  m_size--;
  delete element;
  return ret;
}

template <class T>
T *VIList<T>::getElemAt(int pos) {
  assert(pos < m_size);
  T *p            = m_begin;
  while (pos--) p = p->next();
  return p;
}

template <class T>
int VIList<T>::getPosOf(T *elem) {
  int count = 0;
  T *p      = m_begin;

  while (p && p != elem) {
    count++;
    p = p->next();
  }
  assert(p == elem);
  return count;
}

//-------------------------------------------------------------

//-----------------------------------------------------------------------------

#ifdef LEVO

void print(list<Intersection> &intersectionList, char *str) {
  ofstream of(str);

  of << "***************************" << endl;

  list<Intersection>::const_iterator it;
  list<IntersectedStroke>::const_iterator it1;
  int i, j;
  for (i = 0, it = intersectionList.begin(); it != intersectionList.end();
       it++, i++) {
    of << "***************************" << endl;
    of << "Intersection#" << i << ": " << it->m_intersection
       << "numBranches: " << it->m_numInter << endl;
    of << endl;

    for (j = 0, it1 = it->m_strokeList.begin(); it1 != it->m_strokeList.end();
         it1++, j++) {
      of << "----Branch #" << j;
      if (it1->m_edge.m_index < 0) of << "(AUTOCLOSE)";
      of << "Intersection at " << it1->m_edge.m_w0 << ": "
         << ": " << endl;
      of << "ColorId: " << it1->m_edge.m_styleId << endl;
      /*
TColorStyle* fs = it1->m_edge.m_fillStyle;
if (fs==0)
of<<"NO color: "<< endl;
else
{
TFillStyleP fp = fs->getFillStyle();
if (fp)
{
fp->
assert(false) ;
else
of<<"Color: ("<< colorStyle->getColor().r<<", "<< colorStyle->getColor().g<<",
"<< colorStyle->getColor().b<<")"<<endl;
*/

      of << "----Stroke " << (it1->m_gettingOut ? "OUT" : "IN") << " #"
         << it1->m_edge.m_index << ": " << endl;
      // if (it1->m_dead)
      // of<<"---- DEAD Intersection.";
      // else
      {
        of << "---- NEXT Intersection:";
        if (it1->m_nextIntersection != intersectionList.end()) {
          int dist =
              std::distance(intersectionList.begin(), it1->m_nextIntersection);
          of << dist;
          list<Intersection>::iterator iit = intersectionList.begin();
          std::advance(iit, dist);
          of << " "
             << std::distance(iit->m_strokeList.begin(), it1->m_nextStroke);
        }

        else
          of << "NULL!!";
        of << "---- NEXT Stroke:";
        if (it1->m_nextIntersection != intersectionList.end())
          of << it1->m_nextStroke->m_edge.m_index;
        else
          of << "NULL!!";
      }
      of << endl << endl;
    }
  }
}

#endif

void findNearestIntersection(list<Intersection> &interList,
                             const list<Intersection>::iterator &i1,
                             const list<IntersectedStroke>::iterator &i2);

//-----------------------------------------------------------------------------

#ifdef _TOLGO
void checkInterList(list<Intersection> &intersectionList) {
  list<Intersection>::iterator it;
  list<IntersectedStroke>::iterator it1;

  for (it = intersectionList.begin(); it != intersectionList.end(); it++) {
    int count = 0;
    for (it1 = it->m_strokeList.begin(); it1 != it->m_strokeList.end(); it1++) {
      int val;
      if (it1->m_nextIntersection != intersectionList.end()) {
        count++;
        // assert (it1->m_nextIntersection!=intersectionList.end());
        assert(it1->m_nextStroke->m_nextIntersection == it);
        assert(it1->m_nextStroke->m_nextStroke == it1);

        // int k = it1->m_edge.m_index;
        val = std::distance(intersectionList.begin(), it1->m_nextIntersection);
      }
      // else
      // assert(it1->m_nextIntersection==intersectionList.end());
    }
    assert(count == it->m_numInter);
  }
}
#else
#define checkInterList
#endif

//-----------------------------------------------------------------------------

// void addFakeIntersection(list<Intersection>& intersectionList,TStroke* s,
// UINT ii, double w);

void addIntersections(IntersectionData &intersectionData,
                      const vector<VIStroke *> &s, int ii, int jj,
                      const vector<DoublePair> &intersections, int numStrokes,
                      bool isVectorized);

void addIntersection(IntersectionData &intData, const vector<VIStroke *> &s,
                     int ii, int jj, DoublePair intersections, int strokeSize,
                     bool isVectorized);

//-----------------------------------------------------------------------------

static bool sortBBox(const TStroke *s1, const TStroke *s2) {
  return s1->getBBox().x0 < s2->getBBox().x0;
}

//-----------------------------------------------------------------------------

static void cleanIntersectionMarks(const VIList<Intersection> &interList) {
  Intersection *p;
  IntersectedStroke *q;
  for (p = interList.first(); p; p = p->next())
    for (q = p->m_strokeList.first(); q; q = q->next()) {
      q->m_visited =
          false;  // Ogni ramo della lista viene messo nella condizione
                  // di poter essere visitato

      if (q->m_nextIntersection) {
        q->m_nextIntersection = 0;
        p->m_numInter--;
      }
    }
}

//-----------------------------------------------------------------------------

static void cleanNextIntersection(const VIList<Intersection> &interList,
                                  TStroke *s) {
  Intersection *p;
  IntersectedStroke *q;

  for (p = interList.first(); p; p = p->next())
    for (q = p->m_strokeList.first(); q; q = q->next())
      if (q->m_edge.m_s == s) {
        // if (it2->m_nextIntersection==NULL)
        //  return; //gia' ripulita prima
        if (q->m_nextIntersection) {
          q->m_nextIntersection = 0;
          p->m_numInter--;
        }
        q->m_nextStroke = 0;
      }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::eraseEdgeFromStroke(IntersectedStroke *is) {
  if (is->m_edge.m_index >=
      0)  // elimino il puntatore all'edge nella lista della VIStroke
  {
    VIStroke *s;
    s = m_strokes[is->m_edge.m_index];
    assert(s->m_s == is->m_edge.m_s);
    list<TEdge *>::iterator iit  = s->m_edgeList.begin(),
                            it_e = s->m_edgeList.end();

    for (; iit != it_e; ++iit)
      if ((*iit)->m_w0 == is->m_edge.m_w0 && (*iit)->m_w1 == is->m_edge.m_w1) {
        assert((*iit)->m_toBeDeleted == false);
        s->m_edgeList.erase(iit);
        return;
      }
  }
}

//-----------------------------------------------------------------------------

IntersectedStroke *TVectorImage::Imp::eraseBranch(Intersection *in,
                                                  IntersectedStroke *is) {
  if (is->m_nextIntersection) {
    Intersection *nextInt         = is->m_nextIntersection;
    IntersectedStroke *nextStroke = is->m_nextStroke;
    assert(nextStroke->m_nextIntersection == in);
    assert(nextStroke->m_nextStroke == is);
    assert(nextStroke != is);

    // nextStroke->m_nextIntersection = intList.end();
    // nextStroke->m_nextStroke = nextInt->m_strokeList.end();

    if (nextStroke->m_nextIntersection) {
      nextStroke->m_nextIntersection = 0;
      nextInt->m_numInter--;
    }
    // nextInt->m_strokeList.erase(nextStroke);//non posso cancellarla, puo'
    // servire in futuro!
  }
  if (is->m_nextIntersection) in->m_numInter--;

  eraseEdgeFromStroke(is);

  is->m_edge.m_w0 = is->m_edge.m_w1 = -3;
  is->m_edge.m_index                = -3;
  is->m_edge.m_s                    = 0;
  is->m_edge.m_styleId              = -3;

  return in->m_strokeList.erase(is);
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::eraseDeadIntersections() {
  Intersection *p = m_intersectionData->m_intList.first();

  while (p)  // la faccio qui, e non nella eraseIntersection. vedi commento li'.
  {
    // Intersection* &intList = m_intersectionData->m_intList;

    if (p->m_strokeList.size() == 1) {
      eraseBranch(p, p->m_strokeList.first());
      assert(p->m_strokeList.size() == 0);
      p = m_intersectionData->m_intList.erase(p);
    } else if (p->m_strokeList.size() == 2 &&
               (p->m_strokeList.first()->m_edge.m_s ==
                    p->m_strokeList.last()->m_edge.m_s &&
                p->m_strokeList.first()->m_edge.m_w0 ==
                    p->m_strokeList.last()->m_edge.m_w0))  // intersezione finta
    {
      IntersectedStroke *it1 = p->m_strokeList.first(), *iit1, *iit2;
      IntersectedStroke *it2 = it1->next();

      eraseEdgeFromStroke(p->m_strokeList.first());
      eraseEdgeFromStroke(p->m_strokeList.first()->next());

      iit1 = (it1->m_nextIntersection) ? it1->m_nextStroke : 0;
      iit2 = (it2->m_nextIntersection) ? it2->m_nextStroke : 0;

      if (iit1 && iit2) {
        iit1->m_edge.m_w1 = iit2->m_edge.m_w0;
        iit2->m_edge.m_w1 = iit1->m_edge.m_w0;
      }
      if (iit1) {
        iit1->m_nextStroke       = iit2;
        iit1->m_nextIntersection = it2->m_nextIntersection;
        if (!iit1->m_nextIntersection) it1->m_nextIntersection->m_numInter--;
      }

      if (iit2) {
        iit2->m_nextStroke       = iit1;
        iit2->m_nextIntersection = it1->m_nextIntersection;
        if (!iit2->m_nextIntersection) it2->m_nextIntersection->m_numInter--;
      }

      p->m_strokeList.clear();
      p->m_numInter = 0;
      p             = m_intersectionData->m_intList.erase(p);
    } else
      p = p->next();
  }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::doEraseIntersection(int index,
                                            vector<int> *toBeDeleted) {
  Intersection *p1  = m_intersectionData->m_intList.first();
  TStroke *deleteIt = 0;

  while (p1) {
    bool removeAutocloses = false;

    IntersectedStroke *p2 = p1->m_strokeList.first();

    while (p2) {
      IntersectedStroke &is = *p2;

      if (is.m_edge.m_index == index) {
        if (is.m_edge.m_index >= 0)
          // if (!is.m_autoclose && (is.m_edge.m_w0==1 || is.m_edge.m_w0==0))
          removeAutocloses = true;
        else
          deleteIt = is.m_edge.m_s;
        p2         = eraseBranch(p1, p2);
      } else
        p2 = p2->next();
      // checkInterList(interList);
    }
    if (removeAutocloses)  // se ho tolto una stroke dall'inter corrente, tolgo
                           // tutti le stroke di autclose che partono da qui
    {
      assert(toBeDeleted);
      for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next())
        if (p2->m_edge.m_index < 0 &&
            (p2->m_edge.m_w0 == 1 || p2->m_edge.m_w0 == 0))
          toBeDeleted->push_back(p2->m_edge.m_index);
    }

    if (p1->m_strokeList.empty())
      p1 = m_intersectionData->m_intList.erase(p1);
    else
      p1 = p1->next();
  }

  if (deleteIt) delete deleteIt;
}

//-----------------------------------------------------------------------------

UINT TVectorImage::Imp::getFillData(std::unique_ptr<IntersectionBranch[]> &v) {
  // print(m_intersectionData->m_intList,
  // "C:\\temp\\intersectionPrimaSave.txt");

  // Intersection* intList = m_intersectionData->m_intList;
  if (m_intersectionData->m_intList.empty()) return 0;

  Intersection *p1;
  IntersectedStroke *p2;
  UINT currInt = 0;
  vector<UINT> branchesBefore(m_intersectionData->m_intList.size() + 1);

  branchesBefore[0] = 0;
  UINT count = 0, size = 0;

  p1 = m_intersectionData->m_intList.first();

  for (; p1; p1 = p1->next(), currInt++) {
    UINT strokeListSize = p1->m_strokeList.size();
    size += strokeListSize;
    branchesBefore[currInt + 1] = branchesBefore[currInt] + strokeListSize;
  }

  v.reset(new IntersectionBranch[size]);
  currInt = 0;
  p1      = m_intersectionData->m_intList.first();
  for (; p1; p1 = p1->next(), currInt++) {
    UINT currBranch = 0;
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next(), currBranch++) {
      IntersectionBranch &b = v[count];
      b.m_currInter         = currInt;
      b.m_strokeIndex       = p2->m_edge.m_index;
      b.m_w                 = p2->m_edge.m_w0;
      b.m_style             = p2->m_edge.m_styleId;
      // assert(b.m_style<100);
      b.m_gettingOut = p2->m_gettingOut;
      if (!p2->m_nextIntersection)
        b.m_nextBranch = count;
      else {
        UINT distInt =
            m_intersectionData->m_intList.getPosOf(p2->m_nextIntersection);
        UINT distBranch =
            p2->m_nextIntersection->m_strokeList.getPosOf(p2->m_nextStroke);

        if ((distInt < currInt) ||
            (distInt == currInt && distBranch < currBranch)) {
          UINT posNext = branchesBefore[distInt] + distBranch;
          assert(posNext < count);
          b.m_nextBranch = posNext;
          assert(v[posNext].m_nextBranch == (std::numeric_limits<UINT>::max)());
          v[posNext].m_nextBranch = count;
        } else
          b.m_nextBranch = (std::numeric_limits<UINT>::max)();
      }
      count++;
    }
  }

// for (UINT i=0; i<count; i++)
//  assert(v[i].m_nextBranch != std::numeric_limits<UINT>::max());

#ifdef _DEBUG
/*ofstream of("C:\\temp\\fillDataOut.txt");

for (UINT ii=0; ii<size; ii++)
  {
  of<<ii<<"----------------------"<<endl;
  of<<"index:"<<v[ii].m_strokeIndex<<endl;
  of<<"w:"<<v[ii].m_w<<endl;
  of<<"curr inter:"<<v[ii].m_currInter<<endl;
  of<<"next inter:"<<v[ii].m_nextBranch<<endl;
  of<<"gettingOut:"<<((v[ii].m_gettingOut)?"TRUE":"FALSE")<<endl;
  of<<"colorId:"<<v[ii].m_style<<endl;
  }*/
#endif

  return size;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
namespace {
TStroke *reconstructAutocloseStroke(Intersection *p1, IntersectedStroke *p2)

{
  bool found        = false;
  Intersection *pp1 = p1->next();
  IntersectedStroke *pp2;

  // vector<TEdge*> vapp;
  for (; !found && pp1; pp1 = pp1->next()) {
    for (pp2 = pp1->m_strokeList.first(); !found && pp2; pp2 = pp2->next()) {
      if (p2->m_edge.m_index == pp2->m_edge.m_index) {
        if ((pp2->m_edge.m_w0 == 1 && p2->m_edge.m_w0 == 0) ||
            (pp2->m_edge.m_w0 == 0 && p2->m_edge.m_w0 == 1)) {
          found = true;
          vector<TPointD> v;
          if (p2->m_edge.m_w0 == 0) {
            v.push_back(p1->m_intersection);
            v.push_back(pp1->m_intersection);
          } else {
            v.push_back(pp1->m_intersection);
            v.push_back(p1->m_intersection);
          }
          p2->m_edge.m_s = pp2->m_edge.m_s = new TStroke(v);
          // for (UINT ii=0; ii<vapp.size(); ii++)
          // vapp[ii]->m_s = it2->m_edge.m_s;
        }
        // else if (iit2->m_edge.m_w0!=0 && iit2->m_edge.m_w0!=1)
        //  vapp.push_back(&(iit2->m_edge));
      }
    }
  }
  assert(found);
  if (!found) p2->m_edge.m_s = 0;

  return p2->m_edge.m_s;
}

}  // namespace
//-----------------------------------------------------------------------------

void TVectorImage::Imp::setFillData(
    std::unique_ptr<IntersectionBranch[]> const &v, UINT branchCount,
    bool doComputeRegions) {
#ifdef _DEBUG
/*ofstream of("C:\\temp\\fillDataIn.txt");

for (UINT ii=0; ii<branchCount; ii++)
  {
  of<<ii<<"----------------------"<<endl;
  of<<"index:"<<v[ii].m_strokeIndex<<endl;
  of<<"w:"<<v[ii].m_w<<endl;
  of<<"curr inter:"<<v[ii].m_currInter<<endl;
  of<<"next inter:"<<v[ii].m_nextBranch<<endl;
  of<<"gettingOut:"<<((v[ii].m_gettingOut)?"TRUE":"FALSE")<<endl;
  of<<"colorId:"<<v[ii].m_style<<endl;
  }*/
#endif

  if (branchCount == 0) return;

  //{
  // QMutexLocker sl(m_mutex);

  VIList<Intersection> &intList = m_intersectionData->m_intList;

  clearPointerContainer(m_regions);
  m_regions.clear();
  intList.clear();
  Intersection *currInt;
  IntersectedStroke *currBranch;

  UINT size = v[branchCount - 1].m_currInter + 1;
  vector<UINT> branchesBefore(size);

  UINT i = 0;
  for (; i < branchCount; i++) {
    const IntersectionBranch &b = v[i];
    if (i == 0 || v[i].m_currInter != v[i - 1].m_currInter) {
      if (v[i].m_currInter >=
          size)  // pezza per immagine corrotte...evito crash
      {
        intList.clear();
        return;
      }

      branchesBefore[v[i].m_currInter] = i;

      currInt = new Intersection();
      intList.pushBack(currInt);
    }

    currBranch = new IntersectedStroke();
    currInt->m_strokeList.pushBack(currBranch);

    currBranch->m_edge.m_styleId = b.m_style;
    // assert(b.m_style<100);
    currBranch->m_edge.m_index = b.m_strokeIndex;
    if (b.m_strokeIndex >= 0)
      currBranch->m_edge.m_s = m_strokes[b.m_strokeIndex]->m_s;
    else
      currBranch->m_edge.m_s = 0;
    currBranch->m_gettingOut = b.m_gettingOut;
    currBranch->m_edge.m_w0  = b.m_w;
    if (b.m_nextBranch < branchCount)
      currBranch->m_edge.m_w1 = v[b.m_nextBranch].m_w;
    else
      currBranch->m_edge.m_w1 = 1.0;
    assert(currBranch->m_edge.m_w0 >= -1e-8 &&
           currBranch->m_edge.m_w0 <= 1 + 1e-8);
    assert(currBranch->m_edge.m_w1 >= -1e-8 &&
           currBranch->m_edge.m_w1 <= 1 + 1e-8);

    if (b.m_nextBranch < i) {
      Intersection *p1;
      IntersectedStroke *p2;
      p1 = intList.getElemAt(v[b.m_nextBranch].m_currInter);

      assert(b.m_nextBranch - branchesBefore[v[b.m_nextBranch].m_currInter] >=
             0);
      p2 = p1->m_strokeList.getElemAt(
          b.m_nextBranch - branchesBefore[v[b.m_nextBranch].m_currInter]);

      currBranch->m_nextIntersection = p1;
      currBranch->m_nextStroke       = p2;
      p2->m_nextIntersection         = currInt;
      p2->m_nextStroke               = currBranch;

      // if (currBranch == currInt->m_strokeList.begin())
      //  currInt->m_intersection =
      //  currBranch->m_edge.m_s->getPoint(currBranch->m_edge.m_w0);

      currInt->m_numInter++;
      p1->m_numInter++;
    } else if (b.m_nextBranch == i)
      currBranch->m_nextIntersection = 0;
    else if (b.m_nextBranch == (std::numeric_limits<UINT>::max)()) {
      currBranch->m_nextIntersection = 0;
      currBranch->m_nextStroke       = 0;
    }

    /* {
assert(b.m_nextBranch<branchCount);
assert(v[b.m_nextBranch].m_nextBranch==i);
}*/

    if (i == branchCount - 1 || v[i].m_currInter != v[i + 1].m_currInter) {
      int j = i;
      while (v[j].m_strokeIndex < 0 &&
             ((j > 0 && v[j].m_currInter == v[j - 1].m_currInter) || j == 0))
        j--;
      if (v[j].m_strokeIndex >= 0)
        currInt->m_intersection =
            m_strokes[v[j].m_strokeIndex]->m_s->getPoint(v[j].m_w);
    }
  }

  for (i = 0; i < m_strokes.size(); i++) m_strokes[i]->m_isNewForFill = false;

  // computeRegions();

  Intersection *p1;
  IntersectedStroke *p2;

  vector<UINT> toBeDeleted;

  for (p1 = intList.first(); p1; p1 = p1->next())
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) {
      if (p2->m_edge.m_index < 0 && !p2->m_edge.m_s &&
          (p2->m_edge.m_w0 == 0 || p2->m_edge.m_w0 == 1)) {
        p2->m_edge.m_s = reconstructAutocloseStroke(p1, p2);
        if (p2->m_edge.m_s)
          m_intersectionData->m_autocloseMap[p2->m_edge.m_index] =
              new VIStroke(p2->m_edge.m_s, TGroupId());
        else
          toBeDeleted.push_back(p2->m_edge.m_index);
      }
    }

  for (p1 = intList.first(); p1; p1 = p1->next())
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) {
      if (!p2->m_edge.m_s && p2->m_edge.m_index < 0) {
        VIStroke *vs = m_intersectionData->m_autocloseMap[p2->m_edge.m_index];
        if (vs) {
          p2->m_edge.m_s =
              m_intersectionData->m_autocloseMap[p2->m_edge.m_index]->m_s;

          // TEdge& e = it2->m_edge;
          if (!p2->m_edge.m_s) toBeDeleted.push_back(p2->m_edge.m_index);
        }
      }
    }

  for (i = 0; i < toBeDeleted.size(); i++) eraseIntersection(toBeDeleted[i]);

  m_areValidRegions = false;
  //}

  if (doComputeRegions) computeRegions();
  // print(m_intersectionData->m_intList, "C:\\temp\\intersectionDopoLoad.txt");
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::eraseIntersection(int index) {
  vector<int> autocloseStrokes;
  doEraseIntersection(index, &autocloseStrokes);

  for (UINT i = 0; i < autocloseStrokes.size(); i++) {
    doEraseIntersection(autocloseStrokes[i]);
    assert(autocloseStrokes[i] < 0);
    m_intersectionData->m_autocloseMap.erase(autocloseStrokes[i]);
  }
}
//-----------------------------------------------------------------------------

static void findNearestIntersection(VIList<Intersection> &interList) {
  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = interList.first(); p1; p1 = p1->next()) {
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) {
      if (p2->m_nextIntersection)  // already set
        continue;

      int versus      = (p2->m_gettingOut) ? 1 : -1;
      double minDelta = (std::numeric_limits<double>::max)();
      Intersection *pp1, *p1Res;
      IntersectedStroke *pp2, *p2Res;

      for (pp1 = p1; pp1; pp1 = pp1->next()) {
        if (pp1 == p1)
          pp2 = p2, pp2 = pp2->next();
        else
          pp2 = pp1->m_strokeList.first();

        for (; pp2; pp2 = pp2->next()) {
          if (pp2->m_edge.m_index == p2->m_edge.m_index &&
              pp2->m_gettingOut == !p2->m_gettingOut) {
            double delta = versus * (pp2->m_edge.m_w0 - p2->m_edge.m_w0);

            if (delta > 0 && delta < minDelta) {
              p1Res    = pp1;
              p2Res    = pp2;
              minDelta = delta;
            }
          }
        }
      }

      if (minDelta != (std::numeric_limits<double>::max)()) {
        p2Res->m_nextIntersection = p1;
        p2Res->m_nextStroke       = p2;
        p2Res->m_edge.m_w1        = p2->m_edge.m_w0;
        p2->m_nextIntersection    = p1Res;
        p2->m_nextStroke          = p2Res;
        p2->m_edge.m_w1           = p2Res->m_edge.m_w0;
        p1->m_numInter++;
        p1Res->m_numInter++;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void markDeadIntersections(VIList<Intersection> &intList, Intersection *p);

// questa funzione "cuscinetto" serve perche crashava il compilatore in
// release!!!
void inline markDeadIntersectionsRic(VIList<Intersection> &intList,
                                     Intersection *p) {
  markDeadIntersections(intList, p);
}

//-----------------------------------------------------------------------------

void markDeadIntersections(VIList<Intersection> &intList, Intersection *p) {
  IntersectedStroke *p1 = p->m_strokeList.first();
  if (!p1) return;

  if (p->m_numInter == 1) {
    while (p1 && !!p1->m_nextIntersection) {
      p1 = p1->next();
    }
    // assert(p1);
    if (!p1) return;

    Intersection *nextInt         = p1->m_nextIntersection;
    IntersectedStroke *nextStroke = p1->m_nextStroke;

    p->m_numInter          = 0;
    p1->m_nextIntersection = 0;

    if (nextInt /*&& !nextStroke->m_dead*/) {
      nextInt->m_numInter--;
      nextStroke->m_nextIntersection = 0;
      markDeadIntersectionsRic(intList, nextInt);
    }
  } else if (p->m_numInter == 2)  // intersezione finta (forse)
  {
    while (p1 && !p1->m_nextIntersection) p1 = p1->next();
    assert(p1);
    if (!p1) return;
    IntersectedStroke *p2 = p1->next();

    while (p2 && !p2->m_nextIntersection) p2 = p2->next();

    assert(p2);
    if (!p2) return;

    if (p1->m_edge.m_s == p2->m_edge.m_s &&
        p1->m_edge.m_w0 == p2->m_edge.m_w0)  // intersezione finta
    {
      IntersectedStroke *pp1, *pp2;
      assert(p1->m_nextIntersection && p2->m_nextIntersection);

      pp1 = p1->m_nextStroke;
      pp2 = p2->m_nextStroke;

      pp2->m_edge.m_w1 = pp1->m_edge.m_w0;
      pp1->m_edge.m_w1 = pp2->m_edge.m_w0;

      // if (iit1!=0)
      pp1->m_nextStroke = pp2;
      // if (iit2!=0)
      pp2->m_nextStroke = pp1;
      // if (iit1!=0)
      pp1->m_nextIntersection = p2->m_nextIntersection;
      // if (iit2!=0)
      pp2->m_nextIntersection = p1->m_nextIntersection;

      p->m_numInter          = 0;
      p1->m_nextIntersection = p2->m_nextIntersection = 0;
    }
  }
}

//-----------------------------------------------------------------------------

// se cross val era 0, cerco di spostarmi un po' su w per vedere come sono
// orientate le tangenti agli stroke...
static double nearCrossVal(TStroke *s0, double w0, TStroke *s1, double w1) {
  double ltot0 = s0->getLength();
  double ltot1 = s1->getLength();
  double dl    = std::min(ltot1, ltot0) / 1000;

  double crossVal, dl0 = dl, dl1 = dl;

  TPointD p0, p1;
  int count = 0;

  if (w0 == 1.0) dl0 = -dl0;
  if (w1 == 1.0) dl1 = -dl1;

  double l0 = s0->getLength(w0) + dl0;
  double l1 = s1->getLength(w1) + dl1;

  do {
    p0       = s0->getSpeed(s0->getParameterAtLength(l0));
    p1       = s1->getSpeed(s1->getParameterAtLength(l1));
    crossVal = cross(p0, p1);
    l0 += dl0, l1 += dl1;
    count++;
  } while (areAlmostEqual(crossVal, 0.0) &&
           ((dl0 > 0 && l0 < ltot0) || (dl0 < 0 && l0 > 0)) &&
           ((dl1 > 0 && l1 < ltot1) || (dl1 < 0 && l1 > 0)));
  return crossVal;
}

//-----------------------------------------------------------------------------

inline void insertBranch(Intersection &in, IntersectedStroke &item,
                         bool gettingOut) {
  if (item.m_edge.m_w0 != (gettingOut ? 1.0 : 0.0)) {
    item.m_gettingOut = gettingOut;
    in.m_strokeList.pushBack(new IntersectedStroke(item));
  }
}

//-----------------------------------------------------------------------------

static double getAngle(const TPointD &p0, const TPointD &p1) {
  double angle1 = atan2(p0.x, p0.y) * M_180_PI;
  double angle2 = atan2(p1.x, p1.y) * M_180_PI;

  if (angle1 < 0) angle1 = 360 + angle1;
  if (angle2 < 0) angle2 = 360 + angle2;

  return (angle2 - angle1) < 0 ? 360 + angle2 - angle1 : angle2 - angle1;
}

//-----------------------------------------------------------------------------
// nel caso l'angolo tra due stroke in un certo w sia nullo,
// si va un po' avanti per vedere come sono orientate....
static double getNearAngle(const TStroke *s1, double w1, bool out1,
                           const TStroke *s2, double w2, bool out2) {
  bool verse1  = (out1 && w1 < 1) || (!out1 && w1 == 0);
  bool verse2  = (out2 && w2 < 1) || (!out2 && w2 == 0);
  double ltot1 = s1->getLength();
  double ltot2 = s2->getLength();
  double l1    = s1->getLength(w1);
  double l2    = s2->getLength(w2);
  double dl    = min(ltot1, ltot2) / 1000;
  double dl1   = verse1 ? dl : -dl;
  double dl2   = verse2 ? dl : -dl;

  while (((verse1 && l1 < ltot1) || (!verse1 && l1 > 0)) &&
         ((verse2 && l2 < ltot2) || (!verse2 && l2 > 0))) {
    l1 += dl1;
    l2 += dl2;
    TPointD p1   = (out1 ? 1 : -1) * s1->getSpeed(s1->getParameterAtLength(l1));
    TPointD p2   = (out2 ? 1 : -1) * s2->getSpeed(s2->getParameterAtLength(l2));
    double angle = getAngle(p1, p2);
    if (!areAlmostEqual(angle, 0, 1e-9)) return angle;
  }
  return 0;
}

//-----------------------------------------------------------------------------

static bool makeEdgeIntersection(Intersection &interList,
                                 IntersectedStroke &item1,
                                 IntersectedStroke &item2, const TPointD &p1a,
                                 const TPointD &p1b, const TPointD &p2a,
                                 const TPointD &p2b) {
  double angle1 = getAngle(p1a, p1b);
  double angle2 = getAngle(p1a, p2a);
  double angle3 = getAngle(p1a, p2b);
  double angle;

  bool eraseP1b = false, eraseP2a = false, eraseP2b = false;

  if (areAlmostEqual(angle1, 0, 1e-9)) {
    angle1 = getNearAngle(item1.m_edge.m_s, item1.m_edge.m_w0, true,
                          item1.m_edge.m_s, item1.m_edge.m_w0, false);
    if (areAlmostEqual(angle1, 1e-9)) eraseP1b = true;
  }
  if (areAlmostEqual(angle2, 0, 1e-9)) {
    angle2 = getNearAngle(item1.m_edge.m_s, item1.m_edge.m_w0, true,
                          item2.m_edge.m_s, item2.m_edge.m_w0, true);
    if (areAlmostEqual(angle2, 1e-9)) eraseP2a = true;
  }
  if (areAlmostEqual(angle3, 0, 1e-9)) {
    angle3 = getNearAngle(item1.m_edge.m_s, item1.m_edge.m_w0, true,
                          item2.m_edge.m_s, item2.m_edge.m_w0, false);
    if (areAlmostEqual(angle3, 1e-9)) eraseP2b = true;
  }

  if (areAlmostEqual(angle1, angle2, 1e-9)) {
    angle = getNearAngle(item1.m_edge.m_s, item1.m_edge.m_w0, false,
                         item2.m_edge.m_s, item2.m_edge.m_w0, true);
    if (angle != 0) {
      angle2 += angle;
      if (angle2 > 360) angle2 -= 360;
    } else
      eraseP2a = true;
  }
  if (areAlmostEqual(angle1, angle3, 1e-9)) {
    angle = getNearAngle(item1.m_edge.m_s, item1.m_edge.m_w0, false,
                         item2.m_edge.m_s, item2.m_edge.m_w0, false);
    if (angle != 0) {
      angle3 += angle;
      if (angle3 > 360) angle3 -= 360;
    } else
      eraseP2b = true;
  }
  if (areAlmostEqual(angle2, angle3, 1e-9)) {
    angle = getNearAngle(item1.m_edge.m_s, item1.m_edge.m_w0, false,
                         item2.m_edge.m_s, item2.m_edge.m_w0, true);
    if (angle != 0) {
      angle3 += angle;
      if (angle3 > 360) angle3 -= 360;
    } else
      eraseP2b = true;
  }

  int fac =
      (angle1 < angle2) | ((angle1 < angle3) << 1) | ((angle2 < angle3) << 2);

  switch (fac) {
  case 0:  // p1a p2b p2a p1b
    insertBranch(interList, item1, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    break;
  case 1:  // p1a p2b p1b p2a
    insertBranch(interList, item1, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    break;
  case 2:
    assert(false);
    break;
  case 3:  // p1a p1b p2b p2a
    insertBranch(interList, item1, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    break;
  case 4:  // p1a p2a p2b p1b
    insertBranch(interList, item1, true);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP1b) insertBranch(interList, item1, false);
    break;
  case 5:
    assert(false);
    break;
  case 6:  // p1a p2a p1b p2b
    insertBranch(interList, item1, true);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2b) insertBranch(interList, item2, false);
    break;
  case 7:  // p1a p1b p2a p2b
    insertBranch(interList, item1, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    break;
  default:
    assert(false);
  }

  return true;
}

//-----------------------------------------------------------------------------

static bool makeIntersection(IntersectionData &intData,
                             const vector<VIStroke *> &s, int ii, int jj,
                             DoublePair inter, int strokeSize,
                             Intersection &interList) {
  IntersectedStroke item1, item2;

  interList.m_intersection = s[ii]->m_s->getPoint(inter.first);

  item1.m_edge.m_w0 = inter.first;
  item2.m_edge.m_w0 = inter.second;

  if (ii >= 0 && ii < strokeSize) {
    item1.m_edge.m_s     = s[ii]->m_s;
    item1.m_edge.m_index = ii;
  } else {
    if (ii < 0) {
      item1.m_edge.m_s     = intData.m_autocloseMap[ii]->m_s;
      item1.m_edge.m_index = ii;
    } else {
      item1.m_edge.m_s     = s[ii]->m_s;
      item1.m_edge.m_index = -(ii + intData.maxAutocloseId * 100000);
      intData.m_autocloseMap[item1.m_edge.m_index] = s[ii];
    }
  }

  if (jj >= 0 && jj < strokeSize) {
    item2.m_edge.m_s     = s[jj]->m_s;
    item2.m_edge.m_index = jj;
  } else {
    if (jj < 0) {
      item2.m_edge.m_s     = intData.m_autocloseMap[jj]->m_s;
      item2.m_edge.m_index = jj;
    } else {
      item2.m_edge.m_s     = s[jj]->m_s;
      item2.m_edge.m_index = -(jj + intData.maxAutocloseId * 100000);
      intData.m_autocloseMap[item2.m_edge.m_index] = s[jj];
    }
  }

  bool reversed = false;

  TPointD p0, p0b, p1, p1b;

  bool ret1 = item1.m_edge.m_s->getSpeedTwoValues(item1.m_edge.m_w0, p0, p0b);
  bool ret2 = item2.m_edge.m_s->getSpeedTwoValues(item2.m_edge.m_w0, p1, p1b);

  if (ret1 || ret2)  // punto angoloso!!!!
    return makeEdgeIntersection(interList, item1, item2, p0, p0b, p1, p1b);

  double crossVal = cross(p0, p1);

  // crossVal = cross(p0, p1);

  if (areAlmostEqual(crossVal, 0.0)) {
    bool endpoint1 = (item1.m_edge.m_w0 == 0.0 || item1.m_edge.m_w0 == 1.0);
    bool endpoint2 = (item2.m_edge.m_w0 == 0.0 || item2.m_edge.m_w0 == 1.0);
    if (endpoint1 && endpoint2 && ((p0.x * p1.x >= 0 && p0.y * p1.y >= 0 &&
                                    item1.m_edge.m_w0 != item2.m_edge.m_w0) ||
                                   (p0.x * p1.x <= 0 && p0.y * p1.y <= 0 &&
                                    item1.m_edge.m_w0 == item2.m_edge.m_w0)))
    // due endpoint a 180 gradi;metto
    {
      item1.m_gettingOut = (item1.m_edge.m_w0 == 0.0);
      interList.m_strokeList.pushBack(new IntersectedStroke(item1));
      item2.m_gettingOut = (item2.m_edge.m_w0 == 0.0);
      interList.m_strokeList.pushBack(new IntersectedStroke(item2));
      return true;
    }
    // crossVal = nearCrossVal(item1.m_edge.m_s, item1.m_edge.m_w0,
    // item2.m_edge.m_s, item2.m_edge.m_w0);
    // if (areAlmostEqual(crossVal, 0.0))
    // return false;
    return makeEdgeIntersection(interList, item1, item2, p0, p0b, p1, p1b);
  }

  if (crossVal > 0)
    reversed = true;  // std::reverse(interList.m_strokeList.begin(),
                      // interList.m_strokeList.end());

  if (item1.m_edge.m_w0 != 1.0) {
    item1.m_gettingOut = true;
    interList.m_strokeList.pushBack(new IntersectedStroke(item1));
  }
  if (item2.m_edge.m_w0 != (reversed ? 0.0 : 1.0)) {
    item2.m_gettingOut = !reversed;
    interList.m_strokeList.pushBack(new IntersectedStroke(item2));
  }
  if (item1.m_edge.m_w0 != 0.0) {
    item1.m_gettingOut = false;
    interList.m_strokeList.pushBack(new IntersectedStroke(item1));
  }
  if (item2.m_edge.m_w0 != (reversed ? 1.0 : 0.0)) {
    item2.m_gettingOut = reversed;
    interList.m_strokeList.pushBack(new IntersectedStroke(item2));
  }

  return true;
}

//-----------------------------------------------------------------------------
/*
void checkAuto(const vector<VIStroke*>& s)
{
for (int i=0; i<(int)s.size(); i++)
for (int j=i+1; j<(int)s.size(); j++)
  if (s[i]->m_s->getChunkCount()==1 && s[j]->m_s->getChunkCount()==1) //se ha
una sola quadratica, probabilmente e' un autoclose.
          {
                const TThickQuadratic*q = s[i]->m_s->getChunk(0);
                const TThickQuadratic*p = s[j]->m_s->getChunk(0);

                if (areAlmostEqual(q->getP0(), p->getP0(), 1e-2) &&
areAlmostEqual(q->getP2(), p->getP2(), 1e-2))
                  assert(!"eccolo!");
                if (areAlmostEqual(q->getP0(), p->getP2(), 1e-2) &&
areAlmostEqual(q->getP2(), p->getP0(), 1e-2))
                  assert(!"eccolo!");
    }
}
*/
//-----------------------------------------------------------------------------

static bool addAutocloseIntersection(IntersectionData &intData,
                                     vector<VIStroke *> &s, int ii, int jj,
                                     double w0, double w1, int strokeSize,
                                     bool isVectorized) {
  assert(s[ii]->m_groupId == s[jj]->m_groupId);

  Intersection *rp = intData.m_intList.last();

  assert(w0 >= 0.0 && w0 <= 1.0);
  assert(w1 >= 0.0 && w1 <= 1.0);

  for (; rp; rp = rp->prev())  // evito di fare la connessione, se gia' ce
                               // ne e' una simile tra le stesse due stroke
  {
    if (rp->m_strokeList.size() < 2) continue;
    IntersectedStroke *ps = rp->m_strokeList.first();
    int s0                = ps->m_edge.m_index;
    if (s0 < 0) continue;
    double ww0 = ps->m_edge.m_w0;
    ps         = ps->next();
    if (ps->m_edge.m_index == s0 && ps->m_edge.m_w0 == ww0) {
      ps = ps->next();
      if (!ps) continue;
    }
    int s1 = ps->m_edge.m_index;
    if (s1 < 0) continue;
    double ww1 = ps->m_edge.m_w0;

    if (!((s0 == ii && s1 == jj) || (s0 == jj && s1 == ii))) continue;

    if (s0 == ii && areAlmostEqual(w0, ww0, 0.1) &&
        areAlmostEqual(w1, ww1, 0.1))
      return false;
    else if (s1 == ii && areAlmostEqual(w0, ww1, 0.1) &&
             areAlmostEqual(w1, ww0, 0.1))
      return false;
  }

  vector<TPointD> v;
  v.push_back(s[ii]->m_s->getPoint(w0));
  v.push_back(s[jj]->m_s->getPoint(w1));
  if (v[0] == v[1])  // le stroke si intersecano , ma la fottuta funzione
                     // intersect non ha trovato l'intersezione(tipicamente,
                     // questo accade agli estremi)!!!
  {
    addIntersection(intData, s, ii, jj, DoublePair(w0, w1), strokeSize,
                    isVectorized);
    return true;
  }

  // se gia e' stato messo questo autoclose, evito
  for (int i = 0; i < (int)s.size(); i++)
    if (s[i]->m_s->getChunkCount() ==
        1)  // se ha una sola quadratica, probabilmente e' un autoclose.
    {
      const TThickQuadratic *q = s[i]->m_s->getChunk(0);

      if (areAlmostEqual(q->getP0(), v[0], 1e-2) &&
              areAlmostEqual(q->getP2(), v[1], 1e-2) ||
          areAlmostEqual(q->getP0(), v[1], 1e-2) &&
              areAlmostEqual(q->getP2(), v[0], 1e-2)) {
        return true;
        addIntersection(intData, s, i, ii, DoublePair(0.0, w0), strokeSize,
                        isVectorized);
        addIntersection(intData, s, i, jj, DoublePair(1.0, w1), strokeSize,
                        isVectorized);
        return true;
      }
    }
  assert(s[ii]->m_groupId == s[jj]->m_groupId);

  s.push_back(new VIStroke(new TStroke(v), s[ii]->m_groupId));
  addIntersection(intData, s, s.size() - 1, ii, DoublePair(0.0, w0), strokeSize,
                  isVectorized);
  addIntersection(intData, s, s.size() - 1, jj, DoublePair(1.0, w1), strokeSize,
                  isVectorized);
  return true;
}

//-----------------------------------------------------------------------------

// double g_autocloseTolerance = c_newAutocloseTolerance;

static bool isCloseEnoughP2P(double facMin, double facMax, TStroke *s1,
                             double w0, TStroke *s2, double w1) {
  double autoDistMin, autoDistMax;

  TThickPoint p0 = s1->getThickPoint(w0);
  TThickPoint p1 = s2->getThickPoint(w1);
  double dist2;

  dist2 = tdistance2(p0, p1);

  /*!when closing beetween a normal stroke and a 0-thickness stroke (very
   * typical) the thin  one is assumed to have same thickness of the other*/
  if (p0.thick == 0)
    p0.thick = p1.thick;
  else if (p1.thick == 0)
    p1.thick = p0.thick;
  if (facMin == 0) {
    autoDistMin = 0;
    autoDistMax =
        std::max(-2.0, facMax * (p0.thick + p1.thick) * (p0.thick + p1.thick));
    if (autoDistMax < 0.0000001)  //! for strokes without thickness, I connect
                                  //! for distances less than min between 2.5
                                  //! and half of the length of the stroke)
    {
      double len1 = s1->getLength();
      double len2 = s2->getLength();
      autoDistMax =
          facMax * std::min({2.5, len1 * len1 / (2 * 2), len2 * len2 / (2 * 2),
                             100.0 /*dummyVal*/});
    }
  } else {
    autoDistMin =
        std::max(-2.0, facMin * (p0.thick + p1.thick) * (p0.thick + p1.thick));
    if (autoDistMin < 0.0000001)  //! for strokes without thickness, I connect
                                  //! for distances less than min between 2.5
                                  //! and half of the length of the stroke)
    {
      double len1 = s1->getLength();
      double len2 = s2->getLength();
      autoDistMin =
          facMax * std::min({2.5, len1 * len1 / (2 * 2), len2 * len2 / (2 * 2),
                             100.0 /*dummyVal*/});
    }

    autoDistMax = autoDistMin + (facMax - facMin) * (facMax - facMin);
  }

  if (dist2 < autoDistMin || dist2 > autoDistMax) return false;

  // if (dist2<=std::max(2.0,
  // g_autocloseTolerance*(p0.thick+p1.thick)*(p0.thick+p1.thick))) //0.01 tiene
  // conto di quando thick==0
  if (s1 == s2) {
    TRectD r = s1->getBBox();  // se e' un autoclose su una stroke piccolissima,
                               // creerebbe uan area trascurabile, ignoro
    if (fabs(r.x1 - r.x0) < 2 && fabs(r.y1 - r.y0) < 2) return false;
  }
  return true;
}

//---------------------------------------------------------------------------------------------------------------------
/*
bool makePoint2PointConnections(double factor, vector<VIStroke*>& s,
                             int ii, bool isIStartPoint,
                             int jj, bool isJStartPoint, IntersectionData&
intData,
                              int strokeSize)
{
double w0 = (isIStartPoint?0.0:1.0);
double w1 = (isJStartPoint?0.0:1.0);
if (isCloseEnoughP2P(factor, s[ii]->m_s, w0,  s[jj]->m_s, w1))
  return addAutocloseIntersection(intData, s, ii, jj, w0, w1, strokeSize);
return false;
}
*/
//-----------------------------------------------------------------------------

static double getCurlW(TStroke *s,
                       bool isBegin)  // trova il punto di split su una
                                      // stroke, in prossimita di un
                                      // ricciolo;
// un ricciolo c'e' se la curva ha un  min o max relativo su x seguito da uno su
// y, o viceversa.
{
  int numChunks = s->getChunkCount();
  double dx2, dx1 = 0, dy2, dy1 = 0;
  int i = 0;
  for (i = 0; i < numChunks; i++) {
    const TQuadratic *q = s->getChunk(isBegin ? i : numChunks - 1 - i);
    dy2                 = q->getP1().y - q->getP0().y;
    if (dy1 * dy2 < 0) break;
    dy1 = dy2;
    dy2 = q->getP2().y - q->getP1().y;
    if (dy1 * dy2 < 0) break;
    dy1 = dy2;
  }
  if (i == numChunks) return -1;

  int maxMin0 = isBegin ? i : numChunks - 1 - i;
  int j       = 0;
  for (j = 0; j < numChunks; j++) {
    const TQuadratic *q = s->getChunk(isBegin ? j : numChunks - 1 - j);
    dx2                 = q->getP1().x - q->getP0().x;
    if (dx1 * dx2 < 0) break;
    dx1 = dx2;
    dx2 = q->getP2().x - q->getP1().x;
    if (dx1 * dx2 < 0) break;
    dx1 = dx2;
  }
  if (j == numChunks) return -1;

  int maxMin1 = isBegin ? j : numChunks - 1 - j;

  return getWfromChunkAndT(
      s, isBegin ? std::max(maxMin0, maxMin1) : std::min(maxMin0, maxMin1),
      isBegin ? 1.0 : 0.0);
}

#ifdef Levo
bool lastIsX = false, lastIsY = false;
for (int i = 0; i < numChunks; i++) {
  const TThickQuadratic *q = s->getChunk(isBegin ? i : numChunks - 1 - i);
  if ((q->getP0().y < q->getP1().y &&
       q->getP2().y <
           q->getP1().y) ||  // la quadratica ha un minimo o massimo relativo
      (q->getP0().y > q->getP1().y && q->getP2().y > q->getP1().y)) {
    double w = getWfromChunkAndT(s, isBegin ? i : numChunks - 1 - i,
                                 isBegin ? 1.0 : 0.0);
    if (lastIsX)  // e' il secondo min o max relativo
      return w;
    lastIsX = false;
    lastIsY = true;

  } else if ((q->getP0().x < q->getP1().x &&
              q->getP2().x <
                  q->getP1()
                      .x) ||  // la quadratica ha un minimo o massimo relativo
             (q->getP0().x > q->getP1().x && q->getP2().x > q->getP1().x)) {
    double w = getWfromChunkAndT(s, isBegin ? i : numChunks - 1 - i,
                                 isBegin ? 1.0 : 0.0);
    if (lastIsY)  // e' il secondo min o max relativo
      return w;
    lastIsX = true;
    lastIsY = false;
  }
}

return -1;
}

#endif
//-----------------------------------------------------------------------------

static bool isCloseEnoughP2L(double facMin, double facMax, TStroke *s1,
                             double w1, TStroke *s2, double &w) {
  if (s1->isSelfLoop()) return false;

  TThickPoint p0 = s1->getThickPoint(w1);
  double t, dist2;
  int index;
  TStroke sAux, *sComp;

  if (s1 == s2)  // per trovare le intersezioni con una stroke e se stessa, si
                 // toglie il
  // pezzo di stroke di cui si cercano vicinanze fino alla prima curva
  {
    double w = getCurlW(s1, w1 == 0.0);
    if (w == -1) return false;

    split<TStroke>(*s1, std::min(1 - w1, w), std::max(1 - w1, w), sAux);
    sComp = &sAux;
  } else
    sComp = s2;

  if (sComp->getNearestChunk(p0, t, index, dist2) && dist2 > 0) {
    if (s1 == s2) {
      double dummy;
      s2->getNearestChunk(sComp->getChunk(index)->getPoint(t), t, index, dummy);
    }

    // if (areAlmostEqual(w, 0.0, 0.05) || areAlmostEqual(w, 1.0, 0.05))
    //  return; //se w e' vicino ad un estremo, rientra nell'autoclose point to
    //  point

    // if (s[jj]->m_s->getLength(w)<=s[jj]->m_s->getThickPoint(0).thick ||
    //    s[jj]->m_s->getLength(w, 1)<=s[jj]->m_s->getThickPoint(1).thick)
    //  return;

    TThickPoint p1 = s2->getChunk(index)->getThickPoint(t);

    /*!when closing beetween a normal stroke and a 0-thickness stroke (very
     * typical) the thin  one is assumed to have same thickness of the other*/
    if (p0.thick == 0)
      p0.thick = p1.thick;
    else if (p1.thick == 0)
      p1.thick = p0.thick;
    double autoDistMin, autoDistMax;
    if (facMin == 0) {
      autoDistMin = 0;
      autoDistMax = std::max(
          -2.0, (facMax + 0.7) * (p0.thick + p1.thick) * (p0.thick + p1.thick));
      if (autoDistMax < 0.0000001)  //! for strokes without thickness, I connect
                                    //! for distances less than min between 2.5
                                    //! and half of the length of the pointing
                                    //! stroke)
      {
        double len1 = s1->getLength();
        autoDistMax = facMax * std::min(2.5, len1 * len1 / (2 * 2));
      }
    } else {
      autoDistMin = std::max(
          -2.0, (facMin + 0.7) * (p0.thick + p1.thick) * (p0.thick + p1.thick));
      if (autoDistMin < 0.0000001)  //! for strokes without thickness, I connect
                                    //! for distances less than min between 2.5
                                    //! and half of the length of the pointing
                                    //! stroke)
      {
        double len1 = s1->getLength();
        autoDistMin = facMax * std::min(2.5, len1 * len1 / (2 * 2));
      }

      autoDistMax =
          autoDistMin + (facMax - facMin + 0.7) * (facMax - facMin + 0.7);
    }

    // double autoDistMin = std::max(-2.0,
    // facMin==0?0:(facMin+0.7)*(p0.thick+p1.thick)*(p0.thick+p1.thick));
    // double autoDistMax = std::max(-2.0,
    // (facMax+0.7)*(p0.thick+p1.thick)*(p0.thick+p1.thick));

    if (dist2 < autoDistMin || dist2 > autoDistMax) return false;

    // if (dist2<=(std::max(2.0,
    // (g_autocloseTolerance+0.7)*(p0.thick+p1.thick)*(p0.thick+p1.thick))))
    // //0.01 tiene conto di quando thick==0

    w = getWfromChunkAndT(s2, index, t);
    return true;
  }
  return false;
}

//-------------------------------------------------------------
/*
void makePoint2LineConnection(double factor, vector<VIStroke*>& s, int ii, int
jj, bool isBegin, IntersectionData& intData,
                    int strokeSize)
{
double w1 = isBegin?0.0: 1.0;

TStroke* s1 = s[ii]->m_s;
TStroke* s2 = s[jj]->m_s;
double w;
if (isCloseEnoughP2L(factor, s1, w1,  s2, w))
  addAutocloseIntersection(intData, s, ii, jj, w1, w, strokeSize);
}
*/
//-----------------------------------------------------------------------------

namespace {

inline bool isSegment(const TStroke &s) {
  vector<TThickPoint> v;
  s.getControlPoints(v);
  UINT n = v.size();
  if (areAlmostEqual(v[n - 1].x, v[0].x, 1e-4)) {
    for (UINT i = 1; i < n - 1; i++)
      if (!areAlmostEqual(v[i].x, v[0].x, 1e-4)) return false;
  } else if (areAlmostEqual(v[n - 1].y, v[0].y, 1e-4)) {
    for (UINT i = 1; i < n - 1; i++)
      if (!areAlmostEqual(v[i].y, v[0].y, 1e-4)) return false;
  } else {
    double fac = (v[n - 1].y - v[0].y) / (v[n - 1].x - v[0].x);
    for (UINT i = 1; i < n - 1; i++)
      if (!areAlmostEqual((v[i].y - v[0].y) / (v[i].x - v[0].x), fac, 1e-4))
        return false;
  }
  return true;
}

//---------------------------------------------------------------------------------
/*
bool segmentAlreadyExist(const TVectorImageP& vi, const TPointD& p1, const
TPointD& p2)
{
for (UINT i=0; i<vi->getStrokeCount(); i++)
  {
  TStroke*s = vi->getStroke(i);
  if (!s->getBBox().contains(p1) || !s->getBBox().contains(p2))
    continue;
  if (((areAlmostEqual(s->getPoint(0.0), p1, 1e-4) &&
areAlmostEqual(s->getPoint(1.0), p2, 1e-4)) ||
      (areAlmostEqual(s->getPoint(0.0), p2, 1e-4) &&
areAlmostEqual(s->getPoint(1.0), p1, 1e-4))) &&
       isSegment(*s))
    return true;

  }
return false;
}
*/
//----------------------------------------------------------------------------------

bool segmentAlreadyPresent(const TVectorImageP &vi, const TPointD &p1,
                           const TPointD &p2) {
  for (UINT i = 0; i < vi->getStrokeCount(); i++) {
    TStroke *s = vi->getStroke(i);
    if (((areAlmostEqual(s->getPoint(0.0), p1, 1e-4) &&
          areAlmostEqual(s->getPoint(1.0), p2, 1e-4)) ||
         (areAlmostEqual(s->getPoint(0.0), p2, 1e-4) &&
          areAlmostEqual(s->getPoint(1.0), p1, 1e-4))) &&
        isSegment(*s))
      return true;
  }
  return false;
  /*
for (UINT i=0; i<vi->getStrokeCount(); i++)
{
TStroke* s = vi->getStroke(i);

if (s->getChunkCount()!=1)
continue;
if (areAlmostEqual((TPointD)s->getControlPoint(0),                           p1,
1e-2) &&
areAlmostEqual((TPointD)s->getControlPoint(s->getControlPointCount()-1), p2,
1e-2))
return true;
}

return false;
*/
}

void getClosingSegments(TL2LAutocloser &l2lautocloser, double facMin,
                        double facMax, TStroke *s1, TStroke *s2,
                        vector<DoublePair> *intersections,
                        vector<std::pair<double, double>> &segments) {
  bool ret1 = false, ret2 = false, ret3 = false, ret4 = false;
#define L2LAUTOCLOSE
#ifdef L2LAUTOCLOSE
  double thickmax2 = s1->getMaxThickness() + s2->getMaxThickness();
  thickmax2 *= thickmax2;
  if (facMin == 0)
    l2lautocloser.setMaxDistance2((facMax + 0.7) * thickmax2);
  else
    l2lautocloser.setMaxDistance2((facMax + 0.7) * thickmax2 +
                                  (facMax - facMin + 0.7) *
                                      (facMax - facMin + 0.7));

  std::vector<TL2LAutocloser::Segment> l2lSegments;
  if (intersections)
    l2lautocloser.search(l2lSegments, s1, s2, *intersections);
  else
    l2lautocloser.search(l2lSegments, s1, s2);

  for (UINT i = 0; i < l2lSegments.size(); i++) {
    TL2LAutocloser::Segment &seg = l2lSegments[i];
    double autoDistMin, autoDistMax;
    if (facMin == 0) {
      autoDistMin = 0;
      autoDistMax = (facMax + 0.7) * (seg.p0.thick + seg.p1.thick) *
                    (seg.p0.thick + seg.p1.thick);
    } else {
      autoDistMin = (facMin + 0.7) * (seg.p0.thick + seg.p1.thick) *
                    (seg.p0.thick + seg.p1.thick);
      autoDistMax =
          autoDistMin + (facMax - facMin + 0.7) * (facMax - facMin + 0.7);
    }

    if (seg.dist2 > autoDistMin && seg.dist2 < autoDistMax)
      segments.push_back(std::pair<double, double>(seg.w0, seg.w1));
  }
#endif

  if (s1->isSelfLoop() && s2->isSelfLoop()) return;

  if (!s1->isSelfLoop() && !s2->isSelfLoop()) {
    if ((ret1 = isCloseEnoughP2P(facMin, facMax, s1, 0.0, s2, 1.0)))
      segments.push_back(std::pair<double, double>(0.0, 1.0));

    if (s1 != s2) {
      if ((ret2 = isCloseEnoughP2P(facMin, facMax, s1, 0.0, s2, 0.0)))
        segments.push_back(std::pair<double, double>(0.0, 0.0));
      if ((ret3 = isCloseEnoughP2P(facMin, facMax, s1, 1.0, s2, 0.0)))
        segments.push_back(std::pair<double, double>(1.0, 0.0));
      if ((ret4 = isCloseEnoughP2P(facMin, facMax, s1, 1.0, s2, 1.0)))
        segments.push_back(std::pair<double, double>(1.0, 1.0));
    }
  }

  double w;
  if (!ret1 && !ret2 && isCloseEnoughP2L(facMin, facMax, s1, 0.0, s2, w))
    segments.push_back(std::pair<double, double>(0.0, w));
  if (!ret1 && !ret4 && isCloseEnoughP2L(facMin, facMax, s2, 1.0, s1, w))
    segments.push_back(std::pair<double, double>(w, 1.0));

  if (s1 != s2) {
    if (!ret2 && !ret3 && isCloseEnoughP2L(facMin, facMax, s2, 0.0, s1, w))
      segments.push_back(std::pair<double, double>(w, 0.0));
    if (!ret3 && !ret4 && isCloseEnoughP2L(facMin, facMax, s1, 1.0, s2, w))
      segments.push_back(std::pair<double, double>(1.0, w));
  }
}

}  // namaspace

//---------------------------------------------------------------------------------

void getClosingPoints(const TRectD &rect, double fac, const TVectorImageP &vi,
                      vector<pair<int, double>> &startPoints,
                      vector<pair<int, double>> &endPoints) {
  UINT strokeCount = vi->getStrokeCount();
  TL2LAutocloser l2lautocloser;

  for (UINT i = 0; i < strokeCount; i++) {
    TStroke *s1 = vi->getStroke(i);
    if (!rect.overlaps(s1->getBBox())) continue;
    if (s1->getChunkCount() == 1) continue;

    for (UINT j = i; j < strokeCount; j++) {
      TStroke *s2 = vi->getStroke(j);

      if (!rect.overlaps(s1->getBBox())) continue;
      if (s2->getChunkCount() == 1) continue;

#ifdef NEW_REGION_FILL
      double autoTol = 0;
#else
      double autoTol = vi->getAutocloseTolerance();
#endif

      double enlarge1 =
          (autoTol + 0.7) *
              (s1->getMaxThickness() > 0 ? s1->getMaxThickness() : 2.5) +
          fac;
      double enlarge2 =
          (autoTol + 0.7) *
              (s2->getMaxThickness() > 0 ? s2->getMaxThickness() : 2.5) +
          fac;

      if (i != j &&
          !s1->getBBox().enlarge(enlarge1).overlaps(
              s2->getBBox().enlarge(enlarge2)))
        continue;

      vector<std::pair<double, double>> segments;
      getClosingSegments(l2lautocloser, autoTol, autoTol + fac, s1, s2, 0,
                         segments);
      for (UINT k = 0; k < segments.size(); k++) {
        TPointD p1 = s1->getPoint(segments[k].first);
        TPointD p2 = s2->getPoint(segments[k].second);
        if (rect.contains(p1) && rect.contains(p2)) {
          if (segmentAlreadyPresent(vi, p1, p2)) continue;
          startPoints.push_back(pair<int, double>(i, segments[k].first));
          endPoints.push_back(pair<int, double>(j, segments[k].second));
        }
      }
    }
  }
}

//-------------------------------------------------------------------------------------------------------

static void autoclose(double factor, vector<VIStroke *> &s, int ii, int jj,
                      IntersectionData &IntData, int strokeSize,
                      TL2LAutocloser &l2lautocloser,
                      vector<DoublePair> *intersections, bool isVectorized) {
  vector<std::pair<double, double>> segments;
  getClosingSegments(l2lautocloser, 0, factor, s[ii]->m_s, s[jj]->m_s,
                     intersections, segments);

  for (UINT i = 0; i < segments.size(); i++)
    addAutocloseIntersection(IntData, s, ii, jj, segments[i].first,
                             segments[i].second, strokeSize, isVectorized);
}

//------------------------------------------------------------------------------------------------------
#ifdef LEVO
void autoclose(double factor, vector<VIStroke *> &s, int ii, int jj,
               IntersectionData &IntData, int strokeSize) {
  bool ret1 = false, ret2 = false, ret3 = false, ret4 = false;

  if (s[ii]->m_s->isSelfLoop() && s[jj]->m_s->isSelfLoop()) return;

  if (!s[ii]->m_s->isSelfLoop() && !s[jj]->m_s->isSelfLoop()) {
    ret1 = makePoint2PointConnections(factor, s, ii, true, jj, false, IntData,
                                      strokeSize);

    if (ii != jj) {
      ret2 = makePoint2PointConnections(factor, s, ii, true, jj, true, IntData,
                                        strokeSize);
      ret3 = makePoint2PointConnections(factor, s, ii, false, jj, true, IntData,
                                        strokeSize);
      ret4 = makePoint2PointConnections(factor, s, ii, false, jj, false,
                                        IntData, strokeSize);
    }
  }

  if (!ret1 && !ret2)
    makePoint2LineConnection(factor, s, ii, jj, true, IntData, strokeSize);
  if (!ret1 && !ret4)
    makePoint2LineConnection(factor, s, jj, ii, false, IntData, strokeSize);
  if (ii != jj) {
    if (!ret2 && !ret3)
      makePoint2LineConnection(factor, s, jj, ii, true, IntData, strokeSize);
    if (!ret3 && !ret4)
      makePoint2LineConnection(factor, s, ii, jj, false, IntData, strokeSize);
  }
}

#endif

//-----------------------------------------------------------------------------

TPointD inline getTangent(const IntersectedStroke &item) {
  return (item.m_gettingOut ? 1 : -1) *
         item.m_edge.m_s->getSpeed(item.m_edge.m_w0, item.m_gettingOut);
}

//-----------------------------------------------------------------------------

static void addBranch(IntersectionData &intData,
                      VIList<IntersectedStroke> &strokeList,
                      const vector<VIStroke *> &s, int ii, double w,
                      int strokeSize, bool gettingOut) {
  IntersectedStroke *p1, *p2;
  TPointD tanRef, lastTan;

  IntersectedStroke *item = new IntersectedStroke();

  if (ii < 0) {
    item->m_edge.m_s     = intData.m_autocloseMap[ii]->m_s;
    item->m_edge.m_index = ii;
  } else {
    item->m_edge.m_s = s[ii]->m_s;
    if (ii < strokeSize)
      item->m_edge.m_index = ii;
    else {
      item->m_edge.m_index = -(ii + intData.maxAutocloseId * 100000);
      intData.m_autocloseMap[item->m_edge.m_index] = s[ii];
    }
  }

  item->m_edge.m_w0  = w;
  item->m_gettingOut = gettingOut;

  /*
if (strokeList.size()==2) //potrebbero essere orientati male; due branch possono
stare come vogliono, ma col terzo no.
{
TPointD tan2 = getTangent(strokeList.back());
TPointD aux= getTangent(*(strokeList.begin()));
double crossVal = cross(aux, tan2);
if (areAlmostEqual(aux, tan2, 1e-3))
    return;

  if (crossVal>0)
{
          std::reverse(strokeList.begin(), strokeList.end());
//tan2 = getTangent(strokeList.back());
          }
}
*/
  /*
if (areAlmostEqual(lastCross, 0.0) && tan1.x*tan2.x>=0 && tan1.y*tan2.y>=0)
//significa angolo tra tangenti nullo
    {
          crossVal =  nearCrossVal(item.m_edge.m_s, item.m_edge.m_w0,
strokeList.back().m_edge.m_s, strokeList.back().m_edge.m_w0);
if (areAlmostEqual(crossVal, 0.0))
            return;
          if (!strokeList.back().m_gettingOut)
            crossVal = -crossVal;
}
*/

  tanRef  = getTangent(*item);
  lastTan = getTangent(*strokeList.last());
  /*
for (it=strokeList.begin(); it!=strokeList.end(); ++it)
{
  TPointD curTan = getTangent(*it);
double angle0 = getAngle(lastTan, curTan);
double angle1 = getAngle(lastTan, tanRef);

if (areAlmostEqual(angle0, angle1, 1e-8))
    {
          double angle = getNearAngle( it->m_edge.m_s,  it->m_edge.m_w0,
it->m_gettingOut,
                                      item.m_edge.m_s, item.m_edge.m_w0,
item.m_gettingOut);
angle1 += angle; if (angle1>360) angle1-=360;
}

if (angle1<angle0)
{
strokeList.insert(it, item);
return;
}
  lastTan=curTan;
}*/

  p2 = strokeList.last();

  for (p1 = strokeList.first(); p1; p1 = p1->next()) {
    TPointD curTan = getTangent(*p1);
    double angle0  = getAngle(lastTan, curTan);
    double angle1  = getAngle(lastTan, tanRef);

    if (areAlmostEqual(angle1, 0, 1e-8)) {
      double angle =
          getNearAngle(p2->m_edge.m_s, p2->m_edge.m_w0, p2->m_gettingOut,
                       item->m_edge.m_s, item->m_edge.m_w0, item->m_gettingOut);
      angle1 += angle;
      if (angle1 > 360) angle1 -= 360;
    }

    if (areAlmostEqual(angle0, angle1, 1e-8)) {
      double angle =
          getNearAngle(p1->m_edge.m_s, p1->m_edge.m_w0, p1->m_gettingOut,
                       item->m_edge.m_s, item->m_edge.m_w0, item->m_gettingOut);
      angle1 += angle;
      if (angle1 > 360) angle1 -= 360;
    }

    if (angle1 < angle0) {
      strokeList.insert(p1, item);
      return;
    }
    lastTan = curTan;
    p2      = p1;
  }

  // assert(!"add branch: can't find where to insert!");
  strokeList.pushBack(item);
}

//-----------------------------------------------------------------------------

static void addBranches(IntersectionData &intData, Intersection &intersection,
                        const vector<VIStroke *> &s, int ii, int jj,
                        DoublePair intersectionPair, int strokeSize) {
  bool foundS1 = false, foundS2 = false;
  IntersectedStroke *p;

  assert(!intersection.m_strokeList.empty());

  for (p = intersection.m_strokeList.first(); p; p = p->next()) {
    if ((ii >= 0 && p->m_edge.m_s == s[ii]->m_s &&
         p->m_edge.m_w0 == intersectionPair.first) ||
        (ii < 0 && p->m_edge.m_index == ii &&
         p->m_edge.m_w0 == intersectionPair.first))
      foundS1 = true;
    if ((jj >= 0 && p->m_edge.m_s == s[jj]->m_s &&
         p->m_edge.m_w0 == intersectionPair.second) ||
        (jj < 0 && p->m_edge.m_index == jj &&
         p->m_edge.m_w0 == intersectionPair.second))
      foundS2 = true;
  }

  if (foundS1 && foundS2) {
    /*
//errore!(vedi commento sotto) possono essere un sacco di intersezioni
coincidenti se passano per l'estremo di una quad
//significa che ci sono due intersezioni coincidenti. cioe' due stroke tangenti.
quindi devo invertire l'ordine di due branch enlla rosa dei branch.
list<IntersectedStroke>::iterator it1, it2;
it1=intersection.m_strokeList.begin();
it2 = it1; it2++;
for (; it2!=intersection.m_strokeList.end(); ++it1, ++it2)
{
if ((*it1).m_gettingOut!=(*it2).m_gettingOut &&((*it1).m_edge.m_index==jj &&
(*it2).m_edge.m_index==ii) ||
        ((*it1).m_edge.m_index==ii && (*it2).m_edge.m_index==jj))
{
            IntersectedStroke& el1 = (*it1);
            IntersectedStroke& el2 = (*it2);
IntersectedStroke app;
            app = el1;
            el1=el2;
            el2=app;
            break;
}
    }
*/
    return;
  }

  if (!foundS1) {
    if (intersectionPair.first != 1)
      addBranch(intData, intersection.m_strokeList, s, ii,
                intersectionPair.first, strokeSize, true);
    if (intersectionPair.first != 0)
      addBranch(intData, intersection.m_strokeList, s, ii,
                intersectionPair.first, strokeSize, false);
    // assert(intersection.m_strokeList.size()-size>0);
  }
  if (!foundS2) {
    if (intersectionPair.second != 1)
      addBranch(intData, intersection.m_strokeList, s, jj,
                intersectionPair.second, strokeSize, true);
    if (intersectionPair.second != 0)
      addBranch(intData, intersection.m_strokeList, s, jj,
                intersectionPair.second, strokeSize, false);
    // intersection.m_numInter+=intersection.m_strokeList.size()-size;
    // assert(intersection.m_strokeList.size()-size>0);
  }
}

//-----------------------------------------------------------------------------

static void addIntersections(IntersectionData &intData,
                             const vector<VIStroke *> &s, int ii, int jj,
                             vector<DoublePair> &intersections, int strokeSize,
                             bool isVectorized) {
  for (int k = 0; k < (int)intersections.size(); k++) {
    if (ii >= strokeSize && (areAlmostEqual(intersections[k].first, 0.0) ||
                             areAlmostEqual(intersections[k].first, 1.0)))
      continue;
    if (jj >= strokeSize && (areAlmostEqual(intersections[k].second, 0.0) ||
                             areAlmostEqual(intersections[k].second, 1.0)))
      continue;

    addIntersection(intData, s, ii, jj, intersections[k], strokeSize,
                    isVectorized);
  }
}

//-----------------------------------------------------------------------------

inline double truncate(double x) {
  x += 1.0;
  TUINT32 *l = (TUINT32 *)&x;

#if TNZ_LITTLE_ENDIAN
  l[0] &= 0xFFE00000;
#else
  l[1] &= 0xFFE00000;
#endif

  return x - 1.0;
}

//-----------------------------------------------------------------------------

void addIntersection(IntersectionData &intData, const vector<VIStroke *> &s,
                     int ii, int jj, DoublePair intersection, int strokeSize,
                     bool isVectorized) {
  Intersection *p;
  TPointD point;

  assert(ii < 0 || jj < 0 || s[ii]->m_groupId == s[jj]->m_groupId);

  // UINT iw;
  // iw = ((UINT)(intersection.first*0x3fffffff));
  // intersection.first = truncate(intersection.first);
  // iw = (UINT)(intersection.second*0x3fffffff);

  // intersection.second = truncate(intersection.second);

  if (areAlmostEqual(intersection.first, 0.0, 1e-5))
    intersection.first = 0.0;
  else if (areAlmostEqual(intersection.first, 1.0, 1e-5))
    intersection.first = 1.0;

  if (areAlmostEqual(intersection.second, 0.0, 1e-5))
    intersection.second = 0.0;
  else if (areAlmostEqual(intersection.second, 1.0, 1e-5))
    intersection.second = 1.0;

  point = s[ii]->m_s->getPoint(intersection.first);

  for (p = intData.m_intList.first(); p; p = p->next())
    if (p->m_intersection == point ||
        (isVectorized &&
         areAlmostEqual(
             p->m_intersection, point,
             1e-2)))  // devono essere rigorosamente uguali, altrimenti
    // il calcolo dell'ordine dei rami con le tangenti sballa
    {
      addBranches(intData, *p, s, ii, jj, intersection, strokeSize);
      return;
    }

  intData.m_intList.pushBack(new Intersection);

  if (!makeIntersection(intData, s, ii, jj, intersection, strokeSize,
                        *intData.m_intList.last()))
    intData.m_intList.erase(intData.m_intList.last());
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::findIntersections() {
  vector<VIStroke *> &strokeArray = m_strokes;
  IntersectionData &intData       = *m_intersectionData;
  int strokeSize                  = (int)strokeArray.size();
  int i, j;
  bool isVectorized = (m_autocloseTolerance < 0);

  assert(intData.m_intersectedStrokeArray.empty());
#define AUTOCLOSE_ATTIVO
#ifdef AUTOCLOSE_ATTIVO
  intData.maxAutocloseId++;

  map<int, VIStroke *>::iterator it, it_b = intData.m_autocloseMap.begin();
  map<int, VIStroke *>::iterator it_e = intData.m_autocloseMap.end();

  // prima cerco le intersezioni tra nuove strokes e vecchi autoclose
  for (i = 0; i < strokeSize; i++) {
    TStroke *s1 = strokeArray[i]->m_s;
    if (!strokeArray[i]->m_isNewForFill || strokeArray[i]->m_isPoint) continue;

    TRectD bBox   = s1->getBBox();
    double thick2 = s1->getThickPoint(0).thick *
                    2.1;  // 2.1 instead of 2.0, for usual problems of approx...
    if (bBox.getLx() <= thick2 && bBox.getLy() <= thick2) {
      strokeArray[i]->m_isPoint = true;
      continue;
    }

    roundStroke(s1);

    for (it = it_b; it != it_e; ++it) {
      if (!it->second || it->second->m_groupId != strokeArray[i]->m_groupId)
        continue;

      TStroke *s2 = it->second->m_s;
      vector<DoublePair> parIntersections;
      if (intersect(s1, s2, parIntersections, true))
        addIntersections(intData, strokeArray, i, it->first, parIntersections,
                         strokeSize, isVectorized);
    }
  }
#endif

  // poi,  intersezioni tra stroke, in cui almeno uno dei due deve essere nuovo

  map<pair<int, int>, vector<DoublePair>> intersectionMap;

  for (i = 0; i < strokeSize; i++) {
    TStroke *s1 = strokeArray[i]->m_s;
    if (strokeArray[i]->m_isPoint) continue;
    for (j = i; j < strokeSize /*&& (strokeArray[i]->getBBox().x1>=
                                  strokeArray[j]->getBBox().x0)*/
         ;
         j++) {
      TStroke *s2 = strokeArray[j]->m_s;

      if (strokeArray[j]->m_isPoint ||
          !(strokeArray[i]->m_isNewForFill || strokeArray[j]->m_isNewForFill))
        continue;
      if (strokeArray[i]->m_groupId != strokeArray[j]->m_groupId) continue;

      vector<DoublePair> parIntersections;
      if (s1->getBBox().overlaps(s2->getBBox())) {
        UINT size = intData.m_intList.size();

        if (intersect(s1, s2, parIntersections, false)) {
          // if (i==0 && j==1) parIntersections.erase(parIntersections.begin());
          intersectionMap[pair<int, int>(i, j)] = parIntersections;
          addIntersections(intData, strokeArray, i, j, parIntersections,
                           strokeSize, isVectorized);
        } else
          intersectionMap[pair<int, int>(i, j)] = vector<DoublePair>();

        if (!strokeArray[i]->m_isNewForFill &&
            size != intData.m_intList.size() &&
            !strokeArray[i]->m_edgeList.empty())  // aggiunte nuove intersezioni
        {
          intData.m_intersectedStrokeArray.push_back(IntersectedStrokeEdges(i));
          list<TEdge *> &_list =
              intData.m_intersectedStrokeArray.back().m_edgeList;
          list<TEdge *>::const_iterator it;
          for (it = strokeArray[i]->m_edgeList.begin();
               it != strokeArray[i]->m_edgeList.end(); ++it)
            _list.push_back(new TEdge(**it, false));
        }
      }
    }
  }

#ifdef AUTOCLOSE_ATTIVO
  TL2LAutocloser l2lautocloser;

  for (i = 0; i < strokeSize; i++) {
    TStroke *s1 = strokeArray[i]->m_s;
    if (strokeArray[i]->m_isPoint) continue;
    for (j = i; j < strokeSize; j++) {
      if (strokeArray[i]->m_groupId != strokeArray[j]->m_groupId) continue;

      TStroke *s2 = strokeArray[j]->m_s;
      if (strokeArray[j]->m_isPoint) continue;
      if (!(strokeArray[i]->m_isNewForFill || strokeArray[j]->m_isNewForFill))
        continue;

      double enlarge1 =
          (m_autocloseTolerance + 0.7) *
          (s1->getMaxThickness() > 0 ? s1->getMaxThickness() : 2.5);
      double enlarge2 =
          (m_autocloseTolerance + 0.7) *
          (s2->getMaxThickness() > 0 ? s2->getMaxThickness() : 2.5);

      if (s1->getBBox().enlarge(enlarge1).overlaps(
              s2->getBBox().enlarge(enlarge2))) {
        map<pair<int, int>, vector<DoublePair>>::iterator it =
            intersectionMap.find(pair<int, int>(i, j));
        if (it == intersectionMap.end())
          autoclose(m_autocloseTolerance, strokeArray, i, j, intData,
                    strokeSize, l2lautocloser, 0, isVectorized);
        else
          autoclose(m_autocloseTolerance, strokeArray, i, j, intData,
                    strokeSize, l2lautocloser, &(it->second), isVectorized);
      }
    }
    strokeArray[i]->m_isNewForFill = false;
  }
#endif

  for (i = 0; i < strokeSize; i++) {
    list<TEdge *>::iterator it, it_b = strokeArray[i]->m_edgeList.begin(),
                                it_e = strokeArray[i]->m_edgeList.end();
    for (it = it_b; it != it_e; ++it)
      if ((*it)->m_toBeDeleted == 1) delete *it;

    strokeArray[i]->m_edgeList.clear();
  }

  // si devono cercare le intersezioni con i segmenti aggiunti per l'autoclose

  for (i = strokeSize; i < (int)strokeArray.size(); ++i) {
    TStroke *s1 = strokeArray[i]->m_s;

    for (j = i + 1; j < (int)strokeArray.size();
         ++j)  // intersezione segmento-segmento
    {
      if (strokeArray[i]->m_groupId != strokeArray[j]->m_groupId) continue;

      TStroke *s2 = strokeArray[j]->m_s;
      vector<DoublePair> parIntersections;
      if (intersect(s1, s2, parIntersections, true))
        addIntersections(intData, strokeArray, i, j, parIntersections,
                         strokeSize, isVectorized);
    }
    for (j = 0; j < strokeSize; ++j)  // intersezione segmento-curva
    {
      if (strokeArray[j]->m_isPoint) continue;
      if (strokeArray[i]->m_groupId != strokeArray[j]->m_groupId) continue;

      TStroke *s2 = strokeArray[j]->m_s;
      vector<DoublePair> parIntersections;
      if (intersect(s1, s2, parIntersections, true))
        addIntersections(intData, strokeArray, i, j, parIntersections,
                         strokeSize, isVectorized);
    }
  }
}

// la struttura delle intersezioni viene poi visitata per trovare
// i link tra un'intersezione e la successiva

//-----------------------------------------------------------------------------
void TVectorImage::Imp::deleteRegionsData() {
  clearPointerContainer(m_strokes);
  clearPointerContainer(m_regions);

  Intersection *p1;
  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    p1->m_strokeList.clear();
  m_intersectionData->m_intList.clear();
  delete m_intersectionData;
  m_intersectionData = 0;
}

void TVectorImage::Imp::initRegionsData() {
  m_intersectionData = new IntersectionData();
}

//-----------------------------------------------------------------------------

int TVectorImage::Imp::computeIntersections() {
  Intersection *p1;
  IntersectionData &intData = *m_intersectionData;
  int strokeSize            = (int)m_strokes.size();

  findIntersections();

  findNearestIntersection(intData.m_intList);

  // for (it1=intData.m_intList.begin(); it1!=intData.m_intList.end();) //la
  // faccio qui, e non nella eraseIntersection. vedi commento li'.
  eraseDeadIntersections();

  for (p1 = intData.m_intList.first(); p1; p1 = p1->next())
    markDeadIntersections(intData.m_intList, p1);

  // checkInterList(intData.m_intList);
  return strokeSize;
}

//-----------------------------------------------------------------------------

/*
void endPointIntersect(const TStroke* s0, const TStroke* s1, vector<DoublePair>&
parIntersections)
{
TPointD p00 = s0->getPoint(0);
TPointD p11 = s1->getPoint(1);
if (tdistance2(p00, p11)< 2*0.06*0.06)
  parIntersections.push_back(DoublePair(0, 1));

if (s0==s1)
  return;

TPointD p01 = s0->getPoint(1);
TPointD p10 = s1->getPoint(0);

if (tdistance2(p00, p10)< 2*0.06*0.06)
  parIntersections.push_back(DoublePair(0, 0));
if (tdistance2(p01, p10)< 2*0.06*0.06)
  parIntersections.push_back(DoublePair(1, 0));
if (tdistance2(p01, p11)< 2*0.06*0.06)
  parIntersections.push_back(DoublePair(1, 1));

}
*/
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

// Trova una possibile regione data una lista di punti di intersezione
static TRegion *findRegion(VIList<Intersection> &intList, Intersection *p1,
                           IntersectedStroke *p2, bool minimizeEdges) {
  TRegion *r    = new TRegion();
  int currStyle = 0;

  IntersectedStroke *pStart = p2;
  Intersection *nextp1;
  IntersectedStroke *nextp2;

  // Cicla finche' t2 non punta ad uno stroke gia' visitato
  while (!p2->m_visited) {
    p2->m_visited = true;

    // Ciclo finche' lo stroke puntato da it2 non ha un successivo punto di
    // intersezione
    do {
      p2 = p2->next();
      if (!p2)  // uso la lista come se fosse circolare
        p2 = p1->m_strokeList.first();
      if (!p2) {
        delete r;
        return 0;
      }

    } while (!p2->m_nextIntersection);

    nextp1 = p2->m_nextIntersection;
    nextp2 = p2->m_nextStroke;

    // Viene controllato e sistemato lo stile degli stroke
    if (p2->m_edge.m_styleId != 0) {
      if (currStyle == 0)
        currStyle = p2->m_edge.m_styleId;
      else if (p2->m_edge.m_styleId != currStyle) {
        currStyle = p2->m_edge.m_styleId;
        for (UINT i                = 0; i < r->getEdgeCount(); i++)
          r->getEdge(i)->m_styleId = currStyle;
      }
    } else
      p2->m_edge.m_styleId = currStyle;

    // Aggiunge lo stroke puntato da p2 alla regione
    r->addEdge(&p2->m_edge, minimizeEdges);

    if (nextp2 == pStart) return r;

    p1 = nextp1;
    p2 = nextp2;
  }

  delete r;
  return 0;
}

//-----------------------------------------------------------------------------
/*
bool areEqualRegions(const TRegion& r1, const TRegion& r2)
{
if (r1.getBBox()!=r2.getBBox())
  return false;
if (r1.getEdgeCount()!=r2.getEdgeCount())
  return false;

for (UINT i=0; i<r1.getEdgeCount(); i++)
  {
  TEdge *e1 = r1.getEdge(i);
  for (j=0; j<r2.getEdgeCount(); j++)
    {
    TEdge *e2 = r2.getEdge(j);
    if (e1->m_s==e2->m_s &&
        std::min(e1->m_w0, e1->m_w1)==std::min(e2->m_w0, e2->m_w1) &&
        std::max(e1->m_w0, e1->m_w1)==std::max(e2->m_w0, e2->m_w1))
      {
      if (e1->m_styleId && !e2->m_styleId)
        e2->m_styleId=e1->m_styleId;
      else if (e2->m_styleId && !e1->m_styleId)
        e1->m_styleId=e2->m_styleId;
      break;
      }
    }
  if (j==r2.getEdgeCount())  //e1 non e' uguale a nessun edge di r2
    return false;
  }


return true;
}
*/
//-----------------------------------------------------------------------------
/*
bool isMetaRegion(const TRegion& r1, const TRegion& r2)
{
if (areEqualRegions(r1, r2))
    return true;

for (UINT i=0; i<r1.getRegionCount(); i++)
  {
  if (isMetaRegion(*r1.getRegion(i), r2))
    return true;
  }
return false;
}

//-----------------------------------------------------------------------------

bool isMetaRegion(const vector<TRegion*>& m_regions, const TRegion& r)
{
for (UINT i=0; i<m_regions.size(); i++)
  if (isMetaRegion(*(m_regions[i]), r))
    return true;

return false;
}

//-----------------------------------------------------------------------------
*/

class TRegionClockWiseFormula final : public TRegionFeatureFormula {
private:
  double m_quasiArea;

public:
  TRegionClockWiseFormula() : m_quasiArea(0) {}

  void update(const TPointD &p1, const TPointD &p2) override {
    m_quasiArea += (p2.y + p1.y) * (p1.x - p2.x);
  }

  bool isClockwise() { return m_quasiArea > 0.1; }
};

//----------------------------------------------------------------------------------------------

void computeRegionFeature(const TRegion &r, TRegionFeatureFormula &formula) {
  TPointD p, pOld /*, pAux*/;
  int pointAdded = 0;

  int size = r.getEdgeCount();

  if (size == 0) return;

  // if (size<2)
  //  return !isMetaRegion(regions, r);

  int firstControlPoint;
  int lastControlPoint;

  TEdge *e = r.getEdge(size - 1);
  pOld     = e->m_s->getPoint(e->m_w1);

  for (int i = 0; i < size; i++) {
    TEdge *e          = r.getEdge(i);
    TStroke *s        = e->m_s;
    firstControlPoint = s->getControlPointIndexAfterParameter(e->m_w0);
    lastControlPoint  = s->getControlPointIndexAfterParameter(e->m_w1);

    p = s->getPoint(e->m_w0);
    formula.update(pOld, p);

    pOld = p;
    pointAdded++;
    if (firstControlPoint <= lastControlPoint) {
      if (firstControlPoint & 0x1) firstControlPoint++;
      if (lastControlPoint - firstControlPoint <=
          2)  /// per evitare di avere troppi pochi punti....
      {
        p = s->getPoint(0.333333 * e->m_w0 + 0.666666 * e->m_w1);
        formula.update(pOld, p);

        pOld = p;
        pointAdded++;
        p = s->getPoint(0.666666 * e->m_w0 + 0.333333 * e->m_w1);
        formula.update(pOld, p);

        pOld = p;
        pointAdded++;
      } else
        for (int j = firstControlPoint; j < lastControlPoint; j += 2) {
          p = s->getControlPoint(j);
          formula.update(pOld, p);
          pOld = p;
          pointAdded++;
        }
    } else {
      firstControlPoint--;  // this case, getControlPointIndexBEFOREParameter
      lastControlPoint--;
      if (firstControlPoint & 0x1) firstControlPoint--;
      if (firstControlPoint - lastControlPoint <=
          2)  /// per evitare di avere troppi pochi punti....
      {
        p = s->getPoint(0.333333 * e->m_w0 + 0.666666 * e->m_w1);
        formula.update(pOld, p);
        pOld = p;
        pointAdded++;
        p = s->getPoint(0.666666 * e->m_w0 + 0.333333 * e->m_w1);
        formula.update(pOld, p);
        pOld = p;
        pointAdded++;
      } else
        for (int j = firstControlPoint; j > lastControlPoint; j -= 2) {
          p = s->getControlPoint(j);
          formula.update(pOld, p);
          pOld = p;
          pointAdded++;
        }
    }
    p = s->getPoint(e->m_w1);
    formula.update(pOld, p);
    pOld = p;
    pointAdded++;
  }
  assert(pointAdded >= 4);
}

//----------------------------------------------------------------------------------

static bool isValidArea(const TRegion &r) {
  TRegionClockWiseFormula formula;
  computeRegionFeature(r, formula);
  return formula.isClockwise();
}

#ifdef LEVO

bool isValidArea(const vector<TRegion *> &regions, const TRegion &r) {
  double area = 0.0;
  TPointD p, pOld /*, pAux*/;
  int pointAdded = 0;

  int size = r.getEdgeCount();

  if (size == 0) return false;

  // if (size<2)
  //  return !isMetaRegion(regions, r);

  int firstControlPoint;
  int lastControlPoint;

  TEdge *e = r.getEdge(size - 1);
  pOld     = e->m_s->getPoint(e->m_w1);

  for (int i = 0; i < size; i++) {
    TEdge *e          = r.getEdge(i);
    TStroke *s        = e->m_s;
    firstControlPoint = s->getControlPointIndexAfterParameter(e->m_w0);
    lastControlPoint  = s->getControlPointIndexAfterParameter(e->m_w1);

    p = s->getPoint(e->m_w0);
    area += (p.y + pOld.y) * (pOld.x - p.x);
    pOld = p;
    pointAdded++;
    if (firstControlPoint <= lastControlPoint) {
      if (firstControlPoint & 0x1) firstControlPoint++;
      if (lastControlPoint - firstControlPoint <=
          2)  /// per evitare di avere troppi pochi punti....
      {
        p = s->getPoint(0.333333 * e->m_w0 + 0.666666 * e->m_w1);
        area += (p.y + pOld.y) * (pOld.x - p.x);
        pOld = p;
        pointAdded++;
        p = s->getPoint(0.666666 * e->m_w0 + 0.333333 * e->m_w1);
        area += (p.y + pOld.y) * (pOld.x - p.x);
        pOld = p;
        pointAdded++;
      } else
        for (int j = firstControlPoint; j < lastControlPoint; j += 2) {
          p = s->getControlPoint(j);
          area += (p.y + pOld.y) * (pOld.x - p.x);
          pOld = p;
          pointAdded++;
        }
    } else {
      firstControlPoint--;  // this case, getControlPointIndexBEFOREParameter
      lastControlPoint--;
      if (firstControlPoint & 0x1) firstControlPoint--;
      if (firstControlPoint - lastControlPoint <=
          2)  /// per evitare di avere troppi pochi punti....
      {
        p = s->getPoint(0.333333 * e->m_w0 + 0.666666 * e->m_w1);
        area += (p.y + pOld.y) * (pOld.x - p.x);
        pOld = p;
        pointAdded++;
        p = s->getPoint(0.666666 * e->m_w0 + 0.333333 * e->m_w1);
        area += (p.y + pOld.y) * (pOld.x - p.x);
        pOld = p;
        pointAdded++;
      } else
        for (int j = firstControlPoint; j > lastControlPoint; j -= 2) {
          p = s->getControlPoint(j);
          area += (p.y + pOld.y) * (pOld.x - p.x);
          pOld = p;
          pointAdded++;
        }
    }
    p = s->getPoint(e->m_w1);
    area += (p.y + pOld.y) * (pOld.x - p.x);
    pOld = p;
    pointAdded++;
  }
  assert(pointAdded >= 4);

  return area > 0.5;
}

#endif

//-----------------------------------------------------------------------------

void transferColors(const list<TEdge *> &oldList, const list<TEdge *> &newList,
                    bool isStrokeChanged, bool isFlipped, bool overwriteColor);

//-----------------------------------------------------------------------------
static void printStrokes1(vector<VIStroke *> &v, int size) {
  UINT i = 0;
  ofstream of("C:\\temp\\strokes.txt");

  for (i = 0; i < (UINT)size; i++) {
    TStroke *s = v[i]->m_s;
    of << "***stroke " << i << endl;
    for (UINT j = 0; j < (UINT)s->getChunkCount(); j++) {
      const TThickQuadratic *q = s->getChunk(j);

      of << "   s0 " << q->getP0() << endl;
      of << "   s1 " << q->getP1() << endl;
      of << "   s2 " << q->getP2() << endl;
      of << "****** " << endl;
    }
    of << endl;
  }
  for (i = size; i < v.size(); i++) {
    TStroke *s = v[i]->m_s;
    of << "***Autostroke " << i << endl;
    of << "s0 " << s->getPoint(0.0) << endl;
    of << "s1 " << s->getPoint(1.0) << endl;
    of << endl;
  }
}

//-----------------------------------------------------------------------------
void printStrokes1(vector<VIStroke *> &v, int size);

// void testHistory();

// Trova le regioni in una TVectorImage
int TVectorImage::Imp::computeRegions() {
#ifdef NEW_REGION_FILL
  return 0;
#endif

#if defined(_DEBUG) && !defined(MACOSX)
  TStopWatch stopWatch;
  stopWatch.start(true);
#endif

  // testHistory();

  if (!m_computeRegions) return 0;

  QMutexLocker sl(m_mutex);

  /*if (m_intersectionData->m_computedAlmostOnce)
{
UINT i,n=m_strokes.size();
  vector<int> vv(n);
for( i=0; i<n;++i) vv[i] = i;
m_intersectionData->m_computedAlmostOnce = true;
notifyChangedStrokes(vv,vector<TStroke*>(), false);

return true;
}*/

  // g_autocloseTolerance = m_autocloseTolerance;

  // Cancella le regioni gia' esistenti per ricalcolarle
  clearPointerContainer(m_regions);
  m_regions.clear();

  // Controlla che ci siano degli stroke
  if (m_strokes.empty()) {
#if defined(_DEBUG) && !defined(MACOSX)
    stopWatch.stop();
#endif
    return 0;
  }

  // Inizializza la lista di intersezioni intList
  m_computedAlmostOnce          = true;
  VIList<Intersection> &intList = m_intersectionData->m_intList;
  cleanIntersectionMarks(intList);

  // calcolo struttura delle intersezioni
  int added = 0, notAdded = 0;
  int strokeSize;
  strokeSize = computeIntersections();

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = intList.first(); p1; p1 = p1->next())
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) p2->m_edge.m_r = 0;

  for (p1 = intList.first(); p1; p1 = p1->next()) {
    // Controlla che il punto in questione non sia isolato
    if (p1->m_numInter == 0) continue;

    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) {
      TRegion *region;

      // se lo stroke non unisce due punti di intersezione
      // non lo considero e vado avanti con un altro stroke
      if (!p2->m_nextIntersection) continue;

      // Se lo stroke puntato da t2 non e' stato ancora visitato, trova una
      // regione
      if (!p2->m_visited &&
          (region = ::findRegion(intList, p1, p2, m_minimizeEdges))) {
        // Se la regione e' valida la aggiunge al vettore delle regioni
        if (isValidArea(*region)) {
          added++;

          addRegion(region);

          // Lega ogni ramo della regione alla regione di appartenenza
          for (UINT i = 0; i < region->getEdgeCount(); i++) {
            TEdge *e = region->getEdge(i);
            e->m_r   = region;
            if (e->m_index >= 0) m_strokes[e->m_index]->addEdge(e);
          }
        } else  // Se la regione non e' valida viene scartata
        {
          notAdded++;
          delete region;
        }
      }
    }
  }

  if (!m_notIntersectingStrokes) {
    UINT i;
    for (i = 0; i < m_intersectionData->m_intersectedStrokeArray.size(); i++) {
      if (!m_strokes[m_intersectionData->m_intersectedStrokeArray[i].m_index]
               ->m_edgeList.empty())
        transferColors(
            m_intersectionData->m_intersectedStrokeArray[i].m_edgeList,
            m_strokes[m_intersectionData->m_intersectedStrokeArray[i].m_index]
                ->m_edgeList,
            false, false, true);
      clearPointerContainer(
          m_intersectionData->m_intersectedStrokeArray[i].m_edgeList);
      m_intersectionData->m_intersectedStrokeArray[i].m_edgeList.clear();
    }
    m_intersectionData->m_intersectedStrokeArray.clear();
  }

  assert(m_intersectionData->m_intersectedStrokeArray.empty());

  // tolgo i segmenti aggiunti con l'autoclose
  vector<VIStroke *>::iterator it = m_strokes.begin();
  advance(it, strokeSize);
  m_strokes.erase(it, m_strokes.end());

  m_areValidRegions = true;

#if defined(_DEBUG)
  checkRegions(m_regions);
#endif

  return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/*
class Branch
  {
        TEdge m_edge;
  bool m_out, m_visited;
        Branch *m_next;
        Branch *m_nextBranch;
        Intersection* m_intersection;

        public:
          Branch* next()
                  {
                        assert(m_intersection);
                        return m_next?m_next:m_intersection->m_branchList;
                        }
        }


class Intersection
  {
        private:
        TPointD m_intersectionPoint;
  int m_intersectionCount;
  Branch *m_branchList;
        Intersection* m_next;
  list<IntersectedStroke> m_strokeList;
        public:
  AddIntersection(int index0, int index1, DoublePair intersectionValues);

        }

*/

#ifdef _DEBUG

void TVectorImage::Imp::checkRegions(const std::vector<TRegion *> &regions) {
  for (UINT i = 0; i < regions.size(); i++) {
    TRegion *r = regions[i];
    UINT j     = 0;
    for (j = 0; j < r->getEdgeCount(); j++) {
      TEdge *e = r->getEdge(j);
      // assert(areSameGroup(e->m_index, false,
      // ==m_strokes[r->getEdge(0)->m_index]->m_groupId);
      assert(e->m_r == r);
      // if (e->m_s->isSelfLoop())
      //  {
      //  assert(r->getEdgeCount()==1);
      // assert(r->getSubregionCount()==0);
      //  }
      // if (j>0)
      //  assert(!e->m_s->isSelfLoop());
    }
    if (r->getSubregionCount() > 0) {
      std::vector<TRegion *> aux(r->getSubregionCount());
      for (j = 0; j < r->getSubregionCount(); j++) aux[j] = r->getSubregion(j);
      checkRegions(aux);
    }
  }
}

#endif

namespace {

inline TGroupId getGroupId(TRegion *r, const std::vector<VIStroke *> &strokes) {
  for (UINT i = 0; i < r->getEdgeCount(); i++)
    if (r->getEdge(i)->m_index >= 0)
      return strokes[r->getEdge(i)->m_index]->m_groupId;
  return TGroupId();
}
}

//------------------------------------------------------------

TRegion *TVectorImage::findRegion(const TRegion &region) const {
  TRegion *ret = 0;

  for (std::vector<TRegion *>::iterator it = m_imp->m_regions.begin();
       it != m_imp->m_regions.end(); ++it)
    if ((ret = (*it)->findRegion(region)) != 0) return ret;

  return 0;
}

//------------------------------------------------------------

void TVectorImage::Imp::addRegion(TRegion *region) {
  for (std::vector<TRegion *>::iterator it = m_regions.begin();
       it != m_regions.end(); ++it) {
    if (getGroupId(region, m_strokes) != getGroupId(*it, m_strokes)) continue;

    if (region->contains(**it)) {
      // region->addSubregion(*it);
      region->addSubregion(*it);
      it = m_regions.erase(it);
      while (it != m_regions.end()) {
        if (region->contains(**it)) {
          region->addSubregion(*it);
          // region->addSubregion(*it);
          it = m_regions.erase(it);
        } else
          it++;
      }

      m_regions.push_back(region);
      return;
    } else if ((*it)->contains(*region)) {
      (*it)->addSubregion(region);
      //(*it)->addSubregion(region);
      return;
    }
  }
  m_regions.push_back(region);
}

//-----------------------------------------------------------------------------

void TVectorImage::replaceStroke(int index, TStroke *newStroke) {
  if ((int)m_imp->m_strokes.size() <= index) return;

  delete m_imp->m_strokes[index]->m_s;
  m_imp->m_strokes[index]->m_s = newStroke;

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_imp->m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2      = p2->next())
      if (p2->m_edge.m_index == index) p2->m_edge.m_s = newStroke;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void TVectorImage::Imp::moveStroke(int fromIndex, int moveBefore) {
  assert((int)m_strokes.size() > fromIndex);
  assert((int)m_strokes.size() >= moveBefore);

#ifdef _DEBUG
  checkIntersections();
#endif

  VIStroke *vi = m_strokes[fromIndex];

  m_strokes.erase(m_strokes.begin() + fromIndex);

  std::vector<VIStroke *>::iterator it = m_strokes.begin();
  if (fromIndex < moveBefore)
    advance(it, moveBefore - 1);
  else
    advance(it, moveBefore);

  m_strokes.insert(it, vi);

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) {
      if (fromIndex < moveBefore) {
        if (p2->m_edge.m_index == fromIndex)
          p2->m_edge.m_index = moveBefore - 1;
        else if (p2->m_edge.m_index > fromIndex &&
                 p2->m_edge.m_index < moveBefore)
          p2->m_edge.m_index--;
      } else  //(fromIndex>moveBefore)
      {
        if (p2->m_edge.m_index == fromIndex)
          p2->m_edge.m_index = moveBefore;
        else if (p2->m_edge.m_index >= moveBefore &&
                 p2->m_edge.m_index < fromIndex)
          p2->m_edge.m_index++;
      }
    }

#ifdef _DEBUG
  checkIntersections();
#endif
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::reindexEdges(UINT strokeIndex) {
  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next()) {
      assert(p2->m_edge.m_index != (int)strokeIndex || p2->m_edge.m_index < 0);
      if (p2->m_edge.m_index > (int)strokeIndex) p2->m_edge.m_index--;
    }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::reindexEdges(const vector<int> &indexes,
                                     bool areAdded) {
  int i;
  int size = indexes.size();

  if (size == 0) return;

#ifdef _DEBUG
  for (i = 0; i < size; i++) assert(i == 0 || indexes[i - 1] < indexes[i]);
#endif

  int min = (int)indexes[0];

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = p1->m_strokeList.first(); p2; p2 = p2->next()) {
      if (areAdded) {
        if (p2->m_edge.m_index < min)
          continue;
        else
          for (i = size - 1; i >= 0; i--)
            if (p2->m_edge.m_index >= (int)indexes[i] - i) {
              p2->m_edge.m_index += i + 1;
              break;
            }
      } else {
        if (p2->m_edge.m_index < min)
          continue;
        else
          for (i = size - 1; i >= 0; i--)
            if (p2->m_edge.m_index > (int)indexes[i]) {
              p2->m_edge.m_index -= i + 1;
              break;
            }
      }
      // assert(it2->m_edge.m_index!=1369);
    }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::insertStrokeAt(VIStroke *vs, int strokeIndex,
                                       bool recomputeRegions) {
  list<TEdge *> oldEdgeList, emptyList;

  if (m_computedAlmostOnce && recomputeRegions) {
    oldEdgeList = vs->m_edgeList;
    vs->m_edgeList.clear();
  }

  assert(strokeIndex >= 0 && strokeIndex <= (int)m_strokes.size());

  vector<VIStroke *>::iterator it = m_strokes.begin();
  advance(it, strokeIndex);

  vs->m_isNewForFill = true;
  m_strokes.insert(it, vs);

  if (!m_computedAlmostOnce) return;

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next())
      if (p2->m_edge.m_index >= (int)strokeIndex) p2->m_edge.m_index++;

  if (!recomputeRegions) return;

  computeRegions();
  transferColors(oldEdgeList, m_strokes[strokeIndex]->m_edgeList, true, false,
                 true);

  /*
#ifdef _DEBUG
checkIntersections();
#endif
*/
}

//-----------------------------------------------------------------------------

void invalidateRegionPropAndBBox(TRegion *reg) {
  for (UINT regId = 0; regId != reg->getSubregionCount(); regId++) {
    invalidateRegionPropAndBBox(reg->getSubregion(regId));
  }
  reg->invalidateProp();
  reg->invalidateBBox();
}

void TVectorImage::transform(const TAffine &aff, bool doChangeThickness) {
  UINT i;
  for (i = 0; i < m_imp->m_strokes.size(); ++i)
    m_imp->m_strokes[i]->m_s->transform(aff, doChangeThickness);

  map<int, VIStroke *>::iterator it =
      m_imp->m_intersectionData->m_autocloseMap.begin();
  for (; it != m_imp->m_intersectionData->m_autocloseMap.end(); ++it)
    it->second->m_s->transform(aff, false);

  for (i = 0; i < m_imp->m_regions.size(); ++i)
    invalidateRegionPropAndBBox(m_imp->m_regions[i]);
}

//-----------------------------------------------------------------------------

#ifdef _DEBUG
#include "tvectorrenderdata.h"
#include "tgl.h"
void TVectorImage::drawAutocloses(const TVectorRenderData &rd) const {
  float width;

  glPushMatrix();
  tglMultMatrix(rd.m_aff);

  glGetFloatv(GL_LINE_WIDTH, &width);
  tglColor(TPixel(0, 255, 0, 255));
  glLineWidth(1.5);
  glBegin(GL_LINES);

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_imp->m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next()) {
      if (p2->m_edge.m_index < 0 && p2->m_edge.m_w0 == 0.0) {
        TStroke *s = p2->m_edge.m_s;
        TPointD p0 = s->getPoint(0.0);
        TPointD p1 = s->getPoint(1.0);

        glVertex2d(p0.x, p0.y);
        glVertex2d(p1.x, p1.y);
      }
    }
  glEnd();
  glLineWidth(width);

  glPopMatrix();
}

#endif

//-----------------------------------------------------------------------------

void TVectorImage::reassignStyles(map<int, int> &table) {
  UINT i;
  UINT strokeCount = getStrokeCount();
  // UINT regionCount = getRegionCount();
  for (i = 0; i < strokeCount; ++i) {
    TStroke *stroke = getStroke(i);
    int styleId     = stroke->getStyle();
    if (styleId != 0) {
      map<int, int>::iterator it = table.find(styleId);
      if (it != table.end()) stroke->setStyle(it->second);
    }
  }

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_imp->m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next())
      if (p2->m_edge.m_styleId != 0) {
        map<int, int>::iterator it = table.find(p2->m_edge.m_styleId);
        if (it != table.end()) p2->m_edge.m_styleId = it->second;
        // assert(it->second<100);
      }
}

//-----------------------------------------------------------------------------

struct TDeleteMapFunctor {
  void operator()(pair<int, VIStroke *> ptr) { delete ptr.second; }
};

IntersectionData::~IntersectionData() {
  std::for_each(m_autocloseMap.begin(), m_autocloseMap.end(),
                TDeleteMapFunctor());
}
//-----------------------------------------------------------------------------

#ifdef _DEBUG
void TVectorImage::Imp::checkIntersections() {
  // return;
  UINT i, j;

  Intersection *p1;
  IntersectedStroke *p2;

  for (i = 0, p1 = m_intersectionData->m_intList.first(); p1;
       p1 = p1->next(), i++)
    for (j = 0, p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next(), j++) {
      IntersectedStroke &is = *p2;
      assert(is.m_edge.m_styleId >= 0 && is.m_edge.m_styleId <= 1000);
      assert(is.m_edge.m_w0 >= 0 && is.m_edge.m_w0 <= 1);
      assert(is.m_edge.m_index < (int)m_strokes.size());
      if (is.m_edge.m_index >= 0) {
        assert(is.m_edge.m_s->getChunkCount() >= 0 &&
               is.m_edge.m_s->getChunkCount() <= 10000);
        assert(m_strokes[is.m_edge.m_index]->m_s == is.m_edge.m_s);
      } else
        assert(m_intersectionData->m_autocloseMap[is.m_edge.m_index]);

      if (p2->m_nextIntersection) {
        IntersectedStroke *nextStroke = p2->m_nextStroke;
        assert(nextStroke->m_nextIntersection == p1);
        assert(nextStroke->m_nextStroke == p2);
      }
    }

  for (i = 0; i < m_strokes.size(); i++) {
    VIStroke *vs                       = m_strokes[i];
    list<TEdge *>::const_iterator it   = vs->m_edgeList.begin(),
                                  it_e = vs->m_edgeList.end();
    for (; it != it_e; ++it) {
      TEdge *e = *it;
      assert(e->getStyle() >= 0 && e->getStyle() <= 1000);
      assert(e->m_w0 >= 0 && e->m_w1 <= 1);
      assert(e->m_s == vs->m_s);
      assert(e->m_s->getChunkCount() >= 0 && e->m_s->getChunkCount() <= 10000);
      // assert(e->m_index<(int)m_strokes.size());   l'indice nella stroke
      // potrebbe essere non valido, non importa.
      // assert(m_strokes[e->m_index]->m_s==e->m_s); deve essere buono nella
      // intersectionData
    }
  }

  for (i = 0; i < m_regions.size(); i++) {
    m_regions[i]->checkRegion();
  }
}

#endif
//-----------------------------------------------------------------------------

TStroke *TVectorImage::Imp::removeEndpoints(int strokeIndex) {
#ifdef _DEBUG
  checkIntersections();
#endif

  VIStroke *vs = m_strokes[strokeIndex];

  if (vs->m_s->isSelfLoop()) return 0;
  if (vs->m_edgeList.empty()) return 0;

  list<TEdge *>::iterator it = vs->m_edgeList.begin();

  double minW = 1.0;
  double maxW = 0.0;
  for (; it != vs->m_edgeList.end(); ++it) {
    minW = std::min({minW - 0.00002, (*it)->m_w0, (*it)->m_w1});
    maxW = std::max({maxW + 0.00002, (*it)->m_w0, (*it)->m_w1});
  }

  if (areAlmostEqual(minW, 0.0, 0.001) && areAlmostEqual(maxW, 1.0, 0.001))
    return 0;

  TStroke *oldS = vs->m_s;

  TStroke *s = new TStroke(*(vs->m_s));

  double offs = s->getLength(minW);

  TStroke s0, s1, final;

  if (!areAlmostEqual(maxW, 1.0, 0.001)) {
    s->split(maxW, s0, s1);
  } else
    s0 = *s;

  if (!areAlmostEqual(minW, 0.0, 0.001)) {
    double newW = (maxW == 1.0) ? minW : s0.getParameterAtLength(offs);
    s0.split(newW, s1, final);
  } else
    final = s0;

  vs->m_s = new TStroke(final);
  vs->m_s->setStyle(oldS->getStyle());

  for (it = vs->m_edgeList.begin(); it != vs->m_edgeList.end(); ++it) {
    (*it)->m_w0 =
        vs->m_s->getParameterAtLength(s->getLength((*it)->m_w0) - offs);
    (*it)->m_w1 =
        vs->m_s->getParameterAtLength(s->getLength((*it)->m_w1) - offs);
    (*it)->m_s = vs->m_s;
  }

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next()) {
      if (p2->m_edge.m_s == oldS) {
        p2->m_edge.m_w0 =
            vs->m_s->getParameterAtLength(s->getLength(p2->m_edge.m_w0) - offs);
        p2->m_edge.m_w1 =
            vs->m_s->getParameterAtLength(s->getLength(p2->m_edge.m_w1) - offs);
        p2->m_edge.m_s = vs->m_s;
      }
    }

#ifdef _DEBUG
  checkIntersections();
#endif

  return oldS;
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::restoreEndpoints(int index, TStroke *oldStroke) {
#ifdef _DEBUG
  checkIntersections();
#endif

  VIStroke *vs = m_strokes[index];

  TStroke *s = vs->m_s;
  TPointD p  = s->getPoint(0.0);

  double offs = oldStroke->getLength(oldStroke->getW(p));

  vs->m_s = oldStroke;

  list<TEdge *>::iterator it = vs->m_edgeList.begin();
  for (; it != vs->m_edgeList.end(); ++it) {
    (*it)->m_w0 =
        vs->m_s->getParameterAtLength(s->getLength((*it)->m_w0) + offs);
    (*it)->m_w1 =
        vs->m_s->getParameterAtLength(s->getLength((*it)->m_w1) + offs);
    (*it)->m_s = vs->m_s;
  }

  Intersection *p1;
  IntersectedStroke *p2;

  for (p1 = m_intersectionData->m_intList.first(); p1; p1 = p1->next())
    for (p2 = (*p1).m_strokeList.first(); p2; p2 = p2->next()) {
      if (p2->m_edge.m_s == s) {
        p2->m_edge.m_w0 =
            vs->m_s->getParameterAtLength(s->getLength(p2->m_edge.m_w0) + offs);
        p2->m_edge.m_w1 =
            vs->m_s->getParameterAtLength(s->getLength(p2->m_edge.m_w1) + offs);
        p2->m_edge.m_s = vs->m_s;
      }
    }

  delete s;

#ifdef _DEBUG
  checkIntersections();
#endif
}
