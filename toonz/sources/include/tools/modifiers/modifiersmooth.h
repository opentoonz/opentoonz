#pragma once

#ifndef MODIFIERSMOOTH_INCLUDED
#define MODIFIERSMOOTH_INCLUDED

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
//    TModifierSmooth definition
//*****************************************************************************************

class DVAPI TModifierSmooth: public TInputModifier {
public:
  class DVAPI Handler: public TSubTrackHandler {
  public:
    const int radius;
    inline explicit Handler(int radius): radius(radius) { }
  };

  int radius;

  explicit TModifierSmooth(int radius = 10);
  
  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;
};


#endif

