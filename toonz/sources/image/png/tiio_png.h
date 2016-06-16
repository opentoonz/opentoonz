#pragma once

#ifndef TTIO_PNG_INCLUDED
#define TTIO_PNG_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

class PngWriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  TBoolProperty m_matte;

  PngWriterProperties();
};

//===========================================================================

Tiio::Reader *makePngReader();
Tiio::Writer *makePngWriter();

}  // namespace

#endif
