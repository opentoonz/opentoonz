

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "tiio_jpg_util.h"
//#include "tiio.h"
#include "tiio_jpg.h"
#include "tproperty.h"
#include <stdio.h>
#include "tfilepath_io.h"
#include "tsystem.h"

void Tiio::createJpg(std::vector<UCHAR> &buffer, const TRaster32P &ras,
                     int quality) {
  // FILE *chan = tmpfile();
  TFilePath fname = TSystem::getUniqueFile();
  FILE *chan      = fopen(fname, "w+b");
  if (chan == 0) throw TException(L". Can not create " + fname.getWideString());

  assert(chan);
  assert(ras);
  assert(0 <= quality && quality <= 100);
  fflush(chan);
  Tiio::Writer *writer = Tiio::makeJpgWriter();
  assert(writer);
  TPropertyGroup *pg = writer->getProperties();
  if (!pg) {
    writer->setProperties(new JpgWriterProperties());
    pg = writer->getProperties();
  }

  TProperty *qualityProp = pg->getProperty(JpgWriterProperties::QUALITY);
  assert(qualityProp);
  TIntProperty *ip = dynamic_cast<TIntProperty *>(qualityProp);
  assert(ip);
  ip->setValue(quality);
  int lx = ras->getLx();
  int ly = ras->getLy();
  assert(lx > 0 && ly > 0);
  writer->open(chan, TImageInfo(lx, ly));

  ras->lock();
  for (int y = 0; y < ly; y++)
    writer->writeLine((char *)ras->getRawData(0, ly - 1 - y));
  ras->unlock();
  writer->flush();
  delete writer;
  fclose(chan);

  // lo chiudo e lo riapro: altrimenti, gettava male  la filesize. boh.
  FILE *chan1 = fopen(fname, "rb");
  if (chan1 == 0)
    throw TException(L". Can not create " + fname.getWideString());
  int ret = fseek(chan1, 0, SEEK_END);
  assert(ret == 0);
  int fileSize = ftell(chan1);

  buffer.resize(fileSize);
  ret = fseek(chan1, 0, SEEK_SET);
  assert(ret == 0);
  for (int i = 0; i < fileSize; i++) {
    int c = fgetc(chan1);
    assert(!feof(chan1));
    buffer[i] = c;
  }

  fclose(chan1);
  TSystem::deleteFile(fname);
}
