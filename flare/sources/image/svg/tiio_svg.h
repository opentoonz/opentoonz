#pragma once

#ifndef TTIO_SVG_INCLUDED
#define TTIO_SVG_INCLUDED

#include "tlevel_io.h"
#include <QCoreApplication>

#endif

class TLevelReaderSvg final : public TLevelReader {
public:
  TLevelReaderSvg(const TFilePath &path);
  ~TLevelReaderSvg() {}

  TLevelP loadInfo() override;
  TImageReaderP getFrameReader(TFrameId fid) override;

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

class SvgWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(SvgWriterProperties)
public:
  TEnumProperty m_strokeMode;
  TEnumProperty m_outlineQuality;
  SvgWriterProperties();
  void updateTranslation() override;
};
}

class TLevelWriterSvg final : public TLevelWriter {
  //! object to manage a pli
  // ParsedPli *m_pli;

  //! number of frame in pli
  //  UINT  m_frameNumber;

  //  vettore da utilizzare per il calcolo della palette
  // std::vector<TPixel> m_colorArray;

public:
  TLevelWriterSvg(const TFilePath &path, TPropertyGroup *winfo);

  TImageWriterP getFrameWriter(TFrameId fid) override;

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
