#pragma once

#ifndef TIIO_PNG_INCLUDED
#define TIIO_PNG_INCLUDED

#include "tiio.h"
// #include "timage_io.h"
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
  TPointerProperty m_colormap;

  PngWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

Tiio::Reader *makePngReader();
Tiio::Writer *makePngWriter();

}  // namespace Tiio

#endif  // TIIO_PNG_INCLUDED
