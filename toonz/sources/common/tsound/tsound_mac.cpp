

#include "tsound_t.h"
#include "texception.h"
#include "tthread.h"
#include "tthreadmessage.h"

#include <errno.h>
#include <unistd.h>
#include <queue>
#include <set>

#include <QByteArray>
#include <QAudioFormat>
#include <QBuffer>
#include <QAudioOutput>

using namespace std;

//==============================================================================
namespace {
TThread::Mutex MutexOut;
}

class TSoundOutputDeviceImp
    : public std::enable_shared_from_this<TSoundOutputDeviceImp> {
public:
  bool m_isPlaying;
  bool m_looped;
  TSoundTrackFormat m_currentFormat;
  std::set<int> m_supportedRate;
  bool m_opened;
  double m_volume = 0.5;

  QAudioOutput *m_audioOutput;
  QBuffer *m_buffer;

  TSoundOutputDeviceImp()
      : m_isPlaying(false)
      , m_looped(false)
      , m_supportedRate()
      , m_opened(false){};

  std::set<TSoundOutputDeviceListener *> m_listeners;

  ~TSoundOutputDeviceImp(){};

  bool doOpenDevice();
  bool doSetStreamFormat(const TSoundTrackFormat &format);
  bool doStopDevice();
  void play(const TSoundTrackP &st, TINT32 s0, TINT32 s1, bool loop,
            bool scrubbing);
  void prepareVolume(double volume);
};

//-----------------------------------------------------------------------------
namespace {

struct MyData {
  char *entireFileBuffer;

  quint64 totalPacketCount;
  quint64 fileByteCount;
  quint32 maxPacketSize;
  quint64 packetOffset;
  quint64 byteOffset;
  bool m_doNotify;
  void *sourceBuffer;
  // AudioConverterRef converter;
  std::shared_ptr<TSoundOutputDeviceImp> imp;
  bool isLooping;
  MyData()
      : entireFileBuffer(0)
      , totalPacketCount(0)
      , fileByteCount(0)
      , maxPacketSize(0)
      , packetOffset(0)
      , byteOffset(0)
      , sourceBuffer(0)
      , isLooping(false)
      , m_doNotify(true) {}
};

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

bool TSoundOutputDeviceImp::doOpenDevice() {
  m_opened      = false;
  m_audioOutput = NULL;
  m_opened      = true;
  return m_opened;
}

bool TSoundOutputDeviceImp::doSetStreamFormat(const TSoundTrackFormat &format) {
  if (!m_opened) doOpenDevice();
  if (!m_opened) return false;
  m_opened = true;
  return true;
}

//==============================================================================

TSoundOutputDevice::TSoundOutputDevice() : m_imp(new TSoundOutputDeviceImp) {
  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
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
  if (!m_imp->doOpenDevice())
    throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice,
                                "Problem to open the output device");
  if (!m_imp->doSetStreamFormat(st->getFormat()))
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

void TSoundOutputDeviceImp::prepareVolume(double volume) {
    m_volume = volume;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::prepareVolume(double volume) {
    m_imp->prepareVolume(volume);
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1,
                              bool loop, bool scrubbing) {
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
  if (!doSetStreamFormat(st->getFormat())) return;

  MyData *myData = new MyData();

  myData->imp              = shared_from_this();
  myData->totalPacketCount = s1 - s0;
  myData->fileByteCount    = (s1 - s0) * st->getSampleSize();
  myData->entireFileBuffer = new char[myData->fileByteCount];

  memcpy(myData->entireFileBuffer, st->getRawData() + s0 * st->getSampleSize(),
         myData->fileByteCount);

  m_isPlaying       = true;
  myData->isLooping = loop;

  QAudioFormat format;
  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());

  format.setSampleSize(st->getBitPerSample());
  format.setCodec("audio/pcm");
  format.setChannelCount(st->getChannelCount());
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(st->getFormat().m_signedSample
                           ? QAudioFormat::SignedInt
                           : QAudioFormat::UnSignedInt);
  format.setSampleRate(st->getSampleRate());
  QList<QAudioFormat::Endian> sbos        = info.supportedByteOrders();
  QList<int> sccs                         = info.supportedChannelCounts();
  QList<int> ssrs                         = info.supportedSampleRates();
  QList<QAudioFormat::SampleType> sstypes = info.supportedSampleTypes();
  QList<int> ssss                         = info.supportedSampleSizes();
  QStringList supCodes                    = info.supportedCodecs();
  if (!info.isFormatSupported((format))) {
    format                                 = info.nearestFormat(format);
    int newChannels                        = format.channelCount();
    int newBitsPerSample                   = format.sampleSize();
    int newSampleRate                      = format.sampleRate();
    QAudioFormat::SampleType newSampleType = format.sampleType();
    QAudioFormat::Endian newBo             = format.byteOrder();
  }
  int test = st->getSampleCount() / st->getSampleRate();
  QByteArray *data =
      new QByteArray(myData->entireFileBuffer, myData->fileByteCount);
  QBuffer *newBuffer = new QBuffer;
  newBuffer->setBuffer(data);
  newBuffer->open(QIODevice::ReadOnly);
  newBuffer->seek(0);
  if (m_audioOutput == NULL) {
    m_audioOutput = new QAudioOutput(format, NULL);
  }
  m_audioOutput->start(newBuffer);
  m_audioOutput->setVolume(m_volume);
}

//------------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doStopDevice() {
  m_isPlaying = false;
  m_audioOutput->stop();

  return true;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
  if (m_imp->m_opened == false) return;

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

//------------------------------------------------------------------------------

double TSoundOutputDevice::getVolume() {
  if (m_imp->m_audioOutput != NULL)
  return m_imp->m_audioOutput->volume();
  else return m_imp->m_volume;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::setVolume(double volume) {
  m_imp->m_volume = volume;
  m_imp->m_audioOutput->setVolume(volume);
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::supportsVolume() { return true; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const { return m_imp->m_isPlaying; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() { return m_imp->m_looped; }

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) { m_imp->m_looped = loop; }

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                         int channelCount,
                                                         int bitPerSample) {
  TSoundTrackFormat fmt(sampleRate, bitPerSample, channelCount, true);
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
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

TSoundInputDevice::~TSoundInputDevice() {}

//------------------------------------------------------------------------------

bool TSoundInputDevice::installed() {
  /*
  if (alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, 0, 0, 0, 0) <=0)
return false;
*/
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
  /*
try {
*/
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
  /*}

catch (TSoundDeviceException &e) {
throw TSoundDeviceException( e.getType(), e.getMessage());
}
*/
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::isRecording() { return m_imp->m_isRecording; }
