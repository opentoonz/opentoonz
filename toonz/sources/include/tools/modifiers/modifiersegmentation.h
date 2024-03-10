#pragma once

#ifndef MODIFIERSEGMENTATION_INCLUDED
#define MODIFIERSEGMENTATION_INCLUDED

// TnzTools includes
#include <tools/inputmanager.h>


#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//===================================================================

//*****************************************************************************************
//    TModifierSegmentation definition
//*****************************************************************************************

class DVAPI TModifierSegmentation: public TInputModifier {
public:
  typedef TSubTrackHandler Handler;
  typedef TTrackIntrOrig Interpolator;
  
private:
  TPointD m_step;
  int m_maxLevel;
  
  void addSegments(TTrack &track, const TTrackPoint &p0, const TTrackPoint &p1, int maxLevel);

public:
  explicit TModifierSegmentation(const TPointD &step = TPointD(1.0, 1.0), int level = 10);

  void setStep(const TPointD &step);
  inline const TPointD& getStep() const { return m_step; }

  void setMaxLevel(int maxLevel);
  inline int getMaxLevel() const { return m_maxLevel; }
  
  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;
};


#endif
