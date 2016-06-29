

#include "tfilepath.h"
#include "tsound.h"
#include "tsound_io.h"
#include "tsop.h"
#include "tthread.h"
#include "texception.h"
#include "tsystem.h"
#include <iostream>
#include <linux/soundcard.h>
#include <set>
#include <sys/time.h>

#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>

// using namespace std;

// forward declaration
namespace {
int openMixer();
int getCurrentRecordSource(int mixer);
bool writeVolume(int volume, int mixer, int indexDev);
bool selectInputDevice(TSoundInputDevice::Source dev);
string parseError(int error);
}

// bisogna interagire con /dev/dsp per il settaggio delle
// caratteristiche della traccia tipo bit, rate, canali e
// per effettuare lettura/scrittura ossia record/play
//
// mentre bisogna interagire con /dev/mixer per modificare
// i valori del volume e per selezionare il dispositivo da
// cui registrare e ascoltare

class SmartWatch {
  struct timeval m_start_tv;
  TINT32 m_totalus;
  bool m_stopped;

public:
  SmartWatch() : m_totalus(0), m_stopped(true) { timerclear(&m_start_tv); }
  void start() {
    m_stopped = false;
    gettimeofday(&m_start_tv, 0);
  }
  void stop() {
    m_stopped = true;
    struct timeval tv;
    gettimeofday(&tv, 0);
    m_totalus = (tv.tv_sec - m_start_tv.tv_sec) * 1000000 +
                (tv.tv_usec - m_start_tv.tv_usec);
  }
  double getTotalTime() {
    if (!m_stopped)  // questa e' una porcata!
    {
      stop();
      m_stopped = false;
    }
    return m_totalus / 1000.;
  }

  void addDelay(double ms) { m_start_tv.tv_usec += (long)(ms * 1000.); }
};

//======================================================================
//======================================================================
//					CLASSI PER IL PLAYBACK
//======================================================================
//======================================================================
class TSoundOutputDeviceImp {
private:
  static int m_count;

public:
  int m_dev;
  bool m_stopped;
  bool m_isPlaying;
  bool m_looped;
  TSoundTrackFormat m_currentFormat;
  std::set<int> m_supportedRate;
  static std::multimap<TUINT32, TSoundTrackFormat> m_supportFormats;

  typedef pair<TSoundTrackP, bool> WaitPair;
  vector<WaitPair> m_waitingTracks;
  std::set<TSoundOutputDeviceListener *> m_listeners;
  TThread::Executor m_executor;
  TThread::Mutex m_mutex;

  TSoundOutputDeviceImp()
      : m_dev(-1)
      , m_stopped(false)
      , m_isPlaying(false)
      , m_looped(false)
      , m_supportedRate() {
    /*
if (m_count != 0)
throw TException("unable to create second instance of TSoundOutputDeviceImp");
*/
    ++m_count;
    checkSupportedFormat();
  };

  ~TSoundOutputDeviceImp() { --m_count; };

  bool doOpenDevice();
  bool doCloseDevice();
  bool verifyRate();
  void insertAllRate();
  void checkSupportedFormat();
  bool isSupportFormat(const TSoundTrackFormat &fmt);
  void setFormat(const TSoundTrackFormat &fmt);
};

int TSoundOutputDeviceImp::m_count = 0;
std::multimap<TUINT32, TSoundTrackFormat>
    TSoundOutputDeviceImp::m_supportFormats;
//----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doOpenDevice() {
  if (m_dev >= 0) return true;

  TThread::ScopedLock sl(m_mutex);
  m_dev = open("/dev/dsp", O_WRONLY, 0);
  if (m_dev < 0) {
    string errMsg = strerror(errno);
    throw TSoundDeviceException(
        TSoundDeviceException::UnableOpenDevice, errMsg + " /dev/dsp"
        /*"Unable to open device /dev/dsp; check permissions"*/);
  }

  // le chiamate a questa ioctl sono state commentate perche' pag 36 doc OSS
  //"this ioctl stop the device immadiately	and returns it to a state where
  // it
  // can accept new parameters. It Should not be called after opening the device
  // as it may cause unwanted side effect in his situation. require to abort
  // play
  // or secord.Generally recommended to open and close device after using the
  // RESET"
  // ioctl(m_dev, SNDCTL_DSP_RESET,0);

  // N.B. e' bene che la dimensione sia piccola cosi' l'attesa, in seguito
  // alla richiesta di stop,
  // e' minore in questo caso vogliamo 32 frammenti ognuno di 256 byte
  // Se non chiamata questa ioctl il device la calcola per conto suo
  // ma alcune volte la dimensione dei frammenti potrebbe essere eccessiva e
  // creare dei click e dei silenzi in attesi ad esempio nel playback dentro
  // zcomp, quindi e' meglio settarla. 32 rappresenta il numero di frammenti
  // di solito e' documentato che 2 siano sufficienti ma potrebbero essere pochi
  // soprattutto se si interagisce con altre console
  // int fraginfo = ( 32<<16)|8;

  int fraginfo = 0xffffffff;
  if (ioctl(m_dev, SNDCTL_DSP_SETFRAGMENT, &fraginfo) == -1)
    perror("SETFRAGMENT");
  // if(fraginfo != ((32<<16)|8))
  // std::cout << std::hex << fraginfo<<std::endl;
  // exit(0);
  audio_buf_info info;
  if (ioctl(m_dev, SNDCTL_DSP_GETOSPACE, &info) == -1) perror("GETOSPACE");

  // std::cout << info.fragments << " frammenti " << " da " << info.fragsize <<
  // "  = " << info.fragsize*info.fragments << " bytes" <<std::endl;

  return true;
}

//----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doCloseDevice() {
  if (m_dev < 0) return true;

  TThread::ScopedLock sl(m_mutex);
  ioctl(m_dev, SNDCTL_DSP_POST, 0);
  ioctl(m_dev, SNDCTL_DSP_RESET, 0);
  int tmpDev      = m_dev;
  bool not_closed = (close(m_dev) < 0);
  if (not_closed) {
    while (true) {
      m_dev = tmpDev;
      perror("non chiude il device :");
      not_closed = (close(m_dev) < 0);
      if (!not_closed) break;
    }
    // return false;
  }
  ioctl(m_dev, SNDCTL_DSP_RESET, 0);
  m_dev = -1;
  return true;
}

//----------------------------------------------------------------------------

void TSoundOutputDeviceImp::insertAllRate() {
  m_supportedRate.insert(8000);
  m_supportedRate.insert(11025);
  m_supportedRate.insert(16000);
  m_supportedRate.insert(22050);
  m_supportedRate.insert(32000);
  m_supportedRate.insert(44100);
  m_supportedRate.insert(48000);
}

