#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <memory>

#include "tiio_bmp.h"
// #include "bmp/filebmp.h"
#include "tpixel.h"
#include "tpixelgr.h"
#include "tproperty.h"
//#include "tconvert.h"
#include <stdio.h>

//=========================================================

namespace {

//=========================================================

typedef struct {
  UINT bfSize;
  UINT bfOffBits;
  UINT biSize;
  UINT biWidth;
  UINT biHeight;
  UINT biPlanes;
  UINT biBitCount;
  UINT biCompression;
  UINT biSizeImage;
  UINT biXPelsPerMeter;
  UINT biYPelsPerMeter;
  UINT biClrUsed;
  UINT biClrImportant;
  UINT biFilesize;
  UINT biPad;

} BMP_HEADER;

const int BMP_WIN_SIZE = 40;
const int BMP_OS2_SIZE = 64;

const int BMP_BI_RGB  = 0;
const int BMP_BI_RLE8 = 1;
const int BMP_BI_RLE4 = 2;

//=========================================================

UINT getshort(FILE *fp) {
  int c = getc(fp), c1 = getc(fp);

  return ((UINT)c) + (((UINT)c1) << 8);
}

//---------------------------------------------------------

UINT getint(FILE *fp) {
  int c = getc(fp), c1 = getc(fp), c2 = getc(fp), c3 = getc(fp);

  return (((UINT)c) << 0) + (((UINT)c1) << 8) + (((UINT)c2) << 16) +
         (((UINT)c3) << 24);
}

//---------------------------------------------------------

void putshort(FILE *fp, int i) {
  int c = (((UINT)i)) & 0xff, c1 = (((UINT)i) >> 8) & 0xff;

  putc(c, fp);
  putc(c1, fp);
}

//---------------------------------------------------------

void putint(FILE *fp, int i) {
  int c = ((UINT)i) & 0xff, c1 = (((UINT)i) >> 8) & 0xff,
      c2 = (((UINT)i) >> 16) & 0xff, c3 = (((UINT)i) >> 24) & 0xff;

  putc(c, fp);
  putc(c1, fp);
  putc(c2, fp);
  putc(c3, fp);
}

//=========================================================

}  // namespace

//=========================================================

class BmpReader final : public Tiio::Reader {
  FILE *m_chan;
  BMP_HEADER m_header;
  char *m_line;
  int m_lineSize;
  std::unique_ptr<TPixel[]> m_cmap;
  bool m_corrupted;
  typedef int (BmpReader::*ReadLineMethod)(char *buffer, int x0, int x1,
                                           int shrink);
  ReadLineMethod m_readLineMethod;

public:
  BmpReader();
  ~BmpReader();

  void open(FILE *file) override;

  int readNoLine(char *buffer, int x0, int x1, int shrink);

  void skipBytes(int count) {
    // fseek(m_chan, count, SEEK_CUR);	//inefficiente se count è piccolo
    for (int i = 0; i < count; i++) {
      getc(m_chan);
    }
  }

  int read1Line(char *buffer, int x0, int x1, int shrink);
  int read4Line(char *buffer, int x0, int x1, int shrink);
  int read8Line(char *buffer, int x0, int x1, int shrink);
  int read8LineRle(char *buffer, int x0, int x1, int shrink);
  int read16m555Line(char *buffer, int x0, int x1, int shrink);
  int read16m565Line(char *buffer, int x0, int x1, int shrink);
  int read24Line(char *buffer, int x0, int x1, int shrink);
  int read32Line(char *buffer, int x0, int x1, int shrink);

  void readLine(char *buffer, int x0, int x1, int shrink) override;
  int skipLines(int lineCount) override {
    fseek(m_chan, lineCount * m_lineSize, SEEK_CUR);
    /* for(int i=0;i<lineCount;i++)
            skipBytes(m_lineSize);*/
    return lineCount;
  }

private:
  void readHeader();
};

//---------------------------------------------------------

