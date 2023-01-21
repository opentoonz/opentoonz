#define TINYEXR_USE_MINIZ 0
#include "zlib.h"

#define TINYEXR_OTMOD_IMPLEMENTATION
#include "tinyexr_otmod.h"

#include "tiio_exr.h"
#include "tpixel.h"

#include <QMap>
#include <QString>

namespace {
inline unsigned char ftouc(float f, float gamma = 2.2f) {
  int i = static_cast<int>(255.0f * powf(f, 1.0f / gamma));
  if (i > 255) i = 255;
  if (i < 0) i = 0;

  return static_cast<unsigned char>(i);
}
inline float uctof(unsigned char uc, float gamma = 2.2f) {
  return powf(static_cast<float>(uc) / 255.0f, gamma);
}

inline unsigned short ftous(float f, float gamma = 2.2f) {
  int i = static_cast<int>(65535.0f * powf(f, 1.0f / gamma));
  if (i > 65535) i = 65535;
  if (i < 0) i = 0;

  return static_cast<unsigned short>(i);
}
inline float ustof(unsigned short us, float gamma = 2.2f) {
  return powf(static_cast<float>(us) / 65535.0f, gamma);
}

inline float toNonlinear(float f, float gamma = 2.2f) {
  if (f < 0.f) return f;
  return std::pow(f, 1.f / gamma);
}

inline float toLinear(float f, float gamma = 2.2f) {
  if (f < 0.f) return f;
  return std::pow(f, gamma);
}

const QMap<int, std::wstring> ExrCompTypeStr = {
    {TINYEXR_COMPRESSIONTYPE_NONE, L"None"},
    {TINYEXR_COMPRESSIONTYPE_RLE, L"RLE"},
    {TINYEXR_COMPRESSIONTYPE_ZIPS, L"ZIPS"},
    {TINYEXR_COMPRESSIONTYPE_ZIP, L"ZIP"},
    {TINYEXR_COMPRESSIONTYPE_PIZ, L"PIZ"}};

const std::wstring EXR_STORAGETYPE_SCANLINE = L"Store Image as Scanlines";
const std::wstring EXR_STORAGETYPE_TILE     = L"Store Image as Tiles";
}  // namespace

//**************************************************************************
//    ExrReader  implementation
//**************************************************************************

class ExrReader final : public Tiio::Reader {
  float* m_rgbaBuf;
  int m_row;
  EXRHeader* m_exr_header;
  FILE* m_fp;

  float m_colorSpaceGamma;

public:
  ExrReader();
  ~ExrReader();

  void open(FILE* file) override;
  Tiio::RowOrder getRowOrder() const override;
  bool read16BitIsEnabled() const override;
  int skipLines(int lineCount) override;
  ;
  void readLine(char* buffer, int x0, int x1, int shrink) override;
  void readLine(short* buffer, int x0, int x1, int shrink) override;
  void readLine(float* buffer, int x0, int x1, int shrink) override;
  void loadImage();
  void setColorSpaceGamma(const double gamma) override {
    assert(gamma > 0);
    m_colorSpaceGamma = static_cast<float>(gamma);
  }
};

ExrReader::ExrReader()
    : m_rgbaBuf(nullptr)
    , m_row(0)
    , m_exr_header(nullptr)
    , m_colorSpaceGamma(2.2f) {}

ExrReader::~ExrReader() {
  if (m_rgbaBuf) free(m_rgbaBuf);
  if (m_exr_header) FreeEXRHeader(m_exr_header);
}

void ExrReader::open(FILE* file) {
  m_fp         = file;
  m_exr_header = new EXRHeader();
  const char* err;
  {
    int ret = LoadEXRHeaderFromFileHandle(*m_exr_header, file, &err);
    if (ret != 0) {
      m_exr_header = nullptr;
      throw(std::string(err));
    }
  }
  m_info.m_lx =
      m_exr_header->data_window.max_x - m_exr_header->data_window.min_x + 1;
  m_info.m_ly =
      m_exr_header->data_window.max_y - m_exr_header->data_window.min_y + 1;

  m_info.m_samplePerPixel = m_exr_header->num_channels;

  int bps = 16;
  switch (m_exr_header->pixel_types[0]) {
  case TINYEXR_PIXELTYPE_UINT:
  case TINYEXR_PIXELTYPE_FLOAT:
    bps = 32;
    break;
  case TINYEXR_PIXELTYPE_HALF:
    // bps = 16;
    bps = 32;  // set 32bps in order to return float raster
    break;
  }
  m_info.m_bitsPerSample = bps;
}

