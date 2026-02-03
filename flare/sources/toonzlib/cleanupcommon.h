#pragma once

#ifndef CLEANUPCOMMON_INCLUDED
#define CLEANUPCOMMON_INCLUDED

#include "texception.h"

// unit conversion
inline double inToPixel(double val, double dpi) { return val * dpi; }
inline double mmToPixel(double val, double dpi) {
  return (val * dpi) * (1.0 / 25.4);
}

class TCleanupException final : public TException {
public:
  TCleanupException(const char *msg) : TException(msg) {}
  ~TCleanupException() {}
};

#ifndef MAX_DOT
#define MAX_DOT 500
#endif

#endif
