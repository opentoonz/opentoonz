#include <memory>

#include "tmachine.h"
#include "tsio_wav.h"
#include "tsystem.h"
#include "tfilepath_io.h"

using namespace std;

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    //------------------------------------------------------------------------------

    void
    swapAndCopySamples(short *srcBuffer, short *dstBuffer, TINT32 sampleCount);

//==============================================================================

// TWAVChunk: classe base per i vari chunk WAV

class TWAVChunk {
public:
  static TINT32 HDR_LENGTH;

  string m_name;
  TINT32 m_length;  // lunghezza del chunk in byte

  TWAVChunk(string name, TINT32 length) : m_name(name), m_length(length) {}

  virtual ~TWAVChunk() {}

  virtual bool read(Tifstream &is) {
    skip(is);
    return true;
  }

  void skip(Tifstream &is) { is.seekg(m_length, ios::cur); }

  static bool readHeader(Tifstream &is, string &name, TINT32 &length) {
    char cName[5];
    TINT32 len = 0;
    memset(cName, 0, sizeof(cName));

    is.read(cName, 4);
    if (is.fail()) return false;
    cName[4] = '\0';

    is.read((char *)&len, sizeof(len));
    if (is.fail()) return false;

    // il formato WAV memorizza i dati come little-endian
    // se la piattaforma non e' little-endian bisogna scambiare i byte
    if (!TNZ_LITTLE_ENDIAN) len = swapTINT32(len);

    name   = string(cName);
    length = len;
    return true;
  }
};

TINT32 TWAVChunk::HDR_LENGTH = 8;

//====================================================================

//  FMT Chunk: Chunk contenente le informazioni sulla traccia

class TFMTChunk final : public TWAVChunk {
public:
  static TINT32 LENGTH;

  USHORT m_encodingType;  // PCM, ...
  USHORT m_chans;         // numero di canali
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

