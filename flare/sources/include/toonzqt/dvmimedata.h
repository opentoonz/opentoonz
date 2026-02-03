#pragma once

#ifndef DVMIMEDATA_INCLUDED
#define DVMIMEDATA_INCLUDED

#include <QMimeData>
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// DvMimedata
//-----------------------------------------------------------------------------

class DVAPI DvMimeData : public QMimeData {
public:
  DvMimeData();
  virtual ~DvMimeData();

  virtual DvMimeData *clone() const = 0;
  virtual void releaseData() {}
};

DVAPI QMimeData *cloneData(const QMimeData *data);

#endif  // DVMIMEDATA_INCLUDED
