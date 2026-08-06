

// TnzCore includes
#include "tfilepath.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"

#include "toonz/onionskinmask.h"

//*****************************************************************************************
//    Macros
//*****************************************************************************************

#define MINFADE 0.35
#define MAXFADE 0.95

//*****************************************************************************************
//    Local namespace
//*****************************************************************************************

namespace {

double inline getIncrement(int paperThickness) {
  struct locals {
    inline static void fillIncrements(double *values, int a, int b) {
      double slope = (values[b] - values[a]) / (b - a);
      for (int i = a + 1; i < b; ++i) values[i] = values[i - 1] + slope;
    }
  };  // locals

  static double Incr[101] = {-1.0};

  if (Incr[0] == -1.0) {
    Incr[0]   = 0.0;
    Incr[10]  = 0.05;
    Incr[50]  = 0.12;
    Incr[90]  = 0.3;
    Incr[100] = MAXFADE - MINFADE;

    locals::fillIncrements(Incr, 0, 10);
    locals::fillIncrements(Incr, 10, 50);
    locals::fillIncrements(Incr, 50, 90);
    locals::fillIncrements(Incr, 90, 100);
  }

  return Incr[paperThickness];
}

}  // namespace

//***************************************************************************
//    OnionSkinMask  implementation
//***************************************************************************

void OnionSkinMask::clear() {
  m_fos.clear();
  m_mos.clear();

  m_shiftTraceStatus = DISABLED;

  m_ghostAff[0]    = TAffine();
  m_ghostAff[1]    = TAffine();
  m_ghostCenter[0] = TPointD();
  m_ghostCenter[1] = TPointD();
  m_ghostFrame[0]  = 0;
  m_ghostFrame[1]  = 0;
}

//-------------------------------------------------------------------

void OnionSkinMask::getAll(int currentRow, std::vector<int> &output) const {
  output.clear();
  output.reserve(m_fos.size() + m_mos.size());

  MarkerList::const_iterator fosIt, fosEnd(m_fos.end());
  MarkerList::const_iterator mosIt, mosEnd(m_mos.end());

  for (fosIt = m_fos.begin(), mosIt = m_mos.begin();
       fosIt != fosEnd && mosIt != mosEnd;) {
    int fos = fosIt->first, mos = mosIt->first + currentRow;

    if (fos < mos) {
      if (fos != currentRow) output.push_back(fos);

      ++fosIt;
    } else if (mos < fos) {
      if (mos != currentRow) output.push_back(mos);

      ++mosIt;
    } else {
      if (fos != currentRow) output.push_back(fos);
      ++fosIt;
      ++mosIt;
    }
  }

  for (; fosIt != fosEnd; ++fosIt)
    if (fosIt->first != currentRow) output.push_back(fosIt->first);

  for (; mosIt != mosEnd; ++mosIt) {
    int mos = mosIt->first + currentRow;
    if (mos != currentRow) output.push_back(mos);
  }
}

//-------------------------------------------------------------------

OnionSkinMask::MarkerList::iterator OnionSkinMask::findMarker(
    MarkerList &markers, int position) {
  return std::lower_bound(
      markers.begin(), markers.end(), position,
      [](const Marker &marker, int value) { return marker.first < value; });
}

//-------------------------------------------------------------------

OnionSkinMask::MarkerList::const_iterator OnionSkinMask::findMarker(
    const MarkerList &markers, int position) {
  return std::lower_bound(
      markers.begin(), markers.end(), position,
      [](const Marker &marker, int value) { return marker.first < value; });
}

//-------------------------------------------------------------------

