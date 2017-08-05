

#include "tsound.h"

#ifndef TNZCORE_LIGHT
#include "tthread.h"
#endif
#include "tsop.h"
#include <set>
#include "tsystem.h"

#include <mmsystem.h>

//=========================================================

// forward declarations
class TSoundOutputDeviceImp;
class TSoundInputDeviceImp;

//=========================================================

namespace {
void CALLBACK recordCB(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1,
                       DWORD dwParam2);

bool setRecordLine(TSoundInputDevice::Source typeInput);

MMRESULT getLineInfo(HMIXEROBJ hMixer, DWORD dwComponentType, MIXERLINE &mxl);

MMRESULT getLineControl(MIXERCONTROL &mxc, HMIXEROBJ hMixer, DWORD dwLineID,
                        DWORD dwControlType);

MMRESULT setControlDetails(HMIXEROBJ hMixer, DWORD dwSelectControlID,
                           DWORD dwMultipleItems,
                           MIXERCONTROLDETAILS_UNSIGNED *mxcdSelectValue);

MMRESULT getControlDetails(HMIXEROBJ hMixer, DWORD dwSelectControlID,
                           DWORD dwMultipleItems,
                           MIXERCONTROLDETAILS_UNSIGNED *mxcdSelectValue);

MMRESULT isaFormatSupported(int sampleRate, int channelCount, int bitPerSample,
                            bool input);

DWORD WINAPI MyWaveOutCallbackThread(LPVOID lpParameter);
void getAmplitude(int &amplitude, const TSoundTrackP st, TINT32 sample);
}

//==============================================================================
//     Class to send the message that a playback is completed
//==============================================================================
#ifndef TNZCORE_LIGHT
class EndPlayMsg final : public TThread::Message {
public:
  EndPlayMsg(TSoundOutputDeviceListener *notifier) { m_listener = notifier; }

  TThread::Message *clone() const override { return new EndPlayMsg(*this); }

  void onDeliver() override { m_listener->onPlayCompleted(); }

private:
  TSoundOutputDeviceListener *m_listener;
};
#endif

/*
static int makeDWORD(const short lo, const short hi) {
  int dw = hi << 16;
  dw |= lo;
  return dw;
}
*/

//==============================================================================
class WavehdrQueue;

class TSoundOutputDeviceImp {
public:
  HWAVEOUT m_wout;
  WavehdrQueue *m_whdrQueue;
  TSoundTrackFormat m_currentFormat;
  std::set<int> m_supportedRate;

  TThread::Mutex m_mutex;

  bool m_stopped;
  bool m_isPlaying;
  bool m_looped;
  bool m_scrubbing;

  std::set<TSoundOutputDeviceListener *> m_listeners;
  DWORD m_notifyThreadId;

  HANDLE m_closeDevice;

  TSoundOutputDeviceImp();
  ~TSoundOutputDeviceImp();

  bool doOpenDevice(const TSoundTrackFormat &format);
  bool doPlay(WAVEHDR *whdr, const TSoundTrackFormat format);
  bool doCloseDevice();

  bool verifyRate();
  void insertAllRate();
};

//==============================================================================

class WavehdrQueue {
public:
  WavehdrQueue(TSoundOutputDeviceImp *devImp, int slotCount)
      : m_devImp(devImp)
      , m_items()
      , m_queuedItems()
      , m_slotCount(slotCount)
      , m_mutex()
      , m_lastOffset(0) {}

  ~WavehdrQueue() {}

  void put(TSoundTrackP &subTrack);
  WAVEHDR *get();
  bool popFront(int count);
  void pushBack(WAVEHDR *whdr, TSoundTrackP st);
  int size();
  void clear();
  bool isAllQueuedItemsPlayed();

private:
  std::list<std::pair<WAVEHDR *, TSoundTrackP>> m_items;
  std::list<WAVEHDR *> m_queuedItems;

  TThread::Mutex m_mutex;
  int m_slotCount;
  int m_lastOffset;
  TSoundOutputDeviceImp *m_devImp;
  TSoundTrackP m_lastTrack;
};

//==============================================================================

static WAVEHDR *prepareWaveHeader(HWAVEOUT wout, const TSoundTrackP &subTrack,
                                  ULONG &count) {
  WAVEHDR *whdr = new WAVEHDR;
  memset(whdr, 0, sizeof(WAVEHDR));
  whdr->dwBufferLength = subTrack->getSampleSize() * subTrack->getSampleCount();
  whdr->lpData         = new char[whdr->dwBufferLength];
  whdr->dwFlags        = 0;
  whdr->dwUser         = count;
  memcpy(whdr->lpData, subTrack->getRawData(), whdr->dwBufferLength);

  MMRESULT ret = waveOutPrepareHeader(wout, whdr, sizeof(WAVEHDR));
  if (ret != MMSYSERR_NOERROR) {
    delete[] whdr->lpData;
    delete whdr;
    return 0;
  }
  ++count;
  return whdr;
}

//==============================================================================