//----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::verifyRate() {
  std::set<int>::iterator it;

  for (it = m_supportedRate.begin(); it != m_supportedRate.end(); ++it) {
    int sampleRate = *it;
    if (ioctl(m_dev, SNDCTL_DSP_SPEED, &sampleRate) == -1)
      throw TSoundDeviceException(
          TSoundDeviceException::UnablePrepare,
          "Failed setting the specified sample rate  aaaa");
    if (sampleRate != *it) m_supportedRate.erase(*it);
  }
  if (m_supportedRate.end() == m_supportedRate.begin()) return false;

  return true;
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::checkSupportedFormat() {
  if (!m_supportFormats.empty()) return;
  int test_formats[] = {
      AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE,
      /* // servono per supportare traccie a 24 e 32 bit ma non compaiono in
// nessun file linux/soundcard.h in distribuzione sulle macchine che abbiamo
// il che fa pensare che non sono supportati ancora
AFMT_S32_LE,
AFMT_S32_BE,
*/
      0};

  int test_channels[] = {1, 2, 0};

  TUINT32 test_sample_rates[] = {8000,  11025, 16000, 22050,
                                 32000, 44100, 48000, 0};

  // Open the device nonblocking, so the open call returns immediately.
  if (m_dev)
    if ((m_dev = open("/dev/dsp", O_WRONLY | O_NONBLOCK)) == -1) {
      /*
m_dev = -1;
string errMsg = strerror(errno);
throw TSoundDeviceException(
                TSoundDeviceException::UnableOpenDevice, errMsg + " /dev/dsp\n:
impossible check supported formats");
*/
      return;
    }

  int mask;
  // Querying hardware supported formats.
  if (ioctl(m_dev, SNDCTL_DSP_GETFMTS, &mask) == -1) {
    /*
throw TSoundDeviceException(
            TSoundDeviceException::UnsupportedFormat,
            "Getting supported formats failed.");
*/
    return;
  } else {
    for (int i = 0; test_formats[i] != 0; i++) {
      // Test all the formats in test_formats which are supported by the
      // hardware.

      if (mask & test_formats[i]) {
        // Test the format only if it is supported by the hardware.
        // Note: We also could test formats that are not supported by the
        // hardware.
        //       In some cases there exist OSS software converter, so that some
        //       formats
        //       work but are not reported by the above SNDCTL_DSP_GETFMTS.

        int fmt = test_formats[i];
        // Try to set the format...
        if (ioctl(m_dev, SNDCTL_DSP_SETFMT, &fmt) == -1)
          continue;  // gli altri formati potrebbero essere supportati
        else {
          // and always check the variable after doing an ioctl!
          if (fmt == test_formats[i]) {
            // Test the supported channel numbers for this format.
            // Note: If you need a channel that is not tested here, simply add
            // it to
            //       the definition of the array test_channels in this file.
            for (int j = 0; test_channels[j] != 0; j++) {
              int test_channel = test_channels[j];

              // Try to set the channel number.
              if (ioctl(m_dev, SNDCTL_DSP_CHANNELS, &test_channel) == -1)
                continue;  // altri canali potrebbero essere supportati
              else {
                if (test_channel == test_channels[j]) {
                  // Last step: Test the supported sample rates for the current
                  // channel number
                  //            and format.
                  // Note: If you need a sample rate that is not tested here,
                  // simply add it to
                  //       the definition of the array test_sample_rates in this
                  //       file.
                  for (int k = 0; test_sample_rates[k] != 0; k++) {
                    TUINT32 test_rate = test_sample_rates[k];
                    if (ioctl(m_dev, SNDCTL_DSP_SPEED, &test_rate) == -1)
                      continue;  // altri rates ppotrebbero essere supportati
                    else {
                      bool sign = true;
                      int bits;

                      if (fmt == AFMT_U8 || fmt == AFMT_S8) {
                        bits                     = 8;
                        if (fmt == AFMT_U8) sign = false;
                      } else if (fmt == AFMT_S16_LE || fmt == AFMT_S16_BE)
                        bits = 16;
                      /*// vedi commento alla variabile test_formats
else if(fmt == AFMT_S32_LE || fmt == AFMT_S32_BE)
bits = 24;
*/
                      // Add it to the format in the input property.
                      // std::cout << test_rate << " " <<bits<<"
                      // "<<test_channel<<std::endl;
                      m_supportFormats.insert(std::make_pair(
                          test_rate, TSoundTrackFormat(test_rate, bits,
                                                       test_channel, sign)));
                    }
                  }
                }  // loop over the test_sample_rates
              }    // channel set correctly ?
            }      // ioctl for sample rate worked ?
          }        // loop over the test_channels
        }          // channel set correctly ?
      }            // ioctl for channel worked ?

      // After testing all configurations for one format, the device is closed
      // and reopened.
      // This is necessary to configure it to another format in a secure way.
      if (close(m_dev) == -1) {
        /*
throw TSoundDeviceException(
          TSoundDeviceException::UnableCloseDevice,
          "Problem to close the output device checking formats");
*/
        continue;
      } else if ((m_dev = open("/dev/dsp", O_WRONLY | O_NONBLOCK)) == -1) {
        /*
m_dev = -1;
string errMsg = strerror(errno);
throw TSoundDeviceException(
            TSoundDeviceException::UnableOpenDevice, errMsg + " /dev/dsp\n:
impossible check supported formats");
*/
        return;
      }
    }  // format set correctly ?
  }    // loop over the test_formats

  // Close the device to make it forget the last format configurations.

  if (close(m_dev) == -1) {
    /*
throw TSoundDeviceException(
            TSoundDeviceException::UnableCloseDevice,
            "Problem to close the output device checking formats");
*/
    return;
  } else
    m_dev = -1;
  // std::cout << "FINITO " << m_supportFormats.size()<< std::endl;
}

//------------------------------------------------------------------------------