void OnionSkinMask::setMos(int drow, bool on) {
  assert(drow != 0);

  MarkerList::iterator marker = findMarker(m_mos, drow);

  if (on) {
    if (marker == m_mos.end() || marker->first != drow)
      m_mos.insert(marker, Marker(drow, -1.0));
  } else {
    if (marker != m_mos.end() && marker->first == drow) m_mos.erase(marker);
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setFos(int row, bool on) {
  MarkerList::iterator marker = findMarker(m_fos, row);

  if (on) {
    if (marker == m_fos.end() || marker->first != row)
      m_fos.insert(marker, Marker(row, -1.0));
  } else {
    if (marker != m_fos.end() && marker->first == row) m_fos.erase(marker);
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setMosOpacity(int drow, double opacity) {
  assert(opacity == -1.0 || (opacity >= 0.0 && opacity <= 1.0));
  MarkerList::iterator marker = findMarker(m_mos, drow);
  if (marker != m_mos.end() && marker->first == drow)
    marker->second = opacity;
}

//-------------------------------------------------------------------

double OnionSkinMask::getMosOpacity(int drow) const {
  MarkerList::const_iterator marker = findMarker(m_mos, drow);
  return marker != m_mos.end() && marker->first == drow ? marker->second : -1.0;
}

//-------------------------------------------------------------------

void OnionSkinMask::setFosOpacity(int row, double opacity) {
  assert(opacity == -1.0 || (opacity >= 0.0 && opacity <= 1.0));
  MarkerList::iterator marker = findMarker(m_fos, row);
  if (marker != m_fos.end() && marker->first == row)
    marker->second = opacity;
}

//-------------------------------------------------------------------

double OnionSkinMask::getFosOpacity(int row) const {
  MarkerList::const_iterator marker = findMarker(m_fos, row);
  return marker != m_fos.end() && marker->first == row ? marker->second : -1.0;
}

//-------------------------------------------------------------------

bool OnionSkinMask::isFos(int row) const {
  MarkerList::const_iterator marker = findMarker(m_fos, row);
  return marker != m_fos.end() && marker->first == row;
}

//-------------------------------------------------------------------

bool OnionSkinMask::isMos(int drow) const {
  MarkerList::const_iterator marker = findMarker(m_mos, drow);
  return marker != m_mos.end() && marker->first == drow;
}

//-------------------------------------------------------------------

bool OnionSkinMask::getMosRange(int &drow0, int &drow1) const {
  if (m_mos.empty()) {
    drow0 = 0, drow1 = -1;
    return false;
  } else {
    drow0 = m_mos.front().first, drow1 = m_mos.back().first;
    return true;
  }
}

//-------------------------------------------------------------------

double OnionSkinMask::getOnionSkinFade(int rowsDistance) {
  if (rowsDistance == 0) return 0.9;

  double fade =
      MINFADE +
      abs(rowsDistance) *
          getIncrement(Preferences::instance()->getOnionPaperThickness());
  return tcrop(fade, MINFADE, MAXFADE);
}

//-------------------------------------------------------------------

void OnionSkinMask::setShiftTraceGhostAff(int index, const TAffine &aff) {
  assert(0 <= index && index < 2);
  m_ghostAff[index] = aff;
}

//-------------------------------------------------------------------

void OnionSkinMask::setShiftTraceGhostCenter(int index, const TPointD &center) {
  assert(0 <= index && index < 2);
  m_ghostCenter[index] = center;
}

//***************************************************************************
//    OnionSkinMaskModifier  implementation
//***************************************************************************

OnionSkinMaskModifier::OnionSkinMaskModifier(OnionSkinMask mask, int currentRow)
    : m_oldMask(mask)
    , m_curMask(mask)
    , m_firstRow(0)
    , m_lastRow(0)
    , m_curRow(currentRow)
    , m_status(0) {}

//-------------------------------------------------------------------

void OnionSkinMaskModifier::click(int row, bool isFos) {
  assert(m_status == 0);

  m_firstRow = m_lastRow = row;
  if (isFos) {
    assert(row != m_curRow);

    if (m_curMask.isEnabled() && m_curMask.isFos(row)) {
      m_status = 2;  // spegnere fos
      m_curMask.setFos(row, false);
    } else {
      if (!m_curMask.isEnabled()) {
        m_curMask.clear();
        m_curMask.enable(true);
      }

      m_curMask.setFos(row, true);
      m_status = 3;  // accendere fos
    }
  } else {
    int drow = row - m_curRow;
    if (drow != 0 && m_curMask.isEnabled() && m_curMask.isMos(drow)) {
      m_status = 4;  // spegnere mos
      m_curMask.setMos(drow, false);
    } else if (drow == 0) {
      m_status = 8 + 4 + 1;  // accendere mos; partito da 0
    } else {
      if (!m_curMask.isEnabled()) m_curMask.enable(true);
      m_curMask.setMos(drow, true);
      m_status = 4 + 1;  // accendere mos;
    }
  }
}

//-------------------------------------------------------------------

void OnionSkinMaskModifier::drag(int row) {
  if (m_status & 128) return;

  if (row == m_lastRow) return;

  m_status |= 64;  // moved

  int n = row - m_lastRow, d = 1;
  if (n < 0) n = -n, d = -d;

  int oldr = m_lastRow, r = oldr + d;

  for (int i = 0; i < n; ++i, r += d) {
    if (m_status & 4) {
      if (!m_curMask.isEnabled()) {
        m_curMask.clear();
        m_curMask.enable(true);
      }
      if (r != m_curRow) m_curMask.setMos(r - m_curRow, (m_status & 1) != 0);
    } else
      m_curMask.setFos(r, (m_status & 1) != 0);
  }

  m_lastRow = row;
}

//-------------------------------------------------------------------

void OnionSkinMaskModifier::release(int row) {
  if (m_status & 128) return;
  if ((m_status & 64) == 0    // non si e' mosso
      && (m_status & 8) == 8  // e' partito da zero
      && row == m_curRow) {
    if (!m_curMask.isEmpty() && m_curMask.isEnabled())
      m_curMask.enable(false);
    else {
      m_curMask.enable(true);
      if (m_curMask.isEmpty()) {
        m_curMask.setMos(-1, true);
        m_curMask.setMos(-2, true);
        m_curMask.setMos(-3, true);
      }
    }
  }
}
