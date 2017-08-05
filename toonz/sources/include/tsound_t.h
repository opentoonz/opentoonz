#pragma once

#ifndef TSOUND_T__INCLUDED
#define TSOUND_T__INCLUDED

#include "tsoundsample.h"

#undef DVAPI
#undef DVVAR
#ifdef TSOUND_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

template <class T>
class DVAPI TSoundTrackT final : public TSoundTrack {
public:
  typedef T SampleType;

  //----------------------------------------------------------------------------

  TSoundTrackT(TUINT32 sampleRate, int channelCount, TINT32 sampleCount)
      : TSoundTrack(sampleRate, T::getBitPerSample(), channelCount, sizeof(T),
                    sampleCount, T::isSampleSigned()) {}

  //----------------------------------------------------------------------------

  TSoundTrackT(TUINT32 sampleRate, int channelCount, TINT32 sampleCount,
               T *buffer, TSoundTrackT<T> *parent)

      : TSoundTrack(sampleRate, T::getBitPerSample(), channelCount, sizeof(T),
                    sampleCount, reinterpret_cast<UCHAR *>(buffer), parent) {}

  //----------------------------------------------------------------------------

  ~TSoundTrackT(){};

  //----------------------------------------------------------------------------

  bool isSampleSigned() const override { return T::isSampleSigned(); }

  //----------------------------------------------------------------------------

  //! Returns the const samples array
  const T *samples() const { return reinterpret_cast<T *>(m_buffer); }

  //----------------------------------------------------------------------------

  //! Returns the samples array
  T *samples() { return reinterpret_cast<T *>(m_buffer); }

  //----------------------------------------------------------------------------

  //! Returns a soundtrack whom is a clone of the object
  TSoundTrackP clone() const override {
    TSoundTrackP dst = TSoundTrack::create(getFormat(), m_sampleCount);
    TSoundTrackP src(const_cast<TSoundTrack *>((const TSoundTrack *)this));
    dst->copy(src, (TINT32)0);
    return dst;
  }

  //----------------------------------------------------------------------------

  //! Extract the subtrack in the samples range [s0,s1] given in samples
  TSoundTrackP extract(TINT32 s0, TINT32 s1) override {
    if (!m_buffer || s0 > s1) return TSoundTrackP();

    // addRef();

    TINT32 ss0, ss1;

    ss0 = tcrop<TINT32>(s0, (TINT32)0, getSampleCount() - 1);
    ss1 = tcrop<TINT32>(s1, (TINT32)0, getSampleCount() - 1);

    return TSoundTrackP(new TSoundTrackT<T>(
        getSampleRate(), getChannelCount(), ss1 - ss0 + 1,
        (T *)(m_buffer + (long)(ss0 * getSampleSize())), this));
  }

  //----------------------------------------------------------------------------

  /*!
Returns a soundtrack whom is a clone of the object for the spicified channel
A clone means that it's an object who lives indipendently from the other
from which it's created.It hasn't reference to the object.
*/
  TSoundTrackP clone(TSound::Channel chan) const override {
    if (getChannelCount() == 1)
      return clone();
    else {
      typedef typename T::ChannelSampleType TCST;
      TSoundTrackT<TCST> *dst =
          new TSoundTrackT<TCST>(m_sampleRate, 1, getSampleCount());

      const T *sample    = samples();
      const T *endSample = sample + getSampleCount();

      TCST *dstSample = dst->samples();

      while (sample < endSample) {
        *dstSample++ = sample->getValue(chan);
        sample++;
      }

      return dst;
    }
  }

  //----------------------------------------------------------------------------

  //! Copies from sample dst_s0 of object the samples of the soundtrack src
  void copy(const TSoundTrackP &src, TINT32 dst_s0) override {
    TSoundTrackT<T> *srcT = dynamic_cast<TSoundTrackT<T> *>(src.getPointer());

    if (!srcT)
      throw(
          TException("Unable to copy from a track whose format is different"));

    T *srcSample    = srcT->samples();
    T *srcEndSample = srcT->samples() + srcT->getSampleCount();
    T *dstEndSample = samples() + getSampleCount();

    TINT32 ss0 = tcrop<TINT32>(dst_s0, (TINT32)0, getSampleCount() - (TINT32)1);

    T *dstSample = samples() + ss0;

    while (srcSample < srcEndSample && dstSample < dstEndSample)
      *dstSample++ = *srcSample++;
  }

  //----------------------------------------------------------------------------

  //! Applies a trasformation (echo, reverb, ect) to the object and returns the
  //! transformed soundtrack
  TSoundTrackP apply(TSoundTransform *transform) override;
  //----------------------------------------------------------------------------

