#pragma once

#ifndef TELLIPTIC_BRUSH_P_H
#define TELLIPTIC_BRUSH_P_H

#include "tcurves.h"
#include "tstroke.h"
#include "tgl.h"  //tglPixelSize
#include "tstrokeoutline.h"

namespace tellipticbrush {

//! Tolerance parameter used somewhere throughout this code.
const double tolPar = 1e-6;

//********************************************************************************
//    Geometric Helper Functions
//********************************************************************************

double dist(const TPointD &P1, const TPointD &P2);
double dist(const TThickPoint &P1, const TThickPoint &P2);
double angle(const TPointD &v1, const TPointD &v2);

TPointD intersectionCoords(const TPointD &P0, const TPointD &d0,
                           const TPointD &P1, const TPointD &d1,
                           double detTol = 1e-2);

void buildEnvelopeDirection(const TThickPoint &p, const TThickPoint &d,
                            bool left, TPointD &res);
void buildEnvelopeDirections(const TThickPoint &p, const TThickPoint &d,
                             TPointD &resL, TPointD &resR);
void buildEnvelopeVector(const TThickPoint &p, const TThickPoint &d, bool left,
                         TPointD &res);
void buildEnvelopeVectors(const TThickPoint &p, const TThickPoint &d,
                          TPointD &resL, TPointD &resR);

void buildAngularSubdivision(double radius, double angle, double err,
                             int &nAngles);

TRectD computeBBox(const TStroke &stroke);

//********************************************************************************
//    Options structure
//********************************************************************************

/*
  Structure needed to hold both external and internal outlinization parameters.
*/
struct OutlinizationData {
  TOutlineUtil::OutlineParameter m_options;
  double m_pixSize;

  OutlinizationData() : m_options(), m_pixSize(0.0) {}
  OutlinizationData(const TOutlineUtil::OutlineParameter &options)
      : m_options(options), m_pixSize(sqrt(tglGetPixelSize2())) {}
};

//********************************************************************************
//    Centerline Point struct
//********************************************************************************

/*!
  CenterlinePoint contains the data a about a discretized centerline stroke
  point -
  which includes its position, and eventual forward and backward derivative-like
  directions. Thickness data is included in the structure.

  These informations are necessary and sufficient to build associated outline
  points,
  and eventual additional points related to caps.
*/
struct CenterlinePoint {
  int m_chunkIdx;  //!< The quadratic chunk index containing this point
  double m_t;      //!< The quadratic parameter where this point can be found

  TThickPoint m_p;  //!< The point's thick coordinates
  bool m_posBuilt;  //!< Wheteher m_p has been calculated

  TThickPoint m_prevD;  //!< The backward direction
  bool m_hasPrevD;      //!< If the point has (envelopable) backward direction

  TThickPoint m_nextD;  //!< The forward direction
  bool m_hasNextD;      //!< If the point has (envelopable) forward direction

  bool m_dirsBuilt;  //!< Whether directions have been calculated

  bool m_covered;  //!< Whether this point's outline can't be seen

  int m_countIdx;  //!< Additional index needed by some procedural style...

  CenterlinePoint() : m_chunkIdx(-1), m_posBuilt(false), m_dirsBuilt(false) {}
  CenterlinePoint(int chunk, double t)
      : m_chunkIdx(chunk)
      , m_t(t)
      , m_posBuilt(false)
      , m_dirsBuilt(false)
      , m_countIdx(0) {}

  ~CenterlinePoint() {}

  void buildPos(const TStroke &stroke);
  void buildDirs(const TStroke &stroke);

  bool operator<(const CenterlinePoint &cp) const {
    return m_chunkIdx < cp.m_chunkIdx
               ? true
               : m_chunkIdx > cp.m_chunkIdx ? false : m_t < cp.m_t;
  }
};

//********************************************************************************
//    Linearizator Classes
//********************************************************************************

/*!
  The StrokeLinearizator class models a stroke linearization interface that
  extracts points of interest from a succession of stroke quadratics.
*/
class StrokeLinearizator {
protected:
  const TStroke *m_stroke;

public:
  StrokeLinearizator(const TStroke *stroke) : m_stroke(stroke) {}
  virtual ~StrokeLinearizator() {}

  /*!
Adds interesting stroke points to be discretized in the
chunk-th TThickQuadratic stroke.

\note The initial point (P0) of the quadratic is always added by the
outlinization algorithm before these linearization functions are invoked
(whereas P2 belongs to the next quadratic).
*/
  virtual void linearize(std::vector<CenterlinePoint> &cPoints, int chunk) = 0;
};

//********************************************************************************
//    Outline Builder classes
//********************************************************************************

/*!
  The OutlineBuilder class is the object used to translate centerline points
  into outline points. The purpose of this class is to take a CenterlinePoint
  instance and build a couple of outline points - at either side of the
  centerline - eventually adding (cap) points to form a smooth outline.
*/
class OutlineBuilder {
  double m_pixSize;
  TStroke::OutlineOptions m_oOptions;

