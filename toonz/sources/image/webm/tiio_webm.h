#pragma once

#ifndef TTIO_WEBM_INCLUDED
#define TTIO_WEBM_INCLUDED

#include "tiio.h"
//#include "timage_io.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

class WebmWriterProperties : public TPropertyGroup {
public:
  // TEnumProperty m_pixelSize;
  //TBoolProperty m_matte;

  WebmWriterProperties();
};

//===========================================================================

Tiio::Reader *makeWebmReader();
Tiio::Writer *makeWebmWriter();

}  // namespace

#endif
