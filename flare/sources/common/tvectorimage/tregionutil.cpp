

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
         ((long)(x * REGION_COMPUTING_PRECISION));
}

inline TThickPoint myRound(const TThickPoint &p) {
  return TThickPoint(myRound(p.x), myRound(p.y), p.thick);
}

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
                      const vector<DoublePair> &intersections, int numStrokes);

void addIntersection(IntersectionData &intData, const vector<VIStroke *> &s,
                     int ii, int jj, DoublePair intersections, int strokeSize);

//-----------------------------------------------------------------------------

bool sortBBox(const TStroke *s1, const TStroke *s2) {
  return s1->getBBox().x0 < s2->getBBox().x0;
}

//-----------------------------------------------------------------------------

void cleanIntersectionMarks(list<Intersection> &interList) {
  for (list<Intersection>::iterator it1 = interList.begin();
       it1 != interList.end(); it1++)
    for (list<IntersectedStroke>::iterator it2 = (*it1).m_strokeList.begin();
         it2 != (*it1).m_strokeList.end(); it2++) {
      it2->m_visited =
          false;  // Ogni ramo della lista viene messo nella condizione
                  // di poter essere visitato

      if (it2->m_nextIntersection != interList.end()) {
        it2->m_nextIntersection =
            interList.end();  // pezza tremenda, da togliere!!!
        it1->m_numInter--;
      }
    }
}

//-----------------------------------------------------------------------------

