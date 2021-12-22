

#include "autofill.h"
#include "tregion.h"
#include "tgeometry.h"
#include "tstroke.h"
#include "toonz4.6/tmacro.h"
#include <QMap>
#include <QPair>
#include <QList>

namespace {

#define BORDER_TOO 1
#define NO_BORDER 0
#define DIM_TRESH 0.00005
#define AMB_TRESH 130000
#define MIN_SIZE 20

//==============================================================================================

struct Region {
  double m_area, m_perimeter;
  TPointD m_barycentre;
  TDimensionD m_size;
  int m_match, m_styleId;
  TRegion *m_region;

  Region()
      : m_area(0)
      , m_perimeter(0)
      , m_barycentre(0, 0)
      , m_size(0, 0)
      , m_match(-1)
      , m_styleId(0)
      , m_region(0) {}
};

struct MatchingProbs {
  int m_from, m_to;
  int m_perimeterProb, m_areaProb, m_barycenterProb;
  bool m_overlappingArea, m_matched;

  MatchingProbs()
      : m_from(0)
      , m_to(0)
      , m_perimeterProb(0)
      , m_areaProb(0)
      , m_barycenterProb(0)
      , m_overlappingArea(false)
      , m_matched(false) {}
};

typedef QMap<int, Region> RegionDataList;

static RegionDataList regionsReference, regionsWork;
static TPointD referenceB(0, 0);
static TPointD workB(0, 0);

//==============================================================================================

class AreasAndPerimeterFormula final : public TRegionFeatureFormula {
  double m_signedArea, m_perimeter;

public:
  AreasAndPerimeterFormula() : m_signedArea(0), m_perimeter(0) {}

  ~AreasAndPerimeterFormula() {}

  void update(const TPointD &p1, const TPointD &p2) override {
    m_perimeter += norm(p2 - p1);
    m_signedArea += ((p1.x * p2.y) - (p2.x * p1.y)) * 0.5;
  }

  double getPerimeter() { return m_perimeter; }
  double getSignedArea() { return m_signedArea; }
  double getArea() { return fabs(m_signedArea); }
};

//---------------------------------------------------------------------------------------------

class CentroidFormula final : public TRegionFeatureFormula {
  TPointD m_centroid;
  double m_signedArea;

public:
  CentroidFormula() : m_centroid(), m_signedArea(0) {}

  ~CentroidFormula() {}

  void update(const TPointD &p1, const TPointD &p2) override {
    double factor = ((p1.x * p2.y) - (p2.x * p1.y));
    m_centroid.x += (p1.x + p2.x) * factor;
    m_centroid.y += (p1.y + p2.y) * factor;
  }

