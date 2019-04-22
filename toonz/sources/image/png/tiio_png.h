#pragma once

#ifndef TTIO_PNG_INCLUDED
#define TTIO_PNG_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"
#include <QCoreApplication>

//===========================================================================

namespace Tiio {

//===========================================================================

class PngWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(PngWriterProperties)
public:
  // TEnumProperty m_pixelSize;
  TBoolProperty m_matte;

  PngWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

Tiio::Reader *makePngReader();
Tiio::Writer *makePngWriter();

}  // namespace

#endif
