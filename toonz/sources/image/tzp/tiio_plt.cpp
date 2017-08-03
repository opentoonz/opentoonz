

#include "tiio_plt.h"
//#include "tiio.h"
#include "trastercm.h"
#include "toonztags.h"

#include "tiffio.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef _WIN32
#include "windows.h"
#endif

extern "C" {
#include "avl.h"
#include "tcm.h"
}

//============================================================

class PltReader final : public Tiio::Reader {
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
  int m_pltType;  //[1..4]
  int m_nColor;
  int m_nPencil;
  std::vector<TPixel32> m_infoRow;
  std::map<int, std::pair<std::string, std::string>>
      m_pltNames;  // colorIndex(<256: paint) , pageName, colorName
public:
  PltReader();
  ~PltReader();

  void open(FILE *file) override;

  Tiio::RowOrder getRowOrder() const override { return Tiio::BOTTOM2TOP; }

  int skipLines(int lineCount) override;
  void readLine(char *buffer, int x0, int x1, int shrink) override;

  void getTzpPaletteColorNames(
      std::map<int, std::pair<std::string, std::string>> &pltColorNames)
      const override;
};

//------------------------------------------------------------

PltReader::PltReader()
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
    , m_pltType(-1)
    , m_nColor(0)
    , m_nPencil(0) {}

//------------------------------------------------------------

PltReader::~PltReader() { delete m_stripBuffer; }

//------------------------------------------------------------

static int decode_group_name(char group_name[], char **name, int *key,
                             int *sister_index) {
  char *s, *t;
  *key          = 0;
  *sister_index = -1;
  s             = group_name;
  while (1) {
    t = strchr(s, '!');
    if (!t) break;
    *t = '\0';
    if (s[0] == 'C' && s[1] >= '0' && s[1] <= '9')
      *sister_index = atoi(s + 1);
    else if (s[0] >= '0' && s[0] <= '9')
      *key = atoi(s);
    *t     = '!';
    s      = t + 1;
  }
  *name = s;
  return 1;
}

void PltReader::getTzpPaletteColorNames(
    std::map<int, std::pair<std::string, std::string>> &pltColorNames) const {
  pltColorNames = m_pltNames;
}

//--------------------------------------------------------------------------------------
namespace {
bool isDefaultName(const std::string &_name) {
  int i;
  QString name = QString::fromStdString(_name);
  if (name.startsWith("Ink_")) {
    for (i = 4; i < name.size(); i++)
      if (!name.at(i).isDigit()) return false;
    return true;
  } else if (name.startsWith("Paint_")) {
    for (i = 6; i < name.size(); i++)
      if (!name.at(i).isDigit()) return false;
    return true;
  } else
    return false;
}

}  // namespace
//------------------------------------------------------------

bool PaletteRead = false;
int ComboInkIndex[256];  // a bad patch....

