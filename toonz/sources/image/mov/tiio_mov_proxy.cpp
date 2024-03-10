

#if defined(x64) || defined(__LP64__) || defined(LINUX) || defined(FREEBSD) || \
    defined(HAIKU) || (defined(_WIN32) && defined(__GNUC__))

// Toonz includes
#include "tfilepath.h"
#include "trasterimage.h"
#include "tstream.h"
#include "timageinfo.h"
#include "trop.h"
#include "tsound.h"
#include "tmsgcore.h"

// tipc includes
#include "tipc.h"
#include "t32bitsrv_wrap.h"

// Qt includes
#include <QSharedMemory>
#include <QMutexLocker>
#include <QDataStream>

#include "tiio_mov_proxy.h"

/*
  For a list of supported commands through the 32-bit background server,
  see the related "t32libserver" project.
*/

//******************************************************************************
//    Generic stuff implementation
//******************************************************************************

bool IsQuickTimeInstalled() {
#if !defined(__OSX__)
  // NOTE: This is *NOT* the same function as Tiio::isQuickTimeInstalled !!
  // There actually are 2 distinct functions with essentially the same name, one
  // in tnzcore lib, the other in image. The core version is currently NEVER
  // USED
  // throughout Toonz, even if it's EXPORT-defined.
  QLocalSocket socket;
  if (!tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), 3000,
                                  t32bitsrv::srvCmdlinePrg(),
                                  t32bitsrv::srvCmdlineArgs()))
    return false;

  tipc::Stream stream(&socket);
  tipc::Message msg;

  stream << (msg << QString("$isQTInstalled"));

  if (tipc::readMessage(stream, msg) != "yes") return false;
  return true;
#else
  return false;
#endif
};

//---------------------------------------------------------------------

Tiio::MovWriterProperties::MovWriterProperties() {
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  // Retrieve a temporary file to pass the data
  QString fp;
  {
    stream << (msg << QString("$tmpfile_request") << QString("MovWriterProps"));

    if (tipc::readMessage(stream, msg) != "ok") goto err;

    msg >> fp;
    assert(!fp.isEmpty());
  }

  // Make the server write the data to the file
  {
    stream << (msg << tipc::clr << QString("$defaultMovProps") << fp);
    if (tipc::readMessage(stream, msg) != "ok") goto err;

    // Load the data
    TFilePath tfp(fp.toStdWString());
    TIStream is(tfp);
    loadData(is);
  }

  // Release the temporary file
  {
    stream << (msg << tipc::clr << QString("$tmpfile_release")
                   << QString("MovWriterProps"));
    if (tipc::readMessage(stream, msg) != "ok") goto err;
  }

  return;

err:

  throw TException("Server error");
}

//******************************************************************************
//    TImageWriterMov Proxy implementation
//******************************************************************************

class TImageWriterMovProxy final : public TImageWriter {
  TLevelWriterMov *m_lw;

public:
  int m_frameIndex;

public:
  TImageWriterMovProxy(const TFilePath &fp, int frameIndex,
                       TLevelWriterMov *lw);
  ~TImageWriterMovProxy();

  bool is64bitOutputSupported() { return false; }
  void save(const TImageP &);

private:
  // not implemented
  TImageWriterMovProxy(const TImageWriterMovProxy &);
  TImageWriterMovProxy &operator=(const TImageWriterMovProxy &src);
};

//------------------------------------------------------------------

TImageWriterMovProxy::TImageWriterMovProxy(const TFilePath &fp, int frameIndex,
                                           TLevelWriterMov *lw)
    : TImageWriter(fp), m_lw(lw), m_frameIndex(frameIndex) {
  m_lw->addRef();
}

//------------------------------------------------------------------

TImageWriterMovProxy::~TImageWriterMovProxy() { m_lw->release(); }

//------------------------------------------------------------------

void TImageWriterMovProxy::save(const TImageP &img) {
  m_lw->save(img, m_frameIndex);
}

//******************************************************************************
//    TLevelWriterMov Proxy implementation
//******************************************************************************

