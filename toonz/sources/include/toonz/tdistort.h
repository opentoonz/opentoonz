#pragma once

#ifndef TDISTORT_H
#define TDISTORT_H

#include "tcommon.h"
#include "tgeometry.h"
#include "traster.h"
#include "trop.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//================================================================================================
//
// TDistorter
//
//================================================================================================

//! The TDistorter class provides the base template for implementing custom
//! image distorters that
//! work with the \b distort() method.
class DVAPI TDistorter {
public:
  TDistorter() {}
  virtual ~TDistorter() {}

  //! Returns the distorted image of point \b p.
  virtual TPointD map(const TPointD &p) const = 0;

  //! Returns the pre-images of distorted point \b p. The results are overed on
  //! the output image
  //! as they appear in the returned vector, so the user must be sure to sort
  //! them accordingly.
  //! \b NOTE: The results array is already allocated with the number of entries
  //! specified by the
  //! maxInvCount() function in order to avoid allocation at this level;
  //! however, the number of
  //! effective inverses must be specified as a return value.
  virtual int invMap(const TPointD &p, TPointD *results) const = 0;

  virtual int maxInvCount() const = 0;
};

//================================================================================================
//
// TQuadDistorter
//
//================================================================================================

//! The TQuadDistorter is just a convenience base class to implement
//! quad-to-quad
//! freeform distorters.
class DVAPI TQuadDistorter : public TDistorter {
protected:
  TPointD m_p00s, m_p10s, m_p01s, m_p11s;
  TPointD m_p00d, m_p10d, m_p01d, m_p11d;

public:
  TQuadDistorter(const TPointD &p00s, const TPointD &p10s, const TPointD &p01s,
                 const TPointD &p11s, const TPointD &p00d, const TPointD &p10d,
                 const TPointD &p01d, const TPointD &p11d)
      : m_p00s(p00s)
      , m_p10s(p10s)
      , m_p01s(p01s)
      , m_p11s(p11s)
      , m_p00d(p00d)
      , m_p10d(p10d)
      , m_p01d(p01d)
      , m_p11d(p11d) {}
  virtual ~TQuadDistorter() {}

  void getSourceQuad(TPointD &p00, TPointD &p10, TPointD &p01,
                     TPointD &p11) const {
    p00 = m_p00s;
    p10 = m_p10s;
    p01 = m_p01s;
    p11 = m_p11s;
  }
  void getDestinationQuad(TPointD &p00, TPointD &p10, TPointD &p01,
                          TPointD &p11) const {
    p00 = m_p00d;
    p10 = m_p10d;
    p01 = m_p01d;
    p11 = m_p11d;
  }

  //! This function is reimplementable to provide a mean of bounding the source
  //! region that will
  //! be mapped to the passed rect, typically for fxs efficiency support.
  virtual TRectD invMap(const TRectD &rect) const {
    return TConsts::infiniteRectD;
  }
  virtual int prova() const { return 1; }
};

//================================================================================================
//
// BilinearDistorter
//
//================================================================================================

//! The BilinearDistorterBase is just a convenience class implementing a
//! lightweight version of
//! \b BilinearDistorter class when the source quad is a rect.
class DVAPI BilinearDistorterBase final : public TQuadDistorter {
  // Used to make things a little bit faster.
  TPointD m_A, m_B, m_C, m_D;
  double m_a, m_b0;

public:
  BilinearDistorterBase(const TPointD &p00s, const TPointD &p10s,
                        const TPointD &p01s, const TPointD &p11s,
                        const TPointD &p00d, const TPointD &p10d,
                        const TPointD &p01d, const TPointD &p11d);

  TPointD map(const TPointD &p) const override;
  int invMap(const TPointD &p, TPointD *results) const override;
  int maxInvCount() const override { return 2; }
};

//------------------------------------------------------------------------------------------------

