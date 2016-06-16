
#include "tsound_t.h"
#include "texception.h"
#include "tthread.h"
#include "tthreadmessage.h"

#include <errno.h>
#include <unistd.h>
#include <queue>
#include <set>

#include <SDL2/SDL.h>
using namespace std;

//==============================================================================
namespace {
TThread::Mutex MutexOut;
}

namespace {

struct MyData {
  char *entireFileBuffer;

  int totalPacketCount;
  int fileByteCount;
  // UInt32 maxPacketSize;
  // UInt64 packetOffset;
  int byteOffset;
  bool m_doNotify;

  void *sourceBuffer;
  // AudioConverterRef converter;
  TSoundOutputDeviceImp *imp;
  bool isLooping;
  MyData()
      : entireFileBuffer(0)
      , totalPacketCount(0)
      , fileByteCount(0)
      , /*maxPacketSize(0), packetOffset(0),*/ byteOffset(0)
      , sourceBuffer(0)
      , isLooping(false)
      , imp(0)
      , m_doNotify(true) {}
};
}

class TSoundOutputDeviceImp {
public:
  bool m_isPlaying;
  bool m_looped;
  TSoundTrackFormat m_currentFormat;
  std::set<int> m_supportedRate;
  bool m_opened;
  struct MyData *m_data;
  int m_volume;

  TSoundOutputDeviceImp()
      : m_isPlaying(false)
      , m_looped(false)
      , m_supportedRate()
      , m_opened(false)
      , m_data(NULL)
      , m_volume(SDL_MIX_MAXVOLUME){};

  std::set<TSoundOutputDeviceListener *> m_listeners;

  ~TSoundOutputDeviceImp(){};

  bool doOpenDevice(const TSoundTrackFormat &format);
  bool doStopDevice();
  void play(const TSoundTrackP &st, TINT32 s0, TINT32 s1, bool loop,
            bool scrubbing);
};

//-----------------------------------------------------------------------------
namespace {

class PlayCompletedMsg : public TThread::Message {
  std::set<TSoundOutputDeviceListener *> m_listeners;
  MyData *m_data;

public:
  PlayCompletedMsg(MyData *data) : m_data(data) {}

  TThread::Message *clone() const { return new PlayCompletedMsg(*this); }

  void onDeliver() {
    if (m_data->imp) {
      if (m_data->m_doNotify == false) return;
      m_data->m_doNotify = false;
      if (m_data->imp->m_isPlaying) m_data->imp->doStopDevice();
      std::set<TSoundOutputDeviceListener *>::iterator it =
          m_data->imp->m_listeners.begin();
      for (; it != m_data->imp->m_listeners.end(); ++it)
        (*it)->onPlayCompleted();
    }
  }
};
}

#define checkStatus(err)                                                       \
  if (err) {                                                                   \
    printf("Error: 0x%x ->  %s: %d\n", (int)err, __FILE__, __LINE__);          \
    fflush(stdout);                                                            \
  }

extern "C" {

void sdl_fill_audio(void *udata, Uint8 *stream, int len) {
  TSoundOutputDeviceImp *_this = (TSoundOutputDeviceImp *)udata;
  MyData *myData               = _this->m_data;

  /* Only play if we have data left */
  if (myData == NULL) return;
  {
    // TThread::ScopedLock sl(MutexOut);
    if (myData->imp->m_isPlaying == false) return;
  }

  int audio_len = myData->fileByteCount - myData->byteOffset;
  if (audio_len <= 0) {
    delete[] myData->entireFileBuffer;
    myData->entireFileBuffer = 0;
#if 0
		{
			TThread::ScopedLock sl(MutexOut);
			*(myData->isPlaying) = false;   //questo lo faccio nel main thread
		}
#endif
    PlayCompletedMsg(myData).send();
    return;
  }

  /* Mix as much data as possible */
  len = (len > audio_len ? audio_len : len);
  SDL_MixAudio(stream, (Uint8 *)myData->entireFileBuffer + myData->byteOffset,
               len, _this->m_volume);
  myData->byteOffset += len;
}

}  // extern "C"