void WavehdrQueue::put(TSoundTrackP &subTrack) {
  assert(subTrack->getRawData());
  static ULONG count = 1;

  // codice messo per tab: facendo il play al rilascio del mouse e su piu'
  // colonne in cui le traccie potrebbe avere diversi formati siccome qui in
  // alcune situazioni si fa subito waveOutWrite c'e' bisogno di controllare
  // se il formato con cui e' stato aperto in precedenza il device e' uguale
  // a quello della traccia
  if (m_devImp->m_wout && m_devImp->m_currentFormat != subTrack->getFormat()) {
    m_devImp->doCloseDevice();
    TSystem::sleep(300);
    m_devImp->doOpenDevice(subTrack->getFormat());
  }

  TThread::MutexLocker sl(&m_mutex);
  if (!m_devImp->m_scrubbing) {
    WAVEHDR *whdr2 = 0;

    // traccia
    whdr2 = prepareWaveHeader(m_devImp->m_wout, subTrack, count);
    getAmplitude(m_lastOffset, subTrack, subTrack->getSampleCount() - 1L);

    MMRESULT ret = MMSYSERR_NOERROR;

    if (whdr2 && whdr2->dwFlags & WHDR_PREPARED) {
      ret = waveOutWrite(m_devImp->m_wout, whdr2, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr2, subTrack);
        getAmplitude(m_lastOffset, subTrack, subTrack->getSampleCount() - 1L);
        TThread::MutexLocker sl(&m_devImp->m_mutex);
        m_devImp->m_isPlaying = true;
        m_devImp->m_stopped   = false;
      }
    }
    return;
  }

  if (m_queuedItems.size() == 0) {
    WAVEHDR *whdr1 = 0;
    WAVEHDR *whdr2 = 0;
    WAVEHDR *whdr3 = 0;

    MMRESULT ret;
    TSoundTrackP riseTrack, decayTrack;
    int sampleSize = subTrack->getSampleSize();

    // cresce
    riseTrack = TSop::fadeIn(subTrack, 0.9);
    whdr1     = prepareWaveHeader(m_devImp->m_wout, riseTrack, count);

    // traccia
    whdr2 = prepareWaveHeader(m_devImp->m_wout, subTrack, count);

    getAmplitude(m_lastOffset, subTrack, subTrack->getSampleCount() - 1L);

    // decresce
    decayTrack = 0;
    if (m_lastOffset) {
      decayTrack = TSop::fadeOut(subTrack, 0.9);
      whdr3      = prepareWaveHeader(m_devImp->m_wout, decayTrack, count);
    }

    if (whdr1 && (whdr1->dwFlags & WHDR_PREPARED)) {
      ret = waveOutWrite(m_devImp->m_wout, whdr1, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr1, riseTrack);
        getAmplitude(m_lastOffset, riseTrack, riseTrack->getSampleCount() - 1L);
      }
    }
    if (whdr2 && (whdr2->dwFlags & WHDR_PREPARED)) {
      ret = waveOutWrite(m_devImp->m_wout, whdr2, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr2, subTrack);
        getAmplitude(m_lastOffset, subTrack, subTrack->getSampleCount() - 1L);
        TThread::MutexLocker sl(&m_devImp->m_mutex);
        m_devImp->m_isPlaying = true;
        m_devImp->m_stopped   = false;
      }
    }
    if (whdr3 && (whdr3->dwFlags & WHDR_PREPARED)) {
      ret = waveOutWrite(m_devImp->m_wout, whdr3, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr3, decayTrack);
        if (decayTrack->isSampleSigned())
          m_lastOffset = 0;
        else
          m_lastOffset = 127;
      }
    }
    return;
  }

  if (m_queuedItems.size() < 10) {
    WAVEHDR *whdr1 = 0;
    WAVEHDR *whdr2 = 0;
    WAVEHDR *whdr  = new WAVEHDR;
    memset(whdr, 0, sizeof(WAVEHDR));
    whdr->dwBufferLength =
        subTrack->getSampleSize() * subTrack->getSampleCount();
    whdr->lpData  = new char[whdr->dwBufferLength];
    whdr->dwFlags = 0;
    memcpy(whdr->lpData, subTrack->getRawData(), whdr->dwBufferLength);
    int sampleSize         = subTrack->getSampleSize();
    TSoundTrackP riseTrack = 0;

    if (m_lastOffset)  /// devo fare ilcross fade
    {
      int offset;
      getAmplitude(offset, subTrack, 0L);

      offset = m_lastOffset - offset;
      if (offset) {
        TSoundTrackP st = TSop::crossFade(m_lastTrack, subTrack, 0.3);
        memcpy(whdr->lpData, st->getRawData(),
               st->getSampleCount() * sampleSize);
      }
    } else  // e' zero ma ne metto uno che cresce faccio il fadeIn
    {
      riseTrack = TSop::fadeIn(subTrack, 0.3);
      whdr1     = prepareWaveHeader(m_devImp->m_wout, riseTrack, count);
    }

    whdr->dwUser = count;
    ++count;
    MMRESULT ret =
        waveOutPrepareHeader(m_devImp->m_wout, whdr, sizeof(WAVEHDR));
    if (ret != MMSYSERR_NOERROR) {
      delete[] whdr->lpData;
      delete whdr;
      return;
    }

    getAmplitude(m_lastOffset, subTrack, subTrack->getSampleCount() - 1L);

    TSoundTrackP decayTrack = 0;
    if (m_queuedItems.size() <= 7) {
      if (m_lastOffset)  // devo fare il fadeOut
      {
        decayTrack = TSop::fadeOut(subTrack, 0.3);
        whdr2      = prepareWaveHeader(m_devImp->m_wout, decayTrack, count);
      }
    }

    if (whdr1 && whdr1->dwFlags & WHDR_PREPARED) {
      ret = waveOutWrite(m_devImp->m_wout, whdr1, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr1, riseTrack);
        getAmplitude(m_lastOffset, riseTrack, riseTrack->getSampleCount() - 1L);
      }
    }

    if (whdr && whdr->dwFlags & WHDR_PREPARED) {
      ret = waveOutWrite(m_devImp->m_wout, whdr, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr, subTrack);
        getAmplitude(m_lastOffset, subTrack, subTrack->getSampleCount() - 1L);
        {
          TThread::MutexLocker sl(&m_devImp->m_mutex);
          m_devImp->m_isPlaying = true;
          m_devImp->m_stopped   = false;
        }
      }
    }

    if (whdr2 && whdr2->dwFlags & WHDR_PREPARED) {
      ret = waveOutWrite(m_devImp->m_wout, whdr2, sizeof(WAVEHDR));
      if (ret == MMSYSERR_NOERROR) {
        pushBack(whdr2, decayTrack);
        if (decayTrack->isSampleSigned())
          m_lastOffset = 0;
        else
          m_lastOffset = 127;
      }
    }
    return;
  }

  if ((int)m_items.size() == m_slotCount) {
    WAVEHDR *item = m_items.front().first;
    MMRESULT ret =
        waveOutUnprepareHeader(m_devImp->m_wout, item, sizeof(WAVEHDR));

    if (ret == MMSYSERR_NOERROR) {
      delete[] item->lpData;
      delete item;
    }

    m_items.pop_front();

    if (m_queuedItems.size() != 0) {
      WAVEHDR *item  = m_items.front().first;
      int sampleSize = m_items.front().second->getSampleSize();
      int offset;
      getAmplitude(offset, m_items.front().second, 0L);

      offset = m_lastOffset - offset;
      if (offset) {
        TSoundTrackP st =
            TSop::crossFade(m_lastTrack, m_items.front().second, 0.3);
        memcpy(item->lpData, st->getRawData(),
               st->getSampleCount() * sampleSize);
      }
    }
  }

  WAVEHDR *whdr = prepareWaveHeader(m_devImp->m_wout, subTrack, count);

  assert(whdr && whdr->dwFlags & WHDR_PREPARED);
  m_items.push_back(std::make_pair(whdr, subTrack));
  assert((int)m_items.size() <= m_slotCount);
}

//----------------------------------------------------------------------------

// restituisce il piu' vecchio WAVEHDR il cui stato e' prepared && !done
WAVEHDR *WavehdrQueue::get() {
  TThread::MutexLocker sl(&m_mutex);
  if (m_items.size() == 0) return 0;

  WAVEHDR *whdr = m_items.front().first;
  assert(whdr->dwFlags & WHDR_PREPARED);

  pushBack(whdr, m_items.front().second);
  getAmplitude(m_lastOffset, m_items.front().second,
               m_items.front().second->getSampleCount() - 1L);
  m_items.pop_front();

  return whdr;
}

//-----------------------------------------------------------------------------

// rimuove dalla coda il piu' vecchio WAVEHDR il cui stato e' done
bool WavehdrQueue::popFront(int count) {
  TThread::MutexLocker sl(&m_mutex);
  // assert(m_queuedItems.size() > 0);
  if (m_queuedItems.size() <= 0) return false;
  WAVEHDR *whdr = m_queuedItems.front();
  // controllo introdotto pr via che su win2k si perde alcune
  // notifiche di WHDR_DONE
  while ((DWORD)count > whdr->dwUser) {
    MMRESULT ret =
        waveOutUnprepareHeader(m_devImp->m_wout, whdr, sizeof(WAVEHDR));
    if (ret == MMSYSERR_NOERROR) {
      m_queuedItems.pop_front();
      delete[] whdr->lpData;
      delete whdr;
      whdr = m_queuedItems.front();
    }
  }
  assert(whdr->dwFlags & WHDR_DONE);
  m_queuedItems.pop_front();
  delete[] whdr->lpData;
  delete whdr;
  return true;
}

//-----------------------------------------------------------------------------

void WavehdrQueue::pushBack(WAVEHDR *whdr, TSoundTrackP st) {
  TThread::MutexLocker sl(&m_mutex);
  m_queuedItems.push_back(whdr);
  m_lastTrack = st;
}

//-----------------------------------------------------------------------------

int WavehdrQueue::size() {
  TThread::MutexLocker sl(&m_mutex);
  int size = m_queuedItems.size();
  return size;
}

//-----------------------------------------------------------------------------

void WavehdrQueue::clear() {
  TThread::MutexLocker sl(&m_mutex);
  m_items.clear();
  m_lastTrack = TSoundTrackP();
  std::list<WAVEHDR *>::iterator it;
  for (it = m_queuedItems.begin(); it != m_queuedItems.end(); it++) {
    WAVEHDR *wvhdr = *it;
    delete[] wvhdr->lpData;
    delete wvhdr;
  }
  m_queuedItems.clear();
}

//-----------------------------------------------------------------------------

bool WavehdrQueue::isAllQueuedItemsPlayed() {
  std::list<WAVEHDR *>::iterator it;
  bool finished = true;
  for (it = m_queuedItems.begin(); it != m_queuedItems.end(); it++) {
    WAVEHDR *wvhdr = *it;
    finished       = finished && (wvhdr->dwFlags & WHDR_DONE);
  }
  return finished;
}

//==============================================================================

TSoundOutputDeviceImp::TSoundOutputDeviceImp()
    : m_stopped(true)
    , m_isPlaying(false)
    , m_looped(false)
    , m_scrubbing(false)
    , m_wout(0) {
  m_whdrQueue = new WavehdrQueue(this, 4);

  insertAllRate();
  if (!verifyRate())
    throw TSoundDeviceException(TSoundDeviceException::FailedInit,
                                "Unable to verify supported rates");

  m_closeDevice = CreateEvent(NULL,   // no security attributes
                              FALSE,  // auto-reset event
                              FALSE,  // initial state is not signaled
                              NULL);  // object not named
}

//----------------------------------------------------------------------------

TSoundOutputDeviceImp::~TSoundOutputDeviceImp() { delete m_whdrQueue; }

