#pragma once

#ifndef MODIFIERASSISTANTS_INCLUDED
#define MODIFIERASSISTANTS_INCLUDED

// TnzTools includes
#include <tools/assistant.h>
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
//    TModifierAssistants definition
//*****************************************************************************************

class DVAPI TModifierAssistants: public TInputModifier {
public:
  typedef TSubTrackHandler Handler;
  class DVAPI Interpolator: public TTrackInterpolator {
  public:
    const double magnetism;
    TGuidelineList guidelines;
    inline Interpolator(TTrack &track, double magnetism):
      TTrackInterpolator(track),
      magnetism(magnetism > 0 ? (magnetism < 1 ? magnetism : 1) : 0)
      { }
    TTrackPoint interpolate(double index) override;
  };

private:
  bool scanAssistants(
    const TPointD *positions,
    int positionsCount,
    TGuidelineList *outGuidelines,
    bool draw,
    bool enabledOnly,
    bool drawGuidelines ) const;

public:
  double magnetism;
  double sensitiveLength;

  explicit TModifierAssistants(double magnetism = 1);

  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;

  TRectD calcDrawBounds(const TTrackList &tracks, const THoverList &hovers) override;

   void drawTrack(const TTrack &track) override;
   void draw(const TTrackList &tracks, const THoverList &hovers) override;
};


#endif
