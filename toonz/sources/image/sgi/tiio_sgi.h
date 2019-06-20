#pragma once

#ifndef TIIO_SGI_INCLUDED
#define TIIO_SGI_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"
#include <QCoreApplication>

namespace Tiio {

Tiio::ReaderMaker makeSgiReader;
Tiio::WriterMaker makeSgiWriter;

class SgiWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(SgiWriterProperties)
public:
  TEnumProperty m_pixelSize;
  TBoolProperty m_compressed;
  TEnumProperty m_endianess;
  SgiWriterProperties();
  void updateTranslation() override;
};

}  // namespace

#endif
