

#include "toonzqt/dvmimedata.h"

#include <QStringList>

//=============================================================================
// DvMimedata
//-----------------------------------------------------------------------------

DvMimeData::DvMimeData() {}

//-----------------------------------------------------------------------------

DvMimeData::~DvMimeData() {}

//=============================================================================
// cloneData
//-----------------------------------------------------------------------------

QMimeData *cloneData(const QMimeData *data) {
  const DvMimeData *dvData = dynamic_cast<const DvMimeData *>(data);
  if (dvData) return dvData->clone();

  QMimeData *newData = new QMimeData();

  QStringList list = data->formats();
  if (list.isEmpty()) return newData;
  QString format = list.first();
  if (format.isEmpty()) return newData;
  QByteArray byteArray = data->data(format);
  if (byteArray.isEmpty()) return newData;

  newData->setData(format, byteArray);

  return newData;
}
