#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif
#define IS_TIFF_MAIN

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <memory>

#include "tiio.h"
#include "tpixel.h"
#include "tsystem.h"
#include "../tzp/toonztags.h"
#include "tconvert.h"
#include "tpixelutils.h"
#include "traster.h"

extern "C" {
#include "tiffio.h"
}

#include "tiio_tif.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include "windows.h"
#endif

//**************************************************************************
//    TifReader  implementation
//**************************************************************************

class TifReader final : public Tiio::Reader {
  TIFF *m_tiff;
  int m_row;
  bool m_tiled, m_stripped;
  int m_rowsPerStrip;
  int m_stripIndex;
  int m_rowLength;
  UCHAR *m_stripBuffer;
  double m_xdpi, m_ydpi;
  Tiio::RowOrder m_rowOrder;
  bool is16bitEnabled;
  bool m_isTzi;
  TRasterGR8P m_tmpRas;

public:
  TifReader(bool isTzi);
  ~TifReader();

  void open(FILE *file) override;

  Tiio::RowOrder getRowOrder() const override { return m_rowOrder; }

  bool read16BitIsEnabled() const override { return false; }

  void enable16BitRead(bool enabled) override { is16bitEnabled = enabled; }

  int skipLines(int lineCount) override;
  void readLine(char *buffer, int x0, int x1, int shrink) override;
  void readLine(short *buffer, int x0, int x1, int shrink) override;
};

//------------------------------------------------------------

TifReader::TifReader(bool isTzi)
    : m_tiff(0)
    , m_row(0)
    , m_rowsPerStrip(0)
    , m_stripIndex(-1)
    //, m_stripBuffer(0)
    , m_rowLength(0)
    , m_xdpi(0)
    , m_ydpi(0)
    , m_rowOrder(Tiio::TOP2BOTTOM)
    , is16bitEnabled(true)
    , m_isTzi(isTzi)
    , m_tmpRas(0) {
  TIFFSetWarningHandler(0);
}

//------------------------------------------------------------

TifReader::~TifReader() {
  if (m_tiff) TIFFClose(m_tiff);

  if (m_tmpRas) m_tmpRas->unlock();

  delete m_info.m_properties;
}

//------------------------------------------------------------

