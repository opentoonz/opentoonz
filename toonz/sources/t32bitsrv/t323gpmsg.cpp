

#if (!(defined(x64) || defined(__LP64__) || defined(LINUX) || defined(FREEBSD)))

// Toonz stuff
#include "tiio.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include "traster.h"
#include "tstream.h"
#include "movsettings.h"
#include "tproperty.h"
#include "tsound.h"

#if defined(_WIN32)
#include "../image/3gp/tiio_3gp.h"
#elif defined(MACOSX)
#include "../image/3gp/tiio_3gpM.h"
#endif

// Qt stuff
#include <QString>
#include <QHash>
#include <QSharedMemory>
#include <QDebug>

#include <QLocalSocket>
#include <QDataStream>

// tipc includes
#include "tipc.h"
#include "tipcmsg.h"
#include "tipcsrv.h"
#include "t32bitsrv_wrap.h"

#include "t323gpmsg.h"

//---------------------------------------------------

//  Local namespace stuff

namespace {
QHash<unsigned int, TLevelReaderP> readers;
QHash<unsigned int, TLevelWriterP> writers;
}

//---------------------------------------------------

using namespace tipc;

namespace _3gp_io {

void addParsers(tipc::Server *srv) {
  srv->addParser(new InitLW3gpParser);
  srv->addParser(new LWSetFrameRateParser);
  srv->addParser(new LWImageWriteParser);
  srv->addParser(new LWSaveSoundTrackParser);
  srv->addParser(new CloseLW3gpParser);
  srv->addParser(new InitLR3gpParser);
  srv->addParser(new LRLoadInfoParser);
  srv->addParser(new LREnableRandomAccessReadParser);
  srv->addParser(new LRImageReadParser);
  srv->addParser(new CloseLR3gpParser);
}

//************************************************************************
//    InitLWMov Parser
//************************************************************************

void InitLW3gpParser::operator()(Message &msg) {
  unsigned int id;
  QString fp, propsFp;
  msg >> id >> fp >> propsFp >> clr;

  TFilePath tfp(fp.toStdWString()), propsTFp(propsFp.toStdWString());

  try {
    TPropertyGroup *props = 0;
    if (!propsTFp.isEmpty()) {
      props = new TPropertyGroup;

      TIStream is(propsTFp);
      props->loadData(is);
    }

    writers.insert(id, TLevelWriterP(tfp, props));

    msg << QString("ok");
  } catch (...) {
    msg << QString("err");
  }
}

//************************************************************************
//    LWsetFrameRate Parser
//************************************************************************

void LWSetFrameRateParser::operator()(Message &msg) {
  unsigned int id;
  double fps;

  msg >> id >> fps >> clr;

  writers.find(id).value()->setFrameRate(fps);

  msg << QString("ok");
}

//************************************************************************
//    LWImageWrite Parser
//************************************************************************

void LWImageWriteParser::operator()(Message &msg) {
  unsigned int id;
  int frameIdx, lx, ly;

  msg >> id >> frameIdx >> lx >> ly;

  // Read the data through a shared memory segment
  TRaster32P ras(lx, ly);
  t32bitsrv::RasterExchanger<TPixel32> exch(ras);
  tipc::readShMemBuffer(*stream(), msg, &exch);

  // Save the image
  try {
    TImageWriterP iw(writers.find(id).value()->getFrameWriter(frameIdx + 1));
    iw->save(TRasterImageP(ras));

    msg << QString("ok");
  } catch (...) {
    msg << QString("err");
  }
}

//************************************************************************
//    LWSaveSoundTrack Parser
//************************************************************************

void LWSaveSoundTrackParser::operator()(Message &msg) {
  unsigned int id;
  QString shMemId;

  TUINT32 sampleRate;
  TINT32 sCount;
  int bps, chanCount;
  bool signedSample;

  msg >> id >> sampleRate >> bps >> chanCount >> sCount >> signedSample;

  // Retrieve the soundtrack buffer
  TSoundTrackP st =
      TSoundTrack::create(sampleRate, bps, chanCount, sCount, signedSample);
  t32bitsrv::BufferExchanger exch((UCHAR *)st->getRawData());
  tipc::readShMemBuffer(*stream(), msg, &exch);

  // Write the soundtrack
  try {
    writers.find(id).value()->saveSoundTrack(st.getPointer());
    msg << QString("ok");
  } catch (...) {
    msg << QString("err");
  }
}

//************************************************************************
//    CloseLW3gp Parser
//************************************************************************

void CloseLW3gpParser::operator()(Message &msg) {
  unsigned int id;
  msg >> id >> clr;

  try {
    writers.take(id);
    msg << QString("ok");
  } catch (...) {
    msg << QString("err");
  }
}

//************************************************************************
//    InitLR3gp Parser
//************************************************************************

void InitLR3gpParser::operator()(Message &msg) {
  unsigned int id;
  QString fp, propsFp;
  msg >> id >> fp >> clr;
  assert(!fp.isEmpty());

  TFilePath tfp(fp.toStdWString());

  try {
    TLevelReaderP lrm(tfp);

    // Extract some info to be returned
    const TImageInfo *info = lrm->getImageInfo();
    if (!info) throw TImageException(tfp, "Couldn't retrieve image properties");

    int lx = info->m_lx, ly = info->m_ly;
    double frameRate = info->m_frameRate;

    readers.insert(id, lrm);

    msg << QString("ok") << lx << ly << frameRate;
  } catch (...) {
    msg << QString("err");
  }
}

//************************************************************************
//    LRLoadInfo Parser
//************************************************************************

void LRLoadInfoParser::operator()(Message &msg) {
  // Read command data
  unsigned int id;
  QString shMemId;
  msg >> id >> shMemId >> clr;

  QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
  if (it == readers.end()) goto err;

  // Read level infos
  {
    TLevelP level;
    try {
      level = it.value()->loadInfo();
    } catch (...) {
      goto err;
    }

    int frameCount = level->getFrameCount();
    if (!shMemId.isEmpty()) {
      // Create a shared memory segment to transfer the infos to
      tipc::DefaultMessageParser<SHMEM_REQUEST> msgParser;
      Message shMsg;

      shMsg << shMemId << frameCount * (int)sizeof(int) << reset;
      msgParser(shMsg);

      QString str;
      shMsg >> reset >> str;
      if (str != QString("ok")) goto err;

      // Copy level data to the shared memory segment
      {
        QSharedMemory shmem(shMemId);
        shmem.attach();
        shmem.lock();

        TLevel::Table *table = level->getTable();

        TLevel::Table::const_iterator jt;
        int *f = (int *)shmem.data();
        for (jt = table->begin(); jt != table->end(); ++jt, ++f)
          *f = jt->first.getNumber();

        shmem.unlock();
        shmem.detach();
      }
    }

    msg << QString("ok") << frameCount;
  }

  return;

err:

  msg << QString("err");
}

//************************************************************************
//    LREnableRandomAccessRead Parser
//************************************************************************

void LREnableRandomAccessReadParser::operator()(Message &msg) {
  unsigned int id;
  QString str;
  msg >> id >> str >> clr;
  bool enable = (str == "true");

  QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
  if (it == readers.end()) {
    msg << QString("err");
    return;
  }

  it.value()->enableRandomAccessRead(enable);

  msg << QString("ok");
}

//************************************************************************
//    LRImageRead Parser
//************************************************************************

void LRImageReadParser::operator()(Message &msg) {
  {
    unsigned int id;
    int lx, ly, pixSize, frameIdx, x, y, shrinkX, shrinkY;

    msg >> id >> lx >> ly >> pixSize >> frameIdx >> x >> y >> shrinkX >>
        shrinkY >> clr;

    if (pixSize != 4) goto err;

    QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
    if (it == readers.end()) goto err;

    // Load the raster
    TRaster32P ras(lx, ly);
    try {
      TImageReaderP ir(it.value()->getFrameReader(frameIdx + 1));
      ir->load(ras, TPoint(x, y), shrinkX, shrinkY);
    } catch (...) {
      goto err;
    }

    t32bitsrv::RasterExchanger<TPixel32> exch(ras);
    if (!tipc::writeShMemBuffer(*stream(), msg << clr,
                                lx * ly * sizeof(TPixel32), &exch))
      goto err;
  }

  return;

err:

  msg << QString("err");
}

//************************************************************************
//    CloseLRMov Parser
//************************************************************************

void CloseLR3gpParser::operator()(Message &msg) {
  unsigned int id;
  msg >> id >> clr;

  readers.take(id);
  msg << QString("ok");
}

}  // namespace mov_io

#endif  // !x64 && !__LP64__