Tiio::RowOrder ExrReader::getRowOrder() const { return Tiio::TOP2BOTTOM; }

bool ExrReader::read16BitIsEnabled() const { return true; }

int ExrReader::skipLines(int lineCount) {
  m_row += lineCount;
  return lineCount;
}

void ExrReader::loadImage() {
  assert(!m_rgbaBuf);
  const char* err;
  {
    int ret =
        LoadEXRImageBufFromFileHandle(&m_rgbaBuf, *m_exr_header, m_fp, &err);
    if (ret != 0) {
      m_exr_header = nullptr;
      throw(std::string(err));
    }
  }
  // header memory is freed after loading image
  m_exr_header = nullptr;
}

void ExrReader::readLine(char* buffer, int x0, int x1, int shrink) {
  const int pixelSize = 4;
  if (m_row < 0 || m_row >= m_info.m_ly) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  if (!m_rgbaBuf) loadImage();

  TPixel32* pix = (TPixel32*)buffer;
  float* v      = m_rgbaBuf + m_row * m_info.m_lx * 4;

  pix += x0;
  v += x0 * 4;

  int width =
      (x1 < x0) ? (m_info.m_lx - 1) / shrink + 1 : (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; i++) {
    pix->r = ftouc(v[0], m_colorSpaceGamma);
    pix->g = ftouc(v[1], m_colorSpaceGamma);
    pix->b = ftouc(v[2], m_colorSpaceGamma);
    pix->m = ftouc(v[3], 1.0f);

    v += shrink * 4;
    pix += shrink;
  }

  m_row++;
}

void ExrReader::readLine(short* buffer, int x0, int x1, int shrink) {
  const int pixelSize = 8;
  if (m_row < 0 || m_row >= m_info.m_ly) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  if (!m_rgbaBuf) loadImage();

  TPixel64* pix = (TPixel64*)buffer;
  float* v      = m_rgbaBuf + m_row * m_info.m_lx * 4;

  pix += x0;
  v += x0 * 4;

  int width =
      (x1 < x0) ? (m_info.m_lx - 1) / shrink + 1 : (x1 - x0) / shrink + 1;

  for (int i = 0; i < width; i++) {
    pix->r = ftous(v[0], m_colorSpaceGamma);
    pix->g = ftous(v[1], m_colorSpaceGamma);
    pix->b = ftous(v[2], m_colorSpaceGamma);
    pix->m = ftous(v[3], 1.0f);

    v += shrink * 4;
    pix += shrink;
  }

  m_row++;
}

void ExrReader::readLine(float* buffer, int x0, int x1, int shrink) {
  const int pixelSize = 16;
  if (m_row < 0 || m_row >= m_info.m_ly) {
    memset(buffer, 0, (x1 - x0 + 1) * pixelSize);
    m_row++;
    return;
  }

  if (!m_rgbaBuf) loadImage();

  TPixelF* pix = (TPixelF*)buffer;
  float* v     = m_rgbaBuf + m_row * m_info.m_lx * 4;

  pix += x0;
  v += x0 * 4;

  int width =
      (x1 < x0) ? (m_info.m_lx - 1) / shrink + 1 : (x1 - x0) / shrink + 1;
  // いったんノンリニアで読み込む。リニア計算のときはあとでリニアに戻す
  for (int i = 0; i < width; i++) {
    pix->r = toNonlinear(v[0], m_colorSpaceGamma);
    pix->g = toNonlinear(v[1], m_colorSpaceGamma);
    pix->b = toNonlinear(v[2], m_colorSpaceGamma);
    pix->m = toNonlinear(v[3], 1.0f);

    v += shrink * 4;
    pix += shrink;
  }

  m_row++;
}