void TifReader::open(FILE *file) {
  int fd = fileno(file);
#if 0
	m_tiff = TIFFFdOpenNoCloseProc(fd, "", "rb");
#elif defined(_WIN32) && defined(__GNUC__)
  m_tiff = TIFFFdOpen((int)_get_osfhandle(dup(fd)), "", "rb");
#else
  m_tiff = TIFFFdOpen(dup(fd), "", "rb");
#endif
  if (!m_tiff) {
    std::string str("Tiff file closed");
    throw(str);
  }

  uint32 w = 0, h = 0, rps = 0;
  uint16 bps = 0, spp = 0;
  uint32 tileWidth = 0, tileLength = 0;

  // TIFFSetDirectory(m_tiff,1);
  // TIFFGetField(m_tiff, TIFFTAG_PAGENUMBER, &pn);
  // int pn = TIFFNumberOfDirectories(m_tiff);
  // TIFFSetDirectory(m_tiff,1);
  TIFFGetField(m_tiff, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(m_tiff, TIFFTAG_IMAGELENGTH, &h);
  TIFFGetField(m_tiff, TIFFTAG_BITSPERSAMPLE, &bps);
  TIFFGetField(m_tiff, TIFFTAG_SAMPLESPERPIXEL, &spp);
  TIFFGetField(m_tiff, TIFFTAG_ROWSPERSTRIP, &rps);
  // int stripCount = TIFFNumberOfStrips(m_tiff);
  // int tileCount = TIFFNumberOfTiles(m_tiff);
  TIFFGetField(m_tiff, TIFFTAG_TILEWIDTH, &tileWidth);
  TIFFGetField(m_tiff, TIFFTAG_TILELENGTH, &tileLength);
  Tiio::TifWriterProperties *prop = new Tiio::TifWriterProperties();
  m_info.m_properties             = prop;
  uint16 orient                   = Tiio::TOP2BOTTOM;
  if (TIFFGetField(m_tiff, TIFFTAG_ORIENTATION, &orient)) {
    switch (orient) {
    case ORIENTATION_TOPLEFT: /* row 0 top, col 0 lhs */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_TOPLEFT);
      m_rowOrder = Tiio::TOP2BOTTOM;
      break;
    case ORIENTATION_TOPRIGHT: /* row 0 top, col 0 rhs */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_TOPRIGHT);
      m_rowOrder = Tiio::TOP2BOTTOM;
      break;
    case ORIENTATION_LEFTTOP: /* row 0 lhs, col 0 top */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_LEFTTOP);
      m_rowOrder = Tiio::TOP2BOTTOM;
      break;
    case ORIENTATION_RIGHTTOP: /* row 0 rhs, col 0 top */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_RIGHTTOP);
      m_rowOrder = Tiio::TOP2BOTTOM;
      break;

    case ORIENTATION_BOTRIGHT: /* row 0 bottom, col 0 rhs */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_BOTRIGHT);
      m_rowOrder = Tiio::BOTTOM2TOP;
      break;
    case ORIENTATION_BOTLEFT: /* row 0 bottom, col 0 lhs */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_BOTLEFT);
      m_rowOrder = Tiio::BOTTOM2TOP;
      break;
    case ORIENTATION_RIGHTBOT: /* row 0 rhs, col 0 bottom */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_RIGHTBOT);
      m_rowOrder = Tiio::BOTTOM2TOP;
      break;
    case ORIENTATION_LEFTBOT: /* row 0 lhs, col 0 bottom */
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_LEFTBOT);
      m_rowOrder = Tiio::BOTTOM2TOP;
      break;
    default:
      prop->m_orientation.setValue(TNZ_INFO_ORIENT_NONE);
      m_rowOrder = Tiio::TOP2BOTTOM;
      break;
    }
  }

  USHORT compression;
  TIFFGetField(m_tiff, TIFFTAG_COMPRESSION, &compression);
  switch (compression) {
  case COMPRESSION_LZW:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_LZW);
    break;
  case COMPRESSION_PACKBITS:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_PACKBITS);
    break;
  case COMPRESSION_THUNDERSCAN:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_THUNDERSCAN);
    break;
  case COMPRESSION_CCITTFAX3:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_CCITTFAX3);
    break;
  case COMPRESSION_CCITTFAX4:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_CCITTFAX4);
    break;
  case COMPRESSION_CCITTRLE:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_CCITTRLE);
    break;
  case COMPRESSION_JPEG:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_JPEG);
    break;
  case COMPRESSION_OJPEG:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_OJPEG);
    break;
  case COMPRESSION_NONE:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_NONE);
    break;
  case COMPRESSION_SGILOG:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_SGILOG);
    break;
  case COMPRESSION_SGILOG24:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_SGILOG24);
    break;
  case COMPRESSION_ADOBE_DEFLATE:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_ADOBE_DEFLATE);
    break;
  case COMPRESSION_DEFLATE:
    prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_DEFLATE);
    break;
  /*default :
prop->m_compressionType.setValue(TNZ_INFO_COMPRESS_UNKNOWN);
break;*/
  default:
    assert(0);
  }

  float xdpi = 0, ydpi = 0;
  TIFFGetField(m_tiff, TIFFTAG_XRESOLUTION, &xdpi);
  TIFFGetField(m_tiff, TIFFTAG_YRESOLUTION, &ydpi);

  bool swapxy = false;  // orient == ORIENTATION_RIGHTTOP;

  if (swapxy) {
    std::swap(w, h);
    std::swap(xdpi, ydpi);
  }

  m_xdpi        = xdpi;
  m_ydpi        = ydpi;
  m_info.m_lx   = w;
  m_info.m_ly   = h;
  m_info.m_dpix = xdpi;
  m_info.m_dpiy = ydpi;

  m_info.m_samplePerPixel = spp;

  if (bps == 64 && spp == 3) bps = 16;  // immagine con bpp = 192

  uint16 photometric;  // codice di controllo
  TIFFGetField(m_tiff, TIFFTAG_PHOTOMETRIC, &photometric);
  if (photometric == 3 &&
      (bps == 2 || bps == 4))  // immagini con PHOTOMATRIC_PALETTE
    bps = 8;

  if (photometric == 1 && (bps == 12 || bps == 24)) bps = 16;

  if (bps == 6) bps = 4;  // immagini con bps = 6

  if (bps == 10 || bps == 12 ||
      bps == 14)  // immagini con bps = 10 , 12 , 14 , 24 , 32
    bps                           = 8;
  if (bps == 24 || bps == 32) bps = 16;

  m_info.m_bitsPerSample = bps;

  if (bps == 8) switch (spp) {
    case 1: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L" 8(GREYTONES)");
      break;
    case 3: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"24(RGB)");
      break;
    case 4: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    }
  else if (bps == 16)
    switch (spp) {
    case 1: /* row 0 top, col 0 lhs */
      // prop->m_bitsPerPixel.setValue(L"16(GREYTONES)");
      break;
    case 3: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"48(RGB)");
      break;
    case 4: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"64(RGBM)");
      break;
    }

  else if (bps == 2)
    switch (spp) {
    case 1: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    case 3: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    case 4: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    }

  else if (bps == 1)
    switch (spp) {
    case 1: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L" 1(BW)");
      break;
    case 3: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    case 4: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    }

  else if (bps == 4)
    switch (spp) {
    case 1: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L" 8(GREYTONES)");
      break;
    case 3: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    case 4: /* row 0 top, col 0 lhs */
      prop->m_bitsPerPixel.setValue(L"32(RGBM)");
      break;
    }

  else if (bps == 64 && spp == 3)
    prop->m_bitsPerPixel.setValue(L"64(RGBM)");

  else
    assert(false);

  if (TIFFIsTiled(m_tiff)) {
    m_rowsPerStrip  = tileLength;
    int tilesPerRow = (w + tileWidth - 1) / tileWidth;
    // m_rowLength = tileWidth * tilesPerRow;
    m_rowLength   = m_info.m_lx;
    int pixelSize = bps == 16 ? 8 : 4;
    int stripSize = m_rowsPerStrip * m_rowLength * pixelSize;

    m_tmpRas = TRasterGR8P(stripSize, 1);
    m_tmpRas->lock();

    m_stripBuffer = m_tmpRas->getRawData();
  } else {
    m_rowsPerStrip = rps;
    // if(m_rowsPerStrip<=0) m_rowsPerStrip = 1;			//potrei
    // mettere
    // qualsiasi
    // valore
    // purchè sia lo stesso in tif_getimage.c linea 2512
    // if(m_rowsPerStrip==-1) assert(0);

    if (m_rowsPerStrip <= 0) m_rowsPerStrip = m_info.m_ly;

    int stripSize = m_rowsPerStrip * w * 4;  // + 4096;  TIFFStripSize(m_tiff);

    if (bps == 16) stripSize *= 2;

    m_tmpRas = TRasterGR8P(stripSize, 1);
    m_tmpRas->lock();

    m_stripBuffer = m_tmpRas->getRawData();

    m_rowLength = m_info.m_lx;  // w;
  }

  /*
int TIFFTileRowSize(m_tiff);

m_rowsPerStrip = 0;
if(TIFFGetField(m_tiff, TIFFTAG_ROWSPERSTRIP, &rps) )
{
int stripSize = TIFFStripSize(m_tiff);
if(stripSize>0)
 {
 }
}
*/
  if (m_isTzi) {
    USHORT risCount  = 0;
    USHORT *risArray = 0;

    if (TIFFGetField(m_tiff, TIFFTAG_TOONZWINDOW, &risCount, &risArray)) {
      if (m_info.m_lx == risArray[2] &&
          m_info.m_ly ==
              risArray[3])  // se sono diverse, la lettura tif crasha....
      {
        // m_info.m_lx = risArray[2];
        // m_info.m_ly = risArray[3];
        m_info.m_x0 = risArray[0];
        m_info.m_y0 = risArray[1];
      }
      //      USHORT extraMask = risArray[4];
      // bool isEduFile = risArray[TOONZWINDOW_COUNT - 1] & 1;
    } else {
      m_info.m_x0 = 0;
      m_info.m_y0 = 0;
    }
    if (swapxy) {
      std::swap(m_info.m_x0, m_info.m_y0);
      std::swap(m_info.m_lx, m_info.m_ly);
    }
    m_info.m_x1 = m_info.m_x0 + w;
    m_info.m_y1 = m_info.m_y0 + h;
  } else {
    m_info.m_x0 = m_info.m_y0 = 0;
    m_info.m_x1               = m_info.m_x0 + m_info.m_lx - 1;
    m_info.m_y1               = m_info.m_y0 + m_info.m_ly - 1;
  }
}

