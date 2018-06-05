

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#pragma warning(disable : 4996)
#endif

//#include "texception.h"
//#include "tfilepath.h"
#include "tiio_tga.h"
#include "tpixel.h"

//#include <iostream.h>

inline unsigned short fgetUSHORT(FILE *chan) {
  int b0 = fgetc(chan);
  int b1 = fgetc(chan);
  return b1 << 8 | b0;
}

/*
inline unsigned short fgetULONG(FILE *chan)
{
  int a = fgetc(chan);
  int b = fgetc(chan);
  int c = fgetc(chan);
  int d = fgetc(chan);
  return a<<24|b<<16|c<<8|d;
}
*/

inline void fputUSHORT(unsigned short x, FILE *chan) {
  int b1 = x >> 8;
  int b0 = x & 0xFF;
  fputc(b0, chan);
  fputc(b1, chan);
}

/*
inline void fputULONG(unsigned long x, FILE *chan)
{
  int a = x>>24;
  int b = (x>>16)&0xff;
  int c = (x>>8)&0xff;
  int d = x&0xFF;
  fputc(a, chan);
  fputc(b, chan);
  fputc(c, chan);
  fputc(d, chan);
}
*/

typedef unsigned char UCHAR;
typedef unsigned short USHORT;

struct TgaHeader {
  UCHAR IdentificationField;
  UCHAR ColorMapType;
  UCHAR ImageTypeCode;
  USHORT ColorMapOrigin;
  USHORT ColorMapLength;
  UCHAR ColorMapSize;
  USHORT Ximage;
  USHORT Yimage;
  USHORT Width;
  USHORT Height;
  UCHAR ImagePixelSize;
  UCHAR ImageDescriptor;
};

//============================================================

static void readTgaHeader(TgaHeader &header, FILE *chan) {
  header.IdentificationField = fgetc(chan);
  header.ColorMapType        = fgetc(chan);
  header.ImageTypeCode       = fgetc(chan);
  header.ColorMapOrigin      = fgetUSHORT(chan);
  header.ColorMapLength      = fgetUSHORT(chan);
  header.ColorMapSize        = fgetc(chan);
  header.Ximage              = fgetUSHORT(chan);
  header.Yimage              = fgetUSHORT(chan);
  header.Width               = fgetUSHORT(chan);
  header.Height              = fgetUSHORT(chan);
  header.ImagePixelSize      = fgetc(chan);
  header.ImageDescriptor     = fgetc(chan);
}

//------------------------------------------------------------

static void writeTgaHeader(TgaHeader &header, FILE *chan) {
  fputc(header.IdentificationField, chan);
  fputc(header.ColorMapType, chan);
  fputc(header.ImageTypeCode, chan);
  fputUSHORT(header.ColorMapOrigin, chan);
  fputUSHORT(header.ColorMapLength, chan);
  fputc(header.ColorMapSize, chan);
  fputUSHORT(header.Ximage, chan);
  fputUSHORT(header.Yimage, chan);
  fputUSHORT(header.Width, chan);
  fputUSHORT(header.Height, chan);
  fputc(header.ImagePixelSize, chan);
  fputc(header.ImageDescriptor, chan);
}

//============================================================

class TgaReader final : public Tiio::Reader {
  FILE *m_chan;
  TgaHeader m_header;
  TPixel32 *m_palette;

  typedef void (TgaReader::*ReadLineProc)(char *buffer, int x0, int x1,
                                          int shrink);
  ReadLineProc m_readLineProc;

  typedef int (TgaReader::*SkipLineProc)(int count);
  SkipLineProc m_skipLineProc;

public:
  TgaReader()
      : m_chan(0)
      , m_readLineProc(&TgaReader::readNoLine)
      , m_skipLineProc(&TgaReader::skipLines0)
      , m_palette(0) {}

  ~TgaReader();

  void open(FILE *file) override;