void PltReader::open(FILE *file) {
  char *data;
  uint32 count;
  int i;
  for (i = 0; i < 256; i++) ComboInkIndex[i] = -1;

  int fd = fileno(file);

  TIFFErrorHandler oldhandler;
  oldhandler = TIFFSetWarningHandler(NULL);
  m_tiff     = TIFFFdOpen(fd, "", "rb");
  TIFFSetWarningHandler(oldhandler);

  if (!m_tiff) return;

  uint32 w = 0, h = 0, rps = 0;
  uint32 tileWidth = 0, tileLength = 0;
  uint16 bps = 0, spp = 0;

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

  assert(h == 1);
  m_ly = m_info.m_ly = 2;  // per l'infoRow

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
  assert(paletteCount);

  m_pltType = palette[0];

  m_nColor  = palette[10];
  m_nPencil = palette[11];

  std::string colorNames;
  if (TIFFGetField(m_tiff, TIFFTAG_TOONZCOLORNAMES, &count, &data))
    colorNames = data;

  TREE *names = cdb_decode_all(data, Tcm_24_default_info);

  CDB_TREE_ITEM *item;

  char *pageName;
  int key, sisterIndex;

  int maxCount = m_nColor + m_nPencil;
  m_infoRow.resize(maxCount, TPixel32(0, 0, 0, 0));

  for (i = 0; i < maxCount; i++) {
    item = (CDB_TREE_ITEM *)avl_locate(names, i);
    if (!item) continue;
    decode_group_name(item->group, &pageName, &key, &sisterIndex);

    if (strcmp(pageName, "_UNUSED_PAGE") == 0) continue;
    if (sisterIndex == -1 || i < m_nColor) m_infoRow[i].r = 255;
    if (i < m_nColor) m_infoRow[i].g                      = 255;

    while (isdigit(item->name[0]))  // in toonz colors cannot begin with digits
      item->name++;

    std::map<int, std::pair<std::string, std::string>>::iterator it;
    if ((it = m_pltNames.find(i)) == m_pltNames.end() ||
        isDefaultName(it->second.second))
      m_pltNames[i] = std::pair<std::string, std::string>(pageName, item->name);

    if (sisterIndex != -1) {
      int comboindex = (i < 256) ? (sisterIndex >> 16) + 256 : sisterIndex >> 8;
      if (i >= 256) ComboInkIndex[i - 256] = comboindex;
      std::map<int, std::pair<std::string, std::string>>::iterator it;
      if ((it = m_pltNames.find(comboindex)) == m_pltNames.end() ||
          isDefaultName(it->second.second)) {
        m_pltNames[comboindex] =
            std::pair<std::string, std::string>(pageName, item->name);
        if (comboindex < m_nColor)
          m_infoRow[comboindex].r = m_infoRow[comboindex].g = 255;
      }
    }
  }

  if (TIFFGetField(m_tiff, TIFFTAG_TOONZHISTORY, &count, &data))
    m_history = data;

  uint16 photometric, planarconfig;

  TIFFGetField(m_tiff, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetField(m_tiff, TIFFTAG_PLANARCONFIG, &planarconfig);

  if (photometric != PHOTOMETRIC_RGB || planarconfig != PLANARCONFIG_CONTIG) {
    // tmsg_error("bad !");
  }
}

//------------------------------------------------------------

int PltReader::skipLines(int lineCount) {
  // m_row+=lineCount;
  return lineCount;
}

//------------------------------------------------------------

void PltReader::readLine(char *buffer, int x0, int x1, int shrink) {
  TPixelRGBM32 *pix = (TPixelRGBM32 *)buffer;
  int i;

  for (i = 0; i < m_info.m_lx; i++) pix[i] = TPixelRGBM32();
  int y                                    = m_row++;

  if (y == 1) {
    for (i = 0; i < m_info.m_lx; i++) pix[i] = m_infoRow[i];
    return;
  } else if (y > 1)
    return;

  unsigned char line[4096 * 4];

  TIFFReadScanline(m_tiff, (char *)line, y - m_y, 0);

  switch (m_pltType) {
  case 1:
    throw "Unsupported palette type";
    break;

  case 2:
  case 4:
    for (i = 0; i < (m_nColor + m_nPencil) * 4;
         i += 4)  // prima i color, poi i pencil
    {
      pix->r = line[i + 0];
      pix->g = line[i + 1];
      pix->b = line[i + 2];
      pix->m = line[i + 3];
      ++pix;
    }
    break;

  case 3:
    for (i = 0; i < (m_nColor + m_nPencil) * 4;
         i += 4)  // prima i color, poi i pencil
    {
      pix->m = line[i + 0]; /* different from type 2 */
      pix->b = line[i + 1];
      pix->g = line[i + 2];
      pix->r = line[i + 3];
      ++pix;
    }
    break;
  default:
    throw "Unknown palette type";
    break;
  }
}

//============================================================

Tiio::Reader *Tiio::makePltReader() { return new PltReader(); }

//------------------------------------------------------------

Tiio::Writer *Tiio::makePltWriter() {
  assert(0);
  return 0;
}

//------------------------------------------------------------
