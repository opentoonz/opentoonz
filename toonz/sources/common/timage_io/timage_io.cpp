

// TnzCore includes
#include "tsystem.h"
#include "timage_io.h"
#include "trop.h"
#include "tconvert.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "tiio.h"
#include "tfilepath_io.h"

// boost includes
#include <boost/range.hpp>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

// OS-specific includes
#ifdef _WIN32
#include <io.h>
#endif
#ifdef LINUX
#include <unistd.h>
#endif

// System V includes
#include <fcntl.h>     // didn't even know Windows had them  :D
#include <sys/stat.h>  // btw, why are they here?

DEFINE_CLASS_CODE(TImageReader, 5)
DEFINE_CLASS_CODE(TImageWriter, 6)

//-----------------------------------------------------------

std::map<QString, TImageReaderCreateProc *> ImageReaderTable;
std::map<QString, std::pair<TImageWriterCreateProc *, bool>> ImageWriterTable;

//-----------------------------------------------------------

bool TImageReader::m_safeMode = false;

//-----------------------------------------------------------

TImageReader::TImageReader(const TFilePath &path)
    : TSmartObject(m_classCode)
    , m_path(path)
    , m_reader(0)
    , m_vectorReader(0)
    , m_readGreytones(true)
    , m_file(NULL)
    , m_is64BitEnabled(false)
    , m_shrink(1)
    , m_region(TRect()) {}

//-----------------------------------------------------------

void TImageReader::doReadGraytones(bool readThem) {
  m_readGreytones = readThem;
}

//-----------------------------------------------------------

TImageReader::~TImageReader() { close(); }

//-----------------------------------------------------------

bool TImageReader::isOpen() const { return m_file != NULL; }

//-----------------------------------------------------------

void TImageReader::close() {
  delete m_reader;
  delete m_vectorReader;

  if (m_file != NULL) {
    int err = 0;
    err     = fclose(m_file);

    assert(err == 0);  // il file e' stato chiuso senza errori
  }

  m_file         = NULL;
  m_reader       = 0;
  m_vectorReader = 0;
}

//-----------------------------------------------------------

/*!
  Opens for reading using \b fopen().
*/
void TImageReader::getTzpPaletteColorNames(
    std::map<int, std::pair<std::string, std::string>> &pltColorNames) {
  if (!m_file) open();

  if (!m_file) return;

  assert(m_reader);
  m_reader->getTzpPaletteColorNames(pltColorNames);
}

//-----------------------------------------------------------

void TImageReader::open() {
  assert(m_file == NULL);

  std::string type = toLower(m_path.getType());
  m_file = fopen(m_path, "rb");  // Opens for reading. If the file does not
                                 // exist or cannot be found, the fopen_s call
                                 // fails

  if (m_file == NULL)  // Non dovrebbe mai andarci!
    close();  // close() chiude il file se e' aperto e setta m_file a NULL.
  else {
    try {
      m_reader = Tiio::makeReader(type);
      if (m_reader)
        m_reader->open(m_file);
      else {
        m_vectorReader = Tiio::makeVectorReader(type);
        if (m_vectorReader)
          m_vectorReader->open(m_file);
        else
          throw TImageException(m_path, "Image format not supported");
      }
    } catch (TException &e) {
      close();  // close() chiude il file e setta m_file a NULL.
      QString msg = QString::fromStdWString(e.getMessage());
      if (msg == QString("Old 4.1 Palette")) throw e;
    } catch (std::string str) {
      if (str == "Tiff file closed") m_file = NULL;
    }
  }
}

//===========================================================

void TImageReader::setShrink(int shrink) { m_shrink = shrink; }

//-----------------------------------------------------------

TImageReaderP::TImageReaderP(const TFilePath &path) {
  m_pointer = new TImageReader(path);
  m_pointer->addRef();
  m_pointer->open();
}

//-----------------------------------------------------------

TImageP TImageReader::load() {
  TImageP image = load0();
  if (!image) return TImageP();

  TImageInfo info = m_reader->getImageInfo();
  if (info.m_lx <= 0 || info.m_ly <= 0) return TImageP();

  return image;
}

//============================================================

template <typename Pix>
struct pixel_traits {};