  void setSignedArea(double signedArea) { m_signedArea = signedArea; }
  TPointD getCentroid() { return (1 / (6 * m_signedArea)) * m_centroid; }
};

//---------------------------------------------------------------------------------------------

int match(std::vector<MatchingProbs> &probsVector, int &from, int &to) {
  int i = 0, maxProb = 0;
  bool overlappingArea = false;
  std::vector<MatchingProbs>::iterator it, matchedProbs;
  for (it = probsVector.begin(); it != probsVector.end(); it++) {
    MatchingProbs probs = *it;
    if (probs.m_matched) continue;
    int probValue =
        probs.m_areaProb * probs.m_barycenterProb * probs.m_perimeterProb;
    if ((!overlappingArea &&
         (maxProb < probValue || probs.m_overlappingArea)) ||
        (overlappingArea && maxProb < probValue && probs.m_overlappingArea)) {
      overlappingArea = probs.m_overlappingArea;
      maxProb         = probValue;
      from            = probs.m_from;
      to              = probs.m_to;
      matchedProbs    = it;
    }
  }
  if (maxProb) matchedProbs->m_matched = true;
  return maxProb;
}

//---------------------------------------------------------------------------------------------

void assignProbs(std::vector<MatchingProbs> &probVector,
                 const Region &reference, const Region &work, int from,
                 int to) {
  double delta_posx1, delta_posy1, delta_posx2, delta_posy2;
  int delta_area, delta_per;
  double delta_pos, delta_pos_max;

  MatchingProbs probs;
  probs.m_from = from;
  probs.m_to   = to;

  TRegion *refRegion  = reference.m_region;
  TRegion *workRegion = work.m_region;
  probs.m_overlappingArea =
      refRegion->getBBox().overlaps(workRegion->getBBox());

  delta_posx1 = reference.m_barycentre.x / reference.m_area - referenceB.x;
  delta_posy1 = reference.m_barycentre.y / reference.m_area - referenceB.y;

  delta_posx2 = work.m_barycentre.x / work.m_area - workB.x;
  delta_posy2 = work.m_barycentre.y / work.m_area - workB.y;

  // Cosi' calcolo il modulo della differenza

  delta_pos = sqrt((delta_posx2 - delta_posx1) * (delta_posx2 - delta_posx1) +
                   (delta_posy2 - delta_posy1) * (delta_posy2 - delta_posy1));
  delta_pos_max = sqrt((double)(work.m_size.lx * work.m_size.lx +
                                work.m_size.ly * work.m_size.ly));

  probs.m_barycenterProb = tround(1000 * (1 - (delta_pos / delta_pos_max)));

  delta_area = std::abs(reference.m_area - work.m_area);

  probs.m_areaProb = tround(
      1000 * (1 - ((double)delta_area / (reference.m_area + work.m_area))));

  delta_per             = std::abs(reference.m_perimeter - work.m_perimeter);
  probs.m_perimeterProb = tround(
      1000 *
      (1 - ((double)delta_per / (reference.m_perimeter + work.m_perimeter))));
  probVector.push_back(probs);
}

//----------------------------------------------------------------------------------------------

void scanRegion(TRegion *reg, int regionIndex, RegionDataList &rlst,
                const TRectD &selectingRect) {
  assert(!rlst.contains(regionIndex));
  if (!selectingRect.contains(reg->getBBox())) return;

  AreasAndPerimeterFormula areasAndPerimeter;
  CentroidFormula centroid;
  computeRegionFeature(*reg, areasAndPerimeter);
  computeRegionFeature(*reg, centroid);
  centroid.setSignedArea(areasAndPerimeter.getSignedArea());

  Region regionData;
  regionData.m_area       = areasAndPerimeter.getArea();
  regionData.m_perimeter  = areasAndPerimeter.getPerimeter();
  regionData.m_barycentre = centroid.getCentroid() * regionData.m_area;
  regionData.m_size       = reg->getBBox().getSize();
  UINT i, subRegCount = reg->getSubregionCount();
  for (i = 0; i < subRegCount; i++) {
    TRegion *subReg = reg->getSubregion(i);
    AreasAndPerimeterFormula subAreasAndPerimeter;
    CentroidFormula subCentroid;
    computeRegionFeature(*subReg, subAreasAndPerimeter);
    computeRegionFeature(*subReg, subCentroid);
    subCentroid.setSignedArea(subAreasAndPerimeter.getSignedArea());

    regionData.m_area -= subAreasAndPerimeter.getArea();
    regionData.m_barycentre -=
        subCentroid.getCentroid() * subAreasAndPerimeter.getArea();
  }
  regionData.m_barycentre.x /= regionData.m_area;
  regionData.m_barycentre.y /= regionData.m_area;
  regionData.m_styleId = reg->getStyle();
  regionData.m_region  = reg;

  rlst[regionIndex] = regionData;
}

//----------------------------------------------------------------------------------------------

void scanSubRegion(TRegion *region, int &index, RegionDataList &rlst,
                   const TRectD &selectingRect) {
  scanRegion(region, index, rlst, selectingRect);
  index++;
  int j, subRegionCount = region->getSubregionCount();
  for (j = 0; j < subRegionCount; j++) {
    TRegion *subRegion = region->getSubregion(j);
    scanSubRegion(subRegion, index, rlst, selectingRect);
  }
}

//----------------------------------------------------------------------------------------------

bool contains(TRegion *container, TRegion *contained) {
  if (!(container->getBBox().contains(contained->getBBox()))) return false;

  for (UINT i = 0; i < contained->getEdgeCount(); i++)
    for (UINT j = 0; j < container->getEdgeCount(); j++)
      if (*contained->getEdge(i) == *container->getEdge(j)) return false;
  for (UINT i = 0; i < contained->getEdgeCount(); i++) {
    TEdge *e = contained->getEdge(i);
    if (!container->contains(e->m_s->getThickPoint(e->m_w0))) return false;
    if (!container->contains(e->m_s->getThickPoint((e->m_w0 + e->m_w1) / 2.0)))
      return false;
    if (!container->contains(e->m_s->getThickPoint(e->m_w1))) return false;
  }
  return true;
}

}  // namespace

