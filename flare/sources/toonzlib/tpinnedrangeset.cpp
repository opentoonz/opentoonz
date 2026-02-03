

#include "toonz/tpinnedrangeset.h"
#include "tstream.h"

//-----------------------------------------------------------------------------

TPinnedRangeSet::TPinnedRangeSet() : m_stageObject() {}

//-----------------------------------------------------------------------------

int TPinnedRangeSet::getRangeIndex(int frame) const {
  if (m_ranges.empty()) return -1;
  if (frame < m_ranges.front().first || m_ranges.back().second < frame)
    return -1;
  int start = 0, end = (int)m_ranges.size() - 1;
  while (start < end) {
    if (start + 1 == end) {
      if (m_ranges[end].first <= frame)
        start = end;
      else
        end = start;
      break;
    }
    int m = (start + end) / 2;
    assert(start < m && m < end);
    if (m_ranges[m].first <= frame)
      start = m;
    else
      end = m;
  }
  int index = start;
  if (m_ranges[index].first <= frame && frame <= m_ranges[index].second)
    return index;
  else
    return -1;
}

//-----------------------------------------------------------------------------

void TPinnedRangeSet::setRange(int first, int second) {
  std::vector<Range>::iterator it;
  // finds the range which could possibly be merged before : range.second+1 >=
  // first
  for (it = m_ranges.begin(); it != m_ranges.end(); ++it)
    if (it->second + 1 >= first) break;

  if (it == m_ranges.end()) {
    // not found => for every range : range.second+1 < first. Just add the new
    // one
    m_ranges.push_back(Range(first, second));
  } else if (it->first > second + 1) {
    // it comes after and can not be merged. Just insert the new one
    m_ranges.insert(it, Range(first, second));
  } else {
    // merge
    if (first < it->first) it->first = first;
    // update it->second, and find all the ranges which should be deleted
    std::vector<Range>::iterator it2 = it;
    for (++it2; it2 != m_ranges.end(); ++it2)
      if (it2->first > second + 1)
        break;
      else
        it->second                      = it2->second;
    if (second > it->second) it->second = second;
    ++it;
    if (it != it2) m_ranges.erase(it, it2);
  }
}

//-----------------------------------------------------------------------------

void TPinnedRangeSet::removeRange(int first, int second) {
  std::vector<Range>::iterator it;
  // finds the first range which could possibly overlap [first,second] :
  // range.second>=first
  for (it = m_ranges.begin(); it != m_ranges.end(); ++it)
    if (it->second >= first) break;

  if (it == m_ranges.end() || it->first > second) {
    // nothing to do: for each range range.second < first or range.first >
    // second
  } else if (it->first < first && it->second > second) {
    // [first,second] is a "hole"
    Range range(it->first, first - 1);
    it->first = second + 1;
    m_ranges.insert(it, range);
  } else {
    std::vector<Range>::iterator it2;
    if (it->first < first) {
      it->second = first - 1;
      ++it;
    }
    it2 = it;
    while (it != m_ranges.end() && it->second <= second) ++it;
    if (it != m_ranges.end() && it->first <= second) it->first = second + 1;
    if (it2 != it) m_ranges.erase(it2, it);
  }
}

//-----------------------------------------------------------------------------

void TPinnedRangeSet::removeAllRanges() { m_ranges.clear(); }

//-----------------------------------------------------------------------------

void TPinnedRangeSet::setPlacement(const TAffine &placement) {
  m_placement = placement;
}

//-----------------------------------------------------------------------------

TPinnedRangeSet *TPinnedRangeSet::clone() {
  TPinnedRangeSet *clonedPinnedRangeSet = new TPinnedRangeSet();
  clonedPinnedRangeSet->m_stageObject   = m_stageObject;
  clonedPinnedRangeSet->m_placement     = m_placement;
  clonedPinnedRangeSet->m_ranges        = m_ranges;
  return clonedPinnedRangeSet;
}

//-----------------------------------------------------------------------------

void TPinnedRangeSet::loadData(TIStream &is) {
  m_ranges.clear();
  int prevFrame = 0;
  int i         = 0;
  std::string tagName;
  int count = 0;
  while (is.matchTag(tagName) && count < 3) {
    if (tagName == "permanent") {
      while (!is.matchEndTag()) {
        i++;
        int frame = 0;
        is >> frame;
        if (i % 2 == 0) m_ranges.push_back(Range(prevFrame, frame));
        prevFrame = frame;
      }
      count++;
    } else if (tagName == "temp") {
      assert(0);
      // OBSOLETO
      while (!is.matchEndTag()) {
        int frame = 0;
        is >> frame;
        // m_tempPinned.push_back(frame);
      }
      count++;
    } else if (tagName == "lockedAngle") {
      assert(0);
      // OBSOLETO
      while (!is.matchEndTag()) {
        int rangeIndex = -1;
        is >> rangeIndex;
      }

      count++;
    } else if (tagName == "placement") {
      is >> m_placement.a11 >> m_placement.a12 >> m_placement.a13;
      is >> m_placement.a21 >> m_placement.a22 >> m_placement.a23;
      is.matchEndTag();
    }
  }
}

//-----------------------------------------------------------------------------

void TPinnedRangeSet::saveData(TOStream &os) {
  if ((int)m_ranges.size() == 0) return;
  os.openChild("pinnedStatus");
  if (m_ranges.size() > 0) {
    os.openChild("permanent");
    for (int p = 0; p < (int)m_ranges.size(); p++) {
      os << m_ranges[p].first << m_ranges[p].second;
    }
    os.closeChild();
  }
  if (m_placement != TAffine()) {
    os.openChild("placement");
    os << m_placement.a11 << m_placement.a12 << m_placement.a13;
    os << m_placement.a21 << m_placement.a22 << m_placement.a23;
    os.closeChild();
  }
  os.closeChild();
}
