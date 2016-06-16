#pragma once

#ifndef TTIO_TZP_INCLUDED
#define TTIO_TZP_INCLUDED

#include "tiio.h"
#include "tproperty.h"

//===========================================================================

namespace Tiio {

//===========================================================================

Tiio::Reader *makeTzpReader();
Tiio::Writer *makeTzpWriter();

}  // namespace

#endif
