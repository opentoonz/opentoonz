

#include "tsound_t.h"
#include "texception.h"
#include "tthread.h"
#include "tthreadmessage.h"

#include <errno.h>
#include <unistd.h>
#include <queue>
#include <set>

#include <QPointer>
#include <QByteArray>
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>
#include <QRecursiveMutex>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#include <QTimer>
#else
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#endif

using namespace std;

//==============================================================================

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using TQtAudioOutput = QAudioSink;
#else
using TQtAudioOutput = QAudioOutput;
#endif
using TQtAudioMutex = QRecursiveMutex;

class TSoundOutputDeviceImp: public std::enable_shared_from_this<TSoundOutputDeviceImp> {
private:
  TQtAudioMutex m_mutex;

  double m_volume;
  bool m_looping;

  qint64 m_bytesSent;
  qint64 m_bufferIndex;

  QByteArray m_buffer;
  QPointer<TQtAudioOutput> m_audioOutput;
  QIODevice *m_audioBuffer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QTimer *m_notifyTimer;
#endif

public:
  std::set<TSoundOutputDeviceListener *> m_listeners;

  TSoundOutputDeviceImp():
    m_volume(0.5),
    m_looping(false),
    m_bytesSent(0),
    m_bufferIndex(0),
    m_audioBuffer()
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    , m_notifyTimer(0)
#endif
  { }

  ~TSoundOutputDeviceImp() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_notifyTimer) {
      m_notifyTimer->stop();
      delete m_notifyTimer;
    }
#endif
  }

private:
  void reset() {
    if (m_audioOutput) {
      m_audioOutput->reset();
      m_audioBuffer = m_audioOutput->start();
      m_bytesSent = 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      if (m_notifyTimer) m_notifyTimer->start();
#endif
    }
  }

  void sendBuffer() {
    QMutexLocker lock(&m_mutex);

    if (!m_audioOutput) return;
    if (!m_audioBuffer) return;
    if (!m_buffer.size()) return;
    if (m_audioOutput->error() != QAudio::NoError
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        && m_audioOutput->error() != QAudio::UnderrunError
#endif
    ) {
      stop();
      std::cerr << "error " << m_audioOutput->error() << std::endl;
      return;
    }

    bool looping = isLooping();
    qint64 bytesRemain = m_audioOutput->bytesFree();
    while(bytesRemain > 0) {
      qint64 bytesCount = m_buffer.size() - m_bufferIndex;
      if (bytesCount <= 0) {
        if (!looping) break;
        m_bufferIndex = 0;
      }
      if (bytesCount > bytesRemain) bytesCount = bytesRemain;

      m_audioBuffer->write(m_buffer.data() + m_bufferIndex, bytesCount);

      bytesRemain -= bytesCount;
      m_bufferIndex += bytesCount;
      m_bytesSent += bytesCount;
    }
  }

