#pragma once

#include "tsound.h"
#include "tsop.h"

#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QBuffer>
#include <limits>
#include <memory>

// A GUI-thread-only, finite audition stream. Reset before replacing its data:
// QAudioOutput must never retain a stream backed by a modified/deleted buffer.
// Continuous playback deliberately does not use this player.
class AudioScrubPlayer final {
  QBuffer m_buffer;
  QAudioDeviceInfo m_device;
  std::unique_ptr<QAudioOutput> m_output;

public:
  ~AudioScrubPlayer() { stop(); }

  void stop() {
    if (m_output) m_output->reset();
    m_buffer.close();
    m_buffer.setData(QByteArray());
  }

  void play(const TSoundTrackP &source) {
    stop();
    if (!source || source->getSampleCount() <= 0) return;

    const QAudioDeviceInfo device = QAudioDeviceInfo::defaultOutputDevice();
    if (device.isNull())
      throw TSoundDeviceException(TSoundDeviceException::NoDevice,
                                  "No audio output device for scrubbing");

    // Use a conservative audition format. This changes only the temporary
    // snippet, never the scene's stored audio or export format.
    QAudioFormat format;
    format.setCodec("audio/pcm");
    format.setSampleRate(source->getSampleRate());
    format.setChannelCount(source->getChannelCount());
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    if (!device.isFormatSupported(format)) format = device.nearestFormat(format);
    if (!format.isValid() || !device.isFormatSupported(format) ||
        format.codec() != "audio/pcm" || format.sampleSize() != 16 ||
        format.sampleType() != QAudioFormat::SignedInt ||
        format.byteOrder() != QAudioFormat::LittleEndian ||
        format.channelCount() < 1 || format.channelCount() > 2)
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Audio scrubbing requires 16-bit PCM output");

    const TSoundTrackFormat target(format.sampleRate(), 16,
                                    format.channelCount(), TSound::INT);
    TSoundTrackP snippet = TSop::convert(source, target);
    if (!snippet || !snippet->getRawData()) return;
    const qint64 bytes = qint64(snippet->getSampleCount()) *
                         snippet->getSampleSize();
    if (bytes <= 0 || bytes > std::numeric_limits<int>::max()) return;

    if (!m_output || m_device != device || m_output->format() != format) {
      m_output.reset(new QAudioOutput(device, format));
      m_device = device;
      // Preserve the existing TXsheet playback gain on each platform.
#ifndef _WIN32
      m_output->setVolume(0.5);
#endif
      // Qt may enlarge this request. Replacement/reset still prevents a
      // backlog, independently of the actual hardware buffer size.
      m_output->setBufferSize(format.bytesForDuration(20000));
    }
    m_buffer.setData(QByteArray(
        reinterpret_cast<const char *>(snippet->getRawData()), int(bytes)));
    m_buffer.open(QIODevice::ReadOnly);
    m_output->start(&m_buffer);
    if (m_output->error() != QAudio::NoError) stop();
  }
};
