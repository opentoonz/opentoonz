#pragma once

#ifndef TSOUNDSAMPLE_INCLUDED
#define TSOUNDSAMPLE_INCLUDED

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

// forward declarations
class TMono8SignedSample;
class TMono8UnsignedSample;
class TStereo8SignedSample;
class TStereo8UnsignedSample;
class TMono16Sample;
class TStereo16Sample;
class TMono24Sample;
class TStereo24Sample;
class TMono32FloatSample;
class TStereo32FloatSample;

//==============================================================================

class DVAPI TMono8SignedSample {
  SCHAR value;

public:
  typedef SCHAR ChannelValueType;
  typedef TMono8SignedSample ChannelSampleType;

  TMono8SignedSample(SCHAR v = 0) : value(v) {}
  ~TMono8SignedSample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 8; }
  static int getSampleType() { return TSound::INT; }

  inline SCHAR getValue(TSound::Channel /*chan*/) const { return value; }

  inline void setValue(TSound::Channel /*chan*/, SCHAR v) { value = v; }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TMono8SignedSample &operator+=(const TMono8SignedSample &s) {
    int ival = value + s.value;
    value    = tcrop(ival, (int)-128, (int)127);
    return *this;
  }

  inline TMono8SignedSample &operator*=(double a) {
    int ival = (int)(value * a);
    value    = tcrop(ival, (int)-128, (int)127);
    return *this;
  }

  inline TMono8SignedSample operator*(double a) { return (*this) *= a; }

  static TMono8SignedSample mix(const TMono8SignedSample &s1, double a1,
                                const TMono8SignedSample &s2, double a2) {
    return TMono8SignedSample(
        tcrop((int)(s1.value * a1 + s2.value * a2), -128, 127));
  }

  static inline TMono8SignedSample from(const TMono8SignedSample &sample);
  static inline TMono8SignedSample from(const TMono8UnsignedSample &sample);
  static inline TMono8SignedSample from(const TStereo8SignedSample &sample);
  static inline TMono8SignedSample from(const TStereo8UnsignedSample &sample);
  static inline TMono8SignedSample from(const TMono16Sample &sample);
  static inline TMono8SignedSample from(const TStereo16Sample &sample);
  static inline TMono8SignedSample from(const TMono24Sample &sample);
  static inline TMono8SignedSample from(const TStereo24Sample &sample);
  static inline TMono8SignedSample from(const TMono32FloatSample &sample);
  static inline TMono8SignedSample from(const TStereo32FloatSample &sample);
};

inline TMono8SignedSample operator+(const TMono8SignedSample &s1,
                                    const TMono8SignedSample &s2) {
  TMono8SignedSample s = s1;
  return s += s2;
}

//==============================================================================

class DVAPI TMono8UnsignedSample {
  UCHAR value;

public:
  typedef UCHAR ChannelValueType;
  typedef TMono8UnsignedSample ChannelSampleType;

  TMono8UnsignedSample(UCHAR v = 127) : value(v) {}
  ~TMono8UnsignedSample(){};

  static bool isSampleSigned() { return false; }
  static int getBitPerSample() { return 8; }
  static int getSampleType() { return TSound::UINT; }

  inline UCHAR getValue(TSound::Channel /*chan*/) const { return value; }

  inline void setValue(TSound::Channel /*chan*/, UCHAR v) { value = v; }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan) - 128);
  }

  inline TMono8UnsignedSample &operator+=(const TMono8UnsignedSample &s) {
    value = tcrop((int)(value - 128 + s.value), 0, 255);
    return *this;
  }

  inline TMono8UnsignedSample &operator*=(double a) {
    value = tcrop((int)((value - 128) * a + 128), 0, 255);
    return *this;
  }

  inline TMono8UnsignedSample operator*(double a) {
    int ival = tcrop((int)((value - 128) * a + 128), 0, 255);
    return TMono8UnsignedSample(ival);
  }

  static TMono8UnsignedSample mix(const TMono8UnsignedSample &s1, double a1,
                                  const TMono8UnsignedSample &s2, double a2) {
    return TMono8UnsignedSample(tcrop(
        (int)((s1.value - 128) * a1 + (s2.value - 128) * a2 + 128), 0, 255));
  }

  static inline TMono8UnsignedSample from(const TMono8SignedSample &sample);
  static inline TMono8UnsignedSample from(const TMono8UnsignedSample &sample);
  static inline TMono8UnsignedSample from(const TStereo8SignedSample &sample);
  static inline TMono8UnsignedSample from(const TStereo8UnsignedSample &sample);
  static inline TMono8UnsignedSample from(const TMono16Sample &sample);
  static inline TMono8UnsignedSample from(const TStereo16Sample &sample);
  static inline TMono8UnsignedSample from(const TMono24Sample &sample);
  static inline TMono8UnsignedSample from(const TStereo24Sample &sample);
  static inline TMono8UnsignedSample from(const TMono32FloatSample &sample);
  static inline TMono8UnsignedSample from(const TStereo32FloatSample &sample);
};

