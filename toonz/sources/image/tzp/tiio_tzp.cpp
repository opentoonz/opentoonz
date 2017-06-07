

#include "tiio_tzp.h"
//#include "tiio.h"
#include "trastercm.h"
#include "toonztags.h"
#include "texception.h"

#include "tiffio.h"
#include "tiffiop.h"
//#include "tspecialstyleid.h"
#include <set>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include "windows.h"
#endif

//============================================================

class TzpReader final : public Tiio::Reader {
  TIFF *m_tiff;
  int m_row;
  bool m_tiled, m_stripped;
  int m_rowsPerStrip;
  int m_stripIndex;
  int m_rowLength;
  UCHAR *m_stripBuffer;
  int m_x, m_y, m_lx, m_ly;
  bool m_isCmap24;
  std::string m_history;
  int m_nColor;
  int m_nPencil;
  bool m_isBigEndian;
  bool m_isFirstLineRead;

public:
  TzpReader();
  ~TzpReader();

  void open(FILE *file) override;

  Tiio::RowOrder getRowOrder() const override { return Tiio::BOTTOM2TOP; }

  int skipLines(int lineCount) override;
  void readLine(char *buffer, int x0, int x1, int shrink) override;
};

//------------------------------------------------------------

TzpReader::TzpReader()
    : m_tiff(0)
    , m_row(0)
    , m_rowsPerStrip(0)
    , m_stripIndex(-1)
    , m_stripBuffer(0)
    , m_rowLength(0)
    , m_x(0)
    , m_y(0)
    , m_lx(0)
    , m_ly(0)
    , m_isCmap24(false)
    , m_nColor(0)
    , m_nPencil(0)
    , m_isBigEndian(false)
    , m_isFirstLineRead(true) {}

//------------------------------------------------------------

TzpReader::~TzpReader() { delete m_stripBuffer; }

//------------------------------------------------------------