  int m_lastChunk;

private:
  typedef void (OutlineBuilder::*OutlineBuilderFunc)(
      std::vector<TOutlinePoint> &outPoints, const CenterlinePoint &cPoint);

  OutlineBuilderFunc m_addBeginCap;
  OutlineBuilderFunc m_addEndCap;
  OutlineBuilderFunc m_addSideCaps;

  typedef void (OutlineBuilder::*BBoxBuilderFunc)(
      TRectD &bbox, const CenterlinePoint &cPoint);

  BBoxBuilderFunc m_addBeginCap_ext;
  BBoxBuilderFunc m_addEndCap_ext;
  BBoxBuilderFunc m_addSideCaps_ext;

private:
  /*
Type-specific outline container functions.
Used with outline building sub-routines that may be used to supply
different outline container types.
For example, a TRectD may be considered a container class to be used
when building the outline bbox.
*/
  template <typename T>
  void addEnvelopePoint(T &container, const TPointD &oPoint, int countIdx = 0);
  template <typename T>
  void addExtensionPoint(T &container, const TPointD &oPoint, int countIdx = 0);
  template <typename T>
  void addOutlineBuilderFunc(OutlineBuilder::OutlineBuilderFunc func,
                             T &container, const CenterlinePoint &cPoint);

public:
  OutlineBuilder(const OutlinizationData &data, const TStroke &stroke);
  ~OutlineBuilder() {}

  /*!
Transforms the specified centerline point into outline points, and adds them to
the supplied outline points vector.
*/
  void buildOutlinePoints(std::vector<TOutlinePoint> &outPoints,
                          const CenterlinePoint &cPoint);

  void buildOutlineExtensions(TRectD &bbox, const CenterlinePoint &cPoint);

private:
  void addCircle(std::vector<TOutlinePoint> &oPoints,
                 const CenterlinePoint &cPoint);
  void addCircularArcPoints(int idx, std::vector<TOutlinePoint> &outPoints,
                            const TPointD &center, const TPointD &ray,
                            double angle, int nAngles, int countIdx);

  void addRoundBeginCap(std::vector<TOutlinePoint> &oPoints,
                        const CenterlinePoint &cPoint);
  void addRoundEndCap(std::vector<TOutlinePoint> &oPoints,
                      const CenterlinePoint &cPoint);

  void addButtBeginCap(std::vector<TOutlinePoint> &oPoints,
                       const CenterlinePoint &cPoint);
  void addButtEndCap(std::vector<TOutlinePoint> &oPoints,
                     const CenterlinePoint &cPoint);

  template <typename T>
  void addProjectingBeginCap(T &oPoints, const CenterlinePoint &cPoint);
  template <typename T>
  void addProjectingEndCap(T &oPoints, const CenterlinePoint &cPoint);

  void addRoundSideCaps(std::vector<TOutlinePoint> &oPoints,
                        const CenterlinePoint &cPoint);
  void addBevelSideCaps(std::vector<TOutlinePoint> &oPoints,
                        const CenterlinePoint &cPoint);
  template <typename T>
  void addMiterSideCaps(T &oPoints, const CenterlinePoint &cPoint);
};

//********************************************************************************
//    Explicit specializations of OutlineBuilder's methods
//********************************************************************************

// Container of Outline Points (for common outline extraction)

template <>
inline void OutlineBuilder::addEnvelopePoint(
    std::vector<TOutlinePoint> &oPoints, const TPointD &oPoint, int countIdx) {
  oPoints.push_back(TOutlinePoint(oPoint, countIdx));
}

template <>
inline void OutlineBuilder::addExtensionPoint(
    std::vector<TOutlinePoint> &oPoints, const TPointD &oPoint, int countIdx) {
  oPoints.push_back(TOutlinePoint(oPoint, countIdx));
}

template <>
inline void OutlineBuilder::addOutlineBuilderFunc(
    OutlineBuilder::OutlineBuilderFunc func,
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  (this->*func)(oPoints, cPoint);
}

//============================================================================================

// Rect (for bounding box extraction)

template <>
inline void OutlineBuilder::addEnvelopePoint(TRectD &bbox,
                                             const TPointD &oPoint,
                                             int countIdx) {}

template <>
inline void OutlineBuilder::addExtensionPoint(TRectD &bbox,
                                              const TPointD &oPoint,
                                              int countIdx) {
  bbox.x0 = std::min(bbox.x0, oPoint.x);
  bbox.y0 = std::min(bbox.y0, oPoint.y);
  bbox.x1 = std::max(bbox.x1, oPoint.x);
  bbox.y1 = std::max(bbox.y1, oPoint.y);
}

template <>
inline void OutlineBuilder::addOutlineBuilderFunc(
    OutlineBuilder::OutlineBuilderFunc func, TRectD &container,
    const CenterlinePoint &cPoint) {}

//********************************************************************************
//    Standard Outline Builder (from Centerline Points)
//********************************************************************************

void buildOutline(const TStroke &stroke, std::vector<CenterlinePoint> &cPoints,
                  TStrokeOutline &outline, const OutlinizationData &data);

}  // namespace tellipticbrush

#endif  // TELLIPTIC_BRUSH_P_H
