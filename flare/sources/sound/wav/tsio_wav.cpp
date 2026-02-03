

#include <memory>

#include "tmachine.h"
#include "tsio_wav.h"
#include "tsystem.h"
#include "tfilepath_io.h"
#include "tsioutils.h"

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    //==============================================================================

    // TWAVChunk: base class for various WAV chunks

    class TWAVChunk {
public:
  static TINT32 HDR_LENGTH;

  std::string m_name;
  TINT32 m_length;  // chunk length in bytes

  TWAVChunk(std::string name, TINT32 length) : m_name(name), m_length(length) {}

  virtual ~TWAVChunk() {}

  virtual bool read(Tifstream &is) {
    skip(is);
    return true;
  }

  void skip(Tifstream &is) { is.seekg(m_length, std::ios::cur); }

  static bool readHeader(Tifstream &is, std::string &name, TINT32 &length) {
    char cName[5];
    TINT32 len = 0;
    memset(cName, 0, sizeof(cName));

    is.read(cName, 4);
    if (is.fail()) return false;
    cName[4] = '\0';

    is.read((char *)&len, sizeof(len));
    if (is.fail()) return false;

    // WAV format stores data as little-endian
    // if platform is not little-endian, bytes need to be swapped
    if (!TNZ_LITTLE_ENDIAN) len = swapTINT32(len);

    name   = std::string(cName);
    length = len;
    return true;
  }
};

TINT32 TWAVChunk::HDR_LENGTH = 8;

//====================================================================

// FMT Chunk: Chunk containing track information

class TFMTChunk final : public TWAVChunk {
public:
  static TINT32 LENGTH;

  USHORT m_encodingType;  // PCM, ...
  USHORT m_chans;         // number of channels
  TUINT32 m_sampleRate;
  TUINT32 m_avgBytesPerSecond;
  USHORT m_bytesPerSample;
  USHORT m_bitPerSample;

  TFMTChunk(TINT32 length) : TWAVChunk("fmt ", length) {}

  bool read(Tifstream &is) override {
    is.read((char *)&m_encodingType, sizeof(m_encodingType));
    is.read((char *)&m_chans, sizeof(m_chans));
    is.read((char *)&m_sampleRate, sizeof(m_sampleRate));
    is.read((char *)&m_avgBytesPerSecond, sizeof(m_avgBytesPerSecond));
    is.read((char *)&m_bytesPerSample, sizeof(m_bytesPerSample));
    is.read((char *)&m_bitPerSample, sizeof(m_bitPerSample));

    if (!TNZ_LITTLE_ENDIAN) {
      m_encodingType      = swapUshort(m_encodingType);
      m_chans             = swapUshort(m_chans);
      m_sampleRate        = swapTINT32(m_sampleRate);
      m_avgBytesPerSecond = swapTINT32(m_avgBytesPerSecond);
      m_bytesPerSample    = swapUshort(m_bytesPerSample);
      m_bitPerSample      = swapUshort(m_bitPerSample);
    }

    assert(m_length >= 16);
    if (m_length > 16) is.seekg((long)is.tellg() + m_length - 16);
    return true;
  }

  bool write(Tofstream &os) {
    TUINT32 length         = m_length;
    USHORT type            = m_encodingType;
    USHORT chans           = m_chans;
    TUINT32 sampleRate     = m_sampleRate;
    TUINT32 bytesPerSecond = m_avgBytesPerSecond;
    USHORT bytesPerSample  = m_bytesPerSample;
    USHORT bitPerSample    = m_bitPerSample;

    if (!TNZ_LITTLE_ENDIAN) {
      length         = swapTINT32(length);
      type           = swapUshort(type);
      chans          = swapUshort(chans);
      sampleRate     = swapTINT32(sampleRate);
      bytesPerSecond = swapTINT32(bytesPerSecond);
      bytesPerSample = swapUshort(bytesPerSample);
      bitPerSample   = swapUshort(bitPerSample);
    }

    os.write((char *)"fmt ", 4);
    os.write((char *)&length, sizeof(length));
    os.write((char *)&type, sizeof(type));
    os.write((char *)&chans, sizeof(chans));
    os.write((char *)&sampleRate, sizeof(sampleRate));
    os.write((char *)&bytesPerSecond, sizeof(bytesPerSecond));
    os.write((char *)&bytesPerSample, sizeof(bytesPerSample));
    os.write((char *)&bitPerSample, sizeof(bitPerSample));

    return true;
  }
};

