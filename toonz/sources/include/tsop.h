#pragma once

#ifndef TSOP_INCLUDED
#define TSOP_INCLUDED

#include "tsound.h"

#undef DVAPI
#undef DVVAR
#ifdef TSOUND_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

class DVAPI TSopException final : public TException {
  TString m_message;

public:
  TSopException(const TString &s) : m_message(s) {}
  ~TSopException() {}

  TString getMessage() const override { return m_message; };
};

//===================================================================
//! \include sop_ex.cpp

namespace TSop {

/*!
    Convert the soundtrack src to the format of the soundtrack dst
  */
DVAPI void convert(TSoundTrackP &dst, const TSoundTrackP &src);

/*!
    Convert the soundtrack src to the format specified in dstFormat
    and return the new soundtrack
  */
DVAPI TSoundTrackP convert(const TSoundTrackP &src,
                           const TSoundTrackFormat &dstFormat);

/*!
    Resampls the soundtrack src at the specified sampleRate and
    returns the obtained soundtrack
  */
DVAPI TSoundTrackP resample(TSoundTrackP src, TINT32 sampleRate);

/*!
    Reverbs the soundtrack src with the specified feature and
    returns the obtained soundtrack
  */
DVAPI TSoundTrackP reverb(TSoundTrackP src, double delayTime,
                          double decayFactor, double extendTime);

/*!
    Gates the soundtrack src with the specified feature and
    returns the obtained soundtrack
  */
DVAPI TSoundTrackP gate(TSoundTrackP src, double threshold, double holdTime,
                        double releaseTime);

/*!
    Does the echo effect to the soundtrack src with the specified feature
  */
DVAPI void echo(TSoundTrackP &dst, const TSoundTrackP &src, double delayTime,
                double decayFactor, double extendTime);

/*!
    Streches the soundtrack src with the specified feature and
    returns the obtained soundtrack
  */
DVAPI TSoundTrackP timeStrech(TSoundTrackP src, double ratio);

/*!
    Do the mixing between the two soundtrack a1 and a2 must be inside [0.0,1.0]
  */
DVAPI TSoundTrackP mix(const TSoundTrackP &st1, const TSoundTrackP &st2,
                       double a1, double a2);

/*!
    Inserts l blank samples starting from the sample s0 of the soundtrack.
  */
DVAPI TSoundTrackP insertBlank(TSoundTrackP src, TINT32 s0, TINT32 l);

/*!
    Inserts a blank portion of duration l into the soundtrack, starting from t0.
    t0 and l are expressed in seconds.
  */
DVAPI TSoundTrackP insertBlank(TSoundTrackP src, double t0, double l);

/*!
    Removes from the soundtrack the samples in the range [s0, s1].
    Returns a soundtrack that contains just the samples that have been removed
  */
DVAPI TSoundTrackP remove(TSoundTrackP src, TINT32 s0, TINT32 s1,
                          TSoundTrackP &paste);

/*!
    Remove the portion of soundtrack in the range [t0, t1].
    t0 and t1 are expressed in seconds.
    Returns a soundtrack that contains just the portion that has been removed.
  */
DVAPI TSoundTrackP remove(TSoundTrackP src, double t0, double t1,
                          TSoundTrackP &paste);

/*!
    Returns a soundtrack that has just the riseFactor of sample of src.
    The samples of the output soundTrack come from the "zero" sample to
    the value of the first sample of src
  */
DVAPI TSoundTrackP fadeIn(const TSoundTrackP src, double riseFactor);

/*!
    Returns a soundtrack that has just the decayFactor of sample of src.
    The samples of the output soundTrack come from the value of the last sample
    of src to the "zero" sample
  */
DVAPI TSoundTrackP fadeOut(const TSoundTrackP src, double decayFactor);

/*!
    Returns a soundtrack that has just the crossFactor of sample of src2.
    The samples of the output soundTrack come from the value of the last sample
    of src1 to the value of the first sample of src2
  */
DVAPI TSoundTrackP crossFade(const TSoundTrackP src1, const TSoundTrackP src2,
                             double crossFactor);

/*!
    Returns a soundtrack that has the sampleCount of src2.
    But the first crossFactor of samples come from the value of the last sample
    of src1 to the value of the crossFactor-th sample of src2
  */
DVAPI TSoundTrackP crossFade(double crossFactor, const TSoundTrackP src1,
                             const TSoundTrackP src2);
}

#endif
