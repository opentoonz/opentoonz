

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
// #include "texception.h"
// #include "tfilepath.h"
// #include "tiio_jpg.h"
// #include "../compatibility/tnz4.h"

#include "tiio_jpg.h"
#include "tiio_jpg_exif.h"
#include "tproperty.h"
#include "tpixel.h"

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */
#include "texception.h"  // for TException

#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmath>      // for std::lround
#include <stdexcept>  // for std::runtime_error

//=========================================================
// JpgWriterProperties
//=========================================================

const std::string Tiio::JpgWriterProperties::QUALITY("Quality");

//=========================================================
// Helper functions
//=========================================================

namespace {  // anonymous namespace

// Fill a buffer with an 8x8 checkerboard pattern
void fillCheckerboard(char *buffer, int x0, int x1, int shrink,
                      int currentLine) {
  TPixel32 *pix = reinterpret_cast<TPixel32 *>(buffer) + x0;
  int width     = (x1 - x0) / shrink + 1;
  for (int i = 0; i < width; ++i) {
    int col        = x0 + i * shrink;
    bool isChecker = (((col >> 3) + (currentLine >> 3)) & 1) == 0;
    if (isChecker)
      pix->r = pix->g = pix->b = 120;  // dark gray
    else
      pix->r = pix->g = pix->b = 180;  // light gray
    pix->m = 255;
    pix += shrink;
  }
}

}  // anonymous namespace

//=========================================================
// JpgReader
//=========================================================

extern "C" {
// Custom error handler for libjpeg: throws a C++ exception
static void tnz_error_exit(j_common_ptr cinfo) {
  char buffer[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buffer);
  throw TException(buffer);
}
}

#ifdef CICCIO
// Custom error and message handling (probably legacy / platform-specific)
JMETHOD(void, error_exit, (j_common_ptr cinfo));
/* Conditionally emit a trace or warning message */
JMETHOD(void, emit_message, (j_common_ptr cinfo, int msg_level));
/* Routine that actually outputs a trace or error message */
JMETHOD(void, output_message, (j_common_ptr cinfo));
/* Format a message string for the most recent JPEG error or message */
JMETHOD(void, format_message, (j_common_ptr cinfo, char *buffer));
#define JMSG_LENGTH_MAX 200 /* recommended size of format_message buffer */
/* Reset error state variables at start of a new image */
JMETHOD(void, reset_error_mgr, (j_common_ptr cinfo));
#endif

using namespace Tiio;

JpgReader::JpgReader()
    : m_chan(nullptr)
    , m_isOpen(false)
    , m_errorOccurred(false)
    , m_currentLine(0) {
  memset(&m_cinfo, 0, sizeof m_cinfo);
  memset(&m_jerr, 0, sizeof m_jerr);
  m_buffer            = nullptr;
  m_decompressCreated = false;

  // Set a default size in case the file is completely corrupted
  m_info.m_lx             = 1920;
  m_info.m_ly             = 1080;
  m_info.m_dpix           = 72.0;
  m_info.m_dpiy           = 72.0;
  m_info.m_samplePerPixel = 3;
}

JpgReader::~JpgReader() {
  if (m_isOpen) {
    try {
      jpeg_finish_decompress(&m_cinfo);
      jpeg_destroy_decompress(&m_cinfo);
    } catch (...) {
    }
  }
  m_chan = nullptr;
}

Tiio::RowOrder JpgReader::getRowOrder() const { return Tiio::TOP2BOTTOM; }

