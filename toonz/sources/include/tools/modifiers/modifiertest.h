
#ifndef NDEBUG
#pragma once

#ifndef MODIFIERTEST_INCLUDED
#define MODIFIERTEST_INCLUDED

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
//    TModifierTest definition
//*****************************************************************************************

class DVAPI TModifierTest : public TInputModifier {
public:
  class DVAPI Handler : public TMultiTrackHandler {
  public:
    const double radius;
    std::vector<double> angles;
    inline explicit Handler(double radius):
      radius(std::max(TConsts::epsilon, radius)) { }
  };

  class DVAPI Interpolator : public TTrackInterpolator {
  public:
    const double angle;
    const double radius;
    const double speed;
    inline Interpolator(TTrack &track, double angle, double radius, double speed):
      TTrackInterpolator(track), angle(angle), radius(radius), speed(speed) { }
    TTrackPoint interpolateFromOriginal(double originalIndex);
    TTrackPoint interpolate(double index) override;
  };

public:
  int count;
  double radius;
  double speed;

  TModifierTest(int count = 3, double radius = 40, double speed = 0.25);

  void modifyTrack(const TTrack &track,
                   TTrackList &outTracks) override;
};

#endif
#endif