BmpReader::BmpReader()
    : m_chan(0), m_corrupted(false), m_readLineMethod(&BmpReader::readNoLine) {
  memset(&m_header, 0, sizeof m_header);
}

//---------------------------------------------------------

BmpReader::~BmpReader() {}

//---------------------------------------------------------

void BmpReader::open(FILE *file) {
  m_chan = file;
  readHeader();
}

//---------------------------------------------------------

void BmpReader::readHeader() {
  if (!m_chan) return;

  fseek(m_chan, 0L, SEEK_END);
  m_header.biFilesize = ftell(m_chan);
  fseek(m_chan, 0L, 0);

  /* read the file type (first two bytes) */
  char signature[2];
  signature[0] = fgetc(m_chan);
  signature[1] = fgetc(m_chan);
  if (signature[0] != 'B' || signature[1] != 'M') {
    m_corrupted = true;
    return;
  }

  m_header.bfSize = getint(m_chan);

  /* reserved and ignored */
  getshort(m_chan);
  getshort(m_chan);

  m_header.bfOffBits = getint(m_chan);
  m_header.biSize    = getint(m_chan);

  if ((int)m_header.biSize == BMP_WIN_SIZE ||
      (int)m_header.biSize == BMP_OS2_SIZE) {
    m_header.biWidth         = getint(m_chan);
    m_header.biHeight        = getint(m_chan);
    m_header.biPlanes        = getshort(m_chan);
    m_header.biBitCount      = getshort(m_chan);
    m_header.biCompression   = getint(m_chan);
    m_header.biSizeImage     = getint(m_chan);
    m_header.biXPelsPerMeter = getint(m_chan);
    m_header.biYPelsPerMeter = getint(m_chan);
    m_header.biClrUsed       = getint(m_chan);
    m_header.biClrImportant  = getint(m_chan);
  } else  // old bitmap format
  {
    m_header.biWidth    = getshort(m_chan);
    m_header.biHeight   = getshort(m_chan);
    m_header.biPlanes   = getshort(m_chan);
    m_header.biBitCount = getshort(m_chan);

    /* Not in old versions so have to compute them */
    m_header.biSizeImage =
        (((m_header.biPlanes * m_header.biBitCount * m_header.biWidth) + 31) /
         32) *
        4 * m_header.biHeight;

    m_header.biCompression   = BMP_BI_RGB;
    m_header.biXPelsPerMeter = 0;
    m_header.biYPelsPerMeter = 0;
    m_header.biClrUsed       = 0;
    m_header.biClrImportant  = 0;
  }
  m_header.biPad = 0;

  m_info.m_lx = m_header.biWidth;
  m_info.m_ly = m_header.biHeight;

  /*
BMP_HEADER *hd = m_header;
if ((int)hd->biSize != BMP_WIN_OS2_OLD)
{
// skip ahead to colormap, using biSize
int c = hd->biSize - 40;  // 40 bytes read from biSize to biClrImportant
for (int i=0; i<c; i++) getc(m_chan);
hd->biPad = hd->bfOffBits - (hd->biSize + 14);
}
else
hd->biPad = 0;
*/

  // load up colormap, if any
  if (m_header.biBitCount < 16) {
    int cmaplen =
        (m_header.biClrUsed) ? m_header.biClrUsed : 1 << m_header.biBitCount;
    assert(cmaplen <= 256);
    m_cmap.reset(new TPixel[256]);
    TPixel32 *pix = m_cmap.get();
    for (int i = 0; i < cmaplen; i++) {
      pix->b = getc(m_chan);
      pix->g = getc(m_chan);
      pix->r = getc(m_chan);
      pix->m = 255;
      getc(m_chan);
      ++pix;
    }
  }

  /*
if (hd->biSize != BMP_WIN_OS2_OLD)
{
// Waste any unused bytes between the colour map (if present)
// and the start of the actual bitmap data.
while (hd->biPad > 0)
{
  (void) getc(m_chan);
  hd->biPad--;
}
}
*/
  // get information about the portion of the image to load
  // get_info_region(&info, x1, y1, x2, y2, scale,
  //		  (int)hd->biWidth,   (int)hd->biHeight, TNZ_BOTLEFT);

  //   skip_bmp_lines(fp, hd->biWidth, (unsigned int)(info.startScanRow-1),
  //   (unsigned int)SEEK_CUR, subtype);

  int lx = m_info.m_lx;

  switch (m_header.biBitCount) {
  case 1:
    m_info.m_samplePerPixel = 1;
    m_readLineMethod        = &BmpReader::read1Line;
    m_lineSize              = (lx + 7) / 8;
    break;
  case 4:
    m_info.m_samplePerPixel                           = 1;
    if (m_header.biCompression == 0) m_readLineMethod = &BmpReader::read4Line;
    m_lineSize                                        = (lx + 1) / 2;
    break;
  case 8:
    m_info.m_samplePerPixel = 1;
    if (m_header.biCompression == 0)
      m_readLineMethod = &BmpReader::read8Line;
    else if (m_header.biCompression == 1)
      m_readLineMethod = &BmpReader::read8LineRle;
    m_lineSize         = lx;
    break;
  case 16:
    m_info.m_samplePerPixel = 3;
    if (m_header.biCompression == 0)  // BI_RGB
      m_readLineMethod = &BmpReader::read16m555Line;
    else if (m_header.biCompression == 3)  // BI_BITFIELDS
    {
      unsigned int rmask = 0, gmask = 0, bmask = 0;
      rmask = getint(m_chan);
      gmask = getint(m_chan);
      bmask = getint(m_chan);
      if (gmask == 0x7E0)
        m_readLineMethod = &BmpReader::read16m565Line;
      else
        m_readLineMethod = &BmpReader::read16m555Line;
    }
    m_lineSize = lx * 2;
    break;
  case 24:
    m_info.m_samplePerPixel = 3;
    m_readLineMethod        = &BmpReader::read24Line;
    m_lineSize              = lx * 3;
    break;
  case 32:
    m_info.m_samplePerPixel = 3;
    m_readLineMethod        = &BmpReader::read32Line;
    m_lineSize              = lx * 4;
    break;
  }
  m_lineSize += 3 - ((m_lineSize + 3) & 3);
  fseek(m_chan, m_header.bfOffBits, SEEK_SET);
}

