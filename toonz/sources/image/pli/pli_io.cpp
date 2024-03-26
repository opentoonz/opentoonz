#include <memory>

#include "tmachine.h"
#include "pli_io.h"
#include "tcommon.h"
#include "timage_io.h"
#include "tvectorimage.h"
//#include "tstrokeoutline.h"
#include "tsimplecolorstyles.h"
#include "tcontenthistory.h"
#include "tfilepath_io.h"
//#include <fstream.h>
#include "../compatibility/tfile_io.h"
#include "tenv.h"

/*=====================================================================*/

#if defined(MACOSX)
#include <architecture/i386/io.h>
#elif defined(_WIN32)
#include <io.h>
#endif

using namespace std;

typedef TVectorImage::IntersectionBranch IntersectionBranch;

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

    static const int c_majorVersionNumber = 150;
static const int c_minorVersionNumber     = 0;

/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/

inline void ulongFromDouble1(double x, TUINT32 &hi, TUINT32 &lo) {
  assert(x < 1.0);
  // x+=1.0;
  TUINT32 *l = (TUINT32 *)&x;
#if TNZ_LITTLE_ENDIAN
  hi = l[1], lo = l[0];
#else
  hi = l[0], lo = l[1];
#endif

  // return (hi&0XFFFFF)<<12 | ((lo&0xFFE00000)>>20);
}

/*=====================================================================*/

inline double doubleFromUlong1(TUINT32 hi, TUINT32 lo) {
  // assert((lo&0X00000001)==0);
  TUINT32 l[2];
#if TNZ_LITTLE_ENDIAN
  l[1] = hi;
  l[0] = lo;
#else
  l[0] = hi;
  l[1] = lo;
#endif

  return *(double *)l;  // - 1;
}

/*=====================================================================*/

static TThickPoint operator*(const TAffine &aff, const TThickPoint &p) {
  TPointD p1(p.x, p.y);
  return TThickPoint(aff * p1, p.thick);
}

/*=====================================================================*/

class MyOfstream final : public Tofstream {
public:
  MyOfstream(const TFilePath &path) : Tofstream(path) {}

  MyOfstream &operator<<(TUINT32 n) {
    TUINT32 app;

#if TNZ_LITTLE_ENDIAN
    app = n;
#else
    UCHAR *uc = (UCHAR *)&n;
    app = *(uc) | (*(uc + 1)) << 8 | (*(uc + 2)) << 16 | (*(uc + 3)) << 24;
#endif
    write((char *)&app, sizeof(TUINT32));
    return *this;
  }

  MyOfstream &operator<<(USHORT n) {
    USHORT app;

#if TNZ_LITTLE_ENDIAN
    app = n;
#else
    UCHAR *uc      = (UCHAR *)&n;
    app            = *(uc) | (*(uc + 1)) << 8;
#endif

    write((char *)&app, sizeof(USHORT));
    return *this;
  }

  MyOfstream &operator<<(UCHAR un) {
    write((char *)&un, sizeof(UCHAR));
    return *this;
  }
  MyOfstream &operator<<(char un) {
    write((char *)&un, sizeof(char));
    return *this;
  }
  MyOfstream &operator<<(const TRaster32P &r);
  MyOfstream &operator<<(const std::string &r);
  MyOfstream &writeBuf(void *un, UINT s) {
    write((char *)un, s);
    return *this;
  }
};

/*=====================================================================*/

MyOfstream &MyOfstream::operator<<(const TRaster32P &r) {
  assert(r->getLx() == r->getWrap());

  (*this) << (USHORT)r->getLx();
  (*this) << (USHORT)r->getLy();
  r->lock();
  MyOfstream &ret =
      writeBuf(r->getRawData(), r->getLx() * r->getLy() * r->getPixelSize());
  r->unlock();

  return ret;
}

/*=====================================================================*/

MyOfstream &MyOfstream::operator<<(const std::string &s) {
  (*this) << (USHORT)s.size();
  for (UINT i = 0; i < s.size(); i++) (*this) << (UCHAR)(s[i]);
  return *this;
}

/*=====================================================================*/

class MyIfstream  // The input is done without stl; it was crashing in release
                  // version loading textures!!
{
private:
  bool m_isIrixEndian;
  FILE *m_fp;

public:
  MyIfstream() : m_isIrixEndian(false) { m_fp = 0; }
  ~MyIfstream() {
    if (m_fp) fclose(m_fp);
  }
  void setEndianness(bool isIrixEndian) { m_isIrixEndian = isIrixEndian; }
  MyIfstream &operator>>(TUINT32 &un);
  MyIfstream &operator>>(string &un);
  MyIfstream &operator>>(USHORT &un);
  MyIfstream &operator>>(UCHAR &un);
  MyIfstream &operator>>(char &un);
  void open(const TFilePath &filename);
  void close() {
    if (m_fp) fclose(m_fp);
    m_fp = 0;
  }
  TUINT32 tellg() { return (TUINT32)ftell(m_fp); }
  // void seekg(TUINT32 pos, ios_base::seek_dir type);
  void seekg(TUINT32 pos, int type);
  void read(char *m_buf, int length) {
    fread((void *)m_buf, sizeof(char), length, m_fp);
  }
};

/*=====================================================================*/

void MyIfstream::open(const TFilePath &filename) {
  try {
    m_fp = fopen(filename, "rb");
  } catch (TException &) {
    throw TImageException(filename, "File not found");
  }
}

/*=====================================================================*/

void MyIfstream::seekg(TUINT32 pos, int type) {
  if (type == ios_base::beg)
    fseek(m_fp, pos, SEEK_SET);
  else if (type == ios_base::cur)
    fseek(m_fp, pos, SEEK_CUR);
  else
    assert(false);
}

/*=====================================================================*/

inline MyIfstream &MyIfstream::operator>>(UCHAR &un) {
  int ret = fread((void *)&un, sizeof(UCHAR), 1, m_fp);
  if (ret < 1) throw TException("corrupted pli file: unexpected end of file");
  // read((char *)&un, sizeof(UCHAR));
  return *this;
}

/*=====================================================================*/

inline MyIfstream &MyIfstream::operator>>(char &un) {
  int ret = fread((void *)&un, sizeof(char), 1, m_fp);
  if (ret < 1) throw TException("corrupted pli file: unexpected end of file");
  // read((char *)&un, sizeof(char));
  return *this;
}

/*=====================================================================*/

inline MyIfstream &MyIfstream::operator>>(USHORT &un) {
  int ret = fread((void *)&un, sizeof(USHORT), 1, m_fp);
  if (ret < 1) throw TException("corrupted pli file: unexpected end of file");
  //  read((char *)&un, sizeof(USHORT));

  if (m_isIrixEndian) un = ((un & 0xff00) >> 8) | ((un & 0x00ff) << 8);
  return *this;
}

/*=====================================================================*/

inline MyIfstream &MyIfstream::operator>>(TUINT32 &un) {
  int ret = fread((void *)&un, sizeof(TUINT32), 1, m_fp);
  if (ret < 1) throw TException("corrupted pli file: unexpected end of file");
  // read((char *)&un, sizeof(TUINT32));

  if (m_isIrixEndian)
    un = ((un & 0xff000000) >> 24) | ((un & 0x00ff0000) >> 8) |
         ((un & 0x0000ff00) << 8) | ((un & 0x000000ff) << 24);
  return *this;
}

/*=====================================================================*/

inline MyIfstream &MyIfstream::operator>>(string &un) {
  string s = "";
  USHORT length;
  (*this) >> length;
  for (UINT i = 0; i < length; i++) {
    UCHAR ch;
    (*this) >> ch;
    s.append(1, ch);
  }
  un = s;

  return *this;
}

/*=====================================================================*/

UINT TStyleParam::getSize() {
  switch (m_type) {
  case SP_BYTE:
    return 1;
  case SP_INT:
    return 4;
  case SP_DOUBLE:
    return 4;
  case SP_USHORT:
    return 2;
  case SP_RASTER:
    return 2 + 2 + m_r->getLx() * m_r->getLy() * m_r->getPixelSize();
  case SP_STRING:
    return (m_string.size() + sizeof(USHORT));
  default:
    assert(false);
    return 0;
  }
}

/*=====================================================================*/

#define CHECK_FOR_READ_ERROR(filePath)

#define CHECK_FOR_WRITE_ERROR(filePath)                                        \
  {                                                                            \
    if (m_oChan->fail() /*m_oChan.flags()&(ios::failbit)*/) {                  \
      m_lastError = WRITE_ERROR;                                               \
      throw TImageException(filePath, "Error on writing file");                \
    }                                                                          \
  }

const TUINT32 c_magicNt   = 0x4D494C50;
const TUINT32 c_magicIrix = 0x504C494D;

class TagElem {
public:
  PliTag *m_tag;
  TUINT32 m_offset;
  TagElem *m_next;

  TagElem(PliTag *tag, TUINT32 offset, TagElem *next = NULL)
      : m_tag(tag), m_offset(offset), m_next(next) {}

  TagElem(const TagElem &elem)
      : m_tag(elem.m_tag), m_offset(elem.m_offset), m_next(NULL) {}

  ~TagElem() {
    if (m_tag) delete m_tag;
  }
};

/*=====================================================================*/
class TContentHistory;

class ParsedPliImp {
public:
  UCHAR m_majorVersionNumber;
  UCHAR m_minorVersionNumber;
  bool m_versionLocked = false;
  USHORT m_framesNumber;
  double m_thickRatio;
  double m_maxThickness;
  double m_autocloseTolerance;
  bool m_isIrixEndian;
  TFilePath m_filePath;
  UCHAR m_currDynamicTypeBytesNum;
  TUINT32 m_tagLength;
  TUINT32 m_bufLength;
  std::unique_ptr<UCHAR[]> m_buf;
  TAffine m_affine;
  int m_precisionScale;
  std::map<TFrameId, int> m_frameOffsInFile;

  PliTag *readTextTag();
  PliTag *readPaletteTag();
  PliTag *readPaletteWithAlphaTag();
  PliTag *readThickQuadraticChainTag(bool isLoop);
  PliTag *readColorTag();
  PliTag *readStyleTag();
  PliTag *readGroupTag();
  PliTag *readImageTag();
  PliTag *readGeometricTransformationTag();
  PliTag *readDoublePairTag();
  PliTag *readBitmapTag();
  PliTag *readIntersectionDataTag();
  PliTag *readOutlineOptionsTag();
  PliTag *readPrecisionScaleTag();
  PliTag *readAutoCloseToleranceTag();

  inline void readDynamicData(TUINT32 &val, TUINT32 &bufOffs);
  inline bool readDynamicData(TINT32 &val, TUINT32 &bufOffs);
  inline void readFloatData(double &val, TUINT32 &bufOffs);

  inline UINT readRasterData(TRaster32P &r, TUINT32 &bufOffs);
  inline void writeFloatData(double val);
  inline void readUShortData(USHORT &val, TUINT32 &bufOffs);
  inline void readTUINT32Data(TUINT32 &val, TUINT32 &bufOffs);