inline TMono8UnsignedSample operator+(const TMono8UnsignedSample &s1,
                                      const TMono8UnsignedSample &s2) {
  TMono8UnsignedSample s = s1;
  return s += s2;
}

//==============================================================================

class DVAPI TStereo8SignedSample {
  SCHAR channel[2];  // l'ordine dei canali e' left,right

public:
  typedef SCHAR ChannelValueType;
  typedef TMono8SignedSample ChannelSampleType;

  TStereo8SignedSample(short v) {
    channel[0] = v >> 8;
    channel[1] = v & 0xFF;
  }

  TStereo8SignedSample(SCHAR lchan = 0, SCHAR rchan = 0) {
    channel[0] = lchan;
    channel[1] = rchan;
  }

  ~TStereo8SignedSample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 8; }
  static int getSampleType() { return TSound::INT; }

  inline SCHAR getValue(TSound::Channel chan) const {
    assert(chan <= 1);
    return channel[chan];
  }

  inline void setValue(TSound::Channel chan, SCHAR v) {
    assert(chan <= 1);
    channel[chan] = v;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TStereo8SignedSample &operator+=(const TStereo8SignedSample &s) {
    int iLeftVal  = channel[0] + s.channel[0];
    int iRightVal = channel[1] + s.channel[1];
    channel[0]    = tcrop(iLeftVal, (int)-128, (int)127);
    channel[1]    = tcrop(iRightVal, (int)-128, (int)127);
    return *this;
  }

  inline TStereo8SignedSample &operator*=(double a) {
    int iLeftVal  = (int)(a * channel[0]);
    int iRightVal = (int)(a * channel[1]);
    channel[0]    = tcrop(iLeftVal, (int)-128, (int)127);
    channel[1]    = tcrop(iRightVal, (int)-128, (int)127);
    return *this;
  }

  inline TStereo8SignedSample operator*(double a) {
    return TStereo8SignedSample(*this) *= a;
  }

  static TStereo8SignedSample mix(const TStereo8SignedSample &s1, double a1,
                                  const TStereo8SignedSample &s2, double a2) {
    return TStereo8SignedSample(
        tcrop((int)(s1.channel[0] * a1 + s2.channel[0] * a2), -128, 127),
        tcrop((int)(s1.channel[1] * a1 + s2.channel[1] * a2), -128, 127));
  }

  static inline TStereo8SignedSample from(const TMono8UnsignedSample &sample);
  static inline TStereo8SignedSample from(const TMono8SignedSample &sample);
  static inline TStereo8SignedSample from(const TStereo8SignedSample &sample);
  static inline TStereo8SignedSample from(const TStereo8UnsignedSample &sample);
  static inline TStereo8SignedSample from(const TMono16Sample &sample);
  static inline TStereo8SignedSample from(const TStereo16Sample &sample);
  static inline TStereo8SignedSample from(const TMono24Sample &sample);
  static inline TStereo8SignedSample from(const TStereo24Sample &sample);
  static inline TStereo8SignedSample from(const TMono32FloatSample &sample);
  static inline TStereo8SignedSample from(const TStereo32FloatSample &sample);
};