//----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doOpenDevice(const TSoundTrackFormat &format) {
  WAVEFORMATEX wf;
  wf.wFormatTag      = WAVE_FORMAT_PCM;
  wf.nChannels       = format.m_channelCount;
  wf.nSamplesPerSec  = format.m_sampleRate;
  wf.wBitsPerSample  = format.m_bitPerSample;
  wf.nBlockAlign     = (wf.nChannels * wf.wBitsPerSample) >> 3;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
  wf.cbSize          = 0;

  TThread::MutexLocker sl(&m_mutex);
  CloseHandle(CreateThread(NULL, 0, MyWaveOutCallbackThread, (LPVOID)this, 0,
                           &m_notifyThreadId));

  MMRESULT ret;
  if ((ret = waveOutOpen(&m_wout, WAVE_MAPPER, &wf, (DWORD)m_notifyThreadId,
                         (DWORD)this, CALLBACK_THREAD)) != MMSYSERR_NOERROR) {
    while (!PostThreadMessage(m_notifyThreadId, WM_QUIT, 0, 0))
      ;
  }
  if (ret != MMSYSERR_NOERROR) return false;

  if (ret != MMSYSERR_NOERROR) return false;
  m_currentFormat = format;
  return (ret == MMSYSERR_NOERROR);
}

//----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doPlay(WAVEHDR *whdr,
                                   const TSoundTrackFormat format) {
  TThread::MutexLocker sl(&m_mutex);
  if (!m_wout || (m_wout && m_currentFormat != format)) doOpenDevice(format);

  MMRESULT ret;
  ret = waveOutWrite(m_wout, whdr, sizeof(WAVEHDR));
  if (ret != MMSYSERR_NOERROR) return false;
  m_stopped   = false;
  m_isPlaying = true;

  return true;
}

//----------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doCloseDevice() {
  TThread::MutexLocker sl(&m_mutex);
  if (m_wout) {
    MMRESULT ret = waveOutReset(m_wout);
    if (ret != MMSYSERR_NOERROR) return false;
    ret = waveOutClose(m_wout);
    if (ret != MMSYSERR_NOERROR) return false;
    m_wout      = 0;
    m_stopped   = true;
    m_isPlaying = false;
    m_looped    = false;
  }

  PostThreadMessage(m_notifyThreadId, WM_QUIT, 0, 0);
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

  for (it = m_supportedRate.begin(); it != m_supportedRate.end();) {
    MMRESULT ret;
    WAVEFORMATEX wf;

    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = *it;
    wf.wBitsPerSample  = 8;
    wf.nBlockAlign     = (wf.nChannels * wf.wBitsPerSample) >> 3;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize          = 0;

    ret = waveOutOpen(NULL, WAVE_MAPPER, &wf, NULL, NULL, WAVE_FORMAT_QUERY);

    if (ret == MMSYSERR_NOERROR) {
      ++it;
      continue;
    }
    if (ret == WAVERR_BADFORMAT)
      it = m_supportedRate.erase(it);
    else
      return false;
  }
  if (m_supportedRate.end() == m_supportedRate.begin()) return false;

  return true;
}

//==============================================================================

TSoundOutputDevice::TSoundOutputDevice() : m_imp(new TSoundOutputDeviceImp()) {}

//------------------------------------------------------------------------------

TSoundOutputDevice::~TSoundOutputDevice() {
  close();
  WaitForSingleObject(m_imp->m_closeDevice, INFINITE);
  CloseHandle(m_imp->m_closeDevice);
}

//------------------------------------------------------------------------------

namespace {
DWORD WINAPI MyWaveOutCallbackThread(LPVOID lpParameter) {
  TSoundOutputDeviceImp *devImp = (TSoundOutputDeviceImp *)lpParameter;
  if (!devImp) return 0;

  MSG msg;
  BOOL bRet;
  while (bRet = GetMessage(&msg, NULL, 0, 0)) {
    if (bRet == -1) {
      // si e' verificato un errore
      break;
    }

    switch (msg.message) {
    case MM_WOM_DONE: {
      WAVEHDR *pWaveHdr = ((WAVEHDR *)msg.lParam);
      {
        TThread::MutexLocker sl(&devImp->m_mutex);
        if (devImp->m_looped) {
          devImp->doPlay(pWaveHdr, devImp->m_currentFormat);
          continue;
        }
      }

      WAVEHDR *whdr = 0;
      if (devImp->m_whdrQueue->popFront(pWaveHdr->dwUser)) {
        whdr = devImp->m_whdrQueue->get();
        if (whdr) devImp->doPlay(whdr, devImp->m_currentFormat);

        WaitForSingleObject(devImp->m_closeDevice, INFINITE);
        CloseHandle(devImp->m_closeDevice);

        MMRESULT ret =
            waveOutUnprepareHeader(devImp->m_wout, pWaveHdr, sizeof(WAVEHDR));
        if (ret == MMSYSERR_NOERROR) {
          delete pWaveHdr->lpData;
          delete pWaveHdr;
        }
      }

      if (!whdr && devImp->m_whdrQueue->size() == 0) {
        std::set<TSoundOutputDeviceListener *>::iterator it =
            devImp->m_listeners.begin();
        for (; it != devImp->m_listeners.end(); ++it) {
#ifndef TNZCORE_LIGHT
          EndPlayMsg *event = new EndPlayMsg(*it);
          event->send();
#else
          assert(false);
#endif
        }
        devImp->doCloseDevice();
      }

      continue;
    }

    case MM_WOM_CLOSE:
      break;

    default:
      continue;
    }
  }

  SetEvent(devImp->m_closeDevice);
  return 0;
}

void getAmplitude(int &amplitude, const TSoundTrackP st, TINT32 sample) {
  TSoundTrackP snd = st;
  amplitude        = 0;
  int k            = 0;
  for (k = 0; k < snd->getChannelCount(); ++k)
    amplitude += (int)snd->getPressure(sample, k);
  amplitude /= k;
}
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::installed() {
  int ndev = waveOutGetNumDevs();
  if (ndev <= 0) return false;
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::open(const TSoundTrackP &st) {
  return m_imp->doOpenDevice(st->getFormat());
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
  TSoundTrackFormat format;
  TSoundTrackP subTrack;

  {
    TThread::MutexLocker sl(&m_imp->m_mutex);
    if (m_imp->m_looped)
      throw TSoundDeviceException(
          TSoundDeviceException::Busy,
          "Unable to queue another playback when the sound player is looping");

    format = st->getFormat();
    try {
      TSoundTrackFormat fmt = getPreferredFormat(format);
      if (fmt != format) {
        throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                    "Unsupported Format");
      }
    } catch (TSoundDeviceException &e) {
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  e.getMessage());
    }

    assert(s1 >= s0);
    subTrack           = st->extract(s0, s1);
    m_imp->m_looped    = loop;
    m_imp->m_scrubbing = scrubbing;
  }

  if (!m_imp->m_wout) m_imp->doOpenDevice(format);

  m_imp->m_whdrQueue->put(subTrack);
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
  if ((m_imp->m_wout) && m_imp->m_isPlaying) {
    MMRESULT ret = waveOutReset(m_imp->m_wout);
    if (ret != MMSYSERR_NOERROR) return;

    TThread::MutexLocker sl(&m_imp->m_mutex);
    m_imp->m_looped    = false;
    m_imp->m_stopped   = true;
    m_imp->m_isPlaying = false;
    m_imp->m_whdrQueue->clear();
  }
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::close() { return m_imp->doCloseDevice(); }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const { return m_imp->m_isPlaying; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isAllQueuedItemsPlayed() {
  return m_imp->m_whdrQueue->isAllQueuedItemsPlayed();
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() {
  TThread::MutexLocker sl(&m_imp->m_mutex);
  return m_imp->m_looped;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) {
  TThread::MutexLocker sl(&m_imp->m_mutex);
  m_imp->m_looped = loop;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                         int channelCount,
                                                         int bitPerSample) {
  TSoundTrackFormat fmt;

  // avvvicinarsi al sample rate => dovrebbe esser OK avendo selezionato i piu'
  // vicini
  std::set<int>::iterator it = m_imp->m_supportedRate.lower_bound(sampleRate);
  if (it == m_imp->m_supportedRate.end()) {
    it = std::max_element(m_imp->m_supportedRate.begin(),
                          m_imp->m_supportedRate.end());
    if (it != m_imp->m_supportedRate.end())
      sampleRate = *(m_imp->m_supportedRate.rbegin());
    else
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "There isn't a supported format");
  } else
    sampleRate = *it;

  if (bitPerSample <= 8)
    bitPerSample = 8;
  else if ((bitPerSample > 8 && bitPerSample < 16) || bitPerSample >= 16)
    bitPerSample = 16;

  if (bitPerSample >= 16)
    fmt.m_signedSample = true;
  else
    fmt.m_signedSample = false;

  // switch mono/stereo
  if (channelCount <= 1)
    channelCount = 1;
  else
    channelCount = 2;

  fmt.m_bitPerSample = bitPerSample;
  fmt.m_channelCount = channelCount;
  fmt.m_sampleRate   = sampleRate;

  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  try {
    return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                              format.m_bitPerSample);
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }
}

//==============================================================================
//==============================================================================
// Classi per la gestione della registrazione
//==============================================================================
//==============================================================================

//==============================================================================

class WaveFormat final : public WAVEFORMATEX {
public:
  WaveFormat(){};
  WaveFormat(unsigned char channelCount, TUINT32 sampleRate,
             unsigned char bitPerSample);

