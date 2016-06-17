

#include "tsound.h"
#include "tsound_t.h"
#include "tconvert.h"

#define TRK_M8 9
#define TRK_S8 10
#define TRK_M16 17
#define TRK_S16 18
#define TRK_M24 25
#define TRK_S24 26

//==============================================================================

DEFINE_CLASS_CODE(TSoundTrack, 12)

//------------------------------------------------------------------------------

TSoundTrack::TSoundTrack()
    : TSmartObject(m_classCode)
    , m_parent(0)
    , m_buffer(0)
    , m_bufferOwner(false) {}

//------------------------------------------------------------------------------

TSoundTrack::TSoundTrack(TUINT32 sampleRate, int bitPerSample, int channelCount,
                         int sampleSize, TINT32 sampleCount,
                         bool isSampleSigned)

    : TSmartObject(m_classCode)
    , m_sampleRate(sampleRate)
    , m_sampleSize(sampleSize)
    , m_bitPerSample(bitPerSample)
    , m_sampleCount(sampleCount)
    , m_channelCount(channelCount)
    , m_parent(0)
    , m_bufferOwner(true) {
  m_buffer = (UCHAR *)malloc(sampleCount * m_sampleSize);
  if (!m_buffer) return;

  // m_buffer = new UCHAR[sampleCount*m_sampleSize];
  if (isSampleSigned)
    memset(m_buffer, 0, sampleCount * sampleSize);
  else
    memset(m_buffer, 127, sampleCount * sampleSize);
}

//------------------------------------------------------------------------------

TSoundTrack::TSoundTrack(TUINT32 sampleRate, int bitPerSample, int channelCount,
                         int sampleSize, TINT32 sampleCount, UCHAR *buffer,
                         TSoundTrack *parent)

    : TSmartObject(m_classCode)
    , m_sampleRate(sampleRate)
    , m_sampleSize(sampleSize)
    , m_bitPerSample(bitPerSample)
    , m_sampleCount(sampleCount)
    , m_channelCount(channelCount)
    , m_parent(parent)
    , m_buffer(buffer)
    , m_bufferOwner(false) {
  if (m_parent) m_parent->addRef();
}

//------------------------------------------------------------------------------