inline TStereo8SignedSample operator+(const TStereo8SignedSample &s1,
                                      const TStereo8SignedSample &s2) {
  TStereo8SignedSample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TStereo8UnsignedSample {
  UCHAR channel[2];  // l'ordine dei canali e' left,right

public:
  typedef UCHAR ChannelValueType;
  typedef TMono8UnsignedSample ChannelSampleType;

  TStereo8UnsignedSample(short v) {
    channel[0] = v >> 8;
    channel[1] = v & 0xFF;
  }

  TStereo8UnsignedSample(UCHAR lchan = 127, UCHAR rchan = 127) {
    channel[0] = lchan;
    channel[1] = rchan;
  }

  ~TStereo8UnsignedSample(){};

  static bool isSampleSigned() { return false; }
  static int getBitPerSample() { return 8; }
  static int getSampleType() { return TSound::UINT; }

  inline UCHAR getValue(TSound::Channel chan) const {
    assert(chan <= 1);
    return channel[chan];
  }

  inline void setValue(TSound::Channel chan, UCHAR v) {
    assert(chan <= 1);
    channel[chan] = v;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (((int)getValue(chan)) - 128);
  }

  inline TStereo8UnsignedSample &operator+=(const TStereo8UnsignedSample &s) {
    int iLeftVal  = channel[0] + s.channel[0] - 128;
    int iRightVal = channel[1] + s.channel[1] - 128;
    channel[0]    = tcrop(iLeftVal, 0, 255);
    channel[1]    = tcrop(iRightVal, 0, 255);
    return *this;
  }

  inline TStereo8UnsignedSample &operator*=(double a) {
    int iLeftVal  = (int)(a * (channel[0] - 128));
    int iRightVal = (int)(a * (channel[1] - 128));
    channel[0]    = tcrop(iLeftVal + 128, 0, 255);
    channel[1]    = tcrop(iRightVal + 128, 0, 255);
    return *this;
  }

  inline TStereo8UnsignedSample operator*(double a) {
    return TStereo8UnsignedSample(*this) *= a;
  }

  static TStereo8UnsignedSample mix(const TStereo8UnsignedSample &s1, double a1,
                                    const TStereo8UnsignedSample &s2,
                                    double a2) {
    return TStereo8UnsignedSample(tcrop((int)((s1.channel[0] - 128) * a1 +
                                              (s2.channel[0] - 128) * a2 + 128),
                                        0, 255),
                                  tcrop((int)((s1.channel[1] - 128) * a1 +
                                              (s2.channel[1] - 128) * a2 + 128),
                                        0, 255));
  }

  static inline TStereo8UnsignedSample from(const TMono8UnsignedSample &sample);
  static inline TStereo8UnsignedSample from(const TMono8SignedSample &sample);
  static inline TStereo8UnsignedSample from(
      const TStereo8UnsignedSample &sample);
  static inline TStereo8UnsignedSample from(const TStereo8SignedSample &sample);
  static inline TStereo8UnsignedSample from(const TMono16Sample &sample);
  static inline TStereo8UnsignedSample from(const TStereo16Sample &sample);
  static inline TStereo8UnsignedSample from(const TMono24Sample &sample);
  static inline TStereo8UnsignedSample from(const TStereo24Sample &sample);
  static inline TStereo8UnsignedSample from(const TMono32FloatSample &sample);
  static inline TStereo8UnsignedSample from(const TStereo32FloatSample &sample);
};

inline TStereo8UnsignedSample operator+(const TStereo8UnsignedSample &s1,
                                        const TStereo8UnsignedSample &s2) {
  TStereo8UnsignedSample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TMono16Sample {
  short value;

public:
  typedef short ChannelValueType;
  typedef TMono16Sample ChannelSampleType;

  TMono16Sample(short v = 0) : value(v) {}
  ~TMono16Sample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 16; }
  static int getSampleType() { return TSound::INT; }

  inline short getValue(TSound::Channel) const { return value; }

  inline void setValue(TSound::Channel /*chan*/, short v) { value = v; }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TMono16Sample &operator+=(const TMono16Sample &s) {
    int iVal = value + s.value;
    value    = tcrop(iVal, -32768, 32767);
    return *this;
  }

  inline TMono16Sample &operator*=(double a) {
    int iVal = (int)(value * a);
    value    = tcrop(iVal, -32768, 32767);
    return *this;
  }

  inline TMono16Sample operator*(double a) { return TMono16Sample(*this) *= a; }

  static TMono16Sample mix(const TMono16Sample &s1, double a1,
                           const TMono16Sample &s2, double a2) {
    return TMono16Sample(
        tcrop((int)(s1.value * a1 + s2.value * a2), -32768, 32767));
  }

  static inline TMono16Sample from(const TMono8UnsignedSample &sample);
  static inline TMono16Sample from(const TMono8SignedSample &sample);
  static inline TMono16Sample from(const TStereo8SignedSample &sample);
  static inline TMono16Sample from(const TStereo8UnsignedSample &sample);
  static inline TMono16Sample from(const TMono16Sample &sample);
  static inline TMono16Sample from(const TStereo16Sample &sample);
  static inline TMono16Sample from(const TMono24Sample &sample);
  static inline TMono16Sample from(const TStereo24Sample &sample);
  static inline TMono16Sample from(const TMono32FloatSample &sample);
  static inline TMono16Sample from(const TStereo32FloatSample &sample);
};

inline TMono16Sample operator+(const TMono16Sample &s1,
                               const TMono16Sample &s2) {
  TMono16Sample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TStereo16Sample {
  short channel[2];  // l'ordine dei canali e' left,right

public:
  typedef short ChannelValueType;
  typedef TMono16Sample ChannelSampleType;

  TStereo16Sample(TINT32 v) {
    channel[0] = (short)(v << 16);
    channel[1] = (short)(v & 0xFFFF);
  }

  TStereo16Sample(short lchan = 0, short rchan = 0) {
    channel[0] = lchan;
    channel[1] = rchan;
  }

  ~TStereo16Sample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 16; }
  static int getSampleType() { return TSound::INT; }

  inline short getValue(TSound::Channel chan) const {
    assert(chan <= 1);
    return channel[chan];
  }

  inline void setValue(TSound::Channel chan, short v) {
    assert(chan <= 1);
    channel[chan] = v;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TStereo16Sample &operator+=(const TStereo16Sample &s) {
    int iLeftVal  = channel[0] + s.channel[0];
    int iRightVal = channel[1] + s.channel[1];
    channel[0]    = tcrop(iLeftVal, -32768, 32767);
    channel[1]    = tcrop(iRightVal, -32768, 32767);
    return *this;
  }

  inline TStereo16Sample &operator*=(double a) {
    int iLeftVal  = (int)(a * channel[0]);
    int iRightVal = (int)(a * channel[1]);
    channel[0]    = tcrop(iLeftVal, -32768, 32767);
    channel[1]    = tcrop(iRightVal, -32768, 32767);
    return *this;
  }

  inline TStereo16Sample operator*(double a) {
    return TStereo16Sample(*this) *= a;
  }

  static TStereo16Sample mix(const TStereo16Sample &s1, double a1,
                             const TStereo16Sample &s2, double a2) {
    return TStereo16Sample(
        tcrop((int)(s1.channel[0] * a1 + s2.channel[0] * a2), -32768, 32767),
        tcrop((int)(s1.channel[1] * a1 + s2.channel[1] * a2), -32768, 32767));
  }

  static inline TStereo16Sample from(const TMono8UnsignedSample &sample);
  static inline TStereo16Sample from(const TMono8SignedSample &sample);
  static inline TStereo16Sample from(const TStereo8SignedSample &sample);
  static inline TStereo16Sample from(const TStereo8UnsignedSample &sample);
  static inline TStereo16Sample from(const TMono16Sample &sample);
  static inline TStereo16Sample from(const TStereo16Sample &sample);
  static inline TStereo16Sample from(const TMono24Sample &sample);
  static inline TStereo16Sample from(const TStereo24Sample &sample);
  static inline TStereo16Sample from(const TMono32FloatSample &sample);
  static inline TStereo16Sample from(const TStereo32FloatSample &sample);
};

inline TStereo16Sample operator+(const TStereo16Sample &s1,
                                 const TStereo16Sample &s2) {
  TStereo16Sample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TMono24Sample {
  UCHAR byte[3];  // LSB first

public:
  typedef TINT32 ChannelValueType;
  typedef TMono24Sample ChannelSampleType;

  TMono24Sample(TINT32 v = 0) { setValue(0, v); }
  ~TMono24Sample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 24; }
  static int getSampleType() { return TSound::INT; }

  inline TINT32 getValue(TSound::Channel) const {
    return byte[0] | (byte[1] << 8) | (byte[2] << 16) |
           (byte[2] & 0x80 ? ~0xFFFFFF : 0);
  }

  inline void setValue(TSound::Channel /*chan*/, TINT32 v) {
    int iVal = tcrop<TINT32>(v, -8388608, 8388607);
    byte[0]  = iVal;
    byte[1]  = iVal >> 8;
    byte[2]  = iVal >> 16;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TMono24Sample &operator+=(const TMono24Sample &s) {
    setValue(0, getValue(0) + s.getValue(0));
    return *this;
  }

  inline TMono24Sample &operator*=(double a) {
    setValue(0, getValue(0) * a);
    return *this;
  }

  inline TMono24Sample operator*(double a) { return TMono24Sample(*this) *= a; }

  static TMono24Sample mix(const TMono24Sample &s1, double a1,
                           const TMono24Sample &s2, double a2) {
    int iVal = s1.getValue(0) * a1 + s2.getValue(0) * a2;
    return TMono24Sample(iVal);
  }

  static inline TMono24Sample from(const TMono8UnsignedSample &sample);
  static inline TMono24Sample from(const TMono8SignedSample &sample);
  static inline TMono24Sample from(const TStereo8SignedSample &sample);
  static inline TMono24Sample from(const TStereo8UnsignedSample &sample);
  static inline TMono24Sample from(const TMono16Sample &sample);
  static inline TMono24Sample from(const TStereo16Sample &sample);
  static inline TMono24Sample from(const TMono24Sample &sample);
  static inline TMono24Sample from(const TStereo24Sample &sample);
  static inline TMono24Sample from(const TMono32FloatSample &sample);
  static inline TMono24Sample from(const TStereo32FloatSample &sample);
};

inline TMono24Sample operator+(const TMono24Sample &s1,
                               const TMono24Sample &s2) {
  TMono24Sample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TStereo24Sample {
  typedef struct {
    UCHAR byte[3];  // LSB first
  } TSample;
  TSample channel[2];  // l'ordine dei canali e' left,right

public:
  typedef TINT32 ChannelValueType;
  typedef TMono24Sample ChannelSampleType;

  TStereo24Sample(TINT32 lchan = 0, TINT32 rchan = 0) {
    setValue(0, lchan);
    setValue(1, rchan);
  }

  ~TStereo24Sample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 24; }
  static int getSampleType() { return TSound::INT; }

  inline TINT32 getValue(TSound::Channel chan) const {
    assert(chan <= 1);
    const TSample &s = channel[chan];
    return s.byte[0] | (s.byte[1] << 8) | (s.byte[2] << 16) |
           (s.byte[2] & 0x80 ? ~0xFFFFFF : 0);
  }

  inline void setValue(TSound::Channel chan, TINT32 v) {
    assert(chan <= 1);
    int iVal   = tcrop<TINT32>(v, -8388608, 8388607);
    TSample &s = channel[chan];
    s.byte[0]  = iVal;
    s.byte[1]  = iVal >> 8;
    s.byte[2]  = iVal >> 16;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TStereo24Sample &operator+=(const TStereo24Sample &s) {
    setValue(0, getValue(0) + s.getValue(0));
    setValue(1, getValue(1) + s.getValue(1));
    return *this;
  }

  inline TStereo24Sample &operator*=(double a) {
    setValue(0, getValue(0) * a);
    setValue(1, getValue(1) * a);
    return *this;
  }

  inline TStereo24Sample operator*(double a) {
    return TStereo24Sample(*this) *= a;
  }

  static TStereo24Sample mix(const TStereo24Sample &s1, double a1,
                             const TStereo24Sample &s2, double a2) {
    int iValL = s1.getValue(0) * a1 + s2.getValue(0) * a2;
    int iValR = s1.getValue(1) * a1 + s2.getValue(1) * a2;
    return TStereo24Sample(iValL, iValR);
  }

  static inline TStereo24Sample from(const TMono8UnsignedSample &sample);
  static inline TStereo24Sample from(const TMono8SignedSample &sample);
  static inline TStereo24Sample from(const TStereo8SignedSample &sample);
  static inline TStereo24Sample from(const TStereo8UnsignedSample &sample);
  static inline TStereo24Sample from(const TMono16Sample &sample);
  static inline TStereo24Sample from(const TStereo16Sample &sample);
  static inline TStereo24Sample from(const TMono24Sample &sample);
  static inline TStereo24Sample from(const TStereo24Sample &sample);
  static inline TStereo24Sample from(const TMono32FloatSample &sample);
  static inline TStereo24Sample from(const TStereo32FloatSample &sample);
};

inline TStereo24Sample operator+(const TStereo24Sample &s1,
                                 const TStereo24Sample &s2) {
  TStereo24Sample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TMono32FloatSample {
  float value;

public:
  typedef float ChannelValueType;
  typedef TMono32FloatSample ChannelSampleType;

  TMono32FloatSample(float v = 0.0f) : value(v) {}
  ~TMono32FloatSample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 32; }
  static int getSampleType() { return TSound::FLOAT; }

  inline float getValue(TSound::Channel) const { return value; }

  inline void setValue(TSound::Channel /*chan*/, float v) {
    value = v;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TMono32FloatSample &operator+=(const TMono32FloatSample &s) {
    value += s.value;
    return *this;
  }

  inline TMono32FloatSample &operator*=(double a) {
    value *= a;
    return *this;
  }

  inline TMono32FloatSample operator*(double a) { return TMono32FloatSample(*this) *= a; }

  static TMono32FloatSample mix(const TMono32FloatSample &s1, double a1,
                           const TMono32FloatSample &s2, double a2) {
    return TMono32FloatSample(s1.value * a1 + s2.value * a2);
  }

  static inline TMono32FloatSample from(const TMono8UnsignedSample &sample);
  static inline TMono32FloatSample from(const TMono8SignedSample &sample);
  static inline TMono32FloatSample from(const TStereo8SignedSample &sample);
  static inline TMono32FloatSample from(const TStereo8UnsignedSample &sample);
  static inline TMono32FloatSample from(const TMono16Sample &sample);
  static inline TMono32FloatSample from(const TStereo16Sample &sample);
  static inline TMono32FloatSample from(const TMono24Sample &sample);
  static inline TMono32FloatSample from(const TStereo24Sample &sample);
  static inline TMono32FloatSample from(const TMono32FloatSample &sample);
  static inline TMono32FloatSample from(const TStereo32FloatSample &sample);
};

inline TMono32FloatSample operator+(const TMono32FloatSample &s1,
                               const TMono32FloatSample &s2) {
  TMono32FloatSample s = s1;
  return s += s2;
}

//=========================================================

class DVAPI TStereo32FloatSample {
  float channel[2];  // l'ordine dei canali e' left,right

public:
  typedef float ChannelValueType;
  typedef TMono32FloatSample ChannelSampleType;

  TStereo32FloatSample(float lchan = 0.0f, float rchan = 0.0f) {
    channel[0] = lchan;
    channel[1] = rchan;
  }

  ~TStereo32FloatSample(){};

  static bool isSampleSigned() { return true; }
  static int getBitPerSample() { return 32; }
  static int getSampleType() { return TSound::FLOAT; }

  inline float getValue(TSound::Channel chan) const {
    assert(chan <= 1);
    return channel[chan];
  }

  inline void setValue(TSound::Channel chan, float v) {
    assert(chan <= 1);
    channel[chan] = v;
  }

  inline double getPressure(TSound::Channel chan) const {
    return (getValue(chan));
  }

  inline TStereo32FloatSample &operator+=(const TStereo32FloatSample &s) {
    channel[0] += s.channel[0];
    channel[1] += s.channel[1];
    return *this;
  }

  inline TStereo32FloatSample &operator*=(double a) {
    channel[0] *= a;
    channel[1] *= a;
    return *this;
  }

  inline TStereo32FloatSample operator*(double a) {
    return TStereo32FloatSample(*this) *= a;
  }

  static TStereo32FloatSample mix(const TStereo32FloatSample &s1, double a1,
                             const TStereo32FloatSample &s2, double a2) {
    return TStereo32FloatSample(s1.channel[0] * a1 + s2.channel[0] * a2,
                           s1.channel[1] * a1 + s2.channel[1] * a2);
  }

  static inline TStereo32FloatSample from(const TMono8UnsignedSample &sample);
  static inline TStereo32FloatSample from(const TMono8SignedSample &sample);
  static inline TStereo32FloatSample from(const TStereo8SignedSample &sample);
  static inline TStereo32FloatSample from(const TStereo8UnsignedSample &sample);
  static inline TStereo32FloatSample from(const TMono16Sample &sample);
  static inline TStereo32FloatSample from(const TStereo16Sample &sample);
  static inline TStereo32FloatSample from(const TMono24Sample &sample);
  static inline TStereo32FloatSample from(const TStereo24Sample &sample);
  static inline TStereo32FloatSample from(const TMono32FloatSample &sample);
  static inline TStereo32FloatSample from(const TStereo32FloatSample &sample);
};

inline TStereo32FloatSample operator+(const TStereo32FloatSample &s1,
                                 const TStereo32FloatSample &s2) {
  TStereo32FloatSample s = s1;
  return s += s2;
}

//==============================================================================

inline TMono8SignedSample TMono8SignedSample::from(
    const TMono8SignedSample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TMono8UnsignedSample &sample) {
  return TMono8SignedSample(sample.getValue(TSound::MONO) - 128);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TStereo8SignedSample &sample) {
  return TMono8SignedSample(
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 1);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TStereo8UnsignedSample &sample) {
  return TMono8SignedSample(
      ((sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 1) -
      128);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TMono16Sample &sample) {
  int val = (sample.getValue(TSound::MONO) >> 8);
  return TMono8SignedSample(val);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TStereo16Sample &sample) {
  return TMono8SignedSample(
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 9);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TMono24Sample &sample) {
  int val = (sample.getValue(TSound::MONO) >> 16);
  return TMono8SignedSample(val);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TStereo24Sample &sample) {
  int val =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 17;
  return TMono8SignedSample(val);
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 128.0;
  return TMono8SignedSample(tcrop((int)fval, -128, 127));
}

//------------------------------------------------------------------------------

inline TMono8SignedSample TMono8SignedSample::from(
    const TStereo32FloatSample &sample) {
  float fval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) * 64.0;
  return TMono8SignedSample(tcrop((int)fval, -128, 127));
}

//==============================================================================

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TMono8SignedSample &sample) {
  return TMono8UnsignedSample(sample.getValue(TSound::MONO) + 128);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TMono8UnsignedSample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TStereo8SignedSample &sample) {
  int iVal = sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT);
  return TMono8UnsignedSample((iVal >> 1) + 128);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TStereo8UnsignedSample &sample) {
  return TMono8UnsignedSample(
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 1);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TMono16Sample &sample) {
  return TMono8UnsignedSample((sample.getValue(TSound::MONO) >> 8) + 128);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TStereo16Sample &sample) {
  return TMono8UnsignedSample(
      ((sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 9) +
      128);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TMono24Sample &sample) {
  return TMono8UnsignedSample(
      (unsigned char)(sample.getValue(TSound::MONO) >> 16) + 128);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TStereo24Sample &sample) {
  return TMono8UnsignedSample(
      (unsigned char)((sample.getValue(TSound::LEFT) +
                       sample.getValue(TSound::RIGHT)) >>
                      17) +
      128);
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 128.0;
  return TMono8UnsignedSample(tcrop((int)fval + 128, 0, 255));
}

//------------------------------------------------------------------------------

inline TMono8UnsignedSample TMono8UnsignedSample::from(
    const TStereo32FloatSample &sample) {
  float fval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) * 64.0;
  return TMono8UnsignedSample(tcrop((int)fval + 128, 0, 255));
}

//==============================================================================

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TMono8UnsignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO) - 128;
  return TStereo8SignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TMono8SignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO);
  return TStereo8SignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TStereo8SignedSample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TStereo8UnsignedSample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) - 128;
  int srcvalR = sample.getValue(TSound::RIGHT) - 128;
  return TStereo8SignedSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TMono16Sample &sample) {
  int srcval = sample.getValue(TSound::MONO) >> 8;
  return TStereo8SignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TStereo16Sample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) >> 8;
  int srcvalR = sample.getValue(TSound::RIGHT) >> 8;
  return TStereo8SignedSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TMono24Sample &sample) {
  int srcval = sample.getValue(TSound::MONO) >> 16;
  return TStereo8SignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TStereo24Sample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) >> 16;
  int srcvalR = sample.getValue(TSound::RIGHT) >> 16;
  return TStereo8SignedSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 128.0;
  int srcval = tcrop((int)fval, -128, 127);
  return TStereo8SignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8SignedSample TStereo8SignedSample::from(
    const TStereo32FloatSample &sample) {
  float fvalL = sample.getValue(TSound::LEFT) * 128.0;
  float fvalR = sample.getValue(TSound::RIGHT) * 128.0;
  return TStereo8SignedSample(tcrop((int)fvalL, -128, 127),
                              tcrop((int)fvalR, -128, 127));
}