//============================================================

Tiio::ExrWriterProperties::ExrWriterProperties()
    : m_compressionType("Compression Type")
    , m_storageType("Storage Type")
    , m_bitsPerPixel("Bits Per Pixel")
    , m_colorSpaceGamma("Color Space Gamma", 0.1, 10.0, 2.2) {
  // internally handles float raster
  m_bitsPerPixel.addValue(L"96(RGB)_HF");
  m_bitsPerPixel.addValue(L"128(RGBA)_HF");
  m_bitsPerPixel.addValue(L"96(RGB)_F");
  m_bitsPerPixel.addValue(L"128(RGBA)_F");
  m_bitsPerPixel.setValue(L"128(RGBA)_HF");
  // m_bitsPerPixel.addValue(L"48(RGB)");
  // m_bitsPerPixel.addValue(L"64(RGBA)");
  // m_bitsPerPixel.setValue(L"64(RGBA)");

  m_compressionType.addValue(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_NONE));
  m_compressionType.addValue(ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_RLE));
  m_compressionType.addValue(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIPS));
  m_compressionType.addValue(ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIP));
  m_compressionType.addValue(ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_PIZ));

  m_compressionType.setValue(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_NONE));

  m_storageType.addValue(EXR_STORAGETYPE_SCANLINE);
  m_storageType.addValue(EXR_STORAGETYPE_TILE);
  m_storageType.setValue(EXR_STORAGETYPE_SCANLINE);

  bind(m_bitsPerPixel);
  bind(m_compressionType);
  bind(m_storageType);
  bind(m_colorSpaceGamma);
}

void Tiio::ExrWriterProperties::updateTranslation() {
  m_bitsPerPixel.setQStringName(tr("Bits Per Pixel"));
  // internally handles float raster
  m_bitsPerPixel.setItemUIName(L"96(RGB)_HF", tr("48(RGB Half Float)"));
  m_bitsPerPixel.setItemUIName(L"128(RGBA)_HF", tr("64(RGBA Half Float)"));
  m_bitsPerPixel.setItemUIName(L"96(RGB)_F", tr("96(RGB Float)"));
  m_bitsPerPixel.setItemUIName(L"128(RGBA)_F", tr("128(RGBA Float)"));
  // m_bitsPerPixel.setItemUIName(L"48(RGB)", tr("48(RGB Half Float)"));
  // m_bitsPerPixel.setItemUIName(L"64(RGBA)", tr("64(RGBA Half Float)"));

  m_compressionType.setQStringName(tr("Compression Type"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_NONE), tr("No compression"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_RLE),
      tr("Run Length Encoding (RLE)"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIPS),
      tr("ZIP compression per Scanline (ZIPS)"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_ZIP),
      tr("ZIP compression per scanline band (ZIP)"));
  m_compressionType.setItemUIName(
      ExrCompTypeStr.value(TINYEXR_COMPRESSIONTYPE_PIZ),
      tr("PIZ-based wavelet compression (PIZ)"));

  m_storageType.setQStringName(tr("Storage Type"));
  m_storageType.setItemUIName(EXR_STORAGETYPE_SCANLINE, tr("Scan-line based"));
  m_storageType.setItemUIName(EXR_STORAGETYPE_TILE, tr("Tile based"));

  m_colorSpaceGamma.setQStringName(tr("Color Space Gamma"));
}

//============================================================

class ExrWriter final : public Tiio::Writer {
  std::vector<float> m_imageBuf[4];
  EXRHeader m_header;
  EXRImage m_image;
  int m_row;
  FILE* m_fp;
  int m_bpp;

public:
  ExrWriter();
  ~ExrWriter();

  void open(FILE* file, const TImageInfo& info) override;
  void writeLine(char* buffer) override;
  void writeLine(short* buffer) override;
  void writeLine(float* buffer) override;

  void flush() override;

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

