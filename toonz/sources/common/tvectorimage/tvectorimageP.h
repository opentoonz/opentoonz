#pragma once

#ifndef _TVECTORIMAGEP_H_
#define _TVECTORIMAGEP_H_

#include "tstroke.h"
#include "tvectorimage.h"
#include "tregion.h"
#include "tcurves.h"

//-----------------------------------------------------------------------------

class IntersectedStroke;
class VIStroke;

//=============================================================================

class VIStroke;

class TGroupId {
public:
  std::vector<int> m_id;  // m_id[i-1] e' parent di m_id[i]
  TGroupId() : m_id() {}

  // ghost group sono i gruppi impliciti: tutti gli stroke che non fanno parte
  // di nessun gruppo ma
  // che stanno tra due gruppi fanno parte di un gruppo implicito. per
  // convenzione un ghostGroup ha id<0

  TGroupId(TVectorImage *vi, bool isGhost);

  TGroupId(const TGroupId &strokeGroup) : m_id(strokeGroup.m_id){};

  // costruisce un gruppo partendo da un parent e da un id esistente.
  TGroupId(const TGroupId &parent, const TGroupId &id);

  bool operator==(const TGroupId &id) const;
  bool operator!=(const TGroupId &id) const { return !(*this == id); }

  // TGroupId makeGroup(vector<VIStroke*> strokes, bool recomputeRegions=false);
  // void unmakeGroup(vector<VIStroke*> strokes);

  // ritrona la depth del gruppo. (0->not grouped)
  int isGrouped(bool implicit = false) const;

  // toglie il parent;  se nera gruppo semplice, gli assegna il parametro id.
  void ungroup(const TGroupId &id);
  // bool sameParent(const TGroupId& id) const;

  bool operator!() const { return m_id.empty() || m_id[0] == 0; };
  bool operator<(const TGroupId &id) const;

  int getDepth() const { return m_id.size(); }
  int getCommonParentDepth(const TGroupId &id) const;
  TGroupId getParent() const;

  int isParentOf(const TGroupId &id) const {
    return getCommonParentDepth(id) == getDepth();
  }
};

class VIStroke {
public:
  TStroke *m_s;
  bool m_isPoint;
  bool m_isNewForFill;
  std::list<TEdge *> m_edgeList;
  TGroupId m_groupId;

  VIStroke(TStroke *s, const TGroupId &StrokeId)
      : m_s(s), m_isPoint(false), m_isNewForFill(true), m_groupId(StrokeId){};

  VIStroke(const VIStroke &s, bool sameId = true);

  ~VIStroke() {
    delete m_s;

    std::list<TEdge *>::iterator it, it_b = m_edgeList.begin(),
                                     it_e = m_edgeList.end();
    for (it = it_b; it != it_e; ++it)
      if ((*it)->m_toBeDeleted) delete *it;
  }

  void inline addEdge(TEdge *e) { m_edgeList.push_back(e); }

  bool inline removeEdge(TEdge *e) {
    std::list<TEdge *>::iterator it = m_edgeList.begin();
    while (it != m_edgeList.end() && *it != e) it++;
    if (*it == e) {
      m_edgeList.erase(it);
      return true;
    }
    return false;
  }
};

//-----------------------------------------------------------------------------
class IntersectionData;
class Intersection;
// class IntersectStroke;

#ifdef LEVO

class TAutocloseEdge final : public TGeneralEdge {
public:
  TSegment m_segment;
  int m_nextStrokeIndex;
  double m_nextStrokeW;

  TAutocloseEdge(const TSegment &segment, int nextStrokeIndex,
                 double nextStrokeW)
      : TGeneralEdge(eAutoclose)
      , m_segment(segment)
      , m_nextStrokeIndex(nextStrokeIndex)
      , m_nextStrokeW(nextStrokeW) {}
};

#endif

//---------------------------------------------------------------------------------------------------
class TRegionFinder;

class TVectorImage::Imp {
  TVectorImage *m_vi;

public:
  int m_maxGroupId;
  int m_maxGhostGroupId;

  bool m_areValidRegions;
  bool m_computedAlmostOnce;
  bool m_justLoaded;
  bool m_minimizeEdges;
  bool m_notIntersectingStrokes, m_computeRegions;
  TGroupId m_insideGroup;

  std::vector<VIStroke *> m_strokes;
  double m_autocloseTolerance;
  IntersectionData *m_intersectionData;
  std::vector<TRegion *> m_regions;
  TThread::Mutex *m_mutex;
  Imp(TVectorImage *vi);
  ~Imp();

  void initRegionsData();
  void deleteRegionsData();

  TRegion *getRegion(const TPointD &p);

  int fill(const TPointD &p, int styleId);
  bool selectFill(const TRectD &selectArea, TStroke *s, int styleId,
                  bool onlyUnfilled, bool fillAreas, bool fillLines);

  void addStrokeRegionRef(UINT strokeIndex, TRegion *region);

  int computeRegions();
  void reindexEdges(UINT strokeIndex);
  void reindexEdges(const std::vector<int> &indexes, bool areAdded);

  void checkRegionDbConsistency();
  void cloneRegions(TVectorImage::Imp &out, bool doComputeRegions = true);

