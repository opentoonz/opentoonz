#pragma once

#ifndef TTIO_BMP_INCLUDED
#define TTIO_BMP_INCLUDED

#include "tiio.h"
#include "tproperty.h"

#include <QCoreApplication>

#undef DVAPI
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace Tiio {

DVAPI Tiio::ReaderMaker makeBmpReader;
DVAPI Tiio::WriterMaker makeBmpWriter;

// DVAPI TPropertyGroup *makeBmpWriterProperties();

class BmpWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(BmpWriterProperties)
public:
  TEnumProperty m_pixelSize;
  // TBoolProperty m_compressed;

  BmpWriterProperties();

  void updateTranslation() override;
};

}  // namespace

#endif