//------------------------------------------------------------

int TifReader::skipLines(int lineCount) {
  m_row += lineCount;
  return lineCount;
}

//------------------------------------------------------------

#include "timage_io.h"

void TifReader::readLine(short *buffer, int x0, int x1, int shrink) {
  assert(shrink > 0);

  const int pixelSize = 8;
  int stripRowSize    = m_rowLength * pixelSize;

  if (m_row < m_info.m_y0 || m_row > m_info.m_y1) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  int stripIndex = m_row / m_rowsPerStrip;
  if (m_stripIndex != stripIndex) {
    // Retrieve the strip holding current row. Please, observe that
    // TIFF functions will return the strip buffer in the BOTTOM-UP orientation,
    // no matter the internal tif's orientation storage

    m_stripIndex = stripIndex;

    if (TIFFIsTiled(m_tiff)) {
      // Retrieve tiles size
      uint32 tileWidth = 0, tileHeight = 0;
      TIFFGetField(m_tiff, TIFFTAG_TILEWIDTH, &tileWidth);
      TIFFGetField(m_tiff, TIFFTAG_TILELENGTH, &tileHeight);
      assert(tileWidth > 0 && tileHeight > 0);

      // Allocate a sufficient buffer to store a single tile
      int tileSize = tileWidth * tileHeight;
      std::unique_ptr<uint64[]> tile(new uint64[tileSize]);

      int x = 0;
      int y = tileHeight * m_stripIndex;

      // In case it's the last tiles row, the tile size might exceed the image
      // bounds
      int lastTy = std::min((int)tileHeight, m_info.m_ly - y);

      // Traverse the tiles row
      while (x < m_info.m_lx) {
        int ret = TIFFReadRGBATile_64(m_tiff, x, y, tile.get());
        assert(ret);

        int tileRowSize = std::min((int)tileWidth, m_info.m_lx - x) * pixelSize;

        // Copy the tile rows in the corresponding output strip rows
        for (int ty = 0; ty < lastTy; ++ty) {
          memcpy(m_stripBuffer + (ty * m_rowLength + x) * pixelSize,
                 (UCHAR *)tile.get() + ty * tileWidth * pixelSize, tileRowSize);
        }

        x += tileWidth;
      }
    } else {
      int y  = m_rowsPerStrip * m_stripIndex;
      int ok = TIFFReadRGBAStrip_64(m_tiff, y, (uint64 *)m_stripBuffer);
      assert(ok);
    }
  }

  uint16 orient = ORIENTATION_TOPLEFT;
  TIFFGetField(m_tiff, TIFFTAG_ORIENTATION, &orient);

  int r = m_rowsPerStrip - 1 - (m_row % m_rowsPerStrip);
  switch (orient)  // Pretty weak check for top/bottom orientation
  {
  case ORIENTATION_TOPLEFT:
  case ORIENTATION_TOPRIGHT:
  case ORIENTATION_LEFTTOP:
  case ORIENTATION_RIGHTTOP:
    // We have to invert the fixed BOTTOM-UP returned by TIFF functions - since
    // this function is
    // supposed to ignore orientation issues (which are managed outside).

    // The last tiles row will actually start at the END OF THE IMAGE (not
    // necessarily at
    // m_rowsPerStrip multiples). So, we must adjust for that.

    r = std::min(m_rowsPerStrip, m_info.m_ly - m_rowsPerStrip * m_stripIndex) -
        1 - (m_row % m_rowsPerStrip);
    break;

  case ORIENTATION_BOTRIGHT:
  case ORIENTATION_BOTLEFT:
  case ORIENTATION_RIGHTBOT:
  case ORIENTATION_LEFTBOT:
    r = m_row % m_rowsPerStrip;
    break;
  }

  // Finally, copy the strip row to the output row buffer
  TPixel64 *pix = (TPixel64 *)buffer;
  USHORT *v     = (USHORT *)(m_stripBuffer + r * stripRowSize);

  pix += x0;
  v += 4 * x0;

  int width =
      (x1 < x0) ? (m_info.m_lx - 1) / shrink + 1 : (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; i++) {
    USHORT c = *v++;
    pix->r   = c;
    c        = *v++;
    pix->g   = c;
    c        = *v++;
    pix->b   = c;
    c        = *v++;
    pix->m   = c;

    pix += shrink;
    v += 4 * (shrink - 1);
  }

  m_row++;
}