//==============================================================================

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TMono8UnsignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO);

  return TStereo8UnsignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TMono8SignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO) + 128;

  return TStereo8UnsignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TStereo8UnsignedSample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TStereo8SignedSample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) + 128;
  int srcvalR = sample.getValue(TSound::RIGHT) + 128;
  return TStereo8UnsignedSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TMono16Sample &sample) {
  int srcval = (sample.getValue(TSound::MONO) >> 8) + 128;
  return TStereo8UnsignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TStereo16Sample &sample) {
  int srcvalL = (sample.getValue(TSound::LEFT) >> 8) + 128;
  int srcvalR = (sample.getValue(TSound::RIGHT) >> 8) + 128;
  return TStereo8UnsignedSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TMono24Sample &sample) {
  int srcval = (sample.getValue(TSound::MONO) >> 16) + 128;
  return TStereo8UnsignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TStereo24Sample &sample) {
  int srcvalL = (sample.getValue(TSound::LEFT) >> 16) + 128;
  int srcvalR = (sample.getValue(TSound::RIGHT) >> 16) + 128;
  return TStereo8UnsignedSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 128.0;
  int srcval = tcrop((int)fval + 128, 0, 255);
  return TStereo8UnsignedSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo8UnsignedSample TStereo8UnsignedSample::from(
    const TStereo32FloatSample &sample) {
  float fvalL = sample.getValue(TSound::LEFT) * 128.0;
  float fvalR = sample.getValue(TSound::RIGHT) * 128.0;
  return TStereo8UnsignedSample(tcrop((int)fvalL + 128, 0, 255),
                                tcrop((int)fvalR + 128, 0, 255));
}

//==============================================================================

inline TMono16Sample TMono16Sample::from(const TMono8UnsignedSample &sample) {
  return TMono16Sample((sample.getValue(TSound::MONO) - 128) << 8);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TMono8SignedSample &sample) {
  return TMono16Sample(sample.getValue(TSound::MONO) << 8);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TStereo8SignedSample &sample) {
  return TMono16Sample(
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) << 7);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TStereo8UnsignedSample &sample) {
  return TMono16Sample(
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT) - 256)
      << 7);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TMono16Sample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TStereo16Sample &sample) {
  return TMono16Sample(
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 1);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TMono24Sample &sample) {
  int srcval = (sample.getValue(TSound::MONO) >> 8);
  return TMono16Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TStereo24Sample &sample) {
  int srcval =
      ((sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 9);
  return TMono16Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 32768.0;
  return TMono16Sample(tcrop((int)fval, -32768, 32767));
}

//------------------------------------------------------------------------------

inline TMono16Sample TMono16Sample::from(const TStereo32FloatSample &sample) {
  float fval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) *
      16384.0;
  return TMono16Sample(tcrop((int)fval, -32768, 32767));
}

