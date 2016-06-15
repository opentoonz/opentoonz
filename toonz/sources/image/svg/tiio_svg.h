#pragma once

#ifndef TTIO_SVG_INCLUDED
#define TTIO_SVG_INCLUDED

#include "tlevel_io.h"

#endif

class TLevelReaderSvg : public TLevelReader {
public:
  TLevelReaderSvg(const TFilePath &path);
  ~TLevelReaderSvg() {}

  TLevelP loadInfo();
  TImageReaderP getFrameReader(TFrameId fid);

  // QString getCreator();
  // friend class TImageReaderPli;

private:
  TLevelP m_level;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderSvg(f);
  }

private:
  // not implemented
  TLevelReaderSvg(const TLevelReaderSvg &);
  TLevelReaderSvg &operator=(const TLevelReaderSvg &);
};

namespace Tiio {

class SvgWriterProperties : public TPropertyGroup {
public:
  TEnumProperty m_strokeMode;
  TEnumProperty m_outlineQuality;
  SvgWriterProperties();
};
}

class TLevelWriterSvg : public TLevelWriter {
  //! object to manage a pli
  // ParsedPli *m_pli;

  //! number of frame in pli
  //  UINT  m_frameNumber;

  //  vettore da utilizzare per il calcolo della palette
  // std::vector<TPixel> m_colorArray;

public:
  TLevelWriterSvg(const TFilePath &path, TPropertyGroup *winfo);

  TImageWriterP getFrameWriter(TFrameId fid);

  friend class TImageWriterSvg;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterSvg(f, winfo);
  }

private:
  // not implemented
  TLevelWriterSvg(const TLevelWriterSvg &);
  TLevelWriterSvg &operator=(const TLevelWriterSvg &);
};