TSoundTrack::~TSoundTrack() {
  if (m_parent) m_parent->release();
  // if (m_buffer && m_bufferOwner) delete [] m_buffer;
  if (m_buffer && m_bufferOwner) free(m_buffer);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrack::create(TUINT32 sampleRate, int bitPerSample,
                                 int channelCount, TINT32 sampleCount,
                                 bool signedSample) {
  TSoundTrackP st;
  int type = bitPerSample + channelCount;
  switch (type) {
  case TRK_M8:
    if (signedSample)
      st = new TSoundTrackMono8Signed(sampleRate, channelCount, sampleCount);
    else
      st = new TSoundTrackMono8Unsigned(sampleRate, channelCount, sampleCount);
    break;
  case TRK_S8:
    if (signedSample)
      st = new TSoundTrackStereo8Signed(sampleRate, channelCount, sampleCount);
    else
      st =
          new TSoundTrackStereo8Unsigned(sampleRate, channelCount, sampleCount);
    break;

  case TRK_M16:
    st = new TSoundTrackMono16(sampleRate, channelCount, sampleCount);
    break;

  case TRK_S16:
    st = new TSoundTrackStereo16(sampleRate, channelCount, sampleCount);
    break;

  case TRK_M24:
    st = new TSoundTrackMono24(sampleRate, channelCount, sampleCount);
    break;

  case TRK_S24:
    st = new TSoundTrackStereo24(sampleRate, channelCount, sampleCount);
    break;

  default:
    std::string s;
    s = "Type " + std::to_string(sampleRate) + " Hz " +
        std::to_string(bitPerSample) + " bits ";
    if (channelCount == 1)
      s += "mono: ";
    else
      s += "stereo: ";
    s += "Unsupported\n";

    throw TException(s);
  }

  if (!st->getRawData()) {
    return 0;
  }
  return st;
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrack::create(TUINT32 sampleRate, int bitPerSample,
                                 int channelCount, TINT32 sampleCount,
                                 void *buffer, bool signedSample) {
  TSoundTrackP st;
  int type = bitPerSample + channelCount;
  switch (type) {
  case TRK_M8:
    if (signedSample)
      st = new TSoundTrackMono8Signed(sampleRate, channelCount, sampleCount,
                                      (TMono8SignedSample *)buffer, 0);
    else
      st = new TSoundTrackMono8Unsigned(sampleRate, channelCount, sampleCount,
                                        (TMono8UnsignedSample *)buffer, 0);
    break;
  case TRK_S8:
    if (signedSample)
      st = new TSoundTrackStereo8Signed(sampleRate, channelCount, sampleCount,
                                        (TStereo8SignedSample *)buffer, 0);
    else
      st = new TSoundTrackStereo8Unsigned(sampleRate, channelCount, sampleCount,
                                          (TStereo8UnsignedSample *)buffer, 0);
    break;

  case TRK_M16:
    st = new TSoundTrackMono16(sampleRate, channelCount, sampleCount,
                               (TMono16Sample *)buffer, 0);
    break;

  case TRK_S16:
    st = new TSoundTrackStereo16(sampleRate, channelCount, sampleCount,
                                 (TStereo16Sample *)buffer, 0);
    break;

  case TRK_M24:
    st = new TSoundTrackMono24(sampleRate, channelCount, sampleCount,
                               (TMono24Sample *)buffer, 0);
    break;

  case TRK_S24:
    st = new TSoundTrackStereo24(sampleRate, channelCount, sampleCount,
                                 (TStereo24Sample *)buffer, 0);
    break;

  default:
    std::string s;
    s = "Type " + std::to_string(sampleRate) + " Hz " +
        std::to_string(bitPerSample) + " bits ";
    if (channelCount == 1)
      s += "mono: ";
    else
      s += "stereo: ";
    s += "Unsupported\n";

    throw TException(s);
  }

  return st;
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrack::create(const TSoundTrackFormat &format,
                                 TINT32 sampleCount, void *buffer) {
  return TSoundTrack::create((int)format.m_sampleRate, format.m_bitPerSample,
                             format.m_channelCount, sampleCount, buffer,
                             format.m_signedSample);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrack::create(const TSoundTrackFormat &format,
                                 TINT32 sampleCount) {
  return TSoundTrack::create((int)format.m_sampleRate, format.m_bitPerSample,
                             format.m_channelCount, sampleCount,
                             format.m_signedSample);
}

//------------------------------------------------------------------------------

TINT32 TSoundTrack::secondsToSamples(double s) const {
  double dsamp = s * m_sampleRate;
  TINT32 lsamp = (TINT32)dsamp;
  if ((double)lsamp < dsamp - TConsts::epsilon) lsamp++;
  return lsamp;
}

//------------------------------------------------------------------------------

double TSoundTrack::samplesToSeconds(TINT32 f) const {
  return f / (double)m_sampleRate;
}

//==============================================================================

bool TSoundTrackFormat::operator==(const TSoundTrackFormat &rhs) {
  return (m_sampleRate == rhs.m_sampleRate &&
          m_bitPerSample == rhs.m_bitPerSample &&
          m_channelCount == rhs.m_channelCount &&
          m_signedSample == rhs.m_signedSample);
}

//------------------------------------------------------------------------------

bool TSoundTrackFormat::operator!=(const TSoundTrackFormat &rhs) {
  return !operator==(rhs);
}

//==============================================================================

double TSoundTrack::getDuration() const {
  return samplesToSeconds(m_sampleCount);
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundTrack::getFormat() const {
  return TSoundTrackFormat(getSampleRate(), getBitPerSample(),
                           getChannelCount(), isSampleSigned());
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrack::extract(double t0, double t1) {
  return extract(secondsToSamples(t0), secondsToSamples(t1));
}

//------------------------------------------------------------------------------

void TSoundTrack::copy(const TSoundTrackP &src, double dst_t0) {
  copy(src, secondsToSamples(dst_t0));
}

//------------------------------------------------------------------------------

void TSoundTrack::blank(double t0, double t1) {
  blank(secondsToSamples(t0), secondsToSamples(t1));
}
//------------------------------------------------------------------------------

double TSoundTrack::getPressure(double second, TSound::Channel chan) const {
  return getPressure(secondsToSamples(second), chan);
}

//------------------------------------------------------------------------------

void TSoundTrack::getMinMaxPressure(double t0, double t1, TSound::Channel chan,
                                    double &min, double &max) const {
  getMinMaxPressure(secondsToSamples(t0), secondsToSamples(t1), chan, min, max);
}

//------------------------------------------------------------------------------

void TSoundTrack::getMinMaxPressure(TSound::Channel chan, double &min,
                                    double &max) const {
  getMinMaxPressure(0, (TINT32)(getSampleCount() - 1), chan, min, max);
}

//------------------------------------------------------------------------------

double TSoundTrack::getMaxPressure(double t0, double t1,
                                   TSound::Channel chan) const {
  return getMaxPressure(secondsToSamples(t0), secondsToSamples(t1), chan);
}

//------------------------------------------------------------------------------

double TSoundTrack::getMaxPressure(TSound::Channel chan) const {
  return getMaxPressure(0, (TINT32)(getSampleCount() - 1), chan);
}

//------------------------------------------------------------------------------

double TSoundTrack::getMinPressure(double t0, double t1,
                                   TSound::Channel chan) const {
  return getMinPressure(secondsToSamples(t0), secondsToSamples(t1), chan);
}

//------------------------------------------------------------------------------

double TSoundTrack::getMinPressure(TSound::Channel chan) const {
  return getMinPressure(0, (TINT32)(getSampleCount() - 1), chan);
}

//------------------------------------------------------------------------------