//==============================================================================

inline TStereo16Sample TStereo16Sample::from(
    const TMono8UnsignedSample &sample) {
  int srcval = (sample.getValue(TSound::MONO) - 128) << 8;
  return TStereo16Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TMono8SignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO) << 8;
  return TStereo16Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(
    const TStereo8SignedSample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) << 8;
  int srcvalR = sample.getValue(TSound::RIGHT) << 8;
  return TStereo16Sample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(
    const TStereo8UnsignedSample &sample) {
  int srcvalL = (sample.getValue(TSound::LEFT) - 128) << 8;
  int srcvalR = (sample.getValue(TSound::RIGHT) - 128) << 8;
  return TStereo16Sample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TMono16Sample &sample) {
  int srcval = sample.getValue(TSound::MONO);
  return TStereo16Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TStereo16Sample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TMono24Sample &sample) {
  int srcval = (sample.getValue(TSound::MONO) >> 8);
  return TStereo16Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TStereo24Sample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) >> 8;
  int srcvalR = sample.getValue(TSound::RIGHT) >> 8;
  return TStereo16Sample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 32768.0;
  int srcval = tcrop((int)fval, -32768, 32767);
  return TStereo16Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo16Sample TStereo16Sample::from(const TStereo32FloatSample &sample) {
  float fvalL = sample.getValue(TSound::LEFT) * 32768.0;
  float fvalR = sample.getValue(TSound::RIGHT) * 32768.0;
  return TStereo16Sample(tcrop((int)fvalL, -32768, 32767),
                         tcrop((int)fvalR, -32768, 32767));
}