  // m_bpp is set to "Bits Per Pixel" property value in the function open()
  bool writeAlphaSupported() const override { return m_bpp == 128; }
  bool writeInLinearColorSpace() const override { return true; }
};

ExrWriter::ExrWriter() : m_row(0), m_bpp(96) {}

ExrWriter::~ExrWriter() {
  free(m_header.channels);
  free(m_header.pixel_types);
  free(m_header.requested_pixel_types);
}

void ExrWriter::open(FILE* file, const TImageInfo& info) {
  m_fp   = file;
  m_info = info;
  InitEXRHeader(&m_header);
  InitEXRImage(&m_image);

  if (!m_properties) m_properties = new Tiio::ExrWriterProperties();

  TEnumProperty* bitsPerPixel =
      (TEnumProperty*)(m_properties->getProperty("Bits Per Pixel"));
  m_bpp = bitsPerPixel ? std::stoi(bitsPerPixel->getValue()) : 128;
  assert(m_bpp == 96 || m_bpp == 128);

  std::wstring compressionType =
      ((TEnumProperty*)(m_properties->getProperty("Compression Type")))
          ->getValue();
  m_header.compression_type = ExrCompTypeStr.key(compressionType);

  std::wstring storageType =
      ((TEnumProperty*)(m_properties->getProperty("Storage Type")))->getValue();
  if (storageType == EXR_STORAGETYPE_TILE) {
    m_header.tiled           = 1;
    m_header.tile_size_x     = 128;
    m_header.tile_size_y     = 128;
    m_header.tile_level_mode = TINYEXR_TILE_ONE_LEVEL;
  } else
    m_header.tiled = 0;

  m_image.num_channels = (m_bpp == 128) ? 4 : 3;

  for (int c = 0; c < m_image.num_channels; c++)
    m_imageBuf[c].resize(m_info.m_lx * m_info.m_ly);

  m_image.width  = m_info.m_lx;
  m_image.height = m_info.m_ly;

  m_header.num_channels = m_image.num_channels;
  m_header.channels =
      (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * m_header.num_channels);
  // Must be BGR(A) order, since most of EXR viewers expect this channel order.
  if (m_bpp == 128) {
    strncpy(m_header.channels[0].name, "B", 255);
    m_header.channels[0].name[strlen("B")] = '\0';
    strncpy(m_header.channels[1].name, "G", 255);
    m_header.channels[1].name[strlen("G")] = '\0';
    strncpy(m_header.channels[2].name, "R", 255);
    m_header.channels[2].name[strlen("R")] = '\0';
    strncpy(m_header.channels[3].name, "A", 255);
    m_header.channels[3].name[strlen("A")] = '\0';
  } else {
    strncpy(m_header.channels[0].name, "B", 255);
    m_header.channels[0].name[strlen("B")] = '\0';
    strncpy(m_header.channels[1].name, "G", 255);
    m_header.channels[1].name[strlen("G")] = '\0';
    strncpy(m_header.channels[2].name, "R", 255);
    m_header.channels[2].name[strlen("R")] = '\0';
  }

  int requested_pixel_type =
      (QString::fromStdWString(bitsPerPixel->getValue()).endsWith("_HF"))
          ? TINYEXR_PIXELTYPE_HALF
          : TINYEXR_PIXELTYPE_FLOAT;

  m_header.pixel_types = (int*)malloc(sizeof(int) * m_header.num_channels);
  m_header.requested_pixel_types =
      (int*)malloc(sizeof(int) * m_header.num_channels);
  for (int i = 0; i < m_header.num_channels; i++) {
    m_header.pixel_types[i] =
        TINYEXR_PIXELTYPE_FLOAT;  // pixel type of input image
    m_header.requested_pixel_types[i] =
        requested_pixel_type;  // pixel type of output image to be stored in
                               // .EXR
  }
}

