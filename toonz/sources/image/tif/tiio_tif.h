#pragma once

#ifndef TTIO_TIF_INCLUDED
#define TTIO_TIF_INCLUDED

#include "tiio.h"
//#include "trandom.h"
// #include "timage_io.h"
#include "tproperty.h"

#include <QCoreApplication>

#define TNZ_INFO_COMPRESS_NONE L"None"
#define TNZ_INFO_COMPRESS_CCITTRLE L"CCITT modified Huffman Run-length encoding"
#define TNZ_INFO_COMPRESS_CCITTFAX3 L"CCITT Group 3 fax encoding"
#define TNZ_INFO_COMPRESS_CCITTFAX4 L"CCITT Group 4 fax encoding"
#define TNZ_INFO_COMPRESS_LZW L"Lempel-Ziv & Welch encoding"
#define TNZ_INFO_COMPRESS_PACKBITS L"Macintosh Run-length encoding"
#define TNZ_INFO_COMPRESS_THUNDERSCAN L"ThunderScan Run-length encoding"
#define TNZ_INFO_COMPRESS_RLE L"Run-length compression"
#define TNZ_INFO_COMPRESS_JPEG L"JPEG compression"
#define TNZ_INFO_COMPRESS_OJPEG L"JPEG compression 6.0"
#define TNZ_INFO_COMPRESS_NEXT L"NEXT 2-bit RLE"
#define TNZ_INFO_COMPRESS_TOONZ1 L"Toonz RLE"
#define TNZ_INFO_COMPRESS_UNKNOWN L"Unknown"
#define TNZ_INFO_COMPRESS_SGILOG L"SGILog"
#define TNZ_INFO_COMPRESS_SGILOG24 L"SGILog24"
#define TNZ_INFO_COMPRESS_ADOBE_DEFLATE L"8"
#define TNZ_INFO_COMPRESS_DEFLATE L"zip"

#define TNZ_INFO_ORIENT_TOPLEFT L"Top Left"
#define TNZ_INFO_ORIENT_TOPRIGHT L"Top Right"
#define TNZ_INFO_ORIENT_BOTRIGHT L"Bottom Right"
#define TNZ_INFO_ORIENT_BOTLEFT L"Bottom Left"
#define TNZ_INFO_ORIENT_LEFTTOP L"Left Top"
#define TNZ_INFO_ORIENT_LEFTBOT L"Left Bottom"
#define TNZ_INFO_ORIENT_RIGHTTOP L"Right Top"
#define TNZ_INFO_ORIENT_RIGHTBOT L"Right Bottom"
#define TNZ_INFO_ORIENT_NONE L""
//===========================================================================

namespace Tiio {

//===========================================================================

class TifWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(TifWriterProperties)
public:
  TEnumProperty m_byteOrdering;
  TEnumProperty m_compressionType;
  TEnumProperty m_bitsPerPixel;
  TEnumProperty m_orientation;

  // TBoolProperty m_matte;
  TifWriterProperties();

  void updateTranslation() override;
};

//===========================================================================

Tiio::Reader *makeTifReader();
Tiio::Writer *makeTifWriter();

Tiio::Reader *makeTziReader();

}  // namespace

#endif  // TTIO_TIF_INCLUDED