TLevelWriterMov::TLevelWriterMov(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  static TAtomicVar count;
  unsigned int currCount = ++count;
  m_id                   = currCount;

  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  QString res, propsFp;
  if (winfo) {
    // Request a temporary file to store the infos to
    stream << (msg << QString("$tmpfile_request")
                   << QString("initLWMov") + QString::number(currCount));
    if (tipc::readMessage(stream, msg) != "ok") goto err;

    msg >> propsFp >> tipc::clr;
    assert(!propsFp.isEmpty());

    TFilePath propsTfp(propsFp.toStdWString());
    {
      TOStream os(propsTfp);
      winfo->saveData(os);
    }
  }

  // Pass fp to the server
  stream << (msg << QString("$initLWMov") << m_id
                 << QString::fromStdWString(path.getWideString()) << propsFp);
  if (tipc::readMessage(stream, msg) != "ok") goto err;

  if (winfo) {
    stream << (msg << tipc::clr << QString("$tmpfile_release")
                   << QString("initLWMov") + QString::number(currCount));
    if (tipc::readMessage(stream, msg) != "ok") goto err;
  }

  return;

err:

  throw TException("Unable to write file");
}

//------------------------------------------------------------------

TLevelWriterMov::~TLevelWriterMov() {
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;
  QString res;

  stream << (msg << QString("$closeLWMov") << m_id);
  if (tipc::readMessage(stream, msg) != "ok")
    DVGui::warning("Unable to write file");
}

//------------------------------------------------------------------

void TLevelWriterMov::setFrameRate(double fps) {
  TLevelWriter::setFrameRate(fps);

  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;
  QString res;

  stream << (msg << QString("$LWMovSetFrameRate") << m_id << fps);
  if (tipc::readMessage(stream, msg) != "ok")
    throw TException("Unexpected error");
}

//------------------------------------------------------------------

TImageWriterP TLevelWriterMov::getFrameWriter(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageWriterP(0);

  int index = fid.getNumber() - 1;
  return new TImageWriterMovProxy(m_path, index, this);
}

//------------------------------------------------------------------

void TLevelWriterMov::save(const TImageP &img, int frameIndex) {
  TRasterImageP ri(img);
  if (!img) throw TImageException(getFilePath(), "Unsupported image type");

  TRasterP ras(ri->getRaster());

  int lx = ras->getLx(), ly = ras->getLy(), pixSize = ras->getPixelSize();
  if (pixSize != 4)
    throw TImageException(getFilePath(), "Unsupported pixel type");

  int size = lx * ly * pixSize;

  // Send messages
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  // Send the write message.
  stream << (msg << QString("$LWMovImageWrite") << m_id << frameIndex << lx
                 << ly);

  // Send the data through a shared memory segment
  {
    t32bitsrv::RasterExchanger<TPixel32> exch(ras);
    tipc::writeShMemBuffer(stream, msg << tipc::clr, size, &exch);
  }

  if (tipc::readMessage(stream, msg) != "ok")
    throw TImageException(getFilePath(), "Couldn't save image");
}

//------------------------------------------------------------------

void TLevelWriterMov::saveSoundTrack(TSoundTrack *st) {
  if (st == 0) return;

  // Prepare connection
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  unsigned int size = st->getSampleSize() * st->getSampleCount();

  // Send the saveSoundTract command to the server
  tipc::Stream stream(&socket);
  tipc::Message msg;

  stream << (msg << QString("$LWMovSaveSoundTrack") << m_id
                 << st->getSampleRate() << st->getBitPerSample()
                 << st->getChannelCount() << st->getSampleCount()
                 << (st->getFormat().m_sampleType != TSound::UINT));

  t32bitsrv::BufferExchanger exch((UCHAR *)st->getRawData());
  tipc::writeShMemBuffer(stream, msg << tipc::clr, size, &exch);

  QString res(tipc::readMessage(stream, msg));
  assert(res == "ok");
}

//******************************************************************************
//    TImageReaderMov Proxy implementation
//******************************************************************************

class TImageReaderMovProxy final : public TImageReader {
  TLevelReaderMov *m_lr;
  TImageInfo *m_info;

public:
  int m_frameIndex;

public:
  TImageReaderMovProxy(const TFilePath &fp, int frameIndex, TLevelReaderMov *lr,
                       TImageInfo *info);
  ~TImageReaderMovProxy() { m_lr->release(); }

  TImageP load();
  void load(const TRasterP &rasP, const TPoint &pos = TPoint(0, 0),
            int shrinkX = 1, int shrinkY = 1);

  TDimension getSize() const { return m_lr->getSize(); }
  TRect getBBox() const { return m_lr->getBBox(); }

  const TImageInfo *getImageInfo() const { return m_info; }

private:
  // not implemented
  TImageReaderMovProxy(const TImageReaderMovProxy &);
  TImageReaderMovProxy &operator=(const TImageReaderMovProxy &src);
};

//------------------------------------------------------------------

