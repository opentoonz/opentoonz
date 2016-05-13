

#ifndef COLORFXUTILS_H
#define COLORFXUTILS_H

#include "tgeometry.h"
#include "tpixel.h"
#include "trandom.h"
#include "tregionoutline.h"
#include "tcurves.h"

class TRegion;
class TFlash;

class RubberDeform
{
	std::vector<T3DPointD> *m_pPolyOri;
	std::vector<T3DPointD> m_polyLoc;

	void deformStep();
	double avgLength();
	void refinePoly(const double rf = -1.0);
	void getBBox(TRectD &bbox);

public:
	RubberDeform();
	RubberDeform(std::vector<T3DPointD> *pPolyOri, const double rf = -1.0);
	void copyLoc2Ori() { *m_pPolyOri = m_polyLoc; };
	void copyOri2Loc() { m_polyLoc = *m_pPolyOri; };

	virtual ~RubberDeform();
	void deform(const double n);
};

class SFlashUtils
{
	void computeOutline(const TRegion *region, TRegionOutline::PointVector &polyline) const;
	void PointVector2QuadsArray(const std::vector<T3DPointD> &pv,
								std::vector<TQuadratic *> &quadArray,
								std::vector<TQuadratic *> &toBeDeleted,
								const bool isRounded) const;
	int nbDiffVerts(const std::vector<TPointD> &pv) const;
	void Triangle2Quad(std::vector<TPointD> &p) const;

public:
	const TRegion *m_r;
	TRegionOutline m_ro;

	SFlashUtils(){};
	SFlashUtils(const TRegion *r) : m_r(r){};
	virtual ~SFlashUtils(){};

	void computeRegionOutline();
	void drawRegionOutline(TFlash &flash, const bool isRounded = true) const;
	void drawGradedPolyline(TFlash &flash,
							std::vector<TPointD> &pv,
							const TPixel32 &c1,
							const TPixel32 &c2) const;
	void drawGradedRegion(TFlash &flash,
						  std::vector<TPointD> &pv,
						  const TPixel32 &c1,
						  const TPixel32 &c2,
						  const TRegion &r) const;
};

#endif