bool TSoundOutputDeviceImp::isSupportFormat(const TSoundTrackFormat &fmt) {
  try {
    if (m_supportFormats.empty()) checkSupportedFormat();
  } catch (TSoundDeviceException &e) {
    return false;
  }

  std::multimap<TUINT32, TSoundTrackFormat>::iterator it;
  pair<std::multimap<TUINT32, TSoundTrackFormat>::iterator,
       std::multimap<TUINT32, TSoundTrackFormat>::iterator>
      findRange;
  findRange = m_supportFormats.equal_range(fmt.m_sampleRate);

  it = findRange.first;
  for (; it != findRange.second; ++it) {
    assert(it->first == fmt.m_sampleRate);
    if (it->second == fmt) {
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::setFormat(const TSoundTrackFormat &fmt) {
  int bps, ch, status;
  TUINT32 sampleRate;

  ch         = fmt.m_channelCount;
  sampleRate = fmt.m_sampleRate;

  if (m_dev == -1)
    if (!doOpenDevice()) return;

  if (fmt.m_bitPerSample == 8) {
    if (fmt.m_signedSample)
      bps = AFMT_S8;
    else
      bps = AFMT_U8;
  } else if (fmt.m_bitPerSample == 16) {
    bps = AFMT_S16_NE;
  }
  int bitPerSample = bps;

  status = ioctl(m_dev, SNDCTL_DSP_SETFMT, &bps);
  if (status == -1) {
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "Failed setting the specified number of bits");
  }

  status = ioctl(m_dev, SNDCTL_DSP_CHANNELS, &ch);
  if (status == -1)
    throw TSoundDeviceException(
        TSoundDeviceException::UnablePrepare,
        "Failed setting the specified number of channel");

  if (ioctl(m_dev, SNDCTL_DSP_SPEED, &sampleRate) == -1)
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "Failed setting the specified sample rate");

  if (ch != fmt.m_channelCount || bps != bitPerSample ||
      sampleRate != fmt.m_sampleRate) {
    doCloseDevice();
    m_currentFormat = TSoundTrackFormat();
    return;
  }
  m_currentFormat = fmt;
}

//==============================================================================

class TPlayTask : public TThread::Runnable {
  SmartWatch *m_stopWatch;

public:
  TSoundOutputDeviceImp *m_devImp;
  TSoundTrackP m_sndtrack;
  static int m_skipBytes;

  TPlayTask(TSoundOutputDeviceImp *devImp, const TSoundTrackP &st);

  ~TPlayTask() { delete m_stopWatch; }

  void run();
  void run2();
};

//-------------------------------------------------------------------------------

TPlayTask::TPlayTask(TSoundOutputDeviceImp *devImp, const TSoundTrackP &st)
    : Runnable()
    , m_stopWatch(new SmartWatch)
    , m_devImp(devImp)
    , m_sndtrack(st) {
  if (st->getFormat() != m_devImp->m_currentFormat)
    if (m_devImp->doCloseDevice()) m_devImp->setFormat(st->getFormat());
  m_stopWatch->start();
};

//-------------------------------------------------------------------------------

void TPlayTask::run() {
  int bytesLeft = m_sndtrack->getSampleCount() * m_sndtrack->getSampleSize();
  char *buf     = (char *)m_sndtrack->getRawData();
  int done      = 0;
  int written   = 0;
  TINT32 sampleSize      = (TINT32)m_sndtrack->getSampleSize();
  const double msToBytes = sampleSize * m_sndtrack->getSampleRate() / 1000.;

  TThread::milestone();
  double startupDelay = m_stopWatch->getTotalTime();
  // std::cout << "ritardo iniziale " << startupDelay << std::endl;
  int miss = 0;
  m_stopWatch->start();  // e' meglio ignorare il ritardo iniziale
  if (done > 0) {
    m_stopWatch->addDelay(((done / sampleSize) * 1000) /
                          double(m_sndtrack->getSampleRate()));
  }
  int auxbuffersize   = 0;
  int preWrittenBytes = 0;
  int bytesToSkipNext;
  TSoundTrackP src    = TSoundTrack::create(m_sndtrack->getFormat(), 1);
  TSoundTrackP dst    = src;
  TSoundTrackP newSrc = src;
  try {
    do  // gia' tracce accodate
    {
      bool changeSnd = false;
      do  // c'e' il flag loop settato
      {
        while ((bytesLeft > 0)) {
          TThread::milestone();
          changeSnd = false;
          audio_buf_info info;
          TINT32 bytesToWrite     = 0;
          TINT32 bytesToWriteNext = 0;
          double samplesDone      = done / (double)sampleSize;
          double trackTime =
              (samplesDone * 1000.) / m_sndtrack->getSampleRate();
          double curTime = m_stopWatch->getTotalTime();
          double delta   = curTime - trackTime;

          /*
           delta
                                           == 0 sync
                                           <	 0 audio piu' veloce del tempo
           di playback  --> simuliamo un ritardo con un continue;
                                           >  0 audio piu' lento del playback ->
           skip una porzione di audio
   */

          const double minDelay = -10;
          const double maxDelay = 0;
          // if (delta>maxDelay)
          // std::cout << "buffer underrun:" << delta << std::endl;
          // std::cout << "buffer " <<
          // (delta<minDelay?"overrun":delta>maxDelay?"underrun":"sync")  << " "
          // << delta<< std::endl;

          if (delta < minDelay)  // overrun
          {
            // std::cout << "out of sync -> audio troppo veloce" << std::endl;
            continue;
          }

          if (ioctl(m_devImp->m_dev, SNDCTL_DSP_GETOSPACE, &info) == -1) {
            miss++;
            break;
          }

          int fragmentsFree_bytes = info.fragsize * info.fragments;
          if (fragmentsFree_bytes == 0) {
            // std::cout << "no bytes left on device" << std::endl;
            continue;
          }

          int bytesToSkip = 0;
          bytesToSkipNext = 0;
          if (delta > maxDelay)  // underrun
          {
            // std::cout << "out of sync -> audio troppo lento"<<std::endl;
            // skip di una porzione... delta in ms
            bytesToSkip =
                tceil(delta * msToBytes);  // numero di bytes da skippare
            // lo arrotondo al sample size
            bytesToSkip += sampleSize - (bytesToSkip % sampleSize);
            // std::cout << "bytes skippati "<< bytesToSkip << std::endl;
          } else {  // sto fra minDelay e maxDelay
            bytesToSkip = 0;
          }

          bytesToSkipNext = bytesToSkip;
          bytesToSkip     = tmin(bytesLeft, bytesToSkip);
          bytesToSkipNext -= bytesToSkip;  // se !=0 => la corrente traccia non
                                           // basta per avere il sync
          bytesLeft -= bytesToSkip;
          done += bytesToSkip;

          bytesToWrite     = tmin(bytesLeft, fragmentsFree_bytes);
          bytesToWriteNext = fragmentsFree_bytes - bytesToWrite;
          assert(bytesToWrite >= 0);
          assert(bytesToWriteNext >= 0);

          if (bytesToWrite % info.fragsize !=
              0) {  // cerco di gestire il write di un frammento non intero
            auxbuffersize =
                ((bytesToWrite / info.fragsize) + 1) * info.fragsize;
          } else
            auxbuffersize = 0;

          //--------------write
          if (bytesToSkipNext == 0)  // la corrente traccia basta per lo skip
          {
            std::cout << " QUI 0 " << std::endl;
            dst = m_sndtrack->extract(done / sampleSize,
                                      (done + bytesToWrite) / sampleSize);
            if (bytesToSkip != 0) {
              // costruisco traccia su cui fare il crossfade
              // utilizzo il contenuto della traccia di crossfade
              dst = TSop::crossFade(0.2, src, dst);
            }
            char *auxbuf = new char[fragmentsFree_bytes];
            memcpy(auxbuf, (char *)dst->getRawData(), bytesToWrite);
            if (bytesToWriteNext != 0) {
              int offset = bytesToWrite;
              if (m_devImp->m_looped) {
                offset += bytesToWriteNext;
                preWrittenBytes = bytesToWriteNext;
                memcpy(auxbuf + offset, buf, preWrittenBytes);
                newSrc = m_sndtrack->extract(preWrittenBytes / sampleSize,
                                             preWrittenBytes / sampleSize);
                std::cout << " QUI 1" << std::endl;
              } else {
                while (!m_devImp->m_waitingTracks.empty()) {
                  TSoundTrackP st = m_devImp->m_waitingTracks[0].first;
                  int count       = st->getSampleCount() * sampleSize;
                  if (bytesToWriteNext >= count) {
                    char *buffer = (char *)st->getRawData();
                    memcpy(auxbuf + offset, buffer, count);
                    bytesToWriteNext -= count;
                    offset += count;
                    std::cout << " QUI 2" << std::endl;
                    if (m_devImp->m_waitingTracks[0].second) {
                      m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                      m_devImp->m_waitingTracks.erase(
                          m_devImp->m_waitingTracks.begin());
                      newSrc = m_sndtrack->extract(count / sampleSize,
                                                   count / sampleSize);
                      m_sndtrack      = st;
                      preWrittenBytes = 0;
                      std::cout << " QUI 3" << std::endl;
                      break;
                    }
                    m_devImp->m_waitingTracks.erase(
                        m_devImp->m_waitingTracks.begin());
                    newSrc = m_sndtrack->extract(count / sampleSize,
                                                 count / sampleSize);
                  } else {
                    m_sndtrack         = st;
                    m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                    preWrittenBytes    = bytesToWriteNext;
                    buf                = (char *)m_sndtrack->getRawData();
                    memcpy(auxbuf + offset, buf, bytesToWriteNext);
                    m_devImp->m_waitingTracks.erase(
                        m_devImp->m_waitingTracks.begin());
                    newSrc =
                        m_sndtrack->extract((bytesToWriteNext) / sampleSize,
                                            (bytesToWriteNext) / sampleSize);
                    std::cout << " QUI 4" << std::endl;
                    break;
                  }
                }  // end while
              }

              if (fragmentsFree_bytes > offset) {
                std::cout << " QUI 5" << std::endl;
                int val = m_sndtrack->isSampleSigned() ? 0 : 127;
                memset(auxbuf + offset, val,
                       fragmentsFree_bytes - offset);  // ci metto silenzio
                newSrc = TSoundTrack::create(m_sndtrack->getFormat(), 1);
              }
            }

            written = write(m_devImp->m_dev, auxbuf, fragmentsFree_bytes);
            delete[] auxbuf;
          } else  // devo skippare anche parte di una delle seguenti
          {
            std::cout << " QUI 6a" << std::endl;
            assert(bytesToWriteNext > 0);
            assert(bytesToWriteNext == fragmentsFree_bytes - bytesToWrite);
            assert(bytesToWrite == 0);
            assert(bytesToSkip != 0);
            char *auxbuf = new char[fragmentsFree_bytes];
            // memcpy(auxbuf, buf+done, bytesToWrite);
            // TSoundTrackP subCross =
            // m_sndtrack->extract((done+bytesToWrite)/sampleSize,
            // (done+bytesToWrite)/sampleSize);
            // togli quelle da skippare
            int backupSkipNext = bytesToSkipNext;
            while (!m_devImp->m_waitingTracks.empty()) {
              TSoundTrackP st = m_devImp->m_waitingTracks[0].first;
              int count       = st->getSampleCount() * sampleSize;
              if (bytesToSkipNext >= count) {
                std::cout << " QUI 6b" << std::endl;
                m_devImp->m_waitingTracks.erase(
                    m_devImp->m_waitingTracks.begin());
                bytesToSkipNext -= count;
              } else {
                std::cout << " QUI 7" << std::endl;
                m_devImp->m_waitingTracks.erase(
                    m_devImp->m_waitingTracks.begin());
                m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                m_sndtrack         = st;
                buf                = (char *)st->getRawData();
                break;
              }
            }
            // scrivi byteWriteNext fai crossfade e cerca quella successiva
            // con cui riempire
            TINT32 displacement =
                0;  // deve essere in munero di campioni non bytes
            dst = TSoundTrack::create(
                m_sndtrack->getFormat(),
                (fragmentsFree_bytes - bytesToWrite) / sampleSize);
            int count = m_sndtrack->getSampleCount() * sampleSize;
            if (count >= bytesToSkipNext + bytesToWriteNext)  // la traccia
                                                              // trovata e' suff
                                                              // sia per
                                                              // skippare che
                                                              // per scrivere
            {
              preWrittenBytes = bytesToSkipNext + bytesToWriteNext;
              dst = m_sndtrack->extract(bytesToSkipNext / sampleSize,
                                        preWrittenBytes / sampleSize);
              newSrc = m_sndtrack->extract(preWrittenBytes / sampleSize,
                                           preWrittenBytes / sampleSize);
            } else  // non e' suff per scrivere
            {
              dst->copy(m_sndtrack->extract(bytesToSkipNext / sampleSize,
                                            m_sndtrack->getSampleCount() - 1),
                        0);
              displacement =
                  m_sndtrack->getSampleCount() - bytesToSkipNext / sampleSize;
              bytesToWriteNext -= displacement * sampleSize;
              while (!m_devImp->m_waitingTracks.empty()) {
                TSoundTrackP st = m_devImp->m_waitingTracks[0].first;
                int count       = st->getSampleCount() * sampleSize;
                if (bytesToWriteNext >= count) {
                  std::cout << " QUI 8" << std::endl;
                  dst->copy(st, displacement);
                  bytesToWriteNext -= count;
                  displacement += count;
                  if (m_devImp->m_waitingTracks[0].second) {
                    std::cout << " QUI 9" << std::endl;
                    m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                    m_devImp->m_waitingTracks.erase(
                        m_devImp->m_waitingTracks.begin());
                    newSrc = m_sndtrack->extract(count / sampleSize,
                                                 count / sampleSize);
                    m_sndtrack = st;
                    break;
                  }
                  m_devImp->m_waitingTracks.erase(
                      m_devImp->m_waitingTracks.begin());
                  newSrc = m_sndtrack->extract(count / sampleSize,
                                               count / sampleSize);
                } else {
                  std::cout << " QUI 10" << std::endl;
                  dst->copy(st->extract(0L, bytesToWriteNext / sampleSize),
                            displacement);
                  m_sndtrack         = st;
                  m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                  preWrittenBytes    = bytesToWriteNext;
                  done               = preWrittenBytes;
                  bytesLeft = m_sndtrack->getSampleCount() * sampleSize - done;
                  buf       = (char *)m_sndtrack->getRawData();
                  m_devImp->m_waitingTracks.erase(
                      m_devImp->m_waitingTracks.begin());
                  newSrc = m_sndtrack->extract(preWrittenBytes / sampleSize,
                                               preWrittenBytes / sampleSize);
                  break;
                }
              }
              bytesToSkipNext = backupSkipNext;
            }

            TSoundTrackP st = TSop::crossFade(0.2, src, dst);
            memcpy(auxbuf + bytesToWrite, (char *)st->getRawData(),
                   fragmentsFree_bytes - bytesToWrite);

            // devo ricercare quella giusta che non deve essere skippata
            // ma sostitutita come traccia corrente
            // devo fare un cross fade
            written = write(m_devImp->m_dev, auxbuf, fragmentsFree_bytes);
            delete[] auxbuf;
          }
          //----------- end write
          src = newSrc;
          if (written == -1) break;
          std::cout << written << " " << (bytesToWrite + preWrittenBytes)
                    << std::endl;
          if (written != bytesToWrite + preWrittenBytes) break;
          std::cout << " update done 2" << std::endl;
          bytesLeft -= written;
          done += written;
        }  // chiudo il while((bytesLeft > 0))
        std::cout << " QUI 11" << std::endl;
        done      = preWrittenBytes + bytesToSkipNext;
        written   = 0;
        bytesLeft = m_sndtrack->getSampleCount() * sampleSize - done;
        m_stopWatch->start();
        if (done > 0) {
          m_stopWatch->addDelay(((done / m_sndtrack->getSampleSize()) * 1000) /
                                double(m_sndtrack->getSampleRate()));
        }
        preWrittenBytes = 0;
      } while (m_devImp->m_looped || changeSnd);

      if (m_devImp->m_waitingTracks.empty()) break;
      m_sndtrack         = m_devImp->m_waitingTracks[0].first;
      m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
      m_devImp->m_waitingTracks.erase(m_devImp->m_waitingTracks.begin());
      bytesLeft = m_sndtrack->getSampleCount() * m_sndtrack->getSampleSize();
      buf       = (char *)m_sndtrack->getRawData();
      done      = 0;
      written   = 0;

      m_stopWatch->start();  // ignoro il ritardo iniziale
      if (done > 0) {
        m_stopWatch->addDelay(((done / m_sndtrack->getSampleSize()) * 1000) /
                              double(m_sndtrack->getSampleRate()));
      }
    } while (true);  // ci sono gia' tracce accodate

    if (!m_devImp->m_waitingTracks.empty()) {
      m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
      m_devImp->m_executor.addTask(
          new TPlayTask(m_devImp, m_devImp->m_waitingTracks[0].first));
      m_devImp->m_waitingTracks.erase(m_devImp->m_waitingTracks.begin());
      // std::cout<<"OPS ..... erase 4"<<std::endl;
    } else if (m_devImp->m_dev != -1) {
      if (ioctl(m_devImp->m_dev, SNDCTL_DSP_SYNC) == -1) {
        std::cout << "unable to sync! " << std::endl;
        throw TException("unable to sync!");
      }

      m_devImp->m_isPlaying = false;
      m_devImp->m_stopped   = true;
      m_devImp->m_looped    = false;

      // std::cout << "miss = " << miss << std::endl;
    }
  } catch (TThread::Interrupt &e) {
    std::cout << "Play interrupted " << e.getMessage() << std::endl;
    m_devImp->m_isPlaying = false;
    m_devImp->m_stopped   = true;
    m_devImp->m_looped    = false;
  } catch (TException &e) {
    std::cout << "esco dal play " << e.getMessage() << std::endl;
    m_devImp->m_isPlaying = false;
    m_devImp->m_stopped   = true;
    m_devImp->m_looped    = false;
  }
}

//-------------------------------------------------------------------------------

void TPlayTask::run2() {
  int bytesLeft = m_sndtrack->getSampleCount() * m_sndtrack->getSampleSize();
  char *buf     = (char *)m_sndtrack->getRawData();
  int done      = 0;
  int written   = 0;
  TINT32 sampleSize      = (TINT32)m_sndtrack->getSampleSize();
  const double msToBytes = sampleSize * m_sndtrack->getSampleRate() / 1000.;

  TThread::milestone();
  double startupDelay = m_stopWatch->getTotalTime();
  // std::cout << "ritardo iniziale " << startupDelay << std::endl;
  int miss = 0;
  m_stopWatch->start();  // e' meglio ignorare il ritardo iniziale
  if (done > 0) {
    m_stopWatch->addDelay(((done / sampleSize) * 1000) /
                          double(m_sndtrack->getSampleRate()));
  }
  int auxbuffersize   = 0;
  int preWrittenBytes = 0;
  TSoundTrackP src    = TSoundTrack::create(m_sndtrack->getFormat(), 1);
  TSoundTrackP dst    = src;
  try {
    do  // gia' tracce accodate
    {
      bool changeSnd = false;
      do  // c'e' il flag loop settato
      {
        while ((bytesLeft > 0)) {
          changeSnd = false;
          TThread::milestone();
          audio_buf_info info;
          TINT32 bytesToWrite;
          double samplesDone = done / (double)sampleSize;
          double trackTime =
              (samplesDone * 1000.) / m_sndtrack->getSampleRate();
          double curTime = m_stopWatch->getTotalTime();
          double delta   = curTime - trackTime;

          /*
           delta
                                           == 0 sync
                                           <	 0 audio piu' veloce del tempo
           di playback  --> simuliamo un ritardo con un continue;
                                           >  0 audio piu' lento del playback ->
           skip una porzione di audio
   */

          const double minDelay = -10;
          const double maxDelay = 0;
          // if (delta>maxDelay)
          // std::cout << "buffer underrun:" << delta << std::endl;
          // std::cout << "buffer " <<
          // (delta<minDelay?"overrun":delta>maxDelay?"underrun":"sync")  << " "
          // << delta<< std::endl;

          if (delta < minDelay)  // overrun
          {
            // std::cout << "out of sync -> audio troppo veloce" << std::endl;
            continue;
          }

          if (ioctl(m_devImp->m_dev, SNDCTL_DSP_GETOSPACE, &info) == -1) {
            miss++;
            break;
          }

          int fragmentsFree_bytes = info.fragsize * info.fragments;
          if (fragmentsFree_bytes == 0) {
            // std::cout << "no bytes left on device" << std::endl;
            continue;
          }

          int bytesToSkip = 0;
          int bigSkip     = 0;
          if (delta > maxDelay)  // underrun
          {
            // std::cout << "out of sync -> audio troppo lento"<<std::endl;
            // skip di una porzione... delta in ms
            bytesToSkip =
                tceil(delta * msToBytes);  // numero di bytes da skippare
            // lo arrotondo al sample size
            bytesToSkip += sampleSize - (bytesToSkip % sampleSize);
            // std::cout << "bytes skippati "<< bytesToSkip << std::endl;
            bigSkip = bytesToSkip;
          } else {  // sto fra minDelay e maxDelay
            bytesToSkip = 0;
          }

          bytesToSkip = tmin(bytesLeft, bytesToSkip);
          bigSkip -= bytesToSkip;
          bytesLeft -= bytesToSkip;
          done += bytesToSkip;

          bytesToWrite = tmin(bytesLeft, fragmentsFree_bytes);

          if (bytesToWrite % info.fragsize !=
              0) {  // cerco di gestire il write di un frammento non intero
            auxbuffersize =
                ((bytesToWrite / info.fragsize) + 1) * info.fragsize;
          }

          if (bigSkip)
            while (!m_devImp->m_waitingTracks.empty()) {
              TSoundTrackP st = m_devImp->m_waitingTracks[0].first;
              int count       = st->getSampleCount() * sampleSize;
              if (bigSkip >= count) {
                m_devImp->m_waitingTracks.erase(
                    m_devImp->m_waitingTracks.begin());
                bigSkip -= count;
              } else
                break;
            }

          preWrittenBytes = 0;
          if (auxbuffersize == 0) {
            if (bytesToSkip != 0) {
              // costruisco traccia su cui fare il crossfade
              // utilizzo il contenuto della traccia di crossfade
              dst = m_sndtrack->extract(done / sampleSize,
                                        (done + bytesToWrite) / sampleSize);
              TSoundTrackP st = TSop::crossFade(0.2, src, dst);
              char *buffer    = (char *)st->getRawData();
              written         = write(m_devImp->m_dev, buffer, bytesToWrite);
            } else
              written = write(m_devImp->m_dev, buf + done, bytesToWrite);
            src       = m_sndtrack->extract((done + bytesToWrite) / sampleSize,
                                      (done + bytesToWrite) / sampleSize);
          } else {  // auxbuffersize != 0 sse il numero di bytes residui nella
                    // traccia e' inferiore alla dimensione del frammento
            char *auxbuf = new char[auxbuffersize];
            TSoundTrackP newSrc;
            dst = TSoundTrack::create(m_sndtrack->getFormat(),
                                      auxbuffersize / sampleSize);
            memcpy(auxbuf, buf + done, bytesToWrite);
            dst->copy(m_sndtrack->extract(done / sampleSize,
                                          (done + bytesToWrite) / sampleSize),
                      0);
            preWrittenBytes = auxbuffersize - bytesToWrite;
            if (m_devImp->m_looped) {
              memcpy(auxbuf + bytesToWrite, buf, preWrittenBytes);
              dst->copy(m_sndtrack->extract(0, preWrittenBytes / sampleSize),
                        bytesToWrite / sampleSize);
              newSrc = m_sndtrack->extract(preWrittenBytes / sampleSize,
                                           preWrittenBytes / sampleSize);
            } else {
              newSrc = TSoundTrack::create(m_sndtrack->getFormat(), 1);
              static int added = 0;
              // se non c'e' alcuna altra traccia o e di diverso format
              // riempo il frammento con del silenzio
              if (m_devImp->m_waitingTracks.empty() ||
                  (m_sndtrack->getFormat() !=
                   m_devImp->m_waitingTracks[0].first->getFormat())) {
                int val = m_sndtrack->isSampleSigned() ? 0 : 127;
                memset(auxbuf + bytesToWrite, val,
                       preWrittenBytes);  // ci metto silenzio
              } else
                while (true)  // ci sono altre tracce accodate
                {
                  TSoundTrackP st = m_devImp->m_waitingTracks[0].first;
                  int sampleBytes = st->getSampleCount() * st->getSampleSize();
                  if (sampleBytes >= preWrittenBytes - added) {
                    // La traccia ha abbastanza campioni per riempire il
                    // frammento
                    // quindi la sostituisco alla corrente del runnable e
                    // continuo
                    buf = (char *)st->getRawData();
                    memcpy(auxbuf + bytesToWrite, buf, preWrittenBytes - added);
                    m_sndtrack         = st;
                    m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                    m_devImp->m_waitingTracks.erase(
                        m_devImp->m_waitingTracks.begin());
                    changeSnd = true;
                    dst->copy(m_sndtrack->extract(
                                  0, (preWrittenBytes - added) / sampleSize),
                              bytesToWrite / sampleSize + added);
                    newSrc = m_sndtrack->extract(
                        (preWrittenBytes - added) / sampleSize,
                        (preWrittenBytes - added) / sampleSize);
                    break;
                  } else {  // occhio al loop
                    // La traccia successiva e' piu corta del frammento da
                    // riempire quindi
                    // ce la metto tutta e se non ha il flag di loop settato
                    // cerco di aggiungere
                    // i byte della successiva
                    memcpy(auxbuf + bytesToWrite, st->getRawData(),
                           sampleBytes);
                    dst->copy(st->extract(0, st->getSampleCount() - 1),
                              bytesToWrite / sampleSize);
                    added += st->getSampleCount();
                    if (m_devImp->m_waitingTracks[0]
                            .second)  // e' quella che deve essere in loop
                    {
                      buf                = (char *)st->getRawData();
                      m_sndtrack         = st;
                      m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
                      preWrittenBytes    = 0;
                      bytesLeft          = 0;
                      m_devImp->m_waitingTracks.erase(
                          m_devImp->m_waitingTracks.begin());
                      changeSnd = true;
                      break;
                    }

                    // la elimino e vedo se esiste la successiva altrimenti
                    // metto campioni "zero"
                    m_devImp->m_waitingTracks.erase(
                        m_devImp->m_waitingTracks.begin());
                    if (!m_devImp->m_waitingTracks.empty()) {
                      st = m_devImp->m_waitingTracks[0].first;
                      std::cout
                          << " Traccia con meno campioni cerco la successiva"
                          << std::endl;
                    } else {
                      int val = m_sndtrack->isSampleSigned() ? 0 : 127;
                      memset(
                          auxbuf + bytesToWrite, val,
                          preWrittenBytes - sampleBytes);  // ci metto silenzio
                      std::cout << "OPS ..... silence" << std::endl;
                      break;
                    }
                  }
                }  // end while(true)
            }
            // qui andrebbe fatto un cross-fade se c'erano da skippare campioni
            // => bytesToSkip != 0
            if (bytesToSkip != 0) {
              TSoundTrackP st = TSop::crossFade(0.2, src, dst);
              char *buffer    = (char *)st->getRawData();
              written         = write(m_devImp->m_dev, buffer, bytesToWrite);
            } else
              written     = write(m_devImp->m_dev, auxbuf, auxbuffersize);
            src           = newSrc;
            auxbuffersize = 0;
            delete[] auxbuf;
          }
          if (written == -1) break;
          if (written != bytesToWrite + preWrittenBytes) break;
          bytesLeft -= written;
          done += written;
        }  // chiudo il while((bytesLeft > 0))
        done    = preWrittenBytes;
        written = 0;
        bytesLeft =
            m_sndtrack->getSampleCount() * m_sndtrack->getSampleSize() - done;
        m_stopWatch->start();
        if (done > 0) {
          m_stopWatch->addDelay(((done / m_sndtrack->getSampleSize()) * 1000) /
                                double(m_sndtrack->getSampleRate()));
        }
      } while (m_devImp->m_looped || changeSnd);

      if (m_devImp->m_waitingTracks.empty()) {
        // std::cout<<"OPS ..... non accodato"<<std::endl;
        break;
      }

      // std::cout<<"OPS ..... accodato"<<std::endl;
      m_sndtrack         = m_devImp->m_waitingTracks[0].first;
      m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
      m_devImp->m_waitingTracks.erase(m_devImp->m_waitingTracks.begin());
      bytesLeft = m_sndtrack->getSampleCount() * m_sndtrack->getSampleSize();
      buf       = (char *)m_sndtrack->getRawData();
      done      = 0;
      written   = 0;

      m_stopWatch->start();  // ignoro il ritardo iniziale
      if (done > 0) {
        m_stopWatch->addDelay(((done / m_sndtrack->getSampleSize()) * 1000) /
                              double(m_sndtrack->getSampleRate()));
      }
    } while (true);  // ci sono gia' tracce accodate

    if (!m_devImp->m_waitingTracks.empty()) {
      m_devImp->m_looped = m_devImp->m_waitingTracks[0].second;
      m_devImp->m_executor.addTask(
          new TPlayTask(m_devImp, m_devImp->m_waitingTracks[0].first));
      m_devImp->m_waitingTracks.erase(m_devImp->m_waitingTracks.begin());
      // std::cout<<"OPS ..... erase 4"<<std::endl;
    } else if (m_devImp->m_dev != -1) {
      if (ioctl(m_devImp->m_dev, SNDCTL_DSP_SYNC) == -1) {
        std::cout << "unable to sync! " << std::endl;
        throw TException("unable to sync!");
      }

      m_devImp->m_isPlaying = false;
      m_devImp->m_stopped   = true;
      m_devImp->m_looped    = false;

      // std::cout << "miss = " << miss << std::endl;
    }
  } catch (TThread::Interrupt &e) {
    std::cout << "Play interrupted " << e.getMessage() << std::endl;
    m_devImp->m_isPlaying = false;
    m_devImp->m_stopped   = true;
    m_devImp->m_looped    = false;
  } catch (TException &e) {
    std::cout << "esco dal play " << e.getMessage() << std::endl;
    m_devImp->m_isPlaying = false;
    m_devImp->m_stopped   = true;
    m_devImp->m_looped    = false;
  }
}

//==============================================================================

TSoundOutputDevice::TSoundOutputDevice() : m_imp(new TSoundOutputDeviceImp) {
  if (m_imp->doOpenDevice()) {
    m_imp->insertAllRate();
    try {
      if (!m_imp->verifyRate())
        throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                    "No default samplerate are supported");
    } catch (TSoundDeviceException &e) {
      throw TSoundDeviceException(e.getType(), e.getMessage());
    }
    m_imp->doCloseDevice();
  }
}