TImageReaderMovProxy::TImageReaderMovProxy(const TFilePath &fp, int frameIndex,
                                           TLevelReaderMov *lr,
                                           TImageInfo *info)
    : TImageReader(fp), m_lr(lr), m_frameIndex(frameIndex), m_info(info) {
  m_lr->addRef();
}

//------------------------------------------------------------------

TImageP TImageReaderMovProxy::load() {
  TRaster32P ras(m_lr->getSize());
  m_lr->load(ras, m_frameIndex, TPointI(), 1, 1);
  return TRasterImageP(ras);
}

//------------------------------------------------------------------

void TImageReaderMovProxy::load(const TRasterP &rasP, const TPoint &pos,
                                int shrinkX, int shrinkY) {
  // NOTE: The original implementation is different. But is also does not make
  // sense...
  // I've substituted it with the lrm plain call.
  m_lr->load(rasP, m_frameIndex, pos, shrinkX, shrinkY);
}

//******************************************************************************
//    TLevelReaderMov Proxy implementation
//******************************************************************************

TLevelReaderMov::TLevelReaderMov(const TFilePath &path) : TLevelReader(path) {
  static TAtomicVar count;
  unsigned int currCount = ++count;
  m_id                   = currCount;

  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  stream << (msg << QString("$initLRMov") << m_id
                 << QString::fromStdWString(path.getWideString()));
  if (tipc::readMessage(stream, msg) != "ok")
    throw TImageException(path, "Couldn't open file");

  double frameRate;
  msg >> m_lx >> m_ly >> frameRate >> tipc::clr;

  m_info              = new TImageInfo;
  m_info->m_lx        = m_lx;
  m_info->m_ly        = m_ly;
  m_info->m_frameRate = frameRate;
}

//------------------------------------------------------------------

TLevelReaderMov::~TLevelReaderMov() {
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  stream << (msg << QString("$closeLRMov") << m_id);
  QString res(tipc::readMessage(stream, msg));
  assert(res == "ok");
}

//------------------------------------------------------------------

TImageReaderP TLevelReaderMov::getFrameReader(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);

  int index = fid.getNumber() - 1;
  return new TImageReaderMovProxy(m_path, index, this, m_info);
}

//------------------------------------------------------------------

TLevelP TLevelReaderMov::loadInfo() {
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  TLevelP level;
  {
    QString shMemId(tipc::uniqueId());

    // Send the appropriate command
    stream << (msg << QString("$LRMovLoadInfo") << m_id << shMemId);
    if (tipc::readMessage(stream, msg) != "ok") goto err;

    int frameCount;

    msg >> frameCount >> tipc::clr;

    // Read the data in the shared memory segment
    QSharedMemory shmem(shMemId);
    shmem.attach();
    shmem.lock();

    int *f, *fBegin = (int *)shmem.data(), *fEnd = fBegin + frameCount;
    assert(fBegin);

    for (f = fBegin; f < fEnd; ++f) level->setFrame(*f, TImageP());

    shmem.unlock();
    shmem.detach();

    // Release the shared memory segment
    stream << (msg << QString("$shmem_release") << shMemId);
    if (tipc::readMessage(stream, msg) != "ok") goto err;
  }

  return level;

err:

  throw TException("Couldn't read movie data");
  return TLevelP();
}

//------------------------------------------------------------------

void TLevelReaderMov::enableRandomAccessRead(bool enable) {
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  stream << (msg << QString("$LRMovEnableRandomAccessRead") << m_id
                 << QString(enable ? "true" : "false"));
  QString res(tipc::readMessage(stream, msg));
  assert(res == "ok");
}

//------------------------------------------------------------------

void TLevelReaderMov::load(const TRasterP &ras, int frameIndex,
                           const TPoint &pos, int shrinkX, int shrinkY) {
  QLocalSocket socket;
  tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1,
                             t32bitsrv::srvCmdlinePrg(),
                             t32bitsrv::srvCmdlineArgs());

  tipc::Stream stream(&socket);
  tipc::Message msg;

  unsigned int size = ras->getLx() * ras->getLy() * ras->getPixelSize();

  // Send the appropriate command to the 32-bit server
  stream << (msg << QString("$LRMovImageRead") << m_id << ras->getLx()
                 << ras->getLy() << ras->getPixelSize() << frameIndex << pos.x
                 << pos.y << shrinkX << shrinkY);

  t32bitsrv::RasterExchanger<TPixel32> exch(ras);
  if (!tipc::readShMemBuffer(stream, msg << tipc::clr, &exch))
    throw TException("Couldn't load image");
}

#endif  // x64