  int skipLines0(int count) {
    int lineSize = m_info.m_lx;
    fseek(m_chan, count * lineSize, SEEK_CUR);
    return count;
  }
  int skipNoLines(int count) { return 0; };
  void skipBytes(int count) {
    // fseek(m_chan, count, SEEK_CUR); //inefficiente se count è piccolo
    for (int i = 0; i < count; i++) getc(m_chan);
  }

  int skipLines32(int count) {
    skipLines0(4 * count);
    return count;
  }
  int skipLines24(int count) {
    skipLines0(3 * count);
    return count;
  }
  int skipLines16(int count) {
    skipLines0(2 * count);
    return count;
  }
  inline TPixel32 fgetPixel32(FILE *chan) {
    TPixel32 pix;
    pix.b = fgetc(chan);
    pix.g = fgetc(chan);
    pix.r = fgetc(chan);
    pix.m = fgetc(chan);
    return pix;
  }
  inline TPixel32 fgetPixel24(FILE *chan) {
    TPixel32 pix;
    pix.b = fgetc(chan);
    pix.g = fgetc(chan);
    pix.r = fgetc(chan);
    pix.m = 255;
    return pix;
  }
  inline TPixel32 fgetPixel16(FILE *chan) {
    TPixel32 pix;
    unsigned short s = fgetUSHORT(chan);
    int b            = s & 0x1f;
    int g            = (s >> 5) & 0x1f;
    int r            = (s >> 10) & 0x1f;
    pix.r            = r << 3 | r >> 2;
    pix.g            = g << 3 | g >> 2;
    pix.b            = b << 3 | b >> 2;
    pix.m            = 255;
    return pix;
  }

  Tiio::RowOrder getRowOrder() const override {
    return ((m_header.ImageDescriptor >> 5) & 1) == 0 ? Tiio::BOTTOM2TOP
                                                      : Tiio::TOP2BOTTOM;
  }

  void readLine(char *buffer, int x0, int x1, int shrink) override {
    (this->*m_readLineProc)(buffer, x0, x1, shrink);
  }
  int skipLines(int count) override { return (this->*m_skipLineProc)(count); }
  void readPalette();

  void readNoLine(char *buffer, int x0, int x1, int shrink) {}

  void readLineRGB32(char *buffer, int x0, int x1, int shrink);
  void readLineRGB32rle(char *buffer, int x0, int x1, int shrink);
  void readLineRGB24(char *buffer, int x0, int x1, int shrink);
  void readLineRGB24rle(char *buffer, int x0, int x1, int shrink);
  void readLineRGB16(char *buffer, int x0, int x1, int shrink);
  void readLineRGB16rle(char *buffer, int x0, int x1, int shrink);
  void readLineGR8(char *buffer, int x0, int x1, int shrink);
  void readLineGR8rle(char *buffer, int x0, int x1, int shrink);

  void readLineCmapped(char *buffer, int x0, int x1, int shrink);
  void readLineCmappedRle(char *buffer, int x0, int x1, int shrink);
};

//------------------------------------------------------------

TgaReader::~TgaReader() { delete m_palette; }

//------------------------------------------------------------

