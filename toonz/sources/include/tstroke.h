#pragma once

#ifndef TSTROKE_H
#define TSTROKE_H

#include <memory>

#include "tsmartpointer.h"
#include "tgeometry.h"
#include "tthreadmessage.h"

#include <QMutex>

#undef DVAPI
#undef DVVAR

#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

// Forward declarations

class TStrokeStyle;
class TVectorRenderData;
class TColorStyle;
class TThickQuadratic;
class TStrokeProp;
class TSegment;

//===================================================================

//***************************************************************************
//    TStroke class
//***************************************************************************

/*!
  This is the new release of TStroke (Version 4).
  It's possible to use style to set brush, texture, pattern,
  TStroke now are smart pointer.
 */
class DVAPI TStroke final : public TSmartObject {
  DECLARE_CLASS_CODE

private:
  //! Pimpl of a TStroke
  struct Imp;
  std::unique_ptr<Imp> m_imp;

public:
  struct OutlineOptions;

public:
  typedef BYTE TStrokeFlag;

  static const TStrokeFlag c_selected_flag;
  static const TStrokeFlag c_changed_region_flag;
  static const TStrokeFlag c_dirty_flag;

  //! to be erased
  TStroke(const TStroke &stroke);
  //! to be erased
  TStroke &operator=(const TStroke &other);

  //! Costructor
  TStroke();

  //! Build a stroke from an array of ThickPoint.
  /*!
TStroke constructor build a stroke from an array of control points.
If chunks are n control points are 2*n+1.
\note Common control points between two chunks are supposed not replicated.
*/
  TStroke(const std::vector<TThickPoint> &);

  //! Build a stroke from an array of TPointD
  /*!
TStroke constructor build a stroke from an array of control points.
If chunks are n control points are 2*n+1.
\note Common control points between two chunks are supposed not replicated,
      and thickness is equal 0.
*/
  TStroke(const std::vector<TPointD> &v);

  ~TStroke();

  //! mark invalid the length and the outline. The next computeOutlines() will
  //! update them
  void invalidate();

  //! computes outlines (only if it's needed). call it before drawing.

  /*!
\return false if the bbox doesn't contain the point
\par p is input point to test
\par outT is parameter of minimum
\par chunkIndex is chunk of stroke where is minimum
\par distance2 is square distance of minimum
\par checkBBox (default true) is a flag to check or not in bounding box
*/
  bool getNearestChunk(const TPointD &p, double &outT, int &chunkIndex,
                       double &distance2, bool checkBBox = true) const;

  bool getNearestW(const TPointD &p, double &outW, double &distance2,
                   bool checkBBox = true) const;

  /*!
\finds all points on stroke which are "enough" close to point p. return the
number of such points.
*/
  int getNearChunks(const TThickPoint &p,
                    std::vector<TThickPoint> &pointsOnStroke,
                    bool checkBBox = true) const;

  //! \return number of chunks in the stroke
  int getChunkCount() const;

  //! Return number of control points in a stroke
  /*!
Return number of control points in a stroke
\note the control point in common between adjacent chunks
  is counted only once. e.g.: A n-chunk TStroke has n*2+1 CP
*/
  int getControlPointCount() const;

  void getControlPoints(std::vector<TThickPoint> &v) const;

  /*!
 Return the n-th control point
 \note n is clamped to [0,ControlPointCount-1] interval
 \par n index of desired control point
*/
  TThickPoint getControlPoint(int n) const;

  /*!
 Return control point nearest of parameter w.
 \note w is clamped to [0,1] interval
*/
  TThickPoint getControlPointAtParameter(double w) const;

  int getControlPointIndexAfterParameter(double w) const;

  /*!
Set the position of n-th control point.
If "n" is not a valid position no points are changed
If the CP is shared between two chunks, then both chunks are modified
The first method doesn't change the point thickness
*/
  void setControlPoint(int n, const TThickPoint &pos);

  /*!
Set the position of n-th control point.
If "n" is not a valid position no points are changed
If the CP is shared between two chunks, then both chunks are modified
The first method doesn't change the point thickness
*/
  void setControlPoint(int n, const TPointD &pos) {
    setControlPoint(n, TThickPoint(pos, getControlPoint(n).thick));
  }

  //! Reshape stroke
  void reshape(const TThickPoint pos[], int count);

  /*!
Return chunk at position index
If index is not valid return NULL
\note it is used mainly for drawing
*/
  const TThickQuadratic *getChunk(int index) const;

  //! Return the bbox of the stroke
  TRectD getBBox(double w0 = 0.0, double w1 = 1.0) const;
  void computeBBox();

  //! Return the bbox of the stroke without thickness.
  TRectD getCenterlineBBox() const;

  //! Returns the length of the stroke at passed chunk-param.
  double getLength(int chunk, double t) const;

  //! Return length of a stroke
  double getLength(double w0, double w1) const;

  //! Return length of a stroke
  double getLength(double w1 = 1.0) const { return getLength(0.0, w1); }