  //! Returns the pressure of the sample s about the channel chan
  double getPressure(TINT32 s, TSound::Channel chan) const override {
    assert(s >= 0 && s < getSampleCount());
    assert(m_buffer);
    const T *sample = samples() + s;
    assert(sample);
    return sample->getPressure(chan);
  }

  //----------------------------------------------------------------------------

  //! Returns the soundtrack pressure max and min values in the given sample
  //! range and channel
  void getMinMaxPressure(TINT32 s0, TINT32 s1, TSound::Channel chan,
                         double &min, double &max) const override {
    TINT32 sampleCount = getSampleCount();
    if (sampleCount <= 0) {
      min = 0;
      max = -1;
      return;
    }

    assert(s1 >= s0);
    TINT32 ss0, ss1;

    ss0 = tcrop<TINT32>(s0, (TINT32)0, sampleCount - (TINT32)1);
    ss1 = tcrop<TINT32>(s1, (TINT32)0, sampleCount - (TINT32)1);

    assert(ss1 >= ss0);

    if (s0 == s1) {
      min = max = getPressure(s0, chan);
      return;
    }

    const T *sample = samples() + ss0;
    assert(sample);
    min = max = sample->getPressure(chan);

    const T *endSample = sample + (ss1 - ss0 + 1);
    ++sample;

    while (sample < endSample) {
      double value = sample->getPressure(chan);

      if (max < value) max = value;
      if (min > value) min = value;

      ++sample;
    }
  }

  //----------------------------------------------------------------------------

  //! Returns the soundtrack pressure max value in the given sample range and
  //! channel
  double getMaxPressure(TINT32 s0, TINT32 s1,
                        TSound::Channel chan) const override {
    TINT32 sampleCount = getSampleCount();
    if (sampleCount <= 0) {
      return -1;
    }

    assert(s1 >= s0);
    TINT32 ss0, ss1;

    ss0 = tcrop<TINT32>(s0, (TINT32)0, sampleCount - (TINT32)1);
    ss1 = tcrop<TINT32>(s1, (TINT32)0, sampleCount - (TINT32)1);
    assert(ss1 >= ss0);

    if (s0 == s1) return (getPressure(s0, chan));

    const T *sample = samples() + ss0;
    assert(sample);
    double maxPressure = sample->getPressure(chan);
    const T *endSample = sample + (ss1 - ss0 + 1);
    ++sample;

    while (sample < endSample) {
      if (maxPressure < sample->getPressure(chan))
        maxPressure = sample->getPressure(chan);
      ++sample;
    }

    return ((double)maxPressure);
  }

  //----------------------------------------------------------------------------

  //! Returns the soundtrack pressure min value in the given sample range and
  //! channel
  double getMinPressure(TINT32 s0, TINT32 s1,
                        TSound::Channel chan) const override {
    TINT32 sampleCount = getSampleCount();
    if (sampleCount <= 0) {
      return 0;
    }

    assert(s1 >= s0);
    TINT32 ss0, ss1;

    ss0 = tcrop<TINT32>(s0, (TINT32)0, (TINT32)(getSampleCount() - (TINT32)1));
    ss1 = tcrop<TINT32>(s1, (TINT32)0, (TINT32)(getSampleCount() - (TINT32)1));

    assert(ss1 >= ss0);

    if (s0 == s1) return (getPressure(s0, chan));

    const T *sample = samples() + ss0;
    assert(sample);
    double minPressure = sample->getPressure(chan);
    const T *endSample = sample + (ss1 - ss0 + 1);
    ++sample;

    while (sample < endSample) {
      if (minPressure > sample->getPressure(chan))
        minPressure = sample->getPressure(chan);
      ++sample;
    }

    return ((double)minPressure);
  }

  //----------------------------------------------------------------------------

  /*!
Copies the samples in the given sample range and channel
inside the dstChan channel from sample dst_s0
*/
  void copyChannel(const TSoundTrackT<T> &src, TINT32 src_s0, TINT32 src_s1,
                   TSound::Channel srcChan, TINT32 dst_s0,
                   TSound::Channel dstChan) {
    TINT32 ss0, ss1;
    // se i valori sono nel range ed uguali => voglio copiare il
    // canale di un solo campione
    if (src_s1 == src_s0 && src_s1 >= 0 && src_s1 < src.getSampleCount())
      ss0 = ss1 = src_s1;
    else {
      assert(src_s1 >= src_s0);
      ss0 = tcrop(src_s0, (TINT32)0, (TINT32)(src.getSampleCount() - 1));
      ss1 = tcrop(src_s1, (TINT32)0, (TINT32)(src.getSampleCount() - 1));
      assert(ss1 >= ss0);
      // esco perche' non ha senso copiare indiscriminatamente il primo
      // o l'ultimo campione della sorgente
      if (ss1 == ss0) return;
    }

    assert(dst_s0 >= 0L && dst_s0 < getSampleCount());

    const T *srcSample = src.samples() + ss0;

    const T *srcEndSample =
        srcSample +
        std::min((TINT32)(ss1 - ss0 + 1), (TINT32)(getSampleCount() - dst_s0));

    T *dstSample = samples() + dst_s0;

    for (; srcSample < srcEndSample; srcSample++, dstSample++)
      dstSample->setValue(srcChan, srcSample->getValue(dstChan));
  }

