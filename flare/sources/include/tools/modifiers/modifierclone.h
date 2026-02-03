#pragma once

#ifndef MODIFIERCLONE_INCLUDED
#define MODIFIERCLONE_INCLUDED

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
//    TModifierClone definition
//*****************************************************************************************

class DVAPI TModifierClone : public TInputModifier {
public:
  class DVAPI Handler : public TMultiTrackHandler {
  public:
    TTrackP original;
    inline explicit Handler(const TTrackP &original = TTrackP()):
      original(original) { }
  };

  class DVAPI Interpolator : public TTrackInterpolator {
  public:
    const TTrackTransform transform;
    inline Interpolator(TTrack &track, const TTrackTransform &transform):
      TTrackInterpolator(track), transform(transform) { }
    TTrackPoint interpolateFromOriginal(double originalIndex);
    TTrackPoint interpolate(double index) override;
  };


public:
  bool keepOriginals;
  TTrackTransformList transforms;
  int skipFirst;
  int skipLast;

  explicit TModifierClone(
    bool keepOriginals = true,
    int skipFirst = 0,
    int skipLast = 0 );

  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;

  void modifyTracks(
    const TTrackList &tracks,
    TTrackList &outTracks ) override;
};

#endif
