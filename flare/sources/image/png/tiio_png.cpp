

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <memory>
#include <stdexcept>
#include <vector>

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_png.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"

#include "png.h"

#include "tpixel.h"
#include "tpixelutils.h"

//------------------------------------------------------------

extern "C" {
static void tnz_abort(jmp_buf, int) {}

static void tnz_error_fun(png_structp pngPtr, png_const_charp error_message) {
  // Jump back to the setjmp point, libpng expects this for error recovery
  longjmp(png_jmpbuf(pngPtr), 1);
}
}

#if !defined(TNZ_LITTLE_ENDIAN)
#error "TNZ_LITTLE_ENDIAN undefined !!"
#endif

//=========================================================
/* Check for the older version of libpng */

#if defined(PNG_LIBPNG_VER)
#if (PNG_LIBPNG_VER < 10527)
extern "C" {
static png_uint_32 png_get_current_row_number(const png_structp png_ptr) {
  /* See the comments in png.h - this is the sub-image row when reading and
   * interlaced image.
   */
  if (png_ptr != NULL) return png_ptr->row_number;

  return PNG_UINT_32_MAX; /* help the app not to fail silently */
}

static png_byte png_get_current_pass_number(const png_structp png_ptr) {
  if (png_ptr != NULL) return png_ptr->pass;
  return 8; /* invalid */
}
}
#endif
#else
#error "PNG_LIBPNG_VER undefined, libpng too old?"
#endif

//=========================================================

inline USHORT mySwap(USHORT val) {
#if TNZ_LITTLE_ENDIAN
  // Correct byte swapping for little-endian systems
  return static_cast<USHORT>((val >> 8) | (val << 8));
#else
  return val;
#endif
}

//=========================================================

class PngReader final : public Tiio::Reader {
  FILE *m_chan;
  png_structp m_png_ptr;
  png_infop m_info_ptr, m_end_info_ptr;
  int m_bit_depth, m_color_type, m_interlace_type;
  int m_compression_type, m_filter_type;
  unsigned int m_sig_read;
  int m_y;
  bool m_is16bitEnabled;
  std::unique_ptr<unsigned char[]> m_rowBuffer;
  int m_canDelete;
  int m_channels;
  int m_rowBytes;
  bool m_hasPalette;
  png_colorp m_palette;
  int m_paletteSize;
  png_bytep m_transparency;
  int m_transparencySize;

public:
  PngReader()
      : m_chan(0)
      , m_png_ptr(0)
      , m_info_ptr(0)
      , m_end_info_ptr(0)
      , m_bit_depth(0)
      , m_color_type(0)
      , m_interlace_type(0)
      , m_compression_type(0)
      , m_filter_type(0)
      , m_sig_read(0)
      , m_y(0)
      , m_is16bitEnabled(true)
      , m_canDelete(0)
      , m_channels(0)
      , m_rowBytes(0)
      , m_hasPalette(false)
      , m_palette(nullptr)
      , m_paletteSize(0)
      , m_transparency(nullptr)
      , m_transparencySize(0) {}

  ~PngReader() {
    if (m_png_ptr) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, &m_end_info_ptr);
    }
    // Note: libpng manages palette and transparency memory internally
  }

  bool read16BitIsEnabled() const override { return m_is16bitEnabled; }

  void enable16BitRead(bool enabled) override { m_is16bitEnabled = enabled; }

  void open(FILE *file) override {
    try {
      m_chan = file;
    } catch (...) {
      throw TException("Can't open file");
    }

    unsigned char signature[8];
    size_t bytesRead = fread(signature, 1, sizeof(signature), m_chan);
    if (bytesRead != sizeof(signature)) {
      throw TException("Can't read PNG signature");
    }

    bool isPng = !png_sig_cmp(signature, 0, sizeof(signature));
    if (!isPng) {
      throw TException("Not a valid PNG file");
    }

    m_png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                       nullptr);
    if (!m_png_ptr) {
      throw TException("Unable to create PNG read structure");
    }

    // Set error handler
    png_set_error_fn(m_png_ptr, nullptr, tnz_error_fun, nullptr);

