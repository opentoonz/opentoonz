

#include "tsio_aiff.h"
#include "tmachine.h"
#include "tsound_t.h"
#include "tsystem.h"
#include "tfilepath_io.h"
#include "tsioutils.h"
#include <math.h>

#define DEFAULT_OFFSET 0
#define DEFAULT_BLOCKSIZE 0
#define AIFF_NBYTE 4
#define COMM_NBYTE 24
#define OFFSETBLOCSIZE_NBYTE 8
#define SSND_PREDATA_NBYTE 16

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    using namespace std;

TUINT32 convertToLong(UCHAR *buffer);
void storeFloat(unsigned char *buffer, TUINT32 value);

//====================================================================

// TAIFFChunk: classe base per i vari chunk

class TAIFFChunk {
public:
  string m_name;
  TINT32 m_length;  // lunghezza del chunk in byte

  TAIFFChunk(string name, TINT32 length) : m_name(name), m_length(length) {}

  virtual ~TAIFFChunk() {}

  virtual bool read(ifstream &is) {
    skip(is);
    return true;
  }

  void skip(ifstream &is) { is.seekg((TINT32)is.tellg() + (TINT32)m_length); }

  static bool readHeader(ifstream &is, string &name, TINT32 &length) {
    char cName[5];
    TINT32 len;

    is.read((char *)&cName, 4);
    if (is.fail()) return false;
    cName[4] = '\0';

    is.read((char *)&len, sizeof(len));

    if (TNZ_LITTLE_ENDIAN) len = swapTINT32(len);

    if (is.fail()) return false;

    name   = string(cName);
    length = len;
    return true;
  }
};

//====================================================================

//  COMM Chunk: Chunk contenente le informazioni sulla traccia

class TCOMMChunk final : public TAIFFChunk {
public:
  USHORT m_chans;    // numero di canali
  TUINT32 m_frames;  // numero di campioni
  USHORT m_bitPerSample;
  TUINT32 m_sampleRate;

  TCOMMChunk(string name, TINT32 length) : TAIFFChunk(name, length) {}

  bool read(ifstream &is) override {
    long pos = is.tellg();
    is.read((char *)&m_chans, sizeof(m_chans));
    is.read((char *)&m_frames, sizeof(m_frames));
    is.read((char *)&m_bitPerSample, sizeof(m_bitPerSample));

    if (TNZ_LITTLE_ENDIAN) {
      m_chans        = swapUshort(m_chans);
      m_frames       = swapTINT32(m_frames);
      m_bitPerSample = swapUshort(m_bitPerSample);
    }

    UCHAR sampleRateBuffer[10];  // sample rate come letto dallo stream
    memset(sampleRateBuffer, 0, 10);
    is.read((char *)&sampleRateBuffer, sizeof(sampleRateBuffer));
    m_sampleRate = convertToLong(sampleRateBuffer);
    is.seekg(pos + (long)m_length, is.beg);
    return true;
  }

  bool write(ofstream &os) {
    TINT32 length       = m_length;
    USHORT chans        = m_chans;
    TUINT32 frames      = m_frames;
    USHORT bitPerSample = m_bitPerSample;
    TUINT32 sampleRate  = m_sampleRate;

    if (TNZ_LITTLE_ENDIAN) {
      length       = swapTINT32(length);
      chans        = swapUshort(chans);
      frames       = swapTINT32(frames);
      bitPerSample = swapUshort(bitPerSample);
    }

    UCHAR sampleRateBuffer[10];
    storeFloat(sampleRateBuffer, sampleRate);

    // assert(convertToLong(sampleRateBuffer) == sampleRate);

    os.write((char *)"COMM", 4);
    os.write((char *)&length, sizeof(TINT32));
    os.write((char *)&chans, sizeof(short));
    os.write((char *)&frames, sizeof(TINT32));
    os.write((char *)&bitPerSample, sizeof(short));
    os.write((char *)&sampleRateBuffer, sizeof(sampleRateBuffer));

    return true;
  }

  virtual void print(ostream &os) const {
    os << "canali   = '" << m_chans << endl;
    os << "frames   = '" << (unsigned int)m_frames << endl;
    os << "bitxsam  = '" << m_bitPerSample << endl;
    os << "rate	    = '" << (unsigned int)m_sampleRate << endl;
  }
};

//--------------------------------------------------------------------

static ostream &operator<<(ostream &os, const TCOMMChunk &commChunk) {
  commChunk.print(os);
  return os;
}