  TUINT32 writeTagHeader(UCHAR type, UINT tagLength);
  TUINT32 writeTextTag(TextTag *tag);
  TUINT32 writePaletteTag(PaletteTag *tag);
  TUINT32 writePaletteWithAlphaTag(PaletteWithAlphaTag *tag);
  TUINT32 writeThickQuadraticChainTag(ThickQuadraticChainTag *tag);
  TUINT32 writeGroupTag(GroupTag *tag);
  TUINT32 writeImageTag(ImageTag *tag);
  TUINT32 writeColorTag(ColorTag *tag);
  TUINT32 writeStyleTag(StyleTag *tag);
  TUINT32 writeGeometricTransformationTag(GeometricTransformationTag *tag);
  TUINT32 writeDoublePairTag(DoublePairTag *tag);
  TUINT32 writeBitmapTag(BitmapTag *tag);
  TUINT32 writeIntersectionDataTag(IntersectionDataTag *tag);
  TUINT32 writeOutlineOptionsTag(StrokeOutlineOptionsTag *tag);
  TUINT32 writePrecisionScaleTag(PrecisionScaleTag *tag);
  TUINT32 writeAutoCloseToleranceTag(AutoCloseToleranceTag *tag);

  inline void writeDynamicData(TUINT32 val);
  inline void writeDynamicData(TINT32 val, bool isNegative);

  inline void setDynamicTypeBytesNum(int minval, int maxval);

  PliTag *findTagFromOffset(UINT tagOffs);
  UINT findOffsetFromTag(PliTag *tag);
  TagElem *findTag(PliTag *tag);
  USHORT readTagHeader();

public:
  enum errorType {
    NO__ERROR = 0,
    NO_FILE,
    BAD_MAGIC,
    PREMATURE_EOF,
    WRITE_ERROR,
    ERRORTYPE_HOW_MANY
  };

  errorType m_lastError;
  string m_creator;

  TagElem *m_firstTag;
  TagElem *m_lastTag;
  TagElem *m_currTag;

  MyIfstream m_iChan;
  MyOfstream *m_oChan;

  ParsedPliImp();
  ParsedPliImp(UCHAR majorVersionNumber, UCHAR minorVersionNumber,
               USHORT framesNumber, UCHAR precision, UCHAR maxThickness,
               double autocloseTolerance);
  ParsedPliImp(const TFilePath &filename, bool readInfo);
  ~ParsedPliImp();

  void setFrameCount(int frameCount);
  int getFrameCount();

  void loadInfo(bool readPalette, TPalette *&palette,
                TContentHistory *&history);
  ImageTag *loadFrame(const TFrameId &frameNumber);

  TagElem *readTag();
  void writeTag(TagElem *tag);

  bool addTag(PliTag *tag, bool addFront = false);
  bool addTag(const TagElem &tag, bool addFront = false);
  bool writePli(const TFilePath &filename);
  inline void WRITE_UCHAR_FROM_DOUBLE(double dval);
  inline void WRITE_SHORT_FROM_DOUBLE(double dval);
};

/*=====================================================================*/
double ParsedPli::getMaxThickness() const { return imp->m_maxThickness; };
void ParsedPli::setMaxThickness(double maxThickness) {
  imp->m_maxThickness = maxThickness;
};

/* indirect inclusion of <math.h> causes 'abs' to return double on Linux */
#if defined(LINUX) || defined(FREEBSD) || (defined(_WIN32) && defined(__GNUC__)) || defined(HAIKU)
template <typename T>
T abs_workaround(T a) {
  return (a > 0) ? a : -a;
}
#define abs abs_workaround
#endif

/*=====================================================================*/

static inline UCHAR complement1(char val, bool isNegative = false) {
  if (val == 0) return isNegative ? 0x80 : 0;

  return (UCHAR)(abs(val) | (val & 0x80));
}

/*=====================================================================*/

static inline USHORT complement1(short val, bool isNegative = false) {
  if (val == 0) return isNegative ? 0x8000 : 0;

  return (USHORT)(abs(val) | (val & 0x8000));
}

/*=====================================================================*/

static inline TUINT32 complement1(TINT32 val, bool isNegative = false) {
  if (val == 0) return isNegative ? 0x80000000 : 0;

  return (TUINT32)(abs(val) | (val & 0x80000000));
}

/*=====================================================================*/

static inline short complement2(USHORT val) {
  return (val & 0x8000) ? -(val & 0x7fff) : (val & 0x7fff);
}

#if defined(LINUX) || defined(FREEBSD) || (defined(_WIN32) && defined(__GNUC__))
#undef abs
#endif

/*=====================================================================*/

ParsedPliImp::ParsedPliImp()
    : m_majorVersionNumber(0)
    , m_minorVersionNumber(0)
    , m_framesNumber(0)
    , m_thickRatio(1.0)
    , m_maxThickness(0.0)
    , m_firstTag(NULL)
    , m_lastTag(NULL)
    , m_currTag(NULL)
    , m_iChan()
    , m_oChan(0)
    , m_bufLength(0)
    , m_affine()
    , m_precisionScale(REGION_COMPUTING_PRECISION)
    , m_creator("") {}

/*=====================================================================*/

ParsedPliImp::ParsedPliImp(UCHAR majorVersionNumber, UCHAR minorVersionNumber,
                           USHORT framesNumber, UCHAR precision,
                           UCHAR maxThickness, double autocloseTolerance)
    : m_majorVersionNumber(majorVersionNumber)
    , m_minorVersionNumber(minorVersionNumber)
    , m_framesNumber(framesNumber)
    , m_maxThickness(maxThickness)
    , m_autocloseTolerance(autocloseTolerance)
    , m_thickRatio(maxThickness / 255.0)
    , m_firstTag(NULL)
    , m_lastTag(NULL)
    , m_currTag(NULL)
    , m_iChan()
    , m_oChan(0)
    , m_bufLength(0)
    , m_affine(TScale(1.0 / pow(10.0, precision)))
    , m_precisionScale(REGION_COMPUTING_PRECISION)
    , m_creator("") {}

/*=====================================================================*/

ParsedPliImp::ParsedPliImp(const TFilePath &filename, bool readInfo)
    : m_majorVersionNumber(0)
    , m_minorVersionNumber(0)
    , m_framesNumber(0)
    , m_thickRatio(1.0)
    , m_maxThickness(0)
    , m_firstTag(NULL)
    , m_lastTag(NULL)
    , m_currTag(NULL)
    , m_iChan()
    , m_oChan(0)
    , m_bufLength(0)
    , m_precisionScale(REGION_COMPUTING_PRECISION)
    , m_creator("") {
  TUINT32 magic;
  //  TUINT32 fileLength;
  TagElem *tagElem;
  UCHAR maxThickness;

  // cerr<<m_filePath<<endl;

  //#ifdef _WIN32
  m_iChan.open(filename);

  // m_iChan.exceptions( ios::failbit | ios::badbit);
  //#else
  //  m_iChan.open(filename.c_str(), ios::in);
  //#endif

  m_iChan >> magic;

  CHECK_FOR_READ_ERROR(filename);

  if (magic == c_magicNt) {
#if TNZ_LITTLE_ENDIAN
    m_isIrixEndian = false;
#else
    m_isIrixEndian = true;
#endif
    m_iChan.setEndianness(false);
  } else if (magic == c_magicIrix) {
#if TNZ_LITTLE_ENDIAN
    m_isIrixEndian = true;
#else
    m_isIrixEndian = false;
#endif
    m_iChan.setEndianness(true);
  } else {
    m_lastError = BAD_MAGIC;
    throw TImageException(filename, "Error on reading magic number");
  }

  m_iChan >> m_majorVersionNumber;
  m_iChan >> m_minorVersionNumber;

  // Loading pli versions AFTER current one is NOT SUPPORTED. This means that an
  // exception is directly called at this point.
  if (m_majorVersionNumber > c_majorVersionNumber ||
      (m_majorVersionNumber == c_majorVersionNumber &&
       m_minorVersionNumber > c_minorVersionNumber))
    throw TImageVersionException(filename, m_majorVersionNumber,
                                 m_minorVersionNumber);

  if (m_majorVersionNumber > 5 ||
      (m_majorVersionNumber == 5 && m_minorVersionNumber >= 8))
    m_iChan >> m_creator;

  if (m_majorVersionNumber < 5) {
    TUINT32 fileLength;

    m_iChan >> fileLength;
    m_iChan >> m_framesNumber;
    m_iChan >> maxThickness;
    m_thickRatio = maxThickness / 255.0;

    if (readInfo) return;

    CHECK_FOR_READ_ERROR(filename);

    m_currDynamicTypeBytesNum = 2;

    while ((tagElem = readTag())) {
      if (!m_firstTag)
        m_firstTag = m_lastTag = tagElem;
      else {
        m_lastTag->m_next = tagElem;
        m_lastTag         = m_lastTag->m_next;
      }
    }

    for (tagElem = m_firstTag; tagElem; tagElem = tagElem->m_next)
      tagElem->m_offset = 0;

    m_iChan.close();
  }
}

/*=====================================================================*/

extern TPalette *readPalette(GroupTag *paletteTag, int majorVersion,
                             int minorVersion);

/*=====================================================================*/

const TFrameId &ParsedPli::getFrameNumber(int index) {
  assert(imp->m_frameOffsInFile.size() == imp->m_framesNumber);

  std::map<TFrameId, int>::iterator it = imp->m_frameOffsInFile.begin();

  std::advance(it, index);

  return it->first;
}

/*=====================================================================*/

void ParsedPliImp::loadInfo(bool readPlt, TPalette *&palette,
                            TContentHistory *&history) {
  TUINT32 fileLength;

  m_iChan >> fileLength;
  m_iChan >> m_framesNumber;
  if (!((m_majorVersionNumber == 5 && m_minorVersionNumber >= 7) ||
        (m_majorVersionNumber > 5))) {
    UCHAR maxThickness;
    m_iChan >> maxThickness;
    m_thickRatio = maxThickness / 255.0;
  } else
    m_thickRatio = 0;

  UCHAR ii, d, s = 2;
  if (m_majorVersionNumber > 6 ||
      (m_majorVersionNumber == 6 && m_minorVersionNumber >= 5))
    m_iChan >> s;
  m_iChan >> ii;
  m_iChan >> d;
  m_autocloseTolerance = ((double)(s - 1)) * (ii + 0.01 * d);

  m_currDynamicTypeBytesNum = 2;

  // m_frameOffsInFile = new int[m_framesNumber];
  // for (int i=0; i<m_framesNumber; i++)
  //  m_frameOffsInFile[i] = -1;

  TUINT32 pos = m_iChan.tellg();
  USHORT type;
  while ((type = readTagHeader()) != PliTag::END_CNTRL) {
    if (type == PliTag::IMAGE_BEGIN_GOBJ) {
      USHORT frame;
      m_iChan >> frame;

      QByteArray suffix;
      if (m_majorVersionNumber >= 150) {
        TUINT32 suffixLength;
        m_iChan >> suffixLength;
        if ((int)suffixLength > 0) {
          suffix.resize(suffixLength);
          m_iChan.read(suffix.data(), suffixLength);
        }
      } else {
        char letter = 0;
        if (m_majorVersionNumber > 6 ||
            (m_majorVersionNumber == 6 && m_minorVersionNumber >= 6))
          m_iChan >> letter;
        if (letter > 0) suffix = QByteArray(&letter, 1);
      }

      m_frameOffsInFile[TFrameId(frame, QString::fromUtf8(suffix))] =
          m_iChan.tellg();

      // m_iChan.seekg(m_tagLength, ios::cur);
      if (m_majorVersionNumber < 150) m_iChan.seekg(m_tagLength - 2, ios::cur);
    } else if (type == PliTag::STYLE_NGOBJ) {
      m_iChan.seekg(pos, ios::beg);
      TagElem *tagElem = readTag();
      addTag(*tagElem);
      tagElem->m_tag = 0;
      delete tagElem;
    } else if (type == PliTag::TEXT) {
      m_iChan.seekg(pos, ios::beg);
      TagElem *tagElem = readTag();
      TextTag *textTag = (TextTag *)tagElem->m_tag;
      history          = new TContentHistory(true);
      history->deserialize(QString::fromStdString(textTag->m_text));
      delete tagElem;
    }

    else if (type == PliTag::GROUP_GOBJ && readPlt)  // la paletta!!!
    {
      m_iChan.seekg(pos, ios::beg);
      TagElem *tagElem   = readTag();
      GroupTag *grouptag = (GroupTag *)tagElem->m_tag;
      if (grouptag->m_type == (UCHAR)GroupTag::PALETTE) {
        readPlt = false;
        palette = readPalette((GroupTag *)tagElem->m_tag, m_majorVersionNumber,
                              m_minorVersionNumber);
      } else
        assert(grouptag->m_type == (UCHAR)GroupTag::STROKE);
      delete tagElem;
    } else {
      m_iChan.seekg(m_tagLength, ios::cur);
      switch (type) {
      case PliTag::SET_DATA_8_CNTRL:
        m_currDynamicTypeBytesNum = 1;
        break;
      case PliTag::SET_DATA_16_CNTRL:
        m_currDynamicTypeBytesNum = 2;
        break;
      case PliTag::SET_DATA_32_CNTRL:
        m_currDynamicTypeBytesNum = 4;
        break;
      default:
        break;
      }
    }
    pos = m_iChan.tellg();
  }

  assert(m_frameOffsInFile.size() == m_framesNumber);
  // palette = new TPalette();
  // for (int i=0; i<256; i++)
  //  palette->getPage(0)->addStyle(TPixel::Black);
}