#if defined(PNG_LIBPNG_VER)
#if (PNG_LIBPNG_VER >= 10527)
    png_set_longjmp_fn(m_png_ptr, tnz_abort, sizeof(jmp_buf));
#endif
#endif

    m_info_ptr = png_create_info_struct(m_png_ptr);
    if (!m_info_ptr) {
      png_destroy_read_struct(&m_png_ptr, (png_infopp)0, (png_infopp)0);
      throw TException("Unable to create PNG info structure");
    }
    m_end_info_ptr = png_create_info_struct(m_png_ptr);
    if (!m_end_info_ptr) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, (png_infopp)0);
      throw TException("Unable to create PNG end info structure");
    }

    // Set up error handling with setjmp
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      png_destroy_read_struct(&m_png_ptr, &m_info_ptr, &m_end_info_ptr);
      m_png_ptr      = nullptr;
      m_info_ptr     = nullptr;
      m_end_info_ptr = nullptr;
      throw TException("PNG initialization failed");
    }

    png_init_io(m_png_ptr, m_chan);
    png_set_sig_bytes(m_png_ptr, sizeof(signature));

    png_read_info(m_png_ptr, m_info_ptr);

    if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_pHYs)) {
      png_uint_32 xdpi = png_get_x_pixels_per_meter(m_png_ptr, m_info_ptr);
      png_uint_32 ydpi = png_get_y_pixels_per_meter(m_png_ptr, m_info_ptr);
      m_info.m_dpix    = tround(xdpi * 0.0254);
      m_info.m_dpiy    = tround(ydpi * 0.0254);
    }

    png_uint_32 lx = 0, ly = 0;
    png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
                 &m_interlace_type, &m_compression_type, &m_filter_type);
    m_info.m_lx = lx;
    m_info.m_ly = ly;

    m_info.m_bitsPerSample = m_bit_depth;

    // Check for palette
    if (m_color_type == PNG_COLOR_TYPE_PALETTE) {
      m_hasPalette = true;

      // Get palette information
      png_get_PLTE(m_png_ptr, m_info_ptr, &m_palette, &m_paletteSize);

      // Get transparency information if available
      if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_tRNS)) {
        png_get_tRNS(m_png_ptr, m_info_ptr, &m_transparency,
                     &m_transparencySize, nullptr);
      }
    }

    if (m_color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(m_png_ptr);
      png_set_filler(m_png_ptr, 0xFF, PNG_FILLER_AFTER);
    }

    if (m_color_type == PNG_COLOR_TYPE_GRAY && m_bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(m_png_ptr);
    }

    if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_tRNS)) {
      png_set_tRNS_to_alpha(m_png_ptr);
    }

    if (m_bit_depth == 16 && !m_is16bitEnabled) {
      png_set_strip_16(m_png_ptr);
    }

    if (m_color_type == PNG_COLOR_TYPE_GRAY ||
        m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(m_png_ptr);
    }

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
    png_set_bgr(m_png_ptr);
    png_set_swap_alpha(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
    png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
    png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
#error "unknown channel order"
#endif

    // Handle interlacing if present - let libpng handle it
    if (m_interlace_type == PNG_INTERLACE_ADAM7) {
      png_set_interlace_handling(m_png_ptr);
    }

    // Update info after all transformations
    png_read_update_info(m_png_ptr, m_info_ptr);

    // Refresh variables after update
    m_channels = png_get_channels(m_png_ptr, m_info_ptr);
    m_rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
    png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
                 &m_interlace_type, &m_compression_type, &m_filter_type);
    m_info.m_samplePerPixel = m_channels;

    // Validate dimensions
    if (m_info.m_lx <= 0 || m_info.m_ly <= 0) {
      throw TException("Invalid PNG dimensions");
    }

    // Allocate row buffer based on updated rowBytes
    m_rowBuffer.reset(new unsigned char[m_rowBytes]);
    if (!m_rowBuffer) {
      throw TException("Memory allocation failed for row buffer");
    }
  }

  void readLine(char *buffer, int x0, int x1, int shrink) override {
    // Validate input parameters
    if (!buffer || x0 < 0 || x1 >= m_info.m_lx || x0 > x1) {
      return;
    }

    if (setjmp(png_jmpbuf(m_png_ptr))) {
      // Error while reading a PNG row.
      // Fill the damaged area with a checkerboard pattern.
      TPixel32 *pix = (TPixel32 *)buffer + x0;
      for (int j = x0; j <= x1; ++j, ++pix) {
        // 8×8 checkerboard pattern
        bool isCheckerboard = (((j >> 3) + (m_y >> 3)) & 1) == 0;

        if (isCheckerboard) {
          pix->r = pix->g = pix->b = 120;  // dark gray
          pix->m                   = 255;  // opaque
        } else {
          pix->r = pix->g = pix->b = 180;  // light gray
          pix->m                   = 255;  // opaque
        }
      }

      m_y++;
      return;
    }

    png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
    writeRow(buffer, x0, x1);
    m_y++;
  }

  void readLine(short *buffer, int x0, int x1, int shrink) override {
    // Validate input parameters
    if (!buffer || x0 < 0 || x1 >= m_info.m_lx || x0 > x1) {
      return;
    }

    if (setjmp(png_jmpbuf(m_png_ptr))) {
      // Error while reading a PNG row.
      // Fill the damaged area with a checkerboard pattern.
      TPixel64 *pix = (TPixel64 *)buffer + x0;
      for (int j = x0; j <= x1; ++j, ++pix) {
        // 8×8 checkerboard pattern
        bool isCheckerboard = (((j >> 3) + (m_y >> 3)) & 1) == 0;

        if (isCheckerboard) {
          pix->r = pix->g = pix->b = 120;  // dark gray
          pix->m                   = 255;  // opaque
        } else {
          pix->r = pix->g = pix->b = 180;  // light gray
          pix->m                   = 255;  // opaque
        }
      }
      m_y++;
      return;
    }

    png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
    writeRow(buffer, x0, x1);
    m_y++;
  }

  int skipLines(int lineCount) override {
    if (lineCount <= 0) return 0;

    int skipped = 0;
    for (int i = 0; i < lineCount; ++i) {
      if (m_y >= m_info.m_ly) break;

      if (setjmp(png_jmpbuf(m_png_ptr))) {
        break;  // Stop on error
      }

      png_read_row(m_png_ptr, m_rowBuffer.get(), nullptr);
      m_y++;
      skipped++;
    }
    return skipped;
  }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

  // Accessor for palette information (if needed by external code)
  bool hasPalette() const { return m_hasPalette; }
  int getPaletteSize() const { return m_paletteSize; }
  const png_color *getPalette() const { return m_palette; }
  const png_byte *getTransparency() const { return m_transparency; }
  int getTransparencySize() const { return m_transparencySize; }