  bool write(ofstream &os) {
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

//  DATA Chunk: Chunk contenente i campioni

class TDATAChunk final : public TWAVChunk {
public:
  std::unique_ptr<UCHAR[]> m_samples;

  TDATAChunk(TINT32 length) : TWAVChunk("data", length) {}

  bool read(Tifstream &is) override {
    // alloca il buffer dei campioni
    m_samples.reset(new UCHAR[m_length]);
    if (!m_samples) return false;
    is.read((char *)m_samples.get(), m_length);
    return true;
  }

  bool write(ofstream &os) {
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

  // legge il nome del chunk
  is.read((char *)&chunkName, sizeof(chunkName) - 1);
  chunkName[4] = '\0';

  // legge la lunghezza del chunk
  is.read((char *)&chunkLength, sizeof(chunkLength));

  if (!TNZ_LITTLE_ENDIAN) chunkLength = swapTINT32(chunkLength);

  // legge il RIFFType
  is.read((char *)&RIFFType, sizeof(RIFFType) - 1);
  RIFFType[4] = '\0';

  // per i .wav il RIFFType DEVE essere uguale a "WAVE"
  if ((string(RIFFType, 4) != "WAVE"))
    throw TException("The WAV file doesn't contain the WAVE form");

  TFMTChunk *fmtChunk   = 0;
  TDATAChunk *dataChunk = 0;

  while (!is.eof()) {
    string name   = "";
    TINT32 length = 0;

    bool ret = TWAVChunk::readHeader(is, name, length);

    if (!ret) break;

    // legge solo i chunk che ci interessano, ossia FMT e DATA

    if (name == "fmt ") {
      // legge i dati del chunk FMT
      fmtChunk = new TFMTChunk(length);
      fmtChunk->read(is);

      // considera il byte di pad alla fine del chunk nel caso
      // in cui la lunghezza di questi e' dispari
      if (length & 1) {
        is.seekg((long)is.tellg() + 1);
      }
    } else if (name == "data") {
      // legge i dati del chunk DATA
      dataChunk = new TDATAChunk(length);

      dataChunk->read(is);

      // considera il byte di pad alla fine del chunk nel caso
      // in cui la lunghezza di questi e' dispari
      if (length & 1) {
        is.seekg((long)is.tellg() + 1);
      }
    } else {
      // spostati nello stream di un numero di byte pari a length
      if (length & 1)
        is.seekg((long)is.tellg() + (long)length + 1);
      else
        is.seekg((long)is.tellg() + (long)length);
    }
  }

  TSoundTrackP track = 0;

  if (fmtChunk && dataChunk) {
    TINT32 sampleCount = dataChunk->m_length / fmtChunk->m_bytesPerSample;
    bool signedSample  = (fmtChunk->m_bitPerSample != 8);

    track = TSoundTrack::create((int)fmtChunk->m_sampleRate,
                                fmtChunk->m_bitPerSample, fmtChunk->m_chans,
                                sampleCount, signedSample);

    if (track) {
      switch (fmtChunk->m_bitPerSample) {
      case 8:
        memcpy((void *)track->getRawData(),
               (void *)(dataChunk->m_samples.get()),
               sampleCount * fmtChunk->m_bytesPerSample);
        break;
      case 16:
        if (!TNZ_LITTLE_ENDIAN)
          swapAndCopySamples((short *)dataChunk->m_samples.get(),
                             (short *)track->getRawData(),
                             sampleCount * fmtChunk->m_chans);
        else
          memcpy((void *)track->getRawData(),
                 (void *)(dataChunk->m_samples.get()),
                 sampleCount * fmtChunk->m_bytesPerSample);
        //#endif
        break;
      case 24:
        if (!TNZ_LITTLE_ENDIAN) {
          UCHAR *begin = (UCHAR *)track->getRawData();
          for (int i = 0; i < (int)(sampleCount * fmtChunk->m_chans); ++i) {
            *(begin + 4 * i)     = 0;
            *(begin + 4 * i + 1) = *(dataChunk->m_samples.get() + 3 * i + 2);
            *(begin + 4 * i + 2) = *(dataChunk->m_samples.get() + 3 * i + 1);
            *(begin + 4 * i + 3) = *(dataChunk->m_samples.get() + 3 * i);
          }
        } else {
          UCHAR *begin = (UCHAR *)track->getRawData();
          for (int i = 0; i < (int)(sampleCount * fmtChunk->m_chans); ++i) {
            memcpy((void *)(begin + 4 * i),
                   (void *)(dataChunk->m_samples.get() + 3 * i), 3);
            *(begin + 4 * i + 3) = 0;
          }
        }
        //#endif
        break;
      }
    }

    /*if (!TNZ_LITTLE_ENDIAN)
{
if (fmtChunk->m_bitPerSample > 8)
{
assert(fmtChunk->m_bitPerSample <= 16);
swapAndCopySamples(
(short*)dataChunk->m_samples,
(short*)track->getRawData(),
sampleCount*fmtChunk->m_chans);
}
else
memcpy(
    (void*)track->getRawData(),
(void*)(dataChunk->m_samples),
sampleCount*fmtChunk->m_bytesPerSample);
}
else
memcpy(
(void*)track->getRawData(),
(void*)(dataChunk->m_samples),
sampleCount*fmtChunk->m_bytesPerSample);*/
  }

  if (fmtChunk) delete fmtChunk;
  if (dataChunk) delete dataChunk;

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

  fmtChunk.m_encodingType      = 1;  // PCM
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

// era if defined(MACOSX)
#if (!TNZ_LITTLE_ENDIAN)
  {
    if (fmtChunk.m_bitPerSample == 8)
      memcpy((void *)waveData.get(), (void *)sndtrack->getRawData(), soundDataLength);
    else if (fmtChunk.m_bitPerSample == 16) {
      swapAndCopySamples((short *)sndtrack->getRawData(), (short *)waveData.get(),
                         sndtrack->getSampleCount() * fmtChunk.m_chans);
    } else if (fmtChunk.m_bitPerSample == 24) {  // swap e togliere quarto byte
      UCHAR *begin = (UCHAR *)sndtrack->getRawData();
      for (int i = 0; i < (int)sndtrack->getSampleCount() * fmtChunk.m_chans;
           ++i) {
        *(waveData.get() + 3 * i)     = *(begin + 4 * i + 3);
        *(waveData.get() + 3 * i + 1) = *(begin + 4 * i + 2);
        *(waveData.get() + 3 * i + 2) = *(begin + 4 * i + 1);
      }
    }
  }
#else
  {
    if (fmtChunk.m_bitPerSample != 24)
      memcpy((void *)waveData.get(), (void *)sndtrack->getRawData(),
             soundDataLength);
    else {  // togliere quarto byte
      UCHAR *begin = (UCHAR *)sndtrack->getRawData();
      for (int i = 0; i < (int)sndtrack->getSampleCount() * fmtChunk.m_chans;
           ++i) {
        *(waveData.get() + 3 * i)     = *(begin + 4 * i);
        *(waveData.get() + 3 * i + 1) = *(begin + 4 * i + 1);
        *(waveData.get() + 3 * i + 2) = *(begin + 4 * i + 2);
      }
    }
  }
#endif
  dataChunk.m_samples = std::move(waveData);

  os.write("RIFF", 4);
  os.write((char *)&RIFFChunkLength, sizeof(TINT32));
  os.write("WAVE", 4);
  fmtChunk.write(os);
  dataChunk.write(os);

  return true;
}