//===============================================================

void TifReader::readLine(char *buffer, int x0, int x1, int shrink) {
  if (this->m_info.m_bitsPerSample == 16 &&
      this->m_info.m_samplePerPixel >= 3) {
    std::vector<short> app(4 * (m_info.m_lx));
    readLine(&app[0], x0, x1, shrink);

    TPixel64 *pixin = (TPixel64 *)&app[0];

    TPixel32 *pixout = (TPixel32 *)buffer;
    for (int j = 0; j < x0; j++) {
      pixout++;
      pixin++;
    }

    for (int i = 0; i < (x1 - x0) + 1; i++)
      *pixout++ = PixelConverter<TPixel32>::from(*pixin++);

    return;
  }

  assert(shrink > 0);

  const int pixelSize = 4;
  int stripRowSize    = m_rowLength * pixelSize;

  if (m_row < m_info.m_y0 || m_row > m_info.m_y1) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  int stripIndex = m_row / m_rowsPerStrip;
  if (m_stripIndex != stripIndex) {
    m_stripIndex = stripIndex;

    if (TIFFIsTiled(m_tiff)) {
      uint32 tileWidth = 0, tileHeight = 0;
      TIFFGetField(m_tiff, TIFFTAG_TILEWIDTH, &tileWidth);
      TIFFGetField(m_tiff, TIFFTAG_TILELENGTH, &tileHeight);
      assert(tileWidth > 0 && tileHeight > 0);

      int tileSize = tileWidth * tileHeight;
      std::unique_ptr<uint32[]> tile(new uint32[tileSize]);

      int x = 0;
      int y = tileHeight * m_stripIndex;

      int lastTy = std::min((int)tileHeight, m_info.m_ly - y);

      while (x < m_info.m_lx) {
        int ret = TIFFReadRGBATile(m_tiff, x, y, tile.get());
        assert(ret);

        int tileRowSize =
            std::min((int)tileWidth, (int)(m_info.m_lx - x)) * pixelSize;

        for (int ty = 0; ty < lastTy; ++ty) {
          memcpy(m_stripBuffer + (ty * m_rowLength + x) * pixelSize,
                 (UCHAR *)tile.get() + ty * tileWidth * pixelSize, tileRowSize);
        }

        x += tileWidth;
      }
    } else {
      int y  = m_rowsPerStrip * m_stripIndex;
      int ok = TIFFReadRGBAStrip(m_tiff, y, (uint32 *)m_stripBuffer);
      assert(ok);
    }
  }

  uint16 orient = ORIENTATION_TOPLEFT;
  TIFFGetField(m_tiff, TIFFTAG_ORIENTATION, &orient);

  int r = m_rowsPerStrip - 1 - (m_row % m_rowsPerStrip);
  switch (orient)  // Pretty weak check for top/bottom orientation
  {
  case ORIENTATION_TOPLEFT:
  case ORIENTATION_TOPRIGHT:
  case ORIENTATION_LEFTTOP:
  case ORIENTATION_RIGHTTOP:
    // We have to invert the fixed BOTTOM-UP returned by TIFF functions - since
    // this function is
    // supposed to ignore orientation issues (which are managed outside).

    // The last tiles row will actually start at the END OF THE IMAGE (not
    // necessarily at
    // m_rowsPerStrip multiples). So, we must adjust for that.

    r = std::min(m_rowsPerStrip, m_info.m_ly - m_rowsPerStrip * m_stripIndex) -
        1 - (m_row % m_rowsPerStrip);
    break;

  case ORIENTATION_BOTRIGHT:
  case ORIENTATION_BOTLEFT:
  case ORIENTATION_RIGHTBOT:
  case ORIENTATION_LEFTBOT:
    r = m_row % m_rowsPerStrip;
    break;
  }

  TPixel32 *pix = (TPixel32 *)buffer;
  uint32 *v     = (uint32 *)(m_stripBuffer + r * stripRowSize);

  pix += x0;
  v += x0;

  int width =
      (x1 < x0) ? (m_info.m_lx - 1) / shrink + 1 : (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; i++) {
    uint32 c = *v;
    pix->r   = (UCHAR)TIFFGetR(c);
    pix->g   = (UCHAR)TIFFGetG(c);
    pix->b   = (UCHAR)TIFFGetB(c);
    pix->m   = (UCHAR)TIFFGetA(c);

    v += shrink;
    pix += shrink;
  }

  m_row++;
}