TINT32 TFMTChunk::LENGTH = TWAVChunk::HDR_LENGTH + 16;

//====================================================================

// DATA Chunk: Chunk containing samples

class TDATAChunk final : public TWAVChunk {
public:
  std::unique_ptr<UCHAR[]> m_samples;

  TDATAChunk(TINT32 length) : TWAVChunk("data", length) {}

  bool read(Tifstream &is) override {
    // allocate sample buffer
    m_samples.reset(new UCHAR[m_length]);
    if (!m_samples) return false;
    is.read((char *)m_samples.get(), m_length);
    return true;
  }

  bool write(Tofstream &os) {
    TINT32 length = m_length;

    if (!TNZ_LITTLE_ENDIAN) {
      length = swapTINT32(length);
    }

    os.write((char *)"data", 4);
    os.write((char *)&length, sizeof(length));
    os.write((char *)m_samples.get(), m_length);
    return true;
  }
};

//==============================================================================

TSoundTrackReaderWav::TSoundTrackReaderWav(const TFilePath &fp)
    : TSoundTrackReader(fp) {}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackReaderWav::load() {
  char chunkName[5];
  char RIFFType[5];
  TINT32 chunkLength;

  Tifstream is(m_path);

  if (!is) throw TException(m_path.getWideString() + L" : File doesn't exist");

  // read chunk name
  is.read((char *)&chunkName, sizeof(chunkName) - 1);
  chunkName[4] = '\0';

  // read chunk length
  is.read((char *)&chunkLength, sizeof(chunkLength));

  if (!TNZ_LITTLE_ENDIAN) chunkLength = swapTINT32(chunkLength);

  // read RIFFType
  is.read((char *)&RIFFType, sizeof(RIFFType) - 1);
  RIFFType[4] = '\0';

  // for .wav files, RIFFType MUST be "WAVE"
  if ((std::string(RIFFType, 4) != "WAVE"))
    throw TException("The WAV file doesn't contain the WAVE form");

  // FIXED: Use smart pointers to prevent memory leaks
  std::unique_ptr<TFMTChunk> fmtChunk;
  std::unique_ptr<TDATAChunk> dataChunk;

  while (!is.eof()) {
    std::string name = "";
    TINT32 length    = 0;

    bool ret = TWAVChunk::readHeader(is, name, length);

    if (!ret) break;

    // only read chunks we care about, i.e., FMT and DATA

    if (name == "fmt ") {
      // read FMT chunk data
      fmtChunk.reset(new TFMTChunk(length));
      fmtChunk->read(is);

      // consider pad byte at end of chunk if length is odd
      if (length & 1) {
        is.seekg((long)is.tellg() + 1);
      }
    } else if (name == "data") {
      // FIXED: If we already have a data chunk, delete it before creating new
      // one This handles the case where there might be multiple data chunks
      if (dataChunk) {
        dataChunk.reset();
      }

      // read DATA chunk data
      dataChunk.reset(new TDATAChunk(length));
      dataChunk->read(is);

      // consider pad byte at end of chunk if length is odd
      if (length & 1) {
        is.seekg((long)is.tellg() + 1);
      }
    } else {
      // skip to next chunk by moving forward by length bytes
      if (length & 1)
        is.seekg((long)is.tellg() + (long)length + 1);
      else
        is.seekg((long)is.tellg() + (long)length);
    }
  }

  TSoundTrackP track = 0;

  if (fmtChunk && dataChunk) {
    TINT32 sampleCount = dataChunk->m_length / fmtChunk->m_bytesPerSample;
    int sampleType     = 0;

    if (fmtChunk->m_encodingType == 1)  // WAVE_FORMAT_PCM
      sampleType = (fmtChunk->m_bitPerSample == 8) ? TSound::UINT : TSound::INT;
    else if (fmtChunk->m_encodingType == 3)  // WAVE_FORMAT_IEEE_FLOAT
      sampleType = TSound::FLOAT;

    if (sampleType)  // valid sample type
      track = TSoundTrack::create((int)fmtChunk->m_sampleRate,
                                  fmtChunk->m_bitPerSample, fmtChunk->m_chans,
                                  sampleCount, sampleType);

    if (track) {
      if (!TNZ_LITTLE_ENDIAN) {
        switch (fmtChunk->m_bitPerSample) {
        case 16:
          swapAndCopy16Bits((short *)dataChunk->m_samples.get(),
                            (short *)track->getRawData(),
                            sampleCount * fmtChunk->m_chans);
          break;
        case 24:
          swapAndCopy24Bits((short *)dataChunk->m_samples.get(),
                            (short *)track->getRawData(),
                            sampleCount * fmtChunk->m_chans);
          break;
        case 32:
          swapAndCopy32Bits((TINT32 *)dataChunk->m_samples.get(),
                            (TINT32 *)track->getRawData(),
                            sampleCount * fmtChunk->m_chans);
          break;
        default:
          memcpy((void *)track->getRawData(),
                 (void *)(dataChunk->m_samples.get()),
                 sampleCount * fmtChunk->m_bytesPerSample);
          break;
        }
      } else {
        memcpy((void *)track->getRawData(),
               (void *)(dataChunk->m_samples.get()),
               sampleCount * fmtChunk->m_bytesPerSample);
      }
    }
  }

  // FIXED: Smart pointers automatically clean up memory, no need for delete
  // The destructors will be called automatically when function returns

  return track;
}