template <>
struct pixel_traits<TPixel32> {
  typedef TPixel32 rgbm_pixel_type;
  typedef char buffer_type;
};

template <>
struct pixel_traits<TPixel64> {
  typedef TPixel64 rgbm_pixel_type;
  typedef short buffer_type;
};

template <>
struct pixel_traits<TPixelGR8> {
  typedef TPixel32 rgbm_pixel_type;
  typedef char buffer_type;
};

template <>
struct pixel_traits<TPixelGR16> {
  typedef TPixel64 rgbm_pixel_type;
  typedef short buffer_type;
};

template <>
struct pixel_traits<TPixelCM32> {
  typedef TPixel32 rgbm_pixel_type;
  typedef char buffer_type;
};

//------------------------------------------------------------------------------------

template <typename Pix>
void copyLine(typename pixel_traits<Pix>::rgbm_pixel_type *lineIn, Pix *lineOut,
              int x0, int length, int shrink) {
  lineIn += x0;

  for (int i = 0; i < length; ++i, lineIn += shrink, ++lineOut)
    memcpy(lineOut, lineIn, sizeof(Pix));
}

template <>
void copyLine<TPixelGR8>(TPixel32 *lineIn, TPixelGR8 *lineOut, int x0,
                         int length, int shrink) {
  lineIn += x0;

  for (int i = 0; i < length; ++i, lineIn += shrink, ++lineOut)
    lineOut->value = lineIn->r;
}

template <>
void copyLine<TPixelGR16>(TPixel64 *lineIn, TPixelGR16 *lineOut, int x0,
                          int length, int shrink) {
  lineIn += x0;

  for (int i = 0; i < length; ++i, lineIn += shrink, ++lineOut)
    lineOut->value = lineIn->r;
}

//------------------------------------------------------------------------------------

template <typename Pix>
void readRaster_copyLines(const TRasterPT<Pix> &ras, Tiio::Reader *reader,
                          int x0, int y0, int x1, int y1, int inLx, int inLy,
                          int shrink) {
  typedef typename pixel_traits<Pix>::buffer_type buffer_type;
  typedef typename pixel_traits<Pix>::rgbm_pixel_type rgbm_pixel_type;

  int linesToSkip = shrink - 1;

  buffer_type *lineBuffer =
      (buffer_type *)malloc(inLx * sizeof(rgbm_pixel_type));
  if (!lineBuffer) return;

  rgbm_pixel_type *lineIn = (rgbm_pixel_type *)lineBuffer;

  if (reader->getRowOrder() == Tiio::BOTTOM2TOP) {
    int start = reader->skipLines(y0);
    int stop  = y1 + 1;

    for (int y = start; y < stop; ++y) {
      reader->readLine(lineBuffer, x0, x1, shrink);

      if (y >= y0 && y <= y1 && (y - y0) % shrink == 0) {
        Pix *line = (Pix *)ras->getRawData(0, (y - y0) / shrink);
        copyLine<Pix>(lineIn, line, x0, ras->getLx(), shrink);
      }

      if (linesToSkip > 0 && y + linesToSkip < inLy)
        y += reader->skipLines(linesToSkip);
    }
  } else  // TOP2BOTTOM
  {
    reader->skipLines(inLy - y1 - 1);

    for (int y = y1; y >= y0; --y) {
      reader->readLine(lineBuffer, x0, x1, shrink);

      if ((y - y0) % shrink == 0) {
        Pix *line = (Pix *)ras->getRawData(0, (y - y0) / shrink);
        copyLine<Pix>(lineIn, line, x0, ras->getLx(), shrink);
      }

      if (linesToSkip > 0 && y - linesToSkip > 0)
        y -= reader->skipLines(linesToSkip);
    }
  }

  free(lineBuffer);
}

//------------------------------------------------------------------------------------