  //! Return approximate length of a stroke
  double getApproximateLength(double w0, double w1, double error) const;

  /*!
Add a control point (in fact delete one and add three) without changing the
stroke shape.
s is the position of the new CP along the stroke itself. (0<s<strokeLength)

\note  if a control point already exists near s, then the stroke remains
unchanged
*/
  void insertControlPointsAtLength(double s);

  /*!
Add a control point (in fact delete one and add three) without changing the
stroke shape.
w is the position of the new CP along parameter. (0<w<1)

\note if a control point already exists near w, then the stroke remains
unchanged
*/
  void insertControlPoints(double w);  // , minDistance2 = .1

  //! Reduce the number of control point according to \b maxError parameter
  void reduceControlPoints(double maxError);

  //! Reduce the number of control point according to \b maxError parameter.
  //! The vector corners contain the corner of the stroke.
  void reduceControlPoints(double maxError, std::vector<int> corners);

  /*!
Return a thickpoint at w (parameter (0<=w<=1))
\note w is 0 at begin of the stroke and is 1.0 at the end.
    Parameter value for a chunk is stored in the vector
    m_parameterValueAtChunk.
    Every change (subdivision of a stroke, resample, insertion of a control
point)
    computes a new value for parameter.
*/
  TThickPoint getThickPoint(double w) const;

  /*!
same that getThickPoint but return a TPointD
*/
  inline TPointD getPoint(double w) const { return convert(getThickPoint(w)); }

  /*!
Return a thickpoint at s (length of arc)
*/
  TThickPoint getThickPointAtLength(double s) const;

  /*!
same that getThickPointAtLength but return a TPointD
*/
  inline TPointD getPointAtLength(double s) const {
    return convert(getThickPointAtLength(s));
  }

  /*!
Return parameter in (based on arc length)
*/
  double getParameterAtLength(double s) const;

  /*!
Return parameter for a control point
\note if control point is not on curve return middle value between
      two adjacent control point
*/
  double getParameterAtControlPoint(int n) const;

  //! Return the stroke leght at passed control point
  double getLengthAtControlPoint(int) const;

  //! Freeze parameter of a stroke in last valid status
  void disableComputeOfCaches();

  void enableComputeOfCaches();

  //! Return parameter w at point position
  double getW(const TPointD &) const;
  double getW(int chunkIndex, double t) const;

  //! Get speed at parameter w
  // \note w is clamped to [0,1] parameter
  // if the stroke has a cuspide on w, the tangents are two different;
  // outSpeed = false return tangent in w-,
  // outspeed = true returns tangent in w+
  TPointD getSpeed(double w, bool outSpeed = false) const;
  //! Returns true if speed1!=speed0, so it is an edge in w
  bool getSpeedTwoValues(double w, TPointD &speed0, TPointD &speed1) const;

  //! Get speed at length s
  // \note s is clamped to [0,getLength] parameter
  TPointD getSpeedAtLength(double s) const;

  /*!
Split a stroke at parameter t
\note Chunk of null length are removed.
*/
  void split(double w, TStroke &f, TStroke &s) const;

  //! Swap the contents of two strokes
  void swap(TStroke &ref);

  //! Swapd the geometrical data of two strokes
  void swapGeometry(TStroke &ref);

  //! Transform a stroke using an affine
  void transform(const TAffine &aff, bool doChangeThickness = false);

  //! \return Style
  int getStyle() const;

  /*!
 Set a style
 \note If the same style exists, it is replaced by new
 \note TStroke becomes the owner of TStrokeStyle
*/
  void setStyle(int styleId);

  //! Return a \b TStrokeProp
  TStrokeProp *getProp(/*const TVectorPalette *palette*/) const;
  //! Set \b TSrokePrope of a stroke
  void setProp(TStrokeProp *prop);

  // void deleteStyle(int);

  //! Only for debug
  void print(std::ostream &os = std::cout) const;

  //! change tangent versus in the stroke
  /*!
change tangent versus in a stroke
\note Change versus of all thickquadratric and change its postion in list
*/
  TStroke &changeDirection();

  //! Set \b TStrokeFlag of the stroke
  void setFlag(TStrokeFlag flag, bool status);
  //! Return \b TStrokeFlag of the stroke
  bool getFlag(TStrokeFlag flag) const;

  //! Gets the chunk and its parameter at stroke parameter \b w; returns true if
  //! a chunk was found.
  bool getChunkAndT(double w, int &chunk, double &t) const;

  //! Gets the chunk and its parameter at stroke length \b s; returns true if a
  //! chunk was found.
  bool getChunkAndTAtLength(double s, int &chunk, double &t) const;

  TThickPoint getCentroid() const;

  //! Set a loop stroke
  void setSelfLoop(bool loop = true);

  //! Return true if the stroke is looped
  bool isSelfLoop() const;

  //! Return true if all control points have negative thickness
  bool isCenterLine() const;

