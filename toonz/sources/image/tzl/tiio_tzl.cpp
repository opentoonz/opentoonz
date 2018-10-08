

#include "tiio_tzl.h"
#include "tmachine.h"
#include "tsystem.h"
#include "ttoonzimage.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tpixelcm.h"
#include "tpalette.h"
#include "tcodec.h"
#include <stdio.h>
#include "trastercm.h"
#include "timageinfo.h"
#include "timagecache.h"
#include "tcontenthistory.h"
#include "tropcm.h"
#include "tenv.h"
#include "tconvert.h"
#include "trasterimage.h"

#include <QByteArray>

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    const int CURRENT_VERSION = 14;
const int CREATOR_LENGTH      = 40;

namespace {

char *reverse(char *buffer, int size) {
  char *start = buffer;
  char *end   = start + size;
  while (start < --end) {
    char tmp = *start;
    *start++ = *end;
    *end     = tmp;
  }
  return buffer;
}
}

static int tfwrite(const char *data, const unsigned int count, FILE *f) {
  return fwrite(data, sizeof(char), count, f);
}

static int tfwrite(UCHAR *data, const unsigned int count, FILE *f) {
  return fwrite(data, sizeof(char), count, f);
}

static int tfwrite(TINT32 *data, const unsigned int count, FILE *f) {
  if (count == 1) {
#if !TNZ_LITTLE_ENDIAN
    TUINT32 dataToWrite = swapTINT32(*data);
#else
    TUINT32 dataToWrite = *data;
#endif
    return fwrite(&dataToWrite, sizeof(TINT32), 1, f);
  }
  assert(0);
  return 0;
}

static int tfwrite(TUINT32 *data, const unsigned int count, FILE *f) {
  if (count == 1) {
#if !TNZ_LITTLE_ENDIAN
    TUINT32 dataToWrite = swapTINT32(*data);
#else
    TUINT32 dataToWrite = *data;
#endif
    return fwrite(&dataToWrite, sizeof(TINT32), 1, f);
  }
  assert(0);
  return 0;
}

static int tfwrite(double *data, unsigned int count, FILE *f) {
  if (count == 1) {
    double v  = *data;
    char *ptr = (char *)&v;
#if !TNZ_LITTLE_ENDIAN
    ptr = reverse((char *)&v, sizeof(double));
#endif
    return fwrite(ptr, sizeof(double), 1, f);
  }
  assert(0);
  return 0;
}

//===================================================================
//
// TImageReaderTzl
//
//-------------------------------------------------------------------

namespace {

bool erasedFrame;  // Vera se è stato rimosso almeno un frame.

bool writeVersionAndCreator(FILE *chan, const char *version, QString creator) {
  if (!chan) return false;
  tfwrite(version, strlen(version), chan);

  // creator : CREATOR_LENGTH characater
  char s[CREATOR_LENGTH];
  if (creator.length() == 0) creator = "UNKNOWN";
  memset(s, 0, sizeof s);
  if (creator.length() > CREATOR_LENGTH - 1)
#if QT_VERSION >= 0x050000
    memcpy(s, creator.toLatin1(), CREATOR_LENGTH - 1);
#else
    memcpy(s, creator.toAscii(), CREATOR_LENGTH - 1);
#endif
  else
#if QT_VERSION >= 0x050000
    memcpy(s, creator.toLatin1(), creator.length());
#else
    memcpy(s, creator.toAscii(), creator.length());
#endif
  tfwrite(s, CREATOR_LENGTH, chan);

  return true;
}

bool readVersion(FILE *chan, int &version) {
  char magic[8];
  memset(magic, 0, sizeof(magic));
  fread(&magic, sizeof(char), 8, chan);
  if (memcmp(magic, "TLV10", 5) == 0) {
    version = 10;
  } else if (memcmp(magic, "TLV11", 5) == 0) {
    version = 11;
  } else if (memcmp(magic, "TLV12", 5) == 0) {
    version = 12;
  } else if (memcmp(magic, "TLV13", 5) == 0) {
    version = 13;
  } else if (memcmp(magic, "TLV14", 5) == 0) {
    version = 14;
  } else {
    return false;
  }
  return true;
}

bool readHeaderAndOffsets(FILE *chan, TzlOffsetMap &frameOffsTable,
                          TzlOffsetMap &iconOffsTable, TDimension &res,
                          int &version, QString &creator, TINT32 *_frameCount,
                          TINT32 *_offsetTablePos, TINT32 *_iconOffsetTablePos,
                          TLevelP level) {
  TINT32 hdrSize;
  TINT32 lx = 0, ly = 0, frameCount = 0;
  char codec[4];
  TINT32 offsetTablePos;
  TINT32 iconOffsetTablePos;
  // char magic[8];

  assert(frameOffsTable.empty());
  assert(iconOffsTable.empty());

  if (!readVersion(chan, version)) return false;

  // read creator
  if (version == 14) {
    char buffer[CREATOR_LENGTH + 1];
    memset(buffer, 0, sizeof buffer);
    fread(&buffer, sizeof(char), CREATOR_LENGTH, chan);
    creator = buffer;
  }

  fread(&hdrSize, sizeof(TINT32), 1, chan);
  fread(&lx, sizeof(TINT32), 1, chan);
  fread(&ly, sizeof(TINT32), 1, chan);
  fread(&frameCount, sizeof(TINT32), 1, chan);

  if (version > 10) {
    fread(&offsetTablePos, sizeof(TINT32), 1, chan);
    fread(&iconOffsetTablePos, sizeof(TINT32), 1, chan);
#if !TNZ_LITTLE_ENDIAN
    offsetTablePos     = swapTINT32(offsetTablePos);
    iconOffsetTablePos = swapTINT32(iconOffsetTablePos);
#endif
  }

  fread(&codec, 4, 1, chan);

#if !TNZ_LITTLE_ENDIAN
  hdrSize    = swapTINT32(hdrSize);
  lx         = swapTINT32(lx);
  ly         = swapTINT32(ly);
  frameCount = swapTINT32(frameCount);
#endif
  assert(0 < frameCount && frameCount < 60000);

  if (version > 10 && offsetTablePos != 0 && iconOffsetTablePos != 0) {
    // assert(offsetTablePos>0);
    assert(frameCount > 0);

    fseek(chan, offsetTablePos, SEEK_SET);
    TFrameId oldFid(TFrameId::EMPTY_FRAME);
    for (int i = 0; i < (int)frameCount; i++) {
      TINT32 number, offs, length;
      char letter;
      fread(&number, sizeof(TINT32), 1, chan);
      fread(&letter, sizeof(char), 1, chan);
      fread(&offs, sizeof(TINT32), 1, chan);
      if (version >= 12) fread(&length, sizeof(TINT32), 1, chan);

#if !TNZ_LITTLE_ENDIAN
      number                    = swapTINT32(number);
      offs                      = swapTINT32(offs);
      if (version == 12) length = swapTINT32(length);
#endif
      //		std::cout << "#" << i << std::hex << " n 0x" << number
      //<< " l 0x" << letter << " o 0x" << offs << std::dec << std::endl;

      TFrameId fid(number, letter);
      // assert(i==0 || oldFid<fid);

      if (version >= 12) {
        frameOffsTable[fid] = TzlChunk(offs, length);
      } else {
        frameOffsTable[fid] = TzlChunk(offs, 0);
        if (i > 0) {
          frameOffsTable[oldFid].m_length =
              offs - frameOffsTable[oldFid].m_offs;
          assert(frameOffsTable[oldFid].m_length > 0);
        }
        if (i == frameCount - 1) {
          frameOffsTable[fid].m_length = offsetTablePos - offs;
          assert(frameOffsTable[fid].m_length > 0);
        }
      }
      oldFid = fid;
      if (level) level->setFrame(fid, TImageP());
    }
    if (version >= 13) {
      // Build IconOffsetTable
      fseek(chan, iconOffsetTablePos, SEEK_SET);

      for (int i = 0; i < (int)frameCount; i++) {
        TINT32 number, thumbnailOffs, thumbnailLength;
        char letter;
        fread(&number, sizeof(TINT32), 1, chan);
        fread(&letter, sizeof(char), 1, chan);
        fread(&thumbnailOffs, sizeof(TINT32), 1, chan);
        fread(&thumbnailLength, sizeof(TINT32), 1, chan);

#if !TNZ_LITTLE_ENDIAN
        number          = swapTINT32(number);
        thumbnailOffs   = swapTINT32(thumbnailOffs);
        thumbnailLength = swapTINT32(thumbnailLength);
#endif
        TFrameId fid(number, letter);
        iconOffsTable[fid] = TzlChunk(thumbnailOffs, thumbnailLength);
      }
    }
  } else {
    // m_frameOffsTable.resize(frameCount);
    frameOffsTable[TFrameId(1)] = TzlChunk(ftell(chan), 0);
    iconOffsTable[TFrameId(1)]  = TzlChunk(ftell(chan), 0);
    int i;
    for (i = 2; i <= (int)frameCount; i++) {
      frameOffsTable[TFrameId(i)] = TzlChunk(0, 0);
      iconOffsTable[TFrameId(i)]  = TzlChunk(0, 0);
    }
    if (level)
      for (i = 1; i <= (int)frameCount; i++)
        level->setFrame(TFrameId(i), TImageP());
  }

  res.lx                                        = lx;
  res.ly                                        = ly;
  if (_offsetTablePos) *_offsetTablePos         = offsetTablePos;
  if (_iconOffsetTablePos) *_iconOffsetTablePos = iconOffsetTablePos;

  if (_frameCount) *_frameCount = frameCount;

  return true;
}
}

static bool adjustIconAspectRatio(TDimension &outDimension,
                                  TDimension inDimension, TDimension imageRes) {
  TINT32 iconLx = inDimension.lx, iconLy = inDimension.ly;
  assert(iconLx > 0 && iconLy > 0);
  assert(imageRes.lx > 0 && imageRes.ly > 0);
  if (iconLx <= 0 || iconLy <= 0 || imageRes.lx <= 0 || imageRes.ly <= 0)
    return false;
  int lx = imageRes.lx;
  int ly = imageRes.ly;

  if (std::max(double(lx) / inDimension.lx, double(ly) / inDimension.ly) ==
      double(ly) / inDimension.ly)
    iconLx = tround((double(lx) * inDimension.ly) / ly);
  else
    iconLy     = tround((double(ly) * inDimension.lx) / lx);
  outDimension = TDimension(iconLx, iconLy);
  return true;
}

static void getThumbnail(TRasterP ras, int shrink, TRasterP &thumbnail) {
  int j          = 0;
  int y          = 0;
  TPixel32 *pix2 = (TPixel32 *)thumbnail->getRawData();
  for (j = 0; j < thumbnail->getLy(); j++) {
    ras->lock();
    TPixel32 *pix    = (TPixel32 *)ras->getRawData() + y * ras->getLx();
    TPixel32 *endPix = pix2 + thumbnail->getLx();
    while (pix2 < endPix) {
      *pix2++ = *pix;
      pix += shrink;
    }
    y += shrink;
    ras->unlock();
  }
}

//-------------------------------------------------------------------

class TImageReaderTzl final : public TImageReader {
public:
  TImageReaderTzl(const TFilePath &f, const TFrameId &fid, TLevelReaderTzl *);
  ~TImageReaderTzl() {}

private:
  // not implemented
  TImageReaderTzl(const TImageReaderTzl &);
  TImageReaderTzl &operator=(const TImageReaderTzl &src);
  TImageP load10();
  TImageP load11();
  TImageP load13();  // Aggiunta iconcine
  TImageP load14();  //	Aggiunto creator con lunghezza fissa
  const TImageInfo *getImageInfo10() const;
  const TImageInfo *getImageInfo11() const;  // vale anche le versioni > di 11

public:
  //! Indice del frame del livello
  TFrameId m_fid;
  TImageP load() override;
  TImageP loadIcon() override {
    m_isIcon = true;
    return load();
  }
  const TImageInfo *getImageInfo() const override;

