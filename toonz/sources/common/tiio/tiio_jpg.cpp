

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
//#include "texception.h"
//#include "tfilepath.h"
//#include "tiio_jpg.h"
//#include "../compatibility/tnz4.h"

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

#include <assert.h>
#include <stdio.h>

//=========================================================

const std::string Tiio::JpgWriterProperties::QUALITY("Quality");

//=========================================================

extern "C" {
static void tnz_error_exit(j_common_ptr cinfo) {
  //  throw "merda";
}
}

#ifdef CICCIO
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

JpgReader::JpgReader() : m_chan(0), m_isOpen(false) {
  memset(&m_cinfo, 0, sizeof m_cinfo);
  memset(&m_jerr, 0, sizeof m_jerr);
  memset(&m_buffer, 0, sizeof m_buffer);
}

JpgReader::~JpgReader() {
  if (m_isOpen) {
    try {
      jpeg_finish_decompress(&m_cinfo);
      jpeg_destroy_decompress(&m_cinfo);
    } catch (...) {
    }
  }
  if (m_chan) {
    m_chan = 0;
  }
}

Tiio::RowOrder JpgReader::getRowOrder() const { return Tiio::TOP2BOTTOM; }

void JpgReader::open(FILE *file) {
  m_cinfo.err             = jpeg_std_error(&m_jerr);
  m_cinfo.err->error_exit = tnz_error_exit;

  jpeg_create_decompress(&m_cinfo);

  m_chan = file;
  jpeg_stdio_src(&m_cinfo, m_chan);
  jpeg_save_markers(&m_cinfo, JPEG_APP0 + 1, 0xffff);  // EXIF
  bool ret = jpeg_read_header(&m_cinfo, TRUE);

  bool resolutionFoundInExif = false;
  jpeg_saved_marker_ptr mark;
  for (mark = m_cinfo.marker_list; NULL != mark; mark = mark->next) {
    switch (mark->marker) {
    case JPEG_APP0 + 1:  // EXIF
      JpgExifReader exifReader;
      exifReader.process_EXIF(mark->data - 2, mark->data_length);
      if (exifReader.containsResolution()) {
        int resUnit           = exifReader.getResolutionUnit();
        resolutionFoundInExif = true;
        if (resUnit == 1 || resUnit == 2) {  // no unit(1) or inch(2)
          m_info.m_dpix = (double)exifReader.getXResolution();
          m_info.m_dpiy = (double)exifReader.getYResolution();
        } else if (resUnit == 3) {  // centimeter(3);
          m_info.m_dpix = (double)exifReader.getXResolution() * 2.54;
          m_info.m_dpiy = (double)exifReader.getYResolution() * 2.54;
        } else  // ignore millimeter(4) and micrometer(5) cases for now
          resolutionFoundInExif = false;
      }
      break;
    }
  }

  ret = ret && jpeg_start_decompress(&m_cinfo);
  if (!ret) return;

  int row_stride = m_cinfo.output_width * m_cinfo.output_components;
  m_buffer = (*m_cinfo.mem->alloc_sarray)((j_common_ptr)&m_cinfo, JPOOL_IMAGE,
                                          row_stride, 1);

  m_info.m_lx             = m_cinfo.output_width;
  m_info.m_ly             = m_cinfo.output_height;
  m_info.m_samplePerPixel = 3;
  m_info.m_valid          = true;
  m_isOpen                = true;

  if (!resolutionFoundInExif && (m_cinfo.saw_JFIF_marker != 0) &&
      (m_cinfo.X_density != 1) && (m_cinfo.Y_density != 1)) {
    if (m_cinfo.density_unit == 1) {
      m_info.m_dpix = (double)m_cinfo.X_density;
      m_info.m_dpiy = (double)m_cinfo.Y_density;
    } else if (m_cinfo.density_unit == 2) {
      m_info.m_dpix = (double)m_cinfo.X_density * 2.54;
      m_info.m_dpiy = (double)m_cinfo.Y_density * 2.54;
    }
  }
}