  ~WaveFormat(){};
};

WaveFormat::WaveFormat(unsigned char channelCount, TUINT32 sampleRate,
                       unsigned char bitPerSample) {
  wFormatTag      = WAVE_FORMAT_PCM;
  nChannels       = channelCount;
  nSamplesPerSec  = sampleRate;
  wBitsPerSample  = bitPerSample;
  nBlockAlign     = (channelCount * bitPerSample) >> 3;
  nAvgBytesPerSec = nBlockAlign * sampleRate;
  cbSize          = 0;
}

//==============================================================================

class WinSoundInputDevice {
public:
  WinSoundInputDevice();
  ~WinSoundInputDevice();

  void open(const WaveFormat &wf);
  void close();

  void prepareHeader(char *sampleBuffer, TUINT32 sampleBufferSize,
                     WAVEHDR &whdr);

  void unprepareHeader(WAVEHDR &whdr);

  void addBlock(WAVEHDR &whdr);

  void start();

  void reset();
  void stop();

  HANDLE m_hBlockDone;

private:
  HWAVEIN m_hWaveIn;
};

//--------------------------------------------------------------------

WinSoundInputDevice::WinSoundInputDevice() : m_hWaveIn(0) {
  m_hBlockDone = CreateEvent(NULL,   // no security attributes
                             FALSE,  // auto-reset event
                             FALSE,  // initial state is not signaled
                             NULL);  // object not named
}

//--------------------------------------------------------------------

WinSoundInputDevice::~WinSoundInputDevice() { CloseHandle(m_hBlockDone); }

//--------------------------------------------------------------------

void WinSoundInputDevice::open(const WaveFormat &wf) {
  if (m_hWaveIn) close();

  MMRESULT ret = waveInOpen(&m_hWaveIn, WAVE_MAPPER, &wf, (DWORD)recordCB,
                            (DWORD)m_hBlockDone, CALLBACK_FUNCTION);

  if (ret != MMSYSERR_NOERROR) {
    throw TException("Error to open the input device");
  }
}

//--------------------------------------------------------------------

void WinSoundInputDevice::close() {
  if (!m_hWaveIn) return;

  MMRESULT ret = waveInClose(m_hWaveIn);

  if (ret != MMSYSERR_NOERROR) {
    throw TException("Error to close the input device");
  }
  m_hWaveIn = 0;
}

//--------------------------------------------------------------------

void WinSoundInputDevice::prepareHeader(char *sampleBuffer,
                                        TUINT32 sampleBufferSize,
                                        WAVEHDR &whdr) {
  whdr.lpData         = sampleBuffer;
  whdr.dwBufferLength = sampleBufferSize;  // numero di byte
  whdr.dwFlags        = 0;
  whdr.dwLoops        = 0;

  MMRESULT ret = waveInPrepareHeader(m_hWaveIn, &whdr, sizeof(WAVEHDR));
  if (ret != MMSYSERR_NOERROR) {
    throw TException("Unable to prepare a wave header");
  }
}

//--------------------------------------------------------------------

void WinSoundInputDevice::unprepareHeader(WAVEHDR &whdr) {
  MMRESULT ret = waveInUnprepareHeader(m_hWaveIn, &whdr, sizeof(WAVEHDR));
  if (ret != MMSYSERR_NOERROR) {
    throw TException("Unable to unprepare a wave header");
  }
}

//--------------------------------------------------------------------

void WinSoundInputDevice::addBlock(WAVEHDR &whdr) {
  MMRESULT ret = waveInAddBuffer(m_hWaveIn, &whdr, sizeof(WAVEHDR));
  if (ret != MMSYSERR_NOERROR) {
    throw TException("Unable to add a waveheader");
  }
}

//--------------------------------------------------------------------

void WinSoundInputDevice::start() {
  int ret = waveInStart(m_hWaveIn);
  if (ret != MMSYSERR_NOERROR) {
    throw TException("Unable to add a waveheader");
  }
}

//--------------------------------------------------------------------

void WinSoundInputDevice::reset() {
  if (!m_hWaveIn) return;

  MMRESULT ret = waveInReset(m_hWaveIn);
  if (ret != MMSYSERR_NOERROR) {
    throw TException("Unable to add a waveheader");
  }
}

//--------------------------------------------------------------------

void WinSoundInputDevice::stop() {
  MMRESULT ret = waveInStop(m_hWaveIn);
  if (ret != MMSYSERR_NOERROR) {
    throw TException("Unable to add a waveheader");
  }
}

//====================================================================

#ifndef TNZCORE_LIGHT
class RecordTask final : public TThread::Runnable {
public:
  RecordTask(std::shared_ptr<TSoundInputDeviceImp> dev)
      : Runnable(), m_dev(std::move(dev)) {}

  ~RecordTask() {}

  void run() override;

  std::shared_ptr<TSoundInputDeviceImp> m_dev;
};

#endif

//====================================================================

class TSoundInputDeviceImp final : public WinSoundInputDevice {
public:
  bool m_allocateBuff;
  bool m_isRecording;
  bool m_supportVolume;
  int m_index;
  TINT32 m_byteRecorded;

  TSoundTrackP m_st;
  TSoundTrackFormat m_format;

#ifndef TNZCORE_LIGHT
  TThread::Executor m_executor;
#endif
  std::vector<WAVEHDR> m_whdr;
  std::vector<char *> m_recordedBlocks;
  std::set<int> m_supportedRate;

  HANDLE m_hLastBlockDone;

  TSoundInputDeviceImp();
  ~TSoundInputDeviceImp();