// unused
void ExrWriter::writeLine(char* buffer) {
  TPixel32* pix    = (TPixel32*)buffer;
  TPixel32* endPix = pix + m_info.m_lx;

  float* r_p = &m_imageBuf[0][m_row * m_info.m_lx];
  float* g_p = &m_imageBuf[1][m_row * m_info.m_lx];
  float* b_p = &m_imageBuf[2][m_row * m_info.m_lx];
  float* a_p;
  if (m_bpp == 128) a_p = &m_imageBuf[3][m_row * m_info.m_lx];
  while (pix < endPix) {
    *r_p++ = uctof(pix->r);
    *g_p++ = uctof(pix->g);
    *b_p++ = uctof(pix->b);
    if (m_bpp == 128) *a_p++ = uctof(pix->m, 1.0f);
    pix++;
  }
  m_row++;
}
// unused
void ExrWriter::writeLine(short* buffer) {
  TPixel64* pix    = (TPixel64*)buffer;
  TPixel64* endPix = pix + m_info.m_lx;

  float* r_p = &m_imageBuf[0][m_row * m_info.m_lx];
  float* g_p = &m_imageBuf[1][m_row * m_info.m_lx];
  float* b_p = &m_imageBuf[2][m_row * m_info.m_lx];
  float* a_p;
  if (m_bpp == 128) a_p = &m_imageBuf[3][m_row * m_info.m_lx];
  while (pix < endPix) {
    *r_p++ = ustof(pix->r);
    *g_p++ = ustof(pix->g);
    *b_p++ = ustof(pix->b);
    if (m_bpp == 128) *a_p++ = ustof(pix->m, 1.0f);
    pix++;
  }
  m_row++;
}

void ExrWriter::writeLine(float* buffer) {
  TPixelF* pix    = (TPixelF*)buffer;
  TPixelF* endPix = pix + m_info.m_lx;

  float* r_p = &m_imageBuf[0][m_row * m_info.m_lx];
  float* g_p = &m_imageBuf[1][m_row * m_info.m_lx];
  float* b_p = &m_imageBuf[2][m_row * m_info.m_lx];
  float* a_p;
  if (m_bpp == 128) a_p = &m_imageBuf[3][m_row * m_info.m_lx];
  while (pix < endPix) {
    // raster is already linearized (see  MovieRenderer::Imp::postProcessImage()
    // in movierenderer.cpp)
    *r_p++ = pix->r;
    *g_p++ = pix->g;
    *b_p++ = pix->b;
    if (m_bpp == 128) *a_p++ = pix->m;
    //*r_p++ = toLinear(pix->r);
    //*g_p++ = toLinear(pix->g);
    //*b_p++ = toLinear(pix->b);
    // if (m_bpp == 128) *a_p++ = toLinear(pix->m, 1.0f);
    pix++;
  }
  m_row++;
}

void ExrWriter::flush() {
  if (m_bpp == 128) {
    float* image_ptr[4];
    image_ptr[0]   = &(m_imageBuf[2].at(0));  // B
    image_ptr[1]   = &(m_imageBuf[1].at(0));  // G
    image_ptr[2]   = &(m_imageBuf[0].at(0));  // R
    image_ptr[3]   = &(m_imageBuf[3].at(0));  // A
    m_image.images = (unsigned char**)image_ptr;
    const char* err;
    int ret = SaveEXRImageToFileHandle(&m_image, &m_header, m_fp, &err);
    if (ret != TINYEXR_SUCCESS) {
      throw(std::string(err));
    }
  } else {
    float* image_ptr[3];
    image_ptr[0]   = &(m_imageBuf[2].at(0));  // B
    image_ptr[1]   = &(m_imageBuf[1].at(0));  // G
    image_ptr[2]   = &(m_imageBuf[0].at(0));  // R
    m_image.images = (unsigned char**)image_ptr;
    const char* err;
    int ret = SaveEXRImageToFileHandle(&m_image, &m_header, m_fp, &err);
    if (ret != TINYEXR_SUCCESS) {
      throw(std::string(err));
    }
  }
}

//============================================================

Tiio::Reader* Tiio::makeExrReader() { return new ExrReader(); }

//------------------------------------------------------------

Tiio::Writer* Tiio::makeExrWriter() { return new ExrWriter(); }
