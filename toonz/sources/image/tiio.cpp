
#include "tnzimage.h"
#include "tiio.h"
#include "tfiletype.h"

//-------------------------------------------------------------------

/*!
    TImageReader::load MUST throw an exception in case of error (file not found,
   ...)
    TImageWriter::save MUST throw an exception in case of error (write denied,
                                                                 disk full, ...)
    N.B.: gestire nei TImageWriter il caso "disk full"!!!
*/

// Math needs to be included before the rest on MAC - but I've not figured out
// why (it would be included anyway though)
#include <math.h>

// Platform-specific includes
#if defined(_WIN32)

#if !defined(x64) && !defined(__GNUC__)

#define list QuickTime_list
#define map QuickTime_map
#define iterator QuickTime_iterator
#define float_t QuickTime_float_t
#define GetProcessInformation QuickTime_GetProcessInformation
#define int_fast8_t QuickTime_int_fast8_t
#define int_fast16_t QuickTime_int_fast16_t
#define uint_fast16_t QuickTime_uint_fast16_t

#include "QuickTimeComponents.h"
#include "tquicktime.h"

#undef list
#undef map
#undef iterator
#undef float_t
#undef GetProcessInformation
#undef int_fast8_t
#undef int_fast16_t
#undef uint_fast16_t

#endif

#include "./mov/tiio_mov.h"
#include "./3gp/tiio_3gp.h"
#include "./zcc/tiio_zcc.h"

#elif defined(MACOSX)
#include "./mov/tiio_movM.h"
#include "./3gp/tiio_3gpM.h"

#elif defined(LINUX) || defined(FREEBSD)  // No more supported by the way...
// #include "./mov/tiio_movL.h"
#include "./mov/tiio_mov_proxy.h"
#include "./3gp/tiio_3gp_proxy.h"
#endif

// Common includes
#include "./quantel/tiio_quantel.h"
#include "./sgi/tiio_sgi.h"
#include "./tga/tiio_tga.h"
#include "./png/tiio_png.h"
/* Can't build this on FreeBSD: libtiff requires internal API access.
 * Probably some one get luck to fix this. */
#ifndef FREEBSD
#include "./tif/tiio_tif.h"
#include "./tzp/tiio_tzp.h"
#include "./tzp/tiio_plt.h"
#endif
#include "./psd/tiio_psd.h"
#include "./avi/tiio_avi.h"
#include "./pli/tiio_pli.h"
#include "./tzl/tiio_tzl.h"
#include "./svg/tiio_svg.h"
#include "./ffmpeg/tiio_gif.h"
#include "./ffmpeg/tiio_webm.h"
#include "./ffmpeg/tiio_mp4.h"
#include "./mesh/tiio_mesh.h"
#include "./sprite/tiio_sprite.h"

//-------------------------------------------------------------------

// static TPluginInfo info("imageIOPlugin");