//====================================================================

//  SSND Chunk: Chunk contenente i campioni della traccia

class TSSNDChunk final : public TAIFFChunk {
public:
  TUINT32 m_offset;  // dall'inizio dei sample frames tra i wavedata
  TUINT32 m_blockSize;
  std::unique_ptr<UCHAR[]> m_waveData;

  TSSNDChunk(string name, TINT32 length) : TAIFFChunk(name, length) {}

  bool read(ifstream &is) override {
    is.read((char *)&m_offset, sizeof(m_offset));
    is.read((char *)&m_blockSize, sizeof(m_blockSize));

    if (TNZ_LITTLE_ENDIAN) {
      m_offset    = swapTINT32(m_offset);
      m_blockSize = swapTINT32(m_blockSize);
    }

    // alloca il buffer dei campioni
    m_waveData.reset(new UCHAR[m_length - OFFSETBLOCSIZE_NBYTE]);
    if (!m_waveData) cout << " ERRORE " << endl;
    is.read((char *)m_waveData.get(), m_length - OFFSETBLOCSIZE_NBYTE);
    return true;
  }

  bool write(ofstream &os) {
    TINT32 length     = m_length;
    TUINT32 offset    = m_offset;
    TUINT32 blockSize = m_blockSize;

    if (TNZ_LITTLE_ENDIAN) {
      length    = swapTINT32(length);
      offset    = swapTINT32(offset);
      blockSize = swapTINT32(blockSize);
    }

    os.write((char *)"SSND", 4);
    os.write((char *)&length, sizeof(TINT32));
    os.write((char *)&offset, sizeof(TINT32));
    os.write((char *)&blockSize, sizeof(TINT32));
    os.write((char *)m_waveData.get(), m_length - OFFSETBLOCSIZE_NBYTE);
    return true;
  }
};

//--------------------------------------------------------------------

static ostream &operator<<(ostream &os, const TSSNDChunk &ssndChunk) {
  os << "name      = '" << ssndChunk.m_name << endl;
  os << "length    = '" << ssndChunk.m_length << endl;
  os << "offset    = '" << (unsigned int)ssndChunk.m_offset << endl;
  os << "blocksize = '" << (unsigned int)ssndChunk.m_blockSize << endl;

#ifdef PRINT_SAMPLES
  os << " samples" << endl;
  for (int i = 0; i < ((ssndChunk.m_length - 8) / 2); ++i)
    os << i << ((short *)*(ssndChunk.m_waveData + i)) << dec << endl;
#endif

  return os;
}

//==========================================================

static void flipLong(unsigned char *ptrc) {
  unsigned char val;

  val         = *(ptrc);
  *(ptrc)     = *(ptrc + 3);
  *(ptrc + 3) = val;
  ptrc += 1;
  val         = *(ptrc);
  *(ptrc)     = *(ptrc + 1);
  *(ptrc + 1) = val;
}

//--------------------------------------------------------------------

static TUINT32 fetchLong(TUINT32 *ptrl) { return (*ptrl); }

//--------------------------------------------------------------------

TUINT32 convertToLong(UCHAR *buffer) {
  TUINT32 mantissa;
  TUINT32 last = 0;
  UCHAR exp;

  if (TNZ_LITTLE_ENDIAN) {
    // flipLong((TUINT32 *) (buffer+2));
    flipLong(buffer + 2);
  }

  mantissa = *((TUINT32 *)(buffer + 2));
  exp      = 30 - *(buffer + 1);
  while (exp--) {
    last = mantissa;
    mantissa >>= 1;
  }

  if (last & 0x00000001) mantissa++;
  return (mantissa);
}

//--------------------------------------------------------------------

static void storeLong(TUINT32 val, TUINT32 *ptr) { *ptr = val; }

//--------------------------------------------------------------------

void storeFloat(unsigned char *buffer, TUINT32 value) {
  TUINT32 exp;
  unsigned char i;

  memset(buffer, 0, 10);
  exp = value;
  exp >>= 1;
  for (i = 0; i < 32; i++) {
    exp >>= 1;
    if (!exp) break;
  }
  *(buffer + 1) = i;

  for (i = 32; i; i--) {
    if (value & 0x80000000) break;
    value <<= 1;
  }

  *((TUINT32 *)(buffer + 2)) = value;

  buffer[0] = 0x40;

  if (TNZ_LITTLE_ENDIAN) {
    // flipLong((TUINT32*) (buffer+2));
    flipLong(buffer + 2);
  }
}