public:
  double getVolume() {
    QMutexLocker lock(&m_mutex);
    return m_volume;
  }

  bool isLooping() {
    QMutexLocker lock(&m_mutex);
    return m_looping;
  }

  void prepareVolume(double x) {
    QMutexLocker lock(&m_mutex);
    m_volume = x;
  }

  bool isPlaying() {
    QMutexLocker lock(&m_mutex);
    return m_audioOutput
        && m_buffer.size()
        && ( isLooping()
          || m_bufferIndex < m_buffer.size()
          /*|| m_audioOutput->state() == QAudio::ActiveState*/ );
  }

  void setVolume(double x) {
    QMutexLocker lock(&m_mutex);
    m_volume = x;
    if (m_audioOutput) m_audioOutput->setVolume(m_volume);
  }

  void setLooping(bool x) {
    QMutexLocker lock(&m_mutex);
    if (m_looping != x) {
      m_looping = x;
      if (m_audioOutput) {
        /* audio buffer too small, so optimization not uses
        qint64 bufferSize = m_buffer.size();
        if (!m_looping && bufferSize > 0) {
          qint64 processedBytes =
            m_audioOutput->format().bytesForDuration(
              m_audioOutput->processedUSecs() );
          qint64 extraBytesSend = m_bytesSent - processedBytes;
          if (extraBytesSend > m_bufferIndex) {
            // remove extra loops from audio buffer
            m_bufferIndex -= extraBytesSend % bufferSize;
            if (m_bufferIndex < 0) m_bufferIndex += bufferSize;
            reset();
          }
        }
        */
        sendBuffer();
      }
    }
  }

  void stop() {
    QMutexLocker lock(&m_mutex);
    //reset(); audio buffer too small, so optimization not uses
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_notifyTimer) m_notifyTimer->stop();
#endif
    m_buffer.clear();
    m_bufferIndex = 0;
  }

  void play(const TSoundTrackP &st, TINT32 s0, TINT32 s1, bool loop, bool scrubbing) {
    QMutexLocker lock(&m_mutex);

    QAudioFormat format;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    format.setSampleRate(st->getSampleRate());
    format.setChannelCount(st->getChannelCount());
    switch (st->getSampleType()) {
    case TSound::INT:
      format.setSampleFormat(st->getBitPerSample() <= 16
                                 ? QAudioFormat::Int16
                                 : QAudioFormat::Int32);
      break;
    case TSound::UINT:
      format.setSampleFormat(st->getBitPerSample() <= 8
                                 ? QAudioFormat::UInt8
                                 : QAudioFormat::Int16);
      break;
    case TSound::FLOAT:
      format.setSampleFormat(QAudioFormat::Float);
      break;
    }

    QAudioDevice info(QMediaDevices::defaultAudioOutput());
    if (!info.isNull() && !info.isFormatSupported(format))
      format = info.preferredFormat();
#else
    format.setSampleSize(st->getBitPerSample());
    format.setCodec("audio/pcm");
    format.setChannelCount(st->getChannelCount());
    format.setByteOrder(QAudioFormat::LittleEndian);
    switch (st->getSampleType()) {
    case TSound::INT:
      format.setSampleType(QAudioFormat::SignedInt);
      break;
    case TSound::UINT:
      format.setSampleType(QAudioFormat::UnSignedInt);
      break;
    case TSound::FLOAT:
      format.setSampleType(QAudioFormat::Float);
      break;
    }
    format.setSampleRate(st->getSampleRate());

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported((format)))
      format = info.nearestFormat(format);
#endif

    qint64 totalPacketCount = s1 - s0;
    qint64 fileByteCount    = (s1 - s0)*st->getSampleSize();
    m_buffer.resize(fileByteCount);
    memcpy(m_buffer.data(), st->getRawData() + s0*st->getSampleSize(), fileByteCount);
    m_bufferIndex = 0;

    m_looping = loop;
    if (!m_audioOutput || m_audioOutput->format() != format) {
      if (m_audioOutput) m_audioOutput->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      if (info.isNull())
        m_audioOutput = new TQtAudioOutput(format);
      else
        m_audioOutput = new TQtAudioOutput(info, format);
#else
      m_audioOutput = new QAudioOutput(format);
#endif
      m_audioOutput->setVolume(m_volume);

      // audio buffer size
      qint64 audioBufferSize = format.bytesForDuration(100000);
      m_audioOutput->setBufferSize(audioBufferSize);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      if (!m_notifyTimer) {
        m_notifyTimer = new QTimer;
        m_notifyTimer->setInterval(50);
        QObject::connect(m_notifyTimer, &QTimer::timeout,
                         [this]() { sendBuffer(); });
      }
#else
      m_audioOutput->setNotifyInterval(50);
      QObject::connect(m_audioOutput.data(), &QAudioOutput::notify, [=](){ sendBuffer(); });
#endif

      reset();
    }/* audio buffer too small, so optimization not uses
    else {
      // if less than 0.1 sec of data in audio buffer,
      // then just sent next portion of data
      // else reset audio buffer before
      qint64 sentUSecs = format.durationForBytes(m_bytesSent);
      qint64 processedUSecs = m_audioOutput->processedUSecs();
      if (sentUSecs - processedUSecs > 100000ll)
        reset();
    }
    */

    sendBuffer();
  }
};

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
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::close() {
  stop();
  return true;
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

  m_imp->play(st, s0, s1, loop, scrubbing);
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
  m_imp->stop();
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
  return m_imp->getVolume();
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::setVolume(double volume) {
  m_imp->setVolume(volume);
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::supportsVolume() { return true; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const { return m_imp->isPlaying(); }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() { return m_imp->isLooping(); }

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) { m_imp->setLooping(loop); }

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
