#pragma once

#ifndef TIIO_INCLUDED
#define TIIO_INCLUDED

#include "tcommon.h"
#include <string>
#include <QStringList>
#include "timageinfo.h"

#undef DVAPI
#undef DVVAR

#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TVectorImage;
class TPropertyGroup;

namespace Tiio {

//-------------------------------------------------------------------

enum RowOrder { BOTTOM2TOP, TOP2BOTTOM };

//-------------------------------------------------------------------

//-------------------------------------------------------------------

class DVAPI Exception {};

//-------------------------------------------------------------------

class DVAPI Reader {
protected:
  TImageInfo m_info;

public:
  Reader();
  virtual ~Reader();

  virtual void open(FILE *file) = 0;

  const TImageInfo &getImageInfo() const { return m_info; }

  virtual TPropertyGroup *getProperties() const { return 0; }

  void readLine(char *buffer) { readLine(buffer, 0, m_info.m_lx - 1, 1); }
  void readLine(short *buffer) { readLine(buffer, 0, m_info.m_lx - 1, 1); }
  virtual void readLine(char *buffer, int x0, int x1, int shrink) = 0;
  virtual void readLine(short *, int, int, int) { assert(false); }
  // Returns skipped lines number.
  // If not implemented returns 0;
  virtual int skipLines(int lineCount) = 0;

  virtual RowOrder getRowOrder() const { return BOTTOM2TOP; }
  virtual bool read16BitIsEnabled() const { return false; }

  // this function enables/disables the 64 bit reading.
  // If disabled, 64 bit images will be automatically  scaled down to 32 bit.
  // The default behaviour for formats that support 64 bit images is "Enabled"

  virtual void enable16BitRead(bool) {}

  virtual void getTzpPaletteColorNames(
      std::map<int, std::pair<std::string, std::string>> &pltColorNames) const {
    assert(false);
  }

private:
  // not implemented
  Reader(const Reader &);
  Reader &operator=(const Reader &);
};

//-------------------------------------------------------------------

class DVAPI Writer {
protected:
  TImageInfo m_info;
  TPropertyGroup *m_properties;
  static int m_bwThreshold;

public:
  static void getSupportedFormats(QStringList &formats, bool onlyRenderFormats);
  static void setBlackAndWhiteThreshold(int threshold) {
    m_bwThreshold = threshold;
  }

  Writer();
  virtual ~Writer();

  virtual void open(FILE *file, const TImageInfo &inafo) = 0;

  TPropertyGroup *getProperties() { return m_properties; }

  virtual void writeLine(char *buffer) = 0;
  virtual void writeLine(short *) { assert(false); }

  virtual void flush() {}

  virtual RowOrder getRowOrder() const { return BOTTOM2TOP; }
  virtual bool write64bitSupported() const { return false; }

  void setProperties(TPropertyGroup *properties);

private:
  // not implemented
  Writer(const Writer &);
  Writer &operator=(const Writer &);
};

//-------------------------------------------------------------------

class DVAPI VectorReader {
public:
  VectorReader() {}
  virtual ~VectorReader() {}

  virtual void open(FILE *file) = 0;

  virtual TVectorImage *read() = 0;
};

//-------------------------------------------------------------------

class DVAPI VectorWriter {
public:
  VectorWriter() {}
  virtual ~VectorWriter() {}

  virtual void open(FILE *file)      = 0;
  virtual void write(TVectorImage *) = 0;
};

//-------------------------------------------------------------------

typedef Reader *ReaderMaker();
typedef Writer *WriterMaker();

typedef VectorReader *VectorReaderMaker();
typedef VectorWriter *VectorWriterMaker();

//-------------------------------------------------------------------

DVAPI Tiio::Reader *makeReader(std::string ext);
DVAPI Tiio::Writer *makeWriter(std::string ext);

DVAPI Tiio::VectorReader *makeVectorReader(std::string ext);
DVAPI Tiio::VectorWriter *makeVectorWriter(std::string ext);

DVAPI TPropertyGroup *makeWriterProperties(std::string ext);

//-------------------------------------------------------------------

DVAPI void defineReaderMaker(const char *ext, Tiio::ReaderMaker *fn);
DVAPI void defineWriterMaker(const char *ext, Tiio::WriterMaker *fn,
                             bool isRenderFormat);

DVAPI void defineVectorReaderMaker(const char *ext,
                                   Tiio::VectorReaderMaker *fn);
DVAPI void defineVectorWriterMaker(const char *ext, Tiio::VectorWriterMaker *fn,
                                   bool isRenderFormat);

DVAPI void defineWriterProperties(const char *ext, TPropertyGroup *);

DVAPI bool isQuicktimeInstalled();

DVAPI void updateFileWritersPropertiesTranslation();

//-------------------------------------------------------------------

}  // namespace

#endif
