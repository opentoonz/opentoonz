

#include "tsound_t.h"
#include "texception.h"
#include "tthread.h"

#include <errno.h>
#ifdef __sgi
#include <audio.h>
#endif
#include <unistd.h>
#include <queue>
#include <set>

#ifndef __sgi
// Warning, this file is for SGI currently,
// otherwise we just stub out the functions.
typedef unsigned long ULONG;
typedef void *ALport;
typedef void *ALvalue;
typedef void *ALpv;
#endif

// forward declaration
namespace {
bool isInterfaceSupported(int deviceType, int interfaceType);
bool setDefaultInput(TSoundInputDevice::Source type);
bool setDefaultOutput();
bool isChangeOutput(ULONG sampleRate);
}
//==============================================================================

class TSoundOutputDeviceImp {
public:
  ALport m_port;
  bool m_stopped;
  bool m_isPlaying;
  bool m_looped;
  TSoundTrackFormat m_currentFormat;
  std::queue<TSoundTrackP> m_queuedSoundTracks;
  std::set<int> m_supportedRate;

  TThread::Executor m_executor;
  TThread::Mutex m_mutex;

  TSoundOutputDeviceImp()
      : m_stopped(false)
      , m_isPlaying(false)
      , m_looped(false)
      , m_port(NULL)
      , m_queuedSoundTracks()
      , m_supportedRate(){};

  ~TSoundOutputDeviceImp(){};

  bool doOpenDevice(const TSoundTrackFormat &format);
  void insertAllRate();
  bool verifyRate();
};

if (!isInterfaceSupported(AL_DEFAULT_OUTPUT, AL_SPEAKER_IF_TYPE))
  return false;  // throw TException("Speakers are not supported");

int dev =
    alGetResourceByName(AL_SYSTEM, (char *)"Headphone/Speaker", AL_DEVICE_TYPE);
if (!dev) return false;  // throw TException("invalid device speakers");

pvbuf[0].param   = AL_DEFAULT_OUTPUT;
pvbuf[0].value.i = dev;
alSetParams(AL_SYSTEM, pvbuf, 1);

ALfixed buf[2] = {alDoubleToFixed(0), alDoubleToFixed(0)};

config = alNewConfig();
// qui devo metterci gli altoparlanti e poi setto i valori per il default
// output
pvbuf[0].param     = AL_RATE;
pvbuf[0].value.ll  = alDoubleToFixed((double)format.m_sampleRate);
pvbuf[1].param     = AL_GAIN;
pvbuf[1].value.ptr = buf;
pvbuf[1].sizeIn    = 8;
pvbuf[2].param     = AL_INTERFACE;
pvbuf[2].value.i   = AL_SPEAKER_IF_TYPE;

if (alSetParams(AL_DEFAULT_OUTPUT, pvbuf, 3) < 0) return false;
// throw TException("Unable to set params for output device");

if (alSetChannels(config, format.m_channelCount) == -1)
  return false;  // throw TException("Error to setting audio hardware.");

int bytePerSample = format.m_bitPerSample >> 3;
switch (bytePerSample) {
case 3:
  bytePerSample++;
  break;
default:
  break;
}

