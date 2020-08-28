#pragma once

#ifndef MODIFIERTANGENTS_INCLUDED
#define MODIFIERTANGENTS_INCLUDED

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
//    TModifierTangents definition
//*****************************************************************************************

class DVAPI TModifierTangents : public TInputModifier {
public:
  class DVAPI Modifier : public TTrackModifier {
  public:
    explicit Modifier(TTrackHandler &handler) : TTrackModifier(handler) {}

    TInputSavePoint::Holder savePoint;
    TTrackTangentList tangents;

    TTrackPoint calcPoint(double originalIndex) override;
  };

  TTrackTangent calcLastTangent(const TTrack &track) const;

  void modifyTrack(const TTrack &track,
                   const TInputSavePoint::Holder &savePoint,
                   TTrackList &outTracks) override;
};

#endif
