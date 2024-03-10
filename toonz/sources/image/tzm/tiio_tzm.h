#pragma once

#ifndef TTIO_TZM_INCLUDED
#define TTIO_TZM_INCLUDED

#include <tlevel_io.h>


namespace tzm {
  TLevelWriter* createWriter(const TFilePath &path, TPropertyGroup *winfo);
  TLevelReader* createReader(const TFilePath &path);
}

#endif  // TTIO_TZM_INCLUDED