// TLIBMAIN
void initImageIo(bool lightVersion) {
  if (!lightVersion) {
    TLevelWriter::define("pli", TLevelWriterPli::create, false);
    TLevelReader::define("pli", TLevelReaderPli::create);
    TFileType::declare("pli", TFileType::VECTOR_LEVEL);

    TLevelReader::define("svg", 0, TLevelReaderSvg::create);
    TFileType::declare("svg", TFileType::VECTOR_LEVEL);
    TLevelWriter::define("svg", TLevelWriterSvg::create, false);
    Tiio::defineWriterProperties("svg", new Tiio::SvgWriterProperties());

    TLevelWriter::define("tzl", TLevelWriterTzl::create, false);
    TLevelReader::define("tzl", TLevelReaderTzl::create);
    TFileType::declare("tzl", TFileType::RASTER_LEVEL);

    TLevelWriter::define("tlv", TLevelWriterTzl::create, false);
    TLevelReader::define("tlv", TLevelReaderTzl::create);
    TFileType::declare("tlv", TFileType::CMAPPED_LEVEL);

/* Can't build this on FreeBSD: libtiff requires internal API access.
 * Probably some one get luck to fix this. */
#ifndef FREEBSD
    Tiio::defineReaderMaker("tzp", Tiio::makeTzpReader);
    Tiio::defineWriterMaker("tzp", Tiio::makeTzpWriter, false);
    TFileType::declare("tzp", TFileType::CMAPPED_IMAGE);

    Tiio::defineReaderMaker("tzu", Tiio::makeTzpReader);
    Tiio::defineWriterMaker("tzu", Tiio::makeTzpWriter, false);
    TFileType::declare("tzu", TFileType::CMAPPED_IMAGE);

    Tiio::defineReaderMaker("tzi", Tiio::makeTziReader);
    TFileType::declare("tzi", TFileType::RASTER_IMAGE);

    Tiio::defineReaderMaker("plt", Tiio::makePltReader);
    Tiio::defineWriterMaker("plt", Tiio::makePltWriter, false);
    TFileType::declare("plt", TFileType::RASTER_IMAGE);
#endif

    Tiio::defineReaderMaker("nol", Tiio::makePngReader);
    Tiio::defineWriterMaker("nol", Tiio::makePngWriter, true);
    TFileType::declare("nol", TFileType::RASTER_IMAGE);

    TLevelWriter::define("psd", TLevelWriterPsd::create, false);
    TLevelReader::define("psd", TLevelReaderPsd::create);
    TFileType::declare("psd", TFileType::RASTER_LEVEL);

    TLevelWriter::define("mesh", TLevelWriterMesh::create, false);
    TLevelReader::define("mesh", TLevelReaderMesh::create);
    TFileType::declare("mesh", TFileType::MESH_IMAGE);

  }  // !lightversion

  TFileType::declare("tpl", TFileType::PALETTE_LEVEL);

  Tiio::defineReaderMaker("png", Tiio::makePngReader);
  Tiio::defineWriterMaker("png", Tiio::makePngWriter, true);
  TFileType::declare("png", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("png", new Tiio::PngWriterProperties());

  Tiio::defineReaderMaker("tga", Tiio::makeTgaReader);
  Tiio::defineWriterMaker("tga", Tiio::makeTgaWriter, true);
  TFileType::declare("tga", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("tga", new Tiio::TgaWriterProperties());

/* Can't build this on FreeBSD: libtiff requires internal API access.
 * Probably some one get luck to fix this. */
#ifndef FREEBSD
  Tiio::defineReaderMaker("tif", Tiio::makeTifReader);
  Tiio::defineWriterMaker("tif", Tiio::makeTifWriter, true);
  TFileType::declare("tif", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("tif", new Tiio::TifWriterProperties());

  Tiio::defineReaderMaker("tiff", Tiio::makeTifReader);
  Tiio::defineWriterMaker("tiff", Tiio::makeTifWriter, true);
  TFileType::declare("tiff", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("tiff", new Tiio::TifWriterProperties());
#endif

  Tiio::defineReaderMaker("sgi", Tiio::makeSgiReader);
  Tiio::defineWriterMaker("sgi", Tiio::makeSgiWriter, true);
  TFileType::declare("sgi", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("sgi", new Tiio::SgiWriterProperties());

  Tiio::defineReaderMaker("rgb", Tiio::makeSgiReader);
  Tiio::defineWriterMaker("rgb", Tiio::makeSgiWriter, true);
  TFileType::declare("rgb", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("rgb", new Tiio::SgiWriterProperties());

  TLevelWriter::define("spritesheet", TLevelWriterSprite::create, true);
  TFileType::declare("spritesheet", TFileType::RASTER_LEVEL);
  Tiio::defineWriterProperties("spritesheet",
                               new Tiio::SpriteWriterProperties());

// ffmpeg
#if !defined(_WIN32) || defined(x64) || (defined(_WIN32) && defined(__GNUC__))
  if (Ffmpeg::checkFfmpeg()) {
    bool ffprobe = Ffmpeg::checkFfprobe();
    if (Ffmpeg::checkFormat("webm")) {
      TLevelWriter::define("webm", TLevelWriterWebm::create, true);
      if (ffprobe) TLevelReader::define("webm", TLevelReaderWebm::create);
      TFileType::declare("webm", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("webm", new Tiio::WebmWriterProperties());
    }
    if (Ffmpeg::checkFormat("gif")) {
      TLevelWriter::define("gif", TLevelWriterGif::create, true);
      if (ffprobe) TLevelReader::define("gif", TLevelReaderGif::create);
      TFileType::declare("gif", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("gif", new Tiio::GifWriterProperties());
    }
    if (Ffmpeg::checkFormat("mp4")) {
      TLevelWriter::define("mp4", TLevelWriterMp4::create, true);
      if (ffprobe) TLevelReader::define("mp4", TLevelReaderMp4::create);
      TFileType::declare("mp4", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("mp4", new Tiio::Mp4WriterProperties());
    }
  }
#endif
  // end ffmpeg

  if (!lightVersion) {
#ifdef _WIN32

    TLevelWriter::define("avi", TLevelWriterAvi::create, true);
    TLevelReader::define("avi", TLevelReaderAvi::create);
    TFileType::declare("avi", TFileType::RASTER_LEVEL);
    Tiio::defineWriterProperties("avi", new Tiio::AviWriterProperties());

#endif  // _WIN32

    if (IsQuickTimeInstalled()) {
      TLevelWriter::define("mov", TLevelWriterMov::create, true);
      TLevelReader::define("mov", TLevelReaderMov::create);
      TFileType::declare("mov", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("mov", new Tiio::MovWriterProperties());

      TLevelWriter::define("3gp", TLevelWriter3gp::create, true);
      TLevelReader::define("3gp", TLevelReader3gp::create);
      TFileType::declare("3gp", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("3gp", new Tiio::MovWriterProperties());
    }

    /*
#if (defined(_WIN32) && !defined(x64))

TLevelWriter::define("pct", TLevelWriterPicPct::create, true);
TLevelReader::define("pct", TLevelReaderPicPct::create);
TFileType::declare("pct", TFileType::RASTER_LEVEL);
Tiio::defineWriterProperties("pct", new Tiio::PctWriterProperties());
TLevelWriter::define("pic", TLevelWriterPicPct::create, true);
TLevelReader::define("pic", TLevelReaderPicPct::create);
TFileType::declare("pic", TFileType::RASTER_LEVEL);

TLevelWriter::define("pict", TLevelWriterPicPct::create, true);
TLevelReader::define("pict", TLevelReaderPicPct::create);
TFileType::declare("pict", TFileType::RASTER_LEVEL);
Tiio::defineWriterProperties("pict", new Tiio::PctWriterProperties());

#endif    // _WIN32 && 32-bit
*/
  }
}
