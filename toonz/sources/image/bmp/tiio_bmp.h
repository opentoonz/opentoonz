#pragma once

#ifndef TTIO_BMP_INCLUDED
#define TTIO_BMP_INCLUDED

#include "timage_io.h"

class TImageReaderBmp : public TImageReader {
  int m_lx, m_ly;

public:
  TImageReaderBmp(const TFilePath &f) : TImageReader(f) {}
  ~TImageReaderBmp() {}

private:
  // not implemented
  TImageReaderBmp(const TImageReaderBmp &);
  TImageReaderBmp &operator=(const TImageReaderBmp &src);

public:
  TImageP load();
  // void load(const TRasterP &rasP, const TPoint &pos = TPoint(0,0), int
  // shrinkX = 1, int shrinkY = 1);

  static TImageReader *create(const TFilePath &f) {
    return new TImageReaderBmp(f);
  };

  TDimension getSize() const;
  TRect getBBox() const;
};

//===========================================================================

class TImageWriterBmp : public TImageWriter {
  int m_lx, m_ly;

public:
  TImageWriterBmp(const TFilePath &f) : TImageWriter(f) {}
  ~TImageWriterBmp() {}
  bool is64bitOutputSupported() { return false; }

private:
  // not implemented
  TImageWriterBmp(const TImageWriterBmp &);
  TImageWriterBmp &operator=(const TImageWriterBmp &src);

public:
  void save(const TImageP &);
  static TImageWriter *create(const TFilePath &f) {
    return new TImageWriterBmp(f);
  };
};

#endif