//==============================================================================

TSoundTrackWriterWav::TSoundTrackWriterWav(const TFilePath &fp)
    : TSoundTrackWriter(fp) {}

//------------------------------------------------------------------------------

bool TSoundTrackWriterWav::save(const TSoundTrackP &sndtrack) {
  if (!sndtrack)
    throw TException(L"Unable to save the soundtrack: " +
                     m_path.getWideString());

  if (sndtrack->getBitPerSample() == 8 && sndtrack->isSampleSigned())
    throw TException("The format (8 bit signed) is incompatible with WAV file");

  TINT32 soundDataLength =
      (TINT32)(sndtrack->getSampleCount() * (sndtrack->getBitPerSample() / 8) *
               sndtrack->getChannelCount() /*sndtrack->getSampleSize()*/);

  TINT32 RIFFChunkLength =
      TFMTChunk::LENGTH + TWAVChunk::HDR_LENGTH + soundDataLength;

  TFileStatus fs(m_path);
  if (fs.doesExist() && !fs.isWritable())
    throw TException(L"Unable to save the soundtrack: " +
                     m_path.getWideString() + L" is read-only");

  Tofstream os(m_path);

  TFMTChunk fmtChunk(16);

  fmtChunk.m_encodingType      = sndtrack->getSampleType() & TSound::WMASK;
  fmtChunk.m_chans             = sndtrack->getChannelCount();
  fmtChunk.m_sampleRate        = sndtrack->getSampleRate();
  fmtChunk.m_avgBytesPerSecond = (sndtrack->getBitPerSample() / 8) *
                                 fmtChunk.m_chans * sndtrack->getSampleRate();
  // sndtrack->getSampleSize()*sndtrack->getSampleRate();
  fmtChunk.m_bytesPerSample = (sndtrack->getBitPerSample() / 8) *
                              fmtChunk.m_chans;  // sndtrack->getSampleSize();
  fmtChunk.m_bitPerSample = sndtrack->getBitPerSample();

  TDATAChunk dataChunk(soundDataLength);

  std::unique_ptr<UCHAR[]> waveData(new UCHAR[soundDataLength]);

  if (!TNZ_LITTLE_ENDIAN) RIFFChunkLength = swapTINT32(RIFFChunkLength);

  if (!TNZ_LITTLE_ENDIAN) {
    switch (fmtChunk.m_bitPerSample) {
    case 16:
      swapAndCopy16Bits((short *)sndtrack->getRawData(),
                        (short *)waveData.get(),
                        sndtrack->getSampleCount() * fmtChunk.m_chans);
      break;
    case 24:
      swapAndCopy24Bits((void *)sndtrack->getRawData(), (void *)waveData.get(),
                        sndtrack->getSampleCount() * fmtChunk.m_chans);
      break;
    case 32:
      swapAndCopy32Bits((TINT32 *)sndtrack->getRawData(),
                        (TINT32 *)waveData.get(),
                        sndtrack->getSampleCount() * fmtChunk.m_chans);
      break;
    default:
      memcpy((void *)waveData.get(), (void *)sndtrack->getRawData(),
             soundDataLength);
      break;
    }
  } else {
    memcpy((void *)waveData.get(), (void *)sndtrack->getRawData(),
           soundDataLength);
  }

  dataChunk.m_samples = std::move(waveData);

  os.write("RIFF", 4);
  os.write((char *)&RIFFChunkLength, sizeof(TINT32));
  os.write("WAVE", 4);
  fmtChunk.write(os);
  dataChunk.write(os);

  return true;
}
