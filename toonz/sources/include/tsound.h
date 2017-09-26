#pragma once

#ifndef TSOUND_INCLUDED
#define TSOUND_INCLUDED

#include <memory>

#include "tsmartpointer.h"
#include "texception.h"
#include "tthreadmessage.h"

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

namespace TSound {
typedef UCHAR Channel;

const int MONO  = 0;
const int LEFT  = 0;
const int RIGHT = LEFT + 1;
}

//=========================================================

// forward declarations

class TSoundTrack;
class TSoundTransform;

#ifdef _WIN32
template class DVAPI TSmartPointerT<TSoundTrack>;
#endif

typedef TSmartPointerT<TSoundTrack> TSoundTrackP;

//==============================================================================
/*!
  The class TSoundTrackFormat contains the main features of a TSoundTrack as:
  sample rate, bit per sample, number of channels and signed or unsigned sample
*/
class DVAPI TSoundTrackFormat {
public:
  TUINT32 m_sampleRate;  // frequenza di campionamento
  int m_bitPerSample;    // numero di bit per campione
  int m_channelCount;    // numero di canali
  bool m_signedSample;

  TSoundTrackFormat(TUINT32 sampleRate = 0, int bitPerSample = 0,
                    int channelCount = 0, bool signedSample = true)
      : m_sampleRate(sampleRate)
      , m_bitPerSample(bitPerSample)
      , m_channelCount(channelCount)
      , m_signedSample(signedSample) {}

  ~TSoundTrackFormat() {}

  bool operator==(const TSoundTrackFormat &rhs);

  bool operator!=(const TSoundTrackFormat &rhs);
};

//==============================================================================
//! \include sound_ex.cpp

/*!
  The class TSoundTrack contains all features about a sound track
  and gives all methods to access to these informations
*/
class DVAPI TSoundTrack : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  TUINT32 m_sampleRate;  // frequenza di campionamento
  int m_sampleSize;      // numero di byte per campione
  int m_bitPerSample;    // numero di bit per campione
  TINT32 m_sampleCount;  // numero di campioni
  int m_channelCount;    // numero di canali

  TSoundTrack *m_parent;  // nel caso di sotto-traccie

  UCHAR *m_buffer;
  bool m_bufferOwner;

  TSoundTrack();

  TSoundTrack(TUINT32 sampleRate, int bitPerSample, int channelCount,
              int sampleSize, TINT32 sampleCount, bool isSampleSigned);

  TSoundTrack(TUINT32 sampleRate, int bitPerSample, int channelCount,
              int sampleSize, TINT32 sampleCount, UCHAR *buffer,
              TSoundTrack *parent);