  void eraseIntersection(int index);
  UINT getFillData(std::unique_ptr<TVectorImage::IntersectionBranch[]> &v);
  void setFillData(std::unique_ptr<TVectorImage::IntersectionBranch[]> const &v,
                   UINT branchCount, bool doComputeRegions = true);
  void notifyChangedStrokes(const std::vector<int> &strokeIndexArray,
                            const std::vector<TStroke *> &oldVectorStrokeArray,
                            bool areFlipped);
  void insertStrokeAt(VIStroke *stroke, int strokeIndex,
                      bool recomputeRegions = true);
  void moveStroke(int fromIndex, int toIndex);
  void autoFill(int styleId, bool oddLevel);

  TRegion *getRegion(TRegionId regId, int index) const;
  TRegion *getRegionFromLoopStroke(int strokeIndex) const;
  VIStroke *joinStroke(int index1, int index2, int cpIndex1, int cpIndex2);
  VIStroke *joinStrokeSmoothly(int index1, int index2, int cpIndex1,
                               int cpIndex2);
  VIStroke *extendStroke(int index, const TThickPoint &p, int cpIndex);
  VIStroke *extendStrokeSmoothly(int index, const TThickPoint &p, int cpIndex);
  void removeStrokes(const std::vector<int> &toBeRemoved, bool deleteThem,
                     bool recomputeRegions);
  TStroke *removeStroke(int index, bool doComputeRegions);
  void splitStroke(int strokeIndex,
                   const std::vector<DoublePair> &sortedWRanges);
  void moveStrokes(int fromIndex, int count, int moveBefore, bool regroup);

  TStroke *removeEndpoints(int strokeIndex);
  void restoreEndpoints(int index, TStroke *oldStroke);

  int areDifferentGroup(UINT index1, bool isRegion1, UINT index2,
                        bool isRegion2) const;
  void rearrangeMultiGroup();
  void reindexGroups(Imp &img);
  void addRegion(TRegion *region);
  void regroupGhosts(std::vector<int> &changedStrokes);
  bool inCurrentGroup(int strokeIndex) const;
  bool canMoveStrokes(int strokeIndex, int count, int moveBefore) const;
#ifdef _DEBUG
  void checkIntersections();
  void checkRegions(const std::vector<TRegion *> &regions);
  void printStrokes(std::ofstream &os);
  void checkGroups();

#endif

#ifdef NEW_REGION_FILL
  TRegionFinder *m_regionFinder;
  void resetRegionFinder();
#endif

private:
  void findRegions(const TRectD &rect);
  int computeIntersections();
  void findIntersections();

  int computeEndpointsIntersections();
  // Imp(const TVectorImage::Imp &);
  // Imp &  operator=(const TVectorImage::Imp &);
  void eraseDeadIntersections();
  IntersectedStroke *eraseBranch(Intersection *in, IntersectedStroke *is);
  void doEraseIntersection(int index, std::vector<int> *toBeDeleted = 0);
  void eraseEdgeFromStroke(IntersectedStroke *is);
  bool areWholeGroups(const std::vector<int> &indexes) const;
  // accorpa tutti i gruppi ghost adiacenti in uno solo, e rinomina gruppi ghost
  // separati con lo stesso id; si usa questa funzione dopo ula creazione di un
  // gruppo o il move di strokes

  //--------------------NUOVO CALCOLO
  // REGIONI------------------------------------------------

public:
#ifdef LEVO
  vector<TRegion *> existingRegions;
  TRegion *findRegionFromStroke(const IntersectStroke &stroke,
                                const TPointD &p);
  bool findNextStrokes(const TEdge &currEdge,
                       std::multimap<double, TGeneralEdge *> &nextEdges);
  bool addNextEdge(TEdge &edge, TRegion &region, TRegion *&existingRegion,
                   bool isStartingEdge = false);
  bool addNextEdge(TAutocloseEdge &edge, TRegion &region,
                   TRegion *&existingRegion);
  bool storeRegion(TRegion *region, const TPointD &p);

  bool exploreAndAddNextEdge(TEdge &edge, TEdge &nextEdge, TRegion &region,
                             TRegion *&existingRegion);
  bool addNextAutocloseEdge(TEdge &edge, TAutocloseEdge &nextEdge,
                            TRegion &region, TRegion *&existingRegion);
  bool addNextAutocloseEdge(TAutocloseEdge &edge, TEdge &nextEdge,
                            TRegion &region, TRegion *&existingRegion);
  void computeAutocloseSegments(
      const TEdge &currEdge, int strokeIndex,
      std::multimap<double, TAutocloseEdge> &segments);
  void computeAutocloseSegmentsSameStroke(
      const TEdge &currEdge, std::multimap<double, TAutocloseEdge> &segments);
#endif

  //--------------------------------------------------------------------------------------

private:
  // not implemented
  Imp(const Imp &);
  Imp &operator=(const Imp &);
};

// Functions

void addRegion(std::vector<TRegion *> &regionArray, TRegion *region);

void transferColors(const std::list<TEdge *> &oldList,
                    const std::list<TEdge *> &newList, bool isStrokeChanged,
                    bool isFlipped, bool overwriteColor);

//=============================================================================

#endif