  // TImageP doLoad();

  TDimension getSize() const;
  TRect getBBox() const;

private:
  //! Size of image
  int m_lx, m_ly;
  bool m_isIcon;
  //! Reference to level reader
  TLevelReaderTzl *m_lrp;
};

//===================================================================
//
// TImageWriterTzl
//
//-------------------------------------------------------------------

class TImageWriterTzl final : public TImageWriter {
  //! Reference to level writer
  TLevelWriterTzl *m_lwp;
  TFrameId m_fid;
  TDimension
      m_iconSize;  // Dimensioni dell'iconcina salvata all'interno del file tlv
  //	In genere questo parametro viene settato come quello impostato
  // dall'utente
  // nelle preferenze.
public:
  TImageWriterTzl(TLevelWriterTzl *lw, TFrameId fid)
      : TImageWriter(TFilePath())
      , m_lwp(lw)
      , m_fid(fid)
      , m_iconSize(TDimension(80, 60)) {}
  ~TImageWriterTzl() {}

private:
  // not implemented
  TImageWriterTzl(const TImageWriterTzl &);
  TImageWriterTzl &operator=(const TImageWriterTzl &src);

public:
  void save(const TImageP &img) override { m_lwp->save(img, m_fid); }
};
/*
//===================================================================
//
// TWriterInfo
//
//-------------------------------------------------------------------


TWriterInfo *TWriterInfoTzl::create(const std::string &ext)
{
  return new TWriterInfoTzl();
}

//-------------------------------------------------------------------

TWriterInfo *TWriterInfoTzl::clone() const
{
  return new TWriterInfoTzl(*this);
}

//-------------------------------------------------------------------
*/
namespace {

class Header {
public:
  enum RasType {
    Raster32RGBM,
    Raster64RGBM,
    Raster32CM,
    RasterGR8,
    RasterUnknown
  };

  int m_lx;
  int m_ly;
  RasType m_rasType;
};

}  // namespace

//-------------------------------------------------------------------

//-------------------------------------------------------------------

void TLevelWriterTzl::buildFreeChunksTable() {
  std::set<TzlChunk> occupiedChunks;
  TzlOffsetMap::const_iterator it1 = m_frameOffsTable.begin();
  TINT32 lastOccupiedPos = 0;  // ultima posizione all'interno del file occupata
                               // dall'ultima immagine(grande o icona)

  while (it1 != m_frameOffsTable.end()) {
    occupiedChunks.insert(TzlChunk(it1->second.m_offs, it1->second.m_length));
    if (it1->second.m_offs + it1->second.m_length > lastOccupiedPos)
      lastOccupiedPos = it1->second.m_offs + it1->second.m_length - 1;
    it1++;
  }
  TzlOffsetMap::const_iterator iconIt1 = m_iconOffsTable.begin();
  while (iconIt1 != m_iconOffsTable.end()) {
    occupiedChunks.insert(
        TzlChunk(iconIt1->second.m_offs, iconIt1->second.m_length));
    if (iconIt1->second.m_offs + iconIt1->second.m_length > lastOccupiedPos)
      lastOccupiedPos = iconIt1->second.m_offs + iconIt1->second.m_length - 1;
    iconIt1++;
  }

  std::set<TzlChunk>::const_iterator it2 = occupiedChunks.begin();
  TINT32 curPos;  // prima posizione utile nel file in cui vengono memorizzati i
                  // dati relativi alle immagini
  if (m_version == 13)
    curPos = 6 * sizeof(TINT32) + 4 * sizeof(char) + 8 * sizeof(char);
  else if (m_version == 14)
    curPos = 6 * sizeof(TINT32) + 4 * sizeof(char) + 8 * sizeof(char) +
             CREATOR_LENGTH * sizeof(char);
  else
    curPos = it2->m_offs;

  while (it2 != occupiedChunks.end()) {
    assert(it2->m_offs >= curPos);
    if (it2->m_offs > curPos)
      m_freeChunks.insert(TzlChunk(curPos, it2->m_offs - curPos));
    curPos = it2->m_offs + it2->m_length;
    it2++;
  }
  assert(lastOccupiedPos < m_offsetTablePos);
  if (lastOccupiedPos + 1 < m_offsetTablePos)
    m_freeChunks.insert(
        TzlChunk(lastOccupiedPos + 1, m_offsetTablePos - lastOccupiedPos));
}

//===================================================================
//
// TLevelWriterTzl
//
//-------------------------------------------------------------------

TLevelWriterTzl::TLevelWriterTzl(const TFilePath &path, TPropertyGroup *info)
    : TLevelWriter(path, info)
    , m_headerWritten(false)
    , m_creatorWritten(false)
    , m_chan(0)
    , m_frameCountPos(0)
    , m_frameCount(0)
    , m_palette(0)
    , m_res(0, 0)
    , m_exists(false)
    , m_version(CURRENT_VERSION)
    , m_updatedIconsSize(false)
    , m_currentIconSize(0, 0)
    , m_iconSize(TDimension(80, 60))
    , m_userIconSize(TDimension(80, 60))
    , m_adjustRatio(false)
    , m_codec(new TRasterCodecLZO("LZO", true))
    , m_overwritePaletteFlag(true) {
  m_path        = path;
  m_palettePath = path.withNoFrame().withType("tpl");
  TFileStatus fs(path);
  m_magic     = "TLV14B1a";  // actual version
  erasedFrame = false;
  // version TLV10B1a: first version
  // version TLV11B1a: added frameIds
  // version TLV12B1a: incremental writings
  // version TLV13B1a: added thumbnails
  // version TLV15B1a: add creator string (fixed size = CREATOR_LENGTH char)

  if (fs.doesExist()) {
    // if (!fs.isWritable())
    m_chan = fopen(path, "rb+");
    /*--- 誰かが開いている、または権限が無いとき ---*/
    if (!m_chan) {
      throw TSystemException(path, "can't fopen.");
    }
    /*--- TLVファイルのヘッダが正しく読めなかった場合 ---*/
    if (!readHeaderAndOffsets(m_chan, m_frameOffsTable, m_iconOffsTable, m_res,
                              m_version, m_creator, &m_frameCount,
                              &m_offsetTablePos, &m_iconOffsetTablePos, 0)) {
      throw TSystemException(path, "can't readHeaderAndOffsets.");
    } else {
      if (m_version >= 12) buildFreeChunksTable();
      m_headerWritten = true;
      m_exists        = true;
      if (m_version == 14)
        m_frameCountPos = 8 + CREATOR_LENGTH + 3 * sizeof(TINT32);
      else
        m_frameCountPos = 8 + 3 * sizeof(TINT32);
    }
  }
  if (!m_exists) {
    TFilePath parentDir = path.getParentDir();
    if (!TFileStatus(parentDir).doesExist()) {
      try {
        TSystem::mkDir(parentDir);

      } catch (...) {
        return;
      }
    }
    m_chan = fopen(path, "wb");
    if (!m_chan) return;
    if (!writeVersionAndCreator(m_chan, m_magic, m_creator)) return;
  }
}

//-------------------------------------------------------------------

TLevelWriterTzl::~TLevelWriterTzl() {
  if (m_version < CURRENT_VERSION) {
    if (!convertToLatestVersion()) return;
    assert(m_version == CURRENT_VERSION);
  }
  delete m_codec;

  TINT32 offsetMapPos     = 0;
  TINT32 iconOffsetMapPos = 0;
  if (!m_chan) return;

  assert(m_frameCount == (int)m_frameOffsTable.size());
  assert(m_frameCount == (int)m_iconOffsTable.size());

  offsetMapPos = (m_exists ? m_offsetTablePos : ftell(m_chan));
  fseek(m_chan, offsetMapPos, SEEK_SET);

  TzlOffsetMap::iterator it = m_frameOffsTable.begin();
  for (; it != m_frameOffsTable.end(); ++it) {
    TFrameId fid  = it->first;
    TINT32 num    = fid.getNumber();
    char letter   = fid.getLetter();
    TINT32 offs   = it->second.m_offs;
    TINT32 length = it->second.m_length;
    tfwrite(&num, 1, m_chan);
    tfwrite(&letter, 1, m_chan);
    tfwrite(&offs, 1, m_chan);
    tfwrite(&length, 1, m_chan);
  }

  // Write Icon Offset Table after frameOffsTable
  iconOffsetMapPos =
      ftell(m_chan);  //(m_exists?m_iconOffsetTablePos: ftell(m_chan));
  fseek(m_chan, iconOffsetMapPos, SEEK_SET);

  TzlOffsetMap::iterator iconIt = m_iconOffsTable.begin();
  for (; iconIt != m_iconOffsTable.end(); ++iconIt) {
    TFrameId fid           = iconIt->first;
    TINT32 num             = fid.getNumber();
    char letter            = fid.getLetter();
    TINT32 thumbnailOffs   = iconIt->second.m_offs;
    TINT32 thumbnailLength = iconIt->second.m_length;
    tfwrite(&num, 1, m_chan);
    tfwrite(&letter, 1, m_chan);
    tfwrite(&thumbnailOffs, 1, m_chan);
    tfwrite(&thumbnailLength, 1, m_chan);
  }

  fseek(m_chan, m_frameCountPos, SEEK_SET);
  TINT32 frameCount = m_frameCount;

  tfwrite(&frameCount, 1, m_chan);
  tfwrite(&offsetMapPos, 1, m_chan);
  tfwrite(&iconOffsetMapPos, 1, m_chan);
  fclose(m_chan);
  m_chan = 0;

  if (m_palette && m_overwritePaletteFlag &&
      (m_palette->getDirtyFlag() ||
       !TSystem::doesExistFileOrLevel(m_palettePath))) {
    TOStream os(m_palettePath);
    os << m_palette;
    m_palette->release();
  }

  if (m_contentHistory) {
    TFilePath historyFp = m_path.withNoFrame().withType("hst");
    FILE *historyChan   = fopen(historyFp, "w");
    if (historyChan) {
      std::string historyData = m_contentHistory->serialize().toStdString();
      fwrite(&historyData[0], 1, historyData.length(), historyChan);
      fclose(historyChan);
    }
  }
  // Se lo spazio libero (cioè la somma di tutti i buchi che si sono creati tra
  // i frame)
  // è maggiore di una certa soglia oppure è stato rimosso almeno un frame
  // allora ottimizzo il file
  // (in pratica risalvo il file da capo senza buchi).
  if (getFreeSpace() > 0.3 || erasedFrame) optimize();
}

//-------------------------------------------------------------------

TImageWriterP TLevelWriterTzl::getFrameWriter(TFrameId fid) {
  return new TImageWriterTzl(this, fid);
}

//-------------------------------------------------------------------

void TLevelWriterTzl::writeHeader(const TDimension &size) {
  m_headerWritten   = true;
  const char *codec = "LZO ";
  int codecLen      = strlen(codec);
  TINT32 hdrSize    = 3 * sizeof(TINT32) + codecLen;
  TINT32 lx = size.lx, ly = size.ly, intval = 1;

  tfwrite(&hdrSize, 1, m_chan);
  tfwrite(&lx, 1, m_chan);
  tfwrite(&ly, 1, m_chan);
  m_frameCountPos = ftell(m_chan);

  assert(m_frameCountPos == 8 + CREATOR_LENGTH + 3 * sizeof(TINT32));

  // I put the place for the frameCount, which I will write in this position at
  // the end  (see in the distructor)
  tfwrite(&intval, 1, m_chan);
  // I put the place for the offsetTableOffset, which I will write in this
  // position at the end  (see in the distructor)
  intval = 0;
  tfwrite(&intval, 1, m_chan);
  // I put the place for the iconOffsetTableOffset, which I will write in this
  // position at the end  (see in the distructor)
  tfwrite(&intval, 1, m_chan);
  tfwrite(codec, codecLen, m_chan);
}