  void insertAllRate();
  bool verifyRate();
};

//------------------------------------------------------------------------------

TSoundInputDeviceImp::TSoundInputDeviceImp()
    : m_allocateBuff(false)
    , m_isRecording(false)
    , m_supportVolume(false)
    , m_index(0)
    , m_byteRecorded(0)
    , m_format()
    , m_whdr(3)
    , m_recordedBlocks()
    , m_supportedRate() {
  m_hLastBlockDone = CreateEvent(NULL,   // no security attributes
                                 FALSE,  // is manual-reset event?
                                 FALSE,  // is signaled?
                                 NULL);  // object not named
}

//------------------------------------------------------------------------------

TSoundInputDeviceImp::~TSoundInputDeviceImp() {
  if (m_isRecording) {
    try {
      reset();
      WaitForSingleObject(m_hLastBlockDone, INFINITE);
      int i;
      for (i = 0; i < (int)m_recordedBlocks.size(); ++i)
        delete[] m_recordedBlocks[i];
      close();
    } catch (TException &) {
    }
  }
  CloseHandle(m_hLastBlockDone);
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

  for (it = m_supportedRate.begin(); it != m_supportedRate.end();) {
    MMRESULT ret;
    WAVEFORMATEX wf;

    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = *it;
    wf.wBitsPerSample  = 8;
    wf.nBlockAlign     = (wf.nChannels * wf.wBitsPerSample) >> 3;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize          = 0;

    ret = waveInOpen(NULL, WAVE_MAPPER, &wf, NULL, NULL, WAVE_FORMAT_QUERY);

    if (ret == MMSYSERR_NOERROR) {
      ++it;
      continue;
    }
    if (ret == WAVERR_BADFORMAT)
      it = m_supportedRate.erase(it);
    else
      return false;
  }
  if (m_supportedRate.end() == m_supportedRate.begin()) return false;

  return true;
}

//====================================================================
namespace {
void CALLBACK recordCB(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1,
                       DWORD dwParam2) {
  WAVEHDR *whdr     = (WAVEHDR *)dwParam1;
  HANDLE *blockDone = (HANDLE *)dwInstance;

  if (uMsg != MM_WIM_DATA) return;
  SetEvent(blockDone);
}
}

//==============================================================================

TSoundInputDevice::TSoundInputDevice() : m_imp(new TSoundInputDeviceImp()) {
  m_imp->insertAllRate();
  if (!m_imp->verifyRate())
    throw TSoundDeviceException(TSoundDeviceException::FailedInit,
                                "Unable to verify supported rates");
  if (supportsVolume()) m_imp->m_supportVolume = true;
}

//------------------------------------------------------------------------------

TSoundInputDevice::~TSoundInputDevice() {}

//------------------------------------------------------------------------------

bool TSoundInputDevice::installed() {
  int ndev = waveInGetNumDevs();
  if (ndev <= 0) return false;
  return true;
}

//------------------------------------------------------------------------------

#ifndef TNZCORE_LIGHT

void TSoundInputDevice::record(const TSoundTrackFormat &format,
                               Source devtype) {
  if (m_imp->m_isRecording)
    throw TSoundDeviceException(TSoundDeviceException::Busy,
                                "Just another recoding is in progress");

  /*if ((format.m_bitPerSample == 8 && format.m_signedSample) ||
(format.m_bitPerSample == 24))
throw TException("This format is not supported for recording");*/
  try {
    TSoundTrackFormat fmt = getPreferredFormat(format);
    if (fmt != format) {
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported Format");
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }

  if (!setRecordLine(devtype))
    throw TSoundDeviceException(TSoundDeviceException::UnableSetDevice,
                                "Problems to set input source line to record");

  m_imp->m_format = format;
  m_imp->m_st     = 0;
  ResetEvent(m_imp->m_hLastBlockDone);
  ResetEvent(m_imp->m_hBlockDone);
  m_imp->m_allocateBuff = true;
  m_imp->m_isRecording  = true;
  m_imp->m_index        = 0;
  m_imp->m_recordedBlocks.clear();
  m_imp->m_byteRecorded = 0;
  TINT32 bytePerSec     = format.m_sampleRate *
                      ((format.m_bitPerSample * format.m_channelCount) >> 3);

  try {
    WaveFormat wf(m_imp->m_format.m_channelCount, m_imp->m_format.m_sampleRate,
                  m_imp->m_format.m_bitPerSample);

    m_imp->open(wf);
  } catch (TException &e) {
    m_imp->m_isRecording = false;
    throw TSoundDeviceException(TSoundDeviceException::UnableOpenDevice,
                                e.getMessage());
  }
  for (; m_imp->m_index < (int)(m_imp->m_whdr.size() - 1); ++m_imp->m_index) {
    try {
      m_imp->prepareHeader(new char[bytePerSec], bytePerSec,
                           m_imp->m_whdr[m_imp->m_index]);
      m_imp->addBlock(m_imp->m_whdr[m_imp->m_index]);
    } catch (TException &e) {
      m_imp->m_isRecording = false;
      for (int j = 0; j < (int)(m_imp->m_whdr.size() - 1); ++j) {
        if (m_imp->m_whdr[j].dwFlags & WHDR_PREPARED) {
          try {
            m_imp->unprepareHeader(m_imp->m_whdr[j]);
          } catch (TException &e) {
            throw TSoundDeviceException(
                TSoundDeviceException::UnableCloseDevice, e.getMessage());
          }
          delete[] m_imp->m_whdr[j].lpData;
        } else if (j == m_imp->m_index)
          delete[] m_imp->m_whdr[j].lpData;
      }
      throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                  e.getMessage());
    }
  }

  m_imp->m_executor.addTask(new RecordTask(m_imp));
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackP &st, Source devtype) {
  if (m_imp->m_isRecording)
    throw TSoundDeviceException(TSoundDeviceException::Busy,
                                "Just another recoding is in progress");

  m_imp->m_format = st->getFormat();
  /*if ((m_imp->m_format.m_bitPerSample == 8 && m_imp->m_format.m_signedSample)
||
(m_imp->m_format.m_bitPerSample == 24))
throw TException("This format is not supported for recording");*/
  try {
    TSoundTrackFormat fmt = getPreferredFormat(st->getFormat());
    if (fmt != st->getFormat()) {
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "Unsupported Format");
    }
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }

  if (!setRecordLine(devtype))
    throw TSoundDeviceException(TSoundDeviceException::UnableSetDevice,
                                "Problems to set input source line to record");

  m_imp->m_st           = st;
  m_imp->m_allocateBuff = false;
  m_imp->m_isRecording  = true;
  ResetEvent(m_imp->m_hLastBlockDone);
  ResetEvent(m_imp->m_hBlockDone);
  m_imp->m_index = 0;
  m_imp->m_recordedBlocks.clear();
  m_imp->m_byteRecorded = 0;
  try {
    WaveFormat wf(m_imp->m_format.m_channelCount, m_imp->m_format.m_sampleRate,
                  m_imp->m_format.m_bitPerSample);

    m_imp->open(wf);
    m_imp->prepareHeader(
        (char *)st->getRawData(),
        st->getSampleCount() *
            ((st->getBitPerSample() * st->getChannelCount()) >> 3),
        m_imp->m_whdr[m_imp->m_index]);
    m_imp->addBlock(m_imp->m_whdr[m_imp->m_index]);
  } catch (TException &e) {
    m_imp->m_isRecording = false;
    if (m_imp->m_whdr[m_imp->m_index].dwFlags & WHDR_PREPARED)
      m_imp->unprepareHeader(m_imp->m_whdr[m_imp->m_index]);

    throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                e.getMessage());
  }

  m_imp->m_executor.addTask(new RecordTask(m_imp));
}
#endif

//------------------------------------------------------------------------------

TSoundTrackP TSoundInputDevice::stop() {
  if (!m_imp->m_isRecording) return 0;

  m_imp->m_isRecording = false;
  try {
    m_imp->reset();
  } catch (TException &e) {
    for (int j = 0; j < (int)m_imp->m_whdr.size(); ++j) {
      if (m_imp->m_whdr[j].dwFlags & WHDR_PREPARED) {
        try {
          m_imp->unprepareHeader(m_imp->m_whdr[j]);
        } catch (TException &e) {
          throw TSoundDeviceException(TSoundDeviceException::UnablePrepare,
                                      e.getMessage());
        }
        delete[] m_imp->m_whdr[j].lpData;
      }
    }
    throw TSoundDeviceException(TSoundDeviceException::UnableCloseDevice,
                                e.getMessage());
  }

  if (m_imp->m_allocateBuff) {
    WaitForSingleObject(m_imp->m_hLastBlockDone, INFINITE);

    TSoundTrackP st = TSoundTrack::create(
        m_imp->m_format,
        m_imp->m_byteRecorded / ((m_imp->m_format.m_bitPerSample *
                                  m_imp->m_format.m_channelCount) >>
                                 3));

    TINT32 bytePerSec =
        m_imp->m_format.m_sampleRate *
        ((m_imp->m_format.m_bitPerSample * m_imp->m_format.m_channelCount) >>
         3);

    int i;
    for (i = 0; i < (int)(m_imp->m_recordedBlocks.size() - 1); ++i) {
      memcpy((void *)(st->getRawData() + bytePerSec * i),
             m_imp->m_recordedBlocks[i], bytePerSec);
      delete[] m_imp->m_recordedBlocks[i];
    }

    TINT32 lastBlockSize = m_imp->m_byteRecorded - (bytePerSec * i);

    if (lastBlockSize != 0) {
      memcpy((void *)(st->getRawData() + bytePerSec * i),
             m_imp->m_recordedBlocks[i], lastBlockSize);
      delete[] m_imp->m_recordedBlocks[i];
    }

    try {
      m_imp->close();
    } catch (TException &e) {
      throw TSoundDeviceException(TSoundDeviceException::UnableCloseDevice,
                                  e.getMessage());
    }
    return st;
  } else {
    WaitForSingleObject(m_imp->m_hLastBlockDone, INFINITE);
    try {
      m_imp->close();
    } catch (TException &e) {
      throw TSoundDeviceException(TSoundDeviceException::UnableCloseDevice,
                                  e.getMessage());
    }
    return m_imp->m_st;
  }
}

//------------------------------------------------------------------------------
#ifndef TNZCORE_LIGHT

