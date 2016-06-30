#pragma once

//-----------------------------------------------------------------------------
// tregion.h: interface for the TRegion class.
//-----------------------------------------------------------------------------
#if !defined(TREGION_H)
#define TREGION_H

#include <memory>

#include "tgeometry.h"
//#include "tsmartpointer.h"

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declarations

class TStroke;
// class  TVectorRenderData;
class TRegion;
class TRegionProp;
// class TVectorPalette;

//=============================================================================

class TRegionId {
public:
  int m_strokeId;
  float m_midW;
  bool m_direction;

  TRegionId(int strokeId, float midW, bool direction)
      : m_strokeId(strokeId), m_midW(midW), m_direction(direction) {}
};

class TFilledRegionInf {
public:
  TRegionId m_regionId;
  int m_styleId;

  TFilledRegionInf(int strokeId, float midW, bool direction, int styleId)
      : m_regionId(strokeId, midW, direction), m_styleId(styleId) {}

  TFilledRegionInf(TRegionId regionId, int styleId)
      : m_regionId(regionId), m_styleId(styleId) {}
};

//=============================================================================

class TGeneralEdge {
public:
  enum Type { eNormal, eAutoclose };

  Type m_type;

  TGeneralEdge(Type type) : m_type(type) {}
};

class TEdge final : public TGeneralEdge {
public:
  TStroke *m_s;
  double m_w0, m_w1;
  int m_index;
  TRegion *m_r;
  int m_styleId;
  bool m_toBeDeleted;
  // bool m_forward;
public:
  TEdge()
      : TGeneralEdge(eNormal)
      , m_s(0)
      , m_w0(-1)
      , m_w1(-1)
      , m_index(-1)
      , m_r(0)
      , m_styleId(0)
      , m_toBeDeleted(false)
  //, m_forward(true)
  {}

  TEdge(TStroke *ref, UINT index, double w0, double w1,
        bool toBeDeletedWithStroke, int styleId)
      : TGeneralEdge(eNormal)
      , m_s(ref)
      , m_w0(w0)
      , m_w1(w1)
      , m_index(index)
      , m_r(0)
      , m_styleId(styleId)
      , m_toBeDeleted(toBeDeletedWithStroke)
  //, m_forward(forward)
  {}

  /* TEdge(TStroke* ref, bool toBeDeletedWithStroke)
: m_s (ref)
, m_w0  (-1)
, m_w1  (-1)
, m_r(0)
, m_styleId(0)
                  , m_toBeDeleted(toBeDeletedWithStroke)
{}*/

  TEdge(const TEdge &e, bool toBeDeletedWithStroke)
      : TGeneralEdge(eNormal)
      , m_s(e.m_s)
      , m_w0(e.m_w0)
      , m_w1(e.m_w1)
      , m_index(e.m_index)
      , m_r(e.m_r)
      , m_styleId(e.m_styleId)
      , m_toBeDeleted(toBeDeletedWithStroke)
  //, m_forward(e.m_forward)
  {}

  TEdge &operator=(const TEdge &e) {
    m_s           = e.m_s;
    m_w0          = e.m_w0;
    m_w1          = e.m_w1;
    m_index       = e.m_index;
    m_r           = e.m_r;
    m_styleId     = e.m_styleId;
    m_toBeDeleted = e.m_toBeDeleted;
    // m_forward = e.m_forward;
    return *this;
  }
  TEdge(const TEdge &e)
      : TGeneralEdge(eNormal)
      , m_s(e.m_s)
      , m_w0(e.m_w0)
      , m_w1(e.m_w1)
      , m_index(e.m_index)
      , m_r(e.m_r)
      , m_styleId(e.m_styleId)
      , m_toBeDeleted(e.m_toBeDeleted) /*, m_forward(e.m_forward)*/ {}

  bool operator<(const TEdge &e) const {
    return (e.m_s == m_s) ? ((e.m_w0 == m_w0) ? e.m_w1 < m_w1 : e.m_w0 < m_w0)
                          : e.m_s < m_s;
  }