template <typename Pix>
void readRaster(const TRasterPT<Pix> &ras, Tiio::Reader *reader, int x0, int y0,
                int x1, int y1, int inLx, int inLy, int shrink) {
  typedef typename pixel_traits<Pix>::buffer_type buffer_type;

  if (shrink == 1) {
    // Direct read
    ras->lock();

    ptrdiff_t linePad = -x0 * ras->getPixelSize();

    if (reader->getRowOrder() == Tiio::BOTTOM2TOP) {
      int start = reader->skipLines(y0);
      int stop  = y1 + 1;

      for (int y = start; y < stop; ++y)
        if (y >= y0 && y <= y1) {
          buffer_type *line =
              (buffer_type *)(ras->getRawData(0, y - y0) + linePad);
          reader->readLine(line, x0, x1, 1);
        }
    } else  // TOP2BOTTOM
    {
      reader->skipLines(inLy - y1 - 1);

      for (int y = y1; y >= y0; --y) {
        buffer_type *line =
            (buffer_type *)(ras->getRawData(0, y - y0) + linePad);
        reader->readLine(line, x0, x1, 1);
      }
    }

    ras->unlock();
  } else
    readRaster_copyLines(ras, reader, x0, y0, x1, y1, inLx, inLy, shrink);
}

//------------------------------------------------------------------------------------

TImageP TImageReader::load0() {
  if (!m_reader && !m_vectorReader) open();

  if (m_reader) {
    TImageInfo info = m_reader->getImageInfo();
    if (info.m_lx <= 0 || info.m_ly <= 0) return TImageP();

    // Initialize raster info
    assert(m_shrink > 0);

    // Build loading rect
    int x0 = 0;
    int x1 = info.m_lx - 1;
    int y0 = 0;
    int y1 = info.m_ly - 1;

    if (!m_region.isEmpty()) {
      // Intersect with the externally specified loading region

      x0 = std::max(x0, m_region.x0);
      y0 = std::max(y0, m_region.y0);
      x1 = std::min(x1, m_region.x1);
      y1 = std::min(y1, m_region.y1);

      if (x0 >= x1 || y0 >= y1) return TImageP();
    }

    if (m_shrink > 1) {
      // Crop to shrink multiples
      x1 -= (x1 - x0) % m_shrink;
      y1 -= (y1 - y0) % m_shrink;
    }

    assert(x0 <= x1 && y0 <= y1);

    TDimension imageDimension =
        TDimension((x1 - x0) / m_shrink + 1, (y1 - y0) / m_shrink + 1);

    if (m_path.getType() == "tzp" || m_path.getType() == "tzu") {
      // Colormap case

      TRasterCM32P ras(imageDimension);
      readRaster(ras, m_reader, x0, y0, x1, y1, info.m_lx, info.m_ly, m_shrink);

      // Build the savebox
      TRect saveBox(info.m_x0, info.m_y0, info.m_x1, info.m_y1);
      if (!m_region.isEmpty()) {
        // Intersect with the loading rect
        if (m_region.overlaps(saveBox)) {
          saveBox *= m_region;

          int sbx0 = saveBox.x0 - m_region.x0;
          int sby0 = saveBox.y0 - m_region.y0;
          int sbx1 = sbx0 + saveBox.getLx() - 1;
          int sby1 = sby0 + saveBox.getLy() - 1;
          assert(sbx0 >= 0);
          assert(sby0 >= 0);
          assert(sbx1 >= 0);
          assert(sby1 >= 0);

          saveBox = TRect(sbx0, sby0, sbx1, sby1);
        } else
          saveBox = TRect(0, 0, 1, 1);
      }

      if (m_shrink > 1) {
        saveBox.x0 = saveBox.x0 / m_shrink;
        saveBox.y0 = saveBox.y0 / m_shrink;
        saveBox.x1 = saveBox.x1 / m_shrink;
        saveBox.y1 = saveBox.y1 / m_shrink;
      }

      TToonzImageP ti(ras, ras->getBounds() * saveBox);
      ti->setDpi(info.m_dpix, info.m_dpiy);

      return ti;
    } else if (info.m_bitsPerSample >= 8) {
      // Common byte-based rasters (see below, we have black-and-white bit-based
      // images too)

      if (info.m_samplePerPixel == 1 && m_readGreytones) {
        //  Greymap case
        // NOTE: Uses a full 32-bit raster first, and then copies down to the
        // GR8

        // Observe that GR16 file images get immediately down-cast to GR8...
        // Should we implement that too?

        TRasterGR8P ras(imageDimension);
        readRaster_copyLines(ras, m_reader, x0, y0, x1, y1, info.m_lx,
                             info.m_ly, m_shrink);

        TRasterImageP ri(ras);
        ri->setDpi(info.m_dpix, info.m_dpiy);

        return ri;
      }

      // assert(info.m_samplePerPixel == 3 || info.m_samplePerPixel == 4);

      TRasterP _ras;
      if (info.m_bitsPerSample == 16) {
        if (m_is64BitEnabled || m_path.getType() != "tif") {
          //  Standard 64-bit case.

          // Also covers down-casting to 32-bit from a 64-bit image file
          // whenever
          // not a tif file (see below).

          TRaster64P ras(imageDimension);
          readRaster(ras, m_reader, x0, y0, x1, y1, info.m_lx, info.m_ly,
                     m_shrink);

          _ras = ras;
        } else {
          // The Tif reader has got an automatically down-casting
          // readLine(char*, ...)
          // in case the input file is 64-bit. The advantage is that no
          // intermediate
          // 64-bit raster is required in this case.

          TRaster32P ras(imageDimension);
          readRaster(ras, m_reader, x0, y0, x1, y1, info.m_lx, info.m_ly,
                     m_shrink);

          _ras = ras;
        }
      } else if (info.m_bitsPerSample == 8) {
        //  Standard 32-bit case
        TRaster32P ras(imageDimension);
        readRaster(ras, m_reader, x0, y0, x1, y1, info.m_lx, info.m_ly,
                   m_shrink);

        _ras = ras;
      } else
        throw TImageException(m_path, "Image format not supported");

      // 64-bit to 32-bit conversions if necessary  (64 bit images not allowed)
      if (!m_is64BitEnabled && (TRaster64P)_ras) {
        TRaster32P raux(_ras->getLx(), _ras->getLy());
        TRop::convert(raux, _ras);
        _ras = raux;
      }

      // Return the image
      TRasterImageP ri(_ras);
      ri->setDpi(info.m_dpix, info.m_dpiy);

      return ri;
    } else if (info.m_samplePerPixel == 1 && info.m_valid == true) {
      //  Previously dubbed as 'Palette cases'. No clue about what is this... :|

      TRaster32P ras(imageDimension);
      readRaster(ras, m_reader, x0, y0, x1, y1, info.m_lx, info.m_ly, m_shrink);

      TRasterImageP ri(ras);
      ri->setDpi(info.m_dpix, info.m_dpiy);

      return ri;
    } else if (info.m_samplePerPixel == 1 && m_readGreytones) {
      //  Black-and-White case, I guess. Standard greymaps were considered
      //  above...

      TRasterGR8P ras(imageDimension);
      readRaster_copyLines(ras, m_reader, x0, y0, x1, y1, info.m_lx, info.m_ly,
                           m_shrink);

      TRasterImageP ri(ras);
      ri->setDpi(info.m_dpix, info.m_dpiy);

      if (info.m_bitsPerSample == 1)  // I suspect this always evaluates true...
        ri->setScanBWFlag(true);

      return ri;
    } else
      return TImageP();
  } else if (m_vectorReader) {
    TVectorImage *vi = m_vectorReader->read();
    return TVectorImageP(vi);
  } else
    return TImageP();
}