//------------------------------------------------------------------------------

TSoundOutputDevice::~TSoundOutputDevice() { close(); }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::installed() {
  bool ret = false;
  int dev  = ::open("/dev/dsp", O_WRONLY, 0);
  if (dev >= 0) {
    ret = true;
    ::close(dev);
  }
  return ret;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::open(const TSoundTrackP &st) {
  m_imp->m_currentFormat = st->getFormat();
  try {
    m_imp->doOpenDevice();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::close() {
  stop();
  if (m_imp->m_dev != -1) {
    bool closed = m_imp->doCloseDevice();
    if (!closed)
      throw TSoundDeviceException(
          TSoundDeviceException::UnableCloseDevice,
          "Error during the closing of the output device");
  }
  return true;
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

void TSoundOutputDevice::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1,
                              bool loop, bool scrubbing) {
  assert((scrubbing && !loop) || !scrubbing);
  if (!st->getSampleCount()) return;

  TThread::ScopedLock sl(m_imp->m_mutex);
  if (m_imp->m_looped)
    throw TSoundDeviceException(
        TSoundDeviceException::Busy,
        "Unable to queue another playback when the sound player is looping");

  TSoundTrackFormat format = st->getFormat();
  if (!m_imp->isSupportFormat(format)) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                "Unsupported format for playback");
  }

  if (m_imp->m_isPlaying) {
    assert(s1 >= s0);
    TSoundTrackP subTrack = st->extract(s0, s1);
    m_imp->m_waitingTracks.push_back(std::make_pair(subTrack, loop));
    // std::cout<<"Sono in pushback"<<std::endl;
    return;
  }

  if ((m_imp->m_dev == -1)) try {
      if (m_imp->doOpenDevice()) m_imp->setFormat(format);
    } catch (TSoundDeviceException &e) {
      m_imp->doCloseDevice();
      throw TSoundDeviceException(e.getType(), e.getMessage());
    }

  m_imp->m_isPlaying = true;
  m_imp->m_stopped   = false;
  m_imp->m_looped    = loop;
  // m_imp->m_currentFormat = st->getFormat();

  assert(s1 >= s0);
  TSoundTrackP subTrack = st->extract(s0, s1);

  m_imp->m_executor.addTask(new TPlayTask(m_imp, subTrack));
}
//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
  TThread::ScopedLock sl(m_imp->m_mutex);
  if (!m_imp->m_isPlaying) return;
  m_imp->m_executor.cancel();
  ioctl(m_imp->m_dev, SNDCTL_DSP_POST, 0);
  m_imp->m_isPlaying = false;
  m_imp->m_stopped   = true;
  m_imp->m_looped    = false;
  m_imp->m_waitingTracks.clear();
}

