#pragma once

#ifndef MODIFIERJITTER_INCLUDED
#define MODIFIERJITTER_INCLUDED

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
//    TModifierJitter definition
//*****************************************************************************************

class DVAPI TModifierJitter : public TInputModifier {
public:
  typedef TSubTrackHandler Handler;

  class DVAPI Interpolator : public TTrackInterpolator {
  public:
    const unsigned int seedX;
    const unsigned int seedY;
    const double frequency;
    const double amplitude;
    const bool keepFirstPoint;
    const bool keepLastPoint;
    Interpolator(
      TTrack &track,
      double period,
      double amplitude,
      bool keepFirstPoint,
      bool keepLastPoint );
    TTrackPoint interpolateFromOriginal(double originalIndex);
    TTrackPoint interpolate(double index) override;
  };

public:
  double period;
  double amplitude;
  int skipFirst;
  int skipLast;
  bool keepFirstPoint;
  bool keepLastPoint;
  
  explicit TModifierJitter(
    double period = 30,
    double amplitude = 10,
    int skipFirst = 0,
    int skipLast = 0,
    bool keepFirstPoint = false,
    bool keepLastPoint = false );

  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;
  
  void modifyTracks(
    const TTrackList &tracks,
    TTrackList &outTracks ) override;
  
  static double func(unsigned int seed, double x);
};

#endif
