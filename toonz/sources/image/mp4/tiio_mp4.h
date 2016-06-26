#pragma once

#ifndef TTIO_MP4_INCLUDED
#define TTIO_MP4_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

class Mp4WriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  //TBoolProperty m_matte;

  Mp4WriterProperties();
};

//===========================================================================

Tiio::Reader *makeMp4Reader();
Tiio::Writer *makeMp4Writer();

}  // namespace

#endif