//---------------------------------------------------------

void BmpReader::readLine(char *buffer, int x0, int x1, int shrink) {
  (this->*m_readLineMethod)(buffer, x0, x1, shrink);
}

//---------------------------------------------------------

int BmpReader::readNoLine(char *buffer, int x0, int x1, int shrink) {
  skipBytes(m_lineSize);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read1Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;

  // pix += x0;
  if (x0 > 0) skipBytes(x0 / 8);

  TPixel32 *endPix = pix + x1 + 1;

  int value = 0;
  int index = x0;

  if (x0 % 8 != 0) {
    value = getc(m_chan);
    for (index = x0; index < x0 + 8 - (x0 % 8); index += shrink) {
      pix[index] = m_cmap[(value >> (7 - ((index) % 8))) & 1];
    }
  }
  value         = getc(m_chan);
  int prevBlock = index / 8;
  for (int j = index; pix + j < endPix; j += shrink) {
    if (j / 8 > prevBlock) value = getc(m_chan);
    prevBlock                    = j / 8;
    pix[j]                       = m_cmap[(value >> (7 - (j % 8))) & 1];
  }

  /*
while(pix+8<=endPix)
{
value = getc(m_chan);
pix[0] = m_cmap[(value>>7)&1];
pix[1] = m_cmap[(value>>6)&1];
pix[2] = m_cmap[(value>>5)&1];
pix[3] = m_cmap[(value>>4)&1];
pix[4] = m_cmap[(value>>3)&1];
pix[5] = m_cmap[(value>>2)&1];
pix[6] = m_cmap[(value>>1)&1];
pix[7] = m_cmap[(value>>0)&1];
   pix +=8*shrink;
   if(shrink>1) value = getc(m_chan);
//pix+=8*shrink;
   //if(pix+8<endPix && shrink>1) skipBytes(shrink-1);
}
if(pix<endPix)
{
value = getc(m_chan);
assert(pix+8>=endPix);
for(int j=0; pix+j<endPix; j++)
pix[j] = m_cmap[(value>>(7-j))&1];
}		*/
  if ((m_info.m_lx - x1) / 8 > 0) {
    skipBytes((m_info.m_lx - x1) / 8);
  }

  int bytes = (m_info.m_lx + 7) / 8;
  if (m_lineSize - bytes > 0) skipBytes(m_lineSize - bytes);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read4Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  pix += 2 * x0;
  if (x0 > 0) skipBytes(x0 / 2);
  TPixel32 *endPix = pix + x1 - x0 + 1;

  int value;
  while (pix + 2 <= endPix) {
    value  = getc(m_chan);
    pix[0] = m_cmap[value & 0xF];
    pix[1] = m_cmap[(value >> 4) & 0xF];
    // pix+=2*shrink;
    pix++;
    // if(pix+2<=endPix && shrink>1) skipBytes(shrink-1);
  }
  if (pix < endPix) {
    value  = getc(m_chan);
    pix[0] = m_cmap[value & 0xF];
  }
  if ((m_info.m_lx - x1) / 2 > 0) {
    skipBytes((m_info.m_lx - x1) / 2);
  }

  int bytes = (m_info.m_lx + 1) / 2;
  if (m_lineSize - bytes) skipBytes(m_lineSize - bytes);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read8Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;

  if (x0 > 0) skipBytes(x0);
  pix += x0;
  TPixel32 *endPix = pix + x1 - x0 + 1;

  while (pix < endPix) {
    int value = getc(m_chan);
    *pix++    = m_cmap[value];
    if (pix < endPix && shrink > 1) {
      skipBytes(shrink - 1);
      pix += (shrink - 1);
    }
  }

  if (m_info.m_lx - x1 - 1 > 0) {
    skipBytes(m_info.m_lx - x1 - 1);
  }

  if (m_lineSize - m_info.m_lx > 0) skipBytes(m_lineSize - m_info.m_lx);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read8LineRle(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  if (x0 > 0) skipBytes(x0);
  pix += x0;
  TPixel32 *endPix = pix + (x1 - x0 + 1);

  while (pix < endPix) {
    int i, count = getc(m_chan);
    assert(count >= 0);
    if (count == 0) {
      int pixels = getc(m_chan);

      assert(pixels >= 0 && pixels != 2);
      if (pixels < 3) return 0;
      for (i = 0; i < pixels; i++) *pix++ = m_cmap[getc(m_chan)];
      if ((1 + 1 + pixels) & 0x1) getc(m_chan);
    } else {
      int value = getc(m_chan);
      assert(value >= 0);
      for (i = 0; i < count; i++) *pix++ = m_cmap[value];
    }
    if (pix < endPix && shrink > 1) {
      skipBytes(shrink - 1);
      pix += (shrink - 1);
    }
  }

  if (m_info.m_lx - x1 - 1 > 0) {
    skipBytes(m_info.m_lx - x1 - 1);
  }

  if (m_lineSize - m_info.m_lx > 0) skipBytes(m_lineSize - m_info.m_lx);
  int val = getc(m_chan);
  assert(val == 0);  // end scanline or end bmp
  val = getc(m_chan);
  assert(val == 0 || val == 1);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read16m555Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  if (x0 > 0) skipBytes(2 * x0);
  pix += x0;
  TPixel32 *endPix = pix + x1 - x0 + 1;

  while (pix < endPix) {
    int v  = getshort(m_chan);
    int r  = (v >> 10) & 0x1F;
    int g  = (v >> 5) & 0x1F;
    int b  = v & 0x1F;
    pix->r = r << 3 | r >> 2;
    pix->g = g << 3 | g >> 2;
    pix->b = b << 3 | b >> 2;
    pix->m = 255;
    pix += shrink;
    if (pix < endPix && shrink > 1) skipBytes(2 * (shrink - 1));
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(2 * (m_info.m_lx - x1 - 1));

  if (m_lineSize - 2 * m_info.m_lx > 0) skipBytes(m_lineSize - 2 * m_info.m_lx);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read16m565Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  if (x0 > 0) skipBytes(2 * x0);
  pix += x0;
  TPixel32 *endPix = pix + x1 - x0 + 1;

  while (pix < endPix) {
    int v  = getshort(m_chan);
    int r  = (v >> 11) & 0x1F;
    int g  = (v >> 5) & 0x3F;
    int b  = v & 0x1F;
    pix->r = r << 3 | r >> 2;
    pix->g = g << 2 | g >> 4;
    pix->b = b << 3 | b >> 2;
    pix->m = 255;
    pix += shrink;
    if (pix < endPix && shrink > 1) skipBytes(2 * (shrink - 1));
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(2 * (m_info.m_lx - x1 - 1));

  if (m_lineSize - 2 * m_info.m_lx > 0) skipBytes(m_lineSize - 2 * m_info.m_lx);
  return 0;
}
//---------------------------------------------------------

int BmpReader::read24Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  if (x0 > 0) skipBytes(3 * x0);
  pix += x0;
  TPixel32 *endPix = pix + x1 - x0 + 1;

  while (pix < endPix) {
    pix->b = getc(m_chan);
    pix->g = getc(m_chan);
    pix->r = getc(m_chan);
    pix->m = 255;
    pix += shrink;
    if (pix < endPix && shrink > 1) skipBytes(3 * (shrink - 1));
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(3 * (m_info.m_lx - x1 - 1));

  if (m_lineSize - 3 * m_info.m_lx > 0) skipBytes(m_lineSize - 3 * m_info.m_lx);
  return 0;
}

//---------------------------------------------------------

int BmpReader::read32Line(char *buffer, int x0, int x1, int shrink) {
  TPixel32 *pix = (TPixel32 *)buffer;
  if (x0 > 0) skipBytes(4 * x0);
  pix += x0;
  TPixel32 *endPix = pix + x1 - x0 + 1;

  while (pix < endPix) {
    pix->b = getc(m_chan);
    pix->g = getc(m_chan);
    pix->r = getc(m_chan);
    pix->m = getc(m_chan);
    pix += shrink;
    if (pix < endPix && shrink > 1) skipBytes(4 * (shrink - 1));
  }
  if (m_info.m_lx - x1 - 1 > 0) skipBytes(4 * (m_info.m_lx - x1 - 1));
  if (m_lineSize - 4 * m_info.m_lx > 0) skipBytes(m_lineSize - 4 * m_info.m_lx);
  return 0;
}

//=========================================================

class BmpWriter final : public Tiio::Writer {
  FILE *m_chan;
  int m_bitPerPixel;
  int m_compression;

public:
  BmpWriter();
  ~BmpWriter();

  void open(FILE *file, const TImageInfo &info) override;

  void flush() override { fflush(m_chan); }

  void writeLine(char *buffer) override;
};

//---------------------------------------------------------

BmpWriter::BmpWriter() : m_chan(0) {}

//---------------------------------------------------------

BmpWriter::~BmpWriter() { delete m_properties; }

//---------------------------------------------------------

void BmpWriter::open(FILE *file, const TImageInfo &info) {
  m_chan = file;

  m_info = info;
  int lx = m_info.m_lx;
  int ly = m_info.m_ly;

  if (!m_properties) m_properties = new Tiio::BmpWriterProperties();

  TEnumProperty *p =
      (TEnumProperty *)(m_properties->getProperty("Bits Per Pixel"));
  assert(p);
  std::string str = ::to_string(p->getValue());
  m_bitPerPixel   = atoi(str.c_str());

  int cmapSize                  = 0;
  std::vector<TPixel> *colormap = 0;

  if (m_bitPerPixel == 8) {
    TPointerProperty *colormapProp =
        (TPointerProperty *)(m_properties->getProperty("Colormap"));
    if (colormapProp) {
      colormap = (std::vector<TPixel> *)colormapProp->getValue();
      cmapSize = colormap->size();
    } else
      cmapSize = 256;
  }

  assert(m_bitPerPixel == 8 || m_bitPerPixel == 24);

  int bytePerLine =
      ((lx * m_bitPerPixel + 31) / 32) * (m_bitPerPixel == 8 ? 1 : 4);

  int fileSize = 14                  // file header
                 + 40                // info header
                 + cmapSize * 4      // colormap size
                 + bytePerLine * ly  // image size
      ;
  int offset      = 14 + 40 + cmapSize * 4;
  int compression = 0;
  int imageSize   = bytePerLine * ly;

  putc('B', m_chan);
  putc('M', m_chan);  // BMP file magic number

  putint(m_chan, fileSize);
  putshort(m_chan, 0);  // reserved1
  putshort(m_chan, 0);  // reserved2

  putint(m_chan, offset);  // offset from BOfile to BObitmap

  putint(m_chan, 40);               // size of bitmap info header
  putint(m_chan, m_info.m_lx);      // width
  putint(m_chan, m_info.m_ly);      // height
  putshort(m_chan, 1);              // must be '1'
  putshort(m_chan, m_bitPerPixel);  // 1,4,8, or 24
  putint(m_chan, compression);      // BMP_BI_RGB, BMP_BI_RLE8 or BMP_BI_RLE4
  putint(m_chan, imageSize);        // size of raw image data
  putint(m_chan, 0);                // dpi * 39" per meter
  putint(m_chan, 0);                // dpi * 39" per meter
  putint(m_chan, cmapSize);         // colors used in cmap
  putint(m_chan, 0);                // same as above

  if (colormap)
    for (int i = 0; i < (int)colormap->size(); i++) {
      putc((*colormap)[i].b, m_chan);
      putc((*colormap)[i].g, m_chan);
      putc((*colormap)[i].r, m_chan);
      putc(0, m_chan);
    }
  else
    for (int i = 0; i < cmapSize; i++)  // palette!!
    {
      putc(i, m_chan);
      putc(i, m_chan);
      putc(i, m_chan);
      putc(0, m_chan);
    }
}

//---------------------------------------------------------

void BmpWriter::writeLine(char *buffer) {
  int lx = m_info.m_lx;

  int j;

  switch (m_bitPerPixel) {
  case 24: {
    TPixel32 *pix = (TPixel32 *)buffer;
    for (j = 0; j < lx; j++) {
      putc(pix->b, m_chan);
      putc(pix->g, m_chan);
      putc(pix->r, m_chan);
      ++pix;
    }
    int bytes = lx * 3;
    while (bytes & 3) {
      putc(0, m_chan);
      bytes++;
    }
    break;
  }
  case 8: {
    TPixelGR8 *pix = (TPixelGR8 *)buffer;
    for (j = 0; j < lx; j++) {
      putc(pix->value, m_chan);
      ++pix;
    }
    while (lx & 3) {
      putc(0, m_chan);
      lx++;
    }
    break;
  }
  default:
    assert(false);
  }
  ftell(m_chan);
}

//=========================================================

Tiio::Reader *Tiio::makeBmpReader() { return new BmpReader(); }

//=========================================================

Tiio::Writer *Tiio::makeBmpWriter() { return new BmpWriter(); }

//=========================================================

Tiio::BmpWriterProperties::BmpWriterProperties()
    : m_pixelSize("Bits Per Pixel") {
  m_pixelSize.addValue(L"24 bits");
  m_pixelSize.addValue(L"8 bits (Greyscale)");
  bind(m_pixelSize);
}

void Tiio::BmpWriterProperties::updateTranslation() {
  m_pixelSize.setQStringName(tr("Bits Per Pixel"));
  m_pixelSize.setItemUIName(L"24 bits", tr("24 bits"));
  m_pixelSize.setItemUIName(L"8 bits (Greyscale)", tr("8 bits (Greyscale)"));
}
