

#include "tiio_zcc.h"
//#include "tparam.h"
#include "tsystem.h"

#ifdef DOPO_LO_FACCIAMO

#define PID_TAG(X)                                                             \
  (((X) >> 24) | ((X >> 8) & 0x0000FF00) | ((X << 8) & 0x00FF0000) |           \
   ((X) << 24))

/*

HEADER 'HDR '  32 bytes >
  0 |  4  |  Magic number 'ZCC '
  4 |  4  |  Major revision
  8 |  4  |  Minor revision
 12 |  4  |  Frame rate num
 16 |  4  |  Frame rate den
 20 |  4  |  Frame count
 24 |  4  |  Compressor type ( 'YUV2', 'RGBM' )
 28 |  4  |  Padding (future use)
<HEADER


FRAMEDATA 'FRME' >
  0 FRAMEDESCRIPTOR 'DSCR' 16 bytes >
     0 |  4  |  Frame ID
     4 |  4  |  Width
     8 |  4  |  Height
    12 |  4  |  RawData size
   <FRAMEDESCRIPTOR

  16 RAWDATA 'DATA' RawDataSize >
     0 |
   <RAWDATA

<FRAMEDATA

*/

/*
class ZCCParser {
TFile &m_file;
public:
  ZCCParser(TFile&file) : m_file(file)
    {
    m_file.seek(0);
    string tag = readTag();
    }
private:
  string readTag()
    {


    }
};

*/
//-----------------------------------------------------------

TReaderWriterInfo *TReaderWriterInfoZCC::create(const string &) {
  return new TReaderWriterInfoZCC();
}

//-----------------------------------------------------------

TReaderWriterInfoZCC::TReaderWriterInfoZCC() {
  int c               = 0;
  TIntEnumParamP type = new TIntEnumParam();

  type->addItem(c++, "RGBM Uncompressed");
  type->addItem(c++, "YUV  Uncompressed");
  addParam("codec", type.getPointer());
}

//-----------------------------------------------------------

TReaderWriterInfo *TReaderWriterInfoZCC::clone() const {
  return new TReaderWriterInfoZCC(*this);
}

//-----------------------------------------------------------

TReaderWriterInfoZCC::~TReaderWriterInfoZCC() {}

//-----------------------------------------------------------

TReaderWriterInfoZCC::TReaderWriterInfoZCC(const TReaderWriterInfoZCC &src)
    : TReaderWriterInfo(src) {}

//===========================================================
class TImageReaderWriterZCC : public TImageReaderWriter {
public:
  TImageReaderWriterZCC(const TFilePath &fp, int index,
                        TLevelReaderWriterZCC *lrw);

private:
  // not implemented
  TImageReaderWriterZCC(const TImageReaderWriterZCC &);
  TImageReaderWriterZCC &operator=(const TImageReaderWriterZCC &src);

public:
  void save(const TImageP &);
  TImageP load();
  void load(const TRasterP &ras, const TPoint &pos = TPoint(0, 0),
            int shrinkX = 1, int shrinkY = 1);
  int m_index;

private:
  TLevelReaderWriterZCC *m_lrw;
};

//------------------------------------------------------------------------------

using namespace TFileConsts;
TLevelReaderWriterZCC::TLevelReaderWriterZCC(const TFilePath &path,
                                             TReaderWriterInfo *winfo)
    : TLevelReaderWriter(path, winfo)
    , m_file(path, kReadWrite | kUnbuffered | kOpenAlways)
    , m_indexFile(path.withType("ndx"), kReadWrite | kOpenAlways)
    , m_initDone(false)
    , m_blockSize(0) {
  if (!m_file.isOpen()) throw TImageException(path, m_file.getLastError());

  TFilePathSet fps = TSystem::getDisks();
  TFilePath disk;

  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
    disk = *it;
    if (disk.isAncestorOf(m_path)) {
      DWORD sectorsPerCluster;
      // DWORD bytesPerSector;
      DWORD numberOfFreeClusters;
      DWORD totalNumberOfClusters;

      BOOL rc = GetDiskFreeSpaceW(
          disk.getWideString().c_str(), &sectorsPerCluster, &m_blockSize,
          &numberOfFreeClusters, &totalNumberOfClusters);
      break;
    }
  }
  assert(m_blockSize);
}

//-----------------------------------------------------------

void TLevelReaderWriterZCC::saveSoundTrack(TSoundTrack *st) {}
//-----------------------------------------------------------

TLevelP TLevelReaderWriterZCC::loadInfo() {
  TLevelP level;
  if (!m_file.size()) return level;

  m_file.seek(0);
  UCHAR *buffer = new UCHAR[m_blockSize];

  m_file.read(buffer, m_blockSize);

  ULONG magicNumber = *(ULONG *)&buffer[0];
  assert(magicNumber == PID_TAG('ZCC '));
  ULONG majorRevision  = *(ULONG *)&buffer[4];
  ULONG minorRevision  = *(ULONG *)&buffer[8];
  ULONG frameRateNum   = *(ULONG *)&buffer[12];
  ULONG frameRateDen   = *(ULONG *)&buffer[16];
  ULONG frameCount     = *(ULONG *)&buffer[20];
  ULONG compressorType = *(ULONG *)&buffer[24];

  delete[] buffer;

  TINT64 offset = m_blockSize;
  TINT64 fs     = m_file.size();
  while (offset < fs) {
    m_file.seek(offset);
    m_file.read(buffer, m_blockSize);

    ULONG frameId     = *(ULONG *)&buffer[0];
    ULONG width       = *(ULONG *)&buffer[4];
    ULONG height      = *(ULONG *)&buffer[8];
    ULONG rawDataSize = *(ULONG *)&buffer[12];

    TINT64 d = rawDataSize + 16;

    int headerSize = 16;
    int dataSize   = rawDataSize + headerSize;

    long bufferSize = tceil(dataSize / (double)m_blockSize) * m_blockSize;

    m_map[frameId] = C(offset, bufferSize, width, height);
    level->setFrame(TFrameId(frameId), TImageP());

    offset += bufferSize;
  }
  return level;
}

