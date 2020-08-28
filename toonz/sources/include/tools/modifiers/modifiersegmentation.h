#pragma once

#ifndef MODIFIERSEGMENTATION_INCLUDED
#define MODIFIERSEGMENTATION_INCLUDED

// TnzTools includes
#include <tools/inputmanager.h>

// std includes
#include <algorithm>

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

class DVAPI TModifierSegmentation : public TInputModifier {
private:
  TPointD m_step;

  void addSegments(TTrack &track, const TTrackPoint &p0, const TTrackPoint &p1,
                   int level = 0);

public:
  explicit TModifierSegmentation(const TPointD &step = TPointD(1.0, 1.0));

  void setStep(const TPointD &step);
  const TPointD &getStep() const { return m_step; }

  void modifyTrack(const TTrack &track,
                   const TInputSavePoint::Holder &savePoint,
                   TTrackList &outTracks) override;
};

#endif