public:
  /*!
Create a new soundtrack according to the sample rate, bits per sample, number
of channels and number of samples specified as inputs.
signedSample must be true for tracks whose samples are signed
*/
  static TSoundTrackP create(TUINT32 sampleRate, int bitPerSample,
                             int channelCount, TINT32 sampleCount,
                             bool signedSample = true);

  static TSoundTrackP create(TUINT32 sampleRate, int bitPerSample,
                             int channelCount, TINT32 sampleCount, void *buffer,
                             bool signedSample = true);

  /*!
Create a new soundtrack according to the format and number of samples
specified as inputs
*/
  static TSoundTrackP create(const TSoundTrackFormat &format,
                             TINT32 sampleCount);

  static TSoundTrackP create(const TSoundTrackFormat &format,
                             TINT32 sampleCount, void *buffer);

  ~TSoundTrack();

  //! Converts from seconds to samples according to the soundtrack sample rate
  TINT32 secondsToSamples(double sec) const;

  //! Converts from samples to seconds according to the soundtrack sample rate
  double samplesToSeconds(TINT32 smp) const;

  TUINT32 getSampleRate() const { return m_sampleRate; }
  int getSampleSize() const { return m_sampleSize; }
  int getChannelCount() const { return m_channelCount; }
  int getBitPerSample() const { return m_bitPerSample; }
  TINT32 getSampleCount() const { return m_sampleCount; }

  //! Returns the duration of the soundtrack in seconds.
  double getDuration() const;

  //! Returns true if the samples of the soundtrack are signed, false otherwise
  virtual bool isSampleSigned() const = 0;

  //! Returns the soundtrack format
  TSoundTrackFormat getFormat() const;

  //! Returns a pointer to the samples buffer.
  const UCHAR *getRawData() const { return m_buffer; };

  void setSampleRate(TUINT32 sampleRate) { m_sampleRate = sampleRate; }

  //! Clones the soundtrack
  virtual TSoundTrackP clone() const = 0;

  //! Returns a soundtrack that contains just the cloned channel
  virtual TSoundTrackP clone(TSound::Channel chan) const = 0;

  //! Returns a subtrack for the range [s0, s1], s0 and s1 are samples
  virtual TSoundTrackP extract(TINT32 s0, TINT32 s1) = 0;

  //! Returns a subtrack for the range [t0, t1], t0 and t1 are seconds
  TSoundTrackP extract(double t0, double t1);

  /*!
Copies the all samples of the source soundtrack
to the object, starting from dst_s0.
dst_s0 is expressed in samples.
*/
  virtual void copy(const TSoundTrackP &src, TINT32 dst_s0) = 0;

  /*!
Copies the all samples of the source soundtrack
to the object, starting from dst_t0.
dst_t0 is expressed in seconds.
*/
  void copy(const TSoundTrackP &src, double dst_t0);

  //! Blanks the soundtrack in the given range
  virtual void blank(TINT32 s0, TINT32 s1) = 0;

  /*!
Blanks the soundtrack in the given range. Range is expressed in seconds.
*/
  void blank(double t0, double t1);

  /*!
Returns a new soundtrack obtained applying the given sound tranform
to the soundtrack
*/
  virtual TSoundTrackP apply(TSoundTransform *) = 0;

  //! Returns the soundtrack pressure value for the given sample and channel:
  //! range [-1,1]
  virtual double getPressure(TINT32 sample, TSound::Channel chan) const = 0;

  //! Returns the soundtrack pressure value for the given time and channel
  double getPressure(double second, TSound::Channel chan) const;

  //! Returns the soundtrack pressure max and min values in the given sample
  //! range and channel
  virtual void getMinMaxPressure(TINT32 s0, TINT32 s1, TSound::Channel chan,
                                 double &min, double &max) const = 0;

  /*!
Returns the soundtrack pressure min and max values in the given range and
channel.
Range in seconds
*/
  void getMinMaxPressure(double t0, double t1, TSound::Channel chan,
                         double &min, double &max) const;

  //! Returns the soundtrack pressure min and max values for the given channel
  void getMinMaxPressure(TSound::Channel chan, double &min, double &max) const;

  //! Returns the soundtrack pressure max value in the given sample range and
  //! channel
  virtual double getMaxPressure(TINT32 s0, TINT32 s1,
                                TSound::Channel chan) const = 0;

  /*!
Returns the soundtrack pressure min value in the given range and channel.
Range in seconds
*/
  double getMaxPressure(double t0, double t1, TSound::Channel chan) const;

  //! Returns the soundtrack pressure max value for the given channel
  double getMaxPressure(TSound::Channel chan) const;

  //! Returns the soundtrack pressure max value in the given sample range and
  //! channel
  virtual double getMinPressure(TINT32 s0, TINT32 s1,
                                TSound::Channel chan) const = 0;

  /*!
Returns the soundtrack pressure mix value in the given sample range and channel
Range in seconds
*/
  double getMinPressure(double t0, double t1, TSound::Channel chan) const;

  //! Returns the soundtrack pressure min value for the given channel
  double getMinPressure(TSound::Channel chan) const;
};

//==============================================================================

class TSoundDeviceException final : public TException {
public:
  enum Type {
    FailedInit,  // fallimento del costruttore
    UnableOpenDevice,
    UnableCloseDevice,
    UnablePrepare,      // non puo' preparare i blocchi per play o rec
    UnsupportedFormat,  // formato non supportato
    UnableSetDevice,    // non puo' impostare il device richiesto
    UnableVolume,       // non puo' leggere o scrivere il valore del volume
    NoMixer,            // assenza del dispositivo per regolare il volume
    Busy                // indica che il dispositivo gia' sta facendo
                        // un play od una registrazione
  };

  TSoundDeviceException(Type type, const std::string &msg)
      : TException(msg), m_type(type) {}
  TSoundDeviceException(Type type, const std::wstring &msg)
      : TException(msg), m_type(type) {}

  Type getType() { return m_type; }

private:
  Type m_type;
};