  inline int getStyle() const { return m_styleId; }
  inline void setStyle(int styleId) { m_styleId = styleId; }

  inline bool operator==(const TEdge &b) const {
    return m_s == b.m_s && ((m_w0 == b.m_w0 && m_w1 == b.m_w1) ||
                            (m_w0 == b.m_w1 && m_w1 == b.m_w0));
  }

private:
  TEdge(TStroke *ref);
};

/*
#ifdef _WIN32
template class DVAPI TSmartPointerT<TEdge>;
#endif

typedef TSmartPointerT<TEdge> TEdgeP;
*/
//-----------------------------------------------------------------------------

// void addRegion(vector<TRegion*>& regionArray, TRegion *region);

//-----------------------------------------------------------------------------

class DVAPI TRegion {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TRegion();
  ~TRegion();

  TRectD getBBox() const;

  bool contains(const TPointD &p) const;
  bool contains(const TRegion &r) const;
  bool contains(const TStroke &s, bool mayIntersect = false) const;
  bool contains(const TEdge &e) const;

  //! returns the region equivalent to r, 0 if does not exists.
  TRegion *findRegion(const TRegion &r) const;
  TRegion *getSubregion(UINT index) const;
  UINT getSubregionCount() const;
  void deleteSubregion(UINT index);
  void addSubregion(TRegion *region);
  TEdge *getEdge(UINT index) const;
  TEdge *getLastEdge() const;
  UINT getEdgeCount() const;
  void addEdge(TEdge *e, bool minimizeEdges);
  TEdge *popFrontEdge();
  TEdge *popBackEdge();

  void moveSubregionsTo(TRegion *r);
  // it returns the  style of the region before filling or -1 if not filled.
  int fill(const TPointD &p, int styleId);

  bool selectFill(const TRectD &selectArea, int styleId);

  //! returns the smallest subregions (including this) containing p
  TRegion *getRegion(const TPointD &p);

  int getStyle() const;

  void setStyle(int styleId);

  void autoFill(int styleId, bool oddLevel);

  bool isSubRegionOf(const TRegion &r) const;
  bool getInternalPoint(TPointD &p);

  TRegionProp *getProp();
  void setProp(TRegionProp *prop);

  TRegionId getId();

  void invalidateProp();

  void invalidateBBox();

  void print();

  // friend void addRegion(vector<TRegion*>& regionArray, TRegion *region);
  void computeScanlineIntersections(double y,
                                    std::vector<double> &intersections) const;

  int scanlineIntersectionsBefore(double x, double y, bool horiz) const;
  int leftScanlineIntersections(double x, double y) const {
    return scanlineIntersectionsBefore(x, y, true);
  }

#ifdef _DEBUG
  void checkRegion();
#endif
};

//-----------------------------------------------------------------------------
/*Spostata in tregion.cpp
inline int TRegion::getStyle() const
{
int ret = 0;
  UINT i=0, j, n=getEdgeCount();
  for (; i<n; i++)
  {
          int styleId = getEdge(i)->getStyle();
      if(styleId != 0 && ret==0)
                          {
                                //assert(styleId<100);
                                ret =  styleId;
                                if (i>0) for (j=0;j<i;j++)
getEdge(i)->setStyle(ret);
                                }
                        else if (styleId!=ret)
                          getEdge(i)->setStyle(ret);
  }
  return ret;
}
*/

class DVAPI TRegionFeatureFormula {
public:
  virtual void update(const TPointD &p1, const TPointD &p2) = 0;
};

// permette di calcolare varie grandesse sul poligono della regione,
// come l'area, il baricentro, il perimetro...
// per usarla si deve sottoclassare la classe virtuale TRegionFeatureFormula
// specificando la formual della grandezza da calcolare.

void DVAPI computeRegionFeature(const TRegion &r,
                                TRegionFeatureFormula &formula);

#endif  // !defined(TREGION_H)
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