  //----------------------------------------------------------------------------

  //! Makes blank the samples in the given sample range
  void blank(TINT32 s0, TINT32 s1) override {
    TINT32 ss0, ss1;
    // se i valori sono nel range ed uguali => voglio pulire
    // un solo campione
    if (s1 == s0 && s1 >= 0 && s1 < getSampleCount())
      ss0 = ss1 = s1;
    else {
      assert(s1 >= s0);

      ss0 = tcrop<TINT32>(s0, (TINT32)0, (TINT32)(getSampleCount() - 1));
      ss1 = tcrop<TINT32>(s1, (TINT32)0, (TINT32)(getSampleCount() - 1));

      assert(ss1 >= ss0);
      // esco perche' non ha senso pulire indiscriminatamente
      // il primo o l'ultimo campione
      if (ss1 == ss0) return;
    }

    T *sample = samples() + ss0;
    assert(sample);

    T blankSample;
    const T *endSample                   = sample + (ss1 - ss0 + 1);
    while (sample < endSample) *sample++ = blankSample;
  }

  //----------------------------------------------------------------------------

  //! Makes blank the samples in the given sample range and channel
  void blankChannel(TINT32 s0, TINT32 s1, TSound::Channel chan) {
    if (s0 > s1) return;
    // ....
    assert(false);
  }
};

//==============================================================================

#ifdef _WIN32
template class DVAPI TSoundTrackT<TMono8SignedSample>;
template class DVAPI TSoundTrackT<TMono8UnsignedSample>;
template class DVAPI TSoundTrackT<TStereo8SignedSample>;
template class DVAPI TSoundTrackT<TStereo8UnsignedSample>;
template class DVAPI TSoundTrackT<TMono16Sample>;
template class DVAPI TSoundTrackT<TStereo16Sample>;
template class DVAPI TSoundTrackT<TMono24Sample>;
template class DVAPI TSoundTrackT<TStereo24Sample>;
#endif

typedef TSoundTrackT<TMono8SignedSample> TSoundTrackMono8Signed;
typedef TSoundTrackT<TMono8UnsignedSample> TSoundTrackMono8Unsigned;
typedef TSoundTrackT<TStereo8SignedSample> TSoundTrackStereo8Signed;
typedef TSoundTrackT<TStereo8UnsignedSample> TSoundTrackStereo8Unsigned;
typedef TSoundTrackT<TMono16Sample> TSoundTrackMono16;
typedef TSoundTrackT<TStereo16Sample> TSoundTrackStereo16;
typedef TSoundTrackT<TMono24Sample> TSoundTrackMono24;
typedef TSoundTrackT<TStereo24Sample> TSoundTrackStereo24;

//==============================================================================

class TSoundTransform {
public:
  TSoundTransform() {}
  virtual ~TSoundTransform() {}

  virtual TSoundTrackP compute(const TSoundTrackMono8Signed &) { return 0; };
  virtual TSoundTrackP compute(const TSoundTrackMono8Unsigned &) { return 0; };
  virtual TSoundTrackP compute(const TSoundTrackStereo8Signed &) { return 0; };
  virtual TSoundTrackP compute(const TSoundTrackStereo8Unsigned &) {
    return 0;
  };
  virtual TSoundTrackP compute(const TSoundTrackMono16 &) { return 0; };
  virtual TSoundTrackP compute(const TSoundTrackStereo16 &) { return 0; };
  virtual TSoundTrackP compute(const TSoundTrackMono24 &) { return 0; };
  virtual TSoundTrackP compute(const TSoundTrackStereo24 &) { return 0; };
};

//==============================================================================

#if !defined(_MSC_VER) || defined(TSOUND_EXPORTS)
template <class T>
TSoundTrackP TSoundTrackT<T>::apply(TSoundTransform *transform) {
  assert(transform);
  return transform->compute(*this);
}
#endif

#endif
