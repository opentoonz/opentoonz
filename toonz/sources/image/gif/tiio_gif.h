#pragma once

#ifndef TTIO_GIF_INCLUDED
#define TTIO_GIF_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

class GifWriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  //TBoolProperty m_matte;

  GifWriterProperties();
};

//===========================================================================

Tiio::Reader *makeGifReader();
Tiio::Writer *makeGifWriter();

}  // namespace

#endif