//==============================================================================================

void rect_autofill_learn(const TVectorImageP &imgToLearn, const TRectD &rect)

{
  if (rect.getLx() * rect.getLy() < MIN_SIZE) return;

  double pbx, pby;
  double totalArea = 0;
  pbx = pby = 0;

  if (!regionsReference.isEmpty()) regionsReference.clear();

  int i, index = 0, regionCount = imgToLearn->getRegionCount();
  for (i = 0; i < regionCount; i++) {
    TRegion *region = imgToLearn->getRegion(i);
    if (rect.contains(region->getBBox())) {
      scanRegion(region, index, regionsReference, rect);
      index++;
    }
    int j, subRegionCount = region->getSubregionCount();
    for (j = 0; j < subRegionCount; j++) {
      TRegion *subRegion = region->getSubregion(j);
      if (rect.contains(subRegion->getBBox()))
        scanSubRegion(subRegion, index, regionsReference, rect);
    }
  }

  QMap<int, Region>::Iterator it;
  for (it = regionsReference.begin(); it != regionsReference.end(); it++) {
    pbx += it.value().m_barycentre.x;
    pby += it.value().m_barycentre.y;
    totalArea += it.value().m_area;
  }

  if (totalArea > 0)
    referenceB = TPointD(pbx / totalArea, pby / totalArea);
  else
    referenceB = TPointD(0.0, 0.0);
}

//----------------------------------------------------------------------------

bool rect_autofill_apply(const TVectorImageP &imgToApply, const TRectD &rect,
                         bool selective) {
  if (rect.getLx() * rect.getLy() < MIN_SIZE) return false;

  if (regionsReference.size() <= 0) return false;

  double pbx, pby;
  double totalArea = 0;
  pbx = pby = 0.0;

  if (!regionsWork.isEmpty()) regionsWork.clear();

  int i, index = 0, regionCount = imgToApply->getRegionCount();
  for (i = 0; i < regionCount; i++) {
    TRegion *region = imgToApply->getRegion(i);
    TRectD bbox     = region->getBBox();
    if (rect.contains(bbox)) {
      scanRegion(region, index, regionsWork, rect);
      index++;
    }
    int j, subRegionCount = region->getSubregionCount();
    for (j = 0; j < subRegionCount; j++) {
      TRegion *subRegion = region->getSubregion(j);
      if (rect.contains(subRegion->getBBox()))
        scanSubRegion(subRegion, index, regionsWork, rect);
    }
  }

  if (regionsWork.size() <= 0) return false;

  QMap<int, Region>::Iterator it;
  for (it = regionsWork.begin(); it != regionsWork.end(); it++) {
    pbx += it.value().m_barycentre.x;
    pby += it.value().m_barycentre.y;
    totalArea += it.value().m_area;
  }

  workB = TPointD(pbx / totalArea, pby / totalArea);

  std::vector<MatchingProbs> probVector;

  RegionDataList::Iterator refIt, workIt;
  for (refIt = regionsReference.begin(); refIt != regionsReference.end();
       refIt++)
    for (workIt = regionsWork.begin(); workIt != regionsWork.end(); workIt++)
      assignProbs(probVector, refIt.value(), workIt.value(), refIt.key(),
                  workIt.key());

  bool filledRegions = false;
  for (refIt = regionsReference.begin(); refIt != regionsReference.end();
       refIt++) {
    int to = 0, from = 0;
    int valore = 0;
    do
      valore = match(probVector, from, to);
    while ((regionsWork[to].m_match != -1 ||
            regionsReference[from].m_match != -1) &&
           valore > 0);
    if (valore > AMB_TRESH) {
      regionsWork[to].m_match        = from;
      regionsReference[from].m_match = to;
      regionsWork[to].m_styleId      = regionsReference[from].m_styleId;
      TRegion *reg                   = regionsWork[to].m_region;
      if (reg && (!selective || (selective && reg->getStyle() == 0))) {
        reg->setStyle(regionsWork[to].m_styleId);
        filledRegions = true;
      }
    }
  }
  return filledRegions;
}

//----------------------------------------------------------------------------