void RecordTask::run() {
  m_dev->start();

  if (m_dev->m_allocateBuff) {
    TINT32 bytePerSec =
        m_dev->m_format.m_sampleRate *
        ((m_dev->m_format.m_bitPerSample * m_dev->m_format.m_channelCount) >>
         3);

    while (m_dev->m_whdr[(m_dev->m_index + 1) % m_dev->m_whdr.size()].dwFlags &
           WHDR_PREPARED) {
      if (m_dev->m_isRecording)
        WaitForSingleObject(m_dev->m_hBlockDone, INFINITE);
      int indexToPrepare = m_dev->m_index;

      // calcolo l'indice successivo per far l'unprepare
      m_dev->m_index = (m_dev->m_index + 1) % m_dev->m_whdr.size();
      if (m_dev->m_whdr[m_dev->m_index].dwFlags & WHDR_DONE) {
        TINT32 byteRecorded = m_dev->m_whdr[m_dev->m_index].dwBytesRecorded;
        if (byteRecorded) {
          m_dev->m_recordedBlocks.push_back(
              m_dev->m_whdr[m_dev->m_index].lpData);
          m_dev->m_byteRecorded += byteRecorded;
        }

        try {
          m_dev->unprepareHeader(m_dev->m_whdr[m_dev->m_index]);
        } catch (TException &) {
          for (int i = 0; i < (int)m_dev->m_recordedBlocks.size(); ++i)
            delete[] m_dev->m_recordedBlocks[i];
          return;
        }

        if (byteRecorded == 0) {
          delete[] m_dev->m_whdr[m_dev->m_index].lpData;
        }

        // con questo controllo si evita che vengano accodati nuovi blocchi
        // dopo che e' stata chiamata la waveInReset
        if (m_dev->m_isRecording) {
          try {
            m_dev->prepareHeader(new char[bytePerSec], bytePerSec,
                                 m_dev->m_whdr[indexToPrepare]);

            m_dev->addBlock(m_dev->m_whdr[indexToPrepare]);
          } catch (TException &) {
            m_dev->m_isRecording = false;
            for (int i = 0; i < (int)m_dev->m_recordedBlocks.size(); ++i)
              delete[] m_dev->m_recordedBlocks[i];
            return;
          }
        }

      } else
        m_dev->m_index = indexToPrepare;
    }
  } else {
    if (m_dev->m_isRecording)
      WaitForSingleObject(m_dev->m_hBlockDone, INFINITE);
    try {
      m_dev->unprepareHeader(m_dev->m_whdr[m_dev->m_index]);
      m_dev->m_isRecording = false;
    } catch (TException &) {
      m_dev->m_isRecording = false;
      return;
    }
  }

  SetEvent(m_dev->m_hLastBlockDone);
  return;
}

//------------------------------------------------------------------------------
#endif