//-------------------------------------------------------------------

void TLevelWriterTzl::addFreeChunk(TINT32 offs, TINT32 length) {
  std::set<TzlChunk>::iterator it = m_freeChunks.begin();
  while (it != m_freeChunks.end()) {
    // if (it->m_offs>offs+length+1)
    //  break;

    if (it->m_offs + it->m_length == offs)  // accorpo due chunks in uno
    {
      TzlChunk chunk(it->m_offs, it->m_length + length);
      m_freeChunks.erase(it);
      m_freeChunks.insert(chunk);
      // it->m_length += length;
      return;
    } else if (offs + length == it->m_offs) {
      TzlChunk chunk(offs, it->m_length + length);
      m_freeChunks.erase(it);
      m_freeChunks.insert(chunk);
      // it->m_offs = offs;
      // it->m_length += length;
      return;
    }
    it++;
  }
  if (it == m_freeChunks.end()) m_freeChunks.insert(TzlChunk(offs, length));
}
//-------------------------------------------------------------------

TINT32 TLevelWriterTzl::findSavingChunk(const TFrameId &fid, TINT32 length,
                                        bool isIcon) {
  TzlOffsetMap::iterator it;
  // prima libero il chunk del fid, se c'e'. accorpo con altro chunk se trovo
  // uno contiguo.
  if (!isIcon) {
    if ((it = m_frameOffsTable.find(fid)) != m_frameOffsTable.end()) {
      addFreeChunk(it->second.m_offs, it->second.m_length);
      m_frameOffsTable.erase(it);
    } else
      m_frameCount++;
  } else {
    if ((it = m_iconOffsTable.find(fid)) != m_iconOffsTable.end()) {
      addFreeChunk(it->second.m_offs, it->second.m_length);
      m_iconOffsTable.erase(it);
    }
  }

  // ora cerco un cioncone libero con la piu' piccola memoria sufficiente
  std::set<TzlChunk>::iterator it1   = m_freeChunks.begin(),
                               found = m_freeChunks.end();
  for (; it1 != m_freeChunks.end(); it1++) {
    //   TINT32 _length = it1->m_length;

    if (it1->m_length >= length &&
        (found == m_freeChunks.end() || found->m_length > it1->m_length))
      found = it1;
  }

  if (found != m_freeChunks.end()) {
    //  TINT32 _length = found->m_length;
    TINT32 _offset = found->m_offs;
    if (found->m_length > length) {
      TzlChunk chunk(found->m_offs + length, found->m_length - length);
      m_freeChunks.insert(chunk);
      // found->m_offs+=length;
      // found->m_length-=length;
    } else
      assert(found->m_length == length);
    m_freeChunks.erase(found);
    return _offset;
  } else {
    m_offsetTablePos += length;
    return m_offsetTablePos - length;
  }
}
//-------------------------------------------------------------------
bool TLevelWriterTzl::convertToLatestVersion() {
  TFileStatus fs(m_path);
  // se il file è di una versione precedente deve necessariamente già esistere
  // su disco
  assert(fs.doesExist());
  if (!fs.doesExist()) return false;
  m_exists         = false;
  m_frameCount     = 0;
  m_frameOffsTable = TzlOffsetMap();
  m_iconOffsTable  = TzlOffsetMap();
  m_headerWritten  = false;

  fclose(m_chan);
  m_chan = 0;

  std::string tempName = "~" + m_path.getName() + "tmp&.tlv";
  TFilePath tempPath   = TSystem::getTempDir() + tempName;
  if (TSystem::doesExistFileOrLevel(m_path)) {
    assert(m_path != tempPath);
    if (TSystem::doesExistFileOrLevel(tempPath)) TSystem::deleteFile(tempPath);
    TSystem::copyFile(tempPath, m_path);
  } else
    return false;
  m_chan = fopen(m_path, "rb+");
  if (!m_chan) return false;
  if (!writeVersionAndCreator(m_chan, m_magic, m_creator)) return false;
  m_creatorWritten = true;
  m_version        = CURRENT_VERSION;
  TLevelReaderP lr(tempPath);
  if (!lr) return false;
  TLevelP level = lr->loadInfo();
  for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
    save(lr->getFrameReader(it->first)->load(), it->first);
  }
  lr = TLevelReaderP();
  if (TSystem::doesExistFileOrLevel(tempPath)) {
    TSystem::deleteFile(tempPath);
  }

  TINT32 offsetMapPos     = 0;
  TINT32 iconOffsetMapPos = 0;
  if (!m_chan) return false;

  assert(m_frameCount == (int)m_frameOffsTable.size());
  assert(m_frameCount == (int)m_iconOffsTable.size());

  offsetMapPos = (m_exists ? m_offsetTablePos : ftell(m_chan));
  fseek(m_chan, offsetMapPos, SEEK_SET);

  TzlOffsetMap::iterator it2 = m_frameOffsTable.begin();
  for (; it2 != m_frameOffsTable.end(); ++it2) {
    TFrameId fid  = it2->first;
    TINT32 num    = fid.getNumber();
    char letter   = fid.getLetter();
    TINT32 offs   = it2->second.m_offs;
    TINT32 length = it2->second.m_length;
    tfwrite(&num, 1, m_chan);
    tfwrite(&letter, 1, m_chan);
    tfwrite(&offs, 1, m_chan);
    tfwrite(&length, 1, m_chan);
  }

  iconOffsetMapPos = ftell(m_chan);
  fseek(m_chan, iconOffsetMapPos, SEEK_SET);

  TzlOffsetMap::iterator iconIt = m_iconOffsTable.begin();
  for (; iconIt != m_iconOffsTable.end(); ++iconIt) {
    TFrameId fid           = iconIt->first;
    TINT32 num             = fid.getNumber();
    char letter            = fid.getLetter();
    TINT32 thumbnailOffs   = iconIt->second.m_offs;
    TINT32 thumbnailLength = iconIt->second.m_length;
    tfwrite(&num, 1, m_chan);
    tfwrite(&letter, 1, m_chan);
    tfwrite(&thumbnailOffs, 1, m_chan);
    tfwrite(&thumbnailLength, 1, m_chan);
  }

  fseek(m_chan, m_frameCountPos, SEEK_SET);
  TINT32 frameCount = m_frameCount;
  tfwrite(&frameCount, 1, m_chan);
  tfwrite(&offsetMapPos, 1, m_chan);
  tfwrite(&iconOffsetMapPos, 1, m_chan);
  m_frameOffsTable = TzlOffsetMap();
  m_iconOffsTable  = TzlOffsetMap();
  m_frameCount     = 0;
  m_res            = TDimension();
  fclose(m_chan);
  m_chan = 0;

  if (m_palette && (m_palette->getDirtyFlag() ||
                    !TSystem::doesExistFileOrLevel(m_palettePath))) {
    TOStream os(m_palettePath);
    os << m_palette;
  }
  // else
  //	m_palette = 0;

  m_chan = fopen(m_path, "rb+");
  if (!m_chan) return false;
  if (!readHeaderAndOffsets(m_chan, m_frameOffsTable, m_iconOffsTable, m_res,
                            m_version, m_creator, &m_frameCount,
                            &m_offsetTablePos, &m_iconOffsetTablePos, 0))
    return false;
  m_freeChunks.clear();
  buildFreeChunksTable();
  m_headerWritten = true;
  m_exists        = true;
  m_frameCountPos = 8 + CREATOR_LENGTH + 3 * sizeof(TINT32);
  assert(m_version == CURRENT_VERSION);
  if (!m_renumberTable.empty()) renumberFids(m_renumberTable);
  return true;
}
//-------------------------------------------------------------------
void TLevelWriterTzl::saveImage(const TImageP &img, const TFrameId &_fid,
                                bool isIcon) {
  TFrameId fid = _fid;
  if (!m_chan) return;

  // se il file è di una versione precedente allora lo converto prima
  if (m_version < CURRENT_VERSION) {
    if (!convertToLatestVersion()) return;
    assert(m_version == CURRENT_VERSION);
  }

  if (!m_updatedIconsSize && m_exists)
    m_updatedIconsSize = checkIconSize(getIconSize());

  TToonzImageP ti = img;
  if (!ti) return;

  if (!m_palette) {
    m_palette = ti->getPalette();
    if (m_palette) m_palette->addRef();
  }

  if (!m_creatorWritten) {
    fseek(m_chan, 0, SEEK_SET);
    writeVersionAndCreator(m_chan, m_magic, m_creator);
    m_creatorWritten = true;
  }
  if (!m_headerWritten) writeHeader(ti->getSize());

  if (m_res != TDimension())
    if (!isIcon)
      assert(m_res == ti->getSize());
    else
      assert(getIconSize() == ti->getSize());
  else
    m_res = ti->getSize();

  TRect sb = ti->getSavebox();

  TINT32 sbx0 = sb.x0, sby0 = sb.y0, sblx = sb.getLx(), sbly = sb.getLy();
  TINT32 buffSize = 0;

  UCHAR *buff = 0;  //  = ti->getCompressedBuffer(buffSize);

  TRasterP cmras = ti->getRaster();

  assert(cmras);

  if (cmras->getBounds() != sb) {
    TRect rectToExtract = sb;
    if (!rectToExtract.isEmpty()) {
      TRasterP smallRaster = cmras->extract(rectToExtract)->clone();
      assert(rectToExtract == sb);
      cmras = smallRaster;
    } else {
      cmras = cmras->create(1, 1);
      cmras->clear();
    }
  }

  TRasterGR8P rCompressed = m_codec->compress(cmras, 1, buffSize);
  if (rCompressed == TRasterGR8P()) {
    static bool firstTime = true;
    if (firstTime) {
      firstTime = false;
      // MessageBox( NULL, "Cannot save level, out of memory!", "Warning!",
      // MB_OK);
    }
    return;
  }

  rCompressed->lock();

  buffSize = rCompressed->getLx();
  buff     = rCompressed->getRawData();

  assert(buffSize);
  assert(buff);

#if !TNZ_LITTLE_ENDIAN

  Header *header = (Header *)buff;

  TRasterP ras;
  m_codec->decompress(buff, buffSize, ras);
  delete[] buff;
  assert((TRasterCM32P)ras);
  assert(ras->getLx() == header->m_lx);
  assert(ras->getLy() == header->m_ly);

  for (int y = 0; y < ras->getLy(); ++y) {
    ras->lock();
    TINT32 *pix    = ((TINT32 *)ras->getRawData()) + y * ras->getLx();
    TINT32 *endPix = pix + ras->getLx();
    while (pix < endPix) {
      *pix = swapTINT32(*pix);
      pix++;
    }
    ras->unlock();
  }

  rCompressed = m_codec->compress(ras, 1, buffSize);

  header = (Header *)buff;

  header->m_lx      = swapTINT32(header->m_lx);
  header->m_ly      = swapTINT32(header->m_ly);
  header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

  double xdpi = 1, ydpi = 1;
  ti->getDpi(xdpi, ydpi);

  TINT32 theBuffSize = buffSize;

  TINT32 length = 0;
  if (!isIcon)
    length = 5 * sizeof(TINT32) + 2 * sizeof(double) + theBuffSize;
  else
    length = 3 * sizeof(TINT32) + theBuffSize;

  if (fid == TFrameId()) {
    assert(!m_exists);
    if (m_frameOffsTable.empty())
      fid = (m_frameOffsTable.empty())
                ? TFrameId(1, 0)
                : TFrameId(m_frameOffsTable.rbegin()->first.getNumber() + 1, 0);
    if (m_iconOffsTable.empty())
      fid = (m_iconOffsTable.empty())
                ? TFrameId(1, 0)
                : TFrameId(m_iconOffsTable.rbegin()->first.getNumber() + 1, 0);
  }

  if (!m_exists) {
    long offs = ftell(m_chan);
    if (!isIcon) {
      m_frameOffsTable[fid] = TzlChunk(offs, length);
      m_frameCount++;
    } else
      m_iconOffsTable[fid] = TzlChunk(offs, length);

  } else {
    long frameOffset = findSavingChunk(fid, length, isIcon);
    if (!isIcon)
      m_frameOffsTable[fid] = TzlChunk(frameOffset, length);
    else
      m_iconOffsTable[fid] = TzlChunk(frameOffset, length);
    fseek(m_chan, frameOffset, SEEK_SET);
  }
  if (!isIcon) {
    tfwrite(&sbx0, 1, m_chan);
    tfwrite(&sby0, 1, m_chan);
    tfwrite(&sblx, 1, m_chan);
    tfwrite(&sbly, 1, m_chan);
    tfwrite(&buffSize, 1, m_chan);
    tfwrite(&xdpi, 1, m_chan);
    tfwrite(&ydpi, 1, m_chan);
    tfwrite(buff, theBuffSize, m_chan);
  } else {
    TINT32 iconLx = getIconSize().lx;
    TINT32 iconLy = getIconSize().ly;
    tfwrite(&iconLx, 1, m_chan);
    tfwrite(&iconLy, 1, m_chan);
    tfwrite(&buffSize, 1, m_chan);
    tfwrite(buff, theBuffSize, m_chan);
  }
  rCompressed->unlock();

  //#if !TNZ_LITTLE_ENDIAN
  // delete [] buff;
  //#endif
}

