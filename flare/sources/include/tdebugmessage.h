#pragma once

#ifndef TDEBUGMESSAGE_INCLUDED
#define TDEBUGMESSAGE_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

namespace TDebugMessage {

class Manager {
public:
  virtual std::ostream &getStream() = 0;
  virtual void flush(int code = 0) = 0;
  virtual ~Manager() {}
};

DVAPI void setManager(Manager *manager);
DVAPI std::ostream &getStream();
DVAPI void flush(int code = 0);

}  // namespace

#endif
