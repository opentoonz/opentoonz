#pragma once

#ifndef TTIO_JPG_INCLUDED
#define TTIO_JPG_INCLUDED

#include "tiio.h"
#include "tproperty.h"

#include <QCoreApplication>
#include <cstdio>

extern "C" {
#ifndef XMD_H
#define XMD_H
#endif
#include "jpeglib.h"

/*
 * <setjmp.h> is used for the optional error recovery mechanism shown in
 * the second part of the example.
 */
#include <setjmp.h>
}

#undef DVAPI
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace Tiio {

//=========================================================
// JpgReader
//=========================================================

class DVAPI JpgReader final : public Tiio::Reader {
public:
  // Structure for the safety error mechanism (setjmp)
  struct tnz_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
  };

private:
  struct jpeg_decompress_struct m_cinfo;
  struct tnz_error_mgr m_jerr;

  FILE *m_chan;         // file pointer
  JSAMPARRAY m_buffer;  // temporary scanline buffer

  bool m_isOpen;             // true if file is opened successfully
  bool m_errorOccurred;      // true if an error occurred during reading
  bool m_decompressCreated;  // true if jpeg_create_decompress has been called
  int m_currentLine;         // current scanline index

public:
  JpgReader();
  ~JpgReader();

  // Returns the scanline order (top-to-bottom for JPEG)
  Tiio::RowOrder getRowOrder() const override;

  // Opens a JPEG file for reading
  void open(FILE *file) override;

  // Reads a scanline; fills checkerboard if invalid/corrupted
  void readLine(char *buffer, int x0, int x1, int shrink) override;

  // Skips a number of scanlines
  int skipLines(int lineCount) override;
};

//=========================================================
// Factory functions
//=========================================================

DVAPI Tiio::Reader *makeJpgReader();
DVAPI Tiio::Writer *makeJpgWriter();

//=========================================================
// JpgWriterProperties
//=========================================================

class DVAPI JpgWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(JpgWriterProperties)

public:
  TIntProperty m_quality;
  TIntProperty m_smoothing;

  static const std::string QUALITY;

  JpgWriterProperties()
      : m_quality(QUALITY, 0, 100, 90), m_smoothing("Smoothing", 0, 100, 0) {
    bind(m_quality);
    bind(m_smoothing);
  }

  void updateTranslation() override;
};

}  // namespace Tiio

#endif