//============================================================

Tiio::TifWriterProperties::TifWriterProperties()
    : m_byteOrdering("Byte Ordering")
    , m_compressionType("Compression Type")
    , m_bitsPerPixel("Bits Per Pixel")
    , m_orientation("Orientation") {
  m_byteOrdering.addValue(L"IBM PC");
  m_byteOrdering.addValue(L"Mac");
#ifdef _WIN32
  m_byteOrdering.setValue(L"IBM PC");
#else
  m_byteOrdering.setValue(L"Mac");
#endif
  m_compressionType.addValue(TNZ_INFO_COMPRESS_LZW);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_NONE);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_PACKBITS);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_THUNDERSCAN);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_CCITTFAX3);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_CCITTFAX4);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_CCITTRLE);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_JPEG);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_OJPEG);

  m_compressionType.addValue(TNZ_INFO_COMPRESS_SGILOG);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_SGILOG24);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_ADOBE_DEFLATE);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_DEFLATE);
  m_compressionType.addValue(TNZ_INFO_COMPRESS_UNKNOWN);

  m_compressionType.setValue(TNZ_INFO_COMPRESS_LZW);

  m_bitsPerPixel.addValue(L"24(RGB)");
  m_bitsPerPixel.addValue(L"48(RGB)");
  m_bitsPerPixel.addValue(
      L" 1(BW)");  // WATCH OUT! If you reorder this remember to look for
  m_bitsPerPixel.addValue(L" 8(GREYTONES)");  // TRasterImage::isScanBW() usage
                                              // that bpp choice index
  // m_bitsPerPixel.addValue(L"16(GREYTONES)");                  // is HARDCODED
  // nearby...                   -.-'
  m_bitsPerPixel.addValue(L"32(RGBM)");
  m_bitsPerPixel.addValue(L"64(RGBM)");

  m_bitsPerPixel.setValue(L"32(RGBM)");

  m_orientation.addValue(TNZ_INFO_ORIENT_TOPLEFT);
  m_orientation.addValue(TNZ_INFO_ORIENT_TOPRIGHT);
  m_orientation.addValue(TNZ_INFO_ORIENT_BOTRIGHT);
  m_orientation.addValue(TNZ_INFO_ORIENT_BOTLEFT);
  m_orientation.addValue(TNZ_INFO_ORIENT_LEFTTOP);
  m_orientation.addValue(TNZ_INFO_ORIENT_RIGHTTOP);
  m_orientation.addValue(TNZ_INFO_ORIENT_RIGHTBOT);
  m_orientation.addValue(TNZ_INFO_ORIENT_LEFTBOT);

  // m_orientation.setValue(TNZ_INFO_ORIENT_TOPLEFT);

  bind(m_byteOrdering);
  bind(m_compressionType);
  bind(m_bitsPerPixel);
  bind(m_orientation);
}

