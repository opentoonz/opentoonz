#pragma once

#ifndef MODIFIERSIMPLIFY_INCLUDED
#define MODIFIERSIMPLIFY_INCLUDED

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
//    TModifierSimplify definition
//*****************************************************************************************

class DVAPI TModifierSimplify: public TInputModifier {
public:
  typedef TSubTrackHandler Handler;
  typedef TTrackIntrOrig Interpolator;
  
  double step;
  
  explicit TModifierSimplify(double step = 1.0);

  void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks ) override;
};


#endif