void JpgReader::open(FILE *file) {
  if (!file) {
    // Keep default dimensions (1920x1080) and mark as invalid
    m_info.m_valid  = false;
    m_isOpen        = true;  // Allows readLine to generate checkerboard
    m_errorOccurred = true;
    return;
  }

  m_cinfo.err             = jpeg_std_error(&m_jerr);
  m_cinfo.err->error_exit = tnz_error_exit;

  try {
    jpeg_create_decompress(&m_cinfo);
    m_chan = file;
    jpeg_stdio_src(&m_cinfo, m_chan);
    jpeg_save_markers(&m_cinfo, JPEG_APP0 + 1, 0xffff);

    int ret = jpeg_read_header(&m_cinfo, TRUE);
    if (ret != JPEG_HEADER_OK) throw TException("JPEG header error");
    m_info.m_lx = m_cinfo.image_width;
    m_info.m_ly = m_cinfo.image_height;
    m_info.m_samplePerPixel =
        3;  // JPEG is always RGB (or grayscale, but treated as RGB)

    // Try reading EXIF for resolution (optional, may fail)
    bool resolutionFoundInExif = false;
    for (jpeg_saved_marker_ptr mark = m_cinfo.marker_list; mark;
         mark                       = mark->next) {
      if (mark->marker == JPEG_APP0 + 1) {
        JpgExifReader exifReader;
        exifReader.process_EXIF(mark->data - 2, mark->data_length);
        if (exifReader.containsResolution()) {
          resolutionFoundInExif = true;
          int resUnit           = exifReader.getResolutionUnit();
          if (resUnit == 1 || resUnit == 2) {
            m_info.m_dpix = static_cast<double>(exifReader.getXResolution());
            m_info.m_dpiy = static_cast<double>(exifReader.getYResolution());
          } else if (resUnit == 3) {
            m_info.m_dpix =
                static_cast<double>(exifReader.getXResolution()) * 2.54;
            m_info.m_dpiy =
                static_cast<double>(exifReader.getYResolution()) * 2.54;
          } else {
            resolutionFoundInExif = false;
          }
        }
        break;
      }
    }

    // Fallback to JFIF if EXIF failed
    if (!resolutionFoundInExif && m_cinfo.saw_JFIF_marker &&
        m_cinfo.X_density != 1 && m_cinfo.Y_density != 1) {
      if (m_cinfo.density_unit == 1) {
        m_info.m_dpix = static_cast<double>(m_cinfo.X_density);
        m_info.m_dpiy = static_cast<double>(m_cinfo.Y_density);
      } else if (m_cinfo.density_unit == 2) {
        m_info.m_dpix = static_cast<double>(m_cinfo.X_density) * 2.54;
        m_info.m_dpiy = static_cast<double>(m_cinfo.Y_density) * 2.54;
      }
    }

    // Now try to start decompression
    if (!jpeg_start_decompress(&m_cinfo)) {
      // If it fails, destroy the structure and mark as invalid
      jpeg_destroy_decompress(&m_cinfo);
      m_info.m_valid  = false;
      m_isOpen        = true;  // Allows readLine to generate checkerboard
      m_errorOccurred = true;
      return;
    }

    // Decompression started successfully
    int row_stride = m_cinfo.output_width * m_cinfo.output_components;
    m_buffer = (*m_cinfo.mem->alloc_sarray)((j_common_ptr)&m_cinfo, JPOOL_IMAGE,
                                            row_stride, 1);

    m_info.m_valid  = true;
    m_isOpen        = true;
    m_currentLine   = 0;
    m_errorOccurred = false;

  } catch (...) {
    // On exception, destroy the structure
    // Dimensions may already have been set (if exception happened after
    // jpeg_read_header) or still be the default (1920x1080)
    jpeg_destroy_decompress(&m_cinfo);
    m_info.m_valid  = false;
    m_isOpen        = true;
    m_errorOccurred = true;
  }
}

// Read a line from JPEG, fills checkerboard if invalid or error occurred
void JpgReader::readLine(char *buffer, int x0, int x1, int shrink) {
  if (!m_isOpen) return;

  if (!m_info.m_valid || m_errorOccurred) {
    fillCheckerboard(buffer, x0, x1, shrink, m_currentLine);
    m_currentLine++;
    return;
  }

  int ret = jpeg_read_scanlines(&m_cinfo, m_buffer, 1);
  if (ret != 1) {
    m_errorOccurred = true;
    fillCheckerboard(buffer, x0, x1, shrink, m_currentLine);
    m_currentLine++;
    return;
  }

  unsigned char *src = m_buffer[0];
  TPixel32 *dst      = reinterpret_cast<TPixel32 *>(buffer) + x0;

  if (m_cinfo.out_color_components == 3) {
    src += 3 * x0;
    int width = (x1 - x0) / shrink + 1;
    while (--width >= 0) {
      dst->r = src[0];
      dst->g = src[1];
      dst->b = src[2];
      dst->m = 255;
      src += 3 * shrink;
      dst += shrink;
    }
  } else if (m_cinfo.out_color_components == 1) {
    src += x0;
    int width = (x1 - x0) / shrink + 1;
    while (--width >= 0) {
      dst->r = dst->g = dst->b = src[0];
      dst->m                   = 255;
      src += shrink;
      dst += shrink;
    }
  }

  m_currentLine++;
}

int JpgReader::skipLines(int lineCount) {
  if (!m_isOpen || !m_info.m_valid || m_errorOccurred) return 0;
  int skipped = 0;
  for (int i = 0; i < lineCount; ++i) {
    int ret = jpeg_read_scanlines(&m_cinfo, m_buffer, 1);
    if (ret != 1) break;
    skipped++;
    m_currentLine++;
  }
  return skipped;
}

//=========================================================
// JpgWriter
//=========================================================