//-----------------------------------------------------------

TImageReaderWriterP TLevelReaderWriterZCC::getFrameReaderWriter(TFrameId id) {
  TImageReaderWriterZCC *iwm =
      new TImageReaderWriterZCC(m_path, id.getNumber(), this);
  return TImageReaderWriterP(iwm);
}

//-----------------------------------------------------------

TLevelReaderWriterZCC::~TLevelReaderWriterZCC() {
  m_file.seek(0);
  UCHAR *buffer = new UCHAR[m_blockSize];
  memset(buffer, 0, m_blockSize);

  ULONG *magicNumber = (ULONG *)&buffer[0];
  *magicNumber       = PID_TAG('ZCC ');

  ULONG *majorRevision = (ULONG *)&buffer[4];
  *majorRevision       = 0x0001;

  ULONG *minorRevision = (ULONG *)&buffer[8];
  *minorRevision       = 0x0000;

  ULONG *frameRateNum = (ULONG *)&buffer[12];
  *frameRateNum       = 25;

  ULONG *frameRateDen = (ULONG *)&buffer[16];
  *frameRateDen       = 1;

  ULONG *frameCount = (ULONG *)&buffer[20];
  *frameCount       = 0xffff;

  ULONG *compressorType = (ULONG *)&buffer[24];
  *compressorType       = PID_TAG('RGBM');

  m_file.write(buffer, m_blockSize);
  delete[] buffer;
}

//-----------------------------------------------------------

TImageReaderWriterZCC::TImageReaderWriterZCC(const TFilePath &fp, int index,
                                             TLevelReaderWriterZCC *lrw)
    : TImageReaderWriter(fp), m_lrw(lrw), m_index(index) {}

//-----------------------------------------------------------

void TImageReaderWriterZCC::save(const TImageP &img) {
  TFile &file = m_lrw->m_file;

  TRaster32P ras = TRasterImageP(img)->getRaster();

  int rasDataSize = ras->getRowSize() * ras->getLy();
  int headerSize  = 16;
  int dataSize    = rasDataSize + headerSize;

  int bufferSize =
      tceil(dataSize / (double)m_lrw->m_blockSize) * m_lrw->m_blockSize;

  TINT64 offset         = bufferSize * m_index + m_lrw->m_blockSize;
  m_lrw->m_map[m_index] = C(offset, bufferSize, ras->getLx(), ras->getLy());

  if (!file.seek(offset /*per l'header*/))
    throw TImageException(m_path, file.getLastError());

  TRasterImageP rasImage = img;

  UCHAR *buffer = new UCHAR[bufferSize];

  ULONG *frameId = (ULONG *)&buffer[0];
  *frameId       = m_index;

  ULONG *width = (ULONG *)&buffer[4];
  *width       = ras->getLx();

  ULONG *height           = (ULONG *)&buffer[8];
  *height                 = ras->getLy();
  bwww ULONG *rawDataSize = (ULONG *)&buffer[12];
  *rawDataSize            = rasDataSize;
  memcpy(&buffer[16], ras->getRawData(), rasDataSize);

  if (!file.write(buffer, bufferSize))
    throw TImageException(m_path, file.getLastError());
  delete[] buffer;
}

//-----------------------------------------------------------

TImageP TImageReaderWriterZCC::load() {
  C c = m_lrw->m_map[m_index];

  TRaster32P ras(c.m_lx, c.m_ly);
  load(ras);
  return TImageP(TRasterImageP(ras));
}

//-----------------------------------------------------------

void TImageReaderWriterZCC::load(const TRasterP &ras, const TPoint &pos,
                                 int shrinkX, int shrinkY) {
  if (!m_lrw->m_initDone) {
    //
    // write header
    //
    m_lrw->m_initDone = true;
  }
  TFile &file = m_lrw->m_file;

  /*
std::map<int, C>::iterator it = std::find(m_lrw->m_map.begin(),
m_lrw->m_map.end(), m_index);
if (it == m_lrw->m_map.end())
return;
*/

  C c = m_lrw->m_map[m_index];

  if (!file.seek(c.m_offset))
    throw TImageException(m_path, file.getLastError());

  UCHAR *buffer = new UCHAR[c.m_size];

  if (!file.read(buffer, c.m_size))
    throw TImageException(m_path, file.getLastError());

  ULONG *frameId = (ULONG *)&buffer[0];
  assert(*frameId == (ULONG)m_index);

  ULONG *width = (ULONG *)&buffer[4];

  ULONG *height = (ULONG *)&buffer[8];

  ULONG *rawDataSize = (ULONG *)&buffer[12];

  assert((ULONG)ras->getLx() == *width);
  assert((ULONG)ras->getLy() == *height);

  memcpy(ras->getRawData(), &buffer[16], *rawDataSize);

  delete[] buffer;
}

#endif