//==============================================================================

inline TMono24Sample TMono24Sample::from(const TMono8UnsignedSample &sample) {
  int srcval = (sample.getValue(TSound::MONO) - 128) << 16;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TMono8SignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO) << 16;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TStereo8SignedSample &sample) {
  int srcval = (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT))
               << 15;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TStereo8UnsignedSample &sample) {
  int srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT) - 256)
      << 15;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TMono16Sample &sample) {
  int srcval = sample.getValue(TSound::MONO) << 8;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TStereo16Sample &sample) {
  int srcval = (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT))
               << 7;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TMono24Sample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TStereo24Sample &sample) {
  int srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) >> 1;
  return TMono24Sample(srcval);
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 8388608.0;
  return TMono24Sample(tcrop((int)fval, -8388608, 8388607));
}

//------------------------------------------------------------------------------

inline TMono24Sample TMono24Sample::from(const TStereo32FloatSample &sample) {
  float fval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) *
      4194304.0;
  return TMono24Sample(tcrop((int)fval, -8388608, 8388607));
}

//==============================================================================

inline TStereo24Sample TStereo24Sample::from(
    const TMono8UnsignedSample &sample) {
  int srcval = (sample.getValue(TSound::MONO) - 128) << 16;
  return TStereo24Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TMono8SignedSample &sample) {
  int srcval = sample.getValue(TSound::MONO) << 16;
  return TStereo24Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(
    const TStereo8SignedSample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) << 16;
  int srcvalR = sample.getValue(TSound::RIGHT) << 16;
  return TStereo24Sample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(
    const TStereo8UnsignedSample &sample) {
  int srcvalL = (sample.getValue(TSound::LEFT) - 128) << 16;
  int srcvalR = (sample.getValue(TSound::RIGHT) - 128) << 16;
  return TStereo24Sample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TMono16Sample &sample) {
  int srcval = sample.getValue(TSound::MONO) << 8;
  return TStereo24Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TStereo16Sample &sample) {
  int srcvalL = sample.getValue(TSound::LEFT) << 8;
  int srcvalR = sample.getValue(TSound::RIGHT) << 8;
  return TStereo24Sample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TMono24Sample &sample) {
  int srcval = sample.getValue(TSound::MONO);
  return TStereo24Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TStereo24Sample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TMono32FloatSample &sample) {
  float fval = sample.getValue(TSound::MONO) * 8388608.0;
  int srcval = tcrop((int)fval, -8388608, 8388607);
  return TStereo24Sample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo24Sample TStereo24Sample::from(const TStereo32FloatSample &sample) {
  float fvalL = sample.getValue(TSound::LEFT) * 8388608.0;
  float fvalR = sample.getValue(TSound::RIGHT) * 8388608.0;
  return TStereo24Sample(tcrop((int)fvalL, -8388608, 8388607),
                         tcrop((int)fvalR, -8388608, 8388607));
}

//==============================================================================

inline TMono32FloatSample TMono32FloatSample::from(const TMono8UnsignedSample &sample) {
  float srcval = (sample.getValue(TSound::MONO) - 128) / 128.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TMono8SignedSample &sample) {
  float srcval = sample.getValue(TSound::MONO) / 128.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TStereo8SignedSample &sample) {
  float srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) / 256.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TStereo8UnsignedSample &sample) {
  float srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT) - 256) /
      256.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TMono16Sample &sample) {
  float srcval = sample.getValue(TSound::MONO) / 32768.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TStereo16Sample &sample) {
  float srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) /
      65536.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TMono24Sample &sample) {
  float srcval = sample.getValue(TSound::MONO) / 8388608.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TStereo24Sample &sample) {
  float srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) /
      16777216.0;
  return TMono32FloatSample(srcval);
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TMono32FloatSample &sample) {
  return sample;
}