void cleanNextIntersection(list<Intersection> &interList, TStroke *s) {
  for (list<Intersection>::iterator it1 = interList.begin();
       it1 != interList.end(); it1++)
    for (list<IntersectedStroke>::iterator it2 = (*it1).m_strokeList.begin();
         it2 != (*it1).m_strokeList.end(); it2++)
      if (it2->m_edge.m_s == s) {
        // if (it2->m_nextIntersection==NULL)
        //  return; //gia' ripulita prima
        if (it2->m_nextIntersection != interList.end()) {
          it2->m_nextIntersection = interList.end();
          it1->m_numInter--;
        }
        it2->m_nextStroke = (*it1).m_strokeList.end();
      }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::eraseEdgeFromStroke(
    list<IntersectedStroke>::iterator it2) {
  if (it2->m_edge.m_index >=
      0)  // elimino il puntatore all'edge nella lista della VIStroke
  {
    VIStroke *s;
    s = m_strokes[it2->m_edge.m_index];
    assert(s->m_s == it2->m_edge.m_s);
    list<TEdge *>::iterator iit  = s->m_edgeList.begin(),
                            it_e = s->m_edgeList.end();

    for (; iit != it_e; ++iit)
      if ((*iit)->m_w0 == it2->m_edge.m_w0 &&
          (*iit)->m_w1 == it2->m_edge.m_w1) {
        assert((*iit)->m_toBeDeleted == false);
        s->m_edgeList.erase(iit);
        return;
      }
  }
}

//-----------------------------------------------------------------------------

list<IntersectedStroke>::iterator TVectorImage::Imp::eraseBranch(
    list<Intersection>::iterator it1, list<IntersectedStroke>::iterator it2) {
  // list<Intersection>::iterator iit1;
  // list<IntersectedStroke>::iterator iit2;
  list<Intersection> &intList = m_intersectionData.m_intList;

  if (it2->m_nextIntersection != intList.end()) {
    list<Intersection>::iterator nextInt         = it2->m_nextIntersection;
    list<IntersectedStroke>::iterator nextStroke = it2->m_nextStroke;
    assert(nextStroke->m_nextIntersection == it1);
    assert(nextStroke->m_nextStroke == it2);
    assert(nextStroke != it2);

    // nextStroke->m_nextIntersection = intList.end();
    // nextStroke->m_nextStroke = nextInt->m_strokeList.end();

    if (nextStroke->m_nextIntersection != intList.end()) {
      nextStroke->m_nextIntersection = intList.end();
      nextInt->m_numInter--;
    }
    // nextInt->m_strokeList.erase(nextStroke);//non posso cancellarla, puo'
    // servire in futuro!
  }
  if (it2->m_nextIntersection != intList.end()) it1->m_numInter--;

  eraseEdgeFromStroke(it2);

  it2->m_edge.m_w0 = it2->m_edge.m_w1 = -3;
  it2->m_edge.m_index                 = -3;
  it2->m_edge.m_s                     = 0;
  it2->m_edge.m_styleId               = -3;

  list<IntersectedStroke>::iterator ret = (*it1).m_strokeList.erase(it2);

  return ret;
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::eraseDeadIntersections() {
  list<Intersection>::iterator it;

  for (it = m_intersectionData.m_intList.begin();
       it != m_intersectionData.m_intList.end();)  // la faccio qui, e non nella
                                                   // eraseIntersection. vedi
                                                   // commento li'.
  {
    list<Intersection> &intList = m_intersectionData.m_intList;

    if (it->m_strokeList.size() == 1) {
      eraseBranch(it, (*it).m_strokeList.begin());
      assert(it->m_strokeList.empty());
      it = intList.erase(it);
    } else if (it->m_strokeList.size() == 2 &&
               ((*it).m_strokeList.front().m_edge.m_s ==
                    (*it).m_strokeList.back().m_edge.m_s &&
                (*it).m_strokeList.front().m_edge.m_w0 ==
                    (*it).m_strokeList.back().m_edge.m_w0))  // intersezione
                                                             // finta
    {
      list<IntersectedStroke>::iterator it1 = it->m_strokeList.begin(), iit1,
                                        iit2;
      list<IntersectedStroke>::iterator it2 = it1;
      it2++;

      eraseEdgeFromStroke(it1);
      eraseEdgeFromStroke(it2);

      iit1 = (it1->m_nextIntersection == intList.end()) ? NULL_ITER
                                                        : it1->m_nextStroke;
      iit2 = (it2->m_nextIntersection == intList.end()) ? NULL_ITER
                                                        : it2->m_nextStroke;

      if (iit1 != NULL_ITER && iit2 != NULL_ITER) {
        iit1->m_edge.m_w1 = iit2->m_edge.m_w0;
        iit2->m_edge.m_w1 = iit1->m_edge.m_w0;
      }
      if (iit1 != NULL_ITER) {
        iit1->m_nextStroke       = iit2;
        iit1->m_nextIntersection = it2->m_nextIntersection;
        if (iit1->m_nextIntersection == intList.end())
          it1->m_nextIntersection->m_numInter--;
      }

      if (iit2 != NULL_ITER) {
        iit2->m_nextStroke       = iit1;
        iit2->m_nextIntersection = it1->m_nextIntersection;
        if (iit2->m_nextIntersection == intList.end())
          it2->m_nextIntersection->m_numInter--;
      }

      it->m_strokeList.clear();
      it->m_numInter = 0;
      it             = intList.erase(it);
    } else
      ++it;
  }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::doEraseIntersection(int index,
                                            vector<int> *toBeDeleted) {
  list<Intersection> &interList = m_intersectionData.m_intList;

  list<Intersection>::iterator it1 = interList.begin();
  TStroke *deleteIt                = 0;

  while (it1 != interList.end()) {
    bool removeAutocloses = false;

    list<IntersectedStroke>::iterator it2 = (*it1).m_strokeList.begin();

    while (it2 != (*it1).m_strokeList.end()) {
      IntersectedStroke &is = *it2;

      if (is.m_edge.m_index == index) {
        if (is.m_edge.m_index >= 0)
          // if (!is.m_autoclose && (is.m_edge.m_w0==1 || is.m_edge.m_w0==0))
          removeAutocloses = true;
        else
          deleteIt = is.m_edge.m_s;
        it2        = eraseBranch(it1, it2);
      } else
        ++it2;
      // checkInterList(interList);
    }
    if (removeAutocloses)  // se ho tolto una stroke dall'inter corrente, tolgo
                           // tutti le stroke di autclose che partono da qui
    {
      assert(toBeDeleted);
      for (it2 = (*it1).m_strokeList.begin(); it2 != (*it1).m_strokeList.end();
           it2++)
        if (it2->m_edge.m_index < 0 &&
            (it2->m_edge.m_w0 == 1 || it2->m_edge.m_w0 == 0))
          toBeDeleted->push_back(it2->m_edge.m_index);
    }

    if ((*it1).m_strokeList.empty())
      it1 = interList.erase(it1);
    else
      it1++;
  }

  if (deleteIt) delete deleteIt;
}

//-----------------------------------------------------------------------------

UINT TVectorImage::Imp::getFillData(IntersectionBranch *&v) {
  // print(m_intersectionData.m_intList, "C:\\temp\\intersectionPrimaSave.txt");

  list<Intersection> &intList = m_intersectionData.m_intList;
  if (intList.empty()) return 0;

  list<Intersection>::iterator it1;
  list<IntersectedStroke>::iterator it2;
  UINT currInt = 0;
  vector<UINT> branchesBefore(intList.size() + 1);

  branchesBefore[0] = 0;
  UINT count = 0, size = 0;

  for (it1 = intList.begin(); it1 != intList.end(); ++it1, currInt++) {
    UINT strokeListSize = it1->m_strokeList.size();
    size += strokeListSize;
    branchesBefore[currInt + 1] = branchesBefore[currInt] + strokeListSize;
  }

  v       = new IntersectionBranch[size];
  currInt = 0;

  for (it1 = intList.begin(); it1 != intList.end(); ++it1, currInt++) {
    UINT currBranch = 0;
    for (it2 = it1->m_strokeList.begin(); it2 != it1->m_strokeList.end();
         ++it2, currBranch++) {
      IntersectionBranch &b = v[count];
      b.m_currInter         = currInt;
      b.m_strokeIndex       = it2->m_edge.m_index;
      b.m_w                 = it2->m_edge.m_w0;
      b.m_style             = it2->m_edge.m_styleId;
      // assert(b.m_style<100);
      b.m_gettingOut = it2->m_gettingOut;
      if (it2->m_nextIntersection == intList.end())
        b.m_nextBranch = count;
      else {
        UINT distInt = std::distance(intList.begin(), it2->m_nextIntersection);
        UINT distBranch = std::distance(
            it2->m_nextIntersection->m_strokeList.begin(), it2->m_nextStroke);

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
TStroke *reconstructAutocloseStroke(list<Intersection> &intList,
                                    list<Intersection>::iterator it1,
                                    list<IntersectedStroke>::iterator it2)

{
  bool found                        = false;
  list<Intersection>::iterator iit1 = it1;
  list<IntersectedStroke>::iterator iit2;
  iit1++;
  // vector<TEdge*> vapp;
  for (; !found && iit1 != intList.end(); iit1++) {
    for (iit2 = iit1->m_strokeList.begin();
         !found && iit2 != iit1->m_strokeList.end(); iit2++) {
      if (it2->m_edge.m_index == iit2->m_edge.m_index) {
        if ((iit2->m_edge.m_w0 == 1 && it2->m_edge.m_w0 == 0) ||
            (iit2->m_edge.m_w0 == 0 && it2->m_edge.m_w0 == 1)) {
          found = true;
          vector<TPointD> v;
          if (it2->m_edge.m_w0 == 0) {
            v.push_back(it1->m_intersection);
            v.push_back(iit1->m_intersection);
          } else {
            v.push_back(iit1->m_intersection);
            v.push_back(it1->m_intersection);
          }
          it2->m_edge.m_s = iit2->m_edge.m_s = new TStroke(v);
          // for (UINT ii=0; ii<vapp.size(); ii++)
          // vapp[ii]->m_s = it2->m_edge.m_s;
        }
        // else if (iit2->m_edge.m_w0!=0 && iit2->m_edge.m_w0!=1)
        //  vapp.push_back(&(iit2->m_edge));
      }
    }
  }
  assert(found);
  if (!found) it2->m_edge.m_s = 0;

  return it2->m_edge.m_s;
}

}  // namespace
//-----------------------------------------------------------------------------

void TVectorImage::Imp::setFillData(IntersectionBranch *v, UINT branchCount) {
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

  list<Intersection> &intList = m_intersectionData.m_intList;
  clearPointerContainer(m_regions);
  m_regions.clear();
  intList.clear();
  list<Intersection>::iterator currInt;
  list<IntersectedStroke>::iterator currBranch;

  vector<UINT> branchesBefore(v[branchCount - 1].m_currInter + 1);

  UINT i = 0;
  for (; i < branchCount; i++) {
    const IntersectionBranch &b = v[i];
    if (i == 0 || v[i].m_currInter != v[i - 1].m_currInter) {
      assert(i == 0 || v[i].m_currInter == v[i - 1].m_currInter + 1);

      branchesBefore[v[i].m_currInter] = i;
      intList.push_back(Intersection());
      currInt = intList.begin();
      advance(currInt, intList.size() - 1);
    }

    IntersectedStroke is;
    currInt->m_strokeList.push_back(is);
    currBranch = currInt->m_strokeList.begin();
    advance(currBranch, currInt->m_strokeList.size() - 1);

    currBranch->m_edge.m_styleId = b.m_style;
    // assert(b.m_style<100);
    currBranch->m_edge.m_index = b.m_strokeIndex;
    if (b.m_strokeIndex >= 0)
      currBranch->m_edge.m_s = m_strokes[b.m_strokeIndex]->m_s;
    else
      currBranch->m_edge.m_s = 0;
    currBranch->m_gettingOut = b.m_gettingOut;
    currBranch->m_edge.m_w0  = b.m_w;
    currBranch->m_edge.m_w1  = v[b.m_nextBranch].m_w;
    assert(currBranch->m_edge.m_w0 >= -1e-8 &&
           currBranch->m_edge.m_w0 <= 1 + 1e-8);
    assert(currBranch->m_edge.m_w1 >= -1e-8 &&
           currBranch->m_edge.m_w1 <= 1 + 1e-8);

    if (b.m_nextBranch < i) {
      list<Intersection>::iterator it1;
      list<IntersectedStroke>::iterator it2;
      it1 = intList.begin();
      std::advance(it1, v[b.m_nextBranch].m_currInter);
      it2 = it1->m_strokeList.begin();
      assert(b.m_nextBranch - branchesBefore[v[b.m_nextBranch].m_currInter] >=
             0);

      std::advance(
          it2, b.m_nextBranch - branchesBefore[v[b.m_nextBranch].m_currInter]);

      currBranch->m_nextIntersection = it1;
      currBranch->m_nextStroke       = it2;
      it2->m_nextIntersection        = currInt;
      it2->m_nextStroke              = currBranch;

      // if (currBranch == currInt->m_strokeList.begin())
      //  currInt->m_intersection =
      //  currBranch->m_edge.m_s->getPoint(currBranch->m_edge.m_w0);

      currInt->m_numInter++;
      it1->m_numInter++;
    } else if (b.m_nextBranch == i)
      currBranch->m_nextIntersection = intList.end();
    else if (b.m_nextBranch == (std::numeric_limits<UINT>::max)()) {
      currBranch->m_nextIntersection = intList.end();
      currBranch->m_nextStroke       = currInt->m_strokeList.end();
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

  list<Intersection>::iterator it1;
  list<IntersectedStroke>::iterator it2;

  vector<UINT> toBeDeleted;

  for (it1 = intList.begin(); it1 != intList.end(); it1++)
    for (it2 = it1->m_strokeList.begin(); it2 != it1->m_strokeList.end();
         ++it2) {
      if (it2->m_edge.m_index < 0 && !it2->m_edge.m_s &&
          (it2->m_edge.m_w0 == 0 || it2->m_edge.m_w0 == 1)) {
        it2->m_edge.m_s = reconstructAutocloseStroke(intList, it1, it2);
        if (it2->m_edge.m_s)
          m_intersectionData.m_autocloseMap[it2->m_edge.m_index] =
              it2->m_edge.m_s;
        else
          toBeDeleted.push_back(it2->m_edge.m_index);
      }
    }

  for (it1 = intList.begin(); it1 != intList.end(); it1++)
    for (it2 = it1->m_strokeList.begin(); it2 != it1->m_strokeList.end();
         ++it2) {
      if (!it2->m_edge.m_s && it2->m_edge.m_index < 0) {
        it2->m_edge.m_s =
            m_intersectionData.m_autocloseMap[it2->m_edge.m_index];

        // TEdge& e = it2->m_edge;
        if (!it2->m_edge.m_s) toBeDeleted.push_back(it2->m_edge.m_index);
      }
    }

  for (i = 0; i < toBeDeleted.size(); i++) eraseIntersection(toBeDeleted[i]);

  m_areValidRegions = false;

  computeRegions();
  // print(m_intersectionData.m_intList, "C:\\temp\\intersectionDopoLoad.txt");
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::eraseIntersection(int index) {
  vector<int> autocloseStrokes;
  doEraseIntersection(index, &autocloseStrokes);

  for (UINT i = 0; i < autocloseStrokes.size(); i++) {
    doEraseIntersection(autocloseStrokes[i]);
    assert(autocloseStrokes[i] < 0);
    m_intersectionData.m_autocloseMap.erase(autocloseStrokes[i]);
  }
}
//-----------------------------------------------------------------------------

void findNearestIntersection(list<Intersection> &interList) {
  list<Intersection>::iterator i1;
  list<IntersectedStroke>::iterator i2;

  for (i1 = interList.begin(); i1 != interList.end(); i1++) {
    for (i2 = (*i1).m_strokeList.begin(); i2 != (*i1).m_strokeList.end();
         i2++) {
      if ((*i2).m_nextIntersection != interList.end())  // already set
        continue;

      int versus      = (i2->m_gettingOut) ? 1 : -1;
      double minDelta = (std::numeric_limits<double>::max)();
      list<Intersection>::iterator it1, it1Res;
      list<IntersectedStroke>::iterator it2, it2Res;

      for (it1 = i1; it1 != interList.end(); ++it1) {
        if (it1 == i1)
          it2 = i2, it2++;
        else
          it2 = (*it1).m_strokeList.begin();

        for (; it2 != (*it1).m_strokeList.end(); ++it2) {
          if ((*it2).m_edge.m_index == i2->m_edge.m_index &&
              (*it2).m_gettingOut == !i2->m_gettingOut) {
            double delta = versus * (it2->m_edge.m_w0 - i2->m_edge.m_w0);

            if (delta > 0 && delta < minDelta) {
              it1Res   = it1;
              it2Res   = it2;
              minDelta = delta;
            }
          }
        }
      }

      if (minDelta != (std::numeric_limits<double>::max)()) {
        (*it2Res).m_nextIntersection = i1;
        (*it2Res).m_nextStroke       = i2;
        (*it2Res).m_edge.m_w1        = i2->m_edge.m_w0;
        (*i2).m_nextIntersection     = it1Res;
        (*i2).m_nextStroke           = it2Res;
        (*i2).m_edge.m_w1            = it2Res->m_edge.m_w0;
        i1->m_numInter++;
        it1Res->m_numInter++;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void markDeadIntersections(list<Intersection> &intList,
                           list<Intersection>::iterator it);

// questa funzione "cuscinetto" serve perche crashava il compilatore in
// release!!!
void inline markDeadIntersectionsRic(list<Intersection> &intList,
                                     list<Intersection>::iterator it) {
  markDeadIntersections(intList, it);
}

//-----------------------------------------------------------------------------

void markDeadIntersections(list<Intersection> &intList,
                           list<Intersection>::iterator it) {
  list<IntersectedStroke>::iterator it1 = it->m_strokeList.begin();

  if (it->m_numInter == 1) {
    while (it1->m_nextIntersection == intList.end()) it1++;
    assert(it1 != it->m_strokeList.end());

    list<Intersection>::iterator nextInt         = it1->m_nextIntersection;
    list<IntersectedStroke>::iterator nextStroke = it1->m_nextStroke;

    it->m_numInter          = 0;
    it1->m_nextIntersection = intList.end();

    if (nextInt != intList.end() /*&& !nextStroke->m_dead*/) {
      nextInt->m_numInter--;
      nextStroke->m_nextIntersection = intList.end();
      markDeadIntersectionsRic(intList, nextInt);
    }
  } else if (it->m_numInter == 2)  // intersezione finta (forse)
  {
    while (it1 != it->m_strokeList.end() &&
           it1->m_nextIntersection == intList.end())
      it1++;
    assert(it1 != it->m_strokeList.end());
    list<IntersectedStroke>::iterator it2 = it1;
    it2++;
    while (it2 != it->m_strokeList.end() &&
           it2->m_nextIntersection == intList.end())
      it2++;
    assert(it2 != it->m_strokeList.end());

    if (it1->m_edge.m_s == it2->m_edge.m_s &&
        it1->m_edge.m_w0 == it2->m_edge.m_w0)  // intersezione finta
    {
      list<IntersectedStroke>::iterator iit1, iit2;
      assert(it1->m_nextIntersection != intList.end() &&
             it2->m_nextIntersection != intList.end());

      iit1 = it1->m_nextStroke;
      iit2 = it2->m_nextStroke;

      iit2->m_edge.m_w1 = iit1->m_edge.m_w0;
      iit1->m_edge.m_w1 = iit2->m_edge.m_w0;

      // if (iit1!=0)
      (*iit1).m_nextStroke = iit2;
      // if (iit2!=0)
      (*iit2).m_nextStroke = iit1;
      // if (iit1!=0)
      (*iit1).m_nextIntersection = it2->m_nextIntersection;
      // if (iit2!=0)
      (*iit2).m_nextIntersection = it1->m_nextIntersection;

      it->m_numInter          = 0;
      it1->m_nextIntersection = intList.end();
      it2->m_nextIntersection = intList.end();
    }
  }
}

//-----------------------------------------------------------------------------

// se cross val era 0, cerco di spostarmi un po' su w per vedere come sono
// orientate le tangenti agli stroke...
double nearCrossVal(TStroke *s0, double w0, TStroke *s1, double w1) {
  double ltot0 = s0->getLength();
  double ltot1 = s1->getLength();
  double dl    = tmin(ltot1, ltot0) / 1000;

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

inline void insertBranch(Intersection &interList, IntersectedStroke &item,
                         bool gettingOut) {
  if (item.m_edge.m_w0 != (gettingOut ? 1.0 : 0.0)) {
    item.m_gettingOut = gettingOut;
    interList.m_strokeList.push_back(item);
  }
}

//-----------------------------------------------------------------------------

double getAngle(const TPointD &p0, const TPointD &p1) {
  double angle1 = 180 * atan2(p0.x, p0.y) / TConsts::pi;
  double angle2 = 180 * atan2(p1.x, p1.y) / TConsts::pi;

  if (angle1 < 0) angle1 = 360 + angle1;
  if (angle2 < 0) angle2 = 360 + angle2;

  return (angle2 - angle1) < 0 ? 360 + angle2 - angle1 : angle2 - angle1;
}

//-----------------------------------------------------------------------------
// nel caso l'angolo tra due stroke in un certo w sia nullo,
// si va un po' avanti per vedere come sono orientate....
double getNearAngle(const TStroke *s1, double w1, bool out1, const TStroke *s2,
                    double w2, bool out2) {
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

bool makeEdgeIntersection(Intersection &interList, IntersectedStroke &item1,
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
    CASE 0 :  // p1a p2b p2a p1b
              insertBranch(interList, item1, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    CASE 1 :  // p1a p2b p1b p2a
              insertBranch(interList, item1, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    CASE 2 : assert(false);
    CASE 3 :  // p1a p1b p2b p2a
              insertBranch(interList, item1, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    CASE 4 :  // p1a p2a p2b p1b
              insertBranch(interList, item1, true);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
    if (!eraseP1b) insertBranch(interList, item1, false);
    CASE 5 : assert(false);
    CASE 6 :  // p1a p2a p1b p2b
              insertBranch(interList, item1, true);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2b) insertBranch(interList, item2, false);
    CASE 7 :  // p1a p1b p2a p2b
              insertBranch(interList, item1, true);
    if (!eraseP1b) insertBranch(interList, item1, false);
    if (!eraseP2a) insertBranch(interList, item2, true);
    if (!eraseP2b) insertBranch(interList, item2, false);
  DEFAULT:
    assert(false);
  }

  return true;
}

//-----------------------------------------------------------------------------

bool makeIntersection(IntersectionData &intData, const vector<VIStroke *> &s,
                      int ii, int jj, DoublePair inter, int strokeSize,
                      Intersection &interList) {
  IntersectedStroke item1(intData.m_intList.end(), NULL_ITER),
      item2(intData.m_intList.end(), NULL_ITER);

  interList.m_intersection = s[ii]->m_s->getPoint(inter.first);

  item1.m_edge.m_w0 = inter.first;
  item2.m_edge.m_w0 = inter.second;

  if (ii >= 0 && ii < strokeSize) {
    item1.m_edge.m_s     = s[ii]->m_s;
    item1.m_edge.m_index = ii;
  } else {
    if (ii < 0) {
      item1.m_edge.m_s     = intData.m_autocloseMap[ii];
      item1.m_edge.m_index = ii;
    } else {
      item1.m_edge.m_s     = s[ii]->m_s;
      item1.m_edge.m_index = -(ii + intData.maxAutocloseId * 100000);
      intData.m_autocloseMap[item1.m_edge.m_index] = item1.m_edge.m_s;
    }
  }

  if (jj >= 0 && jj < strokeSize) {
    item2.m_edge.m_s     = s[jj]->m_s;
    item2.m_edge.m_index = jj;
  } else {
    if (jj < 0) {
      item2.m_edge.m_s     = intData.m_autocloseMap[jj];
      item2.m_edge.m_index = jj;
    } else {
      item2.m_edge.m_s     = s[jj]->m_s;
      item2.m_edge.m_index = -(jj + intData.maxAutocloseId * 100000);
      intData.m_autocloseMap[item2.m_edge.m_index] = item2.m_edge.m_s;
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
      interList.m_strokeList.push_back(item1);
      item2.m_gettingOut = (item2.m_edge.m_w0 == 0.0);
      interList.m_strokeList.push_back(item2);
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
    interList.m_strokeList.push_back(item1);
  }
  if (item2.m_edge.m_w0 != (reversed ? 0.0 : 1.0)) {
    item2.m_gettingOut = !reversed;
    interList.m_strokeList.push_back(item2);
  }
  if (item1.m_edge.m_w0 != 0.0) {
    item1.m_gettingOut = false;
    interList.m_strokeList.push_back(item1);
  }
  if (item2.m_edge.m_w0 != (reversed ? 1.0 : 0.0)) {
    item2.m_gettingOut = reversed;
    interList.m_strokeList.push_back(item2);
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

bool addAutocloseIntersection(IntersectionData &intData, vector<VIStroke *> &s,
                              int ii, int jj, double w0, double w1,
                              int strokeSize) {
  list<Intersection>::reverse_iterator rit = intData.m_intList.rbegin();

  assert(w0 >= 0.0 && w0 <= 1.0);
  assert(w1 >= 0.0 && w1 <= 1.0);

  for (; rit != intData.m_intList.rend();
       rit++)  // evito di fare la connessione, se gia' ce
               // ne e' una simile tra le stesse due stroke
  {
    if (rit->m_strokeList.size() < 2) continue;
    list<IntersectedStroke>::iterator is = rit->m_strokeList.begin();
    int s0                               = is->m_edge.m_index;
    if (s0 < 0) continue;
    double ww0 = is->m_edge.m_w0;
    is++;
    if (is->m_edge.m_index == s0 && is->m_edge.m_w0 == ww0) {
      is++;
      if (is == rit->m_strokeList.end()) continue;
    }
    int s1 = is->m_edge.m_index;
    if (s1 < 0) continue;
    double ww1 = is->m_edge.m_w0;

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
    addIntersection(intData, s, ii, jj, DoublePair(w0, w1), strokeSize);
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
        addIntersection(intData, s, i, ii, DoublePair(0.0, w0), strokeSize);
        addIntersection(intData, s, i, jj, DoublePair(1.0, w1), strokeSize);
        return true;
      }
    }

  s.push_back(new VIStroke(new TStroke(v)));
  addIntersection(intData, s, s.size() - 1, ii, DoublePair(0.0, w0),
                  strokeSize);
  addIntersection(intData, s, s.size() - 1, jj, DoublePair(1.0, w1),
                  strokeSize);
  return true;
}

//-----------------------------------------------------------------------------

double g_autocloseTolerance = c_newAutocloseTolerance;

bool makeEndPointConnections(vector<VIStroke *> &s, int ii, bool isIStartPoint,
                             int jj, bool isJStartPoint,
                             IntersectionData &intData, int strokeSize) {
  double x0 = (isIStartPoint ? 0.0 : 1.0);
  double x1 = (isJStartPoint ? 0.0 : 1.0);

  TThickPoint p0 = s[ii]->m_s->getThickPoint(x0);
  TThickPoint p1 = s[jj]->m_s->getThickPoint(x1);
  double dist2;

  dist2 = tdistance2(p0, p1);
  if (dist2 >= 0 &&
      dist2 <=
          tmax((g_autocloseTolerance == c_oldAutocloseTolerance) ? 9.09 : 2.0,
               g_autocloseTolerance * (p0.thick + p1.thick) *
                   (p0.thick +
                    p1.thick)))  // 0.01 tiene conto di quando thick==0
  {
    if (ii == jj) {
      TRectD r = s[ii]->m_s->getBBox();  // se e' un autoclose su una stroke
                                         // piccolissima, creerebbe uan area
                                         // trascurabile, ignoro
      if (fabs(r.x1 - r.x0) < 2 && fabs(r.y1 - r.y0) < 2) return false;
    }
    return addAutocloseIntersection(intData, s, ii, jj, x0, x1, strokeSize);
  }

  return false;
}
/*
if (s[ii]==s[jj])
  return;

dist2 = c_autocloseTolerance*tdistance2(p01, p10);
if (dist2>0 && dist2<=(p01.thick+p10.thick)*(p01.thick+p10.thick))
  addAutocloseIntersection(intData, s, ii, jj, 1.0, 0.0, strokeSize);


dist2 = c_autocloseTolerance*tdistance2(p00, p10);
if ((dist2>0 && dist2<=(p00.thick+p10.thick)*(p00.thick+p10.thick)))
  addAutocloseIntersection(intData, s, ii, jj, 0.0, 0.0, strokeSize);

dist2 = c_autocloseTolerance*tdistance2(p01, p11);
if ((dist2>0 && dist2<=(p01.thick+p11.thick)*(p01.thick+p11.thick)))
  addAutocloseIntersection(intData, s, ii, jj, 1.0, 1.0, strokeSize);
}
*/
//-----------------------------------------------------------------------------

double getCurlW(TStroke *s, bool isBegin)  // trova il punto di split su una
                                           // stroke, in prossimita di un
                                           // ricciolo;
// un ricciolo c'e' se la curva ha un  min o max relativo su x seguito da uno su
// y, o viceversa.
{
  int numChunks = s->getChunkCount();
  double dx2, dx1 = 0, dy2, dy1 = 0;

  for (int i = 0; i < numChunks; i++) {
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

  for (int j = 0; j < numChunks; j++) {
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
      s, isBegin ? tmax(maxMin0, maxMin1) : tmin(maxMin0, maxMin1),
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

void makeConnection(vector<VIStroke *> &s, int ii, int jj, bool isBegin,
                    IntersectionData &intData, int strokeSize) {
  if (s[ii]->m_s->isSelfLoop()) return;

  double w0 = isBegin ? 0.0 : 1.0;

  TThickPoint p0 = s[ii]->m_s->getThickPoint(w0);
  double t, dist2;
  int index;
  TStroke sAux, *sComp;

  if (ii == jj)  // per trovare le intersezioni con una stroke e se stessa, si
                 // toglie il
  // pezzo di stroke di cui si cercano vicinanze fino alla prima curva
  {
    double w = getCurlW(s[ii]->m_s, isBegin);
    if (w == -1) return;

    split<TStroke>(*(s[ii]->m_s), tmin(isBegin ? 1.0 : 0.0, w),
                   tmax(isBegin ? 1.0 : 0.0, w), sAux);
    sComp = &sAux;
  } else
    sComp = s[jj]->m_s;

  if (sComp->getNearestChunk(p0, t, index, dist2) && dist2 > 0) {
    if (ii == jj) {
      double dummy;
      s[jj]->m_s->getNearestChunk(sComp->getChunk(index)->getPoint(t), t, index,
                                  dummy);
    }

    // if (areAlmostEqual(w, 0.0, 0.05) || areAlmostEqual(w, 1.0, 0.05))
    //  return; //se w e' vicino ad un estremo, rientra nell'autoclose point to
    //  point

    // if (s[jj]->m_s->getLength(w)<=s[jj]->m_s->getThickPoint(0).thick ||
    //    s[jj]->m_s->getLength(w, 1)<=s[jj]->m_s->getThickPoint(1).thick)
    //  return;

    TThickPoint p1 = s[jj]->m_s->getChunk(index)->getThickPoint(t);
    if (dist2 <=
        (tmax(
            (g_autocloseTolerance == c_oldAutocloseTolerance) ? 9.09 : 2.0,
            (g_autocloseTolerance + 0.7) * (p0.thick + p1.thick) *
                (p0.thick + p1.thick))))  // 0.01 tiene conto di quando thick==0
    {
      // if (areAlmostEqual(dist2, 0.0))
      //  return;
      double w = getWfromChunkAndT(s[jj]->m_s, index, t);
      addAutocloseIntersection(intData, s, ii, jj, w0, w, strokeSize);
    }
  }
}

//-----------------------------------------------------------------------------

void autoclose(vector<VIStroke *> &s, int ii, int jj, IntersectionData &IntData,
               int strokeSize) {
  bool ret1 = false, ret2 = false, ret3 = false, ret4 = false;

  if (!s[ii]->m_s->isSelfLoop() && !s[jj]->m_s->isSelfLoop()) {
    ret1 = makeEndPointConnections(s, ii, true, jj, false, IntData, strokeSize);

    if (ii != jj) {
      ret2 =
          makeEndPointConnections(s, ii, true, jj, true, IntData, strokeSize);
      ret3 =
          makeEndPointConnections(s, ii, false, jj, true, IntData, strokeSize);
      ret4 =
          makeEndPointConnections(s, ii, false, jj, false, IntData, strokeSize);
    }
  }

  if (!ret1 && !ret2) makeConnection(s, ii, jj, true, IntData, strokeSize);
  if (!ret1 && !ret4) makeConnection(s, jj, ii, false, IntData, strokeSize);
  if (ii != jj) {
    if (!ret2 && !ret3) makeConnection(s, jj, ii, true, IntData, strokeSize);
    if (!ret3 && !ret4) makeConnection(s, ii, jj, false, IntData, strokeSize);
  }
}

//-----------------------------------------------------------------------------

TPointD inline getTangent(const IntersectedStroke &item) {
  return (item.m_gettingOut ? 1 : -1) *
         item.m_edge.m_s->getSpeed(item.m_edge.m_w0, item.m_gettingOut);
}

//-----------------------------------------------------------------------------

void addBranch(IntersectionData &intData, list<IntersectedStroke> &strokeList,
               const vector<VIStroke *> &s, int ii, double w, int strokeSize,
               bool gettingOut) {
  list<IntersectedStroke>::iterator it1, it2;
  TPointD tanRef, lastTan;

  IntersectedStroke item(intData.m_intList.end(), strokeList.end());

  if (ii < 0) {
    item.m_edge.m_s     = intData.m_autocloseMap[ii];
    item.m_edge.m_index = ii;
  } else {
    item.m_edge.m_s = s[ii]->m_s;
    if (ii < strokeSize)
      item.m_edge.m_index = ii;
    else {
      item.m_edge.m_index = -(ii + intData.maxAutocloseId * 100000);
      intData.m_autocloseMap[item.m_edge.m_index] = item.m_edge.m_s;
    }
  }

  item.m_edge.m_w0  = w;
  item.m_gettingOut = gettingOut;

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

  tanRef  = getTangent(item);
  lastTan = getTangent(strokeList.back());
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

  it2 = strokeList.end();
  it2--;
  for (it1 = strokeList.begin(); it1 != strokeList.end(); ++it1) {
    TPointD curTan = getTangent(*it1);
    double angle0  = getAngle(lastTan, curTan);
    double angle1  = getAngle(lastTan, tanRef);

    if (areAlmostEqual(angle1, 0, 1e-8)) {
      double angle =
          getNearAngle(it2->m_edge.m_s, it2->m_edge.m_w0, it2->m_gettingOut,
                       item.m_edge.m_s, item.m_edge.m_w0, item.m_gettingOut);
      angle1 += angle;
      if (angle1 > 360) angle1 -= 360;
    }

    if (areAlmostEqual(angle0, angle1, 1e-8)) {
      double angle =
          getNearAngle(it1->m_edge.m_s, it1->m_edge.m_w0, it1->m_gettingOut,
                       item.m_edge.m_s, item.m_edge.m_w0, item.m_gettingOut);
      angle1 += angle;
      if (angle1 > 360) angle1 -= 360;
    }

    if (angle1 < angle0) {
      strokeList.insert(it1, item);
      return;
    }
    lastTan = curTan;
    it2     = it1;
  }

  // assert(!"add branch: can't find where to insert!");
  strokeList.push_back(item);
}

//-----------------------------------------------------------------------------

void addBranches(IntersectionData &intData, Intersection &intersection,
                 const vector<VIStroke *> &s, int ii, int jj,
                 DoublePair intersectionPair, int strokeSize) {
  bool foundS1 = false, foundS2 = false;
  list<IntersectedStroke>::iterator it;

  assert(!intersection.m_strokeList.empty());

  for (it = intersection.m_strokeList.begin();
       it != intersection.m_strokeList.end(); it++) {
    if ((ii >= 0 && (*it).m_edge.m_s == s[ii]->m_s &&
         it->m_edge.m_w0 == intersectionPair.first) ||
        (ii < 0 && (*it).m_edge.m_index == ii &&
         it->m_edge.m_w0 == intersectionPair.first))
      foundS1 = true;
    if ((jj >= 0 && (*it).m_edge.m_s == s[jj]->m_s &&
         it->m_edge.m_w0 == intersectionPair.second) ||
        (jj < 0 && (*it).m_edge.m_index == jj &&
         it->m_edge.m_w0 == intersectionPair.second))
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

void addIntersections(IntersectionData &intData, const vector<VIStroke *> &s,
                      int ii, int jj, vector<DoublePair> &intersections,
                      int strokeSize) {
  for (int k = 0; k < (int)intersections.size(); k++) {
    if (ii >= strokeSize && (areAlmostEqual(intersections[k].first, 0.0) ||
                             areAlmostEqual(intersections[k].first, 1.0)))
      continue;
    if (jj >= strokeSize && (areAlmostEqual(intersections[k].second, 0.0) ||
                             areAlmostEqual(intersections[k].second, 1.0)))
      continue;

    addIntersection(intData, s, ii, jj, intersections[k], strokeSize);
  }
}

//-----------------------------------------------------------------------------

inline double truncate(double x) {
  x += 1.0;
  unsigned long *l = (unsigned long *)&x;

#if TNZ_LITTLE_ENDIAN
  l[0] &= 0xFFE00000;
#else
  l[1] &= 0xFFE00000;
#endif

  return x - 1.0;
}

//-----------------------------------------------------------------------------

void addIntersection(IntersectionData &intData, const vector<VIStroke *> &s,
                     int ii, int jj, DoublePair intersection, int strokeSize) {
  list<Intersection>::iterator it;
  TPointD p;

  // UINT iw;
  // iw = ((UINT)(intersection.first*0x3fffffff));
  // intersection.first = truncate(intersection.first);
  // iw = (UINT)(intersection.second*0x3fffffff);

  // intersection.second = truncate(intersection.second);

  if (areAlmostEqual(intersection.first, 0.0, 1e-8))
    intersection.first = 0.0;
  else if (areAlmostEqual(intersection.first, 1.0, 1e-8))
    intersection.first = 1.0;

  if (areAlmostEqual(intersection.second, 0.0, 1e-8))
    intersection.second = 0.0;
  else if (areAlmostEqual(intersection.second, 1.0, 1e-8))
    intersection.second = 1.0;

  p = s[ii]->m_s->getPoint(intersection.first);

  for (it = intData.m_intList.begin(); it != intData.m_intList.end(); it++)
    if ((*it).m_intersection ==
        p)  // devono essere rigorosamente uguali, altrimenti
    // il calcolo dell'ordine dei rami con le tangenti sballa
    {
      addBranches(intData, *it, s, ii, jj, intersection, strokeSize);
      return;
    }

  intData.m_intList.push_back(Intersection());

  if (!makeIntersection(intData, s, ii, jj, intersection, strokeSize,
                        intData.m_intList.back())) {
    list<Intersection>::iterator it = intData.m_intList.begin();
    advance(it, intData.m_intList.size() - 1);

    intData.m_intList.erase(it);
  }
}

//-----------------------------------------------------------------------------

void TVectorImage::Imp::findIntersections() {
  vector<VIStroke *> &strokeArray = m_strokes;
  IntersectionData &intData       = m_intersectionData;
  int strokeSize                  = (int)strokeArray.size();
  int i, j;

  assert(intData.m_intersectedStrokeArray.empty());

  intData.maxAutocloseId++;

  map<int, TStroke *>::iterator it, it_b = intData.m_autocloseMap.begin();
  map<int, TStroke *>::iterator it_e = intData.m_autocloseMap.end();

  // prima cerco le intersezioni tra nuove strokes e vecchi autoclose
  for (i = 0; i < strokeSize; i++) {
    TStroke *s1 = strokeArray[i]->m_s;
    if (!strokeArray[i]->m_isNewForFill || strokeArray[i]->m_isPoint) continue;
    TRectD bBox   = s1->getBBox();
    double thick2 = s1->getThickPoint(0).thick * 2;
    if (bBox.getLx() <= thick2 && bBox.getLy() <= thick2) {
      strokeArray[i]->m_isPoint = true;
      continue;
    }
    for (int j = 0; j < (int)s1->getControlPointCount(); j++) {
      TThickPoint p = s1->getControlPoint(j);
      s1->setControlPoint(j, myRound(p));
    }

    for (it = it_b; it != it_e; ++it) {
      TStroke *s2 = it->second;
      vector<DoublePair> parIntersections;
      if (intersect(s1, s2, parIntersections, true))
        addIntersections(intData, strokeArray, i, it->first, parIntersections,
                         strokeSize);
    }
  }

  // poi,  intersezioni tra stroke, in cui almeno uno dei due deve essere nuovo

  for (i = 0; i < strokeSize; i++) {
    TStroke *s1 = strokeArray[i]->m_s;
    if (strokeArray[i]->m_isPoint) continue;
    for (j = i; j < strokeSize /*&& (strokeArray[i]->getBBox().x1>=
                                  strokeArray[j]->getBBox().x0)*/
         ;
         j++) {
      TStroke *s2 = strokeArray[j]->m_s;
      if (strokeArray[j]->m_isPoint) continue;
      if (!(strokeArray[i]->m_isNewForFill || strokeArray[j]->m_isNewForFill))
        continue;
      vector<DoublePair> parIntersections;
      if (s1->getBBox().overlaps(s2->getBBox())) {
        UINT size = intData.m_intList.size();

        if (intersect(s1, s2, parIntersections, false)) {
          // if (i==0 && j==1) parIntersections.erase(parIntersections.begin());

          addIntersections(intData, strokeArray, i, j, parIntersections,
                           strokeSize);
        }
        // autoclose(strokeArray, i, j, intData, strokeSize);
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
    // strokeArray[i]->m_isNewForFill = false;
  }

  for (i = 0; i < strokeSize; i++) {
    TStroke *s1 = strokeArray[i]->m_s;
    if (strokeArray[i]->m_isPoint) continue;
    for (j = i; j < strokeSize; j++) {
      TStroke *s2 = strokeArray[j]->m_s;
      if (strokeArray[j]->m_isPoint) continue;
      if (!(strokeArray[i]->m_isNewForFill || strokeArray[j]->m_isNewForFill))
        continue;
      if (s1->getBBox().overlaps(s2->getBBox()))
        autoclose(strokeArray, i, j, intData, strokeSize);
    }
    strokeArray[i]->m_isNewForFill = false;
  }

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
      TStroke *s2 = strokeArray[j]->m_s;
      vector<DoublePair> parIntersections;
      if (intersect(s1, s2, parIntersections, true))
        addIntersections(intData, strokeArray, i, j, parIntersections,
                         strokeSize);
    }
    for (j = 0; j < strokeSize; ++j)  // intersezione segmento-curva
    {
      if (strokeArray[j]->m_isPoint) continue;
      TStroke *s2 = strokeArray[j]->m_s;
      vector<DoublePair> parIntersections;
      if (intersect(s1, s2, parIntersections, true))
        addIntersections(intData, strokeArray, i, j, parIntersections,
                         strokeSize);
    }
  }
}

// la struttura delle intersezioni viene poi visitata per trovare
// i link tra un'intersezione e la successiva

//-----------------------------------------------------------------------------

int TVectorImage::Imp::computeIntersections() {
  list<Intersection>::iterator it1;
  list<IntersectedStroke>::iterator it2;
  IntersectionData &intData = m_intersectionData;
  int strokeSize            = (int)m_strokes.size();

  findIntersections();

  findNearestIntersection(intData.m_intList);

  // for (it1=intData.m_intList.begin(); it1!=intData.m_intList.end();) //la
  // faccio qui, e non nella eraseIntersection. vedi commento li'.
  eraseDeadIntersections();

  for (it1 = intData.m_intList.begin(); it1 != intData.m_intList.end(); it1++)
    markDeadIntersections(intData.m_intList, it1);

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
TRegion *findRegion(list<Intersection> &intList,
                    list<Intersection>::iterator it1,
                    list<IntersectedStroke>::iterator it2) {
  TRegion *r    = new TRegion();
  int currStyle = 0;

  list<IntersectedStroke>::iterator itStart = it2;
  list<Intersection>::iterator nextIt1;
  list<IntersectedStroke>::iterator nextIt2;

  // Cicla finche' t2 non punta ad uno stroke gia' visitato
  while (!it2->m_visited) {
    it2->m_visited = true;

    // Ciclo finche' lo stroke puntato da it2 non ha un successivo punto di
    // intersezione
    do {
      it2++;
      if (it2 ==
          it1->m_strokeList.end())  // uso la lista come se fosse circolare
        it2 = it1->m_strokeList.begin();
    } while (it2->m_nextIntersection == intList.end());

    nextIt1 = it2->m_nextIntersection;
    nextIt2 = it2->m_nextStroke;

    // Viene controllato e sistemato lo stile degli stroke
    if (it2->m_edge.m_styleId != 0) {
      if (currStyle == 0)
        currStyle = it2->m_edge.m_styleId;
      else if (it2->m_edge.m_styleId != currStyle) {
        currStyle = it2->m_edge.m_styleId;
        for (UINT i                = 0; i < r->getEdgeCount(); i++)
          r->getEdge(i)->m_styleId = currStyle;
      }
    } else
      it2->m_edge.m_styleId = currStyle;

    // Aggiunge lo stroke puntato da p2 alla regione
    r->addEdge(&it2->m_edge);

    if (nextIt2 == itStart) return r;

    it1 = nextIt1;
    it2 = nextIt2;
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
        tmin(e1->m_w0, e1->m_w1)==tmin(e2->m_w0, e2->m_w1) &&
        tmax(e1->m_w0, e1->m_w1)==tmax(e2->m_w0, e2->m_w1))
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

//-----------------------------------------------------------------------------

void transferColors(const list<TEdge *> &oldList, const list<TEdge *> &newList,
                    bool isStrokeChanged, bool isFlipped, bool overwriteColor);

//-----------------------------------------------------------------------------
void printStrokes1(vector<VIStroke *> &v, int size) {
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
#ifdef _DEBUG
static void printTime(TStopWatch &sw, string name) {
  ostringstream ss;
  ss << name << " : ";
  sw.print(ss);
  ss << '\n' << '\0';
  string s(ss.str());
  // TSystem::outputDebug(s);
}
#endif
//-----------------------------------------------------------------------------
void printStrokes1(vector<VIStroke *> &v, int size);

// Trova le regioni in una TVectorImage
int TVectorImage::Imp::computeRegions() {
#if defined(_DEBUG) && !defined(MACOSX)
  TStopWatch stopWatch;
  stopWatch.start(true);
#endif

  if (!m_computeRegions) return 0;

  /*if (m_intersectionData.m_computedAlmostOnce)
{
UINT i,n=m_strokes.size();
  vector<int> vv(n);
for( i=0; i<n;++i) vv[i] = i;
m_intersectionData.m_computedAlmostOnce = true;
notifyChangedStrokes(vv,vector<TStroke*>(), false);

return true;
}*/

  g_autocloseTolerance = m_autocloseTolerance;

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
  m_intersectionData.m_computedAlmostOnce = true;
  list<Intersection> &intList             = m_intersectionData.m_intList;
  cleanIntersectionMarks(intList);

  // calcolo struttura delle intersezioni
  int added = 0, notAdded = 0;
  int strokeSize;
  strokeSize = computeIntersections();

  list<Intersection>::iterator it1;
  list<IntersectedStroke>::iterator it2;

  for (it1 = intList.begin(); it1 != intList.end(); it1++)
    for (it2 = (*it1).m_strokeList.begin(); it2 != (*it1).m_strokeList.end();
         it2++)
      it2->m_edge.m_r = 0;

  for (it1 = intList.begin(); it1 != intList.end(); it1++) {
    // Controlla che il punto in questione non sia isolato
    if (it1->m_numInter == 0) continue;

    for (it2 = it1->m_strokeList.begin(); it2 != it1->m_strokeList.end();
         it2++) {
      TRegion *region;

      // se lo stroke non unisce due punti di intersezione
      // non lo considero e vado avanti con un altro stroke
      if (it2->m_nextIntersection == intList.end()) continue;

      // Se lo stroke puntato da t2 non e' stato ancora visitato, trova una
      // regione
      if (!it2->m_visited && (region = findRegion(intList, it1, it2))) {
        // Se la regione e' valida la aggiunge al vettore delle regioni
        if (isValidArea(m_regions, *region)) {
          added++;
          addRegion(m_regions, region);

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
    for (i = 0; i < m_intersectionData.m_intersectedStrokeArray.size(); i++) {
      if (!m_strokes[m_intersectionData.m_intersectedStrokeArray[i].m_index]
               ->m_edgeList.empty())
        transferColors(
            m_intersectionData.m_intersectedStrokeArray[i].m_edgeList,
            m_strokes[m_intersectionData.m_intersectedStrokeArray[i].m_index]
                ->m_edgeList,
            false, false, true);
      clearPointerContainer(
          m_intersectionData.m_intersectedStrokeArray[i].m_edgeList);
      m_intersectionData.m_intersectedStrokeArray[i].m_edgeList.clear();
    }
    m_intersectionData.m_intersectedStrokeArray.clear();
  }

  assert(m_intersectionData.m_intersectedStrokeArray.empty());

  // tolgo i segmenti aggiunti con l'autoclose
  vector<VIStroke *>::iterator it = m_strokes.begin();
  advance(it, strokeSize);
  m_strokes.erase(it, m_strokes.end());

  m_areValidRegions = true;

#if defined(_DEBUG) && !defined(MACOSX)

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