//-----------------------------------------------------------

TImageWriter::TImageWriter(const TFilePath &path)
    : TSmartObject(m_classCode)
    , m_path(path)
    , m_writer(0)
    , m_vectorWriter(0)
    , m_properties(0) {}

//-----------------------------------------------------------

TImageWriter::~TImageWriter() {
  delete m_writer;
  delete m_vectorWriter;
  delete m_properties;
}

//-----------------------------------------------------------

void TImageWriter::setProperties(const TPropertyGroup *g) {
  if (m_properties) {
    assert(m_properties != g);
    delete m_properties;
  }
  m_properties = g ? g->clone() : 0;
}

//-----------------------------------------------------------

static void convertForWriting(TRasterP &ras, const TRasterP &rin, int bpp) {
  switch (bpp) {
  case 1:
  case 8:
    ras = TRasterGR8P(rin->getSize());
    TRop::convert(ras, rin);
    break;
  case 24:
  case 32:
    ras = TRaster32P(rin->getSize());
    TRop::convert(ras, rin);
    break;
  case 48:
  case 64:
    ras = TRaster64P(rin->getSize());
    TRop::convert(ras, rin);
    break;
  default:
    assert(false);
  }
}

//-----------------------------------------------------------

void TImageWriter::save(const TImageP &img) {
  const std::string &type = toLower(m_path.getType());

  Tiio::Writer *writer = Tiio::makeWriter(type);
  if (!writer)
    throw TImageException(m_path, "unsupported format for raster images");

  writer->setProperties(m_properties);

  FILE *file = fopen(m_path, "wb");
  if (file == NULL) throw TImageException(m_path, "Can't write file");

  if (TRasterImageP ri = img) {
    TImageInfo info;
    TRasterP ras;

    TRasterGR8P rasGr = ri->getRaster();
    TRaster32P ras32  = ri->getRaster();
    TRaster64P ras64  = ri->getRaster();

    TEnumProperty *p =
        m_properties
            ? (TEnumProperty *)m_properties->getProperty("Bits Per Pixel")
            : 0;

    if (p && ri->isScanBW()) {
      const std::vector<std::wstring> &range = p->getRange();
      p->setValue(range[2]);  // Horrible. See tiio_tif.cpp (732 or near) -.-'
    }

    int bpp = p ? std::stoi(p->getValue()) : 32;

    //  bpp       1  8  16 24 32 40  48 56  64
    int spp[] = {
        1, 1, 1, 4, 4,
        0, 4, 0, 4};  // 0s are for pixel sizes which are normally unsupported
    int bps[] = {
        1, 8,  16, 8, 8,
        0, 16, 0,  16};  // by image formats, let alone by Toonz raster ones.
    // The 24 and 48 cases get automatically promoted to 32 and 64.
    int bypp = bpp / 8;
    assert(bypp < boost::size(spp) && spp[bypp] && bps[bypp]);

    info.m_samplePerPixel = spp[bypp];
    info.m_bitsPerSample  = bps[bypp];

    if (rasGr) {
      if (bypp < 2)   // Seems 16 bit greymaps are not handled... why?
        ras = rasGr;  // we do have a Toonz raster for those...    >:|
      else
        convertForWriting(ras, rasGr, bpp);
    } else if (ras32) {
      if (bpp == 32 || bpp == 24)
        ras = ras32;
      else
        convertForWriting(ras, ras32, bpp);
    } else if (ras64) {
      if (bpp == 64 || bpp == 48)
        ras = ras64;
      else
        convertForWriting(ras, ras64, bpp);
    } else {
      fclose(file);
      throw TImageException(m_path, "unsupported raster type");
    }

    info.m_lx = ras->getLx();
    info.m_ly = ras->getLy();

    ri->getDpi(info.m_dpix, info.m_dpiy);

    if (writer->getProperties() && m_properties)
      writer->getProperties()->setProperties(m_properties);

    writer->open(file, info);

    ras->lock();

    if (writer->getRowOrder() == Tiio::BOTTOM2TOP) {
      if (bpp == 1 || bpp == 8 || bpp == 24 || bpp == 32 || bpp == 16)
        for (int i = 0; i < ras->getLy(); i++)
          writer->writeLine((char *)ras->getRawData(0, i));
      else
        for (int i = 0; i < ras->getLy(); i++)
          writer->writeLine((short *)ras->getRawData(0, i));
    } else {
      if (bpp == 1 || bpp == 8 || bpp == 24 || bpp == 32 || bpp == 16)
        for (int i = ras->getLy() - 1; i >= 0; i--)
          writer->writeLine((char *)ras->getRawData(0, i));
      else
        for (int i = ras->getLy() - 1; i >= 0; i--)
          writer->writeLine((short *)ras->getRawData(0, i));
    }

    ras->unlock();

    writer->flush();
    delete writer;
  } else if (TVectorImageP vi = img) {
    Tiio::VectorWriter *writer = Tiio::makeVectorWriter(type);
    if (!writer) {
      fclose(file);
      throw TImageException(m_path, "unsupported format for vector images");
    }

    writer->open(file);
    writer->write(vi.getPointer());

    delete writer;
  } else {
    fclose(file);
    throw TImageException(m_path, "Can't write file");
  }

  fclose(file);
}