void TgaReader::open(FILE *file) {
  {
    try {
      m_chan = file;
    } catch (...) {
      perror("uffa");
      return;
    }
    readTgaHeader(m_header, m_chan);
    m_info.m_lx = m_header.Width;
    m_info.m_ly = m_header.Height;

    if (m_header.IdentificationField > 0) {
      char tmp[256];
      fread(tmp, 1, m_header.IdentificationField, m_chan);
    }
    if (m_header.ColorMapType > 0) {
      readPalette();
    }
    Tiio::TgaWriterProperties *prop = new Tiio::TgaWriterProperties();
    m_info.m_properties             = prop;
    prop->m_compressed.setValue(false);
    switch (m_header.ImageTypeCode) {
    case 0:
      // cout << "NO IMAGE" << endl;
      break;
    case 1:
      // cout << "Uncompressed Cmapped; pixelsize=" << m_header.ImagePixelSize
      // << endl;
      m_readLineProc          = &TgaReader::readLineCmapped;
      m_info.m_samplePerPixel = 4;
      break;
    case 2:
      // cout << "Uncompressed RGB; pixelsize=" << m_header.ImagePixelSize <<
      // endl;
      m_info.m_samplePerPixel = 4;
      if (m_header.ImagePixelSize == 32) {
        m_readLineProc = &TgaReader::readLineRGB32;
        m_skipLineProc = &TgaReader::skipLines32;
      } else if (m_header.ImagePixelSize == 24) {
        m_info.m_samplePerPixel = 3;
        m_readLineProc          = &TgaReader::readLineRGB24;
        m_skipLineProc          = &TgaReader::skipLines24;
      } else if (m_header.ImagePixelSize == 16) {
        m_readLineProc = &TgaReader::readLineRGB16;
        m_skipLineProc = &TgaReader::skipLines16;
      }
      break;
    case 3:
      m_info.m_samplePerPixel = 1;
      m_readLineProc          = &TgaReader::readLineGR8;
      break;
    case 9:
      // cout << "Compressed Cmapped; pixelsize=" << m_header.ImagePixelSize <<
      // endl;
      prop->m_compressed.setValue(true);
      m_readLineProc          = &TgaReader::readLineCmappedRle;
      m_info.m_samplePerPixel = 4;
      break;
    case 10:
      // cout << "Compressed RGB; pixelsize=" << m_header.ImagePixelSize <<
      // endl;
      prop->m_compressed.setValue(true);
      m_info.m_samplePerPixel = 4;
      if (m_header.ImagePixelSize == 32) {
        m_readLineProc = &TgaReader::readLineRGB32rle;
        m_skipLineProc = &TgaReader::skipNoLines;
      } else if (m_header.ImagePixelSize == 24) {
        m_info.m_samplePerPixel = 3;
        m_readLineProc          = &TgaReader::readLineRGB24rle;
        m_skipLineProc          = &TgaReader::skipNoLines;
      } else if (m_header.ImagePixelSize == 16) {
        m_readLineProc = &TgaReader::readLineRGB16rle;
        m_skipLineProc = &TgaReader::skipNoLines;
      }
      break;
    case 11:
      m_info.m_samplePerPixel = 1;
      m_readLineProc          = &TgaReader::readLineGR8rle;
      break;
    default:;
      // cout << "***Unknown***" << endl;
    }
  }
}

//------------------------------------------------------------

void TgaReader::readPalette() {
  int colorCount  = m_header.ColorMapLength;
  int bitPerColor = m_header.ColorMapSize;

  // cout << "Reading palette: " << colorCount << " " << bitPerColor <<
  // "-entries" << endl;

  m_palette = new TPixel32[colorCount];
  int i;
  switch (bitPerColor) {
  case 16:
    for (i = 0; i < colorCount; i++) m_palette[i] = fgetPixel16(m_chan);
    break;
  case 24:
    for (i = 0; i < colorCount; i++) m_palette[i] = fgetPixel24(m_chan);
    break;
  case 32:
    for (i = 0; i < colorCount; i++) m_palette[i] = fgetPixel32(m_chan);
    break;
    break;
  }
}

//------------------------------------------------------------