//-------------------------------------------------------------------

void TLevelWriterTzl::doSave(const TImageP &img, const TFrameId &_fid) {
  // salvo l'immagine grande
  saveImage(img, _fid);
  if (!img)
    throw(TException(
        "Saving tlv: it is not possible to create the image frame."));
  // creo l'iconcina
  TImageP icon = TImageP();
  createIcon(img, icon);
  assert(icon);
  if (!icon)
    throw(
        TException("Saving tlv: it is not possible to create the image icon."));
  // salvo l'iconcina
  saveImage(icon, _fid, true);
  return;
}

//-------------------------------------------------------------------
void TLevelWriterTzl::save(const TImageP &img, const TFrameId &fid) {
  doSave(img, fid);
}
void TLevelWriterTzl::save(const TImageP &img) { doSave(img, TFrameId()); }
//-------------------------------------------------------------------
void TLevelWriterTzl::saveIcon(const TImageP &img, const TFrameId &fid) {
  // salvo su file come icona
  saveImage(img, fid, true);
}
//-------------------------------------------------------------------

void TLevelWriterTzl::setPalette(TPalette *palette) {
  if (!m_palette) {
    m_palette = palette;
    m_palette->addRef();
  }
}
//-------------------------------------------------------------------
void TLevelWriterTzl::renumberFids(
    const std::map<TFrameId, TFrameId> &renumberTable) {
  m_renumberTable = renumberTable;
  if (m_version < 13) return;
  if (m_frameOffsTable.empty() || !m_exists) return;

  // Rimozione dei frame non presenti nella prima colonna di renumberTable
  std::map<TFrameId, TzlChunk> frameOffsTableTemp;
  frameOffsTableTemp        = m_frameOffsTable;
  TzlOffsetMap::iterator it = frameOffsTableTemp.begin();
  while (it != frameOffsTableTemp.end()) {
    TFrameId fid = it->first;
    std::map<TFrameId, TFrameId>::const_iterator it2 = renumberTable.find(fid);
    if (it2 == renumberTable.end()) remove(fid);
    it++;
  }

  std::map<TFrameId, TzlChunk> frameOffsTable, iconOffsTable;
  // Renumber Frame
  std::map<TFrameId, TFrameId>::const_iterator it3 = renumberTable.begin();
  while (it3 != renumberTable.end()) {
    TFrameId fidToRenumber        = it3->first;
    TzlOffsetMap::iterator it     = m_frameOffsTable.find(fidToRenumber);
    TzlOffsetMap::iterator iconIt = m_iconOffsTable.find(fidToRenumber);
    if (it != m_frameOffsTable.end()) frameOffsTable[it3->second] = it->second;
    if (m_iconOffsTable.size() > 0 && iconIt != m_iconOffsTable.end())
      iconOffsTable[it3->second] = iconIt->second;
    it3++;
  }

  m_frameOffsTable.clear();
  m_frameOffsTable = frameOffsTable;
  m_iconOffsTable.clear();
  m_iconOffsTable = iconOffsTable;
  m_frameCount    = m_frameOffsTable.size();
  // buildFreeChunksTable();
}

//-------------------------------------------------------------------

void TLevelWriterTzl::createIcon(const TImageP &imgIn, TImageP &imgOut) {
  // Creo iconcina e poi la salvo
  TToonzImageP ti = imgIn;
  if (!ti) return;
  TRect sb    = ti->getSavebox();
  TINT32 sbx0 = sb.x0, sby0 = sb.y0, sblx = sb.getLx(), sbly = sb.getLy();
  TRasterP cmras = ti->getRaster();

  assert(m_iconSize.lx > 0 && m_iconSize.ly > 0);
  if (m_iconSize.lx <= 0 || m_iconSize.ly <= 0) return;
  TINT32 iconLx = m_iconSize.lx, iconLy = m_iconSize.ly;
  if (cmras->getLx() <= iconLx && cmras->getLy() <= iconLy) {
    iconLx     = cmras->getLx();
    iconLy     = cmras->getLy();
    m_iconSize = TDimension(iconLx, iconLy);
    imgOut     = imgIn;
    return;
  }

  TRasterCM32P thumbnailRas(iconLx, iconLy);
  if (iconLx > 0 && iconLy > 0) {
    // Do thumbnail
    TRop::makeIcon(thumbnailRas, TRasterCM32P(cmras));
    assert(thumbnailRas);
    if (!thumbnailRas) return;
  }
  // Compute savebox
  if (ti->getSize().lx <= 0 || ti->getSize().ly <= 0) return;
  TRect tmp_savebox;
  TRect savebox;
  TRop::computeBBox(thumbnailRas, tmp_savebox);
  if (tmp_savebox.isEmpty()) {
    TINT32 iconsbx0 = tround((double)iconLx * sbx0 / ti->getSize().lx);
    TINT32 iconsby0 = tround((double)iconLy * sby0 / ti->getSize().ly);
    TINT32 iconsblx =
        std::max(tround((double)iconLx * sblx / ti->getSize().lx), 1);
    TINT32 iconsbly =
        std::max(tround((double)iconLy * sbly / ti->getSize().ly), 1);
    savebox = TRect(TPoint(iconsbx0, iconsby0), TDimension(iconsblx, iconsbly));
  } else {
    savebox = tmp_savebox;
  }

  if (!TRect(m_iconSize).contains(savebox))  // it should be better to use
                                             // tfloor instead of tround in the
                                             // previous lines:
    // sometimes, for rounding problems, the savebox is outside 1 pixel the
    // raster size.
    // the best solution should be to replace tround with tfloor here
    // and in the icon reading of tzl 1.4 (method load14),
    // but this way the old 1.4 tzl are not readed correctly (crash!)
    // so, this 'if' is a patch. vinz
    savebox = savebox * TRect(m_iconSize);
  thumbnailRas->clearOutside(savebox);
  TToonzImageP ticon(thumbnailRas, savebox);
  double xdpi = 1, ydpi = 1;
  ti->getDpi(xdpi, ydpi);
  ticon->setDpi(xdpi, ydpi);
  ticon->setPalette(ti->getPalette());
  imgOut = TImageP(ticon);
}

//-------------------------------------------------------------------

void TLevelWriterTzl::remove(const TFrameId &fid) {
  TzlOffsetMap::iterator it = m_frameOffsTable.find(fid);
  // se il fid non esiste non faccio nulla
  if (it == m_frameOffsTable.end()) return;
  // aggiungo spazio vuoto
  // m_freeChunks.insert(TzlChunk(it->second.m_offs, it->second.m_length));
  addFreeChunk(it->second.m_offs, it->second.m_length);
  // cancello l'immagine dalla tabella
  m_frameOffsTable.erase(it);

  if (m_iconOffsTable.size() > 0) {
    TzlOffsetMap::iterator iconIt = m_iconOffsTable.find(fid);
    // deve necessariamente esserci anche l'iconcina
    assert(iconIt != m_iconOffsTable.end());
    if (iconIt == m_iconOffsTable.end()) return;
    // aggiungo spazio vuoto
    // m_freeChunks.insert(TzlChunk(iconIt->second.m_offs,
    // iconIt->second.m_length));
    addFreeChunk(iconIt->second.m_offs, iconIt->second.m_length);
    // Cancello la relativa icona
    m_iconOffsTable.erase(iconIt);
    erasedFrame = true;
  }
}

//-------------------------------------------------------------------

void TLevelWriterTzl::setIconSize(TDimension iconSize) {
  assert(iconSize.lx > 0 && iconSize.ly > 0);

  m_iconSize     = TDimension(iconSize.lx, iconSize.ly);
  m_userIconSize = TDimension(
      iconSize.lx, iconSize.ly);  // Used in TLevelWriterTzl::optimize().
                                  // Observe that m_iconSize may change below.
  if (m_version >= 13 && m_exists) {
    // A supported level file already exists at the specified path

    // Check whether icons stored in the file already
    // have the required size
    if (!m_updatedIconsSize)  // m_updatedIconsSize tells whether
      m_updatedIconsSize =
          checkIconSize(m_iconSize);  // icon sizes in the file match m_iconSize

    if (!m_updatedIconsSize) {
      // Icons in the stored file mismatch with the
      // required size. They need to be resized.
      m_updatedIconsSize = resizeIcons(m_iconSize);
      assert(m_updatedIconsSize);
    }
  }
}

//-------------------------------------------------------------------

/*!
  \brief    Checks the icons size stored inside an already existing level
            file, and compares it with the specified newSize.
  \return   Whether the specified newSize \a matches the icons size in
            the level file.
*/

bool TLevelWriterTzl::checkIconSize(const TDimension &newSize) {
  assert(m_version >= 13);

  if (!m_exists || m_iconOffsTable.empty() || !m_chan || m_version < 13)
    return false;

  // Read the size of icons in the file
  TINT32 iconLx = 0, iconLy = 0;

  long currentPos =
      ftell(m_chan);  // Backup current reading position in the file

  TzlOffsetMap::iterator it = m_iconOffsTable.begin();
  long offs                 = it->second.m_offs;

  fseek(m_chan, offs, SEEK_SET);

  fread(&iconLx, sizeof(TINT32), 1, m_chan);
  fread(&iconLy, sizeof(TINT32), 1, m_chan);

  fseek(m_chan, currentPos,
        SEEK_SET);  // Reset to the original position in the file

  assert(iconLx > 0 && iconLy > 0);
  if (iconLx <= 0 || iconLy <= 0 || iconLx > m_res.lx || iconLy > m_res.ly)
    return false;

  m_currentIconSize = TDimension(
      iconLx, iconLy);  // This is not performed if the above pre-emptive
                        // bailout kicks in. BTW, m_currentIconSize
                        // is currently UNUSED! Should be cleaned up...
  // Compare newSize against read icon size in the file
  return (m_currentIconSize == newSize);
}

//-------------------------------------------------------------------