void Tiio::TifWriterProperties::updateTranslation() {
  m_byteOrdering.setQStringName(tr("Byte Ordering"));
  m_compressionType.setQStringName(tr("Compression Type"));
  m_bitsPerPixel.setQStringName(tr("Bits Per Pixel"));
  m_bitsPerPixel.setItemUIName(L"24(RGB)", tr("24(RGB)"));
  m_bitsPerPixel.setItemUIName(L"48(RGB)", tr("48(RGB)"));
  m_bitsPerPixel.setItemUIName(L" 1(BW)", tr(" 1(BW)"));
  m_bitsPerPixel.setItemUIName(L" 8(GREYTONES)", tr(" 8(GREYTONES)"));
  m_bitsPerPixel.setItemUIName(L"32(RGBM)", tr("32(RGBM)"));
  m_bitsPerPixel.setItemUIName(L"64(RGBM)", tr("64(RGBM)"));
  m_orientation.setQStringName(tr("Orientation"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_TOPLEFT, tr("Top Left"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_TOPRIGHT, tr("Top Right"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_BOTRIGHT, tr("Bottom Right"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_BOTLEFT, tr("Bottom Left"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_LEFTTOP, tr("Left Top"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_RIGHTTOP, tr("Left Bottom"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_RIGHTBOT, tr("Right Top"));
  m_orientation.setItemUIName(TNZ_INFO_ORIENT_LEFTBOT, tr("Right Bottom"));
}

//============================================================

class TifWriter final : public Tiio::Writer {
  TIFF *m_tiff;
  int m_row;
  // Tiio::TifWriterProperties m_properties;
  unsigned char *m_lineBuffer;
  Tiio::RowOrder m_rowOrder;
  int m_bpp;
  int m_RightToLeft;
  void fillBits(UCHAR *bufout, UCHAR *bufin, int lx, int incr);

public:
  TifWriter();
  ~TifWriter();

  void open(FILE *file, const TImageInfo &info) override;
  void writeLine(char *buffer) override;
  void writeLine(short *buffer) override;

  void flush() override;

  Tiio::RowOrder getRowOrder() const override { return m_rowOrder; }
};

//------------------------------------------------------------

TifWriter::TifWriter()
    : m_tiff(0), m_row(-1), m_lineBuffer(0), m_RightToLeft(false) {
  TIFFSetWarningHandler(0);
}

//------------------------------------------------------------

TifWriter::~TifWriter() {
  if (m_tiff) TIFFClose(m_tiff);

  delete[] m_lineBuffer;
  delete m_properties;
}

//------------------------------------------------------------

void TifWriter::open(FILE *file, const TImageInfo &info) {
  m_info           = info;
  std::string mode = "w";

  if (!m_properties) m_properties = new Tiio::TifWriterProperties();

  std::wstring byteOrdering =
      ((TEnumProperty *)(m_properties->getProperty("Byte Ordering")))
          ->getValue();
  if (byteOrdering == L"IBM PC")
    mode += "l";
  else
    mode += "b";

  TEnumProperty *p =
      (TEnumProperty *)(m_properties->getProperty("Bits Per Pixel"));
  assert(p);
  std::string str = ::to_string(p->getValue());
  m_bpp           = atoi(str.c_str());
  assert(m_bpp == 1 || m_bpp == 8 || m_bpp == 16 || m_bpp == 24 ||
         m_bpp == 32 || m_bpp == 48 || m_bpp == 64);

  int fd = fileno(file);
#if 0
	m_tiff = TIFFFdOpenNoCloseProc(fd, "", mode.c_str());
#elif defined(_WIN32) && defined(__GNUC__)
  m_tiff = TIFFFdOpen((int)_get_osfhandle(dup(fd)), "", mode.c_str());
#else
  m_tiff = TIFFFdOpen(dup(fd), "", mode.c_str());
#endif
  if (!m_tiff) return;

  std::wstring worientation =
      ((TEnumProperty *)(m_properties->getProperty("Orientation")))->getValue();

  int orientation;
  if (worientation == TNZ_INFO_ORIENT_TOPLEFT)
    orientation = ORIENTATION_TOPLEFT;
  else if (worientation == TNZ_INFO_ORIENT_TOPRIGHT)
    orientation = ORIENTATION_TOPRIGHT;
  else if (worientation == TNZ_INFO_ORIENT_BOTRIGHT)
    orientation = ORIENTATION_BOTRIGHT;
  else if (worientation == TNZ_INFO_ORIENT_BOTLEFT)
    orientation = ORIENTATION_BOTLEFT;
  else if (worientation == TNZ_INFO_ORIENT_LEFTTOP)
    orientation = ORIENTATION_LEFTTOP;
  else if (worientation == TNZ_INFO_ORIENT_RIGHTTOP)
    orientation = ORIENTATION_RIGHTTOP;
  else if (worientation == TNZ_INFO_ORIENT_RIGHTBOT)
    orientation = ORIENTATION_RIGHTBOT;
  else if (worientation == TNZ_INFO_ORIENT_LEFTBOT)
    orientation = ORIENTATION_LEFTBOT;
  else
    assert(0);

  switch (orientation) {
  case ORIENTATION_TOPLEFT: /* row 0 top, col 0 lhs */
    m_rowOrder = Tiio::TOP2BOTTOM;
    break;
  case ORIENTATION_TOPRIGHT: /* row 0 top, col 0 rhs */
    m_rowOrder = Tiio::TOP2BOTTOM;
    break;
  case ORIENTATION_LEFTTOP: /* row 0 lhs, col 0 top */
    m_rowOrder = Tiio::TOP2BOTTOM;
    break;
  case ORIENTATION_RIGHTTOP: /* row 0 rhs, col 0 top */
    m_rowOrder = Tiio::TOP2BOTTOM;
    break;

  case ORIENTATION_BOTRIGHT: /* row 0 bottom, col 0 rhs */
    m_rowOrder = Tiio::BOTTOM2TOP;
    break;
  case ORIENTATION_BOTLEFT: /* row 0 bottom, col 0 lhs */
    m_rowOrder = Tiio::BOTTOM2TOP;
    break;
  case ORIENTATION_RIGHTBOT: /* row 0 rhs, col 0 bottom */
    m_rowOrder = Tiio::BOTTOM2TOP;
    break;
  case ORIENTATION_LEFTBOT: /* row 0 lhs, col 0 bottom */
    m_rowOrder = Tiio::BOTTOM2TOP;
    break;
  default:
    m_rowOrder = Tiio::TOP2BOTTOM;
    break;
  }
  m_RightToLeft = false;
  if (orientation == ORIENTATION_TOPRIGHT ||
      orientation == ORIENTATION_BOTRIGHT ||
      orientation == ORIENTATION_RIGHTTOP ||
      orientation == ORIENTATION_RIGHTBOT)
    m_RightToLeft = true;

  int bitsPerSample =
      (m_bpp == 1) ? 1 : ((m_bpp == 8 || m_bpp == 24 || m_bpp == 32) ? 8 : 16);

  TIFFSetField(m_tiff, TIFFTAG_IMAGEWIDTH, m_info.m_lx);
  TIFFSetField(m_tiff, TIFFTAG_IMAGELENGTH, m_info.m_ly);
  TIFFSetField(m_tiff, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
  TIFFSetField(m_tiff, TIFFTAG_SAMPLESPERPIXEL, m_bpp / bitsPerSample);
  TIFFSetField(m_tiff, TIFFTAG_ORIENTATION, orientation);

  if (m_bpp == 1)
    TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX4);
  else {
    std::wstring compressionType =
        ((TEnumProperty *)(m_properties->getProperty("Compression Type")))
            ->getValue();
    if (compressionType == TNZ_INFO_COMPRESS_LZW)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    else if (compressionType == TNZ_INFO_COMPRESS_PACKBITS)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_PACKBITS);
    else if (compressionType == TNZ_INFO_COMPRESS_CCITTFAX3)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX3);
    else if (compressionType == TNZ_INFO_COMPRESS_CCITTFAX4)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX4);
    else if (compressionType == TNZ_INFO_COMPRESS_CCITTRLE)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_CCITTRLE);
    else if (compressionType == TNZ_INFO_COMPRESS_JPEG)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
    else if (compressionType == TNZ_INFO_COMPRESS_OJPEG)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_OJPEG);
    else if (compressionType == TNZ_INFO_COMPRESS_NONE)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    else if (compressionType == TNZ_INFO_COMPRESS_THUNDERSCAN)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_THUNDERSCAN);
    else if (compressionType == TNZ_INFO_COMPRESS_ADOBE_DEFLATE)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE);
    else if (compressionType == TNZ_INFO_COMPRESS_DEFLATE)
      TIFFSetField(m_tiff, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    else
      assert(false);
  }
  TIFFSetField(m_tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(m_tiff, TIFFTAG_PHOTOMETRIC, (m_bpp == 8 || m_bpp == 1)
                                                ? PHOTOMETRIC_MINISBLACK
                                                : PHOTOMETRIC_RGB);
  TIFFSetField(m_tiff, TIFFTAG_XRESOLUTION, m_info.m_dpix);
  TIFFSetField(m_tiff, TIFFTAG_YRESOLUTION, m_info.m_dpiy);
  TIFFSetField(m_tiff, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
  TIFFSetField(m_tiff, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(m_tiff, 0));

  m_row = 0;
  if (m_bpp == 1)
    m_lineBuffer = new unsigned char[m_info.m_lx / 8 + 1];
  else
    m_lineBuffer =
        new unsigned char[(m_bpp == 1 ? 1 : m_bpp / 8) * m_info.m_lx];
}

//------------------------------------------------------------

void TifWriter::flush() { TIFFFlush(m_tiff); }

//------------------------------------------------------------

void TifWriter::writeLine(short *buffer) {
  int delta = 1;
  int start = 0;
  if (m_RightToLeft) {
    delta = -1;
    start = m_info.m_lx - 1;
  }
  if (m_bpp == 16) {
    unsigned short *pix = ((unsigned short *)buffer) + start;
    for (int i = 0; i < m_info.m_lx; i++) {
      unsigned short *b = (unsigned short *)m_lineBuffer + i * 2;
      b[0]              = pix[0];
      b[1]              = pix[1];
      pix               = pix + delta;
    }
  } else {
    assert(m_bpp == 48 || m_bpp == 64);
    TPixel64 *pix = ((TPixel64 *)buffer) + start;

    if (m_bpp == 64)
      for (int i = 0; i < m_info.m_lx; i++) {
        unsigned short *b = (unsigned short *)m_lineBuffer + i * 4;
        b[0]              = pix->r;
        b[1]              = pix->g;
        b[2]              = pix->b;
        b[3]              = pix->m;
        pix               = pix + delta;
      }
    else if (m_bpp == 48)
      for (int i = 0; i < m_info.m_lx; i++) {
        unsigned short *b = (unsigned short *)m_lineBuffer + i * 3;
        b[0]              = pix->r;
        b[1]              = pix->g;
        b[2]              = pix->b;
        pix               = pix + delta;
      }
  }
  TIFFWriteScanline(m_tiff, m_lineBuffer, m_row++, 0);
}

//------------------------------------------------------------

void TifWriter::fillBits(UCHAR *bufout, UCHAR *bufin, int lx, int incr) {
  int lx1 = lx / 8 + ((lx % 8) ? 1 : 0);

  for (int i = 0; i < lx1; i++, bufout++) {
    UCHAR pix = 0xff;
    for (int j = 0; j < 8; j++, bufin += incr)
      if (*bufin < m_bwThreshold) pix &= ~(0x1 << (7 - j));
    *bufout = pix;
  }
}

//------------------------------------------------------

void TifWriter::writeLine(char *buffer) {
  int delta = 1;
  int start = 0;
  if (m_RightToLeft) {
    delta = -1;
    start = m_info.m_lx - 1;
  }
  if (m_bpp == 1)
    fillBits(m_lineBuffer, ((unsigned char *)buffer) + start, m_info.m_lx,
             delta);
  else if (m_bpp == 8) {
    unsigned char *pix = ((unsigned char *)buffer) + start;
    for (int i = 0; i < m_info.m_lx; i++) {
      unsigned char *b = m_lineBuffer + i;
      b[0]             = pix[0];
      pix              = pix + delta;
    }
  } else {
    assert(m_bpp == 24 || m_bpp == 32);
    TPixel32 *pix = ((TPixel32 *)buffer) + start;

    if (m_bpp == 32)
      for (int i = 0; i < m_info.m_lx; i++) {
        unsigned char *b = m_lineBuffer + i * 4;
        b[0]             = pix->r;
        b[1]             = pix->g;
        b[2]             = pix->b;
        b[3]             = pix->m;
        pix              = pix + delta;
      }
    else if (m_bpp == 24)
      for (int i = 0; i < m_info.m_lx; i++) {
        unsigned char *b = m_lineBuffer + i * 3;
        b[0]             = pix->r;
        b[1]             = pix->g;
        b[2]             = pix->b;
        pix              = pix + delta;
      }
  }
  TIFFWriteScanline(m_tiff, m_lineBuffer, m_row++, 0);
}

//============================================================
#ifdef _DEBUG
/* Error & Waring Handler per debug */

extern "C" {
static void MyWarningHandler(const char *module, const char *fmt, va_list ap) {
  std::string outMsg;
  char msg[2048];
  msg[0]                     = 0;
  if (module != NULL) outMsg = std::string(module);
  outMsg += "Warning, ";

  _vsnprintf(msg, 2048, fmt, ap);
  strcat(msg, ".\n");

  outMsg += msg;
  TSystem::outputDebug(outMsg);
}

static void MyErrorHandler(const char *module, const char *fmt, va_list ap) {
  std::string outMsg;
  char msg[2048];
  msg[0]                     = 0;
  if (module != NULL) outMsg = std::string(module);
  // outMsg += "Warning, ";

  _vsnprintf(msg, 2048, fmt, ap);
  strcat(msg, ".\n");

  outMsg += msg;
  TSystem::outputDebug(outMsg);
}
}

#endif

Tiio::Reader *Tiio::makeTifReader() {
#ifdef _DEBUG
  TIFFSetErrorHandler(MyErrorHandler);
  TIFFSetWarningHandler(MyWarningHandler);
#endif
  return new TifReader(false);
}

//------------------------------------------------------------

Tiio::Reader *Tiio::makeTziReader() {
#ifdef _DEBUG
  TIFFSetErrorHandler(MyErrorHandler);
  TIFFSetWarningHandler(MyWarningHandler);
#endif
  return new TifReader(true);
}

//------------------------------------------------------------

Tiio::Writer *Tiio::makeTifWriter() {
#ifdef _DEBUG
  TIFFSetErrorHandler(MyErrorHandler);
  TIFFSetWarningHandler(MyWarningHandler);
#endif

  return new TifWriter();
}