double TSoundInputDevice::getVolume() {
  DWORD dwVolumeControlID;

  UINT nNumMixers;
  MMRESULT ret;
  MIXERLINE mxl;

  nNumMixers = mixerGetNumDevs();

  if (nNumMixers == 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Doesn't exist a audio mixer device");

  // get dwLineID
  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");

  // get dwControlID
  MIXERCONTROL mxc;
  ret = getLineControl(mxc, (HMIXEROBJ)0, mxl.dwLineID,
                       MIXERCONTROL_CONTROLTYPE_VOLUME);
  if (ret == MIXERR_INVALCONTROL)
    throw TSoundDeviceException(
        TSoundDeviceException::UnableVolume,
        "Is not possible to obtain info of volume by mixer");

  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");

  dwVolumeControlID = mxc.dwControlID;

  MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;

  ret = getControlDetails((HMIXEROBJ)0, dwVolumeControlID, mxc.cMultipleItems,
                          &mxcdVolume);
  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");

  DWORD dwVal = mxcdVolume.dwValue;

  return (double)dwVal * 10.0 / (double)mxc.Bounds.dwMaximum;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::setVolume(double value) {
  DWORD dwVolumeControlID, dwMaximum;

  UINT nNumMixers;
  MMRESULT ret;
  MIXERLINE mxl;

  nNumMixers = mixerGetNumDevs();

  if (nNumMixers == 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Doesn't exist a audio mixer device");

  // get dwLineID
  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    throw TException("Error to obtain info by mixer");

  // get dwControlID
  MIXERCONTROL mxc;
  ret = getLineControl(mxc, (HMIXEROBJ)0, mxl.dwLineID,
                       MIXERCONTROL_CONTROLTYPE_VOLUME);
  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");

  dwMaximum         = mxc.Bounds.dwMaximum;
  dwVolumeControlID = mxc.dwControlID;

  int newValue;
  double fattProp = ((double)(mxc.Metrics.cSteps - 1) * value) / 10;
  double delta    = (double)(dwMaximum / (mxc.Metrics.cSteps - 1));
  newValue        = (int)(tround(fattProp) * delta);

  MIXERCONTROLDETAILS_UNSIGNED mxcdVolume = {newValue};
  ret = setControlDetails((HMIXEROBJ)0, dwVolumeControlID, mxc.cMultipleItems,
                          &mxcdVolume);
  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");
  return true;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::supportsVolume() {
  UINT nNumMixers;
  MMRESULT ret;
  MIXERLINE mxl;

  nNumMixers = mixerGetNumDevs();

  if (nNumMixers == 0)
    throw TSoundDeviceException(TSoundDeviceException::NoMixer,
                                "Doesn't exist a audio mixer device");

  // get dwLineID
  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");

  // get dwControlID
  MIXERCONTROL mxc;
  ret = getLineControl(mxc, (HMIXEROBJ)0, mxl.dwLineID,
                       MIXERCONTROL_CONTROLTYPE_VOLUME);
  if (ret == MIXERR_INVALCONTROL) return false;

  if (ret != MMSYSERR_NOERROR)
    throw TSoundDeviceException(TSoundDeviceException::UnableVolume,
                                "Error to obtain info by mixer");

  return true;
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::isRecording() { return m_imp->m_isRecording; }

//------------------------------------------------------------------------------

/*TSoundTrackFormat TSoundInputDevice::getPreferredFormat(
      ULONG sampleRate, int channelCount, int bitPerSample)
{
  MMRESULT ret;
  TSoundTrackFormat fmt;

  ret = isaFormatSupported(sampleRate, channelCount, bitPerSample, true);

  if (ret == MMSYSERR_NOERROR)
  {
    fmt.m_bitPerSample = bitPerSample;
    fmt.m_channelCount = channelCount;
    fmt.m_sampleRate = sampleRate;

    if (bitPerSample >= 16)
      fmt.m_signedSample = true;
    else
      fmt.m_signedSample = false;
    return fmt;
  }
  if (ret == WAVERR_BADFORMAT)
  {
    //avvvicinarsi al sample rate => dovrebbe esser OK avendo selezionato i piu'
vicini
    std::set<int>::iterator it = m_imp->m_supportedRate.lower_bound(sampleRate);
    if (it == m_imp->m_supportedRate.end())
    {
                  it = std::max_element(m_imp->m_supportedRate.begin(),
                                         m_imp->m_supportedRate.end());
                        if (it != m_imp->m_supportedRate.end())
                  sampleRate = *(m_imp->m_supportedRate.rbegin());
      else
        throw TSoundDeviceException(
          TSoundDeviceException::UnsupportedFormat,
          "There isn't a supported format");
    }
    else
      sampleRate = *it;
    ret = isaFormatSupported(sampleRate, channelCount, bitPerSample, true);
    if (ret == MMSYSERR_NOERROR)
    {
      fmt.m_bitPerSample = bitPerSample;
      fmt.m_channelCount = channelCount;
      fmt.m_sampleRate = sampleRate;

      if (bitPerSample >= 16)
        fmt.m_signedSample = true;
      else
        fmt.m_signedSample = false;
      return fmt;
    }
    if (ret == WAVERR_BADFORMAT)
    {
      //cambiare bps
      if (bitPerSample <= 8)
        bitPerSample = 8;
      else if ((bitPerSample > 8 && bitPerSample < 16) || bitPerSample >= 16)
        bitPerSample = 16;

      ret = isaFormatSupported(sampleRate, channelCount, bitPerSample, true);
      if (ret == MMSYSERR_NOERROR)
      {
        fmt.m_bitPerSample = bitPerSample;
        fmt.m_channelCount = channelCount;
        fmt.m_sampleRate = sampleRate;

        if (bitPerSample >= 16)
          fmt.m_signedSample = true;
        else
          fmt.m_signedSample = false;
        return fmt;
      }
      if (ret == WAVERR_BADFORMAT)
      {
        //switch mono/stereo
        if (channelCount <= 1)
          channelCount = 1;
        else
          channelCount = 2;

        ret = isaFormatSupported(sampleRate, channelCount, bitPerSample, true);
        if (ret == MMSYSERR_NOERROR)
        {
          fmt.m_bitPerSample = bitPerSample;
          fmt.m_channelCount = channelCount;
          fmt.m_sampleRate = sampleRate;

          if (bitPerSample >= 16)
            fmt.m_signedSample = true;
          else
            fmt.m_signedSample = false;
          return fmt;
        }
        if (ret == WAVERR_BADFORMAT)
        {
          throw TSoundDeviceException(
            TSoundDeviceException::UnsupportedFormat,
            "Doesn't exist a preferred format");
        }
      }
    }
  }
  throw TSoundDeviceException(
    TSoundDeviceException::UnsupportedFormat,
    "Error to query supported format");
}
*/
TSoundTrackFormat TSoundInputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                        int channelCount,
                                                        int bitPerSample) {
  TSoundTrackFormat fmt;

  // avvvicinarsi al sample rate => dovrebbe esser OK avendo selezionato i piu'
  // vicini
  std::set<int>::iterator it = m_imp->m_supportedRate.lower_bound(sampleRate);
  if (it == m_imp->m_supportedRate.end()) {
    it = std::max_element(m_imp->m_supportedRate.begin(),
                          m_imp->m_supportedRate.end());
    if (it != m_imp->m_supportedRate.end())
      sampleRate = *(m_imp->m_supportedRate.rbegin());
    else
      throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                  "There isn't a supported format");
  } else
    sampleRate = *it;

  if (bitPerSample <= 8)
    bitPerSample = 8;
  else if ((bitPerSample > 8 && bitPerSample < 16) || bitPerSample >= 16)
    bitPerSample = 16;

  if (bitPerSample >= 16)
    fmt.m_signedSample = true;
  else
    fmt.m_signedSample = false;

  // switch mono/stereo
  if (channelCount <= 1)
    channelCount = 1;
  else
    channelCount = 2;

  fmt.m_bitPerSample = bitPerSample;
  fmt.m_channelCount = channelCount;
  fmt.m_sampleRate   = sampleRate;

  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  try {
    return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                              format.m_bitPerSample);
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(TSoundDeviceException::UnsupportedFormat,
                                e.getMessage());
  }
}

//==============================================================================
//==============================================================================
// Funzioni per l'interazione con il mixer device
//==============================================================================
//==============================================================================

namespace {

// restituisce dentro la struttura mxc le informazioni relative
// al controllo di tipo dwControlType associato alla linea
// identificata da dwLineID
MMRESULT getLineControl(MIXERCONTROL &mxc, HMIXEROBJ hMixer, DWORD dwLineID,
                        DWORD dwControlType) {
  MIXERLINECONTROLS mxlc;
  mxlc.cbStruct      = sizeof(MIXERLINECONTROLS);
  mxlc.dwLineID      = dwLineID;
  mxlc.dwControlType = dwControlType;
  mxlc.cControls     = 1;
  mxlc.cbmxctrl      = sizeof(MIXERCONTROL);
  mxlc.pamxctrl      = &mxc;
  MMRESULT ret       = mixerGetLineControls(
      (HMIXEROBJ)hMixer, &mxlc,
      MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE);
  return ret;
}

//------------------------------------------------------------------------------

// restituisce nella struttura mxl le informazioni relative alla linea
// sorgente individuata dagli estremi destination e source
MMRESULT getLineInfo(HMIXEROBJ hMixer, MIXERLINE &mxl, DWORD destination,
                     DWORD source) {
  MMRESULT ret;

  mxl.cbStruct      = sizeof(mxl);
  mxl.dwDestination = destination;
  mxl.dwSource      = source;
  ret               = mixerGetLineInfo(0, &mxl,
                         MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_SOURCE);
  return ret;
}

//------------------------------------------------------------------------------

// restituisce nella struttura mxl le informazioni relative alla linea
// individuata da dwLineID
MMRESULT getLineInfo(HMIXEROBJ hMixer, MIXERLINE &mxl, DWORD dwLineID) {
  MMRESULT ret;

  mxl.cbStruct = sizeof(mxl);
  mxl.dwLineID = dwLineID;
  ret          = mixerGetLineInfo((HMIXEROBJ)hMixer, &mxl,
                         MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_LINEID);
  return ret;
}

//------------------------------------------------------------------------------

// restituisce nella struttura mxl le informazioni relative alla linea
// individuata dal tipo specificato in dwComponentType
MMRESULT getLineInfo(HMIXEROBJ hMixer, DWORD dwComponentType, MIXERLINE &mxl) {
  MMRESULT ret;

  mxl.cbStruct        = sizeof(MIXERLINE);
  mxl.dwComponentType = dwComponentType;
  ret =
      mixerGetLineInfo((HMIXEROBJ)hMixer, &mxl,
                       MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
  return ret;
}

//------------------------------------------------------------------------------

// consente di settare il valore booleano specificato in mxcdSelectValue
// relativo al controllo specificato in dwSelectControlID
MMRESULT setControlDetails(HMIXEROBJ hMixer, DWORD dwSelectControlID,
                           DWORD dwMultipleItems,
                           MIXERCONTROLDETAILS_BOOLEAN *mxcdSelectValue) {
  MMRESULT ret;
  MIXERCONTROLDETAILS mxcd;
  mxcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
  mxcd.dwControlID    = dwSelectControlID;
  mxcd.cChannels      = 1;
  mxcd.cMultipleItems = dwMultipleItems;
  mxcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
  mxcd.paDetails      = mxcdSelectValue;
  ret                 = mixerSetControlDetails(
      (HMIXEROBJ)hMixer, &mxcd,
      MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);
  return ret;
}

//------------------------------------------------------------------------------

// consente di settare il valore UNSIGNED specificato in mxcdSelectValue
// relativo al controllo specificato in dwSelectControlID
MMRESULT setControlDetails(HMIXEROBJ hMixer, DWORD dwSelectControlID,
                           DWORD dwMultipleItems,
                           MIXERCONTROLDETAILS_UNSIGNED *mxcdSelectValue) {
  MMRESULT ret;
  MIXERCONTROLDETAILS mxcd;
  mxcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
  mxcd.dwControlID    = dwSelectControlID;
  mxcd.cChannels      = 1;
  mxcd.cMultipleItems = dwMultipleItems;
  mxcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  mxcd.paDetails      = mxcdSelectValue;
  ret                 = mixerSetControlDetails(
      (HMIXEROBJ)hMixer, &mxcd,
      MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);
  return ret;
}

//------------------------------------------------------------------------------

// consente di ottenere il valore UNSIGNED specificato in mxcdSelectValue
// relativo al controllo specificato in dwSelectControlID
MMRESULT getControlDetails(HMIXEROBJ hMixer, DWORD dwSelectControlID,
                           DWORD dwMultipleItems,
                           MIXERCONTROLDETAILS_UNSIGNED *mxcdSelectValue) {
  MMRESULT ret;
  MIXERCONTROLDETAILS mxcd;
  mxcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
  mxcd.dwControlID    = dwSelectControlID;
  mxcd.cChannels      = 1;
  mxcd.cMultipleItems = dwMultipleItems;
  mxcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  mxcd.paDetails      = mxcdSelectValue;
  ret                 = mixerGetControlDetails(
      (HMIXEROBJ)hMixer, &mxcd,
      MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);
  return ret;
}
//------------------------------------------------------------------------------

// consente di ottenere la lista di informazioni in pmxcdSelectText
// relativo al controllo specificato in dwSelectControlID
MMRESULT getControlDetails(HMIXEROBJ hMixer, DWORD dwSelectControlID,
                           DWORD dwMultipleItems,
                           MIXERCONTROLDETAILS_LISTTEXT *pmxcdSelectText) {
  MMRESULT ret;
  MIXERCONTROLDETAILS mxcd;

  mxcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
  mxcd.dwControlID    = dwSelectControlID;
  mxcd.cChannels      = 1;
  mxcd.cMultipleItems = dwMultipleItems;
  mxcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_LISTTEXT);
  mxcd.paDetails      = pmxcdSelectText;
  ret                 = mixerGetControlDetails((HMIXEROBJ)0, &mxcd,
                               MIXER_GETCONTROLDETAILSF_LISTTEXT);
  return ret;
}

//------------------------------------------------------------------------------

// restituiscei l nome della linea identificata da lineID
std::string getMixerLineName(DWORD lineID) {
  MIXERLINE mxl;
  MMRESULT ret;

  ret = getLineInfo((HMIXEROBJ)0, mxl, lineID);
#ifdef TNZCORE_LIGHT
  assert(false);
  return "";
#else
  return std::string(mxl.szName);
#endif
}

//------------------------------------------------------------------------------

// restituisce la lista degli identificativi delle linee sorgente associate
// alla destinazione di tipo dstComponentType
std::list<DWORD> getMixerSrcLines(DWORD dstComponentType) {
  std::list<DWORD> srcList;
  MMRESULT ret;
  MIXERLINE mxl;

  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    // forse bisognerebbe lanciare un'eccezione
    return srcList;  // non ha linea di dst per la registrazione

  int v;
  for (v = 0; v < (int)mxl.cConnections; v++) {
    MIXERLINE mxl1;

    ret = getLineInfo((HMIXEROBJ)0, mxl1, mxl.dwDestination, v);
    if (ret == MMSYSERR_NOERROR) srcList.push_back(mxl1.dwLineID);
  }
  return srcList;
}

//------------------------------------------------------------------------------

// restituisce la lista degli identificativi delle linee sorgente di tipo
// srcComponentType associate alla destinazione di tipo dstComponentType
std::list<DWORD> getMixerSrcLines(DWORD dstComponentType,
                                  DWORD srcComponentType) {
  std::list<DWORD> srcList;
  MMRESULT ret;
  MIXERLINE mxl;

  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    // forse bisognerebbe lanciare un'eccezione
    return srcList;  // non ha linea di dst per la registrazione

  int v;
  for (v = 0; v < (int)mxl.cConnections; v++) {
    MIXERLINE mxl1;

    ret = getLineInfo((HMIXEROBJ)0, mxl1, mxl.dwDestination, v);
    if (ret == MMSYSERR_NOERROR)
      if (mxl1.dwComponentType == srcComponentType)
        srcList.push_back(mxl1.dwLineID);
  }
  return srcList;
}

//------------------------------------------------------------------------------

// restituisce true sse la linea destinazione di tipo dstComponentType
// supporta una linea sorgente di tipo srcComponentType
bool isSrcLineSupported(DWORD dstComponentType, DWORD srcComponentType) {
  // ci possono essere piu' linee sorgente dello stesso tipo in
  // corrispondenza di una data linea destinazione ?
  MMRESULT ret;
  MIXERLINE mxl;

  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    return false;  // non ha linea di dst per la registrazione

  int v;
  for (v = 0; v < (int)mxl.cConnections; v++) {
    MIXERLINE mxl1;

    ret = getLineInfo((HMIXEROBJ)0, mxl1, mxl.dwDestination, v);
    if (ret == MMSYSERR_NOERROR)
      if (mxl1.dwComponentType == srcComponentType) return true;
  }

  return false;
}

//------------------------------------------------------------------------------

bool activateSrcLine(const MIXERLINE &mxlDst, DWORD componentTypeSrc) {
  if (!isSrcLineSupported(mxlDst.dwComponentType, componentTypeSrc))
    return false;

  bool bRetVal = true;

  for (DWORD v = 0; v < mxlDst.cConnections; v++) {
    MIXERLINE mxlSrc;
    MMRESULT ret = getLineInfo((HMIXEROBJ)0, mxlSrc, mxlDst.dwDestination, v);

    if (ret == MMSYSERR_NOERROR) {
      // chiedo il controllo di tipo MUTE della linea sorgente
      MIXERCONTROL mxc;
      ret = getLineControl(mxc, (HMIXEROBJ)0, mxlSrc.dwLineID,
                           MIXERCONTROL_CONTROLTYPE_MUTE);

      if (ret == MMSYSERR_NOERROR) {
        MIXERCONTROLDETAILS_BOOLEAN mxcdSelectValue;
        mxcdSelectValue.fValue =
            mxlSrc.dwComponentType == componentTypeSrc ? 0L : 1L;

        ret = setControlDetails((HMIXEROBJ)0, mxc.dwControlID,
                                mxc.cMultipleItems, &mxcdSelectValue);
        if (ret != MMSYSERR_NOERROR) bRetVal = false;
      }
    }
  }
  return bRetVal;
}

//------------------------------------------------------------------------------

bool setSrcMixMuxControl(MIXERCONTROL mxc, DWORD componentTypeSrc) {
  MMRESULT ret;
  DWORD dwIndexLine;
  bool found = false;

  // mantengo nota del ID del controllo dsst individuato e
  // del numero di linee src ad esso associate
  DWORD dwSelectControlID = mxc.dwControlID;
  DWORD dwMultipleItems   = mxc.cMultipleItems;

  if (dwMultipleItems == 0) return false;

  // determino l'indice dell'item corrispondente alla linea sorgente
  // di tipo componentTypeSrc

  std::unique_ptr<MIXERCONTROLDETAILS_LISTTEXT[]> pmxcdSelectText(
      new MIXERCONTROLDETAILS_LISTTEXT[dwMultipleItems]);

  if (pmxcdSelectText) {
    // estraggo le info su tutte le linee associate al controllo
    ret = getControlDetails((HMIXEROBJ)0, dwSelectControlID, dwMultipleItems,
                            pmxcdSelectText.get());

    if (ret == MMSYSERR_NOERROR) {
      for (DWORD dwi = 0; dwi < dwMultipleItems; dwi++) {
        // prendo le info su ogni linea e verifico se e' del giusto tipo
        MIXERLINE mxl;
        ret = getLineInfo((HMIXEROBJ)0, mxl, pmxcdSelectText[dwi].dwParam1);
        if (ret == MMSYSERR_NOERROR &&
            mxl.dwComponentType == componentTypeSrc) {
          dwIndexLine = dwi;
          found       = true;
          break;
        }
      }
    }

    if (!found) return false;
  }

  if (dwIndexLine >= dwMultipleItems) return false;

  bool bRetVal = false;

  std::unique_ptr<MIXERCONTROLDETAILS_BOOLEAN[]> pmxcdSelectValue(
      new MIXERCONTROLDETAILS_BOOLEAN[dwMultipleItems]);

  if (pmxcdSelectValue) {
    ::ZeroMemory(pmxcdSelectValue.get(),
                 dwMultipleItems * sizeof(MIXERCONTROLDETAILS_BOOLEAN));

    // impostazione del valore
    pmxcdSelectValue[dwIndexLine].fValue =
        (TINT32)1;  // lVal; //dovrebbe esser uno

    ret = setControlDetails((HMIXEROBJ)0, dwSelectControlID, dwMultipleItems,
                            pmxcdSelectValue.get());
    if (ret == MMSYSERR_NOERROR) bRetVal = true;
  }
  return bRetVal;
}

//------------------------------------------------------------------------------

bool setRecordLine(TSoundInputDevice::Source typeInput) {
  DWORD dwComponentTypeSrc;

  UINT nNumMixers;
  MMRESULT ret;
  MIXERLINE mxl = {0};

  switch (typeInput) {
  case TSoundInputDevice::LineIn:
    dwComponentTypeSrc = MIXERLINE_COMPONENTTYPE_SRC_LINE /*|
                             MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY |
                             MIXERLINE_COMPONENTTYPE_SRC_ANALOG*/;
    break;
  case TSoundInputDevice::DigitalIn:
    dwComponentTypeSrc = MIXERLINE_COMPONENTTYPE_SRC_DIGITAL;
    break;
  case TSoundInputDevice::CdAudio:
    dwComponentTypeSrc = MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC;
    break;
  default:
    dwComponentTypeSrc = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;
  }

  nNumMixers = mixerGetNumDevs();

  if (nNumMixers == 0) return false;

  // utilizziamo il MIXER di default identificato dall'indice 0
  // vedo se il device ha una linea dst per il wave_input
  ret = getLineInfo((HMIXEROBJ)0, MIXERLINE_COMPONENTTYPE_DST_WAVEIN, mxl);
  if (ret != MMSYSERR_NOERROR)
    return false;  // non ha linea di dst per la registrazione

  // vediamo che tipo controllo ha questa linea dst

  // sara' un MIXER?
  MIXERCONTROL mxc = {0};

  ret = getLineControl(mxc, (HMIXEROBJ)0, mxl.dwLineID,
                       MIXERCONTROL_CONTROLTYPE_MIXER);

  if (ret != MMSYSERR_NOERROR) {
    // no mixer, try MUX
    ret = getLineControl(mxc, (HMIXEROBJ)0, mxl.dwLineID,
                         MIXERCONTROL_CONTROLTYPE_MUX);

    if (ret != MMSYSERR_NOERROR) {
      // vediamo se e' uno di quei device ne' MIXER ne' MUX
      return activateSrcLine(mxl, dwComponentTypeSrc);
    } else {
      // la linea ha un controllo di tipo MUX
      return setSrcMixMuxControl(mxc, dwComponentTypeSrc);
    }
  } else {
    // la linea ha un controllo di tipo MIXER
    return setSrcMixMuxControl(mxc, dwComponentTypeSrc);
  }
}

//------------------------------------------------------------------------------

MMRESULT isaFormatSupported(int sampleRate, int channelCount, int bitPerSample,
                            bool input) {
  WAVEFORMATEX wf;
  MMRESULT ret;

  wf.wFormatTag      = WAVE_FORMAT_PCM;
  wf.nChannels       = channelCount;
  wf.nSamplesPerSec  = sampleRate;
  wf.wBitsPerSample  = bitPerSample;
  wf.nBlockAlign     = (wf.nChannels * wf.wBitsPerSample) >> 3;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
  wf.cbSize          = 0;

  if (input)
    ret = waveInOpen(NULL, WAVE_MAPPER, &wf, NULL, NULL, WAVE_FORMAT_QUERY);
  else
    ret = waveOutOpen(NULL, WAVE_MAPPER, &wf, NULL, NULL, WAVE_FORMAT_QUERY);
  return ret;
}
}
