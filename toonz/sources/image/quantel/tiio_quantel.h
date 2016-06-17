#pragma once

#ifndef TIIO_QUANTEL_INCLUDED
#define TIIO_QUANTEL_INCLUDED

//#include "timage_io.h"
#include "tiio.h"

//===========================================================================

namespace Tiio {

Tiio::Reader *makeQntReader();
Tiio::Writer *makeQntWriter();

}  // namespace

#endif
