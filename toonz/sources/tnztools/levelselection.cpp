

#include "tools/levelselection.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"

// TnzCore includes
#include "tfilepath.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include "tregion.h"

// Boost includes
#include <boost/iterator/counting_iterator.hpp>
#include <boost/bind.hpp>

//*******************************************************************************
//    Local namespace  stuff
//*******************************************************************************

namespace {

TTool *tool() { return TTool::getApplication()->getCurrentTool()->getTool(); }

//========================================================================

struct StrokeData {
  UCHAR m_hasColor, m_hasRegion;
};

void getBoundaries(TVectorImage &vi, std::vector<int> &strokes) {
  enum { FORWARD = 0x1, BACKWARD = 0x2, INTERNAL = FORWARD | BACKWARD };

  struct locals {
    static inline bool isBoundary(const std::vector<StrokeData> &sData,
                                  UINT s) {
      return (sData[s].m_hasColor != INTERNAL);
    }

    static void markEdges(const TRegion &region, std::vector<StrokeData> &sData,
                          bool parentRegionHasColor) {
      bool regionHasColor = (region.getStyle() != 0);

      // Traverse region edges, marking associated strokes accordingly
      UINT e, eCount = region.getEdgeCount();
      for (e = 0; e != eCount; ++e) {
        const TEdge &ed = *region.getEdge(e);
        assert(ed.m_s);

        int strokeIdx = ed.m_index;
        if (strokeIdx >= 0)  // Could be <0 in case the corresponding
        {                    // stroke is a region 'closure' (autoclose)
          assert(0 <= strokeIdx && strokeIdx < sData.size());

          StrokeData &sd = sData[strokeIdx];

          UCHAR side = (ed.m_w1 > ed.m_w0) ? FORWARD : BACKWARD;

          sd.m_hasRegion |= side;
          if (regionHasColor) sd.m_hasColor |= side;
        }
      }

      if (parentRegionHasColor) {
        // Mark non-region edge sides with color
        for (e = 0; e != eCount; ++e) {
          const TEdge &ed = *region.getEdge(e);
          assert(ed.m_s);

          int strokeIdx = ed.m_index;
          if (strokeIdx >= 0) {
            StrokeData &sd = sData[strokeIdx];
            sd.m_hasColor |= (INTERNAL & ~sd.m_hasRegion);
          }
        }
      }

      // Mark recursively on sub-regions
      UINT sr, srCount = region.getSubregionCount();
      for (sr = 0; sr != srCount; ++sr)
        markEdges(*region.getSubregion(sr), sData, regionHasColor);
    }
  };  // locals

  std::vector<StrokeData> sData(vi.getStrokeCount());

  // Traverse regions, mark each stroke edge with the side a COLORED region is
  // on
  UINT r, rCount = vi.getRegionCount();
  for (r = 0; r != rCount; ++r)
    locals::markEdges(*vi.getRegion(r), sData, false);

  // Strokes not appearing as region edges must be checked for region inclusion
  // separately
  UINT s, sCount = vi.getStrokeCount();
  for (s = 0; s != sCount; ++s) {
    if (!sData[s].m_hasRegion) {
      TRegion *parentRegion = vi.getRegion(vi.getStroke(s)->getPoint(0.5));

      if (parentRegion && parentRegion->getStyle())
        sData[s].m_hasColor = INTERNAL;
    }
  }

  // Output all not internal regions
  std::copy_if(boost::make_counting_iterator(0u),
               boost::make_counting_iterator(vi.getStrokeCount()),
               std::back_inserter(strokes),
               boost::bind(locals::isBoundary, sData, _1));
}

}  // namespace

//*******************************************************************************
//    VectorLevelSelection  implementation
//*******************************************************************************

LevelSelection::LevelSelection() : m_framesMode(FRAMES_NONE), m_filter(EMPTY) {}

//---------------------------------------------------------------------

bool LevelSelection::isEmpty() const {
  return (m_framesMode == FRAMES_NONE || m_filter == EMPTY);
}

//---------------------------------------------------------------------

void LevelSelection::selectNone() {
  m_framesMode = FRAMES_NONE;
  m_filter     = EMPTY;

  m_styles.clear();
}

//*******************************************************************************
//    Related standalone functions
//*******************************************************************************

std::vector<int> getBoundaryStrokes(TVectorImage &vi) {
  std::vector<int> result;
  getBoundaries(vi, result);

  return result;
}

//------------------------------------------------------------------------

std::vector<int> getSelectedStrokes(TVectorImage &vi,
                                    const LevelSelection &levelSelection) {
  struct locals {
    static void selectStyles(const TVectorImage &vi,
                             const std::set<int> &styles,
                             std::vector<int> &strokes) {
      UINT s, sCount = vi.getStrokeCount();
      for (s = 0; s != sCount; ++s) {
        if (styles.count(vi.getStroke(s)->getStyle())) strokes.push_back(s);
      }
    }
  };  // locals

  std::vector<int> strokes;

  switch (levelSelection.filter()) {
  case LevelSelection::EMPTY:
    break;
  case LevelSelection::WHOLE:
    strokes.assign(boost::make_counting_iterator(0u),
                   boost::make_counting_iterator(vi.getStrokeCount()));
    break;
  case LevelSelection::SELECTED_STYLES:
    locals::selectStyles(vi, levelSelection.styles(), strokes);
    break;
  case LevelSelection::BOUNDARY_STROKES:
    getBoundaries(vi, strokes);
    break;
  }

  return strokes;
}