class JpgWriter final : public Tiio::Writer {
  struct jpeg_compress_struct m_cinfo;
  struct jpeg_error_mgr m_jerr;
  FILE *m_chan;
  JSAMPARRAY m_buffer;
  bool m_headerWritten;
  bool m_ownProperties;

public:
  JpgWriter()
      : m_chan(nullptr), m_headerWritten(false), m_ownProperties(false) {}

  void open(FILE *file, const TImageInfo &info) override {
    m_cinfo.err = jpeg_std_error(&m_jerr);
    jpeg_create_compress(&m_cinfo);

    m_cinfo.image_width      = info.m_lx;
    m_cinfo.image_height     = info.m_ly;
    m_cinfo.input_components = 3;
    m_cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&m_cinfo);

    // Save dpi in JFIF header
    m_cinfo.write_JFIF_header  = 1;
    m_cinfo.JFIF_major_version = 1;
    m_cinfo.JFIF_minor_version = 2;
    m_cinfo.X_density          = static_cast<UINT16>(std::lround(info.m_dpix));
    m_cinfo.Y_density          = static_cast<UINT16>(std::lround(info.m_dpiy));
    m_cinfo.density_unit       = 1;  // dots per inch
    m_cinfo.write_Adobe_marker = 0;

    if (!m_properties) {
      m_properties    = new Tiio::JpgWriterProperties();
      m_ownProperties = true;
    }

    int quality =
        static_cast<TIntProperty *>(m_properties->getProperty("Quality"))
            ->getValue();
    jpeg_set_quality(&m_cinfo, quality, TRUE);
    m_cinfo.smoothing_factor =
        static_cast<TIntProperty *>(m_properties->getProperty("Smoothing"))
            ->getValue();

    // Chroma subsampling based on quality
    if (quality >= 70) {  // no chroma-subsampling (4:4:4)
      m_cinfo.comp_info[0].h_samp_factor = 1;
      m_cinfo.comp_info[0].v_samp_factor = 1;
    } else if (quality >= 30) {  // medium chroma-subsampling (4:2:2)
      m_cinfo.comp_info[0].h_samp_factor = 2;
      m_cinfo.comp_info[0].v_samp_factor = 1;
    } else {  // quality < 30, high chroma-subsampling (4:1:1)
      m_cinfo.comp_info[0].h_samp_factor = 2;
      m_cinfo.comp_info[0].v_samp_factor = 2;
    }
    m_cinfo.comp_info[1].h_samp_factor = 1;
    m_cinfo.comp_info[1].v_samp_factor = 1;
    m_cinfo.comp_info[2].h_samp_factor = 1;
    m_cinfo.comp_info[2].v_samp_factor = 1;

    int row_stride = m_cinfo.image_width * m_cinfo.input_components;
    m_buffer = (*m_cinfo.mem->alloc_sarray)((j_common_ptr)&m_cinfo, JPOOL_IMAGE,
                                            row_stride, 1);

    m_chan = file;
    jpeg_stdio_dest(&m_cinfo, m_chan);
  }

  ~JpgWriter() {
    if (m_headerWritten) jpeg_finish_compress(&m_cinfo);
    jpeg_destroy_compress(&m_cinfo);

    if (m_ownProperties) {
      delete m_properties;
      m_properties = nullptr;
    }
  }

  void flush() override { fflush(m_chan); }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

  void writeLine(char *buffer) override {
    if (!m_headerWritten) {
      m_headerWritten = true;
      jpeg_start_compress(&m_cinfo, TRUE);
    }

    TPixel32 *src      = reinterpret_cast<TPixel32 *>(buffer);
    unsigned char *dst = m_buffer[0];
    int lx             = m_cinfo.image_width;

    while (--lx >= 0) {
      // If the pixel is fully transparent (alpha == 0),
      // replace with white. Otherwise, copy original colors.
      if (src->m == 0) {
        dst[0] = 255;  // R
        dst[1] = 255;  // G
        dst[2] = 255;  // B
      } else {
        dst[0] = src->r;
        dst[1] = src->g;
        dst[2] = src->b;
      }
      dst += 3;
      ++src;
    }

    jpeg_write_scanlines(&m_cinfo, m_buffer, 1);
  }

  bool writeAlphaSupported() const override { return false; }
};

//=========================================================
// Properties translation
//=========================================================

void Tiio::JpgWriterProperties::updateTranslation() {
  m_quality.setQStringName(tr("Quality"));
  m_smoothing.setQStringName(tr("Smoothing"));
}

//=========================================================
// Factory functions
//=========================================================

Tiio::Reader *Tiio::makeJpgReader() { return new JpgReader(); }
Tiio::Writer *Tiio::makeJpgWriter() { return new JpgWriter(); }