void TgaReader::readLineRGB32(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  pix += x0;
  if (x0 > 0) skipBytes(4 * x0);
  TPixel32 *endPix = pix + x1 - x0 + 1;

  while (pix < endPix) {
    *pix++ = fgetPixel32(m_chan);
    if (pix < endPix && shrink > 1) {
      pix += (shrink - 1);
      skipBytes(4 * (shrink - 1));
    }
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(4 * (m_info.m_lx - x1 - 1));
}

//------------------------------------------------------------

void TgaReader::readLineRGB24(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  pix += x0;
  if (x0 > 0) skipBytes(3 * x0);
  TPixel32 *endPix = pix + x1 - x0 + 1;
  while (pix < endPix) {
    *pix++ = fgetPixel24(m_chan);
    if (pix < endPix && shrink > 1) {
      pix += (shrink - 1);
      skipBytes(3 * (shrink - 1));
    }
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(3 * (m_info.m_lx - x1 - 1));
}

//------------------------------------------------------------

void TgaReader::readLineRGB16(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  pix += x0;
  if (x0 > 0) skipBytes(2 * x0);
  TPixel32 *endPix = pix + x1 - x0 + 1;
  while (pix < endPix) {
    *pix++ = fgetPixel16(m_chan);
    if (pix < endPix && shrink > 1) {
      pix += (shrink - 1);
      skipBytes(2 * (shrink - 1));
    }
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(2 * (m_info.m_lx - x1 - 1));
}

//------------------------------------------------------------

void TgaReader::readLineCmapped(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  pix += x0;
  if (x0 > 0) skipBytes(x0);
  TPixel32 *endPix = pix + x1 - x0 + 1;
  while (pix < endPix) {
    int index = fgetc(m_chan);
    *pix++    = m_palette[index];
    if (pix < endPix && shrink > 1) {
      pix += (shrink - 1);
      skipBytes(shrink - 1);
    }
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(m_info.m_lx - x1 - 1);
}

//------------------------------------------------------------

void TgaReader::readLineRGB32rle(char *buffer, int x0, int x1, int shrink) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    UCHAR h   = fgetc(m_chan);
    int count = (h & 0x7F) + 1;
    if (h & 0x80) {
      TPixel32 pix = fgetPixel32(m_chan);
      for (int i = 0; i < count && x < m_info.m_lx; i++) row[x++] = pix;
    } else
      for (int i = 0; i < count && x < m_info.m_lx; i++)
        row[x++] = fgetPixel32(m_chan);
  }
}

//------------------------------------------------------------

void TgaReader::readLineRGB24rle(char *buffer, int x0, int x1, int shrink) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    UCHAR h   = fgetc(m_chan);
    int count = (h & 0x7F) + 1;
    if (h & 0x80) {
      TPixel32 pix = fgetPixel24(m_chan);
      for (int i = 0; i < count && x < m_info.m_lx; i++) row[x++] = pix;
    } else
      for (int i = 0; i < count && x < m_info.m_lx; i++)
        row[x++] = fgetPixel24(m_chan);
  }
}

//------------------------------------------------------------

void TgaReader::readLineRGB16rle(char *buffer, int x0, int x1, int shrink) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    UCHAR h   = fgetc(m_chan);
    int count = (h & 0x7F) + 1;
    if (h & 0x80) {
      TPixel32 pix = fgetPixel16(m_chan);
      for (int i = 0; i < count && x < m_info.m_lx; i++) row[x++] = pix;
    } else
      for (int i = 0; i < count && x < m_info.m_lx; i++)
        row[x++] = fgetPixel16(m_chan);
  }
}

//------------------------------------------------------------

void TgaReader::readLineGR8(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *row = (TPixel32 *)buffer;
  int w         = x1 - x0 + 1;
  while (w--) {
    row->r = row->g = row->b = fgetc(m_chan);
    row->m                   = 0xff;
    ++row;
  }
}

//------------------------------------------------------------

void TgaReader::readLineGR8rle(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *row = (TPixel32 *)buffer;
  UCHAR count, byte;
  int nx, x, i;
  int width = x1 - x0 + 1;
  for (nx = 0; nx < width;) {
    count = fgetc(m_chan);
    if ((count >> 7) & 0x01) {
      count -= 127;
      byte = fgetc(m_chan);
      for (x = 0; x < count; x++) {
        row->r = row->g = row->b = byte;
        row->m                   = 0xff;
        ++row;
        nx++;
      }
    } else {
      for (i = 0; i < count + 1; i++) {
        byte   = fgetc(m_chan);
        row->r = row->g = row->b = byte;
        row->m                   = 0xff;
        ++row;
        nx++;
      }
    }
  }
  if (nx != width) fprintf(stderr, "tga read error: decode failed\n");
}

