#pragma once

#ifndef TTIO_JPG_INCLUDED
#define TTIO_JPG_INCLUDED

#include "tiio.h"
#include "tproperty.h"

#include <QCoreApplication>
#include <cstdio>

extern "C" {
#define XMD_H
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
  struct jpeg_decompress_struct m_cinfo;
  struct jpeg_error_mgr m_jerr;
  FILE *m_chan;         // file pointer
  JSAMPARRAY m_buffer;  // temporary scanline buffer
  bool m_isOpen;        // true if file is opened successfully

  // === Added members to track decoding state ===
  bool m_errorOccurred;      // true if an error occurred during reading
  int m_currentLine;         // current scanline index
  bool m_decompressCreated;  // true if jpeg_create_decompress has been called
  // ============================================

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

DVAPI Tiio::ReaderMaker makeJpgReader;
DVAPI Tiio::WriterMaker makeJpgWriter;

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