bool TLevelWriterTzl::resizeIcons(const TDimension &newSize) {
  if (!m_exists) return false;
  if (m_iconOffsTable.empty()) return false;
  if (!m_chan) return false;
  assert(m_version >= 13);

  // faccio una copia di m_path per poi usarla per la resizeIcons()
  fclose(m_chan);
  m_chan = 0;
  TFileStatus fs(m_path);
  assert(fs.doesExist());
  if (!fs.doesExist()) return false;
  std::string tempName = "~" + m_path.getName() + "tmpIcon&.tlv";
  TFilePath tempPath   = TSystem::getTempDir() + tempName;
  if (TSystem::doesExistFileOrLevel(m_path)) {
    assert(m_path != tempPath);
    if (TSystem::doesExistFileOrLevel(tempPath)) TSystem::deleteFile(tempPath);
    TSystem::copyFile(tempPath, m_path);
  } else
    return false;
  m_chan = fopen(m_path, "rb+");
  assert(m_chan);

  assert(TSystem::doesExistFileOrLevel(tempPath));
  if (!TSystem::doesExistFileOrLevel(tempPath)) return false;
  TLevelReaderP lr(tempPath);
  TLevelP level = lr->loadInfo();
  /*
  if(m_currentIconSize.lx>newSize.lx && m_currentIconSize.ly>newSize.ly)
  {
          for(TLevel::Iterator it=level->begin(); it != level->end(); ++it)
          {
                  TImageReaderP ir = lr->getFrameReader(it->first);

                  // carico l'iconcina
            TImageP img = ir->loadIcon();
                  TImageP icon;
                  createIcon(img,icon);
                  saveIcon(icon,it->first);
          }
  }
  else
  {				 */
  // leggo tutte le immagini da file e creo le iconcine da da zero
  for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
    // carico l'iconcina
    TImageP img = lr->getFrameReader(it->first)->load();
    // creo e salvo
    TImageP icon;
    createIcon(img, icon);
    saveIcon(icon, it->first);
  }

  //}
  lr = TLevelReaderP();
  // delete m_tempPath
  if (TSystem::doesExistFileOrLevel(tempPath)) {
    TSystem::deleteFile(tempPath);
  }

  return true;
}

float TLevelWriterTzl::getFreeSpace() {
  if (m_exists && m_version >= 13) {
    TINT32 freeSpace                = 0;
    std::set<TzlChunk>::iterator it = m_freeChunks.begin();
    for (; it != m_freeChunks.end(); ++it) freeSpace += it->m_length;

    TINT32 totalSpace = 0;
    if (m_version == 13)
      totalSpace = m_offsetTablePos - 6 * sizeof(TINT32) - 4 * sizeof(char) -
                   8 * sizeof(char);
    else if (m_version == 14)
      totalSpace = m_offsetTablePos - 6 * sizeof(TINT32) - 4 * sizeof(char) -
                   8 * sizeof(char) - CREATOR_LENGTH * sizeof(char);
    assert(totalSpace > 0);
    return (float)freeSpace / totalSpace;
  }
  return 0;
}
// creo il file ottimizzato, cioè senza spazi liberi
bool TLevelWriterTzl::optimize() {
  TFileStatus fs(m_path);
  assert(fs.doesExist());

  std::string tempName = "~" + m_path.getName() + "tmp&.tlv";
  TFilePath tempPath =
      TSystem::getTempDir() + tempName;  // Path temporaneo del file ottimizzato

  if (TSystem::doesExistFileOrLevel(tempPath)) TSystem::deleteFile(tempPath);

  TLevelWriterP lw(tempPath);

  TLevelReaderP lr(m_path);
  lw->setIconSize(m_userIconSize);
  TLevelP level = lr->loadInfo();
  if (!level || level->getFrameCount() == 0) return false;
  TLevel::Iterator levelIt = level->begin();
  for (levelIt; levelIt != level->end(); ++levelIt) {
    TToonzImageP img = lr->getFrameReader(levelIt->first)->load();
    assert(img);
    lw->getFrameWriter(levelIt->first)->save(img);
  }
  lr = TLevelReaderP();
  lw = TLevelWriterP();

  // Se il file tempPath (ottimizzato) è stato creato
  // lo copio sostituendolo ad m_path
  if (TSystem::doesExistFileOrLevel(tempPath)) {
    assert(m_path != tempPath);
    if (TSystem::doesExistFileOrLevel(m_path))
      TSystem::deleteFile(m_path);  // Cancello il file non ottimizzato
    TSystem::copyFile(m_path, tempPath);
    TSystem::deleteFile(tempPath);
  } else
    return false;
  return true;
}

//===================================================================
//
// TLevelReaderTzl
//
//-------------------------------------------------------------------

void TLevelReaderTzl::readPalette() {
  TFilePath pltfp = m_path.withNoFrame().withType("tpl");
  TFileStatus fs(pltfp);
  TPersist *p = 0;
  TIStream is(pltfp);
  TPalette *palette = 0;

  if (is && fs.doesExist()) {
    std::string tagName;
    if (is.matchTag(tagName) && tagName == "palette") {
      std::string gname;
      is.getTagParam("name", gname);
      palette = new TPalette();
      palette->loadData(is);
      palette->setGlobalName(::to_wstring(gname));
      is.matchEndTag();
    }
  }

  if (!palette) {
    int i;
    palette = new TPalette();
    for (i = 1; i < 128 + 32; i++) palette->addStyle(TPixel32(127, 150, 255));

    for (i = 0; i < 10; i++) palette->getPage(0)->addStyle(i + 1);
    for (i = 0; i < 10; i++) palette->getPage(0)->addStyle(128 + i);
  }
  if (m_level) m_level->setPalette(palette);
}

//---------------------------------------------------

TLevelReaderTzl::TLevelReaderTzl(const TFilePath &path)
    : TLevelReader(path)
    , m_chan(0)
    , m_res(0, 0)
    , m_xDpi(0)
    , m_yDpi(0)
    , m_version(0)
    , m_frameOffsTable()
    , m_iconOffsTable()
    , m_level()
    , m_readPalette(true) {
  m_chan = fopen(path, "rb");

  if (!m_chan) return;

  if (!readHeaderAndOffsets(m_chan, m_frameOffsTable, m_iconOffsTable, m_res,
                            m_version, m_creator, 0, 0, 0, m_level))
    return;

  TFilePath historyFp = path.withNoFrame().withType("hst");
  FILE *historyChan   = fopen(historyFp, "r");
  if (historyChan) {
    // read the whole file into historyData string
    fseek(historyChan, 0, SEEK_END);
    long lSize = ftell(historyChan);
    rewind(historyChan);
    std::string historyData(lSize, '\0');
    fread(&historyData[0], 1, lSize, historyChan);
    fclose(historyChan);

    if (!m_contentHistory) m_contentHistory = new TContentHistory(true);
    m_contentHistory->deserialize(QString::fromStdString(historyData));
  }

  // UCHAR *imgBuff = new UCHAR[imgBuffSize];

  // fclose(m_chan);
  // m_chan = 0;
}

//-------------------------------------------------------------------

void TLevelReaderTzl::doReadPalette(bool doReadIt) { m_readPalette = doReadIt; }

//-------------------------------------------------------------------

TLevelReaderTzl::~TLevelReaderTzl() {
  if (m_chan) fclose(m_chan);
  m_chan = 0;
}

//-------------------------------------------------------------------

TLevelP TLevelReaderTzl::loadInfo() {
  if (m_level && m_level->getPalette() == 0 && m_readPalette) readPalette();
  return m_level;
}

//-------------------------------------------------------------------

TImageReaderP TLevelReaderTzl::getFrameReader(TFrameId fid) {
  if (m_level && m_level->getPalette() == 0 && m_readPalette) readPalette();

  return new TImageReaderTzl(getFilePath(), fid, this);
}

//-------------------------------------------------------------------
QString TLevelReaderTzl::getCreator() {
  if (m_version < 14) return "";
  return m_creator;
}

//-------------------------------------------------------------------
bool TLevelReaderTzl::getIconSize(TDimension &iconSize) {
  if (m_iconOffsTable.empty()) return false;
  if (m_version < 13) return false;
  assert(m_chan);
  TINT32 currentPos         = ftell(m_chan);
  TzlOffsetMap::iterator it = m_iconOffsTable.begin();
  TINT32 offs               = it->second.m_offs;

  fseek(m_chan, offs, SEEK_SET);
  TINT32 iconLx = 0, iconLy = 0;
  // leggo la dimensione delle iconcine nel file
  fread(&iconLx, sizeof(TINT32), 1, m_chan);
  fread(&iconLy, sizeof(TINT32), 1, m_chan);
  assert(iconLx > 0 && iconLy > 0);
  // ritorno alla posizione corrente
  fseek(m_chan, currentPos, SEEK_SET);
  iconSize = TDimension(iconLx, iconLy);
  return true;
}

//===================================================================
//
// TImageReaderTzl
//
//-------------------------------------------------------------------

TImageReaderTzl::TImageReaderTzl(const TFilePath &f, const TFrameId &fid,
                                 TLevelReaderTzl *lr)
    : TImageReader(f)
    , m_fid(fid)
    , m_lrp(lr)
    , m_lx(lr->m_res.lx)
    , m_ly(lr->m_res.ly)
    , m_isIcon(false) {}

//-------------------------------------------------------------------

TImageP TImageReaderTzl::load10() {
  FILE *chan = m_lrp->m_chan;

  if (!chan) return TImageP();

  // SAVEBOX_X0 SAVEBOX_Y0 SAVEBOX_LX SAVEBOX_LY BUFFER_SIZE
  TINT32 sbx0, sby0, sblx, sbly;
  TINT32 actualBuffSize;
  double xdpi = 1, ydpi = 1;
  TINT32 imgBuffSize = 0;
  UCHAR *imgBuff     = 0;
  int frameIndex     = m_fid.getNumber();
  // int pos = ftell(chan);
  /*if(m_lrp->m_frameIndex>=m_frameIndex)
{
fseek(chan, m_lrp->m_framesOffset, SEEK_SET);
m_lrp->m_frameIndex = 0;
}
int k = m_lrp->m_frameIndex + 1;
m_lrp->m_frameIndex = m_frameIndex;*/

  int k;
  if (m_lrp->m_frameOffsTable[TFrameId(frameIndex)].m_offs == 0) {
    int i;
    for (i = 2; m_lrp->m_frameOffsTable[TFrameId(i)].m_offs != 0 &&
                i <= m_fid.getNumber();
         i++)
      ;
    if (i == frameIndex) i = 2;

    fseek(chan, m_lrp->m_frameOffsTable[TFrameId(i - 1)].m_offs, SEEK_SET);
    k = i - 1;
  } else {
    fseek(chan, m_lrp->m_frameOffsTable[TFrameId(frameIndex)].m_offs, SEEK_SET);
    k = frameIndex;
  }
  for (; k <= frameIndex; k++) {
    m_lrp->m_frameOffsTable[TFrameId(k)] = TzlChunk(ftell(chan), 0);

    fread(&sbx0, sizeof(TINT32), 1, chan);
    fread(&sby0, sizeof(TINT32), 1, chan);
    fread(&sblx, sizeof(TINT32), 1, chan);
    fread(&sbly, sizeof(TINT32), 1, chan);
    fread(&actualBuffSize, sizeof(TINT32), 1, chan);
    fread(&xdpi, sizeof(double), 1, chan);
    fread(&ydpi, sizeof(double), 1, chan);

#if !TNZ_LITTLE_ENDIAN
    sbx0           = swapTINT32(sbx0);
    sby0           = swapTINT32(sby0);
    sblx           = swapTINT32(sblx);
    sbly           = swapTINT32(sbly);
    actualBuffSize = swapTINT32(actualBuffSize);
    reverse((char *)&xdpi, sizeof(double));
    reverse((char *)&ydpi, sizeof(double));
#endif

    imgBuffSize = m_lx * m_ly * sizeof(TPixelCM32);
    assert(actualBuffSize <= imgBuffSize);

    delete[] imgBuff;
    imgBuff = new UCHAR[imgBuffSize];
    // int ret =
    fread(imgBuff, actualBuffSize, 1, chan);
    // assert(ret==1);
  }

  Header *header = (Header *)imgBuff;

#if !TNZ_LITTLE_ENDIAN
  header->m_lx      = swapTINT32(header->m_lx);
  header->m_ly      = swapTINT32(header->m_ly);
  header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

  TRasterCodecLZO codec("LZO", false);
  TRasterP ras;
  if (!codec.decompress(imgBuff, actualBuffSize, ras, m_safeMode))
    return TImageP();
  assert((TRasterCM32P)ras);
  assert(ras->getLx() == header->m_lx);
  assert(ras->getLy() == header->m_ly);

#if !TNZ_LITTLE_ENDIAN
  ras->lock();
  for (int y = 0; y < ras->getLy(); ++y) {
    TINT32 *pix    = ((TINT32 *)ras->getRawData(0, y));
    TINT32 *endPix = pix + ras->getLx();
    while (pix < endPix) {
      *pix = swapTINT32(*pix);
      pix++;
    }
  }
  ras->unlock();
// codec.compress(ras, 1, &imgBuff, actualBuffSize);
#endif

  TRect savebox(TPoint(sbx0, sby0), TDimension(sblx, sbly));
  TDimension imgSize(m_lrp->m_res.lx, m_lrp->m_res.ly);
  assert(TRect(imgSize).contains(savebox));
  if (imgSize != savebox.getSize() && !savebox.isEmpty()) {
    assert(TRect(imgSize).contains(savebox));
    TRasterCM32P fullRas(imgSize);
    TPixelCM32 bgColor;
    fullRas->fillOutside(savebox, bgColor);
    assert(savebox.getSize() == ras->getSize());
    fullRas->extractT(savebox)->copy(ras);
    ras = fullRas;
  }

  delete[] imgBuff;

  // codec.decompress(m_compressedBuffer, m_compressedBufferSize, ras);

  // assert(0);
  // TToonzImageP ti; //  = new
  // TToonzImage(m_lx,m_ly,savebox,imgBuff,actualBuffSize);
  TToonzImageP ti(ras, savebox);
  // if(dpiflag)
  ti->setDpi(xdpi, ydpi);
  // m_lrp->m_level->setFrame(TFrameId(m_frameIndex+1), ti);
  ti->setPalette(m_lrp->m_level->getPalette());
  // delete [] imgBuff;
  // imgBuff = 0;
  return ti;

  // ToonzImageUtils::updateRas32(ti);
}

