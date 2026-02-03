#pragma once

#ifndef MODIFIERLINE_INCLUDED
#define MODIFIERLINE_INCLUDED

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
//    TModifierLine definition
//*****************************************************************************************

class DVAPI TModifierLine: public TInputModifier {
public:
  class DVAPI Handler: public TSubTrackHandler {
  public:
    double maxPressure;
    inline Handler(): maxPressure() { }
  };

  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;
};

#endif