bool TSoundOutputDeviceImp::doOpenDevice(const TSoundTrackFormat &format) {
#ifdef __sgi
  ALconfig config;
  ALpv pvbuf[3];

  m_currentFormat = format;

  // AL_MONITOR_CTL fa parte dei vecchi andrebbero trovati quelli nuovi
  pvbuf[0].param = AL_PORT_COUNT;
  pvbuf[1].param = AL_MONITOR_CTL;
  if (alGetParams(AL_DEFAULT_OUTPUT, pvbuf, 2) < 0)
    if (oserror() == AL_BAD_DEVICE_ACCESS)
      return false;  // throw TException("Could not access audio hardware.");

  if (!isInterfaceSupported(AL_DEFAULT_OUTPUT, AL_SPEAKER_IF_TYPE))
    return false;  // throw TException("Speakers are not supported");

  int dev = alGetResourceByName(AL_SYSTEM, (char *)"Headphone/Speaker",
                                AL_DEVICE_TYPE);
  if (!dev) return false;  // throw TException("invalid device speakers");

  pvbuf[0].param   = AL_DEFAULT_OUTPUT;
  pvbuf[0].value.i = dev;
  alSetParams(AL_SYSTEM, pvbuf, 1);

  ALfixed buf[2] = {alDoubleToFixed(0), alDoubleToFixed(0)};

  config = alNewConfig();
  // qui devo metterci gli altoparlanti e poi setto i valori per il default
  // output
  pvbuf[0].param     = AL_RATE;
  pvbuf[0].value.ll  = alDoubleToFixed((double)format.m_sampleRate);
  pvbuf[1].param     = AL_GAIN;
  pvbuf[1].value.ptr = buf;
  pvbuf[1].sizeIn    = 8;
  pvbuf[2].param     = AL_INTERFACE;
  pvbuf[2].value.i   = AL_SPEAKER_IF_TYPE;

  if (alSetParams(AL_DEFAULT_OUTPUT, pvbuf, 3) < 0) return false;
  // throw TException("Unable to set params for output device");

  if (alSetChannels(config, format.m_channelCount) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  int bytePerSample = format.m_bitPerSample >> 3;
  switch (bytePerSample) {
  case 3:
    bytePerSample++;
    break;
  default:
    break;
  }

  if (alSetWidth(config, bytePerSample) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  if (alSetSampFmt(config, AL_SAMPFMT_TWOSCOMP) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  if (alSetQueueSize(config, (TINT32)format.m_sampleRate) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  m_port = alOpenPort("AudioOutput", "w", config);
  if (!m_port) return false;  // throw TException("Could not open audio port.");

  alFreeConfig(config);
  return true;
#else
  return false;
#endif
}

//-----------------------------------------------------------------------------

void TSoundOutputDeviceImp::insertAllRate() {
  m_supportedRate.insert(8000);
  m_supportedRate.insert(11025);
  m_supportedRate.insert(16000);
  m_supportedRate.insert(22050);
  m_supportedRate.insert(32000);
  m_supportedRate.insert(44100);
  m_supportedRate.insert(48000);
}

//-----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::verifyRate() {
#ifdef __sgi
  // Sample Rate
  ALparamInfo pinfo;
  int ret = alGetParamInfo(AL_DEFAULT_OUTPUT, AL_RATE, &pinfo);
  if (ret != -1 && pinfo.elementType == AL_FIXED_ELEM) {
    int min = (int)alFixedToDouble(pinfo.min.ll);
    int max = (int)alFixedToDouble(pinfo.max.ll);

    std::set<int>::iterator it;
    for (it = m_supportedRate.begin(); it != m_supportedRate.end(); ++it)
      if (*it < min || *it > max) m_supportedRate.erase(*it);
    if (m_supportedRate.end() == m_supportedRate.begin()) return false;
  } else if (ret == AL_BAD_PARAM)
    return false;
  else
    return false;
#endif
  return true;
}

//==============================================================================

class PlayTask : public TThread::Runnable {
public:
  TSoundOutputDeviceImp *m_devImp;
  TSoundTrackP m_sndtrack;

  PlayTask(TSoundOutputDeviceImp *devImp, const TSoundTrackP &st)
      : TThread::Runnable(), m_devImp(devImp), m_sndtrack(st){};

  ~PlayTask(){};

  void run();
};

void PlayTask::run() {
#ifdef __sgi
  int leftToPlay = m_sndtrack->getSampleCount();
  int i          = 0;

  if (!m_devImp->m_port ||
      (m_devImp->m_currentFormat != m_sndtrack->getFormat()) ||
      isChangeOutput(m_sndtrack->getSampleRate()))
    if (!m_devImp->doOpenDevice(m_sndtrack->getFormat())) return;

  while ((leftToPlay > 0) && m_devImp->m_isPlaying) {
    int fillable = alGetFillable(m_devImp->m_port);
    if (!fillable) continue;

    if (fillable < leftToPlay) {
      alWriteFrames(m_devImp->m_port, (void *)(m_sndtrack->getRawData() + i),
                    fillable);
      // ricorda getSampleSize restituisce m_sampleSize che comprende gia'
      // la moltiplicazione per il numero dei canali
      i += fillable * m_sndtrack->getSampleSize();
      leftToPlay -= fillable;
    } else {
      alWriteFrames(m_devImp->m_port, (void *)(m_sndtrack->getRawData() + i),
                    leftToPlay);
      leftToPlay = 0;
    }
  }

  if (!m_devImp->m_stopped) {
    while (ALgetfilled(m_devImp->m_port) > 0) sginap(1);
    {
      TThread::ScopedLock sl(m_devImp->m_mutex);
      if (!m_devImp->m_looped) m_devImp->m_queuedSoundTracks.pop();
      if (m_devImp->m_queuedSoundTracks.empty()) {
        m_devImp->m_isPlaying = false;
        m_devImp->m_stopped   = true;
        m_devImp->m_looped    = false;
      } else {
        m_devImp->m_executor.addTask(
            new PlayTask(m_devImp, m_devImp->m_queuedSoundTracks.front()));
      }
    }
  } else {
    alDiscardFrames(m_devImp->m_port, alGetFilled(m_devImp->m_port));
    while (!m_devImp->m_queuedSoundTracks.empty())
      m_devImp->m_queuedSoundTracks.pop();
  }
#endif
}

//==============================================================================

TSoundOutputDevice::TSoundOutputDevice() : m_imp(new TSoundOutputDeviceImp) {
#ifdef __sgi
  if (!setDefaultOutput())
    throw TSoundDeviceException(TSoundDeviceException::UnableSetDevice,
                                "Speaker not supported");
  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
  m_imp->insertAllRate();
#endif
}

//------------------------------------------------------------------------------

TSoundOutputDevice::~TSoundOutputDevice() {
  close();
  delete m_imp;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::installed() {
#ifdef __sgi
  if (alQueryValues(AL_SYSTEM, AL_DEFAULT_OUTPUT, 0, 0, 0, 0) <= 0)
    return false;
#endif
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::open(const TSoundTrackP &st) {
  if (!m_imp->doOpenDevice(st->getFormat()))
    throw TSoundDeviceException(
        TSoundDeviceException::UnableOpenDevice,
        "Problem to open the output device or set some params");
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::close() {
#ifdef __sgi
  stop();
  if (m_imp->m_port) alClosePort(m_imp->m_port);
  m_imp->m_port = NULL;
#endif
  return true;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1,
                              bool loop, bool scrubbing) {
#ifdef __sgi
  if (!st->getSampleCount()) return;

  {
    TThread::ScopedLock sl(m_imp->m_mutex);
    if (m_imp->m_looped)
      throw TSoundDeviceException(
          TSoundDeviceException::Busy,
          "Unable to queue another playback when the sound player is looping");

    m_imp->m_isPlaying = true;
    m_imp->m_stopped   = false;
    m_imp->m_looped    = loop;
  }

  TSoundTrackFormat fmt;
  try {
    fmt = getPreferredFormat(st->getFormat());
    if (fmt != st->getFormat()) {
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported Format");
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }

  assert(s1 >= s0);
  TSoundTrackP subTrack = st->extract(s0, s1);

  // far partire il thread
  if (m_imp->m_queuedSoundTracks.empty()) {
    m_imp->m_queuedSoundTracks.push(subTrack);

    m_imp->m_executor.addTask(new PlayTask(m_imp, subTrack));
  } else
    m_imp->m_queuedSoundTracks.push(subTrack);
#endif
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
#ifdef __sgi
  if (!m_imp->m_isPlaying) return;

  TThread::ScopedLock sl(m_imp->m_mutex);
  m_imp->m_isPlaying = false;
  m_imp->m_stopped   = true;
  m_imp->m_looped    = false;
#endif
}

//------------------------------------------------------------------------------

#if 0
double TSoundOutputDevice::getVolume() {
#ifdef __sgi
  ALpv pv[1];
  ALfixed value[2];

  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  pv[0].param     = AL_GAIN;
  pv[0].value.ptr = value;
  pv[0].sizeIn    = 8;
  alGetParams(AL_DEFAULT_OUTPUT, pv, 1);

  double val =
      (((alFixedToDouble(value[0]) + alFixedToDouble(value[1])) / 2.) + 60.) /
      8.05;
  return val;
#else
  return 0.0f;
#endif
}
#endif

//------------------------------------------------------------------------------

#if 0
bool TSoundOutputDevice::setVolume(double volume) {
  ALpv pv[1];
  ALfixed value[2];

  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  double val = -60. + 8.05 * volume;
  value[0]   = alDoubleToFixed(val);
  value[1]   = alDoubleToFixed(val);

  pv[0].param     = AL_GAIN;
  pv[0].value.ptr = value;
  pv[0].sizeIn    = 8;
  if (alSetParams(AL_DEFAULT_OUTPUT, pv, 1) < 0) return false;
  return true;
}
#endif

//------------------------------------------------------------------------------

#if 0
bool TSoundOutputDevice::supportsVolume()
{
#ifdef __sgi
  ALparamInfo pinfo;
  int ret;
  ret        = alGetParamInfo(AL_DEFAULT_OUTPUT, AL_GAIN, &pinfo);
  double min = alFixedToDouble(pinfo.min.ll);
  double max = alFixedToDouble(pinfo.max.ll);
  if ((ret != -1) && (min != max) && (max != 0.0))
    return true;
  else if ((ret == AL_BAD_PARAM) || ((min == max) && (max == 0.0)))
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "It is impossible to chamge setting of volume");
  else
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Output device is not accessible");
#else
  return true;
#endif
}
#endif

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const { return m_imp->m_isPlaying; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() {
#ifdef __sgi
  TThread::ScopedLock sl(m_imp->m_mutex);
  return m_imp->m_looped;
#else
  return false;
#endif
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) {
#ifdef __sgi
  TThread::ScopedLock sl(m_imp->m_mutex);
  m_imp->m_looped = loop;
#endif
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                         int channelCount,
                                                         int bitPerSample) {
  TSoundTrackFormat fmt;
#ifdef __sgi
  int ret;

  if (!m_imp->verifyRate())
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                "There isn't any support rate");

  if (m_imp->m_supportedRate.find((int)sampleRate) ==
      m_imp->m_supportedRate.end()) {
    std::set<int>::iterator it =
        m_imp->m_supportedRate.lower_bound((int)sampleRate);
    if (it == m_imp->m_supportedRate.end()) {
      it = std::max_element(m_imp->m_supportedRate.begin(),
                            m_imp->m_supportedRate.end());
      if (it != m_imp->m_supportedRate.end())
        sampleRate = *(m_imp->m_supportedRate.rbegin());
      else
        throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                    "There isn't a supported rate");
    } else
      sampleRate = *it;
  }

  int value;
  ALvalue vals[32];
  if ((ret = alQueryValues(AL_DEFAULT_OUTPUT, AL_CHANNELS, vals, 32, 0, 0)) > 0)
    for (int i = 0; i < ret; ++i) value = vals[i].i;
  else if (oserror() == AL_BAD_PARAM)
    throw TSoundDeviceException(
        TSoundDeviceException::NoMixer,
        "It is impossible ask for the max numbers of channels supported");
  else
    throw TSoundDeviceException(
        TSoundDeviceException::NoMixer,
        "It is impossibile information about ouput device");

  if (value > 2) value = 2;
  if (channelCount > value)
    channelCount = value;
  else if (channelCount <= 0)
    channelCount = 1;

  if (bitPerSample <= 8)
    bitPerSample = 8;
  else if (bitPerSample <= 16)
    bitPerSample = 16;
  else
    bitPerSample = 24;

  fmt.m_bitPerSample = bitPerSample;
  fmt.m_channelCount = channelCount;
  fmt.m_sampleRate   = sampleRate;
  fmt.m_signedSample = true;
#endif

  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  try {
    return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                              format.m_bitPerSample);
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
}

//==============================================================================
//==============================================================================
//                REGISTRAZIONE
//==============================================================================
//==============================================================================

class TSoundInputDeviceImp {
public:
  ALport m_port;
  bool m_stopped;
  bool m_isRecording;
  bool m_oneShotRecording;

  TINT32 m_recordedSampleCount;

  vector<char *> m_recordedBlocks;
  vector<int> m_samplePerBlocks;
  TSoundTrackFormat m_currentFormat;
  TSoundTrackP m_st;
  std::set<int> m_supportedRate;

  TThread::Executor m_executor;

  TSoundInputDeviceImp()
      : m_stopped(false)
      , m_isRecording(false)
      , m_port(NULL)
      , m_recordedBlocks()
      , m_samplePerBlocks()
      , m_recordedSampleCount(0)
      , m_oneShotRecording(false)
      , m_st(0)
      , m_supportedRate(){};

  ~TSoundInputDeviceImp(){};

  bool doOpenDevice(const TSoundTrackFormat &format,
                    TSoundInputDevice::Source devType);
  void insertAllRate();
  bool verifyRate();
};

bool TSoundInputDeviceImp::doOpenDevice(const TSoundTrackFormat &format,
                                        TSoundInputDevice::Source devType) {
#ifdef __sgi
  ALconfig config;
  ALpv pvbuf[2];

  m_currentFormat = format;

  // AL_MONITOR_CTL fa parte dei vecchi andrebbero trovati quelli nuovi
  pvbuf[0].param = AL_PORT_COUNT;
  pvbuf[1].param = AL_MONITOR_CTL;
  if (alGetParams(AL_DEFAULT_INPUT, pvbuf, 2) < 0)
    if (oserror() == AL_BAD_DEVICE_ACCESS)
      return false;  // throw TException("Could not access audio hardware.");

  config = alNewConfig();

  if (!setDefaultInput(devType))
    return false;  // throw TException("Could not set the input device
                   // specified");

  pvbuf[0].param    = AL_RATE;
  pvbuf[0].value.ll = alDoubleToFixed(format.m_sampleRate);

  ALfixed buf[2]     = {alDoubleToFixed(0), alDoubleToFixed(0)};
  pvbuf[1].param     = AL_GAIN;
  pvbuf[1].value.ptr = buf;
  pvbuf[1].sizeIn    = 8;

  if (alSetParams(AL_DEFAULT_INPUT, pvbuf, 2) < 0)
    return false;  // throw TException("Problem to set params ");

  if (alSetChannels(config, format.m_channelCount) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  int bytePerSample = format.m_bitPerSample >> 3;
  switch (bytePerSample) {
  case 3:
    bytePerSample++;
    break;
  default:
    break;
  }
  if (alSetWidth(config, bytePerSample) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  if (alSetSampFmt(config, AL_SAMPFMT_TWOSCOMP) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  if (alSetQueueSize(config, (TINT32)format.m_sampleRate) == -1)
    return false;  // throw TException("Error to setting audio hardware.");

  alSetDevice(config, AL_DEFAULT_INPUT);

  m_port = alOpenPort("AudioInput", "r", config);
  if (!m_port) return false;  // throw TException("Could not open audio port.");

  alFreeConfig(config);
#endif
  return true;
}

//-----------------------------------------------------------------------------

void TSoundInputDeviceImp::insertAllRate() {
  m_supportedRate.insert(8000);
  m_supportedRate.insert(11025);
  m_supportedRate.insert(16000);
  m_supportedRate.insert(22050);
  m_supportedRate.insert(32000);
  m_supportedRate.insert(44100);
  m_supportedRate.insert(48000);
}

//-----------------------------------------------------------------------------

bool TSoundInputDeviceImp::verifyRate() {
#ifdef __sgi
  // Sample Rate
  ALparamInfo pinfo;
  int ret = alGetParamInfo(AL_DEFAULT_INPUT, AL_RATE, &pinfo);
  if (ret != -1 && pinfo.elementType == AL_FIXED_ELEM) {
    int min = (int)alFixedToDouble(pinfo.min.ll);
    int max = (int)alFixedToDouble(pinfo.max.ll);

    std::set<int>::iterator it;
    for (it = m_supportedRate.begin(); it != m_supportedRate.end(); ++it)
      if (*it < min || *it > max) m_supportedRate.erase(*it);
    if (m_supportedRate.end() == m_supportedRate.begin()) return false;
  } else if (ret == AL_BAD_PARAM)
    return false;
  else
    return false;
#endif
  return true;
}

//==============================================================================

class RecordTask : public TThread::Runnable {
public:
  TSoundInputDeviceImp *m_devImp;
  int m_ByteToSample;

  RecordTask(TSoundInputDeviceImp *devImp, int numByte)
      : TThread::Runnable(), m_devImp(devImp), m_ByteToSample(numByte){};

  ~RecordTask(){};

  void run();
};

void RecordTask::run() {
#ifdef __sgi
  TINT32 byteRecordedSample = 0;
  int filled                = alGetFilled(m_devImp->m_port);

  if (m_devImp->m_oneShotRecording) {
    char *rawData = m_devImp->m_recordedBlocks.front();
    int sampleSize;

    if ((m_devImp->m_currentFormat.m_bitPerSample >> 3) == 3)
      sampleSize = 4;
    else
      sampleSize = (m_devImp->m_currentFormat.m_bitPerSample >> 3);

    sampleSize *= m_devImp->m_currentFormat.m_channelCount;
    while ((byteRecordedSample <= (m_ByteToSample - filled * sampleSize)) &&
           m_devImp->m_isRecording) {
      alReadFrames(m_devImp->m_port, (void *)(rawData + byteRecordedSample),
                   filled);
      byteRecordedSample += filled * sampleSize;
      filled = alGetFilled(m_devImp->m_port);
    }

    if (m_devImp->m_isRecording) {
      alReadFrames(m_devImp->m_port, (void *)(rawData + byteRecordedSample),
                   (m_ByteToSample - byteRecordedSample) / sampleSize);
      while (alGetFillable(m_devImp->m_port) > 0) sginap(1);
    }
  } else {
    while (m_devImp->m_isRecording) {
      filled = alGetFilled(m_devImp->m_port);
      if (filled > 0) {
        char *dataBuffer = new char[filled * m_ByteToSample];
        m_devImp->m_recordedBlocks.push_back(dataBuffer);
        m_devImp->m_samplePerBlocks.push_back(filled * m_ByteToSample);

        alReadFrames(m_devImp->m_port, (void *)dataBuffer, filled);
        m_devImp->m_recordedSampleCount += filled;
      }
    }
    while (alGetFillable(m_devImp->m_port) > 0) sginap(1);
  }
  alClosePort(m_devImp->m_port);
  m_devImp->m_port    = 0;
  m_devImp->m_stopped = true;
#endif
}

//==============================================================================

TSoundInputDevice::TSoundInputDevice() : m_imp(new TSoundInputDeviceImp) {}

//------------------------------------------------------------------------------

TSoundInputDevice::~TSoundInputDevice() {
#ifdef __sgi
  if (m_imp->m_port) alClosePort(m_imp->m_port);
#endif
  delete m_imp;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::installed() {
#ifdef __sgi
  if (alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, 0, 0, 0, 0) <= 0) return false;
#endif
  return true;
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackFormat &format,
                               TSoundInputDevice::Source type) {
  if (m_imp->m_isRecording == true)
    throw TSoundDeviceException(TSoundDeviceException::Busy,
                                "Just another recoding is in progress");

  m_imp->m_recordedBlocks.clear();
  m_imp->m_samplePerBlocks.clear();

  // registra creando una nuova traccia
  m_imp->m_oneShotRecording = false;

  if (!setDefaultInput(type))
    throw TSoundDeviceException(TSoundDeviceException::UnableSetDevice,
                                "Error to set the input device");

  m_imp->insertAllRate();
  TSoundTrackFormat fmt;

  try {
    fmt = getPreferredFormat(format);
    if (fmt != format) {
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported Format");
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }

  if (!m_imp->m_port) m_imp->doOpenDevice(format, type);

  m_imp->m_currentFormat       = format;
  m_imp->m_isRecording         = true;
  m_imp->m_stopped             = false;
  m_imp->m_recordedSampleCount = 0;

  int bytePerSample = format.m_bitPerSample >> 3;
  switch (bytePerSample) {
  case 3:
    bytePerSample++;
    break;
  default:
    break;
  }
  bytePerSample *= format.m_channelCount;

  // far partire il thread
  /*TRecordThread *recordThread = new TRecordThread(m_imp, bytePerSample);
  if (!recordThread)
  {
  m_imp->m_isRecording = false;
  m_imp->m_stopped = true;
throw TSoundDeviceException(
TSoundDeviceException::UnablePrepare,
"Unable to create the recording thread");
  }
recordThread->start();*/
  m_imp->m_executor.addTask(new RecordTask(m_imp, bytePerSample));
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackP &st,
                               TSoundInputDevice::Source type) {
  if (m_imp->m_isRecording == true)
    throw TSoundDeviceException(TSoundDeviceException::Busy,
                                "Just another recoding is in progress");

  m_imp->m_recordedBlocks.clear();
  m_imp->m_samplePerBlocks.clear();

  if (!setDefaultInput(type))
    throw TSoundDeviceException(TSoundDeviceException::UnableSetDevice,
                                "Error to set the input device");

  m_imp->insertAllRate();
  TSoundTrackFormat fmt;

  try {
    fmt = getPreferredFormat(st->getFormat());
    if (fmt != st->getFormat()) {
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported Format");
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }

  if (!m_imp->m_port)
    if (!m_imp->doOpenDevice(st->getFormat(), type))
      throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice,
                                  "Unable to open input device");

  // Sovrascive un'intera o parte di traccia gia' esistente
  m_imp->m_oneShotRecording    = true;
  m_imp->m_currentFormat       = st->getFormat();
  m_imp->m_isRecording         = true;
  m_imp->m_stopped             = false;
  m_imp->m_recordedSampleCount = 0;
  m_imp->m_st                  = st;

  m_imp->m_recordedBlocks.push_back((char *)st->getRawData());

  int totByteToSample = st->getSampleCount() * st->getSampleSize();

  // far partire il thread
  /*TRecordThread *recordThread = new TRecordThread(m_imp, totByteToSample);
  if (!recordThread)
  {
  m_imp->m_isRecording = false;
  m_imp->m_stopped = true;
throw TSoundDeviceException(
TSoundDeviceException::UnablePrepare,
"Unable to create the recording thread");
  }
recordThread->start();*/
  m_imp->m_executor.addTask(new RecordTask(m_imp, totByteToSample));
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundInputDevice::stop() {
  TSoundTrackP st;
#ifdef __sgi
  if (!m_imp->m_isRecording)
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "No recording process is in execution");

  m_imp->m_isRecording = false;

  alDiscardFrames(m_imp->m_port, alGetFilled(m_imp->m_port));

  while (!m_imp->m_stopped) sginap(1);

  if (m_imp->m_oneShotRecording)
    st = m_imp->m_st;
  else {
    st = TSoundTrack::create(m_imp->m_currentFormat,
                             m_imp->m_recordedSampleCount);
    TINT32 bytesCopied = 0;

    for (int i = 0; i < (int)m_imp->m_recordedBlocks.size(); ++i) {
      memcpy((void *)(st->getRawData() + bytesCopied),
             m_imp->m_recordedBlocks[i], m_imp->m_samplePerBlocks[i]);

      delete[] m_imp->m_recordedBlocks[i];

      bytesCopied += m_imp->m_samplePerBlocks[i];
    }
    m_imp->m_samplePerBlocks.clear();
  }
#endif
  return st;
}

//------------------------------------------------------------------------------

double TSoundInputDevice::getVolume() {
#ifdef __sgi
  ALpv pv[1];
  ALfixed value[2];

  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  pv[0].param     = AL_GAIN;
  pv[0].value.ptr = value;
  pv[0].sizeIn    = 8;
  alGetParams(AL_DEFAULT_INPUT, pv, 1);

  double val =
      (((alFixedToDouble(value[0]) + alFixedToDouble(value[1])) / 2.) + 60.) /
      8.05;
  return val;
#else
  return 0.0f;
#endif
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::setVolume(double volume) {
#ifdef __sgi
  ALpv pv[1];
  ALfixed value[2];

  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  double val = -60. + 8.05 * volume;
  value[0]   = alDoubleToFixed(val);
  value[1]   = alDoubleToFixed(val);

  pv[0].param     = AL_GAIN;
  pv[0].value.ptr = value;
  pv[0].sizeIn    = 8;
  alSetParams(AL_DEFAULT_INPUT, pv, 1);
#endif
  return true;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::supportsVolume() {
#ifdef __sgi
  ALparamInfo pinfo;
  int ret;
  ret        = alGetParamInfo(AL_DEFAULT_INPUT, AL_GAIN, &pinfo);
  double min = alFixedToDouble(pinfo.min.ll);
  double max = alFixedToDouble(pinfo.max.ll);
  if ((ret != -1) && (min != max) && (max != 0.0))
    return true;
  else if ((ret == AL_BAD_PARAM) || ((min == max) && (max == 0.0)))
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "It is impossible to chamge setting of volume");
  else
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Output device is not accessible");
#endif
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                        int channelCount,
                                                        int bitPerSample) {
  TSoundTrackFormat fmt;
#ifdef __sgi
  int ret;

  if (!m_imp->verifyRate())
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                "There isn't any support rate");

  if (m_imp->m_supportedRate.find((int)sampleRate) ==
      m_imp->m_supportedRate.end()) {
    std::set<int>::iterator it =
        m_imp->m_supportedRate.lower_bound((int)sampleRate);
    if (it == m_imp->m_supportedRate.end()) {
      it = std::max_element(m_imp->m_supportedRate.begin(),
                            m_imp->m_supportedRate.end());
      if (it != m_imp->m_supportedRate.end())
        sampleRate = *(m_imp->m_supportedRate.rbegin());
      else
        throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                    "There isn't a supported rate");
    } else
      sampleRate = *it;
  }

  int value;
  ALvalue vals[32];
  if ((ret = alQueryValues(AL_DEFAULT_INPUT, AL_CHANNELS, vals, 32, 0, 0)) > 0)
    for (int i = 0; i < ret; ++i) value = vals[i].i;
  else if (oserror() == AL_BAD_PARAM)
    throw TSoundDeviceException(
        TSoundDeviceException::NoMixer,
        "It is impossible ask for the max nembers of channels supported");
  else
    throw TSoundDeviceException(
        TSoundDeviceException::NoMixer,
        "It is impossibile information about ouput device");

  if (value > 2) value = 2;
  if (channelCount > value)
    channelCount = value;
  else if (channelCount <= 0)
    channelCount = 1;

  if (bitPerSample <= 8)
    bitPerSample = 8;
  else if (bitPerSample <= 16)
    bitPerSample = 16;
  else
    bitPerSample = 24;

  fmt.m_bitPerSample = bitPerSample;
  fmt.m_channelCount = channelCount;
  fmt.m_sampleRate   = sampleRate;
  fmt.m_signedSample = true;
#endif
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  try {
    return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                              format.m_bitPerSample);
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::isRecording() { return m_imp->m_isRecording; }

//******************************************************************************
//******************************************************************************
//						funzioni per l'interazione con
// la
// libreria
// audio
//******************************************************************************
//******************************************************************************
namespace {
bool isInterfaceSupported(int deviceType, int interfaceType) {
#ifdef __sgi
  ALvalue vals[16];
  int devNum;

  if ((devNum = alQueryValues(AL_SYSTEM, deviceType, vals, 16, 0, 0)) > 0) {
    int i;
    for (i = 0; i < devNum; ++i) {
      ALpv quals[2];
      quals[0].param   = AL_TYPE;
      quals[0].value.i = interfaceType;
      if (alQueryValues(vals[i].i, AL_INTERFACE, 0, 0, quals, 1) > 0)
        return true;
    }
  }
#endif
  return false;
}

//------------------------------------------------------------------------------

bool setDefaultInput(TSoundInputDevice::Source type) {
#ifdef __sgi
  string label;

  switch (type) {
  case TSoundInputDevice::LineIn:
    label = "Line In";
    break;
  case TSoundInputDevice::DigitalIn:
    label = "AES Input";
    break;
  default:
    label = "Microphone";
  }

  int dev =
      alGetResourceByName(AL_SYSTEM, (char *)label.c_str(), AL_DEVICE_TYPE);
  if (!dev) return false;  // throw TException("Error to set input device");
  int itf;
  if (itf = alGetResourceByName(AL_SYSTEM, (char *)label.c_str(),
                                AL_INTERFACE_TYPE)) {
    ALpv p;

    p.param   = AL_INTERFACE;
    p.value.i = itf;
    if (alSetParams(dev, &p, 1) < 0 || p.sizeOut < 0)
      return false;  // throw TException("Error to set input device");
  }

  ALpv param;

  param.param   = AL_DEFAULT_INPUT;
  param.value.i = dev;
  if (alSetParams(AL_SYSTEM, &param, 1) < 0)
    return false;  // throw TException("Error to set input device");
#endif
  return true;
}

//------------------------------------------------------------------------------

bool setDefaultOutput() {
#ifdef __sgi
  ALpv pvbuf[1];

  if (!isInterfaceSupported(AL_DEFAULT_OUTPUT, AL_SPEAKER_IF_TYPE))
    return false;  // throw TException("Speakers are not supported");

  int dev = alGetResourceByName(AL_SYSTEM, (char *)"Headphone/Speaker",
                                AL_DEVICE_TYPE);
  if (!dev) return false;  // throw TException("invalid device speakers");

  pvbuf[0].param   = AL_DEFAULT_OUTPUT;
  pvbuf[0].value.i = dev;
  alSetParams(AL_SYSTEM, pvbuf, 1);

  // qui devo metterci gli altoparlanti e poi setto i valori per il default
  // output
  pvbuf[0].param   = AL_INTERFACE;
  pvbuf[0].value.i = AL_SPEAKER_IF_TYPE;

  if (alSetParams(AL_DEFAULT_OUTPUT, pvbuf, 1) < 0)
    return false;  // throw TException("Unable to set output device params");
#endif
  return true;
}

//------------------------------------------------------------------------------

// return the indexes of all input device of a particular type
list<int> getInputDevice(int deviceType) {
  ALvalue vals[16];
  ALpv quals[1];
  list<int> devList;
#ifdef __sgi
  int devNum;

  quals[0].param   = AL_TYPE;
  quals[0].value.i = deviceType;
  if ((devNum = alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, vals, 16, quals,
                              1)) > 0) {
    int i;
    for (i = 0; i < devNum; ++i) {
      int itf;
      ALvalue val[16];
      if ((itf = alQueryValues(i, AL_INTERFACE, val, 16, 0, 0)) > 0) {
        int j;
        for (j = 0; j < itf; ++j) devList.push_back(vals[j].i);
      }
    }
  }
#endif
  return devList;
}

//------------------------------------------------------------------------------

// return the indexes of all input device of a particular type and interface
list<int> getInputDevice(int deviceType, int itfType) {
  ALvalue vals[16];
  ALpv quals[1];
  list<int> devList;
  int devNum;
#ifdef __sgi

  quals[0].param   = AL_TYPE;
  quals[0].value.i = deviceType;
  if ((devNum = alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, vals, 16, quals,
                              1)) > 0) {
    int i;
    for (i = 0; i < devNum; ++i) {
      int itf;
      ALvalue val[16];
      quals[0].param   = AL_TYPE;
      quals[0].value.i = itfType;
      if ((itf = alQueryValues(i, AL_INTERFACE, val, 16, quals, 1)) > 0) {
        int j;
        for (j = 0; j < itf; ++j) devList.push_back(vals[j].i);
      }
    }
  }
#endif
  return devList;
}

//------------------------------------------------------------------------------

string getResourceLabel(int resourceID) {
#ifdef __sgi
  ALpv par[1];
  char l[32];

  par[0].param     = AL_LABEL;
  par[0].value.ptr = l;
  par[0].sizeIn    = 32;

  alGetParams(resourceID, par, 1);

  return string(l);
#else
  return "";
#endif
}

//------------------------------------------------------------------------------

// verify the samplerate of the select device is changed from another
// application
bool isChangeOutput(ULONG sampleRate) {
#ifdef __sgi
  ALpv par[2];
  char l[32];

  par[0].param     = AL_LABEL;
  par[0].value.ptr = l;
  par[0].sizeIn    = 32;
  par[1].param     = AL_RATE;

  alGetParams(AL_DEFAULT_OUTPUT, par, 2);
  if ((strcmp(l, "Analog Out") != 0) ||
      (alFixedToDouble(par[1].value.ll) != sampleRate))
    return true;
  else
    return false;
#else
  return true;
#endif
}
}
