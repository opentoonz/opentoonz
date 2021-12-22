

#include "tregionprop.h"
#include "tstroke.h"
#include "tregion.h"
//#include "tcurves.h"
//#include "tgl.h"
//#include "tcolorfunctions.h"
#include "tmathutil.h"
#include "drawutil.h"
#include "tvectorrenderdata.h"
//#include "tcolorstyles.h"
#include "tsimplecolorstyles.h"
//#include "tcurveutil.h"
//#include "tdebugmessage.h"

#ifndef _WIN32
#define CALLBACK
#endif

//=============================================================================

TRegionProp::TRegionProp(const TRegion *region)
    : m_region(region), m_regionChanged(true) {}

//-------------------------------------------------------------------

//===================================================================
namespace {

//-------------------------------------------------------------------

bool computeOutline(const TRegion *region,
                    TRegionOutline::PointVector &polyline,
                    const double pixelSize) {
  bool doAntialiasing = false;
  polyline.clear();

  std::vector<TPointD> polyline2d;
  std::vector<int> indices;

  int i, edgeSize = region->getEdgeCount(), oldSize = 0;
  for (i = 0; i < edgeSize; i++) {
    TEdge &edge = *region->getEdge(i);

    if (edge.m_index >= 0 && edge.m_s) {
      bool outline = (edge.m_s->getAverageThickness() == 0.0);
      if (outline && !doAntialiasing) {
        // The region must be antialiased if it has at least one invisible
        // (0-thin) edge
        doAntialiasing = true;

        // Plus, edges will have to be reproduced independently from the edge
        // side
        indices.reserve(edgeSize);
      }

      if (outline && (edge.m_w0 > edge.m_w1)) {
        int newSize = polyline2d.size();
        if (oldSize < newSize) {
          // There is a sequence of forward vertices
          indices.push_back(newSize - oldSize);
          oldSize = newSize;
        }

        stroke2polyline(polyline2d, *edge.m_s, pixelSize, edge.m_w1, edge.m_w0,
                        true);

        // Insert the sequence of backward vertices
        newSize = polyline2d.size();
        indices.push_back(oldSize - newSize);
        oldSize = newSize;
      } else
        stroke2polyline(polyline2d, *edge.m_s, pixelSize, edge.m_w0, edge.m_w1);
    }
  }

  // Copy points to the output
  int pointNumber = polyline2d.size();
  polyline.reserve(pointNumber);

  int j, k, l, indicesCount = indices.size();
  for (i = 0, k = 0; k < indicesCount; ++k) {
    l = indices[k];
    if (l == 0) continue;

    if (l > 0) {
      l = i + l;
      for (j = i; j < l; ++j) polyline.push_back(T3DPointD(polyline2d[j], 0.0));
      i      = l;
    } else {
      l = i - l;
      j = l - 1;

      // Inverted sequences are symmetric by construction.
      // If necessary, chop the last element.
      if (polyline2d[i] == polyline2d[j]) ++i;

      for (; j >= i; --j) polyline.push_back(T3DPointD(polyline2d[j], 0.0));
      i = l;
    }
  }

  // Finally, copy the remaining (forward) ones
  for (; i < pointNumber; ++i)
    polyline.push_back(T3DPointD(polyline2d[i], 0.0));

  return doAntialiasing;
}

/*!
 This function accept a polygon which can have autointersections,
 and creates a number of not-autointersecting polygons. the second function is
 for recursive calls.
 It is used for The GlTessellator
*/
//-------------------------------------------------------------------

//-------------------------------------------------------------------
}  // end namaspace
//-------------------------------------------------------------------

//-------------------------------------------------------------------

void OutlineRegionProp::computeRegionOutline() {
  int subRegionNumber = getRegion()->getSubregionCount();
  TRegionOutline::PointVector app;

  m_outline.m_exterior.clear();

  computeOutline(getRegion(), app, m_pixelSize);
  m_outline.m_doAntialiasing = true;

  m_outline.m_exterior.push_back(app);
  m_outline.m_interior.clear();
  m_outline.m_interior.reserve(subRegionNumber);
  for (int i = 0; i < subRegionNumber; i++) {
    app.clear();
    computeOutline(getRegion()->getSubregion(i), app, m_pixelSize);
    m_outline.m_doAntialiasing = true;
    m_outline.m_interior.push_back(app);
  }

  m_outline.m_bbox = getRegion()->getBBox();
}

//-------------------------------------------------------------------

OutlineRegionProp::OutlineRegionProp(const TRegion *region,
                                     const TOutlineStyleP colorStyle)
    : TRegionProp(region), m_pixelSize(0), m_colorStyle(colorStyle) {
  m_styleVersionNumber = m_colorStyle->getVersionNumber();
}

//-------------------------------------------------------------------

void OutlineRegionProp::draw(const TVectorRenderData &rd) {
  if (rd.m_clippingRect != TRect() && !rd.m_is3dView &&
      !(rd.m_aff * getRegion()->getBBox()).overlaps(convert(rd.m_clippingRect)))
    return;

  glPushMatrix();
  tglMultMatrix(rd.m_aff);
  double pixelSize = sqrt(tglGetPixelSize2());

  if (!isAlmostZero(pixelSize - m_pixelSize, 1e-5) || m_regionChanged ||
      m_styleVersionNumber != m_colorStyle->getVersionNumber()) {
    m_pixelSize     = pixelSize;
    m_regionChanged = false;
    computeRegionOutline();
    TOutlineStyle::RegionOutlineModifier *modifier =
        m_colorStyle->getRegionOutlineModifier();
    if (modifier) modifier->modify(m_outline);

    m_styleVersionNumber = m_colorStyle->getVersionNumber();
  }

  assert(!m_outline.m_exterior.empty());

  m_colorStyle->drawRegion(rd.m_cf, rd.m_antiAliasing && rd.m_regionAntialias,
                           m_outline);

  glPopMatrix();
}

//-------------------------------------------------------------------

TRegionProp *OutlineRegionProp::clone(const TRegion *region) const {
  OutlineRegionProp *prop = new OutlineRegionProp(region, m_colorStyle);
  prop->m_regionChanged   = m_regionChanged;
  prop->m_pixelSize       = m_pixelSize;
  prop->m_outline         = m_outline;
  return prop;
}

//-------------------------------------------------------------------

const TColorStyle *OutlineRegionProp::getColorStyle() const {
  return m_colorStyle.getPointer();
}
