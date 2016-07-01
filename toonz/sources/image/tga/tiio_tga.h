#pragma once

#ifndef TIIO_TGA_INCLUDED
#define TIIO_TGA_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

class TgaWriterProperties final : public TPropertyGroup {
public:
  TEnumProperty m_pixelSize;  // 16,24,32
  TBoolProperty m_compressed;

  TgaWriterProperties();
};

//===========================================================================

Tiio::Reader *makeTgaReader();
Tiio::Writer *makeTgaWriter();

//===========================================================================

}  // namespace

#endif