bool TSoundOutputDeviceImp::doOpenDevice(const TSoundTrackFormat &format) {
  SDL_AudioSpec wanted;

  static bool first = true;  // TODO: should be shared with InputDevice
  if (first) {
    SDL_Init(SDL_INIT_AUDIO);
    first = false;
  }

  if (m_opened) {
    SDL_CloseAudio();
    // we'll just reopen right away
  }

  wanted.freq = format.m_sampleRate;
  switch (format.m_bitPerSample) {
  case 8:
    wanted.format = AUDIO_S8;
    break;
  case 16:
    wanted.format = AUDIO_S16;
    break;
  default:
    throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice,
                                "invalid bits per sample");
    return false;
  }
  wanted.channels = format.m_channelCount; /* 1 = mono, 2 = stereo */
  wanted.samples  = 1024; /* Good low-latency value for callback */
  wanted.callback = sdl_fill_audio;
  wanted.userdata = this;

  /* Open the audio device, forcing the desired format */
  if (SDL_OpenAudio(&wanted, NULL) < 0) {
    std::string msg("Couldn't open audio: ");
    msg += SDL_GetError();
    throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice, msg);
    return false;
  }

  m_opened = true;
  return m_opened;
}

//==============================================================================

TSoundOutputDevice::TSoundOutputDevice() : m_imp(new TSoundOutputDeviceImp) {
#if 0
	try {
		supportsVolume();
	} catch (TSoundDeviceException &e) {
		throw TSoundDeviceException(e.getType(), e.getMessage());
	}
#endif
}

//------------------------------------------------------------------------------

