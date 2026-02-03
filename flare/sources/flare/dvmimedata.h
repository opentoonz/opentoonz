#pragma once

#ifndef DVMIMEDATA_INCLUDED
#define DVMIMEDATA_INCLUDED

#include <QMimeData>

//=============================================================================
// DvMimedata
//-----------------------------------------------------------------------------

class DvMimeData : public QMimeData {
public:
  DvMimeData();
  ~DvMimeData();

  virtual DvMimeData *clone() const = 0;
};

QMimeData *cloneData(const QMimeData *data);

#endif  // DVMIMEDATA_INCLUDED