/*=====================================================================*/

USHORT ParsedPliImp::readTagHeader() {
  UCHAR ucharTagType, tagLengthId;
  USHORT tagType;

// unused variable
#if 0
  TUINT32 tagOffset = m_iChan.tellg();
#endif

  m_iChan >> ucharTagType;

  if (ucharTagType == 0xFF) {
    m_iChan >> tagType;
    tagLengthId = tagType >> 14;
    tagType &= 0x3FFF;
  } else {
    tagType     = ucharTagType;
    tagLengthId = tagType >> 6;
    tagType &= 0x3F;
  }

  m_tagLength = 0;

  switch (tagLengthId) {
  case 0x0:
    m_tagLength = 0;
    break;
  case 0x1: {
    UCHAR clength;
    m_iChan >> clength;
    m_tagLength = clength;
    break;
  }
  case 0x2: {
    USHORT slength;
    m_iChan >> slength;
    m_tagLength = slength;
    break;
  }
  case 0x3:
    m_iChan >> m_tagLength;
    break;
  default:
    assert(false);
    break;
  }

  return tagType;
}

/*=====================================================================*/

ImageTag *ParsedPliImp::loadFrame(const TFrameId &frameNumber) {
  m_currDynamicTypeBytesNum = 2;

  TagElem *tagElem = m_firstTag;
  while (tagElem) {
    TagElem *auxTag = tagElem;
    tagElem         = tagElem->m_next;
    delete auxTag;
  }
  m_firstTag = 0;

  // PliTag *tag;
  USHORT type = PliTag::IMAGE_BEGIN_GOBJ;
  USHORT frame;
  QByteArray suffix;
  TFrameId frameId;

  // cerco il frame
  std::map<TFrameId, int>::iterator it;

  it = m_frameOffsInFile.find(frameNumber);

  if (it != m_frameOffsInFile.end()) {
    m_iChan.seekg(it->second, ios::beg);
    frameId = it->first;
  } else
    while ((type = readTagHeader()) != PliTag::END_CNTRL) {
      if (type == PliTag::IMAGE_BEGIN_GOBJ) {
        m_iChan >> frame;

        if (m_majorVersionNumber >= 150) {
          TUINT32 suffixLength;
          m_iChan >> suffixLength;
          suffix.resize(suffixLength);
          m_iChan.read(suffix.data(), suffixLength);
        } else {
          char letter = 0;
          if (m_majorVersionNumber > 6 ||
              (m_majorVersionNumber == 6 && m_minorVersionNumber >= 6)) {
            m_iChan >> letter;
            if (letter > 0) suffix = QByteArray(&letter, 1);
          }
        }

        frameId                    = TFrameId(frame, QString::fromUtf8(suffix));
        m_frameOffsInFile[frameId] = m_iChan.tellg();
        if (frameId == frameNumber) break;
      } else
        m_iChan.seekg(m_tagLength, ios::cur);
    }

  if (type == PliTag::END_CNTRL) {
    throw TImageException(TFilePath(), "Pli: frame not found");
    return 0;
  }

  // trovato; leggo i suoi tag
  while ((tagElem = readTag())) {
    if (!m_firstTag)
      m_firstTag = m_lastTag = tagElem;
    else {
      m_lastTag->m_next = tagElem;
      m_lastTag         = m_lastTag->m_next;
    }
    if (tagElem->m_tag->m_type == PliTag::IMAGE_GOBJ) {
      assert(((ImageTag *)(tagElem->m_tag))->m_numFrame == frameId);
      return (ImageTag *)tagElem->m_tag;
    }
  }

  return 0;
}

/*=====================================================================*/

TagElem *ParsedPliImp::readTag() {
  UCHAR ucharTagType, tagLengthId;
  USHORT tagType;

  TUINT32 tagOffset = m_iChan.tellg();

  m_iChan >> ucharTagType;

  if (ucharTagType == 0xFF) {
    m_iChan >> tagType;
    tagLengthId = tagType >> 14;
    tagType &= 0x3FFF;
  } else {
    tagType     = ucharTagType;
    tagLengthId = tagType >> 6;
    tagType &= 0x3F;
  }

  m_tagLength = 0;

  switch (tagLengthId) {
  case 0x0:
    m_tagLength = 0;
    break;
  case 0x1: {
    UCHAR clength;
    m_iChan >> clength;
    m_tagLength = clength;
    break;
  }
  case 0x2: {
    USHORT slength;
    m_iChan >> slength;
    m_tagLength = slength;
    break;
  }
  case 0x3:
    m_iChan >> m_tagLength;
    break;
  default:
    assert(false);
  }

  if (m_bufLength < m_tagLength) {
    m_bufLength = m_tagLength;
    m_buf.reset(new UCHAR[m_bufLength]);
  }

  if (m_tagLength) {
    m_iChan.read((char *)m_buf.get(), (int)m_tagLength);
    CHECK_FOR_READ_ERROR(m_filePath);
  }

  PliTag *newTag = NULL;

  switch (tagType) {
  case PliTag::SET_DATA_8_CNTRL:
    m_currDynamicTypeBytesNum = 1;
    break;
  case PliTag::SET_DATA_16_CNTRL:
    m_currDynamicTypeBytesNum = 2;
    break;
  case PliTag::SET_DATA_32_CNTRL:
    m_currDynamicTypeBytesNum = 4;
    break;
  case PliTag::TEXT:
    newTag = readTextTag();
    break;
  case PliTag::PALETTE:
    newTag = readPaletteTag();
    break;
  case PliTag::PALETTE_WITH_ALPHA:
    newTag = readPaletteWithAlphaTag();
    break;
  case PliTag::THICK_QUADRATIC_CHAIN_GOBJ:
  case PliTag::THICK_QUADRATIC_LOOP_GOBJ:
    newTag = readThickQuadraticChainTag(tagType ==
                                        PliTag::THICK_QUADRATIC_LOOP_GOBJ);
    break;
  case PliTag::GROUP_GOBJ:
    newTag = readGroupTag();
    break;
  case PliTag::IMAGE_GOBJ:
    newTag = readImageTag();
    break;
  case PliTag::COLOR_NGOBJ:
    newTag = readColorTag();
    break;
  case PliTag::STYLE_NGOBJ:
    newTag = readStyleTag();
    break;
  case PliTag::GEOMETRIC_TRANSFORMATION_GOBJ:
    newTag = readGeometricTransformationTag();
    break;
  case PliTag::DOUBLEPAIR_OBJ:
    newTag = readDoublePairTag();
    break;
  case PliTag::BITMAP_GOBJ:
    newTag = readBitmapTag();
    break;
  case PliTag::INTERSECTION_DATA_GOBJ:
    newTag = readIntersectionDataTag();
    break;
  case PliTag::OUTLINE_OPTIONS_GOBJ:
    newTag = readOutlineOptionsTag();
    break;
  case PliTag::PRECISION_SCALE_GOBJ:
    newTag = readPrecisionScaleTag();
    break;
  case PliTag::AUTOCLOSE_TOLERANCE_GOBJ:
    newTag = readAutoCloseToleranceTag();
    break;
  case PliTag::END_CNTRL:
    return 0;
  }

  if (newTag)
    return new TagElem(newTag, tagOffset);
  else
    return readTag();
}

/*=====================================================================*/

PliTag *ParsedPliImp::findTagFromOffset(UINT tagOffs) {
  for (TagElem *elem = m_firstTag; elem; elem = elem->m_next)
    if (elem->m_offset == tagOffs) return elem->m_tag;
  return NULL;
}
/*=====================================================================*/

UINT ParsedPliImp::findOffsetFromTag(PliTag *tag) {
  for (TagElem *elem = m_firstTag; elem; elem = elem->m_next)
    if (elem->m_tag == tag) return elem->m_offset;

  return 0;
}
/*=====================================================================*/

TagElem *ParsedPliImp::findTag(PliTag *tag) {
  for (TagElem *elem = m_firstTag; elem; elem = elem->m_next)
    if (elem->m_tag == tag) return elem;

  return NULL;
}
/*=====================================================================*/

inline void ParsedPliImp::readDynamicData(TUINT32 &val, TUINT32 &bufOffs) {
  switch (m_currDynamicTypeBytesNum) {
  case 1:
    val = m_buf[bufOffs++];
    break;
  case 2:
    if (m_isIrixEndian)
      val = m_buf[bufOffs + 1] | (m_buf[bufOffs] << 8);
    else
      val = m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8);
    bufOffs += 2;
    break;
  case 4:
    if (m_isIrixEndian)
      val = m_buf[bufOffs + 3] | (m_buf[bufOffs + 2] << 8) |
            (m_buf[bufOffs + 1] << 16) | (m_buf[bufOffs] << 24);
    else
      val = m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8) |
            (m_buf[bufOffs + 2] << 16) | (m_buf[bufOffs + 3] << 24);
    bufOffs += 4;
    break;
  default:
    assert(false);
  }
}

/*=====================================================================*/