//! The BilinearDistort class implements a quadrilateral distorter which maps
//! bilinear combinations
//! of the source corners into the same combinations of destination ones. The
//! resulting image deformation
//! is similar to bending a paper foil.
class DVAPI BilinearDistorter final : public TQuadDistorter {
  struct Base {
    TPointD m_p00, m_p10, m_p01, m_p11;
    // Used to make things a little bit faster.
    TPointD m_A, m_B, m_C, m_D;
    double m_a, m_b0;

    TPointD map(const TPointD &p) const;
    int invMap(const TPointD &p, TPointD *results) const;
  };

  Base m_refToSource;
  Base m_refToDest;

public:
  BilinearDistorter(const TPointD &p00s, const TPointD &p10s,
                    const TPointD &p01s, const TPointD &p11s,
                    const TPointD &p00d, const TPointD &p10d,
                    const TPointD &p01d, const TPointD &p11d);
  ~BilinearDistorter();

  TPointD map(const TPointD &p) const override;
  int invMap(const TPointD &p, TPointD *results) const override;
  int maxInvCount() const override { return 2; }
  TRectD invMap(const TRectD &rect) const override;
};

//================================================================================================
//
// PerspectiveDistorter
//
//================================================================================================

//! The PerspectiveDistorter class implements a quadrilateral distorter that
//! makes the source quad
//! bend into the destination one while keeping a perspectical resemblance.
class DVAPI PerspectiveDistorter final : public TQuadDistorter {
  //================================================================================================
  // TPerspect
  //================================================================================================
public:
  class DVAPI TPerspect {
  public:
    double a11, a12, a13;
    double a21, a22, a23;
    double a31, a32, a33;

    TPerspect();
    TPerspect(double p11, double p12, double p13, double p21, double p22,
              double p23, double p31, double p32, double p33);
    TPerspect(const TPerspect &p);
    ~TPerspect();

    TPerspect &operator=(const TPerspect &p);
    TPerspect operator*(const TPerspect &p) const;
    TPerspect operator*=(const TPerspect &p);
    TPerspect operator*(const TAffine &p) const;
    TPerspect operator*=(const TAffine &p);
    TPerspect inv() const;
    double det() const;
    bool operator==(const TPerspect &p) const;
    bool operator!=(const TPerspect &p) const;
    bool isIdentity(double err = 1.e-8) const;
    TPointD operator*(const TPointD &p) const;
    T3DPointD operator*(const T3DPointD &p) const;
    TRectD operator*(const TRectD &rect) const;
  };

private:
  TPerspect m_matrix;
  TPerspect m_matrixInv;

public:
  PerspectiveDistorter(const TPointD &p00s, const TPointD &p10s,
                       const TPointD &p01s, const TPointD &p11s,
                       const TPointD &p00d, const TPointD &p10d,
                       const TPointD &p01d, const TPointD &p11d);
  ~PerspectiveDistorter();

  const TPerspect &getMatrix() const { return m_matrix; }

  TPointD map(const TPointD &p) const override;
  int invMap(const TPointD &p, TPointD *results) const override;
  int maxInvCount() const override { return 1; }
  TRectD invMap(const TRectD &rect) const override;

private:
  //! Compute the matrix used to distort image.
  //! First is compute a transformation A' starting from m_startPoints to (0,0),
  //! (0,1), (1,1), (1,0);
  //! the a second tranformation A'' is compute starting from (0,0), (0,1),
  //! (1,1), (1,0) to m_endPoints.
  //! The resulting matrix is A'*A''.
  void computeMatrix();
  double determinant(double a11, double a12, double a21, double a22);
  TPerspect computeSquareToMatrix(const TPointD &p00, const TPointD &p10,
                                  const TPointD &p01, const TPointD &p11);

  void getJacobian(const TPointD &destPoint, TPointD &srcPoint, TPointD &xDer,
                   TPointD &yDer) const;
};

PerspectiveDistorter::TPerspect operator*(
    const TAffine &aff, const PerspectiveDistorter::TPerspect &p);

//===========================================================================================

void DVAPI distort(TRasterP &outRas, const TRasterP &inRas,
                   const TDistorter &distorter, const TPoint &dstPos,
                   const TRop::ResampleFilterType &filter = TRop::Bilinear);

#endif  // TDISTORT_H