//-----------------------------------------------------------

TImageWriterP::TImageWriterP(const TFilePath &path) {
  m_pointer = new TImageWriter(path);
  m_pointer->addRef();
}

//============================================================
//
// Helper functions statiche
//
//============================================================

bool TImageReader::load(const TFilePath &path, TRasterP &raster) {
  raster = TRasterP();
  TImageReaderP ir(path);
  if (!ir) return false;
  TImageP img = ir->load();
  if (!img) return false;

  TRasterImageP ri(img);
  if (!ri) return false;

  raster = ri->getRaster();
  return true;
}

//-----------------------------------------------------------

//-----------------------------------------------------------

void TImageReader::load(const TRasterP &ras, const TPoint &pos, int shrinkX,
                        int shrinkY) {
  TImageP srcImage          = load();
  TRasterImageP srcRasImage = srcImage;
  TRaster32P srcRaster      = srcRasImage->getRaster();
  /*
TRaster32P clippedRas = srcRaster->extractT
  (shrinkX*pos.x, shrinkY*pos.y,
  (pos.x + ras->getLx()) * shrinkX - 1, (pos.y + ras->getLy()) * shrinkY - 1);

if (shrinkX != 1 || shrinkY != 1)
{
TRaster32P ras32 = ras;
if (ras32)
ras32->fill(TPixel32::Transparent);
TRop::resample(ras, clippedRas, TScale(1./shrinkX, 1./shrinkY),
TRop::ClosestPixel);
}
else*/
  ras->copy(srcRaster);
}