//------------------------------------------------------------------------------

inline TMono32FloatSample TMono32FloatSample::from(const TStereo32FloatSample &sample) {
  float srcval =
      (sample.getValue(TSound::LEFT) + sample.getValue(TSound::RIGHT)) / 2.0;
  return TMono32FloatSample(srcval);
}

//==============================================================================

inline TStereo32FloatSample TStereo32FloatSample::from(
    const TMono8UnsignedSample &sample) {
  float srcval = (sample.getValue(TSound::MONO) - 128) / 128.0;
  return TStereo32FloatSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TMono8SignedSample &sample) {
  float srcval = sample.getValue(TSound::MONO) / 128.0;
  return TStereo32FloatSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(
    const TStereo8SignedSample &sample) {
  float srcvalL = sample.getValue(TSound::LEFT) / 128.0;
  float srcvalR = sample.getValue(TSound::RIGHT) / 128.0;
  return TStereo32FloatSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(
    const TStereo8UnsignedSample &sample) {
  float srcvalL = (sample.getValue(TSound::LEFT) - 128) / 128.0;
  float srcvalR = (sample.getValue(TSound::RIGHT) - 128) / 128.0;
  return TStereo32FloatSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TMono16Sample &sample) {
  float srcval = sample.getValue(TSound::MONO) / 32768.0;
  return TStereo32FloatSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TStereo16Sample &sample) {
  float srcvalL = sample.getValue(TSound::LEFT) / 32768.0;
  float srcvalR = sample.getValue(TSound::RIGHT) / 32768.0;
  return TStereo32FloatSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TMono24Sample &sample) {
  float srcval = sample.getValue(TSound::MONO) / 8388608.0;
  return TStereo32FloatSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TStereo24Sample &sample) {
  float srcvalL = sample.getValue(TSound::LEFT) / 8388608.0;
  float srcvalR = sample.getValue(TSound::RIGHT) / 8388608.0;
  return TStereo32FloatSample(srcvalL, srcvalR);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TMono32FloatSample &sample) {
  float srcval = sample.getValue(TSound::MONO);
  return TStereo32FloatSample(srcval, srcval);
}

//------------------------------------------------------------------------------

inline TStereo32FloatSample TStereo32FloatSample::from(const TStereo32FloatSample &sample) {
  return sample;
}

#endif
