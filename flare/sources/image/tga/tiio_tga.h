#pragma once

#ifndef TIIO_TGA_INCLUDED
#define TIIO_TGA_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"
#include <QCoreApplication>

//===========================================================================

namespace Tiio {

//===========================================================================

class TgaWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(TgaWriterProperties)
public:
  TEnumProperty m_pixelSize;  // 16,24,32
  TBoolProperty m_compressed;

  TgaWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

Tiio::Reader *makeTgaReader();
Tiio::Writer *makeTgaWriter();

//===========================================================================

}  // namespace

#endif