  //! Get a stroke from a \b TThickPoint vector
  /*!
Create a \b T3DPointD vector froma a \b TThickPoint vector. The \b T3DPointD
vector is used to
find a \b TCubicStroke; than find the quadratic stroke.
*/
  static TStroke *interpolate(const std::vector<TThickPoint> &points,
                              double error, bool findCorners = true);

  //! Get a stroke from a \b TThickQuadratic vector
  /*!
Take from \b curves the control points used to create the stroke
*/
  static TStroke *create(const std::vector<TThickQuadratic *> &curves);

  int getId() const;
  void setId(int id);

  //! Return the average value of stroke thickness
  double getAverageThickness() const;

  double getMaxThickness();

  //! Set the average value of stroke thickness to \b thickness
  void setAverageThickness(double thickness);

  bool operator==(const TStroke &s) const;

  bool operator!=(const TStroke &s) const { return !operator==(s); }

  OutlineOptions &outlineOptions();
  const OutlineOptions &outlineOptions() const;
};

//-----------------------------------------------------------------------------

typedef TSmartPointerT<TStroke> TStrokeP;

//***************************************************************************
//    TStrokeProp class
//***************************************************************************

class DVAPI TStrokeProp {
protected:
  const TStroke *m_stroke;
  bool m_strokeChanged;
  int m_styleVersionNumber;
  TThread::Mutex m_mutex;

public:
  TStrokeProp(const TStroke *stroke);
  virtual ~TStrokeProp() {}

  virtual TStrokeProp *clone(const TStroke *stroke) const = 0;

  TThread::Mutex *getMutex() { return &m_mutex; }

  virtual void draw(const TVectorRenderData &rd) = 0;

  virtual const TColorStyle *getColorStyle() const = 0;

  const TStroke *getStroke() const { return m_stroke; }
  virtual void notifyStrokeChange() { m_strokeChanged = true; }

private:
  // not implemented
  TStrokeProp(const TStrokeProp &);
  TStrokeProp &operator=(const TStrokeProp &);

  friend class TStroke;
  void setStroke(const TStroke *stroke) { m_stroke = stroke; }
};

//***************************************************************************
//    TStroke::OutlineOptions structure
//***************************************************************************

/*!
  Contains complementary stroke parameters used in the process of transforming
  the raw (centerline) stroke data in its drawable (outline) form.
*/
struct DVAPI TStroke::OutlineOptions {
  enum CapStyle { BUTT_CAP = 0, ROUND_CAP, PROJECTING_CAP };
  enum JoinStyle { MITER_JOIN = 0, ROUND_JOIN, BEVEL_JOIN };

  UCHAR m_capStyle;
  UCHAR m_joinStyle;

  double m_miterLower;
  double m_miterUpper;

public:
  OutlineOptions();
  OutlineOptions(UCHAR capStyle, UCHAR JoinStyle, double lower, double upper);
};

//***************************************************************************
//    Related Non-member functions
//***************************************************************************

//! return the stroke equal to the join of stroke1 and stroke2 (that are not
//! deleted)
DVAPI TStroke *joinStrokes(const TStroke *stroke1, const TStroke *stroke2);

//=============================================================================

/*!
  Intersection between stroke and segment.
  \par stroke to intersect
  \par segment to intersect
  \ret a vector of pair of double (first value is stroke parameter second is
  segment parameter)
 */
DVAPI int intersect(const TStroke &stroke, const TSegment &segment,
                    std::vector<DoublePair> &intersections);

DVAPI int intersect(const TSegment &segment, const TStroke &stroke,
                    std::vector<DoublePair> &intersections);

//=============================================================================

/*!
  Intersection between stroke.
  \par  first stroke
  \par  second stroke
  \par  vector for intersection
  \par  checkBBox (?)
  \ret number of intersections
 */
DVAPI int intersect(const TStroke *s1, const TStroke *s2,
                    std::vector<DoublePair> &intersections,
                    bool checkBBox = true);

//=============================================================================

DVAPI int intersect(const TStroke &stroke, const TPointD &center, double radius,
                    std::vector<double> &intersections);
//======================================s=======================================

/*
  To be replaced with new split
  and a specialyzed function.
 */
DVAPI void splitStroke(const TStroke &tq, const std::vector<double> &pars,
                       std::vector<TStroke *> &v);

/* !puts in corners the indexes of quadric junctions of the stroke that create
angles greater
than minDegree */
DVAPI void detectCorners(const TStroke *stroke, double minDegree,
                         std::vector<int> &corners);

//=============================================================================

//! retrieve stroke parameter from chunk and chunk parameter
inline double getWfromChunkAndT(const TStroke *ref, UINT chunkIndex, double t) {
  assert(ref);
  assert(ref->getChunkCount());

  return (chunkIndex + t) / ref->getChunkCount();
}

//=============================================================================

DVAPI void computeQuadraticsFromCubic(
    const TThickPoint &p0, const TThickPoint &p1, const TThickPoint &p2,
    const TThickPoint &p3, double error,
    std::vector<TThickQuadratic *> &chunkArray);

#endif  // TSTROKE_H
