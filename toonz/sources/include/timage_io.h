#pragma once

#ifndef TIMAGE_IO_INCLUDED
#define TIMAGE_IO_INCLUDED

// #include "trasterimage.h"
// #include "texception.h"
// #include "tfilepath.h"
#include <QStringList>

#include "tfilepath_io.h"
#include "traster.h"
#include "timage.h"

#undef DVAPI
#undef DVVAR
#ifdef TIMAGE_IO_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
namespace Tiio {
class Reader;
class Writer;
class VectorReader;
class VectorWriter;
}  // namespace Tiio
class TPropertyGroup;
class TImageInfo;

//===========================================================

class DVAPI TImageException final : public TException {
  TFilePath m_fp;

public:
  TImageException(const TFilePath &fp, const std::string &msg);
  ~TImageException() {}

  TString getMessage() const override;
  TString getRawMessage() const { return TException::getMessage(); }
  const TFilePath &getFilePath() const { return m_fp; }
};

//===========================================================

class DVAPI TImageVersionException final : public TException {
  TFilePath m_fp;
  int m_major, m_minor;

public:
  TImageVersionException(const TFilePath &fp, int major, int minor);
  ~TImageVersionException() {}

  void getVersion(int &major, int &minor) const {
    major = m_major;
    minor = m_minor;
  }
  int getMajor() const { return m_major; }
  int getMinor() const { return m_minor; }
};

//===========================================================
//
// Image Reader
//
//===========================================================

class TImageReader;
typedef TImageReader *TImageReaderCreateProc(const TFilePath &path);

//===========================================================

//! \include imgReader_ex.cpp

class DVAPI TImageReader : public TSmartObject {
  DECLARE_CLASS_CODE

  friend class TImageReaderP;

protected:
  // std::fstream m_stream;
  TFilePath m_path;
  FILE *m_file;
  Tiio::Reader *m_reader;
  Tiio::VectorReader *m_vectorReader;
  void open();
  void close();
  bool isOpen() const;
  bool m_readGreytones;
  bool m_is64BitEnabled;
  bool m_isFloatEnabled;
  int m_shrink;
  TRect m_region;
  static bool m_safeMode;
  double m_colorSpaceGamma;

public:
  static void setSafeModeReadingForTzl(bool activated) {
    m_safeMode = activated;
  }
  TImageReader(const TFilePath &path);
  virtual ~TImageReader();

private:
  // not implemented
  TImageReader(const TImageReader &);
  TImageReader &operator=(const TImageReader &src);
  TImageP load0();

public:
  // TImageReader keeps ownership: DO NOT DELETE
  virtual const TImageInfo *getImageInfo() const;

  TFilePath getFilePath() const { return m_path; };

  TPropertyGroup *getProperties();
  void setProperties(const TPropertyGroup *);
  /*! Load Image from disk.
If set region then loads only image's region.
          If set shrink then loads image (or region, if set) with shrink.

Note: if the region, or part of it, is not contained in the image
    then returns only the intersection between the image and region.
          If the intersection is void returns TImageP();
*/
  virtual TImageP load();

  void load(const TRasterP &ras, const TPoint &pos = TPoint(0, 0),
            int shrinkX = 1, int shrinkY = 1);

  static bool load(const TFilePath &, TRasterP &);
  static bool load(const TFilePath &, TImageP &);

  // Only for TLV (version 13). Return the icon in TLV file.
  virtual TImageP loadIcon() { return TImageP(); }

  static void getSupportedFormats(QStringList &names);

  // TDimension getSize() const;
  // virtual TRect getBBox() const = 0;

  static void define(QString extension, TImageReaderCreateProc *proc);

  void doReadGraytones(bool readThem);
  void enable16BitRead(bool is64bitEnabled) {
    m_is64BitEnabled = is64bitEnabled;
  }
  void enableFloatRead(bool isFloatEnabled) {
    m_isFloatEnabled = isFloatEnabled;
  }

  int getShrink() const { return m_shrink; }
  /*!
     Setta lo shrink in modo che al load() viene letta da disco
           una riga ogni "shrink" righe ed all'interno di queste righe viene
     letto un pixel ogni "shrink" pixel.
           La prima riga viene sempre letta.
           Il primo pixel di una riga letta viene sempre preso.
     Nota che se shrink=1 non viene saltata alcuna riga ne pixel.
  */
  void setShrink(int shrink);

  /*!
Set image's region.
Region dimension doesn't consider shrink.
*/
  void setRegion(TRect rect) { m_region = rect; }
  /*!
Returns the image region.
Region dimension doesn't consider shrink
*/
  TRect getRegion() const { return m_region; }

  void getTzpPaletteColorNames(
      std::map<int, std::pair<std::string, std::string>>
          &pltColorNames);  // colorindex(<256: paint), pagename, colorname

  void setColorSpaceGamma(const double colorSpaceGamma) {
    m_colorSpaceGamma = colorSpaceGamma;
  }
};

//-----------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TImageReader>;
#endif

class DVAPI TImageReaderP final : public TSmartPointerT<TImageReader> {
public:
  TImageReaderP(TImageReader *ir) : TSmartPointerT<TImageReader>(ir){};
  // il costruttore "non banale"
  TImageReaderP(const TFilePath &filepath);
};

//===========================================================
//
// Image Writer
//
//===========================================================

class TImageWriter;
typedef TImageWriter *TImageWriterCreateProc(const TFilePath &path);

//===========================================================

//! \include imgWriter_ex.cpp

class DVAPI TImageWriter : public TSmartObject {
  DECLARE_CLASS_CODE

  // Background color for saving transparent pixel to the format not
  // supporting alpha channel. Specified in the preferences.
  static TPixel32 m_backgroundColor;

protected:
  // std::fstream m_stream;
  TFilePath m_path;

  Tiio::Writer *m_writer;
  Tiio::VectorWriter *m_vectorWriter;
  TPropertyGroup *m_properties;

public:
  TImageWriter(const TFilePath &path);
  virtual ~TImageWriter();

private:
  // not implemented
  TImageWriter(const TImageWriter &);
  TImageWriter &operator=(const TImageWriter &src);

public:
  void setFilePath(const TFilePath &path) { m_path = path; }
  TFilePath getFilePath() const { return m_path; };

  // don't get ownership
  void setProperties(const TPropertyGroup *);

  virtual void save(const TImageP &img);
  virtual bool is64bitOutputSupported() { return true; }
  static void save(const TFilePath &, TRasterP);
  static void save(const TFilePath &, const TImageP &);

  static void getSupportedFormats(QStringList &names, bool onlyRenderFormats);

  static void define(QString extension, TImageWriterCreateProc *proc,
                     bool isRenderFormat);

  static void setBackgroundColor(TPixel32 color);
  static TPixel32 getBackgroundColor();
};

//-----------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TImageWriter>;
#endif

class DVAPI TImageWriterP final : public TSmartPointerT<TImageWriter> {
public:
  TImageWriterP(TImageWriter *iw) : TSmartPointerT<TImageWriter>(iw){};
  // il costruttore "non banale"
  TImageWriterP(const TFilePath &filepath);
};

//===========================================================
//
// Image ReaderWriter
//
//===========================================================

class TImageReaderWriter;
typedef TImageReaderWriter *TImageReaderWriterCreateProc(const TFilePath &path);

//===========================================================

#endif