//-------------------------------------------------------------------

TImageP TImageReaderTzl::load11() {
  FILE *chan = m_lrp->m_chan;

  if (!chan) return TImageP();

  // SAVEBOX_X0 SAVEBOX_Y0 SAVEBOX_LX SAVEBOX_LY BUFFER_SIZE
  TINT32 sbx0, sby0, sblx, sbly;
  TINT32 actualBuffSize;
  double xdpi = 1, ydpi = 1;
  TINT32 imgBuffSize = 0;
  UCHAR *imgBuff     = 0;

  assert(!m_lrp->m_frameOffsTable.empty());

  // int pos = ftell(chan);
  /*if(m_lrp->m_frameIndex>=m_frameIndex)
{
fseek(chan, m_lrp->m_framesOffset, SEEK_SET);
m_lrp->m_frameIndex = 0;
}
int k = m_lrp->m_frameIndex + 1;
m_lrp->m_frameIndex = m_frameIndex;*/

  TzlOffsetMap::iterator it = m_lrp->m_frameOffsTable.find(m_fid);

  if (it == m_lrp->m_frameOffsTable.end()) return 0;

  fseek(chan, it->second.m_offs, SEEK_SET);

  fread(&sbx0, sizeof(TINT32), 1, chan);
  fread(&sby0, sizeof(TINT32), 1, chan);
  fread(&sblx, sizeof(TINT32), 1, chan);
  fread(&sbly, sizeof(TINT32), 1, chan);
  fread(&actualBuffSize, sizeof(TINT32), 1, chan);

  fread(&xdpi, sizeof(double), 1, chan);
  fread(&ydpi, sizeof(double), 1, chan);

#if !TNZ_LITTLE_ENDIAN
  sbx0           = swapTINT32(sbx0);
  sby0           = swapTINT32(sby0);
  sblx           = swapTINT32(sblx);
  sbly           = swapTINT32(sbly);
  actualBuffSize = swapTINT32(actualBuffSize);
  reverse((char *)&xdpi, sizeof(double));
  reverse((char *)&ydpi, sizeof(double));
#endif

  imgBuffSize = m_lx * m_ly * sizeof(TPixelCM32);
  assert(actualBuffSize <= imgBuffSize);

  imgBuff = new UCHAR[imgBuffSize];
  // int ret =
  fread(imgBuff, actualBuffSize, 1, chan);
  // assert(ret==1);

  Header *header = (Header *)imgBuff;

#if !TNZ_LITTLE_ENDIAN
  header->m_lx      = swapTINT32(header->m_lx);
  header->m_ly      = swapTINT32(header->m_ly);
  header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

  TRasterCodecLZO codec("LZO", false);
  TRasterP ras;
  if (!codec.decompress(imgBuff, actualBuffSize, ras, m_safeMode))
    return TImageP();
  assert((TRasterCM32P)ras);
  assert(ras->getLx() == header->m_lx);
  assert(ras->getLy() == header->m_ly);

#if !TNZ_LITTLE_ENDIAN

  for (int y = 0; y < ras->getLy(); ++y) {
    ras->lock();
    TINT32 *pix    = ((TINT32 *)ras->getRawData(0, y));
    TINT32 *endPix = pix + ras->getLx();
    while (pix < endPix) {
      *pix = swapTINT32(*pix);
      pix++;
    }
    ras->unlock();
  }

// codec.compress(ras, 1, &imgBuff, actualBuffSize);
#endif

  TRect savebox(TPoint(sbx0, sby0), TDimension(sblx, sbly));
  TDimension imgSize(m_lrp->m_res.lx, m_lrp->m_res.ly);
  assert(TRect(imgSize).contains(savebox));
  if (imgSize != savebox.getSize() && !savebox.isEmpty()) {
    assert(TRect(imgSize).contains(savebox));
    TRasterCM32P fullRas(imgSize);
    TPixelCM32 bgColor;
    fullRas->fillOutside(savebox, bgColor);
    assert(savebox.getSize() == ras->getSize());
    fullRas->extractT(savebox)->copy(ras);
    ras = fullRas;
  }

  delete[] imgBuff;

  // codec.decompress(m_compressedBuffer, m_compressedBufferSize, ras);

  // assert(0);
  // TToonzImageP ti; //  = new
  // TToonzImage(m_lx,m_ly,savebox,imgBuff,actualBuffSize);
  TToonzImageP ti(ras, savebox);
  // if(dpiflag)
  ti->setDpi(xdpi, ydpi);
  // m_lrp->m_level->setFrame(TFrameId(m_frameIndex+1), ti);
  ti->setPalette(m_lrp->m_level->getPalette());
  // delete [] imgBuff;
  // imgBuff = 0;
  return ti;

  // ToonzImageUtils::updateRas32(ti);
}

//-------------------------------------------------------------------

TImageP TImageReaderTzl::load13() {
  FILE *chan = m_lrp->m_chan;

  if (!chan) return TImageP();
  // SAVEBOX_X0 SAVEBOX_Y0 SAVEBOX_LX SAVEBOX_LY BUFFER_SIZE
  TINT32 sbx0, sby0, sblx, sbly;
  TINT32 actualBuffSize;
  double xdpi = 1, ydpi = 1;
  TINT32 imgBuffSize = 0;
  UCHAR *imgBuff     = 0;
  TINT32 iconLx = 0, iconLy = 0;
  assert(!m_lrp->m_frameOffsTable.empty());
  assert(!m_lrp->m_iconOffsTable.empty());
  TzlOffsetMap::iterator it     = m_lrp->m_frameOffsTable.find(m_fid);
  TzlOffsetMap::iterator iconIt = m_lrp->m_iconOffsTable.find(m_fid);

  if (it == m_lrp->m_frameOffsTable.end() ||
      iconIt == m_lrp->m_iconOffsTable.end())
    return 0;

  fseek(chan, it->second.m_offs, SEEK_SET);
  fread(&sbx0, sizeof(TINT32), 1, chan);
  fread(&sby0, sizeof(TINT32), 1, chan);
  fread(&sblx, sizeof(TINT32), 1, chan);
  fread(&sbly, sizeof(TINT32), 1, chan);
  fread(&actualBuffSize, sizeof(TINT32), 1, chan);
  fread(&xdpi, sizeof(double), 1, chan);
  fread(&ydpi, sizeof(double), 1, chan);

#if !TNZ_LITTLE_ENDIAN
  sbx0           = swapTINT32(sbx0);
  sby0           = swapTINT32(sby0);
  sblx           = swapTINT32(sblx);
  sbly           = swapTINT32(sbly);
  actualBuffSize = swapTINT32(actualBuffSize);
  reverse((char *)&xdpi, sizeof(double));
  reverse((char *)&ydpi, sizeof(double));
#endif
  // Carico l'icona dal file
  if (m_isIcon) {
    fseek(chan, iconIt->second.m_offs, SEEK_SET);
    fread(&iconLx, sizeof(TINT32), 1, chan);
    fread(&iconLy, sizeof(TINT32), 1, chan);
    assert(iconLx > 0 && iconLy > 0);
    if (iconLx <= 0 || iconLy <= 0) throw TException();
    fread(&actualBuffSize, sizeof(TINT32), 1, chan);

    imgBuffSize = (iconLx * iconLy * sizeof(TPixelCM32));
    imgBuff     = new UCHAR[imgBuffSize];
    fread(imgBuff, actualBuffSize, 1, chan);

#if !TNZ_LITTLE_ENDIAN
    Header *header    = (Header *)imgBuff;
    header->m_lx      = swapTINT32(header->m_lx);
    header->m_ly      = swapTINT32(header->m_ly);
    header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif
    TRasterCodecLZO codec("LZO", false);
    TRasterP ras;
    if (!codec.decompress(imgBuff, actualBuffSize, ras, m_safeMode))
      return TImageP();
    assert((TRasterCM32P)ras);
    delete[] imgBuff;

#if !TNZ_LITTLE_ENDIAN

    for (int y = 0; y < ras->getLy(); ++y) {
      ras->lock();
      TINT32 *pix    = ((TINT32 *)ras->getRawData(0, y));
      TINT32 *endPix = pix + ras->getLx();
      while (pix < endPix) {
        *pix = swapTINT32(*pix);
        pix++;
      }
      ras->unlock();
    }
#endif

    /*
            TINT32 iconsbx0 = tround((double)iconLx*sbx0/m_lrp->m_res.lx);
            TINT32 iconsby0 = tround((double)iconLy*sby0/m_lrp->m_res.ly);
            TRect savebox(TPoint(iconsbx0,iconsby0),
       TDimension(ras->getLx(),ras->getLy()));
             */

    // Compute savebox

    TINT32 iconsbx0 = tround((double)iconLx * sbx0 / m_lrp->m_res.lx);
    TINT32 iconsby0 =
        tround((double)iconLy * sby0 /
               m_lrp->m_res.ly);  //+tround((double)(iconLy-ly)/2);
    TINT32 iconsblx = tround((double)iconLx * sblx / m_lrp->m_res.lx);
    TINT32 iconsbly = tround((double)iconLy * sbly / m_lrp->m_res.ly);
    TRect savebox(TPoint(iconsbx0, iconsby0), TDimension(iconsblx, iconsbly));

    // int ly = tround((double)m_lrp->m_res.ly*iconLx/m_lrp->m_res.lx);
    // TRect
    // rect(TPoint(0,tround((double)(iconLy-ly)/2)),TDimension(iconLx,ly));
    // TPixelCM32 bgColorRect(1,0,100);

    TDimension imgSize(iconLx, iconLy);
    assert(TRect(imgSize).contains(savebox));

    if (imgSize != savebox.getSize() && !savebox.isEmpty()) {
      assert(TRect(imgSize).contains(savebox));
      TRasterCM32P fullRas(imgSize);
      TPixelCM32 bgColor;

      fullRas->fillOutside(savebox, bgColor);

      // fullRas->extractT(rect)->fill(bgColorRect);
      assert(savebox.getSize() == ras->getSize());
      fullRas->extractT(savebox)->copy(ras);
      ras = fullRas;
    }

    TToonzImageP ti(ras, savebox);
    ti->setDpi(xdpi, ydpi);
    ti->setPalette(m_lrp->m_level->getPalette());
    return ti;
  }

  TRasterCM32P raux = TRasterCM32P(m_lx, m_ly);
  raux->lock();
  imgBuff = (UCHAR *)raux->getRawData();  // new UCHAR[imgBuffSize];
  // imgBuff = new UCHAR[imgBuffSize];
  // imgBuffSize = m_lx*m_ly*sizeof(TPixelCM32);
  // assert(actualBuffSize <= imgBuffSize);

  // imgBuff = new UCHAR[imgBuffSize];
  // int ret =
  fread(imgBuff, actualBuffSize, 1, chan);
  // assert(ret==1);

  Header *header = (Header *)imgBuff;

#if !TNZ_LITTLE_ENDIAN
  header->m_lx      = swapTINT32(header->m_lx);
  header->m_ly      = swapTINT32(header->m_ly);
  header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

  TRasterCodecLZO codec("LZO", false);
  TRasterP ras;
  if (!codec.decompress(imgBuff, actualBuffSize, ras, m_safeMode))
    return TImageP();
  assert((TRasterCM32P)ras);
  assert(ras->getLx() == header->m_lx);
  assert(ras->getLy() == header->m_ly);

#if !TNZ_LITTLE_ENDIAN

  for (int y = 0; y < ras->getLy(); ++y) {
    ras->lock();
    TINT32 *pix    = ((TINT32 *)ras->getRawData(0, y));
    TINT32 *endPix = pix + ras->getLx();
    while (pix < endPix) {
      *pix = swapTINT32(*pix);
      pix++;
    }
    ras->unlock();
  }

// codec.compress(ras, 1, &imgBuff, actualBuffSize);
#endif

  TRect savebox(TPoint(sbx0, sby0), TDimension(sblx, sbly));
  TDimension imgSize(m_lrp->m_res.lx, m_lrp->m_res.ly);
  assert(TRect(imgSize).contains(savebox));
  if (imgSize != savebox.getSize() && !savebox.isEmpty()) {
    assert(TRect(imgSize).contains(savebox));
    TRasterCM32P fullRas(imgSize);
    TPixelCM32 bgColor;
    fullRas->fillOutside(savebox, bgColor);
    assert(savebox.getSize() == ras->getSize());
    fullRas->extractT(savebox)->copy(ras);
    ras = fullRas;
  }
  raux->unlock();
  raux = TRasterCM32P();

  // delete [] imgBuff;

  // codec.decompress(m_compressedBuffer, m_compressedBufferSize, ras);

  // assert(0);
  // TToonzImageP ti; //  = new
  // TToonzImage(m_lx,m_ly,savebox,imgBuff,actualBuffSize);
  TToonzImageP ti(ras, savebox);
  // if(dpiflag)
  ti->setDpi(xdpi, ydpi);
  // m_lrp->m_level->setFrame(TFrameId(m_frameIndex+1), ti);
  ti->setPalette(m_lrp->m_level->getPalette());
  // delete [] imgBuff;
  // imgBuff = 0;
  return ti;

  // ToonzImageUtils::updateRas32(ti);
}