//-----------------------------------------------------------

bool TImageReader::load(const TFilePath &path, TImageP &image) {
  image = TImageReaderP(path)->load();
  return image;
}

//-----------------------------------------------------------

void TImageWriter::save(const TFilePath &path, TRasterP raster) {
  TRasterImageP rasImage(raster);
  TImageWriterP(path)->save(TImageP(rasImage));
}

//-----------------------------------------------------------

void TImageWriter::save(const TFilePath &path, const TImageP &image) {
  TImageWriterP(path)->save(image);
}

//===========================================================
//
// funzioni per la registrazione dei formati (chiamate dal Plugin)
//
//===========================================================

void TImageReader::define(QString extension, TImageReaderCreateProc *proc) {
  ImageReaderTable[extension] = proc;
}

//-----------------------------------------------------------

void TImageWriter::define(QString extension, TImageWriterCreateProc *proc,
                          bool isRenderFormat) {
  ImageWriterTable[extension] =
      std::pair<TImageWriterCreateProc *, bool>(proc, isRenderFormat);
}

//-----------------------------------------------------------

void TImageReader::getSupportedFormats(QStringList &names) {
  for (std::map<QString, TImageReaderCreateProc *>::iterator it =
           ImageReaderTable.begin();
       it != ImageReaderTable.end(); ++it) {
    names.push_back(it->first);
  }
}

//-----------------------------------------------------------

void TImageWriter::getSupportedFormats(QStringList &names,
                                       bool onlyRenderFormat) {
  for (std::map<QString, std::pair<TImageWriterCreateProc *, bool>>::iterator
           it = ImageWriterTable.begin();
       it != ImageWriterTable.end(); ++it) {
    if (!onlyRenderFormat || it->second.second) names.push_back(it->first);
  }
}

//-----------------------------------------------------------

const TImageInfo *TImageReader::getImageInfo() const {
  if (m_reader)
    return &(m_reader->getImageInfo());
  else
    return 0;
}

//-----------------------------------------------------------

//===========================================================
//
// Eccezioni
//
//===========================================================

TImageException::TImageException(const TFilePath &fp, const std::string &msg)
    : TException(msg), m_fp(fp) {}

//-----------------------------------------------------------

TString TImageException::getMessage() const {
  return m_fp.getWideString() + L": " + TException::getMessage();
}

//===========================================================

TImageVersionException::TImageVersionException(const TFilePath &fp, int major,
                                               int minor)
    : TException(L"The file " + fp.getWideString() +
                 L" was generated by a newer version of OpenToonz and cannot "
                 L"be loaded.")
    , m_fp(fp)
    , m_major(major)
    , m_minor(minor) {}