//==============================================================================

// forward declaration
class TSoundInputDeviceImp;

//! \include sndInDev_ex.cpp
/*!
  The class TSoundInputDevice permits the recording of a new sound track
*/
class DVAPI TSoundInputDevice {
  std::shared_ptr<TSoundInputDeviceImp> m_imp;

public:
  enum Source { Mic = 0, LineIn, DigitalIn, CdAudio };

  TSoundInputDevice();
  ~TSoundInputDevice();

  /*!
Returns true if on the machine there is an audio card installed correctly
*/
  static bool installed();

  //! Returns the best format supported near the given parameters
  TSoundTrackFormat getPreferredFormat(TUINT32 sampleRate, int channelCount,
                                       int bitPerSample);

  //! Returns the best format supported near the given format
  TSoundTrackFormat getPreferredFormat(const TSoundTrackFormat &);

  //! Returns true if is possible to change volume setting on current input
  //! interface
  bool supportsVolume();

  //! Starts the recording of a soundtrack with the given format from the given
  //! source.
  void record(const TSoundTrackFormat &format, Source devtype = Mic);

  /*!
Starts the recording over the soundtrack st from the given source.
If not stopped before, recording ends when the whole soundtrack has been
overwritten.
*/
  void record(const TSoundTrackP &st, Source src = Mic);

  //! Stops the recording and returns the result of recording.
  TSoundTrackP stop();

  //! Return the current value of the volume
  double getVolume();

  //! Set the value of the volume
  bool setVolume(double value);

  //! Returns true if and only if the device is recording
  bool isRecording();
};

//=============================================================================
/*!
  The class TSoundOutputDeviceListener permits to notify to other object
  if a playback is completed. Use it as base class that needs to know if
  a playback is ended
*/

class DVAPI TSoundOutputDeviceListener {
public:
  virtual ~TSoundOutputDeviceListener(){};

  virtual void onPlayCompleted() = 0;
};

//==============================================================================

class TSoundOutputDeviceImp;
//! \include sndOutDev_ex.cpp

/*!
  The class TSoundOutputDevice permits the playback of a sound track
*/
class DVAPI TSoundOutputDevice {
  std::shared_ptr<TSoundOutputDeviceImp> m_imp;

public:
  TSoundOutputDevice();
  ~TSoundOutputDevice();

  /*!
Returns true if on the machine there is an audio card installed correctly
*/
  static bool installed();

  //! Returns the best format supported near the given parameters
  TSoundTrackFormat getPreferredFormat(TUINT32 sampleRate, int channelCount,
                                       int bitPerSample);

  //! Returns the best format supported near the given format
  TSoundTrackFormat getPreferredFormat(const TSoundTrackFormat &);

  bool isFormatSupported(const TSoundTrackFormat &);

#ifdef MACOSX
  //! Returns true if is possible to change volume setting on current input
  //! interface
  bool supportsVolume();

  //! Returns the current value of the volume [0,1]
  double getVolume();

  //! Set the value of volume , between [0,1]
  bool setVolume(double value);
  void prepareVolume(double volume);
#endif

  //! Open the device according to the features of soundtrack
  bool open(const TSoundTrackP &st);

  //! Playback of the sndtrack in the request sample range
  void play(const TSoundTrackP &st, TINT32 s0, TINT32 s1, bool loop = false,
            bool scrubbing = false);

  //! Close in the right mode the device
  bool close();

  /*!Playback of the sndtrack in the request time range.
The loop argument permits to set the looping status of player
The scrubbing permits to an application to set the mode for the
interaction between sound and mouse
*/
  void play(const TSoundTrackP &st, double t0, double t1, bool loop = false,
            bool scrubbing = false) {
    play(st, st->secondsToSamples(t0), st->secondsToSamples(t1), loop,
         scrubbing);
  }

  //! Stop the playback of one or more soundtracks
  void stop();

  //! Returns if the device is busy with a playback
  bool isPlaying() const;

#ifndef MACOSX
  //! Return true if the playback of all soundtracks is ended.
  bool isAllQueuedItemsPlayed();
#endif

  //! Returns if the status of player is in looping
  bool isLooping();

  //! Permits to change the looping status of player
  void setLooping(bool loop);

  void attach(TSoundOutputDeviceListener *listener);
  void detach(TSoundOutputDeviceListener *listener);
};

#endif