//==============================================================================

TSoundTrackReaderAiff::TSoundTrackReaderAiff(const TFilePath &fp)
    : TSoundTrackReader(fp) {}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackReaderAiff::load() {
  char ckID[5];
  char formType[5];
  TINT32 ckSize;

  Tifstream is(m_path);

  if (!is)
    throw TException(L"Unable to load the AIFF file " + m_path.getWideString() +
                     L" : doesn't exist");

  // legge il chunk ID
  is.read((char *)&ckID, sizeof(ckID) - 1);
  ckID[4] = '\0';

  // legge il chunk Size
  is.read((char *)&ckSize, sizeof(ckSize));

  if (TNZ_LITTLE_ENDIAN) ckSize = swapTINT32(ckSize);

  // legge il formType
  is.read((char *)&formType, sizeof(formType) - 1);
  formType[4] = '\0';

  // il formType DEVE essere uguale a "AIFF"
  if ((string(formType, 4) != "AIFF") && (string(formType, 4) != "AIFC"))
    throw TException("The AIFF file doesn't contain the AIFF form");

  TCOMMChunk *commChunk = 0;
  TSSNDChunk *ssndChunk = 0;

  while (!is.eof()) {
    string name;
    TINT32 length;

    bool ret = TAIFFChunk::readHeader(is, name, length);

    if (!ret) break;

    // legge solo i chunk che ci interessano, ossia COMM e SSND

    if (name == "COMM") {
      // legge i dati del chunk COMM
      commChunk = new TCOMMChunk("COMM", length);
      commChunk->read(is);

      // considera il byte di pad alla fine del chunk nel caso
      // in cui la lunghezza di questi e' dispari
      if (length % 2) is.seekg((TINT32)is.tellg() + 1);
    } else if (name == "SSND") {
      // legge i dati del chunk SSND
      ssndChunk = new TSSNDChunk("SSND", length);
      ssndChunk->read(is);

      // considera il byte di pad alla fine del chunk nel caso
      // in cui la lunghezza di questi e' dispari
      if (length % 2) is.seekg((TINT32)is.tellg() + 1);
    } else {
      // spostati nello stream di un numero di byte pari a length
      if (!(length % 2))
        is.seekg((TINT32)is.tellg() + length);
      else
        is.seekg((TINT32)is.tellg() + (TINT32)length + 1);
    }
  }

  TSoundTrack *track = 0;

  if (commChunk && ssndChunk) {
    if (commChunk->m_chans < 1)
      throw TException("Invalid channels number in sound file");

    if (commChunk->m_chans > 2)
      throw TException("Unsupported channels number in sound file");

    switch (commChunk->m_bitPerSample) {
    case 8:
      if (commChunk->m_chans == 1)
        track = new TSoundTrackMono8Signed(commChunk->m_sampleRate, 1,
                                           (TINT32)commChunk->m_frames);
      else
        track = new TSoundTrackStereo8Signed(commChunk->m_sampleRate, 2,
                                             (TINT32)commChunk->m_frames);

      memcpy((void *)track->getRawData(),
             (void *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
             commChunk->m_frames * commChunk->m_chans);
      break;

    case 16:
      if (commChunk->m_chans == 1)
        track = new TSoundTrackMono16(commChunk->m_sampleRate, 1,
                                      (TINT32)commChunk->m_frames);
      else  // due canali
        track = new TSoundTrackStereo16(commChunk->m_sampleRate, 2,
                                        (TINT32)commChunk->m_frames);

      if (!TNZ_LITTLE_ENDIAN)
        memcpy((void *)track->getRawData(),
               (void *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
               commChunk->m_frames * track->getSampleSize());
      else
        swapAndCopy16Bits(
            (short *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
            (short *)track->getRawData(),
            (TINT32)(commChunk->m_frames * commChunk->m_chans));
      break;

    case 24:
      if (commChunk->m_chans == 1)
        track = new TSoundTrackMono24(commChunk->m_sampleRate, 1,
                                      (TINT32)commChunk->m_frames);
      else  // due canali
        track = new TSoundTrackStereo24(commChunk->m_sampleRate, 2,
                                        (TINT32)commChunk->m_frames);

       if (!TNZ_LITTLE_ENDIAN)
        memcpy((void *)track->getRawData(),
               (void *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
               commChunk->m_frames * track->getSampleSize());
      else
         swapAndCopy24Bits(
             (void *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
             (void *)track->getRawData(),
             (TINT32)(commChunk->m_frames * commChunk->m_chans));
       break;
    case 32:
      if (commChunk->m_chans == 1)
        track = new TSoundTrackMono32Float(commChunk->m_sampleRate, 1,
                                           (TINT32)commChunk->m_frames);
      else  // due canali
        track = new TSoundTrackStereo32Float(commChunk->m_sampleRate, 2,
                                             (TINT32)commChunk->m_frames);

       if (!TNZ_LITTLE_ENDIAN)
        memcpy((void *)track->getRawData(),
               (void *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
               commChunk->m_frames * track->getSampleSize());
      else
         swapAndCopy32Bits(
             (TINT32 *)(ssndChunk->m_waveData.get() + ssndChunk->m_offset),
             (TINT32 *)track->getRawData(),
             (TINT32)(commChunk->m_frames * commChunk->m_chans));
       break;
    }

    if (commChunk) delete commChunk;
    if (ssndChunk) delete ssndChunk;
  }

  return track;
}

//==============================================================================

TSoundTrackWriterAiff::TSoundTrackWriterAiff(const TFilePath &fp)
    : TSoundTrackWriter(fp) {}

//------------------------------------------------------------------------------

bool TSoundTrackWriterAiff::save(const TSoundTrackP &st) {
  if (!st)
    throw TException(L"Unable to save the soundtrack: " +
                     m_path.getWideString());

  TSoundTrackP sndtrack;
  if (st->getBitPerSample() == 8 && !st->isSampleSigned())
    throw TException(
        "The format (8 bit unsigned) is incompatible with AIFF file");
  else
    sndtrack = st;

  TINT32 soundDataCount =
      (TINT32)(sndtrack->getSampleCount() * sndtrack->getChannelCount() *
               tceil(sndtrack->getBitPerSample() / 8));

  TINT32 postHeadData =
      AIFF_NBYTE + COMM_NBYTE + SSND_PREDATA_NBYTE + soundDataCount;

  TFileStatus fs(m_path);
  if (fs.doesExist() && !fs.isWritable())
    throw TException(L"Unable to save the soundtrack: " +
                     m_path.getWideString() + L" is read-only");

  Tofstream os(m_path);

  TCOMMChunk commChunk("COMM", 18);
  commChunk.m_chans  = sndtrack->getChannelCount();
  commChunk.m_frames = sndtrack->getSampleCount();
  commChunk.m_bitPerSample =
      sndtrack->getBitPerSample();  // assumendo che non ci siano 12 bit
  commChunk.m_sampleRate = sndtrack->getSampleRate();

  TSSNDChunk ssndChunk("SSND", soundDataCount + OFFSETBLOCSIZE_NBYTE);
  ssndChunk.m_offset    = DEFAULT_OFFSET;
  ssndChunk.m_blockSize = DEFAULT_BLOCKSIZE;

  std::unique_ptr<UCHAR[]> waveData(new UCHAR[soundDataCount]);

  if (TNZ_LITTLE_ENDIAN) {
    postHeadData = swapTINT32(postHeadData);

    switch (commChunk.m_bitPerSample) {
    case 16:
      swapAndCopy16Bits((short *)sndtrack->getRawData(),
                        (short *)waveData.get(),
                        (TINT32)(commChunk.m_frames * commChunk.m_chans));
      break;
    case 24:
      swapAndCopy24Bits((void *)sndtrack->getRawData(), (void *)waveData.get(),
                        (TINT32)(commChunk.m_frames * commChunk.m_chans));
      break;
    case 32:
      swapAndCopy32Bits((TINT32 *)sndtrack->getRawData(),
                        (TINT32 *)waveData.get(),
                        (TINT32)(commChunk.m_frames * commChunk.m_chans));
      break;
    default:
      memcpy((void *)waveData.get(), (void *)sndtrack->getRawData(),
             soundDataCount);
      break;
    }
  } else {
    memcpy((void *)waveData.get(), (void *)sndtrack->getRawData(),
           soundDataCount);
  }

  ssndChunk.m_waveData = std::move(waveData);

  os.write("FORM", 4);
  os.write((char *)&postHeadData, sizeof(TINT32));
  os.write("AIFF", 4);
  commChunk.write(os);
  ssndChunk.write(os);

  return true;
}