TSoundOutputDevice::~TSoundOutputDevice() {
  stop();
  close();
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::installed() { return true; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::open(const TSoundTrackP &st) {
  if (!m_imp->doOpenDevice(st->getFormat()))
    throw TSoundDeviceException(
        TSoundDeviceException::UnableOpenDevice,
        "Problem to open the output device setting some params");
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::close() {
  stop();
  m_imp->m_opened = false;
  return true;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1,
                              bool loop, bool scrubbing) {
  // TThread::ScopedLock sl(MutexOut);
  int lastSample = st->getSampleCount() - 1;
  notLessThan(0, s0);
  notLessThan(0, s1);

  notMoreThan(lastSample, s0);
  notMoreThan(lastSample, s1);

  if (s0 > s1) {
#ifdef DEBUG
    cout << "s0 > s1; reorder" << endl;
#endif
    swap(s0, s1);
  }

  if (isPlaying()) {
#ifdef DEBUG
    cout << "is playing, stop it!" << endl;
#endif
    stop();
  }
  m_imp->play(st, s0, s1, loop, scrubbing);
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1,
                                 bool loop, bool scrubbing) {
  if (!doOpenDevice(st->getFormat())) return;

  MyData *myData = new MyData();

  myData->imp              = this;
  myData->totalPacketCount = s1 - s0;
  myData->fileByteCount    = (s1 - s0) * st->getSampleSize();
  myData->entireFileBuffer = new char[myData->fileByteCount];

#if defined(i386)
  // XXX: let's see if that's needed for SDL
  if (st->getBitPerSample() == 16) {
    int i;
    USHORT *dst = (USHORT *)(myData->entireFileBuffer);
    USHORT *src = (USHORT *)(st->getRawData() + s0 * st->getSampleSize());

    for (i = 0; i < myData->fileByteCount / 2; i++) *dst++ = swapUshort(*src++);
  } else
    memcpy(myData->entireFileBuffer,
           st->getRawData() + s0 * st->getSampleSize(), myData->fileByteCount);
#else
  memcpy(myData->entireFileBuffer, st->getRawData() + s0 * st->getSampleSize(),
         myData->fileByteCount);
#endif

  //	myData->maxPacketSize = fileASBD.mFramesPerPacket *
  //fileASBD.mBytesPerFrame;
  {
    // TThread::ScopedLock sl(MutexOut);
    m_isPlaying = true;
  }
  myData->isLooping = loop;

  // cout << "total packet count = " << myData->totalPacketCount <<endl;
  // cout << "filebytecount " << myData->fileByteCount << endl;

  m_data = myData;
  SDL_PauseAudio(0);
}

//------------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doStopDevice() {
  SDL_PauseAudio(1);
  m_isPlaying = false;
  delete m_data;
  m_data = NULL;
  SDL_CloseAudio();
  m_opened = false;
  return true;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
  // TThread::ScopedLock sl(MutexOut);
  if (m_imp->m_opened == false) return;

  // TThread::ScopedLock sl(MutexOut);
  m_imp->doStopDevice();
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::attach(TSoundOutputDeviceListener *listener) {
  m_imp->m_listeners.insert(listener);
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::detach(TSoundOutputDeviceListener *listener) {
  m_imp->m_listeners.erase(listener);
}

#if 0
//------------------------------------------------------------------------------

double TSoundOutputDevice::getVolume()
{
	if (!m_imp->m_opened)
		m_imp->doOpenDevice();

	double vol = m_volume / SDL_MIX_MAXVOLUME;

	return (vol < 0. ? 0. : vol);
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::setVolume(double volume)
{
	Float32 vol = volume;

	m_imp->m_volume = (int)(volume * SDL_MIX_MAXVOLUME);
	return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::supportsVolume()
{
	return true;
}

#endif
//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const {
  // TThread::ScopedLock sl(MutexOut);
  // TODO: handle actually queuing items?
  return m_imp->m_isPlaying;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isAllQueuedItemsPlayed() {
  // TThread::ScopedLock sl(MutexOut);
  return m_imp->m_data == NULL;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() {
  // TThread::ScopedLock sl(MutexOut);
  return m_imp->m_looped;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) {
  // TThread::ScopedLock sl(MutexOut);
  m_imp->m_looped = loop;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                         int channelCount,
                                                         int bitPerSample) {
  if (bitPerSample > 16) bitPerSample = 16;
  // not sure SDL supports more than 2 channels
  if (channelCount > 2) channelCount = 2;
  TSoundTrackFormat fmt(sampleRate, bitPerSample, channelCount, true);
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
#if 0
	try {
#endif
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
#if 0
	}
	catch (TSoundDeviceException &e) {
		throw TSoundDeviceException( e.getType(), e.getMessage());
	}
#endif
}

//==============================================================================
//==============================================================================
//                REGISTRAZIONE
//==============================================================================
//==============================================================================

class TSoundInputDeviceImp {
public:
  // ALport m_port;
  bool m_stopped;
  bool m_isRecording;
  bool m_oneShotRecording;

  long m_recordedSampleCount;

  TSoundTrackFormat m_currentFormat;
  TSoundTrackP m_st;
  std::set<int> m_supportedRate;

  TThread::Executor m_executor;

  TSoundInputDeviceImp()
      : m_stopped(false)
      , m_isRecording(false)
      //   , m_port(NULL)
      , m_oneShotRecording(false)
      , m_recordedSampleCount(0)
      , m_st(0)
      , m_supportedRate(){};

  ~TSoundInputDeviceImp(){};

  bool doOpenDevice(const TSoundTrackFormat &format,
                    TSoundInputDevice::Source devType);
};

bool TSoundInputDeviceImp::doOpenDevice(const TSoundTrackFormat &format,
                                        TSoundInputDevice::Source devType) {
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

void RecordTask::run() {}

//==============================================================================

TSoundInputDevice::TSoundInputDevice() : m_imp(new TSoundInputDeviceImp) {}

//------------------------------------------------------------------------------

TSoundInputDevice::~TSoundInputDevice() {
#if 0
	if(m_imp->m_port)
		alClosePort(m_imp->m_port);
	delete m_imp;
#endif
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::installed() {
#if 0
	if (alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, 0, 0, 0, 0) <= 0)
		return false;
#endif
  return true;
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackFormat &format,
                               TSoundInputDevice::Source type) {}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackP &st,
                               TSoundInputDevice::Source type) {}

//------------------------------------------------------------------------------

TSoundTrackP TSoundInputDevice::stop() {
  TSoundTrackP st;
  return st;
}

//------------------------------------------------------------------------------

double TSoundInputDevice::getVolume() { return 0.0; }

//------------------------------------------------------------------------------

bool TSoundInputDevice::setVolume(double volume) { return true; }

//------------------------------------------------------------------------------

bool TSoundInputDevice::supportsVolume() { return true; }

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                        int channelCount,
                                                        int bitPerSample) {
  TSoundTrackFormat fmt;
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
#if 0
	try {
#endif
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
#if 0
	}
	catch (TSoundDeviceException &e) {
		throw TSoundDeviceException( e.getType(), e.getMessage());
	}
#endif
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::isRecording() { return m_imp->m_isRecording; }
