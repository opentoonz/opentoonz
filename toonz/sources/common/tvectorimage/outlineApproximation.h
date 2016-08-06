#pragma once

// outlineApproximation.h: interface for the outlineApproximation class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(OUTLINEAPPROXIMATION_H)
#define OUTLINEAPPROXIMATION_H

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif  // _MSC_VER > 1000

#include <vector>

class TQuadratic;
class TStroke;

typedef std::pair<TQuadratic *, TQuadratic *> outlineEdge;
typedef std::vector<outlineEdge> outlineBoundary;

void drawOutline(const outlineBoundary &, double pixelSize);

void computeOutline(const TStroke *stroke, std::vector<TQuadratic *> &quadArray,
                    double error2)

/*
ONLY FOT TEST
class TThickQuadratic;
class TSegment;
extern  TSegment  g_tangEnvelope_1;
extern  TSegment  g_tangEnvelope_2;
extern  std::vector<TQuadratic>  g_testOutline;
TQuadratic  makeOutline( const TThickQuadratic* tq, int upOrDown );
*/
#endif  // !defined(OUTLINEAPPROXIMATION_H)