//------------------------------------------------------------

void TgaReader::readLineCmappedRle(char *buffer, int x0, int x1, int shrink) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    UCHAR h   = fgetc(m_chan);
    int count = (h & 0x7F) + 1;
    if (h & 0x80) {
      int index    = fgetc(m_chan);
      TPixel32 pix = m_palette[index];
      for (int i = 0; i < count && x < m_info.m_lx; i++) row[x++] = pix;
    } else {
      for (int i = 0; i < count && x < m_info.m_lx; i++) {
        int index = fgetc(m_chan);
        row[x++]  = m_palette[index];
      }
    }
  }
}

//============================================================

Tiio::TgaWriterProperties::TgaWriterProperties()
    : m_pixelSize("Bits Per Pixel"), m_compressed("Compression", true) {
  m_pixelSize.addValue(L"16 bits");
  m_pixelSize.addValue(L"24 bits");
  m_pixelSize.addValue(L"32 bits");
  m_pixelSize.setValue(L"24 bits");
  bind(m_pixelSize);
  bind(m_compressed);
}

void Tiio::TgaWriterProperties::updateTranslation() {
  m_pixelSize.setQStringName(tr("Bits Per Pixel"));
  m_pixelSize.setItemUIName(L"16 bits", tr("16 bits"));
  m_pixelSize.setItemUIName(L"24 bits", tr("24 bits"));
  m_pixelSize.setItemUIName(L"32 bits", tr("32 bits"));
  m_compressed.setQStringName(tr("Compression"));
}

//============================================================

class TgaWriter final : public Tiio::Writer {
  FILE *m_chan;
  TgaHeader m_header;

  typedef void (TgaWriter::*WriteLineProc)(char *buffer);
  WriteLineProc m_writeLineProc;

public:
  TgaWriter() : m_chan(0), m_writeLineProc(&TgaWriter::writeNoLine) {}

  void open(FILE *file, const TImageInfo &info) override {
    m_info = info;
    try {
      m_chan = file;
    } catch (...) {
      perror("uffa");
      return;
    }
    if (!m_properties) m_properties = new Tiio::TgaWriterProperties();

    bool compressed =
        ((TBoolProperty *)(m_properties->getProperty("Compression")))
            ->getValue();
    memset(&m_header, 0, sizeof(m_header));
    m_header.ImageTypeCode = compressed ? 10 : 2;
    m_header.Width         = m_info.m_lx;
    m_header.Height        = m_info.m_ly;

    std::wstring pixelSizeW =
        ((TEnumProperty *)(m_properties->getProperty("Bits Per Pixel")))
            ->getValue()
            .substr(0, 2);
    if (pixelSizeW == L"16") {
      m_header.ImagePixelSize = 16;
      m_writeLineProc =
          compressed ? &TgaWriter::writeLine16rle : &TgaWriter::writeLine16;
    } else if (pixelSizeW == L"24") {
      m_header.ImagePixelSize = 24;
      m_writeLineProc =
          compressed ? &TgaWriter::writeLine24rle : &TgaWriter::writeLine24;
    } else {
      m_header.ImagePixelSize = 32;
      m_writeLineProc =
          compressed ? &TgaWriter::writeLine32rle : &TgaWriter::writeLine32;
    }
    writeTgaHeader(m_header, m_chan);
  }

  void flush() override { fflush(m_chan); }

  ~TgaWriter() { delete m_properties; }

  inline void fputPixel32(const TPixel32 &pix, FILE *chan) {
    fputc(pix.b, m_chan);
    fputc(pix.g, m_chan);
    fputc(pix.r, m_chan);
    fputc(pix.m, m_chan);
  }
  inline void fputPixel24(const TPixel32 &pix, FILE *chan) {
    fputc(pix.b, m_chan);
    fputc(pix.g, m_chan);
    fputc(pix.r, m_chan);
  }
  inline void fputPixel16(const TPixel32 &pix, FILE *chan) {
    unsigned short word;
    word = (pix.r >> 3) << 10 | (pix.g >> 3) << 5 | (pix.b >> 3);
    fputc(word & 0xFF, m_chan);
    fputc((word >> 8) & 0xFF, m_chan);
  }