// Restituisce la regione del raster shrinkata e la relativa savebox.
static TRect applyShrinkAndRegion(TRasterP &ras, int shrink, TRect region,
                                  TRect savebox) {
  // estraggo la regione solo se essa ha coordinate valide.
  if (!region.isEmpty() && region != TRect() && region.getLx() > 0 &&
      region.getLy() > 0)
    ras = ras->extract(region);
  else
    region = TRect(0, 0, ras->getLx() - 1, ras->getLy() - 1);
  if (shrink > 1) {
    ras = TRop::shrink(ras, shrink);
  }
  // calcolo la nuova savebox
  savebox *= region;
  if (savebox == TRect() || savebox.getLx() <= 0 || savebox.getLy() <= 0)
    return TRect();
  int firstColIndexOfLayer = (savebox.x0 - region.x0) - 1 + shrink -
                             (abs(savebox.x0 - region.x0 - 1) % shrink);
  int firstRowIndexOfLayer = (savebox.y0 - region.y0) - 1 + shrink -
                             (abs(savebox.y0 - region.y0 - 1) % shrink);
  savebox.x0 = (firstColIndexOfLayer) / shrink;
  savebox.y0 = (firstRowIndexOfLayer) / shrink;
  savebox.x1 = savebox.x0 + (savebox.getLx()) / shrink - 1;
  savebox.y1 = savebox.y0 + (savebox.getLy()) / shrink - 1;
  return savebox;
}

//-------------------------------------------------------------------

TImageP TImageReaderTzl::load14() {
  FILE *chan = m_lrp->m_chan;

  if (!chan) return TImageP();
  // SAVEBOX_X0 SAVEBOX_Y0 SAVEBOX_LX SAVEBOX_LY BUFFER_SIZE
  TINT32 sbx0 = 0, sby0 = 0, sblx, sbly;
  TINT32 actualBuffSize;
  double xdpi = 1, ydpi = 1;
  // TINT32 imgBuffSize = 0;
  UCHAR *imgBuff = 0;
  TINT32 iconLx = 0, iconLy = 0;
  assert(!m_lrp->m_frameOffsTable.empty());
  assert(!m_lrp->m_iconOffsTable.empty());
  if (m_lrp->m_frameOffsTable.empty())
    throw TException("Loading tlv: the frames table is empty.");
  if (m_lrp->m_iconOffsTable.empty())
    throw TException("Loading tlv: the frames icons table is empty.");

  TzlOffsetMap::iterator it     = m_lrp->m_frameOffsTable.find(m_fid);
  TzlOffsetMap::iterator iconIt = m_lrp->m_iconOffsTable.find(m_fid);
  if (it == m_lrp->m_frameOffsTable.end() ||
      iconIt == m_lrp->m_iconOffsTable.end())
    throw TException("Loading tlv: frame ID not found.");

  fseek(chan, it->second.m_offs, SEEK_SET);
  fread(&sbx0, sizeof(TINT32), 1, chan);
  fread(&sby0, sizeof(TINT32), 1, chan);
  fread(&sblx, sizeof(TINT32), 1, chan);
  fread(&sbly, sizeof(TINT32), 1, chan);
  fread(&actualBuffSize, sizeof(TINT32), 1, chan);
  fread(&xdpi, sizeof(double), 1, chan);
  fread(&ydpi, sizeof(double), 1, chan);

  if (sbx0 < 0 || sby0 < 0 || sblx < 0 || sbly < 0 || sblx > m_lx ||
      sbly > m_ly)
    throw TException("Loading tlv: savebox dimension error.");

#if !TNZ_LITTLE_ENDIAN
  sbx0           = swapTINT32(sbx0);
  sby0           = swapTINT32(sby0);
  sblx           = swapTINT32(sblx);
  sbly           = swapTINT32(sbly);
  actualBuffSize = swapTINT32(actualBuffSize);
  reverse((char *)&xdpi, sizeof(double));
  reverse((char *)&ydpi, sizeof(double));
#endif

  // Carico l'icona dal file
  if (m_isIcon) {
    fseek(chan, iconIt->second.m_offs, SEEK_SET);
    fread(&iconLx, sizeof(TINT32), 1, chan);
    fread(&iconLy, sizeof(TINT32), 1, chan);
    assert(iconLx > 0 && iconLy > 0);
    if (iconLx < 0 || iconLy < 0 || iconLx > m_lx || iconLy > m_ly)
      throw TException("Loading tlv: bad icon size.");
    fread(&actualBuffSize, sizeof(TINT32), 1, chan);

    if (actualBuffSize <= 0 ||
        actualBuffSize > (int)(iconLx * iconLx * sizeof(TPixelCM32)))
      throw TException("Loading tlv: icon buffer size error.");

    TRasterCM32P raux = TRasterCM32P(iconLx, iconLy);
    if (!raux) return TImageP();
    raux->lock();
    imgBuff = (UCHAR *)raux->getRawData();  // new UCHAR[imgBuffSize];
    fread(imgBuff, actualBuffSize, 1, chan);

#if !TNZ_LITTLE_ENDIAN
    Header *header    = (Header *)imgBuff;
    header->m_lx      = swapTINT32(header->m_lx);
    header->m_ly      = swapTINT32(header->m_ly);
    header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

    TRasterCodecLZO codec("LZO", false);
    TRasterP ras;
    if (!codec.decompress(imgBuff, actualBuffSize, ras, m_safeMode))
      return TImageP();
    assert((TRasterCM32P)ras);
    raux->unlock();
    raux = TRasterCM32P();

#if !TNZ_LITTLE_ENDIAN

    for (int y = 0; y < ras->getLy(); ++y) {
      ras->lock();
      TINT32 *pix    = ((TINT32 *)ras->getRawData(0, y));
      TINT32 *endPix = pix + ras->getLx();
      while (pix < endPix) {
        *pix = swapTINT32(*pix);
        pix++;
      }
      ras->unlock();
    }
#endif

    /*
            TINT32 iconsbx0 = tround((double)iconLx*sbx0/m_lrp->m_res.lx);
            TINT32 iconsby0 = tround((double)iconLy*sby0/m_lrp->m_res.ly);
            TRect savebox(TPoint(iconsbx0,iconsby0),
       TDimension(ras->getLx(),ras->getLy()));
             */
    if (m_lrp->m_res.lx == 0 || m_lrp->m_res.ly == 0) return TImageP();
    // Compute savebox
    if (m_lrp->m_res.lx < 0 || m_lrp->m_res.ly < 0)
      throw TException("Loading tlv: icon resolution error");

    TRect tmp_savebox;
    TRect savebox;
    TRop::computeBBox(ras, tmp_savebox);
    if (tmp_savebox.isEmpty()) {
      TINT32 iconsbx0 = tround((double)iconLx * sbx0 / m_lrp->m_res.lx);
      TINT32 iconsby0 = tround((double)iconLy * sby0 / m_lrp->m_res.ly);
      TINT32 iconsblx =
          std::max(tround((double)iconLx * sblx / m_lrp->m_res.lx), 1);
      TINT32 iconsbly =
          std::max(tround((double)iconLy * sbly / m_lrp->m_res.ly), 1);
      savebox =
          TRect(TPoint(iconsbx0, iconsby0), TDimension(iconsblx, iconsbly));
    } else {
      TINT32 iconsbx0 = tfloor((double)iconLx * sbx0 / m_lrp->m_res.lx);
      TINT32 iconsby0 = tfloor((double)iconLy * sby0 / m_lrp->m_res.ly);
      savebox         = TRect(TPoint(iconsbx0, iconsby0),
                      TDimension(tmp_savebox.getLx(), tmp_savebox.getLy()));
    }

    // int ly = tround((double)m_lrp->m_res.ly*iconLx/m_lrp->m_res.lx);
    // TRect
    // rect(TPoint(0,tround((double)(iconLy-ly)/2)),TDimension(iconLx,ly));
    // TPixelCM32 bgColorRect(1,0,100);

    TDimension imgSize(iconLx, iconLy);
    if (!TRect(imgSize).contains(
            savebox))  // for this 'if', see comment in createIcon method. vinz
      savebox = savebox * TRect(imgSize);
    // if(!TRect(imgSize).contains(savebox)) throw TException("Loading tlv: bad
    // icon savebox size.");

    if (imgSize != savebox.getSize()) {
      TRasterCM32P fullRas(imgSize);
      TPixelCM32 bgColor;

      fullRas->fillOutside(savebox, bgColor);

      // fullRas->extractT(rect)->fill(bgColorRect);
      // assert(savebox.getSize() == ras->getSize());
      if (savebox.getSize() != ras->getSize())
        throw TException("Loading tlv: bad icon savebox size.");
      fullRas->extractT(savebox)->copy(ras);
      ras = fullRas;
    }

    TToonzImageP ti(ras, savebox);
    ti->setDpi(xdpi, ydpi);
    ti->setPalette(m_lrp->m_level->getPalette());
    return ti;
  }
  if (actualBuffSize <= 0 ||
      actualBuffSize > (int)(m_lx * m_ly * sizeof(TPixelCM32)))
    throw TException("Loading tlv: buffer size error");

  TRasterCM32P raux = TRasterCM32P(m_lx, m_ly);

  // imgBuffSize = m_lx*m_ly*sizeof(TPixelCM32);

  raux->lock();
  imgBuff = (UCHAR *)raux->getRawData();  // new UCHAR[imgBuffSize];
  // int ret =

  fread(imgBuff, actualBuffSize, 1, chan);
  // assert(ret==1);

  Header *header = (Header *)imgBuff;

#if !TNZ_LITTLE_ENDIAN
  header->m_lx      = swapTINT32(header->m_lx);
  header->m_ly      = swapTINT32(header->m_ly);
  header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

  TRasterCodecLZO codec("LZO", false);
  TRasterP ras;
  if (!codec.decompress(imgBuff, actualBuffSize, ras, m_safeMode))
    return TImageP();
  assert((TRasterCM32P)ras);
  assert(ras->getLx() == header->m_lx);
  assert(ras->getLy() == header->m_ly);
  if (ras->getLx() != header->m_lx)
    throw TException("Loading tlv: lx dimension error.");
  if (ras->getLy() != header->m_ly)
    throw TException("Loading tlv: ly dimension error.");
  raux->unlock();
  raux = TRasterCM32P();

#if !TNZ_LITTLE_ENDIAN

  for (int y = 0; y < ras->getLy(); ++y) {
    ras->lock();
    TINT32 *pix    = ((TINT32 *)ras->getRawData(0, y));
    TINT32 *endPix = pix + ras->getLx();
    while (pix < endPix) {
      *pix = swapTINT32(*pix);
      pix++;
    }
    ras->unlock();
  }

// codec.compress(ras, 1, &imgBuff, actualBuffSize);
#endif

  TRect savebox(TPoint(sbx0, sby0), TDimension(sblx, sbly));
  TDimension imgSize(m_lrp->m_res.lx, m_lrp->m_res.ly);
  assert(TRect(imgSize).contains(savebox));
  if (!TRect(imgSize).contains(savebox))
    throw TException("Loading tlv: bad savebox size.");
  if (imgSize != savebox.getSize()) {
    TRasterCM32P fullRas(imgSize);
    TPixelCM32 bgColor;
    if (!savebox.isEmpty()) {
      fullRas->fillOutside(savebox, bgColor);
      assert(savebox.getSize() == ras->getSize());
      if (savebox.getSize() != ras->getSize())
        throw TException("Loading tlv: bad icon savebox size.");
      fullRas->extractT(savebox)->copy(ras);
    } else
      fullRas->clear();
    ras = fullRas;
  }

  TToonzImageP ti(ras, savebox);
  // if(dpiflag)
  ti->setDpi(xdpi, ydpi);
  // m_lrp->m_level->setFrame(TFrameId(m_frameIndex+1), ti);
  ti->setPalette(m_lrp->m_level->getPalette());
  // delete [] imgBuff;
  // imgBuff = 0;
  return ti;

  // ToonzImageUtils::updateRas32(ti);
}

