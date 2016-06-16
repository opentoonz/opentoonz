

#include "tsound_t.h"
#include "texception.h"
#include "tthread.h"
#include "tthreadmessage.h"

#include <errno.h>
#include <unistd.h>
#include <queue>
#include <set>

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
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
  AudioFileID musicFileID;
  AudioUnit theOutputUnit;
  AudioStreamBasicDescription fileASBD;
  AudioStreamBasicDescription outputASBD;
  AudioConverterRef converter;

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
};

//-----------------------------------------------------------------------------
namespace {

struct MyData {
  char *entireFileBuffer;

  UInt64 totalPacketCount;
  UInt64 fileByteCount;
  UInt32 maxPacketSize;
  UInt64 packetOffset;
  UInt64 byteOffset;
  bool m_doNotify;

  void *sourceBuffer;
  AudioConverterRef converter;
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

extern "C" {
// This is an example of a Input Procedure from a call to
// AudioConverterFillComplexBuffer.
// The total amount of data needed is "ioNumberDataPackets" when this method is
// first called.
// On exit, "ioNumberDataPackets" must be set to the actual amount of data
// obtained.
// Upon completion, all new input data must point to the AudioBufferList in the
// parameter ( "ioData" )
OSStatus MyACComplexInputProc(
    AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets,
    AudioBufferList *ioData,
    AudioStreamPacketDescription **outDataPacketDescription, void *inUserData) {
  OSStatus err       = noErr;
  UInt32 bytesCopied = 0;

  MyData *myData = static_cast<MyData *>(inUserData);

  // initialize in case of failure
  ioData->mBuffers[0].mData         = NULL;
  ioData->mBuffers[0].mDataByteSize = 0;

  {
    // TThread::ScopedLock sl(MutexOut);
    if (myData->imp->m_isPlaying == false) return noErr;
  }

  // if there are not enough packets to satisfy request, then read what's left
  if (myData->packetOffset + *ioNumberDataPackets > myData->totalPacketCount)
    *ioNumberDataPackets = myData->totalPacketCount - myData->packetOffset;

  // do nothing if there are no packets available
  if (*ioNumberDataPackets) {
    if (myData->sourceBuffer != NULL) {
      free(myData->sourceBuffer);
      myData->sourceBuffer = NULL;
    }

    // the total amount of data requested by the AudioConverter
    bytesCopied = *ioNumberDataPackets * myData->maxPacketSize;
    // alloc a small buffer for the AudioConverter to use.
    myData->sourceBuffer = (void *)calloc(1, bytesCopied);
    // copy the amount of data needed (bytesCopied) from buffer of audio file
    memcpy(myData->sourceBuffer, myData->entireFileBuffer + myData->byteOffset,
           bytesCopied);

    // keep track of where we want to read from next time
    myData->byteOffset += *ioNumberDataPackets * myData->maxPacketSize;
    myData->packetOffset += *ioNumberDataPackets;

    ioData->mBuffers[0].mData = myData->sourceBuffer;  // tell the Audio
                                                       // Converter where it's
                                                       // source data is
    ioData->mBuffers[0].mDataByteSize =
        bytesCopied;  // tell the Audio Converter how much data in each buffer
  } else {
    // there aren't any more packets to read.
    // Set the amount of data read (mDataByteSize) to zero
    // and return noErr to signal the AudioConverter there are
    // no packets left.

    ioData->mBuffers[0].mData         = NULL;
    ioData->mBuffers[0].mDataByteSize = 0;
    delete[] myData->entireFileBuffer;
    myData->entireFileBuffer = 0;
    err                      = noErr;
    /*
{
TThread::ScopedLock sl(MutexOut);
*(myData->isPlaying) = false;   //questo lo faccio nel main thread
}
*/
    PlayCompletedMsg(myData).send();
  }

  return err;
}

OSStatus MyFileRenderProc(void *inRefCon,
                          AudioUnitRenderActionFlags *inActionFlags,
                          const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                          UInt32 inNumFrames, AudioBufferList *ioData) {
  MyData *myData                = static_cast<MyData *>(inRefCon);
  OSStatus err                  = noErr;
  void *inInputDataProcUserData = inRefCon;
  AudioStreamPacketDescription *outPacketDescription = NULL;
  // To obtain a data buffer of converted data from a complex input
  // source(compressed files, etc.)
  // use AudioConverterFillComplexBuffer.  The total amount of data requested is
  // "inNumFrames" and
  // on return is set to the actual amount of data recieved.
  // All converted data is returned to "ioData" (AudioBufferList).
  err = AudioConverterFillComplexBuffer(myData->converter, MyACComplexInputProc,
                                        inInputDataProcUserData, &inNumFrames,
                                        ioData, outPacketDescription);

  /*Parameters for AudioConverterFillComplexBuffer()
converter - the converter being used
ACComplexInputProc() - input procedure to supply data to the Audio Converter
inInputDataProcUserData - Used to hold any data that needs to be passed on.  Not
needed in this example.
inNumFrames - The amount of requested data.  On output, this
number is the amount actually received.
ioData - Buffer of the converted data recieved on return
outPacketDescription - contains the format of the returned data.  Not used in
this example.
*/

  // checkStatus(err);
  return err;
}

}  // extern "C"

void PrintStreamDesc(AudioStreamBasicDescription *inDesc) {
  if (!inDesc) {
    printf("Can't print a NULL desc!\n");
    return;
  }

  printf("- - - - - - - - - - - - - - - - - - - -\n");
  printf("  Sample Rate:%f\n", inDesc->mSampleRate);
  printf("  Format ID:%.*s\n", (int)sizeof(inDesc->mFormatID),
         (char *)&inDesc->mFormatID);
  printf("  Format Flags:%lX\n", inDesc->mFormatFlags);
  printf("  Bytes per Packet:%ld\n", inDesc->mBytesPerPacket);
  printf("  Frames per Packet:%ld\n", inDesc->mFramesPerPacket);
  printf("  Bytes per Frame:%ld\n", inDesc->mBytesPerFrame);
  printf("  Channels per Frame:%ld\n", inDesc->mChannelsPerFrame);
  printf("  Bits per Channel:%ld\n", inDesc->mBitsPerChannel);
  printf("- - - - - - - - - - - - - - - - - - - -\n");
}

bool TSoundOutputDeviceImp::doOpenDevice() {
  m_opened     = false;
  OSStatus err = noErr;
  ComponentDescription desc;
  Component comp;

  desc.componentType    = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  // all Audio Units in AUComponent.h must use "kAudioUnitManufacturer_Apple" as
  // the Manufacturer
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags        = 0;
  desc.componentFlagsMask    = 0;

  comp = FindNextComponent(
      NULL, &desc);  // Finds an component that meets the desc spec's
  if (comp == NULL) return false;
  err = OpenAComponent(comp, &theOutputUnit);  // gains access to the services
                                               // provided by the component
  if (err) return false;

  UInt32 size;
  Boolean outWritable;
  UInt32 theInputBus = 0;
  // Gets the size of the Stream Format Property and if it is writable
  err =
      AudioUnitGetPropertyInfo(theOutputUnit, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Output, 0, &size, &outWritable);
  // Get the current stream format of the output
  err = AudioUnitGetProperty(theOutputUnit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output, 0, &outputASBD, &size);
  checkStatus(err);
  // Set the stream format of the output to match the input
  err = AudioUnitSetProperty(theOutputUnit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input, theInputBus, &outputASBD,
                             size);
  checkStatus(err);

  // Initialize AudioUnit, alloc mem buffers for processing
  err = AudioUnitInitialize(theOutputUnit);
  checkStatus(err);
  if (err == noErr) m_opened = true;
  return m_opened;
}

bool TSoundOutputDeviceImp::doSetStreamFormat(const TSoundTrackFormat &format) {
  if (!m_opened) doOpenDevice();
  if (!m_opened) return false;

  fileASBD.mSampleRate  = format.m_sampleRate;
  fileASBD.mFormatID    = kAudioFormatLinearPCM;
  fileASBD.mFormatFlags = 14;
  /*
Standard flags: kAudioFormatFlagIsFloat = (1L << 0)
kAudioFormatFlagIsBigEndian = (1L << 1)
kAudioFormatFlagIsSignedInteger = (1L << 2)
kAudioFormatFlagIsPacked = (1L << 3)
kAudioFormatFlagIsAlignedHigh = (1L << 4)
kAudioFormatFlagIsNonInterleaved = (1L << 5)
kAudioFormatFlagsAreAllClear = (1L << 31)

Linear PCM flags:
kLinearPCMFormatFlagIsFloat = kAudioFormatFlagIsFloat
kLinearPCMFormatFlagIsBigEndian = kAudioFormatFlagIsBigEndian
kLinearPCMFormatFlagIsSignedInteger = kAudioFormatFlagIsSignedInteger
kLinearPCMFormatFlagIsPacked = kAudioFormatFlagIsPacked
kLinearPCMFormatFlagIsAlignedHigh = kAudioFormatFlagIsAlignedHigh
kLinearPCMFormatFlagIsNonInterleaved = kAudioFormatFlagIsNonInterleaved
kLinearPCMFormatFlagsAreAllClear = kAudioFormatFlagsAreAllClear
*/
  fileASBD.mBytesPerPacket =
      (format.m_bitPerSample >> 3) * format.m_channelCount;
  fileASBD.mFramesPerPacket = 1;
  fileASBD.mBytesPerFrame =
      (format.m_bitPerSample >> 3) * format.m_channelCount;
  fileASBD.mChannelsPerFrame = format.m_channelCount;
  fileASBD.mBitsPerChannel   = format.m_bitPerSample;
  fileASBD.mReserved         = 0;
  // PrintStreamDesc(&fileASBD);
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
  AudioUnitUninitialize(
      m_imp->theOutputUnit);  // release resources without closing the component
  CloseComponent(m_imp->theOutputUnit);  // Terminates your application's access
                                         // to the services provided
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
  if (!doSetStreamFormat(st->getFormat())) return;

  OSStatus err   = noErr;
  MyData *myData = new MyData();

  myData->imp            = shared_from_this();
  UInt32 magicCookieSize = 0;
  // PrintStreamDesc(&outputASBD);
  err = AudioConverterNew(&fileASBD, &outputASBD, &converter);
  checkStatus(err);
  err = AudioFileGetPropertyInfo(musicFileID, kAudioFilePropertyMagicCookieData,
                                 &magicCookieSize, NULL);

  if (err == noErr) {
    void *magicCookie = calloc(1, magicCookieSize);
    if (magicCookie) {
      // Get Magic Cookie data from Audio File
      err = AudioFileGetProperty(musicFileID, kAudioFilePropertyMagicCookieData,
                                 &magicCookieSize, magicCookie);

      // Give the AudioConverter the magic cookie decompression params if there
      // are any
      if (err == noErr) {
        err = AudioConverterSetProperty(myData->converter,
                                        kAudioConverterDecompressionMagicCookie,
                                        magicCookieSize, magicCookie);
      }
      err = noErr;
      if (magicCookie) free(magicCookie);
    }
  } else  // this is OK because some audio data doesn't need magic cookie data
    err = noErr;

  checkStatus(err);
  myData->converter        = converter;
  myData->totalPacketCount = s1 - s0;
  myData->fileByteCount    = (s1 - s0) * st->getSampleSize();
  myData->entireFileBuffer = new char[myData->fileByteCount];

#if defined(i386)
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

  myData->maxPacketSize = fileASBD.mFramesPerPacket * fileASBD.mBytesPerFrame;
  {
    // TThread::ScopedLock sl(MutexOut);
    m_isPlaying = true;
  }
  myData->isLooping = loop;

  // cout << "total packet count = " << myData->totalPacketCount <<endl;
  // cout << "filebytecount " << myData->fileByteCount << endl;

  AURenderCallbackStruct renderCallback;
  memset(&renderCallback, 0, sizeof(AURenderCallbackStruct));

  renderCallback.inputProc       = MyFileRenderProc;
  renderCallback.inputProcRefCon = myData;

  // Sets the callback for the Audio Unit to the renderCallback
  err =
      AudioUnitSetProperty(theOutputUnit, kAudioUnitProperty_SetRenderCallback,
                           kAudioUnitScope_Input, 0, &renderCallback,
                           sizeof(AURenderCallbackStruct));

  checkStatus(err);

  err = AudioOutputUnitStart(theOutputUnit);

  checkStatus(err);
}

//------------------------------------------------------------------------------

bool TSoundOutputDeviceImp::doStopDevice() {
  m_isPlaying = false;
  AudioOutputUnitStop(
      theOutputUnit);  // you must stop the audio unit from processing
  AudioConverterDispose(
      converter);  // deallocates the memory used by inAudioConverter
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

//------------------------------------------------------------------------------

double TSoundOutputDevice::getVolume() {
  if (!m_imp->m_opened) m_imp->doOpenDevice();

  Float32 leftVol, rightVol;
  AudioUnitGetParameter(m_imp->theOutputUnit, kHALOutputParam_Volume,
                        kAudioUnitScope_Output, 0, &leftVol);

  AudioUnitGetParameter(m_imp->theOutputUnit, kHALOutputParam_Volume,
                        kAudioUnitScope_Output, 0, &rightVol);
  double vol = (leftVol + rightVol) / 2;

  return (vol < 0. ? 0. : vol);
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::setVolume(double volume) {
  Float32 vol = volume;
  AudioUnitSetParameter(m_imp->theOutputUnit, kHALOutputParam_Volume,
                        kAudioUnitScope_Output, 0, vol, 0);

  AudioUnitSetParameter(m_imp->theOutputUnit, kHALOutputParam_Volume,
                        kAudioUnitScope_Output, 0, vol, 0);
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::supportsVolume() { return true; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const {
  // TThread::ScopedLock sl(MutexOut);
  return m_imp->m_isPlaying;
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
  TSoundTrackFormat fmt(sampleRate, bitPerSample, channelCount, true);
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  // try {
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
  /*}
catch (TSoundDeviceException &e) {
throw TSoundDeviceException( e.getType(), e.getMessage());
}*/
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