inline bool ParsedPliImp::readDynamicData(TINT32 &val, TUINT32 &bufOffs) {
  bool isNegative = false;

  switch (m_currDynamicTypeBytesNum) {
  case 1:
    val = m_buf[bufOffs] & 0x7f;
    if (m_buf[bufOffs] & 0x80) {
      val        = -val;
      isNegative = true;
    }
    bufOffs++;
    break;
  case 2:
    if (m_isIrixEndian) {
      val = (m_buf[bufOffs + 1] | (m_buf[bufOffs] << 8)) & 0x7fff;
      if (m_buf[bufOffs] & 0x80) {
        val        = -val;
        isNegative = true;
      }
    } else {
      val = (m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8)) & 0x7fff;
      if (m_buf[bufOffs + 1] & 0x80) {
        val        = -val;
        isNegative = true;
      }
    }
    bufOffs += 2;
    break;
  case 4:
    if (m_isIrixEndian) {
      val = (m_buf[bufOffs + 3] | (m_buf[bufOffs + 2] << 8) |
             (m_buf[bufOffs + 1] << 16) | (m_buf[bufOffs] << 24)) &
            0x7fffffff;
      if (m_buf[bufOffs] & 0x80) {
        val        = -val;
        isNegative = true;
      }
    } else {
      val = (m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8) |
             (m_buf[bufOffs + 2] << 16) | (m_buf[bufOffs + 3] << 24)) &
            0x7fffffff;
      if (m_buf[bufOffs + 3] & 0x80) {
        val        = -val;
        isNegative = true;
      }
    }
    bufOffs += 4;
    break;
  default:
    assert(false);
  }
  return isNegative;
}

/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/

PliTag *ParsedPliImp::readTextTag() {
  if (m_tagLength == 0) return new TextTag("");

  return new TextTag(string((char *)m_buf.get(), m_tagLength));
}

/*=====================================================================*/