void stroke_autofill_learn(const TVectorImageP &imgToLearn, TStroke *stroke) {
  if (!imgToLearn || !stroke || stroke->getControlPointCount() == 0) return;
  TVectorImage appImg;
  TStroke *appStroke = new TStroke(*stroke);
  appImg.addStroke(appStroke);
  appImg.findRegions();

  double pbx, pby;
  double totalArea = 0;
  pbx = pby = 0;

  if (!regionsReference.isEmpty()) regionsReference.clear();

  int i, j, index = 0;

  for (i = 0; i < (int)imgToLearn->getRegionCount(); i++) {
    TRegion *currentRegion = imgToLearn->getRegion(i);
    for (j = 0; j < (int)appImg.getRegionCount(); j++) {
      TRegion *region = appImg.getRegion(j);
      if (contains(region, currentRegion)) {
        scanRegion(currentRegion, index, regionsReference, region->getBBox());
        index++;
        int k, subRegionCount = currentRegion->getSubregionCount();
        for (k = 0; k < subRegionCount; k++) {
          TRegion *subRegion = currentRegion->getSubregion(k);
          if (contains(region, subRegion))
            scanSubRegion(subRegion, index, regionsReference,
                          region->getBBox());
        }
      }
    }
  }

  QMap<int, Region>::Iterator it;
  for (it = regionsReference.begin(); it != regionsReference.end(); it++) {
    pbx += it.value().m_barycentre.x;
    pby += it.value().m_barycentre.y;
    totalArea += it.value().m_area;
  }

  if (totalArea > 0)
    referenceB = TPointD(pbx / totalArea, pby / totalArea);
  else
    referenceB = TPointD(0.0, 0.0);
}

//----------------------------------------------------------------------------

bool stroke_autofill_apply(const TVectorImageP &imgToApply, TStroke *stroke,
                           bool selective) {
  if (!imgToApply || !stroke || stroke->getControlPointCount() == 0)
    return false;
  TVectorImage appImg;
  TStroke *appStroke = new TStroke(*stroke);
  appImg.addStroke(appStroke);
  appImg.findRegions();

  if (regionsReference.size() <= 0) return false;

  double pbx, pby;
  double totalArea = 0;
  pbx = pby = 0.0;

  if (!regionsWork.isEmpty()) regionsWork.clear();

  int i, j, index = 0;

  for (i = 0; i < (int)imgToApply->getRegionCount(); i++) {
    TRegion *currentRegion = imgToApply->getRegion(i);
    for (j = 0; j < (int)appImg.getRegionCount(); j++) {
      TRegion *region = appImg.getRegion(j);
      if (contains(region, currentRegion)) {
        scanRegion(currentRegion, index, regionsWork, region->getBBox());
        index++;
        int k, subRegionCount = currentRegion->getSubregionCount();
        for (k = 0; k < subRegionCount; k++) {
          TRegion *subRegion = currentRegion->getSubregion(k);
          if (contains(region, subRegion))
            scanSubRegion(subRegion, index, regionsWork, region->getBBox());
        }
      }
    }
  }

  if (regionsWork.size() <= 0) return false;

  QMap<int, Region>::Iterator it;
  for (it = regionsWork.begin(); it != regionsWork.end(); it++) {
    pbx += it.value().m_barycentre.x;
    pby += it.value().m_barycentre.y;
    totalArea += it.value().m_area;
  }

  workB = TPointD(pbx / totalArea, pby / totalArea);

  std::vector<MatchingProbs> probVector;

  RegionDataList::Iterator refIt, workIt;
  for (refIt = regionsReference.begin(); refIt != regionsReference.end();
       refIt++)
    for (workIt = regionsWork.begin(); workIt != regionsWork.end(); workIt++)
      assignProbs(probVector, refIt.value(), workIt.value(), refIt.key(),
                  workIt.key());

  bool filledRegions = false;
  for (refIt = regionsReference.begin(); refIt != regionsReference.end();
       refIt++) {
    int to = 0, from = 0;
    int valore = 0;
    do
      valore = match(probVector, from, to);
    while ((regionsWork[to].m_match != -1 ||
            regionsReference[from].m_match != -1) &&
           valore > 0);
    if (valore > AMB_TRESH) {
      regionsWork[to].m_match        = from;
      regionsReference[from].m_match = to;
      regionsWork[to].m_styleId      = regionsReference[from].m_styleId;
      TRegion *reg                   = regionsWork[to].m_region;
      if (reg && (!selective || (selective && reg->getStyle() == 0))) {
        reg->setStyle(regionsWork[to].m_styleId);
        filledRegions = true;
      }
    }
  }
  return filledRegions;
}