private:
  void writeRow(char *buffer, int x0, int x1) {
    bool hasAlpha = (m_channels == 4);

    if (m_bit_depth == 16) {
      // 16-bit not supported for char* buffer
      TPixel32 *pix = (TPixel32 *)buffer + x0;
      for (int j = x0; j <= x1; ++j, ++pix) {
        pix->r = pix->g = pix->b = pix->m = 0;
      }
      return;
    }

    TPixel32 *pix = (TPixel32 *)buffer + x0;
    int srcIdx    = x0 * m_channels;

    for (int j = x0; j <= x1; ++j, ++pix) {
      if (srcIdx + m_channels > m_rowBytes) {
        break;  // Prevent buffer overflow
      }

      if (hasAlpha) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
        pix->m = m_rowBuffer[srcIdx++];
        pix->r = m_rowBuffer[srcIdx++];
        pix->g = m_rowBuffer[srcIdx++];
        pix->b = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        pix->r = m_rowBuffer[srcIdx++];
        pix->g = m_rowBuffer[srcIdx++];
        pix->b = m_rowBuffer[srcIdx++];
        pix->m = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        pix->b = m_rowBuffer[srcIdx++];
        pix->g = m_rowBuffer[srcIdx++];
        pix->r = m_rowBuffer[srcIdx++];
        pix->m = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
        pix->m = m_rowBuffer[srcIdx++];
        pix->b = m_rowBuffer[srcIdx++];
        pix->g = m_rowBuffer[srcIdx++];
        pix->r = m_rowBuffer[srcIdx++];
#else
#error "unknown channel order"
#endif
        premult(*pix);
      } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        pix->r = m_rowBuffer[srcIdx++];
        pix->g = m_rowBuffer[srcIdx++];
        pix->b = m_rowBuffer[srcIdx++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        pix->b = m_rowBuffer[srcIdx++];
        pix->g = m_rowBuffer[srcIdx++];
        pix->r = m_rowBuffer[srcIdx++];
#else
#error "unknown channel order"
#endif
        pix->m = 255;
      }
    }
  }

  void writeRow(short *buffer, int x0, int x1) {
    bool hasAlpha       = (m_channels == 4);
    int bytesPerChannel = (m_bit_depth == 16) ? 2 : 1;

    TPixel64 *pix = (TPixel64 *)buffer + x0;
    int srcIdx    = x0 * m_channels * bytesPerChannel;

    for (int j = x0; j <= x1; ++j, ++pix) {
      if (srcIdx >= m_rowBytes ||
          srcIdx + (m_channels * bytesPerChannel) > m_rowBytes) {
        break;  // Prevent buffer overflow
      }

      if (hasAlpha) {
        if (m_bit_depth == 16) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
          USHORT r, g, b, m;
          memcpy(&r, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&g, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&b, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&m, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          pix->r = mySwap(r);
          pix->g = mySwap(g);
          pix->b = mySwap(b);
          pix->m = mySwap(m);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
          USHORT b, g, r, m;
          memcpy(&b, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&g, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&r, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&m, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          pix->b = mySwap(b);
          pix->g = mySwap(g);
          pix->r = mySwap(r);
          pix->m = mySwap(m);
#else
#error "unknown channel order"
#endif
        } else {
          // 8-bit with alpha: 1 byte per channel, converted to 16-bit
          // Layout: R(1) G(1) B(1) A(1) bytes
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
          pix->r =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->g =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->b =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->m =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
          // Layout for 8-bit with alpha (BGR): B(1) G(1) R(1) A(1) bytes
          pix->b =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->g =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->r =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->m =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
#else
#error "unknown channel order"
#endif
        }
        premult(*pix);
      } else {
        if (m_bit_depth == 16) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
          // Layout for 16-bit without alpha: R(2) G(2) B(2) bytes
          USHORT r, g, b;
          memcpy(&r, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&g, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&b, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          pix->r = mySwap(r);
          pix->g = mySwap(g);
          pix->b = mySwap(b);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
          // Layout for 16-bit without alpha (BGR): B(2) G(2) R(2) bytes
          USHORT b, g, r;
          memcpy(&b, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&g, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          memcpy(&r, m_rowBuffer.get() + srcIdx, sizeof(USHORT));
          srcIdx += 2;
          pix->b = mySwap(b);
          pix->g = mySwap(g);
          pix->r = mySwap(r);
#else
#error "unknown channel order"
#endif
        } else {
          // 8-bit without alpha
          // Layout: R(1) G(1) B(1) bytes
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
          pix->r =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->g =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->b =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
          // Layout for 8-bit without alpha (BGR): B(1) G(1) R(1) bytes
          pix->b =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->g =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
          pix->r =
              static_cast<USHORT>((m_rowBuffer[srcIdx++] * 65535 + 127) / 255);
#else
#error "unknown channel order"
#endif
        }
        pix->m = 65535;
      }
    }
  }
};

//=========================================================

Tiio::PngWriterProperties::PngWriterProperties()
    : m_matte("Alpha Channel", true), m_colormap("Colormap", nullptr) {
  bind(m_matte);
  bind(m_colormap);
}

void Tiio::PngWriterProperties::updateTranslation() {
  m_matte.setQStringName(tr("Alpha Channel"));
  m_colormap.setQStringName(tr("Colormap"));
}

//=========================================================

class PngWriter final : public Tiio::Writer {
  png_structp m_png_ptr;
  png_infop m_info_ptr;
  FILE *m_chan;
  bool m_matte;
  bool m_written_end;
  const std::vector<TPixel32> *m_colormap;

public:
  PngWriter()
      : m_png_ptr(0)
      , m_info_ptr(0)
      , m_chan(nullptr)
      , m_matte(true)
      , m_written_end(false)
      , m_colormap(nullptr) {}

  ~PngWriter() {
    // If an exception occurs in the destructor, we log the error but continue
    // trying to release resources to avoid leaks
    try {
      if (m_png_ptr && !m_written_end) {
        // Try to finalize writing if not done
        png_write_end(m_png_ptr, m_info_ptr);
      }
    } catch (...) {
      // Ignore exceptions in destructor
    }

    if (m_png_ptr) {
      png_destroy_write_struct(&m_png_ptr, &m_info_ptr);
      m_png_ptr  = nullptr;
      m_info_ptr = nullptr;
    }

    if (m_properties) {
      delete m_properties;
      m_properties = nullptr;
    }
    // Note: The FILE* is owned by the caller, we should not close it here
    // The caller is responsible for closing the file
  }

  void open(FILE *file, const TImageInfo &info) override {
    // Null pointer check
    if (!file) {
      throw TException("Invalid file pointer (null)");
    }
    m_chan = file;
    m_info = info;

    m_png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                        nullptr);
    if (!m_png_ptr) {
      throw TException("Unable to create PNG write structure");
    }

    // Set error handler
    png_set_error_fn(m_png_ptr, nullptr, tnz_error_fun, nullptr);

    m_info_ptr = png_create_info_struct(m_png_ptr);
    if (!m_info_ptr) {
      png_destroy_write_struct(&m_png_ptr, nullptr);
      throw TException("Unable to create PNG info structure");
    }

    if (setjmp(png_jmpbuf(m_png_ptr))) {
      png_destroy_write_struct(&m_png_ptr, &m_info_ptr);
      throw TException("PNG write initialization failed");
    }

    png_init_io(m_png_ptr, m_chan);

    if (!m_properties) m_properties = new Tiio::PngWriterProperties();

    TBoolProperty *alphaProp = dynamic_cast<TBoolProperty *>(
        m_properties->getProperty("Alpha Channel"));
    m_matte = alphaProp ? alphaProp->getValue() : true;

    TPointerProperty *colormapProp =
        dynamic_cast<TPointerProperty *>(m_properties->getProperty("Colormap"));
    m_colormap = colormapProp ? static_cast<const std::vector<TPixel32> *>(
                                    colormapProp->getValue())
                              : nullptr;

    png_uint_32 x_pixels_per_meter = tround(m_info.m_dpix / 0.0254);
    png_uint_32 y_pixels_per_meter = tround(m_info.m_dpiy / 0.0254);

    int colorType = PNG_COLOR_TYPE_RGB;
    int bitDepth  = info.m_bitsPerSample;

    if (m_colormap) {
      colorType = PNG_COLOR_TYPE_PALETTE;
      bitDepth  = 8;  // Palettes are always 8-bit

      // Validate palette size
      if (m_colormap->size() > 256) {
        throw TException("Palette too large (max 256 colors)");
      }
    } else if (m_matte) {
      colorType = PNG_COLOR_TYPE_RGB_ALPHA;
    }

    png_set_IHDR(m_png_ptr, m_info_ptr, m_info.m_lx, m_info.m_ly, bitDepth,
                 colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    // Configure palette if needed
    if (m_colormap) {
      png_color palette[256];
      size_t paletteSize = std::min(m_colormap->size(), (size_t)256);

      for (size_t i = 0; i < paletteSize; ++i) {
        palette[i].red   = (*m_colormap)[i].r;
        palette[i].green = (*m_colormap)[i].g;
        palette[i].blue  = (*m_colormap)[i].b;
      }

      png_set_PLTE(m_png_ptr, m_info_ptr, palette, (int)paletteSize);

      // Add transparency information if matte is enabled
      if (m_matte) {
        // Create transparency array - assume index 0 is transparent if alpha is
        // 0
        png_byte trans = 0;
        png_set_tRNS(m_png_ptr, m_info_ptr, &trans, 1, nullptr);
      }
    }

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
    png_set_bgr(m_png_ptr);
    png_set_swap_alpha(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
    png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
    png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
#error "unknown channel order"
#endif

    png_set_pHYs(m_png_ptr, m_info_ptr, x_pixels_per_meter, y_pixels_per_meter,
                 PNG_RESOLUTION_METER);

    png_write_info(m_png_ptr, m_info_ptr);
  }

  void writeLine(char *buffer) override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG write error");
    }

    if (!buffer) {
      throw TException("Invalid buffer");
    }

    // Handle palette mode
    if (m_colormap) {
      // For palette mode, buffer should contain palette indices (0-255)
      png_write_row(m_png_ptr, (unsigned char *)buffer);
      return;
    }

    // Normal RGBA or RGB mode: std::vector used instead of manual allocation
    std::vector<unsigned char> row(m_info.m_lx * (m_matte ? 4 : 3));
    TPixel32 *pix = (TPixel32 *)buffer;
    int k         = 0;

    for (int j = 0; j < m_info.m_lx; ++j, ++pix) {
      int required = k + (m_matte ? 4 : 3);
      if (required > static_cast<int>(row.size())) {
        break;  // Prevent buffer overflow
      }

      TPixel32 depix = *pix;
      if (m_matte && depix.m != 0) depremult(depix);

      if (m_matte) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
        row[k++] = depix.m;
        row[k++] = depix.r;
        row[k++] = depix.g;
        row[k++] = depix.b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = depix.r;
        row[k++] = depix.g;
        row[k++] = depix.b;
        row[k++] = depix.m;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
        row[k++] = depix.m;
        row[k++] = depix.b;
        row[k++] = depix.g;
        row[k++] = depix.r;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = depix.b;
        row[k++] = depix.g;
        row[k++] = depix.r;
        row[k++] = depix.m;
#else
#error "unknown channel order"
#endif
      } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = depix.r;
        row[k++] = depix.g;
        row[k++] = depix.b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = depix.b;
        row[k++] = depix.g;
        row[k++] = depix.r;
#else
#error "unknown channel order"
#endif
      }
    }
    png_write_row(m_png_ptr, row.data());
  }

  void writeLine(short *buffer) override {
    if (m_colormap) {
      // Palette mode doesn't support 16-bit
      throw TException("Palette mode doesn't support 16-bit data");
    }

    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG write error");
    }

    if (!buffer) {
      throw TException("Invalid buffer");
    }

    // Using std::vector instead of manual allocation
    std::vector<unsigned short> row(m_info.m_lx * (m_matte ? 4 : 3));
    TPixel64 *pix = (TPixel64 *)buffer;
    int k         = 0;

    for (int j = 0; j < m_info.m_lx; ++j, ++pix) {
      int required = k + (m_matte ? 4 : 3);
      if (required > static_cast<int>(row.size())) {
        break;  // Prevent buffer overflow
      }

      TPixel64 depix = *pix;
      if (m_matte && depix.m != 0) depremult(depix);

      if (m_matte) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = mySwap(depix.r);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.b);
        row[k++] = mySwap(depix.m);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = mySwap(depix.b);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.r);
        row[k++] = mySwap(depix.m);
#else
#error "unknown channel order"
#endif
      } else {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) ||                                 \
    defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
        row[k++] = mySwap(depix.r);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.b);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) ||                               \
    defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
        row[k++] = mySwap(depix.b);
        row[k++] = mySwap(depix.g);
        row[k++] = mySwap(depix.r);
#else
#error "unknown channel order"
#endif
      }
    }
    png_write_row(m_png_ptr, reinterpret_cast<unsigned char *>(row.data()));
  }

  void flush() override {
    if (setjmp(png_jmpbuf(m_png_ptr))) {
      throw TException("PNG write error during flush");
    }
    png_write_end(m_png_ptr, m_info_ptr);
    m_written_end = true;

    if (m_chan) {
      fflush(m_chan);
    }
  }

  Tiio::RowOrder getRowOrder() const override { return Tiio::TOP2BOTTOM; }

  bool write64bitSupported() const override { return true; }

  bool writeAlphaSupported() const override { return m_matte; }
};

//=========================================================

Tiio::Reader *Tiio::makePngReader() { return new PngReader(); }

Tiio::Writer *Tiio::makePngWriter() { return new PngWriter(); }
