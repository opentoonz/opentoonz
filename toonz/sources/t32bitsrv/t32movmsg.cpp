

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
#include "../image/mov/tiio_mov.h"
#elif defined(MACOSX)
#include "../image/mov/tiio_movM.h"
#include <ApplicationServices/ApplicationServices.h>
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

#include "t32movmsg.h"

//---------------------------------------------------

//  Diagnostics stuff

//#define TIPC_DEBUG

#ifdef TIPC_DEBUG
#define tipc_debug(expr) expr
#else
#define tipc_debug(expr)
#endif

#ifdef TIPC_DEBUG
#include <QTime>
#endif

//---------------------------------------------------

//  Local namespace stuff

namespace {
QHash<unsigned int, TLevelReaderP> readers;
QHash<unsigned int, TLevelWriterP> writers;
}

//---------------------------------------------------

using namespace tipc;

namespace mov_io {

void addParsers(tipc::Server *srv) {
  srv->addParser(new IsQTInstalledParser);
  srv->addParser(new DefaultMovPropsParser);
  srv->addParser(new OpenMovSettingsPopupParser);
  srv->addParser(new InitLWMovParser);
  srv->addParser(new LWSetFrameRateParser);
  srv->addParser(new LWImageWriteParser);
  srv->addParser(new LWSaveSoundTrackParser);
  srv->addParser(new CloseLWMovParser);
  srv->addParser(new InitLRMovParser);
  srv->addParser(new LRLoadInfoParser);
  srv->addParser(new LREnableRandomAccessReadParser);
  srv->addParser(new LRImageReadParser);
  srv->addParser(new LRImageReadSHMParser);
  srv->addParser(new CloseLRMovParser);

#ifdef WIN32
  srv->addParser(new LRSetYMirrorParser);
  srv->addParser(new LRSetLoadTimecodeParser);
  srv->addParser(new LRTimecodeParser);
#endif
}

//************************************************************************
//    IsQTInstalled Parser
//************************************************************************

void IsQTInstalledParser::operator()(Message &msg) {
  bool ret = Tiio::isQuicktimeInstalled();
  msg << clr << QString((ret) ? "yes" : "no");
}

//************************************************************************
//    DefaultMovProps Parser
//************************************************************************

void DefaultMovPropsParser::operator()(Message &msg) {
  // Ensure the file path was passed - and retrieve it
  QString reply;
  msg >> reply >> clr;
  if (reply.isEmpty()) goto err;

#ifdef WIN32
  // Ensure that QuickTime is correctly running
  if (InitializeQTML(0) != noErr) goto err;
#endif

  // Success - retrieve the props
  {
    TPropertyGroup movProps;
    {
      // Low-level QuickTime stuff
      ComponentInstance ci = OpenDefaultComponent(StandardCompressionType,
                                                  StandardCompressionSubType);
      QTAtomContainer settings;

      if (SCGetSettingsAsAtomContainer(ci, &settings) != noErr) assert(false);

      fromAtomsToProperties(settings, movProps);
    }

    // Write the retrieved properties
    TFilePath tfp(reply.toStdWString());
    TOStream os(tfp);

    movProps.saveData(os);

    msg << QString("ok");
  }

  return;

err:

  msg << QString("err");
}

//************************************************************************
//    OpenMovSettingsPopup Parser
//************************************************************************

void OpenMovSettingsPopupParser::operator()(Message &msg) {
  // Open the properties file
  QString fp;
  msg >> fp >> clr;

  // Retrieve the properties
  TPropertyGroup *props = new TPropertyGroup;
  TFilePath tfp(fp.toStdWString());
  {
    TIStream is(tfp);
    props->loadData(is);
  }

#ifdef MACOSX

  ProcessSerialNumber psn = {0, kCurrentProcess};
  TransformProcessType(&psn, kProcessTransformToForegroundApplication);
  SetFrontProcess(&psn);

#endif

  openMovSettingsPopup(props, true);

  {
    TOStream os(tfp);  // Should NOT append
    props->saveData(os);
  }

  delete props;

  msg << QString("ok");
}

//************************************************************************
//    InitLWMov Parser
//************************************************************************

void InitLWMovParser::operator()(Message &msg) {
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
//    CloseLWMov Parser
//************************************************************************

void CloseLWMovParser::operator()(Message &msg) {
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
//    InitLRMov Parser
//************************************************************************

void InitLRMovParser::operator()(Message &msg) {
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

    tipc_debug(qDebug() << "Inserted image"
                        << QString::fromStdWString(tfp.getWideString()));
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
//    LRSetYMirror Parser
//************************************************************************

#ifdef WIN32

void LRSetYMirrorParser::operator()(Message &msg) {
  unsigned int id;
  QString str;
  msg >> id >> str >> clr;
  bool enable = (str == "true");

  QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
  if (it == readers.end()) {
    msg << QString("err");
    return;
  }

  static_cast<TLevelReaderMov *>(it.value().getPointer())->setYMirror(enable);

  msg << QString("ok");
}

//************************************************************************
//    LRSetLoadTimecode Parser
//************************************************************************

void LRSetLoadTimecodeParser::operator()(Message &msg) {
  unsigned int id;
  QString str;
  msg >> id >> str >> clr;
  bool enable = (str == "true");

  QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
  if (it == readers.end()) {
    msg << QString("err");
    return;
  }

  static_cast<TLevelReaderMov *>(it.value().getPointer())
      ->setLoadTimecode(enable);

  msg << QString("ok");
}

//************************************************************************
//    LRTimecode Parser
//************************************************************************

void LRTimecodeParser::operator()(Message &msg) {
  unsigned int id;
  int frameIdx;
  QString str;
  msg >> id >> frameIdx >> clr;

  QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
  if (it == readers.end()) {
    msg << QString("err");
    return;
  }

  UCHAR hh, mm, ss, ff;
  static_cast<TLevelReaderMov *>(it.value().getPointer())
      ->timecode(frameIdx, hh, mm, ss, ff);

  msg << QString("ok") << hh << mm << ss << ff;
}

#endif

//************************************************************************
//    LRImageRead Parser
//************************************************************************

void LRImageReadParser::operator()(Message &msg) {
  tipc_debug(QTime fTime; QTime irTime; QTime shTime; fTime.start(););

  {
    unsigned int id;
    int lx, ly, pixSize, frameIdx, x, y, shrinkX, shrinkY;

    msg >> id >> lx >> ly >> pixSize >> frameIdx >> x >> y >> shrinkX >>
        shrinkY >> clr;

    if (pixSize != 4) goto err;

    QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
    if (it == readers.end()) goto err;

    tipc_debug(irTime.start());

    // Load the raster
    TRaster32P ras(lx, ly);
    try {
      TImageReaderP ir(it.value()->getFrameReader(frameIdx + 1));
      ir->load(ras, TPoint(x, y), shrinkX, shrinkY);
    } catch (...) {
      goto err;
    }

    tipc_debug(qDebug() << "load time:" << irTime.elapsed());
    tipc_debug(shTime.start());

    t32bitsrv::RasterExchanger<TPixel32> exch(ras);
    if (!tipc::writeShMemBuffer(*stream(), msg << clr,
                                lx * ly * sizeof(TPixel32), &exch))
      goto err;

    tipc_debug(qDebug() << "exchange time:" << shTime.elapsed());
    tipc_debug(qDebug() << "TLevelReaderMov::loadImage time:"
                        << fTime.elapsed());
  }

  return;

err:

  msg << QString("err");
}

//************************************************************************
//    LRImageReadSHM Parser
//************************************************************************

void LRImageReadSHMParser::operator()(Message &msg) {
  tipc_debug(QTime fTime; QTime irTime; fTime.start(););

  unsigned int id;
  int lx, ly, frameIdx;
  QString shMemId;

  msg >> id >> lx >> ly >> frameIdx >> shMemId >> clr;

  tipc_debug(qDebug() << "LoadImageSHM data:" << id << lx << ly << frameIdx
                      << shMemId);

  QHash<unsigned int, TLevelReaderP>::iterator it = readers.find(id);
  if (it == readers.end()) goto err;

  // Attach the shared memory segment the raster
  {
    QSharedMemory shm(shMemId);
    shm.attach();
    if (!shm.isAttached()) goto err;

    // Load the raster
    TRaster32P ras(lx, ly, lx, (TPixel32 *)shm.data());
    try {
      tipc_debug(qDebug() << "loading image...");
      tipc_debug(irTime.start());

      shm.lock();
      TImageReaderP ir(it.value()->getFrameReader(frameIdx + 1));
      ir->load(ras, TPoint(), 1, 1);
      shm.unlock();

      tipc_debug(qDebug() << "load time:" << irTime.elapsed());
    } catch (TImageException e) {
      shm.unlock();
      tipc_debug(qDebug() << "Image Read Error:"
                          << QString::fromStdWString(e.getMessage()));
      goto err;
    } catch (...) {
      shm.unlock();
      tipc_debug(qDebug() << "Unknown Image Read Error");
      goto err;
    }
  }

  msg << QString("ok");

#ifdef WIN32

  UCHAR hh, mm, ss, ff;
  TLevelReaderMov *lrm =
      static_cast<TLevelReaderMov *>(it.value().getPointer());
  lrm->loadedTimecode(hh, mm, ss, ff);

  tipc_debug(qDebug() << "TLevelReaderMov::loadImage time:" << fTime.elapsed());
  msg << hh << mm << ss << ff;

#endif

  return;

err:

  msg << QString("err");
}

//************************************************************************
//    CloseLRMov Parser
//************************************************************************

void CloseLRMovParser::operator()(Message &msg) {
  unsigned int id;
  msg >> id >> clr;

  readers.take(id);
  msg << QString("ok");
}

}  // namespace mov_io

#endif  // !x64 && !__LP64__