void TzpReader::open(FILE *file) {
  int fd = fileno(file);
  TIFFErrorHandler oldhandler;
  oldhandler = TIFFSetWarningHandler(NULL);
  m_tiff     = TIFFFdOpen(fd, "", "rb");
  TIFFSetWarningHandler(oldhandler);
  if (!m_tiff) return;

  uint32 w = 0, h = 0, bps = 0, spp = 0, rps = 0;
  uint32 tileWidth = 0, tileLength = 0;

  TIFFGetField(m_tiff, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(m_tiff, TIFFTAG_IMAGELENGTH, &h);
  TIFFGetField(m_tiff, TIFFTAG_BITSPERSAMPLE, &bps);
  TIFFGetField(m_tiff, TIFFTAG_SAMPLESPERPIXEL, &spp);
  TIFFGetField(m_tiff, TIFFTAG_ROWSPERSTRIP, &rps);

  float xdpi, ydpi;
  TIFFGetField(m_tiff, TIFFTAG_XRESOLUTION, &xdpi);
  TIFFGetField(m_tiff, TIFFTAG_YRESOLUTION, &ydpi);
  m_info.m_dpix = xdpi;
  m_info.m_dpiy = ydpi;

  TIFFGetField(m_tiff, TIFFTAG_TILEWIDTH, &tileWidth);
  TIFFGetField(m_tiff, TIFFTAG_TILELENGTH, &tileLength);

  uint32 risCount  = 0;
  USHORT *risArray = 0;

  m_info.m_lx = w;
  m_info.m_ly = h;
  m_x         = 0;
  m_y         = 0;
  m_lx        = w;
  m_ly        = h;

  if (TIFFGetField(m_tiff, TIFFTAG_TOONZWINDOW, &risCount, &risArray)) {
    m_info.m_x0 = m_x = risArray[0];
    m_info.m_y0 = m_y = risArray[1];
    m_info.m_lx       = risArray[2];
    m_info.m_ly       = risArray[3];
  } else {
    m_info.m_x0 = 0;
    m_info.m_y0 = 0;
  }

  m_info.m_x1 = m_info.m_x0 + w;
  m_info.m_y1 = m_info.m_y0 + h;

  if (TIFFIsTiled(m_tiff)) {
    m_rowsPerStrip  = tileLength;
    int tilesPerRow = (w + tileWidth - 1) / tileWidth;
    m_rowLength     = tileWidth * tilesPerRow;
    int stripSize   = m_rowsPerStrip * m_rowLength * 4;
    m_stripBuffer   = new UCHAR[stripSize];
  } else {
    m_rowsPerStrip = rps;
    int stripSize  = rps * w * 4 + 4096;  // TIFFStripSize(m_tiff);

    m_stripBuffer = new UCHAR[stripSize];
    m_rowLength   = w;
  }

  uint32 paletteCount;
  USHORT *palette;

  TIFFGetField(m_tiff, TIFFTAG_TOONZPALETTE, &paletteCount, &palette);

  m_nColor  = palette[10];
  m_nPencil = palette[11];

  if (m_nColor == 128 && m_nPencil == 32) {
    throw TException("Old 4.1 Palette");
  }

  if (bps == 32)
    m_isCmap24 = true;
  else
    m_isCmap24 = false;

  char *data;
  uint32 count;
  TIFFGetField(m_tiff, TIFFTAG_TOONZHISTORY, &count, &data);
  std::string history(data);

  uint16 planarconfig;
  TIFFGetField(m_tiff, TIFFTAG_PLANARCONFIG, &planarconfig);
  if (planarconfig == PLANARCONFIG_SEPARATE) {
    // tmsg_error("separate buffer image not supported yet in .tz(up) files");
  }
}

//------------------------------------------------------------

int TzpReader::skipLines(int lineCount) {
  // m_row += lineCount;
  return 0;
}

//------------------------------------------------------------

extern int ComboInkIndex[];  // a bad patch....

void TzpReader::readLine(char *buffer, int x0, int x1, int shrink) {
  TPixelCM32 *pix = (TPixelCM32 *)buffer;
  for (int i = 0; i < m_info.m_lx; i++) pix[i] = TPixelCM32();
  int y                                        = m_row++;

  const int paintOffset = 0;                 // paint#1 --> 1
  const int inkOffset   = 1 + m_nColor - 1;  // ink#0 --> nColor-1
  // (n.b. c'e' un colore in meno visto che il primo colore e' il BG)

  if (m_isCmap24) {
    if (m_y <= y && y < m_y + m_ly) {
      std::vector<TUINT32> line(m_lx);
      TIFFReadScanline(m_tiff, (char *)&line[0], y - m_y, 0);
      if (m_isFirstLineRead) {
        m_isFirstLineRead = false;
        for (int i = 0; i < m_lx; i++)
          if ((line[i] >> 24) != 0) {
            m_isBigEndian = true;
            break;
          }
      }

      pix += m_x;
      for (int i = 0; i < m_lx; i++) {
        TUINT32 inPix = m_isBigEndian ? swapTINT32(line[i]) : line[i];
        int tone      = inPix & 0xFF;
        int paint     = ((inPix >> 8) & 0xFF);
        int ink       = ((inPix >> 16) & 0xFF);
        if (paint > 0) paint += paintOffset;
        if (ComboInkIndex[ink] != -1)
          ink = ComboInkIndex[ink];
        else
          ink += inkOffset;
        pix[i] = TPixelCM32(ink, paint, tone);
      }
    }
  } else {
    if (m_y <= y && y < m_y + m_ly) {
      std::vector<unsigned short> line(m_lx);
      TIFFReadScanline(m_tiff, (char *)&line[0], y - m_y, 0);
      pix += m_x;
      static std::set<int> table;

      /// per le tzp che vengono da Irix
      bool bigEndian =
          (m_tiff->tif_header.classic.tiff_magic == TIFF_BIGENDIAN);

      for (int i = 0; i < m_lx; i++) {
        unsigned short inPix = line[i];
        if (bigEndian) inPix = swapUshort(inPix);

        int tone  = inPix & 0xF;
        int paint = ((inPix >> 4) & 0x7F);
        int ink   = ((inPix >> 11) & 0x1F);
        tone |= tone << 4;
        if (paint > 0) paint += paintOffset;
        ink += inkOffset;
        pix[i] = TPixelCM32(ink, paint, tone);
        if (tone < 255) {
          //           int w = ink;
        }
        if (table.find(ink) == table.end()) {
          table.insert(ink);
        }
      }
    }
  }
}

//============================================================

Tiio::Reader *Tiio::makeTzpReader() { return new TzpReader(); }

//------------------------------------------------------------

Tiio::Writer *Tiio::makeTzpWriter() {
  assert(0);
  return 0;
}

//------------------------------------------------------------