PliTag *ParsedPliImp::readPaletteTag() {
  TPixelRGBM32 *plt;
  TUINT32 numColors = 0;

  plt = new TPixelRGBM32[m_tagLength / 3];

  for (unsigned int i = 0; i < m_tagLength; i += 3, numColors++) {
    plt[numColors].r = m_buf[i];
    plt[numColors].g = m_buf[i + 1];
    plt[numColors].b = m_buf[i + 2];
  }

  PaletteTag *tag = new PaletteTag(numColors, plt);

  delete[] plt;

  return tag;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readPaletteWithAlphaTag() {
  TPixelRGBM32 *plt;
  TUINT32 numColors = 0;

  plt = new TPixelRGBM32[m_tagLength / 4];

  for (unsigned int i = 0; i < m_tagLength; i += 4, numColors++) {
    plt[numColors].r = m_buf[i];
    plt[numColors].g = m_buf[i + 1];
    plt[numColors].b = m_buf[i + 2];
    plt[numColors].m = m_buf[i + 3];
  }

  PaletteWithAlphaTag *tag = new PaletteWithAlphaTag(numColors, plt);

  delete[] plt;

  return tag;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readThickQuadraticChainTag(bool isLoop) {
  TThickPoint p;
  TUINT32 bufOffs = 0;
  double dx1, dy1, dx2, dy2;
  TINT32 d;
  TUINT32 numQuadratics = 0;
  double scale;

  bool newThicknessWriteMethod =
      ((m_majorVersionNumber == 5 && m_minorVersionNumber >= 7) ||
       (m_majorVersionNumber > 5));

  scale = 1.0 / (double)m_precisionScale;
  int maxThickness;
  if (newThicknessWriteMethod)

  {
    maxThickness = m_buf[bufOffs++];
    m_thickRatio = maxThickness / 255.0;
  } else {
    maxThickness = (int)m_maxThickness;
    assert(m_thickRatio != 0);
  }

  TINT32 val;
  readDynamicData(val, bufOffs);
  p.x = scale * val;
  readDynamicData(val, bufOffs);
  p.y = scale * val;

  p.thick = m_buf[bufOffs++] * m_thickRatio;
  if (newThicknessWriteMethod)
    numQuadratics = (m_tagLength - 2 * m_currDynamicTypeBytesNum - 1 - 1) /
                    (4 * m_currDynamicTypeBytesNum + 2);
  else
    numQuadratics = (m_tagLength - 2 * m_currDynamicTypeBytesNum - 1) /
                    (4 * m_currDynamicTypeBytesNum + 3);

  std::unique_ptr<TThickQuadratic[]> quadratic(
      new TThickQuadratic[numQuadratics]);

  for (unsigned int i = 0; i < numQuadratics; i++) {
    quadratic[i].setThickP0(p);

    readDynamicData(d, bufOffs);
    dx1 = scale * d;
    readDynamicData(d, bufOffs);
    dy1 = scale * d;

    if (newThicknessWriteMethod)
      p.thick = m_buf[bufOffs++] * m_thickRatio;
    else {
      if (m_isIrixEndian)
        p.thick =
            complement2((USHORT)(m_buf[bufOffs + 1] | (m_buf[bufOffs] << 8))) *
            m_thickRatio;
      else
        p.thick =
            complement2((USHORT)(m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8))) *
            m_thickRatio;
      bufOffs += 2;
    }

    readDynamicData(d, bufOffs);
    dx2 = scale * d;
    readDynamicData(d, bufOffs);
    dy2 = scale * d;

    if (dx1 == 0 && dy1 == 0)  // p0==p1, or p1==p2  creates problems (in the
                               // increasecontrolpoints for example) I slightly
                               // move it...
    {
      if (dx2 != 0 || dy2 != 0) {
        dx1 = 0.001 * dx2;
        dx2 = 0.999 * dx2;
        dy1 = 0.001 * dy2;
        dy2 = 0.999 * dy2;
        assert(dx1 != 0 || dy1 != 0);
      }
    } else if (dx2 == 0 && dy2 == 0) {
      if (dx1 != 0 || dy1 != 0) {
        dx2 = 0.001 * dx1;
        dx1 = 0.999 * dx1;
        dy2 = 0.001 * dy1;
        dy1 = 0.999 * dy1;
        assert(dx2 != 0 || dy2 != 0);
      }
    }

    p.x += dx1;
    p.y += dy1;

    quadratic[i].setThickP1(p);

    p.thick = m_buf[bufOffs++] * m_thickRatio;

    p.x += dx2;
    p.y += dy2;

    quadratic[i].setThickP2(p);
  }

  ThickQuadraticChainTag *tag = new ThickQuadraticChainTag();
  tag->m_numCurves            = numQuadratics;
  tag->m_curve                = std::move(quadratic);
  tag->m_isLoop               = isLoop;
  tag->m_maxThickness         = maxThickness;

  return tag;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readGroupTag() {
  TUINT32 bufOffs = 0;

  UCHAR type = m_buf[bufOffs++];

  assert(type < GroupTag::TYPE_HOW_MANY);

  TUINT32 numObjects = (m_tagLength - 1) / m_currDynamicTypeBytesNum;
  std::unique_ptr<PliObjectTag *[]> object(new PliObjectTag *[numObjects]);

  std::unique_ptr<TUINT32[]> tagOffs(new TUINT32[numObjects]);

  for (TUINT32 i = 0; i < numObjects; i++) {
    readDynamicData(tagOffs[i], bufOffs);
  }

  TagElem *elem;
  for (TUINT32 i = 0; i < numObjects; i++)
    while (!(object[i] = (PliObjectTag *)findTagFromOffset(tagOffs[i])))
      if ((elem = readTag()))
        addTag(*elem);
      else
        assert(false);

  std::unique_ptr<GroupTag> tag(new GroupTag());
  tag->m_type       = type;
  tag->m_numObjects = numObjects;
  tag->m_object     = std::move(object);

  return tag.release();
}

/*=====================================================================*/

PliTag *ParsedPliImp::readColorTag() {
  ColorTag::styleType style;
  ColorTag::attributeType attribute;
  TUINT32 bufOffs = 0;

  style     = (ColorTag::styleType)m_buf[bufOffs++];
  attribute = (ColorTag::attributeType)m_buf[bufOffs++];

  assert(style < ColorTag::STYLE_HOW_MANY);
  assert(attribute < ColorTag::ATTRIBUTE_HOW_MANY);

  TUINT32 numColors = (m_tagLength - 2) / m_currDynamicTypeBytesNum;
  std::unique_ptr<TUINT32[]> colorArray(new TUINT32[numColors]);

  for (unsigned int i = 0; i < numColors; i++) {
    TUINT32 color;

    readDynamicData(color, bufOffs);
    colorArray[i] = color;
  }

  std::unique_ptr<ColorTag> tag(
      new ColorTag(style, attribute, numColors, std::move(colorArray)));
  return tag.release();
}

/*=====================================================================*/

PliTag *ParsedPliImp::readStyleTag() {
  std::vector<TStyleParam> paramArray;
  TUINT32 bufOffs = 0;
  int length      = m_tagLength;
  UINT i;
  USHORT id        = 0;
  USHORT pageIndex = 0;

  UCHAR currDynamicTypeBytesNumSaved = m_currDynamicTypeBytesNum;
  m_currDynamicTypeBytesNum          = 2;

  readUShortData(id, bufOffs);
  length -= 2;
  if (m_majorVersionNumber > 5 ||
      (m_majorVersionNumber == 5 && m_minorVersionNumber >= 6)) {
    readUShortData(pageIndex, bufOffs);
    length -= 2;
  }
  while (length > 0) {
    TStyleParam param;

    param.m_type = (enum TStyleParam::Type)m_buf[bufOffs++];
    length--;
    switch (param.m_type) {
    case TStyleParam::SP_BYTE:
      param.m_numericVal = m_buf[bufOffs++];
      length--;
      break;
    case TStyleParam::SP_USHORT: {
      USHORT val;
      readUShortData(val, bufOffs);
      param.m_numericVal = val;
      length -= 2;
      break;
    }
    case TStyleParam::SP_INT:
    case TStyleParam::SP_DOUBLE:
      readFloatData(param.m_numericVal, bufOffs);
      length -= 4;
      break;
    case TStyleParam::SP_RASTER:
      length -= readRasterData(param.m_r, bufOffs);
      break;
    case TStyleParam::SP_STRING: {
      USHORT strLen;
      readUShortData(strLen, bufOffs);
      // bufOffs+=2;
      param.m_string = "";
      for (i = 0; i < strLen; i++) {
        param.m_string.append(1, m_buf[bufOffs++]);
      }
      length -= strLen + sizeof(USHORT);
      break;
    }
    default:
      assert(false);
    }
    paramArray.push_back(param);
  }

  int paramArraySize = paramArray.size();
  StyleTag *tag =
      new StyleTag(id, pageIndex, paramArraySize,
                   (paramArraySize > 0) ? paramArray.data() : nullptr);
  m_currDynamicTypeBytesNum = currDynamicTypeBytesNumSaved;

  return tag;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readOutlineOptionsTag() {
  TUINT32 bufOffs = 0;
  TINT32 d;

  const double scale = 0.001;

  // Read OutlineOptions
  int capStyle, joinStyle;
  double miterLower, miterUpper;

  capStyle  = m_buf[bufOffs++];
  joinStyle = m_buf[bufOffs++];

  readDynamicData(d, bufOffs);
  miterLower = scale * d;
  readDynamicData(d, bufOffs);
  miterUpper = scale * d;

  return new StrokeOutlineOptionsTag(
      TStroke::OutlineOptions(capStyle, joinStyle, miterLower, miterUpper));
}

/*=====================================================================*/

PliTag *ParsedPliImp::readPrecisionScaleTag() {
  TUINT32 bufOffs = 0;

  TINT32 d;
  readDynamicData(d, bufOffs);
  m_precisionScale = d;

  return new PrecisionScaleTag(m_precisionScale);
}

/*=====================================================================*/

PliTag *ParsedPliImp::readAutoCloseToleranceTag() {
  TUINT32 bufOffs = 0;

  TINT32 d;
  readDynamicData(d, bufOffs);

  return new AutoCloseToleranceTag(d);
}

/*=====================================================================*/

void ParsedPliImp::readFloatData(double &val, TUINT32 &bufOffs) {
  // UCHAR currDynamicTypeBytesNumSaved = m_currDynamicTypeBytesNum;
  // m_currDynamicTypeBytesNum = 2;
  TINT32 valInt;
  TUINT32 valDec;
  bool isNegative;

  isNegative = readDynamicData(valInt, bufOffs);
  readDynamicData(valDec, bufOffs);

  val = valInt + (double)valDec / 65536.0;  // 2^16

  if (valInt == 0 && isNegative) val = -val;

  // m_currDynamicTypeBytesNum = currDynamicTypeBytesNumSaved;
}

/*=====================================================================*/

UINT ParsedPliImp::readRasterData(TRaster32P &r, TUINT32 &bufOffs) {
  USHORT lx, ly;
  readUShortData(lx, bufOffs);
  readUShortData(ly, bufOffs);

  // readUShortData((USHORT&)lx, bufOffs);
  // readUShortData((USHORT&)ly, bufOffs);

  r.create((int)lx, (int)ly);
  UINT size = lx * ly * 4;
  r->lock();
  memcpy(r->getRawData(), m_buf.get() + bufOffs, size);
  r->unlock();
  bufOffs += size;
  return size + 2 + 2;
}

/*=====================================================================*/

inline void getLongValFromFloat(double val, TINT32 &intVal, TUINT32 &decVal) {
  intVal = (TINT32)val;
  if (val < 0) decVal = (TUINT32)((double)((-val) - (-intVal)) * 65536.0);
  /*if (intVal<(0x1<<7))
intVal|=(0x1<<7);
else if (intVal<(0x1<<15))
intVal|=(0x1<<15);
else
{
assert(intVal<(0x1<<31));
intVal|=(0x1<<31);
}*/
  else
    decVal = (TUINT32)((double)(val - intVal) * 65536.0);
}

/*=====================================================================*/

void ParsedPliImp::writeFloatData(double val) {
  UCHAR currDynamicTypeBytesNumSaved = m_currDynamicTypeBytesNum;
  m_currDynamicTypeBytesNum          = 2;
  TINT32 valInt;
  TUINT32 valDec;
  // bool neg=false;

  valInt = (int)val;

  if (val < 0)
    valDec = (int)((double)(-val + valInt) * 65536.0);
  else
    valDec = (int)((double)(val - valInt) * 65536.0);

  assert(valInt < (0x1 << 15));
  assert(valDec < (0x1 << 16));

  writeDynamicData(valInt, val < 0);
  writeDynamicData(valDec);
  m_currDynamicTypeBytesNum = currDynamicTypeBytesNumSaved;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readGeometricTransformationTag() {
  TUINT32 bufOffs = 0;
  TAffine affine;

  readFloatData(affine.a11, bufOffs);
  readFloatData(affine.a12, bufOffs);
  readFloatData(affine.a13, bufOffs);
  readFloatData(affine.a21, bufOffs);
  readFloatData(affine.a22, bufOffs);
  readFloatData(affine.a23, bufOffs);

  TUINT32 tagOffs;
  readDynamicData(tagOffs, bufOffs);

  TagElem *elem;
  PliObjectTag *object = NULL;
  if (tagOffs != 0)
    while (!(object = (PliObjectTag *)findTagFromOffset(tagOffs)))
      if ((elem = readTag()))
        addTag(*elem);
      else
        assert(false);
  else
    m_affine = affine;

  /*int realScale = tround(log10(1.0/m_affine.a11));

m_affine = TScale(1.0/pow(10.0, realScale));*/

  GeometricTransformationTag *tag =
      new GeometricTransformationTag(affine, (PliGeometricTag *)object);

  return tag;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readDoublePairTag() {
  TUINT32 bufOffs = 0;
  double first, second;

  readFloatData(first, bufOffs);
  readFloatData(second, bufOffs);

  DoublePairTag *tag = new DoublePairTag(first, second);

  return tag;
}

/*=====================================================================*/

void ParsedPliImp::readUShortData(USHORT &val, TUINT32 &bufOffs) {
  if (m_isIrixEndian)
    val = m_buf[bufOffs + 1] | (m_buf[bufOffs] << 8);
  else
    val = m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8);

  bufOffs += 2;
}

/*=====================================================================*/

void ParsedPliImp::readTUINT32Data(TUINT32 &val, TUINT32 &bufOffs) {
  if (m_isIrixEndian)
    val = m_buf[bufOffs + 3] | (m_buf[bufOffs + 2] << 8) |
          (m_buf[bufOffs + 1] << 16) | (m_buf[bufOffs] << 24);
  else
    val = m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8) |
          (m_buf[bufOffs + 2] << 16) | (m_buf[bufOffs + 3] << 24);

  bufOffs += 4;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readBitmapTag() {
  USHORT lx, ly;
  TUINT32 bufOffs = 0;

  readUShortData(lx, bufOffs);
  readUShortData(ly, bufOffs);

  TRaster32P r;

  r.create(lx, ly);
  r->lock();
  memcpy(r->getRawData(), m_buf.get() + bufOffs, lx * ly * 4);
  r->unlock();
  BitmapTag *tag = new BitmapTag(r);

  return tag;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readImageTag() {
  USHORT frame;
  TUINT32 bufOffs = 0;

  if (m_isIrixEndian)
    frame = m_buf[bufOffs + 1] | (m_buf[bufOffs] << 8);
  else
    frame = m_buf[bufOffs] | (m_buf[bufOffs + 1] << 8);

  bufOffs += 2;

  int headerLength = 2;

  QByteArray suffix;
  if (m_majorVersionNumber >= 150) {
    TUINT32 suffixLength;
    readTUINT32Data(suffixLength, bufOffs);
    headerLength += 4;
    if (suffixLength > 0) {
      suffix = QByteArray((char *)m_buf.get() + bufOffs, suffixLength);
      bufOffs += suffixLength;
      headerLength += suffixLength;
    }
  } else {
    char letter = 0;
    if (m_majorVersionNumber > 6 ||
        (m_majorVersionNumber == 6 && m_minorVersionNumber >= 6)) {
      letter = (char)m_buf[bufOffs++];
      ++headerLength;
      if (letter > 0) suffix = QByteArray(&letter, 1);
    }
  }

  TUINT32 numObjects = (m_tagLength - headerLength) / m_currDynamicTypeBytesNum;
  std::unique_ptr<PliObjectTag *[]> object(new PliObjectTag *[numObjects]);

  std::unique_ptr<TUINT32[]> tagOffs(new TUINT32[numObjects]);
  for (TUINT32 i = 0; i < numObjects; i++) {
    readDynamicData(tagOffs[i], bufOffs);
  }

  TagElem *elem;
  for (TUINT32 i = 0; i < numObjects; i++)
    while (!(object[i] = (PliObjectTag *)findTagFromOffset(tagOffs[i])))
      if ((elem = readTag()))
        addTag(*elem);
      else
        assert(false);

  std::unique_ptr<ImageTag[]> tag(
      new ImageTag(TFrameId(frame, QString::fromUtf8(suffix)), numObjects,
                   std::move(object)));
  return tag.release();
}

/*=====================================================================*/

const TSolidColorStyle ConstStyle(TPixel32::Red);

/*=====================================================================*/

inline double doubleFromUlong(TUINT32 q) {
  assert((q & 0X00000001) == 0);
  TUINT32 l[2];
#if TNZ_LITTLE_ENDIAN
  l[1] = 0x3FF00000 | (q >> 12);
  l[0] = (q & 0xFFE) << 20;
#else
  l[0] = 0x3FF00000 | (q >> 12);
  l[1] = (q & 0xFFE) << 20;
#endif

  return *(double *)l - 1;
}

/*=====================================================================*/

// vedi commento sulla write per chiarimenti!!

inline double truncate(double x) {
  x += 1.0;
  TUINT32 *l = (TUINT32 *)&x;

#if TNZ_LITTLE_ENDIAN
  l[0] &= 0xFFE00000;
#else
  l[1] &= 0xFFE00000;
#endif

  return x - 1.0;
}

/*=====================================================================*/

PliTag *ParsedPliImp::readIntersectionDataTag() {
  TUINT32 bufOffs = 0;
  TUINT32 branchCount;

  readTUINT32Data(branchCount, bufOffs);

  std::unique_ptr<IntersectionBranch[]> branchArray(
      new IntersectionBranch[branchCount]);

  UINT i;
  for (i = 0; i < branchCount; i++) {
    TINT32 currInter;
    readDynamicData((TINT32 &)branchArray[i].m_strokeIndex, bufOffs);
    readDynamicData(currInter, bufOffs);
    readDynamicData((TUINT32 &)branchArray[i].m_nextBranch, bufOffs);
    USHORT style;
    readUShortData(style, bufOffs);
    branchArray[i].m_style = style;
    /*
     */
    if (m_buf[bufOffs] & 0x80)  // in un numero double tra 0 e 1, il bit piu'
                                // significativo e' sempre 0
    // sfrutto questo bit; se e' 1, vuol dire che il valore e' 0.0 o 1.0 in un
    // singolo byte
    {
      branchArray[i].m_w = (m_buf[bufOffs] & 0x1) ? 1.0 : 0.0;
      bufOffs++;
    } else {
      TUINT32 hi, lo;
      hi = m_buf[bufOffs + 3] | (m_buf[bufOffs + 2] << 8) |
           (m_buf[bufOffs + 1] << 16) | (m_buf[bufOffs] << 24);
      bufOffs += 4;
      readTUINT32Data(lo, bufOffs);
      // readTUINT32Data(hi, bufOffs);

      branchArray[i].m_w = doubleFromUlong1(hi, lo);
    }

    if (currInter < 0) {
      branchArray[i].m_currInter  = -currInter - 1;
      branchArray[i].m_gettingOut = false;
    } else {
      branchArray[i].m_currInter  = currInter - 1;
      branchArray[i].m_gettingOut = true;
    }
  }

  IntersectionDataTag *tag = new IntersectionDataTag();
  tag->m_branchCount       = branchCount;
  tag->m_branchArray       = std::move(branchArray);

  return tag;
}

/*=====================================================================*/

bool ParsedPliImp::addTag(PliTag *tagPtr, bool addFront) {
  TagElem *_tag = new TagElem(tagPtr, 0);

  assert(tagPtr->m_type);

  if (!m_firstTag) {
    m_firstTag = m_lastTag = _tag;
  } else if (addFront) {
    _tag->m_next = m_firstTag;
    m_firstTag   = _tag;
  } else {
    m_lastTag->m_next = _tag;
    m_lastTag         = m_lastTag->m_next;
  }
  return true;
}

/*=====================================================================*/

bool ParsedPliImp::addTag(const TagElem &elem, bool addFront) {
  TagElem *_tag = new TagElem(elem);

  if (!m_firstTag) {
    m_firstTag = m_lastTag = _tag;
  } else if (addFront) {
    _tag->m_next = m_firstTag;
    m_firstTag   = _tag;
  } else {
    m_lastTag->m_next = _tag;
    m_lastTag         = m_lastTag->m_next;
  }
  return true;
}

/*=====================================================================*/

void ParsedPliImp::writeTag(TagElem *elem) {
  if (elem->m_offset != 0)  // already written
    return;

  switch (elem->m_tag->m_type) {
  case PliTag::TEXT:
    elem->m_offset = writeTextTag((TextTag *)elem->m_tag);
    break;
  case PliTag::PALETTE:
    elem->m_offset = writePaletteTag((PaletteTag *)elem->m_tag);
    break;
  case PliTag::PALETTE_WITH_ALPHA:
    elem->m_offset =
        writePaletteWithAlphaTag((PaletteWithAlphaTag *)elem->m_tag);
    break;
  case PliTag::THICK_QUADRATIC_CHAIN_GOBJ:
    elem->m_offset =
        writeThickQuadraticChainTag((ThickQuadraticChainTag *)elem->m_tag);
    break;
  case PliTag::GROUP_GOBJ:
    elem->m_offset = writeGroupTag((GroupTag *)elem->m_tag);
    break;
  case PliTag::IMAGE_GOBJ:
    elem->m_offset = writeImageTag((ImageTag *)elem->m_tag);
    break;
  case PliTag::COLOR_NGOBJ:
    elem->m_offset = writeColorTag((ColorTag *)elem->m_tag);
    break;
  case PliTag::STYLE_NGOBJ:
    elem->m_offset = writeStyleTag((StyleTag *)elem->m_tag);
    break;
  case PliTag::GEOMETRIC_TRANSFORMATION_GOBJ:
    elem->m_offset = writeGeometricTransformationTag(
        (GeometricTransformationTag *)elem->m_tag);
    break;
  case PliTag::DOUBLEPAIR_OBJ:
    elem->m_offset = writeDoublePairTag((DoublePairTag *)elem->m_tag);
    break;
  case PliTag::BITMAP_GOBJ:
    elem->m_offset = writeBitmapTag((BitmapTag *)elem->m_tag);
    break;
  case PliTag::INTERSECTION_DATA_GOBJ:
    elem->m_offset =
        writeIntersectionDataTag((IntersectionDataTag *)elem->m_tag);
    break;
  case PliTag::OUTLINE_OPTIONS_GOBJ:
    elem->m_offset =
        writeOutlineOptionsTag((StrokeOutlineOptionsTag *)elem->m_tag);
    break;
  case PliTag::PRECISION_SCALE_GOBJ:
    elem->m_offset = writePrecisionScaleTag((PrecisionScaleTag *)elem->m_tag);
    break;
  case PliTag::AUTOCLOSE_TOLERANCE_GOBJ:
    elem->m_offset =
        writeAutoCloseToleranceTag((AutoCloseToleranceTag *)elem->m_tag);
    break;
  default:
    assert(false);
    // m_error = UNKNOWN_TAG;
    ;
  }
}

/*=====================================================================*/

inline void ParsedPliImp::setDynamicTypeBytesNum(int minval, int maxval) {
  assert(m_oChan);
  if (maxval > 32767 || minval < -32767) {
    if (m_currDynamicTypeBytesNum != 4) {
      m_currDynamicTypeBytesNum = 4;
      *m_oChan << (UCHAR)PliTag::SET_DATA_32_CNTRL;
    }
  } else if (maxval > 127 || minval < -127) {
    if (m_currDynamicTypeBytesNum != 2) {
      m_currDynamicTypeBytesNum = 2;
      *m_oChan << (UCHAR)PliTag::SET_DATA_16_CNTRL;
    }
  } else if (m_currDynamicTypeBytesNum != 1) {
    m_currDynamicTypeBytesNum = 1;
    *m_oChan << (UCHAR)PliTag::SET_DATA_8_CNTRL;
  }
}

/*=====================================================================*/

inline void ParsedPliImp::writeDynamicData(TUINT32 val) {
  assert(m_oChan);
  switch (m_currDynamicTypeBytesNum) {
  case 1:
    *m_oChan << (UCHAR)val;
    break;
  case 2:
    *m_oChan << (USHORT)val;
    break;
  case 4:
    *m_oChan << (TUINT32)val;
    break;
  default:
    assert(false);
  }
}

/*=====================================================================*/

inline void ParsedPliImp::writeDynamicData(TINT32 val,
                                           bool isNegative = false) {
  assert(m_oChan);
  switch (m_currDynamicTypeBytesNum) {
  case 1:
    *m_oChan << complement1((char)val, isNegative);
    break;
  case 2:
    *m_oChan << complement1((short)val, isNegative);
    break;
  case 4:
    *m_oChan << complement1(val, isNegative);
    break;
  default:
    assert(false);
  }
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeTagHeader(UCHAR type, UINT tagLength) {
  assert(m_oChan);
  TUINT32 offset = m_oChan->tellp();

  assert((type & 0xc0) == 0x0);

  if (tagLength == 0)
    *m_oChan << type;
  else if (tagLength < 256) {
    *m_oChan << (UCHAR)(type | (0x1 << 6));
    *m_oChan << (UCHAR)tagLength;
  } else if (tagLength < 65535) {
    *m_oChan << (UCHAR)(type | (0x2 << 6));
    *m_oChan << (USHORT)tagLength;
  } else {
    *m_oChan << (UCHAR)(type | (0x3 << 6));
    *m_oChan << (TUINT32)tagLength;
  }

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeTextTag(TextTag *tag) {
  assert(m_oChan);
  int offset, tagLength = tag->m_text.length();

  offset = (int)writeTagHeader((UCHAR)PliTag::TEXT, tagLength);

  for (int i = 0; i < tagLength; i++) *m_oChan << tag->m_text[i];

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writePaletteTag(PaletteTag *tag) {
  assert(m_oChan);

  int offset, tagLength = (int)(tag->m_numColors * 3);

  offset = (int)writeTagHeader((UCHAR)PliTag::PALETTE, tagLength);

  for (unsigned int i = 0; i < tag->m_numColors; i++) {
    *m_oChan << tag->m_color[i].r;
    *m_oChan << tag->m_color[i].g;
    *m_oChan << tag->m_color[i].b;
  }

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writePaletteWithAlphaTag(PaletteWithAlphaTag *tag) {
  assert(m_oChan);

  int offset, tagLength = (int)(tag->m_numColors * 4);

  offset = (int)writeTagHeader((UCHAR)PliTag::PALETTE_WITH_ALPHA, tagLength);

  for (unsigned int i = 0; i < tag->m_numColors; i++) {
    *m_oChan << tag->m_color[i].r;
    *m_oChan << tag->m_color[i].g;
    *m_oChan << tag->m_color[i].b;
    *m_oChan << tag->m_color[i].m;
  }

  return offset;
}

/*=====================================================================*/

#define SET_MINMAX                                                             \
  if (p.x < minval) minval = (int)p.x;                                         \
  if (p.y < minval) minval = (int)p.y;                                         \
  if (p.x > maxval) maxval = (int)p.x;                                         \
  if (p.y > maxval) maxval = (int)p.y;

/*=====================================================================*/

inline void ParsedPliImp::WRITE_UCHAR_FROM_DOUBLE(double dval) {
  assert(m_oChan);
  int ival = tround(dval);
  if (ival > 255) ival = 255;
  assert(ival >= 0);
  *m_oChan << (UCHAR)ival;
}

/*=====================================================================*/

inline void ParsedPliImp::WRITE_SHORT_FROM_DOUBLE(double dval) {
  assert(m_oChan);
  int ival = (int)(dval);
  assert(ival >= -32768 && ival < 32768);
  *m_oChan << complement1((short)ival);
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeThickQuadraticChainTag(ThickQuadraticChainTag *tag) {
  assert(m_oChan);

  int maxval = -(std::numeric_limits<int>::max)(),
      minval = (std::numeric_limits<int>::max)();
  TPointD p;
  double scale;
  int i;

  assert(m_majorVersionNumber > 5 ||
         (m_majorVersionNumber == 5 && m_minorVersionNumber >= 5));

  scale = m_precisionScale;

  /*for ( i=0; i<tag->m_numCurves; i++)
tag->m_curve[i] =  aff*tag->m_curve[i];*/

  p = scale * tag->m_curve[0].getP0();
  SET_MINMAX

  for (i = 0; i < (int)tag->m_numCurves; i++) {
    p = scale * (tag->m_curve[i].getP1() - tag->m_curve[i].getP0());
    SET_MINMAX
    p = scale * (tag->m_curve[i].getP2() - tag->m_curve[i].getP1());
    SET_MINMAX
  }

  setDynamicTypeBytesNum(minval, maxval);

  int tagLength =
      (int)(2 * (2 * tag->m_numCurves + 1) * m_currDynamicTypeBytesNum + 1 + 1 +
            2 * tag->m_numCurves);
  int offset;
  if (tag->m_isLoop)
    offset = (int)writeTagHeader((UCHAR)PliTag::THICK_QUADRATIC_LOOP_GOBJ,
                                 tagLength);
  else
    offset = (int)writeTagHeader((UCHAR)PliTag::THICK_QUADRATIC_CHAIN_GOBJ,
                                 tagLength);

  // assert(scale*tag->m_curve[0].getThickP0().x ==
  // (double)(TINT32)(scale*tag->m_curve[0].getThickP0().x));
  // assert(scale*tag->m_curve[0].getThickP0().y ==
  // (double)(TINT32)(scale*tag->m_curve[0].getThickP0().y));
  double thickRatio = tag->m_maxThickness / 255.0;

  assert(tag->m_maxThickness <= 255);
  assert(tag->m_maxThickness > 0);
  UCHAR maxThickness = (UCHAR)(tceil(tag->m_maxThickness));

  *m_oChan << maxThickness;
  thickRatio = maxThickness / 255.0;
  writeDynamicData((TINT32)(scale * tag->m_curve[0].getThickP0().x));
  writeDynamicData((TINT32)(scale * tag->m_curve[0].getThickP0().y));
  double thick = tag->m_curve[0].getThickP0().thick / thickRatio;

  WRITE_UCHAR_FROM_DOUBLE(thick < 0 ? 0 : thick);

  for (i = 0; i < (int)tag->m_numCurves; i++) {
    TPoint dp =
        convert(scale * (tag->m_curve[i].getP1() - tag->m_curve[i].getP0()));

    assert(dp.x == (double)(TINT32)dp.x);
    assert(dp.y == (double)(TINT32)dp.y);

    writeDynamicData((TINT32)dp.x);
    writeDynamicData((TINT32)dp.y);
    thick = tag->m_curve[i].getThickP1().thick / thickRatio;

    WRITE_UCHAR_FROM_DOUBLE(thick < 0 ? 0 : thick);
    dp = convert(scale * (tag->m_curve[i].getP2() - tag->m_curve[i].getP1()));
    writeDynamicData((TINT32)dp.x);
    writeDynamicData((TINT32)dp.y);
    thick = tag->m_curve[i].getThickP2().thick / thickRatio;

    WRITE_UCHAR_FROM_DOUBLE(thick < 0 ? 0 : thick);
  }

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeGroupTag(GroupTag *tag) {
  assert(m_oChan);

  TUINT32 offset, tagLength;
  int maxval = 0, minval = 100000;
  std::vector<TUINT32> objectOffset(tag->m_numObjects);

  unsigned int i;

  for (i = 0; i < tag->m_numObjects; i++) {
    if (!(objectOffset[i] =
              findOffsetFromTag(tag->m_object[i])))  // the object was not
                                                     // already written before:
                                                     // write it now
    {
      TagElem elem(tag->m_object[i], 0);
      writeTag(&elem);
      objectOffset[i] = elem.m_offset;
      addTag(elem);
      elem.m_tag = 0;
    }

    if (objectOffset[i] < (unsigned int)minval) minval = (int)objectOffset[i];
    if (objectOffset[i] > (unsigned int)maxval) maxval = (int)objectOffset[i];
  }

  setDynamicTypeBytesNum(minval, maxval);

  tagLength = tag->m_numObjects * m_currDynamicTypeBytesNum + 1;

  offset = writeTagHeader((UCHAR)PliTag::GROUP_GOBJ, tagLength);

  *m_oChan << tag->m_type;

  for (i = 0; i < tag->m_numObjects; i++) writeDynamicData(objectOffset[i]);

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeImageTag(ImageTag *tag) {
  assert(m_oChan);

  TUINT32 *objectOffset, offset, tagLength;
  int maxval = 0, minval = 100000;

  QByteArray suffix    = tag->m_numFrame.getLetter().toUtf8();
  TUINT32 suffixLength = suffix.size();
  UINT fIdTagLength;
  if (m_majorVersionNumber >= 150) {  // write the suffix length before data
    fIdTagLength = 2 + 4 + suffixLength;
    writeTagHeader((UCHAR)PliTag::IMAGE_BEGIN_GOBJ, fIdTagLength);
    *m_oChan << (USHORT)tag->m_numFrame.getNumber();
    *m_oChan << suffixLength;
    if (suffixLength > 0) m_oChan->writeBuf(suffix.data(), suffixLength);
  } else {  // write only the first byte
    fIdTagLength = 3;
    writeTagHeader((UCHAR)PliTag::IMAGE_BEGIN_GOBJ, fIdTagLength);
    *m_oChan << (USHORT)tag->m_numFrame.getNumber();
    if (suffixLength > 0)
      m_oChan->writeBuf(suffix.data(), 1);
    else
      *m_oChan << (UCHAR)0;
  }
  m_currDynamicTypeBytesNum = 3;

  objectOffset = new TUINT32[tag->m_numObjects];
  unsigned int i;
  for (i = 0; i < tag->m_numObjects; i++) {
    if (!(objectOffset[i] =
              findOffsetFromTag(tag->m_object[i])))  // the object was not
                                                     // already written before:
                                                     // write it now
    {
      TagElem elem(tag->m_object[i], 0);
      writeTag(&elem);
      objectOffset[i] = elem.m_offset;
      addTag(elem);
      elem.m_tag = 0;
    }

    if (objectOffset[i] < (unsigned int)minval) minval = (int)objectOffset[i];
    if (objectOffset[i] > (unsigned int)maxval) maxval = (int)objectOffset[i];
  }

  setDynamicTypeBytesNum(minval, maxval);

  tagLength = tag->m_numObjects * m_currDynamicTypeBytesNum + fIdTagLength;
  // tagLength = tag->m_numObjects * m_currDynamicTypeBytesNum + 3;

  offset = writeTagHeader((UCHAR)PliTag::IMAGE_GOBJ, tagLength);

  suffix = tag->m_numFrame.getLetter().toUtf8();
  *m_oChan << (USHORT)tag->m_numFrame.getNumber();

  if (m_majorVersionNumber >= 150) {  // write the suffix length before data
    *m_oChan << suffixLength;
    if (suffixLength > 0) m_oChan->writeBuf(suffix.data(), suffixLength);
  } else {  // write only the first byte
    if (suffixLength > 0)
      m_oChan->writeBuf(suffix.data(), 1);
    else
      *m_oChan << (UCHAR)0;
  }

  for (i = 0; i < tag->m_numObjects; i++) writeDynamicData(objectOffset[i]);

  delete[] objectOffset;

  return offset;
}

/*=====================================================================*/
/*struct intersectionBranch
  {
  int m_strokeIndex;
  const TColorStyle* m_style;
  double m_w;
  UINT currInter;
  UINT m_nextBranch;
  bool m_gettingOut;
  };
*/

// per scrivere il valore m_w, molto spesso vale 0 oppure 1;
// se vale 0, scrivo un bye con valore 0x0;
// se vale 1, scrivo un bye con valore 0x1;
// altrimenti, 4 byte con val&0x3==0x2;
// e gli altri (32-2) bit contenenti iol valore di w.

inline TUINT32 ulongFromDouble(double x) {
  assert(x < 1.0);
  x += 1.0;
  TUINT32 *l = (TUINT32 *)&x;
#if TNZ_LITTLE_ENDIAN
  TUINT32 hi = l[1], lo = l[0];
#else
  TUINT32 hi = l[0], lo = l[1];
#endif

  return (hi & 0XFFFFF) << 12 | ((lo & 0xFFE00000) >> 20);
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeIntersectionDataTag(IntersectionDataTag *tag) {
  TUINT32 offset, tagLength;
  int maxval = -100000, minval = 100000;
  //  bool isNew = false;
  int floatWCount = 0;
  unsigned int i;

  assert(m_oChan);

  if (-(int)tag->m_branchCount - 1 < minval)
    minval = -(int)tag->m_branchCount - 1;
  if ((int)tag->m_branchCount + 1 > maxval)
    maxval = (int)tag->m_branchCount + 1;

  for (i = 0; i < tag->m_branchCount; i++) {
    if (tag->m_branchArray[i].m_w != 0 && tag->m_branchArray[i].m_w != 1)
      floatWCount++;
    if (tag->m_branchArray[i].m_strokeIndex < minval)
      minval = tag->m_branchArray[i].m_strokeIndex;
    else if (tag->m_branchArray[i].m_strokeIndex > maxval)
      maxval = tag->m_branchArray[i].m_strokeIndex;
  }
  setDynamicTypeBytesNum(minval, maxval);

  tagLength = 4 + tag->m_branchCount * (3 * m_currDynamicTypeBytesNum + 2) +
              floatWCount * 8 + (tag->m_branchCount - floatWCount) * 1;

  offset = writeTagHeader((UCHAR)PliTag::INTERSECTION_DATA_GOBJ, tagLength);

  *m_oChan << (TUINT32)tag->m_branchCount;

  for (i = 0; i < tag->m_branchCount; i++) {
    writeDynamicData((TINT32)tag->m_branchArray[i].m_strokeIndex);
    writeDynamicData((tag->m_branchArray[i].m_gettingOut)
                         ? (TINT32)(tag->m_branchArray[i].m_currInter + 1)
                         : -(TINT32)(tag->m_branchArray[i].m_currInter + 1));
    writeDynamicData((TUINT32)tag->m_branchArray[i].m_nextBranch);

    assert(tag->m_branchArray[i].m_style >= 0 &&
           tag->m_branchArray[i].m_style < 65536);
    *m_oChan << (USHORT)tag->m_branchArray[i].m_style;

    assert(tag->m_branchArray[i].m_w >= 0 && tag->m_branchArray[i].m_w <= 1);

    if (tag->m_branchArray[i].m_w == 0)
      *m_oChan << ((UCHAR)0x80);
    else if (tag->m_branchArray[i].m_w == 1)
      *m_oChan << ((UCHAR)0x81);
    else {
      TUINT32 hi, lo;
      ulongFromDouble1(tag->m_branchArray[i].m_w, hi, lo);
      assert((hi & 0x80000000) == 0);
      *m_oChan << (UCHAR)((hi >> 24) & 0xff);
      *m_oChan << (UCHAR)((hi >> 16) & 0xff);
      *m_oChan << (UCHAR)((hi >> 8) & 0xff);
      *m_oChan << (UCHAR)((hi)&0xff);
      // m_oChan<<((TUINT32)hi);
      *m_oChan << (TUINT32)(lo);
      // m_oChan<<((TUINT32)hi);
    }
  }

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeColorTag(ColorTag *tag) {
  assert(m_oChan);
  TUINT32 tagLength, offset;
  int maxval = 0, minval = 100000;
  unsigned int i;
  for (i = 0; i < tag->m_numColors; i++) {
    if (tag->m_color[i] < (unsigned int)minval) minval = (int)tag->m_color[i];
    if (tag->m_color[i] > (unsigned int)maxval) maxval = (int)tag->m_color[i];
  }

  setDynamicTypeBytesNum(minval, maxval);

  tagLength = tag->m_numColors * m_currDynamicTypeBytesNum + 2;

  offset = writeTagHeader((UCHAR)PliTag::COLOR_NGOBJ, tagLength);

  *m_oChan << (UCHAR)tag->m_style;
  *m_oChan << (UCHAR)tag->m_attribute;

  for (i = 0; i < tag->m_numColors; i++) writeDynamicData(tag->m_color[i]);

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeStyleTag(StyleTag *tag) {
  assert(m_oChan);
  TUINT32 tagLength = 0, offset;
  // int maxval=0, minval = 100000;
  int i;

  tagLength = 2 + 2;
  for (i = 0; i < tag->m_numParams; i++)
    tagLength += 1 + tag->m_param[i].getSize();

  offset = writeTagHeader((UCHAR)PliTag::STYLE_NGOBJ, tagLength);

  *m_oChan << tag->m_id;
  *m_oChan << tag->m_pageIndex;

  for (i = 0; i < tag->m_numParams; i++) {
    *m_oChan << (UCHAR)tag->m_param[i].m_type;
    switch (tag->m_param[i].m_type) {
    case TStyleParam::SP_BYTE:
      *m_oChan << (UCHAR)tag->m_param[i].m_numericVal;
      break;
    case TStyleParam::SP_USHORT:
      *m_oChan << (USHORT)tag->m_param[i].m_numericVal;
      break;
    case TStyleParam::SP_INT:
    case TStyleParam::SP_DOUBLE:
      writeFloatData((double)tag->m_param[i].m_numericVal);
      break;
    case TStyleParam::SP_RASTER:
      *m_oChan << tag->m_param[i].m_r;
      break;
    case TStyleParam::SP_STRING:
      *m_oChan << tag->m_param[i].m_string;
      break;
    default:
      assert(false);
      break;
    }
  }
  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeOutlineOptionsTag(StrokeOutlineOptionsTag *tag) {
  assert(m_oChan);

  const double scale = 1000.0;

  TINT32 miterLower = scale * tag->m_options.m_miterLower;
  TINT32 miterUpper = scale * tag->m_options.m_miterUpper;

  setDynamicTypeBytesNum(scale * miterLower, scale * miterUpper);

  int tagLength = 2 + 2 * m_currDynamicTypeBytesNum;
  int offset =
      (int)writeTagHeader((UCHAR)PliTag::OUTLINE_OPTIONS_GOBJ, tagLength);

  *m_oChan << (UCHAR)tag->m_options.m_capStyle;
  *m_oChan << (UCHAR)tag->m_options.m_joinStyle;
  writeDynamicData(miterLower);
  writeDynamicData(miterUpper);

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writePrecisionScaleTag(PrecisionScaleTag *tag) {
  assert(m_oChan);

  setDynamicTypeBytesNum(0, tag->m_precisionScale);

  int tagLength = m_currDynamicTypeBytesNum;
  int offset =
      (int)writeTagHeader((UCHAR)PliTag::PRECISION_SCALE_GOBJ, tagLength);

  writeDynamicData((TINT32)tag->m_precisionScale);
  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeAutoCloseToleranceTag(AutoCloseToleranceTag *tag) {
  assert(m_oChan);
  setDynamicTypeBytesNum(0, 10000);

  int tagLength = m_currDynamicTypeBytesNum;
  int offset =
      (int)writeTagHeader((UCHAR)PliTag::AUTOCLOSE_TOLERANCE_GOBJ, tagLength);

  writeDynamicData((TINT32)tag->m_autoCloseTolerance);
  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeGeometricTransformationTag(
    GeometricTransformationTag *tag) {
  assert(m_oChan);
  TUINT32 offset, tagLength;
  int maxval = 0, minval = 100000;
  TINT32 intVal[6];
  TUINT32 decVal[6];

  TUINT32 objectOffset = 0;

  if (tag->m_object) {
    if (!(objectOffset = findOffsetFromTag(tag->m_object)))  // the object was
                                                             // not already
                                                             // written before:
                                                             // write it now
    {
      TagElem elem(tag->m_object, 0);
      writeTag(&elem);
      objectOffset = elem.m_offset;
      addTag(elem);
      elem.m_tag = 0;
    }
  }

  if (objectOffset < (unsigned int)minval) minval = (int)objectOffset;
  if (objectOffset > (unsigned int)maxval) maxval = (int)objectOffset;

  getLongValFromFloat(tag->m_affine.a11, intVal[0], decVal[0]);
  if (intVal[0] < minval) minval = (int)intVal[0];
  if (intVal[0] > maxval) maxval = (int)intVal[0];
  if (decVal[0] > (unsigned int)maxval) maxval = (int)decVal[0];
  getLongValFromFloat(tag->m_affine.a12, intVal[1], decVal[1]);
  if (decVal[1] > (unsigned int)maxval) maxval = (int)decVal[1];
  if (intVal[1] < minval) minval = (int)intVal[1];
  if (intVal[1] > maxval) maxval = (int)intVal[1];
  getLongValFromFloat(tag->m_affine.a13, intVal[2], decVal[2]);
  if (decVal[2] > (unsigned int)maxval) maxval = (int)decVal[2];
  if (intVal[2] < minval) minval = (int)intVal[2];
  if (intVal[2] > maxval) maxval = (int)intVal[2];
  getLongValFromFloat(tag->m_affine.a21, intVal[3], decVal[3]);
  if (decVal[3] > (unsigned int)maxval) maxval = (int)decVal[3];
  if (intVal[3] < minval) minval = (int)intVal[3];
  if (intVal[3] > maxval) maxval = (int)intVal[3];
  getLongValFromFloat(tag->m_affine.a22, intVal[4], decVal[4]);
  if (decVal[4] > (unsigned int)maxval) maxval = (int)decVal[4];
  if (intVal[4] < minval) minval = (int)intVal[4];
  if (intVal[4] > maxval) maxval = (int)intVal[4];
  getLongValFromFloat(tag->m_affine.a23, intVal[5], decVal[5]);
  if (decVal[5] > (unsigned int)maxval) maxval = (int)decVal[5];
  if (intVal[5] < minval) minval = (int)intVal[5];
  if (intVal[5] > maxval) maxval = (int)intVal[5];

  setDynamicTypeBytesNum(minval, maxval);

  tagLength = (1 + 6 * 2) * m_currDynamicTypeBytesNum;

  offset =
      writeTagHeader((UCHAR)PliTag::GEOMETRIC_TRANSFORMATION_GOBJ, tagLength);

  writeDynamicData(intVal[0]);
  writeDynamicData(decVal[0]);
  writeDynamicData(intVal[1]);
  writeDynamicData(decVal[1]);
  writeDynamicData(intVal[2]);
  writeDynamicData(decVal[2]);
  writeDynamicData(intVal[3]);
  writeDynamicData(decVal[3]);
  writeDynamicData(intVal[4]);
  writeDynamicData(decVal[4]);
  writeDynamicData(intVal[5]);
  writeDynamicData(decVal[5]);

  writeDynamicData(objectOffset);

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeDoublePairTag(DoublePairTag *tag) {
  TUINT32 offset, tagLength;
  TINT32 minval = 100000, maxval = 0;
  TINT32 xIntVal, yIntVal;
  TUINT32 xDecVal, yDecVal;

  getLongValFromFloat(tag->m_first, xIntVal, xDecVal);
  getLongValFromFloat(tag->m_second, yIntVal, yDecVal);
  if (xIntVal < minval) minval = (int)xIntVal;
  if (xIntVal > maxval) maxval = (int)xIntVal;
  if ((int)xDecVal > maxval) maxval = (int)xDecVal;
  if (yIntVal < minval) minval = (int)yIntVal;
  if (yIntVal > maxval) maxval = (int)yIntVal;
  if ((int)yDecVal > maxval) maxval = (int)yDecVal;

  setDynamicTypeBytesNum(minval, maxval);

  tagLength = 4 * m_currDynamicTypeBytesNum;

  offset = writeTagHeader((UCHAR)PliTag::DOUBLEPAIR_OBJ, tagLength);

  writeDynamicData(xIntVal);
  writeDynamicData(xDecVal);
  writeDynamicData(yIntVal);
  writeDynamicData(yDecVal);

  return offset;
}

/*=====================================================================*/

TUINT32 ParsedPliImp::writeBitmapTag(BitmapTag *tag) {
  assert(m_oChan);

  TUINT32 offset, tagLength;
  TRaster32P r = tag->m_r;

  UINT bmpSize = r->getLx() * r->getLy() * r->getPixelSize();

  tagLength = 2 + 2 + bmpSize;

  offset = writeTagHeader((UCHAR)PliTag::BITMAP_GOBJ, tagLength);

  *m_oChan << (USHORT)r->getLx();
  *m_oChan << (USHORT)r->getLy();
  r->lock();
  m_oChan->writeBuf(r->getRawData(), bmpSize);
  r->unlock();
  return offset;
}

/*=====================================================================*/

bool ParsedPliImp::writePli(const TFilePath &filename) {
  MyOfstream os(filename);
  if (!os || os.fail()) return false;
  m_oChan = &os;

  *m_oChan << c_magicNt;
  // m_oChan << c_magicIrix;

  *m_oChan << m_majorVersionNumber;
  *m_oChan << m_minorVersionNumber;

  *m_oChan << m_creator;

  *m_oChan << (TUINT32)0;  // fileLength;
  *m_oChan << m_framesNumber;

  UCHAR s, i, d;
  double absAutoClose = fabs(m_autocloseTolerance);
  s                   = tsign(m_autocloseTolerance) + 1;
  i                   = (UCHAR)((int)absAutoClose);
  d                   = (UCHAR)((int)round((absAutoClose - i) * 100));
  *m_oChan << s;
  *m_oChan << i;
  *m_oChan << d;

  CHECK_FOR_WRITE_ERROR(filename);

  m_currDynamicTypeBytesNum = 2;

  for (TagElem *elem = m_firstTag; elem; elem = elem->m_next) {
    writeTag(elem);
    CHECK_FOR_WRITE_ERROR(filename);
  }

  *m_oChan << (UCHAR)PliTag::END_CNTRL;

  m_oChan->close();
  m_oChan = 0;

  return true;
}

/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/

ParsedPli::ParsedPli() { imp = new ParsedPliImp(); }

/*=====================================================================*/

ParsedPli::ParsedPli(USHORT framesNumber, UCHAR precision, UCHAR maxThickness,
                     double autocloseTolerance) {
  imp =
      new ParsedPliImp(c_majorVersionNumber, c_minorVersionNumber, framesNumber,
                       precision, maxThickness, autocloseTolerance);
}

/*=====================================================================*/

ParsedPli::~ParsedPli() { delete imp; }

/*=====================================================================*/

bool ParsedPli::addTag(PliTag *tag, bool addFront) {
  return imp->addTag(tag, addFront);
}

PliTag *ParsedPli::getFirstTag() {
  imp->m_currTag = imp->m_firstTag;
  return imp->m_currTag->m_tag;
}

/*=====================================================================*/

PliTag *ParsedPli::getNextTag() {
  assert(imp->m_currTag);

  imp->m_currTag = imp->m_currTag->m_next;

  return (imp->m_currTag) ? imp->m_currTag->m_tag : NULL;
}

/*=====================================================================*/

void ParsedPli::setCreator(const QString &creator) {
  imp->m_creator = creator.toStdString();
}

/*=====================================================================*/

QString ParsedPli::getCreator() const {
  return QString::fromStdString(imp->m_creator);
}

/*=====================================================================*/

ParsedPli::ParsedPli(const TFilePath &filename, bool readInfo) {
  imp = new ParsedPliImp(filename, readInfo);
}

/*=====================================================================*/

void ParsedPli::getVersion(UINT &majorVersionNumber,
                           UINT &minorVersionNumber) const {
  majorVersionNumber = imp->m_majorVersionNumber;
  minorVersionNumber = imp->m_minorVersionNumber;
}

/*=====================================================================*/

void ParsedPli::setVersion(UINT majorVersionNumber, UINT minorVersionNumber) {
  if (imp->m_versionLocked) {
    // accept only when settings higher versions
    if ((imp->m_majorVersionNumber > majorVersionNumber) ||
        (imp->m_majorVersionNumber == majorVersionNumber &&
         imp->m_minorVersionNumber >= minorVersionNumber))
      return;
  }
  if (majorVersionNumber >= 120) imp->m_versionLocked = true;
  imp->m_majorVersionNumber = majorVersionNumber;
  imp->m_minorVersionNumber = minorVersionNumber;
}

/*=====================================================================*/

bool ParsedPli::writePli(const TFilePath &filename) {
  return imp->writePli(filename);
}

/*=====================================================================*/

/*
  Necessario per fissare un problema di lettura con le vecchie
  versioni di PLI ( < 3 ).
 */
double ParsedPli::getThickRatio() const { return imp->m_thickRatio; }

/*=====================================================================*/

ParsedPliImp::~ParsedPliImp() {
  TagElem *tag = m_firstTag;
  while (tag) {
    TagElem *auxTag = tag;
    tag             = tag->m_next;
    delete auxTag;
  }
}

/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/
/*=====================================================================*/

void ParsedPli::loadInfo(bool readPalette, TPalette *&palette,
                         TContentHistory *&history) {
  imp->loadInfo(readPalette, palette, history);
}

/*=====================================================================*/

ImageTag *ParsedPli::loadFrame(const TFrameId &frameId) {
  return imp->loadFrame(frameId);
}

/*=====================================================================*/

void ParsedPli::setFrameCount(int frameCount) {
  imp->setFrameCount(frameCount);
}

/*=====================================================================*/

void ParsedPliImp::setFrameCount(int frameCount) {
  m_framesNumber = frameCount;
}

/*=====================================================================*/

int ParsedPli::getFrameCount() const { return imp->getFrameCount(); }

/*=====================================================================*/

int ParsedPliImp::getFrameCount() { return m_framesNumber; }

/*=====================================================================*/

double ParsedPli::getAutocloseTolerance() const {
  return imp->m_autocloseTolerance;
}

/*=====================================================================*/

/*=====================================================================*/

void ParsedPli::setAutocloseTolerance(int tolerance) {
  imp->m_autocloseTolerance = tolerance;
}

/*=====================================================================*/

int &ParsedPli::precisionScale() { return imp->m_precisionScale; }