  void writeNoLine(char *buffer) {}

  void writeLine16(char *buffer);
  void writeLine24(char *buffer);
  void writeLine32(char *buffer);
  void writeLine16rle(char *buffer);
  void writeLine24rle(char *buffer);
  void writeLine32rle(char *buffer);

  void writeLine(char *buffer) override { (this->*m_writeLineProc)(buffer); }
};

//------------------------------------------------------------

void TgaWriter::writeLine32rle(char *buffer) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    if (x + 1 < m_info.m_lx && row[x] == row[x + 1]) {
      int count = 2;
      int max   = std::min(128, m_info.m_lx - x);
      while (count < max && row[x + count - 1] == row[x + count]) count++;
      fputc((count - 1) | 0x80, m_chan);
      fputPixel32(row[x], m_chan);
      x += count;
    } else {
      int count = 1;
      int max   = std::min(128, m_info.m_lx - x);
      while (count < max && row[x + count - 1] != row[x + count]) count++;
      fputc(count - 1, m_chan);
      for (int i = 0; i < count; i++) fputPixel32(row[x++], m_chan);
    }
  }
}

//------------------------------------------------------------

void TgaWriter::writeLine24rle(char *buffer) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    if (x + 1 < m_info.m_lx && row[x] == row[x + 1]) {
      int count = 2;
      int max   = std::min(128, m_info.m_lx - x);
      while (count < max && row[x + count - 1] == row[x + count]) count++;
      fputc((count - 1) | 0x80, m_chan);
      fputPixel24(row[x], m_chan);
      x += count;
    } else {
      int count = 1;
      int max   = std::min(128, m_info.m_lx - x);
      while (count < max && row[x + count - 1] != row[x + count]) count++;
      fputc(count - 1, m_chan);
      for (int i = 0; i < count; i++) fputPixel24(row[x++], m_chan);
    }
  }
}

//------------------------------------------------------------

void TgaWriter::writeLine16rle(char *buffer) {
  int x         = 0;
  TPixel32 *row = (TPixel32 *)buffer;
  while (x < m_info.m_lx) {
    if (x + 1 < m_info.m_lx && row[x] == row[x + 1]) {
      int count = 2;
      int max   = std::min(128, m_info.m_lx - x);
      while (count < max && row[x + count - 1] == row[x + count]) count++;
      fputc((count - 1) | 0x80, m_chan);
      fputPixel16(row[x], m_chan);
      x += count;
    } else {
      int count = 1;
      int max   = std::min(128, m_info.m_lx - x);
      while (count < max && row[x + count - 1] != row[x + count]) count++;
      fputc(count - 1, m_chan);
      for (int i = 0; i < count; i++) fputPixel16(row[x++], m_chan);
    }
  }
}

//------------------------------------------------------------

void TgaWriter::writeLine32(char *buffer) {
  TPixel32 *pix    = (TPixel32 *)buffer;
  TPixel32 *endPix = pix + m_info.m_lx;
  while (pix < endPix) fputPixel32(*pix++, m_chan);
}

//------------------------------------------------------------

void TgaWriter::writeLine24(char *buffer) {
  TPixel32 *pix    = (TPixel32 *)buffer;
  TPixel32 *endPix = pix + m_info.m_lx;
  while (pix < endPix) fputPixel24(*pix++, m_chan);
}

//------------------------------------------------------------

void TgaWriter::writeLine16(char *buffer) {
  TPixel32 *pix    = (TPixel32 *)buffer;
  TPixel32 *endPix = pix + m_info.m_lx;
  while (pix < endPix) fputPixel16(*pix++, m_chan);
}

//============================================================

Tiio::Reader *Tiio::makeTgaReader() { return new TgaReader(); }

Tiio::Writer *Tiio::makeTgaWriter() { return new TgaWriter(); }