void JpgReader::readLine(char *buffer, int x0, int x1, int shrink) {
  if (m_cinfo.out_color_space == JCS_RGB && m_cinfo.out_color_components == 3) {
    int ret = jpeg_read_scanlines(&m_cinfo, m_buffer, 1);
    assert(ret == 1);
    unsigned char *src = m_buffer[0];
    TPixel32 *dst      = (TPixel32 *)buffer;
    dst += x0;
    src += 3 * x0;

    int width           = (m_cinfo.output_width - 1) / shrink + 1;
    if (x1 >= x0) width = (x1 - x0) / shrink + 1;

    while (--width >= 0) {
      dst->r = src[0];
      dst->g = src[1];
      dst->b = src[2];
      dst->m = (char)255;
      src += 3 * shrink;
      dst += shrink;
    }
  } else if (m_cinfo.out_color_components == 1) {
    int ret = jpeg_read_scanlines(&m_cinfo, m_buffer, 1);
    assert(ret == 1);
    unsigned char *src = m_buffer[0];
    TPixel32 *dst      = (TPixel32 *)buffer;

    dst += x0;
    src += x0;

    int width           = (m_cinfo.output_width - 1) / shrink + 1;
    if (x1 >= x0) width = (x1 - x0) / shrink + 1;

    while (--width >= 0) {
      dst->r = *src;
      dst->g = *src;
      dst->b = *src;
      dst->m = (char)255;
      src += shrink;
      dst += shrink;
    }
  }
}

int JpgReader::skipLines(int lineCount) {
  for (int i = 0; i < lineCount; i++) {
    int ret = jpeg_read_scanlines(&m_cinfo, m_buffer, 1);
    assert(ret == 1);
  }
  return lineCount;
}

class JpgWriter final : public Tiio::Writer {
  struct jpeg_compress_struct m_cinfo;
  struct jpeg_error_mgr m_jerr;
  FILE *m_chan;
  JSAMPARRAY m_buffer;
  bool m_headerWritten;

public:
  JpgWriter() : m_chan(0), m_headerWritten(false) {}

  void open(FILE *file, const TImageInfo &info) override {
    m_cinfo.err = jpeg_std_error(&m_jerr);
    jpeg_create_compress(&m_cinfo);

    m_cinfo.image_width      = info.m_lx;
    m_cinfo.image_height     = info.m_ly;
    m_cinfo.input_components = 3;
    m_cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&m_cinfo);

    // save dpi always in JFIF header, instead of EXIF
    m_cinfo.write_JFIF_header  = 1;
    m_cinfo.JFIF_major_version = 1;
    m_cinfo.JFIF_minor_version = 2;
    m_cinfo.X_density          = (UINT16)info.m_dpix;
    m_cinfo.Y_density          = (UINT16)info.m_dpiy;
    m_cinfo.density_unit       = 1;  // dot per inch
    m_cinfo.write_Adobe_marker = 0;

    if (!m_properties) m_properties = new Tiio::JpgWriterProperties();

    int quality =
        ((TIntProperty *)(m_properties->getProperty("Quality")))->getValue();

    jpeg_set_quality(&m_cinfo, quality, TRUE);
    m_cinfo.smoothing_factor =
        ((TIntProperty *)(m_properties->getProperty("Smoothing")))->getValue();

    // set horizontal and vertical chroma subsampling factor to encoder
    // according to the quality value.
    if (quality >= 70) {  // none chroma-subsampling (4:4:4)
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
    jpeg_finish_compress(&m_cinfo);
    jpeg_destroy_compress(&m_cinfo);
    delete m_properties;
  }

  void flush() override { fflush(m_chan); }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

  void writeLine(char *buffer) override {
    if (!m_headerWritten) {
      m_headerWritten = true;
      jpeg_start_compress(&m_cinfo, TRUE);
    }
    TPixel32 *src      = (TPixel32 *)buffer;
    unsigned char *dst = m_buffer[0];
    int lx             = m_cinfo.image_width;
    while (--lx >= 0) {
      dst[0] = src->r;
      dst[1] = src->g;
      dst[2] = src->b;
      dst += 3;
      ++src;
    }
    jpeg_write_scanlines(&m_cinfo, m_buffer, 1);
  }

  // jpeg format does not support alpha channel
  bool writeAlphaSupported() const override { return false; }
};

//----

void Tiio::JpgWriterProperties::updateTranslation() {
  m_quality.setQStringName(tr("Quality"));
  m_smoothing.setQStringName(tr("Smoothing"));
}

//----
//----

Tiio::Reader *Tiio::makeJpgReader() { return new JpgReader(); }

Tiio::Writer *Tiio::makeJpgWriter() { return new JpgWriter(); }
