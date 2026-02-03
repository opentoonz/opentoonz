#pragma once

#ifndef TTIO_PLT_INCLUDED
#define TTIO_PLT_INCLUDED

#include "tiio.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

Tiio::Reader *makePltReader();
Tiio::Writer *makePltWriter();

}  // namespace

#endif
