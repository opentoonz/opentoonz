#pragma once

#ifndef KEYFRAMEDATA_INCLUDED
#define KEYFRAMEDATA_INCLUDED

#include "toonzqt/dvmimedata.h"

#include "toonz/tstageobject.h"
#include "keyframeselection.h"

// forward declarations
class TXsheet;

//=============================================================================
// TKeyframeData
//-----------------------------------------------------------------------------

class TKeyframeData : public DvMimeData {
public:
  typedef TKeyframeSelection::Position Position;
  typedef std::map<Position, TStageObject::Keyframe> KeyData;
  typedef std::map<Position, TStageObject::Keyframe>::const_iterator Iterator;

  KeyData m_keyData;

  // Numero di colonna della pegbar associato al booleano isCycleEnabled della
  // pegbar
  std::map<int, bool> m_isPegbarsCycleEnabled;

  TKeyframeData();
  TKeyframeData(const TKeyframeData *src);
  ~TKeyframeData();

  TKeyframeData *clone() const { return new TKeyframeData(this); }

  // data <- xsh
  void setKeyframes(std::set<Position> positions, TXsheet *xsh);

  // data -> xsh
  bool getKeyframes(std::set<Position> &positions, TXsheet *xsh) const;

  // Reads data keyframes and fills in positions set. The first element
  // passed with the set is automatically added to stored positions.
  void getKeyframes(std::set<Position> &positions) const;

  TKeyframeData *clone() {
    TKeyframeData *data = new TKeyframeData(this);
    return data;
  }
};

#endif  // KEYFRAMEDATA_INCLUDED
