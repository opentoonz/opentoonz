

// The following contains classes and prototypes for most code of centerline
// vectorization.
#include "tcenterlinevectP.h"

// TnzLib includes
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/Naa2TlvConverter.h"

// TnzCore includes
#include "tpalette.h"
#include "tcolorstyles.h"
#include "trastercm.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tgeometry.h"
#include "tstroke.h"
#include "tropcm.h"

// STD includes
#include <vector>
#include <list>
#include <queue>
#include <map>
#include <functional>
#include <algorithm>
#include <math.h>
#include <assert.h>

//==========================================================================

//*********************************
//*     Further miscellaneous     *
//*********************************

//--------------------------------------------------------------------------

// Reduces strokes of image by currConfig->m_reduceThicknessRatio/100
inline void reduceThickness(TVectorImageP image, double thicknessRatio) {
  thicknessRatio *= 0.01;

  unsigned int i;
  int j;
  for (i = 0; i < image->getStrokeCount(); ++i) {
    for (j = 0; j < image->getStroke(i)->getControlPointCount(); ++j) {
      TThickPoint newCP = image->getStroke(i)->getControlPoint(j);
      newCP.thick *= thicknessRatio;
      image->getStroke(i)->setControlPoint(j, newCP);
    }
  }
}

//========================================================================

// Delete Skeleton graphs and list
inline void deleteSkeletonList(SkeletonList *skeleton) {
  unsigned int i;
  for (i = 0; i < skeleton->size(); ++i) delete (*skeleton)[i];

  delete skeleton;
}

//========================================================================

static TVectorImageP copyStrokes(std::vector<TStroke *> &strokes) {
  unsigned int i;
  TVectorImageP result = new TVectorImage;

  for (i = 0; i < strokes.size(); ++i)
    if (strokes[i]->getStyle() >= 0) result->addStroke(strokes[i]);

  return result;
}

//========================================================================

inline TThickPoint randomized(const TThickPoint &P) {
  TThickPoint Q = P;

  Q.x += ((((double)rand()) / RAND_MAX) - 0.5) * 0.1;
  Q.y += ((((double)rand()) / RAND_MAX) - 0.5) * 0.1;

  return Q;
}

//------------------------------------------------------------------------

// Give stroke extremities a random shake. May help for region computing.
static void randomizeExtremities(TVectorImageP vi) {
  unsigned int i;
  for (i = 0; i < vi->getStrokeCount(); ++i) {
    TThickPoint P = vi->getStroke(i)->getControlPoint(0);
    vi->getStroke(i)->setControlPoint(0, randomized(P));
    int n = vi->getStroke(i)->getControlPointCount();
    P     = vi->getStroke(i)->getControlPoint(n - 1);
    vi->getStroke(i)->setControlPoint(n - 1, randomized(P));
  }
}

//========================================================================

// Looped frame
inline void addFrameStrokes(TVectorImageP vi, TRasterP ras, TPalette *palette) {
  const double epsThick = 1.0;
  TStroke *frameStroke;
  std::vector<TThickPoint> CPs;

  CPs.push_back(TThickPoint(0, 0, epsThick));
  CPs.push_back(TThickPoint(ras->getLx() / 2.0, 0, epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), 0, epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), 0, epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), 0, epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), ras->getLy() / 2.0, epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(ras->getLx(), ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(ras->getLx() / 2.0, ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(0, ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(0, ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(0, ras->getLy(), epsThick));
  CPs.push_back(TThickPoint(0, ras->getLy() / 2.0, epsThick));
  CPs.push_back(TThickPoint(0, 0, epsThick));

  frameStroke = new TStroke(CPs);
  frameStroke->setStyle(0);
  frameStroke->setSelfLoop(true);
  vi->addStroke(frameStroke);
}

//========================================================================

// Returns if input stroke is edge boundary of an ink-filled region
bool VectorizerCore::isInkRegionEdge(TStroke *stroke) {
  return stroke->getFlag(SkeletonArc::SS_OUTLINE);
}

// Similar as above
bool VectorizerCore::isInkRegionEdgeReversed(TStroke *stroke) {
  return stroke->getFlag(SkeletonArc::SS_OUTLINE_REVERSED);
}

void VectorizerCore::clearInkRegionFlags(TVectorImageP vi) {
  int flag = SkeletonArc::SS_OUTLINE | SkeletonArc::SS_OUTLINE_REVERSED;
  for (int i = 0; i < (int)vi->getStrokeCount(); i++)
    vi->getStroke(i)->setFlag(flag, false);
}

//==========================================================================

//************************
//*    Vectorizer Main   *
//************************

//--------------------------------------------------------------------------

TVectorImageP VectorizerCore::centerlineVectorize(
    TImageP &image, const CenterlineConfiguration &configuration,
    TPalette *palette) {
  TRasterImageP ri = image;
  TToonzImageP ti  = image;

  TRasterP ras;
  if (ri)
    ras = ri->getRaster();
  else {
    ras = ti->getRaster()->clone();
    TRop::expandPaint(ras);
  }

  if (configuration.m_naaSource) {
    if (TRaster32P ras32 = ras) {
      Naa2TlvConverter converter;

      converter.process(ras32);
      converter.setPalette(palette);

      QList<int> dummy;
      ti = converter.makeTlv(true, dummy);
      if (ti)  // Transparent synthetic inks
      {
        image = ti;
        ras   = ti->getRaster();

        TRop::expandPaint(ras);
      }
    }
  }

  VectorizerCoreGlobals globals;
  globals.currConfig = &configuration;

  Contours polygons;
  polygonize(ras, polygons, globals);

  // Most time-consuming part of vectorization, 'this' is passed to inform of
  // partial progresses
  SkeletonList *skeletons = skeletonize(polygons, this, globals);

  if (isCanceled()) {
    // Clean and return 0 at cancel command
    deleteSkeletonList(skeletons);

    return TVectorImageP();
  }

  organizeGraphs(skeletons, globals);

  // junctionRecovery(polygons);   //Da' problemi per maxThickness<inf...
  // sarebbe da rendere compatibile

  std::vector<TStroke *> sortibleResult;
  TVectorImageP result;

  calculateSequenceColors(ras, globals);  // Extract stroke colors here
  conversionToStrokes(sortibleResult, globals);
  applyStrokeColors(sortibleResult, ras, palette,
                    globals);  // Strokes get sorted here
  result = copyStrokes(sortibleResult);

  // Further misc adjustments
  if (globals.currConfig->m_thicknessRatio < 100)
    reduceThickness(result, configuration.m_thicknessRatio);
  if (globals.currConfig->m_maxThickness == 0.0)
    for (unsigned int i = 0; i < result->getStrokeCount(); ++i)
      result->getStroke(i)->setSelfLoop(true);
  if (globals.currConfig->m_makeFrame) addFrameStrokes(result, ras, palette);
  // randomizeExtremities(result);   //Cuccio random - non serve...

  deleteSkeletonList(skeletons);

  return result;
}