//-------------------------------------------------------------------

TImageP TImageReaderTzl::load() {
  int version   = m_lrp->m_version;
  TImageP image = TImageP();
  switch (version) {
  case 11:
    if (!m_lrp->m_frameOffsTable.empty()) image = load11();
    break;
  case 12:
    if (!m_lrp->m_frameOffsTable.empty()) image = load11();
    break;
  case 13:
    if (!m_lrp->m_frameOffsTable.empty() && !m_lrp->m_iconOffsTable.empty())
      image = load13();
    break;
  case 14:
    if (!m_lrp->m_frameOffsTable.empty() && !m_lrp->m_iconOffsTable.empty())
      image = load14();
    break;
  default:
    image = load10();
  }
  if (image == TImageP()) return TImageP();

  if (!m_isIcon) {
    TToonzImageP ti = image;
    if (!ti) return TImageP();
    TRasterP ras  = ti->getRaster();
    TRect savebox = ti->getSavebox();
    if (m_region != TRect() && m_region.getLx() > 0 && m_region.getLy() > 0) {
      m_region *= TRect(0, 0, ti->getSize().lx, ti->getSize().ly);
      if (m_region.isEmpty() || m_region == TRect() || m_region.getLx() <= 0 ||
          m_region.getLy() <= 0)
        return TImageP();
    }
    savebox = applyShrinkAndRegion(ras, m_shrink, m_region, savebox);
    if (savebox == TRect()) {
      if (m_region != TRect()) {
        ras = ras->create(m_region.getLx(), m_region.getLy());
        ras->clear();
        savebox = m_region;
      } else {
        // se sia la savebox che la regione sono vuote non faccio nulla
      }
    }
    ti->setCMapped(ras);
    ti->setSavebox(savebox);
    return ti;
  }
  return image;
}
//-------------------------------------------------------------------

const TImageInfo *TImageReaderTzl::getImageInfo11() const {
  assert(!m_lrp->m_frameOffsTable.empty());
  FILE *chan = m_lrp->m_chan;
  if (!chan) return 0;

  TzlOffsetMap::iterator it = m_lrp->m_frameOffsTable.find(m_fid);

  if (it == m_lrp->m_frameOffsTable.end()) return 0;

  fseek(chan, it->second.m_offs, SEEK_SET);

  // SAVEBOX_X0 SAVEBOX_Y0 SAVEBOX_LX SAVEBOX_LY BUFFER_SIZE
  TINT32 sbx0, sby0, sblx, sbly;
  TINT32 actualBuffSize;
  double xdpi = 1, ydpi = 1;
  //  TINT32 imgBuffSize = 0;

  // int pos = ftell(chan);

  fread(&sbx0, sizeof(TINT32), 1, chan);
  fread(&sby0, sizeof(TINT32), 1, chan);
  fread(&sblx, sizeof(TINT32), 1, chan);
  fread(&sbly, sizeof(TINT32), 1, chan);
  fread(&actualBuffSize, sizeof(TINT32), 1, chan);

  fread(&xdpi, sizeof(double), 1, chan);
  fread(&ydpi, sizeof(double), 1, chan);

#if !TNZ_LITTLE_ENDIAN
  sbx0           = swapTINT32(sbx0);
  sby0           = swapTINT32(sby0);
  sblx           = swapTINT32(sblx);
  sbly           = swapTINT32(sbly);
  actualBuffSize = swapTINT32(actualBuffSize);
  reverse((char *)&xdpi, sizeof(double));
  reverse((char *)&ydpi, sizeof(double));
#endif

  static TImageInfo info;
  info.m_x0   = sbx0;
  info.m_y0   = sby0;
  info.m_x1   = sbx0 + sblx - 1;
  info.m_y1   = sby0 + sbly - 1;
  info.m_lx   = m_lx;
  info.m_ly   = m_ly;
  info.m_dpix = xdpi;
  info.m_dpiy = ydpi;

  // m_lrp->m_frameIndex = m_frameIndex;
  return &info;
}

//-------------------------------------------------------------------

const TImageInfo *TImageReaderTzl::getImageInfo10() const {
  FILE *chan = m_lrp->m_chan;
  if (!chan) return 0;

  // SAVEBOX_X0 SAVEBOX_Y0 SAVEBOX_LX SAVEBOX_LY BUFFER_SIZE
  TINT32 sbx0, sby0, sblx, sbly;
  TINT32 actualBuffSize;
  double xdpi = 1, ydpi = 1;
  TINT32 imgBuffSize = 0;
  UCHAR *imgBuff     = 0;
  int frameIndex     = m_fid.getNumber();

  // int pos = ftell(chan);
  assert(m_lrp->m_frameOffsTable[TFrameId(1)].m_offs > 0);
  int k;
  if (m_lrp->m_frameOffsTable[TFrameId(frameIndex)].m_offs == 0) {
    int i;
    for (i = 2;
         m_lrp->m_frameOffsTable[TFrameId(i)].m_offs != 0 && i <= frameIndex;
         i++)
      ;
    if (i == frameIndex) i = 2;

    fseek(chan, m_lrp->m_frameOffsTable[TFrameId(i - 1)].m_offs, SEEK_SET);
    k = i - 1;
  } else {
    fseek(chan, m_lrp->m_frameOffsTable[TFrameId(frameIndex)].m_offs, SEEK_SET);
    k = frameIndex;
  }

  for (; k <= frameIndex; k++) {
    m_lrp->m_frameOffsTable[TFrameId(k)] = TzlChunk(ftell(chan), 0);
    fread(&sbx0, sizeof(TINT32), 1, chan);
    fread(&sby0, sizeof(TINT32), 1, chan);
    fread(&sblx, sizeof(TINT32), 1, chan);
    fread(&sbly, sizeof(TINT32), 1, chan);
    fread(&actualBuffSize, sizeof(TINT32), 1, chan);

    fread(&xdpi, sizeof(double), 1, chan);
    fread(&ydpi, sizeof(double), 1, chan);

#if !TNZ_LITTLE_ENDIAN
    sbx0           = swapTINT32(sbx0);
    sby0           = swapTINT32(sby0);
    sblx           = swapTINT32(sblx);
    sbly           = swapTINT32(sbly);
    actualBuffSize = swapTINT32(actualBuffSize);
    reverse((char *)&xdpi, sizeof(double));
    reverse((char *)&ydpi, sizeof(double));
#endif

    imgBuffSize = m_lx * m_ly * sizeof(TPixelCM32);
    assert(actualBuffSize <= imgBuffSize);
    delete[] imgBuff;
    imgBuff = new UCHAR[imgBuffSize];
    fread(imgBuff, actualBuffSize, 1, chan);
  }

  Header *header = (Header *)imgBuff;

#if !TNZ_LITTLE_ENDIAN
  header->m_lx      = swapTINT32(header->m_lx);
  header->m_ly      = swapTINT32(header->m_ly);
  header->m_rasType = (Header::RasType)swapTINT32(header->m_rasType);
#endif

  delete[] imgBuff;

  static TImageInfo info;
  info.m_x0   = sbx0;
  info.m_y0   = sby0;
  info.m_x1   = sbx0 + sblx - 1;
  info.m_y1   = sby0 + sbly - 1;
  info.m_lx   = m_lx;
  info.m_ly   = m_ly;
  info.m_dpix = xdpi;
  info.m_dpiy = ydpi;

  // m_lrp->m_frameIndex = m_frameIndex;
  return &info;
}

//-------------------------------------------------------------------

const TImageInfo *TImageReaderTzl::getImageInfo() const {
  if (m_lrp->m_version > 10 && !m_lrp->m_frameOffsTable.empty())
    return getImageInfo11();
  else
    return getImageInfo10();
}

//-------------------------------------------------------------------

TDimension TImageReaderTzl::getSize() const { return TDimension(m_lx, m_ly); }

//-------------------------------------------------------------------

TRect TImageReaderTzl::getBBox() const { return TRect(getSize()); }