//------------------------------------------------------------------------------
double TSoundOutputDevice::getVolume() {
  int mixer;
  if ((mixer = openMixer()) < 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't open the mixer device");

  int devmask;
  if (ioctl(mixer, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int recmask;
  if (ioctl(mixer, SOUND_MIXER_READ_RECMASK, &recmask) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int stereo;
  if (ioctl(mixer, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int outmask = devmask | ~recmask;

  int index;
  if (outmask & (1 << SOUND_MIXER_ALTPCM))
    index = SOUND_MIXER_ALTPCM;
  else if (outmask & (1 << SOUND_MIXER_PCM))
    index = SOUND_MIXER_PCM;

  int level;
  if (ioctl(mixer, MIXER_READ(index), &level) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to read the volume");
  }
  if ((1 << index) & stereo) {
    int left  = level & 0xff;
    int right = ((level & 0xff00) >> 8);
    ::close(mixer);
    return (left + right) / 20.0;
  } else {
    ::close(mixer);
    return (level & 0xff) / 10.0;
  }
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::setVolume(double volume) {
  int mixer;
  if ((mixer = openMixer()) < 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't open the mixer device");

  int devmask;
  if (ioctl(mixer, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int recmask;
  if (ioctl(mixer, SOUND_MIXER_READ_DEVMASK, &recmask) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int stereo;
  if (ioctl(mixer, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int outmask = devmask | ~recmask;

  if (outmask & (1 << SOUND_MIXER_ALTPCM)) {
    int vol, index = SOUND_MIXER_ALTPCM;
    if ((1 << index) & stereo) {
      volume *= 10.0;
      vol = (int)volume + ((int)(volume * 256.0));
    } else
      vol = (int)(volume * 10.0);

    if (!writeVolume(vol, mixer, index)) {
      ::close(mixer);
      throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                  "Can't write the volume");
    }

    // metto anche l'altro ad un livello di sensibilita' adeguata
    if (outmask & (1 << SOUND_MIXER_PCM)) {
      int vol, index = SOUND_MIXER_PCM;
      double volDefault = 6.7;
      if ((1 << index) & stereo) {
        volDefault *= 10.0;
        vol = (int)volDefault + ((int)(volDefault * 256.0));
      } else
        vol = (int)(volDefault * 10.0);

      if (!writeVolume(vol, mixer, index)) {
        ::close(mixer);
        throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                    "Can't write the volume");
      }
    }
  } else if (outmask & (1 << SOUND_MIXER_PCM)) {
    int vol, index = SOUND_MIXER_PCM;
    if ((1 << index) & stereo) {
      volume *= 10.0;
      vol = (int)volume + ((int)(volume * 256.0));
    } else
      vol = (int)(volume * 10.0);

    if (!writeVolume(vol, mixer, index)) {
      ::close(mixer);
      throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                  "Can't write the volume");
    }
  }

  ::close(mixer);
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const {
  TThread::ScopedLock sl(m_imp->m_mutex);
  return m_imp->m_isPlaying;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() {
  TThread::ScopedLock sl(m_imp->m_mutex);
  return m_imp->m_looped;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) {
  TThread::ScopedLock sl(m_imp->m_mutex);
  m_imp->m_looped = loop;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::supportsVolume() {
  int mixer;
  if ((mixer = openMixer()) < 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't open the mixer device");

  int devmask;
  if (ioctl(mixer, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int recmask;
  if (ioctl(mixer, SOUND_MIXER_READ_DEVMASK, &recmask) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int stereo;
  if (ioctl(mixer, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Error in ioctl with mixer device");
  }

  int outmask = devmask | ~recmask;

  if ((outmask & (1 << SOUND_MIXER_ALTPCM)) ||
      (outmask & (1 << SOUND_MIXER_PCM))) {
    ::close(mixer);
    return true;
  }

  return false;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                         int channelCount,
                                                         int bitPerSample) {
  TSoundTrackFormat fmt;
  if (bitPerSample == 8)
    fmt = TSoundTrackFormat(sampleRate, channelCount, bitPerSample, false);
  else
    fmt = TSoundTrackFormat(sampleRate, channelCount, bitPerSample);
  if (m_imp->isSupportFormat(fmt)) return fmt;

  int bps, ch, status;

  bps = bitPerSample;
  ch  = channelCount;

  if (m_imp->m_dev == -1) m_imp->doOpenDevice();

  if (bitPerSample <= 8) {
    bitPerSample       = AFMT_U8;
    fmt.m_signedSample = false;
  } else if ((bitPerSample > 8 && bitPerSample < 16) || bitPerSample >= 16) {
    bitPerSample       = AFMT_S16_NE;
    fmt.m_signedSample = true;
  }

  status = ioctl(m_imp->m_dev, SNDCTL_DSP_SETFMT, &bitPerSample);
  if (status == -1) {
    perror("CHE palle ");
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "Failed setting the specified number of bits");
  }
  fmt.m_bitPerSample = bitPerSample;

  status = ioctl(m_imp->m_dev, SNDCTL_DSP_CHANNELS, &channelCount);
  if (status == -1)
    throw TSoundDeviceException(
        TSoundDeviceException::UnablePrepare,
        "Failed setting the specified number of channel");
  fmt.m_channelCount = channelCount;

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

  if (ioctl(m_imp->m_dev, SNDCTL_DSP_SPEED, &sampleRate) == -1)
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "Failed setting the specified sample rate");
  fmt.m_sampleRate = sampleRate;

  if (ch != channelCount || bps != bitPerSample) {
    m_imp->doCloseDevice();
  }

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

//======================================================================
//======================================================================
//					CLASSI PER IL RECORD
//======================================================================
//======================================================================

class TSoundInputDeviceImp {
public:
  int m_dev;
  bool m_stopped;
  bool m_isRecording;
  TSoundTrackFormat m_currentFormat;
  TSoundTrackP m_st;
  std::set<int> m_supportedRate;

  TINT32 m_recordedSampleCount;
  vector<char *> m_recordedBlocks;
  vector<int> m_samplePerBlocks;
  bool m_oneShotRecording;

  TThread::Executor m_executor;

  TSoundInputDeviceImp()
      : m_dev(-1)
      , m_stopped(false)
      , m_isRecording(false)
      , m_st(0)
      , m_supportedRate()
      , m_recordedBlocks()
      , m_samplePerBlocks()
      , m_oneShotRecording(false) {}

  ~TSoundInputDeviceImp() {}

  bool doOpenDevice(const TSoundTrackFormat &format,
                    TSoundInputDevice::Source devType);
  bool doCloseDevice();
  void insertAllRate();
  bool verifyRate();
};

//------------------------------------------------------------------------------

bool TSoundInputDeviceImp::doOpenDevice(const TSoundTrackFormat &format,
                                        TSoundInputDevice::Source devType) {
  m_dev = open("/dev/dsp", O_RDONLY);
  if (m_dev < 0)
    throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice,
                                "Cannot open the dsp device");

  // ioctl(m_dev, SNDCTL_DSP_RESET,0);

  return true;
}

//------------------------------------------------------------------------------

bool TSoundInputDeviceImp::doCloseDevice() {
  if (close(m_dev) < 0)
    throw TSoundDeviceException(TSoundDeviceException::UnableCloseDevice,
                                "Cannot close the dsp device");
  m_dev = -1;
  return true;
}

//----------------------------------------------------------------------------

void TSoundInputDeviceImp::insertAllRate() {
  m_supportedRate.insert(8000);
  m_supportedRate.insert(11025);
  m_supportedRate.insert(16000);
  m_supportedRate.insert(22050);
  m_supportedRate.insert(32000);
  m_supportedRate.insert(44100);
  m_supportedRate.insert(48000);
}

//----------------------------------------------------------------------------

bool TSoundInputDeviceImp::verifyRate() {
  std::set<int>::iterator it;

  for (it = m_supportedRate.begin(); it != m_supportedRate.end(); ++it) {
    int sampleRate = *it;
    if (ioctl(m_dev, SNDCTL_DSP_SPEED, &sampleRate) == -1)
      throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                  "Failed setting the specified sample rate");
    if (sampleRate != *it) m_supportedRate.erase(*it);
  }
  if (m_supportedRate.end() == m_supportedRate.begin()) return false;

  return true;
}
//==============================================================================

class TRecordTask : public TThread::Runnable {
public:
  TSoundInputDeviceImp *m_devImp;

  TRecordTask(TSoundInputDeviceImp *devImp) : Runnable(), m_devImp(devImp){};

  ~TRecordTask(){};

  void run();
};

//------------------------------------------------------------------------------

void TRecordTask::run() {
  // N.B. e' bene che la dimensione sia piccola cosi' l'attesa perche' termini
  // e' minore in questo caso vogliamo 16 frammenti ognuno di 4096 byte
  int fraginfo = (16 << 16) | 12;
  if (ioctl(m_devImp->m_dev, SNDCTL_DSP_SETFRAGMENT, &fraginfo) == -1)
    perror("SETFRAGMENT");

  int fragsize = 0;
  if (ioctl(m_devImp->m_dev, SNDCTL_DSP_GETBLKSIZE, &fragsize) == -1)
    perror("GETFRAGMENT");
  TINT32 byteRecordedSample = 0;

  if (m_devImp->m_oneShotRecording) {
    TINT32 byteToSample =
        m_devImp->m_st->getSampleSize() * m_devImp->m_st->getSampleCount();
    char *buf = (char *)m_devImp->m_st->getRawData();

    while ((byteRecordedSample < byteToSample) && m_devImp->m_isRecording) {
      int sample;
      if (fragsize > (byteToSample - byteRecordedSample))
        sample = byteToSample - byteRecordedSample;
      else
        sample  = fragsize;
      int nread = read(m_devImp->m_dev, buf + byteRecordedSample, sample);
      if (nread == -1) break;
      if (nread != sample) break;

      byteRecordedSample += nread;
    }
  } else {
    int bytePerSample = m_devImp->m_currentFormat.m_bitPerSample >> 3;
    switch (bytePerSample) {
    case 3:
      bytePerSample++;
      break;
    default:
      break;
    }
    bytePerSample *= m_devImp->m_currentFormat.m_channelCount;

    while (m_devImp->m_isRecording) {
      char *dataBuffer = new char[fragsize];
      m_devImp->m_recordedBlocks.push_back(dataBuffer);
      m_devImp->m_samplePerBlocks.push_back(fragsize);

      int nread = read(m_devImp->m_dev, dataBuffer, fragsize);
      if (nread == -1) break;
      if (nread != fragsize) break;

      m_devImp->m_recordedSampleCount += (fragsize / bytePerSample);
    }
  }
  ioctl(m_devImp->m_dev, SNDCTL_DSP_RESET, 0);
}

//==============================================================================

TSoundInputDevice::TSoundInputDevice() : m_imp(new TSoundInputDeviceImp) {
  m_imp->m_dev = open("/dev/dsp", O_RDONLY);
  if (m_imp->m_dev < 0)
    throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice,
                                "Cannot open the dsp device");

  m_imp->insertAllRate();
  try {
    if (!m_imp->verifyRate())
      throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                  "No default samplerate are supported");
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
  m_imp->doCloseDevice();
}

//------------------------------------------------------------------------------

TSoundInputDevice::~TSoundInputDevice() {
  if (m_imp->m_dev != -1) m_imp->doCloseDevice();
  delete m_imp;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::installed() {
  bool ret = false;
  int dev  = ::open("/dev/dsp", O_RDONLY);
  if (dev >= 0) {
    ret = true;
    ::close(dev);
  }
  return ret;
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackFormat &format,
                               TSoundInputDevice::Source type) {
  m_imp->m_recordedBlocks.clear();
  m_imp->m_samplePerBlocks.clear();

  // registra creando una nuova traccia
  m_imp->m_oneShotRecording = false;
  try {
    if (m_imp->m_dev == -1) m_imp->doOpenDevice(format, type);

    if (!selectInputDevice(type))
      throw TSoundDeviceException(
          TSoundDeviceException::UnableSetDevice,
          "Input device is not supported for recording");

    TSoundTrackFormat fmt = getPreferredFormat(format);
    if (fmt != format)
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported format");
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  try {
    if (getVolume() == 0.0) {
      double volume = 5.0;
      setVolume(volume);
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  m_imp->m_currentFormat       = format;
  m_imp->m_isRecording         = true;
  m_imp->m_stopped             = false;
  m_imp->m_recordedSampleCount = 0;

  // far partire il thread
  /*TRecordThread *recordThread = new TRecordThread(m_imp);
if (!recordThread)
throw TSoundDeviceException(
TSoundDeviceException::UnablePrepare,
"Problemi per creare il thread");
recordThread->start();*/
  m_imp->m_executor.addTask(new TRecordTask(m_imp));
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackP &st,
                               TSoundInputDevice::Source type) {
  m_imp->m_recordedBlocks.clear();
  m_imp->m_samplePerBlocks.clear();

  try {
    if (m_imp->m_dev == -1) m_imp->doOpenDevice(st->getFormat(), type);

    if (!selectInputDevice(type))
      throw TSoundDeviceException(
          TSoundDeviceException::UnableSetDevice,
          "Input device is not supported for recording");

    TSoundTrackFormat fmt = getPreferredFormat(st->getFormat());
    if (fmt != st->getFormat())
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported format");

    if (getVolume() == 0.0) {
      double volume = 5.0;
      setVolume(volume);
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  // Sovrascive un'intera o parte di traccia gia' esistente
  m_imp->m_oneShotRecording    = true;
  m_imp->m_currentFormat       = st->getFormat();
  m_imp->m_isRecording         = true;
  m_imp->m_stopped             = false;
  m_imp->m_recordedSampleCount = 0;
  m_imp->m_st                  = st;

  m_imp->m_recordedBlocks.push_back((char *)st->getRawData());

  // far partire il thread
  /*TRecordThread *recordThread = new TRecordThread(m_imp);
if (!recordThread)
throw TSoundDeviceException(
TSoundDeviceException::UnablePrepare,
"Problemi per creare il thread");
recordThread->start();*/
  m_imp->m_executor.addTask(new TRecordTask(m_imp));
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundInputDevice::stop() {
  TSoundTrackP st;

  if (!m_imp->m_isRecording) return st;

  m_imp->m_isRecording = false;

  // mettere istruzioni per fermare il rec
  ioctl(m_imp->m_dev, SNDCTL_DSP_SYNC, 0);
  try {
    m_imp->doCloseDevice();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }

  // attendo 1/5 di secondo
  usleep(200000);
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

  return st;
}

//------------------------------------------------------------------------------

double TSoundInputDevice::getVolume() {
  int mixer;
  if ((mixer = openMixer()) < 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't have the mixer");

  int index;
  if ((index = getCurrentRecordSource(mixer)) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't obtain information by mixer");
  }

  int stereo;
  if (ioctl(mixer, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't obtain information by mixer");
  }

  int level;
  if (ioctl(mixer, MIXER_READ(index), &level) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Can't read the volume value");
  }

  if ((1 << index) & stereo) {
    int left  = level & 0xff;
    int right = ((level & 0xff00) >> 8);
    ::close(mixer);
    return (left + right) / 20.0;
  } else {
    ::close(mixer);
    return (level & 0xff) / 10.0;
  }
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::setVolume(double volume) {
  int mixer;
  if ((mixer = openMixer()) < 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't have the mixer");

  int caps;
  if (ioctl(mixer, SOUND_MIXER_READ_CAPS, &caps) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't obtain information by mixer");
  }

  if (!(caps & SOUND_CAP_EXCL_INPUT)) {
    int rec;
    if (ioctl(mixer, SOUND_MIXER_READ_RECMASK, &rec) == -1) {
      ::close(mixer);
      throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                  "Can't obtain information by mixer");
    }
    int i;
    int nosound = 0;
    for (i = 0; i < 32; ++i)
      if (rec & (1 << i))
        if (!writeVolume(nosound, mixer, i)) {
          ::close(mixer);
          throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                      "Can't set the volume value");
        }
  }

  int index;
  if ((index = getCurrentRecordSource(mixer)) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't obtain information by mixer");
  }

  int stereo;
  if (ioctl(mixer, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Can't obtain information by mixer");
  }

  int vol;
  if ((1 << index) & stereo) {
    volume *= 10.0;
    vol = (int)volume + ((int)(volume * 256.0));
  } else
    vol = (int)(volume * 10.0);

  if (!writeVolume(vol, mixer, index)) {
    ::close(mixer);
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Can't write the volume value");
  }
  ::close(mixer);
  return true;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::isRecording() { return m_imp->m_isRecording; }

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                        int channelCount,
                                                        int bitPerSample) {
  TSoundTrackFormat fmt;
  int status;

  if (bitPerSample <= 8) {
    bitPerSample       = AFMT_U8;
    fmt.m_signedSample = false;
  } else if ((bitPerSample > 8 && bitPerSample < 16) || bitPerSample >= 16) {
    bitPerSample       = AFMT_S16_NE;
    fmt.m_signedSample = true;
  }

  status = ioctl(m_imp->m_dev, SNDCTL_DSP_SETFMT, &bitPerSample);
  if (status == -1)
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "Failed setting the specified number of bits");
  fmt.m_bitPerSample = bitPerSample;

  status = ioctl(m_imp->m_dev, SNDCTL_DSP_CHANNELS, &channelCount);
  if (status == -1)
    throw TSoundDeviceException(
        TSoundDeviceException::UnablePrepare,
        "Failed setting the specified number of channel");
  fmt.m_channelCount = channelCount;

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

  if (ioctl(m_imp->m_dev, SNDCTL_DSP_SPEED, &sampleRate) == -1)
    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                "Failed setting the specified sample rate");
  fmt.m_sampleRate = sampleRate;

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

//******************************************************************************
//******************************************************************************
//						funzioni per l'interazione con
// la
// libreria
// OSS
//******************************************************************************
//******************************************************************************
namespace {
string parseError(int error) {
  switch (error) {
  case EBADF:
    return string("Bad file descriptor");
  case EFAULT:
    return string("Pointer to/from buffer data is invalid");
  case EINTR:
    return string("Signal interrupt the signal");
  case EINVAL:
    return string("Request/arg isn't valid for this device");
  case EIO:
    return string("Some phisical I/O error has occurred");
  case ENOTTY:
    return string(
        "Fieldes isn't associated with a device that accepts control");
  case ENXIO:
    return string(
        "Request/arg  valid, but the requested cannot be performed on this "
        "subdevice");
  default:
    return string("Unknown error");
    break;
  }
}

//------------------------------------------------------------------------------

TSoundInputDevice::Source stringToSource(string dev) {
  if (dev == "mic")
    return TSoundInputDevice::Mic;
  else if (dev == "line")
    return TSoundInputDevice::LineIn;
  else if (dev == "cd")
    return TSoundInputDevice::CdAudio;
  else
    return TSoundInputDevice::DigitalIn;
}

//------------------------------------------------------------------------------

string sourceToString(TSoundInputDevice::Source dev) {
  switch (dev) {
  case TSoundInputDevice::Mic:
    return string("mic");
  case TSoundInputDevice::LineIn:
    return string("line");
  case TSoundInputDevice::DigitalIn:
    return string("digital");
  default:
    return string("cd");
  }
}

//------------------------------------------------------------------------------

int openMixer() {
  int mixer = open("/dev/mixer", O_RDWR);
  if (mixer == -1) return false;
  return mixer;
}
//------------------------------------------------------------------------------

int getCurrentRecordSource(int mixer) {
  int recsrc;
  if (ioctl(mixer, SOUND_MIXER_READ_RECSRC, &recsrc) == -1) return -1;
  int index = -1;
  for (index = 0; index < 32; ++index)
    if (recsrc & 1 << index) break;
  return index;
}
//------------------------------------------------------------------------------

bool writeVolume(int volume, int mixer, int indexDev) {
  if (ioctl(mixer, MIXER_WRITE(indexDev), &volume) == -1) return false;
  return true;
}

//------------------------------------------------------------------------------

bool controlEnableRecord(int mixer) {
  int recmask;
  if (ioctl(mixer, SOUND_MIXER_READ_RECMASK, &recmask) == -1) {
    perror("Read recmask");
    return false;
  }
  if (recmask & (1 << SOUND_MIXER_IGAIN)) {
    int volume;
    if (ioctl(mixer, MIXER_READ(SOUND_MIXER_IGAIN), &volume) == -1)
      return false;

    int app = (volume & 0xff);
    if (app <= 30) {
      volume = 80 | 80 << 8;
      if (!writeVolume(volume, mixer, SOUND_MIXER_IGAIN)) return false;
    }
  }
  return true;
}

//------------------------------------------------------------------------------

int isInputDeviceSupported(TSoundInputDevice::Source dev, int &mixer) {
  int recmask;
  if (ioctl(mixer, SOUND_MIXER_READ_RECMASK, &recmask) == -1) {
    perror("Read recmask");
    return -1;
  }

  int i;
  string devS              = sourceToString(dev);
  const char *deviceName[] = SOUND_DEVICE_NAMES;
  for (i = 0; i < 32; ++i) {
    if (!(recmask & 1 << i)) continue;
    if (strcmp(devS.c_str(), deviceName[i]) == 0) return i;
  }
  return -1;
}

//------------------------------------------------------------------------------

bool selectInputDevice(TSoundInputDevice::Source dev) {
  int mixer;
  if ((mixer = openMixer()) < 0) {
    close(mixer);
    return false;  // throw TException("Can't open the mixer device");
  }
  int index = isInputDeviceSupported(dev, mixer);
  if (index == -1) return false;
  int recsrc;
  if (ioctl(mixer, SOUND_MIXER_READ_RECSRC, &recsrc) == -1) {
    perror("Read recsrc");
    close(mixer);
    return false;
  }
  if (!(recsrc & 1 << index)) {
    recsrc = 1 << index;
    if (ioctl(mixer, SOUND_MIXER_WRITE_RECSRC, &recsrc) == -1) {
      perror("Write recsrc");
      ::close(mixer);
      return false;
    }
  }

  if (!controlEnableRecord(mixer)) {
    close(mixer);
    return false;  // throw TException("Can't enable recording");
  }
  ::close(mixer);
  return true;
}
}
