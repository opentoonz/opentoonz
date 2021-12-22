#include <sstream>

#include "tsystem.h"
#include "tcachedlevel.h"

#include "tcodec.h"

#include "texception.h"
//#include "tcachedlevel.h"
//#include "tcodec.h"

#include "tconvert.h"

#if defined(LINUX) || defined(FREEBSD)
#include "texception.h"
//#include "tsystem.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef MACOSX
#include "sys/mman.h"
#include "sys/errno.h"
#endif
//------------------------------------------------------------------------------

#define DWORDLONG_LO_DWORD(dwl64) ((DWORD)(dwl64))
#define DWORDLONG_HI_DWORD(dwl64) ((DWORD)(dwl64 >> 32))

//==============================================================================
//==============================================================================
//==============================================================================
//              TDiskCachePersist
//------------------------------------------------------------------------------

class ImpPD {
public:
  ImpPD(const TFilePath &fn)
      : m_fname(fn)
      , m_chunkSize(0)
      , m_currentFileSize(0)
      , m_fileMapAddress(0)
      , m_mapOffset(0) {
    TFileStatus fileStatus(fn);
    if (fileStatus.doesExist())
      m_currentFileSize = fileStatus.getSize();
    else
      m_currentFileSize = ImpPD::m_defaultFileSize;
  };
  virtual ~ImpPD() {}
  virtual void openFile(const TFilePath &, TINT64 fileSize) = 0;
  virtual void setCurrentView(int pos, int &newLowPos, int &newHiPos) = 0;
  TFilePath m_fname;

  TINT64 m_chunkSize;
  TINT64 m_currentFileSize;
  void *m_fileMapAddress;
  TINT64 m_mapOffset;

  // quantita' espresse in byte
  static TINT64 m_defaultFileSize;
  TUINT32 m_viewSize;
  TINT64 m_reallocSize;
};

TINT64 ImpPD::m_defaultFileSize(100 * 1024 * 1024);

//------------------------------------------------------------------------------

class TDiskCachePersist::Imp {
public:
  Imp(const TFilePath &fp);
  ~Imp();

  bool put(int frame, UCHAR *data, TUINT32 dataSize);
  UCHAR *get(int pos, TUINT32 *size);
  void openFile(const TFilePath &fp, TINT64 fileSize);
  void setCurrentView(int frame) {
    if (!m_force && ((m_lowFrame <= frame) && (frame < m_hiFrame)))
      return;  // la vista corrente gia' copre il frame
    m_force = false;
    m_impPD->setCurrentView(frame, m_lowFrame, m_hiFrame);
  }

  ImpPD *m_impPD;

  int m_lowFrame, m_hiFrame;
  TThread::Mutex m_mutex;

  bool m_force;
};

#ifdef WIN32
class ImpPDW : public ImpPD {
private:
  string getLastErrorMessage() {
    LPVOID lpMsgBuf;

    DWORD err = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
        (LPTSTR)&lpMsgBuf, 0, NULL);

    string msg((LPCTSTR)lpMsgBuf);

    // Free the buffer.
    LocalFree(lpMsgBuf);
    return msg;
  }

public:
  ImpPDW(const TFilePath &fp);
  ~ImpPDW();
  void openFile(const TFilePath &fname, TINT64 fileSize);
  void setCurrentView(int pos, int &newLowPos, int &newHiPos);

private:
  HANDLE m_hFile;
  HANDLE m_hMap;

  SYSTEM_INFO m_systemInfo;
};

#else

class ImpPDX : public ImpPD {
private:
  class TCachedLevelException : public TException {
    static string msgFromErrorCode(int errorCode) {
      switch (errorCode) {
      case EBADF:
        return " fd is not a valid file descriptor  (and  MAP_ANONY­MOUS was "
               "not set).";
        break;
      /*
case EACCES_MAP_PRIVATE:
return "Map private was requested, but fd is not open for reading. Or MAP_SHARED
was requested and PROT_WRITE is set, but fd is not open in read/write O_RDWR)
mode.";
break;
*/
      case EINVAL:
        return "We don't like start or length  or  offset.   (E.g., they  are  "
               "too  large, or not aligned on a PAGESIZE boundary.)";
        break;

      case ETXTBSY:
        return "MAP_DENYWRITE was set but the object  specified  by fd is open "
               "for writing.";
        break;

      case EAGAIN:
        return "The  file  has  been locked, or too much memory has been "
               "locked.";
        break;

      case ENOMEM:
        return "No memory is available.";
        break;

      default:
        char *sysErr = strerror(errorCode);
        ostringstream os;
        os << errorCode << '\0';
        return string(sysErr) + "(" + os.str() + ")";
        break;
      }
      return "";
    }

  public:
    TCachedLevelException(int errorCode)
        : TException(msgFromErrorCode(errorCode)) {}
    ~TCachedLevelException() {}
  };

public:
  ImpPDX(const TFilePath &fp);
  ~ImpPDX();
  void openFile(const TFilePath &fname, TINT64 fileSize);
  void setCurrentView(int pos, int &newLowPos, int &newHiPos);

private:
  int m_fd;
  size_t m_pageSize;
};

#endif

//  HIGH
//------------------------------------------------------------------------------

TDiskCachePersist::TDiskCachePersist(TRasterCodec *codec, const TFilePath &fp)
    : TCachePersist(codec), m_imp(new Imp(fp)) {}

//------------------------------------------------------------------------------

TDiskCachePersist::~TDiskCachePersist() { delete m_imp; }

//------------------------------------------------------------------------------

void TDiskCachePersist::setFrameSize(int lx, int ly, int bpp) {
  m_imp->m_impPD->m_chunkSize = lx * ly * (bpp >> 3) + m_codec->getHeaderSize();
  m_imp->m_force              = true;
}

//------------------------------------------------------------------------------

TRasterP TDiskCachePersist::doGetRaster(int frame) {
  TRasterP rasP;
  TUINT32 size;
  UCHAR *src = m_imp->get(frame, &size);
  m_codec->decompress(src, size, rasP);
  delete[] src;
  return rasP;
}

//------------------------------------------------------------------------------

bool TDiskCachePersist::doGetRaster(int frame, TRaster32P &ras) const {
  assert(false);
  return false;
}

//------------------------------------------------------------------------------

bool TDiskCachePersist::doPutRaster(int frame, const TRasterP &ras) {
  UCHAR *outData     = 0;
  TINT32 outDataSize = 0;
  m_codec->compress(ras, 1, &outData, outDataSize);
  bool cached = m_imp->put(frame, outData, outDataSize);
  delete[] outData;
  return cached;
}

//------------------------------------------------------------------------------

UCHAR *TDiskCachePersist::getRawData(int frame, TINT32 &size, int &lx,
                                     int &ly) {
  TUINT32 inDataSize;
  UCHAR *src = m_imp->get(frame, &inDataSize);
  return m_codec->removeHeader(src, inDataSize, size, lx, ly);
}

//------------------------------------------------------------------------------
// MEDIUM
TDiskCachePersist::Imp::Imp(const TFilePath &fp) : m_impPD(0) {
#ifdef WIN32
  m_impPD = new ImpPDW(fp);
#else
  m_impPD = new ImpPDX(fp);
#endif
  m_impPD->m_currentFileSize = TFileStatus(fp).doesExist()
                                   ? TFileStatus(fp).getSize()
                                   : 0;  // per gli arrotondamenti...
}

//------------------------------------------------------------------------------

TDiskCachePersist::Imp::~Imp() { delete m_impPD; }
//------------------------------------------------------------------------------
// LOW

#ifdef WIN32
ImpPDW::ImpPDW(const TFilePath &fp) : ImpPD(fp), m_hFile(0), m_hMap(0) {
  GetSystemInfo(&m_systemInfo);

  m_viewSize    = 100 * 1024 * 1024;
  m_reallocSize = 250 * 1024 * 1024;

  TINT64 allocUnitCount = m_reallocSize / m_systemInfo.dwAllocationGranularity;

  // rendo m_reallocSize multiplo di m_systemInfo.dwAllocationGranularity
  if ((m_reallocSize % m_systemInfo.dwAllocationGranularity) != 0)
    ++allocUnitCount;

  m_reallocSize = allocUnitCount * m_systemInfo.dwAllocationGranularity;

  TINT64 fileSize = m_defaultFileSize;

  TFileStatus fileStatus(fp);
  if (fileStatus.doesExist()) fileSize = fileStatus.getSize();

  try {
    openFile(fp, fileSize);
  } catch (TException &e) {
    m_currentFileSize = 0;
    throw e;
  }

  m_currentFileSize = fileSize;
}

//------------------------------------------------------------------------------

ImpPDW::~ImpPDW() {
  if (m_fileMapAddress) UnmapViewOfFile(m_fileMapAddress);
  CloseHandle(m_hMap);
  CloseHandle(m_hFile);
}

//------------------------------------------------------------------------------

void ImpPDW::openFile(const TFilePath &fname, TINT64 fileSize) {
  DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
  DWORD dwShareMode     = 0;  // dwShareMode == 0 --> accesso esclusivo

  // lpSecurityAttributes == NULL --> l'handle non puo' essere
  // ereditato da processi figli
  LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;

  DWORD dwCreationDisposition = OPEN_ALWAYS;  // CREATE_ALWAYS;
  DWORD dwFlagsAndAttributes =
      FILE_FLAG_SEQUENTIAL_SCAN;  // FILE_ATTRIBUTE_NORMAL;//

  HANDLE hTemplateFile = NULL;

  m_hFile = CreateFileW(fname.getWideString().c_str(),  // file name
                        dwDesiredAccess,                // access mode
                        dwShareMode,                    // share mode
                        NULL,                           // SD
                        dwCreationDisposition,          // how to create
                        dwFlagsAndAttributes,           // file attributes
                        hTemplateFile  // handle to template file
                        );

  if (m_hFile == INVALID_HANDLE_VALUE) {
    string errMsg = getLastErrorMessage();
    throw TException(wstring(L"Unable to open cache file: ") +
                     fname.getWideString() + L"\n" + toWideString(errMsg));
  }

  DWORD flProtect         = PAGE_READWRITE;
  DWORD dwMaximumSizeHigh = DWORDLONG_HI_DWORD(fileSize);
  DWORD dwMaximumSizeLow  = DWORDLONG_LO_DWORD(fileSize);
  LPCTSTR lpName          = NULL;  // l'oggetto non ha nome

  m_hMap = CreateFileMapping(m_hFile,            // handle to file
                             NULL,               // security
                             flProtect,          // protection
                             dwMaximumSizeHigh,  // high-order DWORD of size
                             dwMaximumSizeLow,   // low-order DWORD of size
                             lpName              // object name
                             );

  if (m_hMap == NULL) {
    string errMsg = getLastErrorMessage();
    CloseHandle(m_hFile);
    m_hFile = 0;
    throw TException("Unable to create file mapping. " + errMsg);
  }
}

//------------------------------------------------------------------------------

void ImpPDW::setCurrentView(int frame, int &newLowFrame, int &newHiFrame) {
  if (m_fileMapAddress) UnmapViewOfFile(m_fileMapAddress);

  newLowFrame = frame;

  newHiFrame = newLowFrame + TINT32(m_viewSize / m_chunkSize);

  DWORD allocGranularity = m_systemInfo.dwAllocationGranularity;
  TINT64 viewOffset =
      (TINT64(newLowFrame * m_chunkSize) / allocGranularity) * allocGranularity;
  m_mapOffset = newLowFrame * m_chunkSize - viewOffset;

  TINT64 fileSize = newHiFrame * m_chunkSize;

  if ((fileSize > m_currentFileSize) || !m_hMap)  // devo riallocare!
  {
    CloseHandle(m_hMap);
    m_hMap = 0;
    CloseHandle(m_hFile);
    m_hFile = 0;

    TINT64 allocUnitCount = fileSize / m_reallocSize;
    // rendo fileSize multiplo di m_reallocSize
    if ((fileSize % m_reallocSize) != 0) ++allocUnitCount;

    fileSize = allocUnitCount * m_reallocSize;

    openFile(m_fname, fileSize);
    m_currentFileSize = fileSize;
  }

  DWORD dwDesiredAccess = FILE_MAP_WRITE;
  m_fileMapAddress      = MapViewOfFile(
      m_hMap,           // handle to file-mapping object
      dwDesiredAccess,  // access mode: Write permission
      DWORDLONG_HI_DWORD(
          viewOffset),  // high-order DWORD of offset: Max. object size.
      DWORDLONG_LO_DWORD(
          viewOffset),  // low-order DWORD of offset: Size of hFile.
      m_viewSize);      // number of bytes to map

  if (m_fileMapAddress == NULL) {
    string errMsg = getLastErrorMessage();
    CloseHandle(m_hMap);
    m_hMap = 0;
    CloseHandle(m_hFile);
    m_hFile = 0;

    throw TException("Unable to memory map cache file. " + errMsg);
  }
}
#else

ImpPDX::ImpPDX(const TFilePath &fp) : ImpPD(fp), m_fd(-1) {
  // std::cout << "cache file " << toString(m_fname.getFullPath()) << std::endl;
  m_pageSize = getpagesize();
  openFile(m_fname, 0);
  assert(m_fd >= 0);
}

//------------------------------------------------------------------------------

ImpPDX::~ImpPDX() {
  if (m_fileMapAddress) munmap(m_fileMapAddress, m_viewSize);
  close(m_fd);
  m_fd = 0;
}

//------------------------------------------------------------------------------

void ImpPDX::openFile(const TFilePath &fname, TINT64 fileSize) {
  assert(0);
  /*
string fn(toString(fname.getWideString()));
std::cout << "open " << fn << std::endl;
m_fd = open(fn.c_str(), O_RDWR|O_CREAT, 00666);
assert(m_fd >=0);
*/
}

void ImpPDX::setCurrentView(int pos, int &newLowPos, int &newHiPos) {
  newLowPos = pos;
  newHiPos  = newLowPos + (m_viewSize / m_chunkSize);

  assert(m_fd >= 0);
  if (m_fileMapAddress)  // previous view...
    if (munmap(m_fileMapAddress, m_viewSize) != 0)
      throw TCachedLevelException(errno);
  void *start       = 0;
  int flags         = MAP_SHARED;
  size_t viewOffset = ((newLowPos * m_chunkSize) / m_pageSize) * m_pageSize;
  m_mapOffset       = newLowPos * m_chunkSize - viewOffset;

  assert(!"controllare le dimensioni");
  unsigned long lastByte =
      (unsigned long)(((newHiPos * m_chunkSize) / (double)m_pageSize + 0.5) *
                      m_pageSize);

  if (lastByte > m_currentFileSize)  // devo riallocare!
  {
    unsigned long bu =
        (unsigned long)((lastByte / (double)m_reallocSize + 0.5) *
                        m_reallocSize);
    bu = (unsigned long)((bu / (double)m_pageSize + 0.5) * m_pageSize);
    // m_maxFileSize = tmax(m_maxFileSize + m_reallocFileSize, lastByte);
    m_currentFileSize += bu;

    std::cout << "new cache size " << m_currentFileSize << std::endl;
    if (lseek(m_fd, m_currentFileSize, SEEK_SET) == -1)
      throw TCachedLevelException(errno);
    if (write(m_fd, "", 1) == -1) throw TCachedLevelException(errno);
    if (ftruncate(m_fd, m_currentFileSize) == -1)
      throw TCachedLevelException(errno);
  }

  m_fileMapAddress =
      mmap(start, m_viewSize, PROT_READ | PROT_WRITE, flags, m_fd, viewOffset);
  if (m_fileMapAddress == (void *)-1) throw TCachedLevelException(errno);
}

#endif

//------------------------------------------------------------------------------
#ifndef WIN32
#define ULONGLONG unsigned long long
#endif
bool TDiskCachePersist::Imp::put(int frame, UCHAR *data, TUINT32 dataSize) {
  if (dataSize != m_impPD->m_chunkSize) return false;

  TThread::ScopedLock sl(m_mutex);

  setCurrentView(frame);
  ULONGLONG offset = (frame - m_lowFrame) * m_impPD->m_chunkSize;
  UCHAR *dst       = (UCHAR *)m_impPD->m_fileMapAddress + offset;
  memcpy(dst + m_impPD->m_mapOffset, data, dataSize);
  return true;
}

//------------------------------------------------------------------------------

UCHAR *TDiskCachePersist::Imp::get(int pos, TUINT32 *size) {
  UCHAR *ret = new UCHAR[TINT32(m_impPD->m_chunkSize)];

  TThread::ScopedLock sl(m_mutex);
  setCurrentView(pos);
  ULONGLONG offset = (pos - m_lowFrame) * m_impPD->m_chunkSize;
  UCHAR *src =
      (UCHAR *)m_impPD->m_fileMapAddress + offset + m_impPD->m_mapOffset;
  memcpy(ret, src, TINT32(m_impPD->m_chunkSize));
  *size = TUINT32(m_impPD->m_chunkSize);
  return ret;
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// TRasterCache
//------------------------------------------------------------------------------

class TRasterCache::Data {
public:
  Data(TCachePersist *cp)
      : m_cp(cp)
      , m_size(0, 0)
      , m_prefetchEnabled(false)
      , m_prefetchedFrame(-1)
      , m_frameToPrefetch(-1)
      , m_preLoader(1, true) {}
  ~Data() {}

  class FrameData {
  public:
    bool m_valid;
    //  int m_size; // dimensione in byte del raster codificato
    ~FrameData() {}
  };

  bool isFrameCached(int frame) const;

  TDimension m_size;  // dimensioni dei raster in cache
  int m_bpp;

  typedef map<int, FrameData> Status;
  Status m_status;
  TCachePersist *m_cp;
  TThread::Mutex m_accessMutex;

  TThread::Executor m_preLoader;
  bool m_prefetchEnabled;
  int m_prefetchedFrame;
  int m_frameToPrefetch;
  TRasterP m_prefetchedRas;
};

bool TRasterCache::Data::isFrameCached(int frame) const {
  // volutamente senza ScopedLock
  Data::Status::const_iterator it = m_status.find(frame);
  if (it == m_status.end()) return false;
  Data::FrameData fd = it->second;
  return fd.m_valid;
}

//==============================================================================

namespace {

class Load : public TThread::Runnable {
public:
  Load(int frameToPrefetch, TCachePersist *cp, int &prefetchedFrame,
       TRasterP &prefetchedRas)
      : m_frame(frameToPrefetch)
      , m_cp(cp)
      , m_prefetchedFrame(prefetchedFrame)
      , m_prefetchedRas(prefetchedRas) {}

  void run();

private:
  int m_frame;
  TCachePersist *m_cp;
  int &m_prefetchedFrame;
  TRasterP &m_prefetchedRas;
};

void Load::run() {
  m_prefetchedRas   = m_cp->doGetRaster(m_frame);
  m_prefetchedFrame = m_frame;
}
};

//==============================================================================

TRasterCache::TRasterCache(TCachePersist *cp) : m_data(new Data(cp)) {}

//------------------------------------------------------------------------------

TRasterCache::~TRasterCache() { delete m_data; }

//------------------------------------------------------------------------------
void TRasterCache::setMode(const TDimension &size, int bpp) {
  TThread::ScopedLock sl(m_data->m_accessMutex);

  m_data->m_size = size;  // dimensioni dei raster in cache
  m_data->m_bpp  = bpp;

  m_data->m_cp->setFrameSize(size.lx, size.ly, bpp);
  invalidate();
}

//------------------------------------------------------------------------------

void TRasterCache::getMode(TDimension &size, int &bpp) const {
  size = m_data->m_size;  // dimensioni dei raster in cache
  bpp  = m_data->m_bpp;
}

//------------------------------------------------------------------------------

TRasterP TRasterCache::getRaster(int frame) const {
  TThread::ScopedLock sl(m_data->m_accessMutex);

  if (m_data->m_prefetchEnabled) {
    /*
if (frame == m_data->m_frameToPrefetch)
m_data->m_preLoader.wait();
else
*/
    {
      m_data->m_preLoader.clear();
      // m_data->m_preLoader.cancel();
    }

    TRasterP ras;
    if (frame == m_data->m_prefetchedFrame)
      ras = m_data->m_prefetchedRas;
    else
      ras = m_data->m_cp->doGetRaster(frame);

    if (isFrameCached(frame + 1)) {
      // il frame successivo a quello richiesto e' nella cache
      // -> avvia il prefetch di tale raster
      m_data->m_frameToPrefetch = frame + 1;
      m_data->m_preLoader.addTask(
          new Load(m_data->m_frameToPrefetch, m_data->m_cp,
                   m_data->m_prefetchedFrame, m_data->m_prefetchedRas));
    }

    return ras;
  } else {
    return m_data->m_cp->doGetRaster(frame);
  }
}

//------------------------------------------------------------------------------

bool TRasterCache::getRaster(int frame, TRaster32P &ras) const {
  TThread::ScopedLock sl(m_data->m_accessMutex);
  if (m_data->isFrameCached(frame)) {
    bool rc = m_data->m_cp->doGetRaster(frame, ras);
    assert(rc);
    return true;
  } else
    return false;
}

//------------------------------------------------------------------------------

void TRasterCache::putRaster(int frame, const TRasterP &ras) {
  TThread::ScopedLock sl(m_data->m_accessMutex);
  Data::Status::iterator it = m_data->m_status.find(frame);
  bool cached               = false;
  try {
    cached = m_data->m_cp->doPutRaster(frame, ras);
  } catch (TException &e) {
    if (it != m_data->m_status.end()) {
      Data::FrameData fd;
      fd.m_valid              = false;
      m_data->m_status[frame] = fd;
    }
    throw e;
  }
  if (cached) {
    Data::FrameData fd;
    fd.m_valid              = true;
    m_data->m_status[frame] = fd;
  }
}

//------------------------------------------------------------------------------

UCHAR *TRasterCache::getRawData(int frame, TINT32 &size, int &lx,
                                int &ly) const {
  return m_data->m_cp->getRawData(frame, size, lx, ly);
}

//------------------------------------------------------------------------------

bool TRasterCache::isFrameCached(int frame) const {
  TThread::ScopedLock sl(m_data->m_accessMutex);
  return m_data->isFrameCached(frame);
}

//------------------------------------------------------------------------------

void TRasterCache::invalidate() {
  TThread::ScopedLock sl(m_data->m_accessMutex);
  m_data->m_status.clear();
  m_data->m_cp->onInvalidate();
}

//------------------------------------------------------------------------------

void TRasterCache::invalidate(int startFrame, int endFrame) {
  assert(startFrame <= endFrame);
  TThread::ScopedLock sl(m_data->m_accessMutex);

  Data::Status::iterator low = m_data->m_status.lower_bound(startFrame);
  Data::Status::iterator hi  = m_data->m_status.upper_bound(endFrame);

#ifdef _DEBUG
  int count = m_data->m_status.size();

  if (low != m_data->m_status.end() && hi != m_data->m_status.end()) {
    int ll = low->first;
    int hh = hi->first;
    assert(ll <= hh);
  }
#endif

  if (low != m_data->m_status.end()) {
    m_data->m_status.erase(low, hi);
    m_data->m_cp->onInvalidate(startFrame, endFrame);
  }
}

//------------------------------------------------------------------------------

void TRasterCache::enablePrefetch(bool newState) {
  m_data->m_prefetchEnabled = newState;
}

//------------------------------------------------------------------------------

bool TRasterCache::isPrefetchEnabled() const {
  return m_data->m_prefetchEnabled;
}

//------------------------------------------------------------------------------

TUINT64 TRasterCache::getUsedSpace() { return m_data->m_cp->getUsedSpace(); }

//==============================================================================
//==============================================================================
//==============================================================================
//              TRamCachePersist
//------------------------------------------------------------------------------
class TRamCachePersist::Imp {
  friend class TRamCachePersist;

public:
  Imp() : m_cacheSize(0), m_chunks() {}
  ~Imp() {
    for (CompressedChunks::iterator it = m_chunks.begin(); it != m_chunks.end();
         ++it) {
      CompressedChunk *cc = it->second;
      m_cacheSize -= cc->m_size;
      delete cc;
    }
    assert(m_cacheSize == 0);  // se m_cacheSize > 0 mi sono perso qualche chunk
    // se m_cacheSize < 0 ho liberato 2 volte qualche chunk
    m_chunks.clear();
  }

  class CompressedChunk {
  public:
    CompressedChunk(UCHAR *buffer, int size) : m_buffer(buffer), m_size(size) {}
    ~CompressedChunk() { delete[] m_buffer; }
    UCHAR *m_buffer;
    int m_size;
  };

  typedef map<int, CompressedChunk *> CompressedChunks;
  CompressedChunks m_chunks;
  TUINT64 m_cacheSize;
};

TRamCachePersist::TRamCachePersist(TRasterCodec *codec)
    : TCachePersist(codec), m_imp(new Imp) {}

//------------------------------------------------------------------------------

TRamCachePersist::~TRamCachePersist() { delete m_imp; }

//------------------------------------------------------------------------------

TRasterP TRamCachePersist::doGetRaster(int frame)
// void TRamCachePersist::doGetRaster(int frame, const TRasterP &ras)
{
  Imp::CompressedChunks::const_iterator it = m_imp->m_chunks.find(frame);
  if (it == m_imp->m_chunks.end()) return TRasterP();
  Imp::CompressedChunk *cc = it->second;
  assert(cc);
  TRasterP rasP;
  m_codec->decompress(cc->m_buffer, cc->m_size, rasP);
  return rasP;
}

//------------------------------------------------------------------------------

bool TRamCachePersist::doGetRaster(int frame, TRaster32P &ras) const {
  Imp::CompressedChunks::const_iterator it = m_imp->m_chunks.find(frame);
  if (it == m_imp->m_chunks.end()) return false;
  Imp::CompressedChunk *cc = it->second;
  assert(cc);
  TRasterP rasP(ras);
  m_codec->decompress(cc->m_buffer, cc->m_size, rasP);
  return true;
}

//------------------------------------------------------------------------------

bool TRamCachePersist::doPutRaster(int frame, const TRasterP &ras) {
  Imp::CompressedChunks::iterator it = m_imp->m_chunks.find(frame);
  if (it != m_imp->m_chunks.end()) {
    m_imp->m_cacheSize -= it->second->m_size;
    delete it->second;
    m_imp->m_chunks.erase(it);
  }

  UCHAR *outData     = 0;
  TINT32 outDataSize = 0;
  m_codec->compress(ras, 1, &outData, outDataSize);
  m_imp->m_cacheSize += outDataSize;
  Imp::CompressedChunk *cc = new Imp::CompressedChunk(outData, outDataSize);
  m_imp->m_chunks.insert(Imp::CompressedChunks::value_type(frame, cc));
  return true;
}

//------------------------------------------------------------------------------

void TRamCachePersist::onInvalidate() {
  for (Imp::CompressedChunks::iterator it = m_imp->m_chunks.begin();
       it != m_imp->m_chunks.end(); ++it) {
    Imp::CompressedChunk *cc = it->second;
    m_imp->m_cacheSize -= cc->m_size;
    delete cc;
  }

  m_imp->m_chunks.clear();
}

//------------------------------------------------------------------------------

void TRamCachePersist::onInvalidate(int startFrame,
                                    int endFrame) {  // ottimizzabile
  assert(startFrame <= endFrame);

  for (int frame = startFrame; frame <= endFrame; ++frame) {
    Imp::CompressedChunks::iterator it = m_imp->m_chunks.find(frame);
    if (it != m_imp->m_chunks.end()) {
      m_imp->m_cacheSize -= it->second->m_size;
      delete it->second;
      m_imp->m_chunks.erase(it);
    }
  }
}

//------------------------------------------------------------------------------

UCHAR *TRamCachePersist::getRawData(int frame, TINT32 &size, int &lx, int &ly) {
  Imp::CompressedChunks::const_iterator it = m_imp->m_chunks.find(frame);
  if (it == m_imp->m_chunks.end()) return 0;
  Imp::CompressedChunk *cc = it->second;
  assert(cc);
  return m_codec->removeHeader(cc->m_buffer, cc->m_size, size, lx, ly);
}

//------------------------------------------------------------------------------

TUINT64 TRamCachePersist::getUsedSpace() { return m_imp->m_cacheSize; }

//------------------------------------------------------------------------------

void TDiskCachePersist::onInvalidate() {
  // m_imp->m_chunkSize = 0;
}

//------------------------------------------------------------------------------

void TDiskCachePersist::onInvalidate(int startFrame, int endFrame) {
  // m_imp->m_chunkSize = 0;
}

//------------------------------------------------------------------------------

TUINT64 TDiskCachePersist::getUsedSpace() {
  assert(0);
  return 0;
}

//==============================================================================
//==============================================================================
//==============================================================================
//
//  TDiskCachePersist2
//
//------------------------------------------------------------------------------
#ifdef WIN32
namespace {

class ZFile {
public:
  ZFile(const TFilePath &fp, bool directIO, bool asyncIO);
  ~ZFile();

  void open();

  int read(BYTE buf[], int size, TINT64 qwOffset) const;
  int write(BYTE buf[], int size, TINT64 qwOffset) const;

  void waitForAsyncIOCompletion() const;

  TFilePath getFilePath() const { return m_filepath; }

  int getBytesPerSector() const { return m_bytesPerSector; }

  static void CALLBACK FileIOCompletionRoutine(DWORD errCode,
                                               DWORD byteTransferred,
                                               LPOVERLAPPED overlapped);

private:
  TFilePath m_filepath;
  bool m_directIO;
  bool m_asyncIO;
  DWORD m_bytesPerSector;

  HANDLE m_fileHandle;
  HANDLE m_writeNotPending;
};

//-----------------------------------------------------------------------------

ZFile::ZFile(const TFilePath &fp, bool directIO, bool asyncIO)
    : m_filepath(fp)
    , m_directIO(directIO)
    , m_asyncIO(asyncIO)
    , m_fileHandle(0)
    , m_writeNotPending(0) {
  DWORD sectorsPerCluster;
  DWORD numberOfFreeClusters;
  DWORD totalNumberOfClusters;

  TFilePathSet disks = TSystem::getDisks();

  TFilePath disk = fp;
  while (std::find(disks.begin(), disks.end(), disk) == disks.end())
    disk = disk.getParentDir();

  BOOL ret = GetDiskFreeSpaceW(disk.getWideString().c_str(),  // root path
                               &sectorsPerCluster,     // sectors per cluster
                               &m_bytesPerSector,      // bytes per sector
                               &numberOfFreeClusters,  // free clusters
                               &totalNumberOfClusters  // total clusters
                               );

  if (m_asyncIO) m_writeNotPending = CreateEvent(NULL, TRUE, TRUE, NULL);
}

//-----------------------------------------------------------------------------

ZFile::~ZFile() {
  if (m_fileHandle) CloseHandle(m_fileHandle);

  if (m_writeNotPending) CloseHandle(m_writeNotPending);
}

//-----------------------------------------------------------------------------

void ZFile::open() {
  DWORD flagsAndAttributes = 0;
  flagsAndAttributes       = m_directIO ? FILE_FLAG_NO_BUFFERING : 0UL;
  flagsAndAttributes |= m_asyncIO ? FILE_FLAG_OVERLAPPED : 0UL;

  // Open the file for write access.
  m_fileHandle =
      CreateFileW(m_filepath.getWideString().c_str(),
                  GENERIC_READ | GENERIC_WRITE,  // Read/Write access
                  0,                             // no sharing allowed
                  NULL,                          // no security
                  OPEN_ALWAYS,  // open it or create new if it doesn't exist
                  flagsAndAttributes,
                  NULL);  // ignored

  if (m_fileHandle == INVALID_HANDLE_VALUE) {
    m_fileHandle = 0;

    char errorMessage[2048];

    DWORD error = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0UL, error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, 2048,
                  NULL);

    throw TException(errorMessage);
  }
}

//-----------------------------------------------------------------------------

int ZFile::read(BYTE buf[], int size, TINT64 qwOffset) const {
  assert(size % m_bytesPerSector == 0);
  assert(qwOffset % m_bytesPerSector == 0);

  char msg[2048] = "";
  unsigned long bytesToRead;  // Padded number of bytes to read.
  unsigned long bytesRead;    // count of bytes actually read

  OVERLAPPED overLapped;
  memset(&overLapped, 0, sizeof(overLapped));

#define DWORDLONG_LO_DWORD(dwl64) ((DWORD)(dwl64))
#define DWORDLONG_HI_DWORD(dwl64) ((DWORD)(dwl64 >> 32))

  // set the overlapped structure with the offsets
  overLapped.Offset     = DWORDLONG_LO_DWORD(qwOffset);
  overLapped.OffsetHigh = DWORDLONG_HI_DWORD(qwOffset);

  if (m_asyncIO) {
    overLapped.hEvent = CreateEvent(NULL,   // SD
                                    TRUE,   // manual reset
                                    FALSE,  // initial state is not signaled
                                    NULL);  // object name
  } else
    overLapped.hEvent = NULL;

  bytesToRead = size;

  // Read a bunch of bytes and store in buf
  int result = ReadFile(m_fileHandle,  // file handle
                        (void *)buf,   // buffer to store data
                        bytesToRead,   // num bytes to read
                        &bytesRead,    // bytes read
                        &overLapped);  // structure for file offsets

  if (!result) {
    DWORD error = GetLastError();
    if (m_asyncIO && ERROR_IO_PENDING == error) {
      if (!GetOverlappedResult(m_fileHandle, &overLapped, &bytesRead, TRUE)) {
        char errorMessage[2048];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0UL, error,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage,
                      2048, NULL);

        throw TException(errorMessage);
      }
    } else {
      char errorMessage[2048];
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0UL, error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage,
                    2048, NULL);

      throw TException(errorMessage);
    }
  }

  return bytesRead;
}

//-----------------------------------------------------------------------------

int ZFile::write(BYTE buf[], int size, TINT64 qwOffset) const {
  assert(size % m_bytesPerSector == 0);
  assert(qwOffset % m_bytesPerSector == 0);

  char msg[2048] = "";
  unsigned long bytesToWrite;      // Padded number of bytes to write.
  unsigned long bytesWritten = 0;  // count of bytes actually written

  int result;

  if (m_asyncIO) {
    OVERLAPPED *overLapped = new OVERLAPPED;
    memset(overLapped, 0, sizeof(OVERLAPPED));

    // set the overlapped structure with the offsets
    overLapped->Offset     = DWORDLONG_LO_DWORD(qwOffset);
    overLapped->OffsetHigh = DWORDLONG_HI_DWORD(qwOffset);
    overLapped->hEvent     = NULL;

    bytesToWrite = size;

    result = WriteFileEx(m_fileHandle,  // file handle
                         (void *)buf,   // data buffer
                         bytesToWrite,  // num bytes to write
                         overLapped,    // structure for file offsets
                         &ZFile::FileIOCompletionRoutine);

    ResetEvent(m_writeNotPending);
  } else {
    OVERLAPPED overLapped;
    memset(&overLapped, 0, sizeof(overLapped));

    // set the overlapped structure with the offsets
    overLapped.Offset     = DWORDLONG_LO_DWORD(qwOffset);
    overLapped.OffsetHigh = DWORDLONG_HI_DWORD(qwOffset);
    overLapped.hEvent     = NULL;

    bytesToWrite = size;

    result = WriteFile(m_fileHandle,   // file handle
                       (void *)buf,    // data buffer
                       bytesToWrite,   // num bytes to read
                       &bytesWritten,  // bytes read
                       &overLapped);   // structure for file offsets
  }

  if (!result) {
    char errorMessage[2048];
    DWORD error = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0UL, error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, 2048,
                  NULL);

    throw TException(errorMessage);
  }

  return bytesWritten;
}

//------------------------------------------------------------------------------

void ZFile::waitForAsyncIOCompletion() const {
  if (m_asyncIO) {
    WaitForSingleObjectEx(m_writeNotPending, INFINITE, TRUE);
    SetEvent(m_writeNotPending);
  }
}

//------------------------------------------------------------------------------

void CALLBACK ZFile::FileIOCompletionRoutine(DWORD errCode,
                                             DWORD byteTransferred,
                                             LPOVERLAPPED overlapped) {
  delete overlapped;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class BufferQueue {
public:
  class Item {
  public:
    Item(int frame, UCHAR *buffer, int bufferSize, int chunkSize)
        : m_frame(frame)
        , m_buffer(buffer)
        , m_bufferSize(bufferSize)
        , m_chunkSize(chunkSize) {}

    int m_frame;
    UCHAR *m_buffer;
    int m_bufferSize;
    TINT64 m_chunkSize;
  };

  BufferQueue(int capacity, int allocUnit)
      : m_capacity(capacity)
      , m_allocUnit(allocUnit)
      , m_notEmpty()
      , m_notFull()
      , m_mutex()
      , m_bufferCount(0)
      , m_nextPutItem(0)
      , m_nextGetItem(0) {
    for (int i = 0; i < m_capacity; ++i)
      m_items.push_back(Item(-1, (UCHAR *)0, 0, 0));
  }

  ~BufferQueue() {
    for (int i = 0; i < m_capacity; ++i) delete[] m_items[i].m_buffer;
  }

  void put(int frame, UCHAR *buffer, int bufferSize, int chunkSize) {
    TThread::ScopedLock sl(m_mutex);
    while (m_bufferCount == m_capacity) m_notFull.wait(sl);

    if (m_items[m_nextPutItem].m_chunkSize != chunkSize) {
      delete[] m_items[m_nextPutItem].m_buffer;
      m_items[m_nextPutItem].m_buffer    = new UCHAR[chunkSize];
      m_items[m_nextPutItem].m_chunkSize = chunkSize;
    }

    memcpy(m_items[m_nextPutItem].m_buffer, buffer, bufferSize);
    m_items[m_nextPutItem].m_frame = frame;

    m_nextPutItem = (m_nextPutItem + 1) % m_capacity;
    ++m_bufferCount;
    m_notEmpty.notifyOne();
  }

  BufferQueue::Item get() {
    TThread::ScopedLock sl(m_mutex);

    while (m_bufferCount == 0) m_notEmpty.wait(sl);

    m_notFull.notifyOne();

    BufferQueue::Item item = m_items[m_nextGetItem];

    m_nextGetItem = (m_nextGetItem + 1) % m_capacity;
    --m_bufferCount;
    return item;
  }

  void saveOne(ZFile *file) {
    TThread::ScopedLock sl(m_mutex);

    while (m_bufferCount == 0) m_notEmpty.wait(sl);

    m_notFull.notifyOne();

    BufferQueue::Item item = m_items[m_nextGetItem];

    m_nextGetItem = (m_nextGetItem + 1) % m_capacity;
    --m_bufferCount;

    TINT64 pos         = item.m_frame * item.m_chunkSize;
    TINT64 sectorCount = pos / m_allocUnit;

    if ((pos % m_allocUnit) != 0) ++sectorCount;

    pos = sectorCount * m_allocUnit;
    file->write(item.m_buffer, (TINT32)item.m_chunkSize, pos);
  }

  int size() {
    TThread::ScopedLock sl(m_mutex);
    return m_bufferCount;
  }

private:
  int m_capacity;
  int m_allocUnit;

  TThread::Condition m_notEmpty;
  TThread::Condition m_notFull;
  TThread::Mutex m_mutex;

  vector<Item> m_items;

  int m_bufferCount;
  int m_nextPutItem;
  int m_nextGetItem;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class WriteBufferTask : public TThread::Runnable {
public:
  WriteBufferTask(ZFile *file, BufferQueue *bufferQueue)
      : m_file(file), m_bufferQueue(bufferQueue) {}

  void run();

  ZFile *m_file;
  BufferQueue *m_bufferQueue;
};

//------------------------------------------------------------------------------

void WriteBufferTask::run() {
  while (true) {
    TThread::milestone();

    try {
      m_bufferQueue->saveOne(m_file);
      m_file->waitForAsyncIOCompletion();
    } catch (TException & /*e*/) {
    } catch (...) {
    }
  }
}

}  // anonymous namespace

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class TDiskCachePersist2::Imp {
public:
  Imp(const TFilePath &fp, bool asyncWrite)
      : m_chunkSize(0)
      , m_readBuffer(0)
      , m_file(new ZFile(fp, true, asyncWrite))
      , m_asyncWrite(asyncWrite)
      , m_executor(0)
      , m_bufferQueue(0) {
    m_file->open();
    m_allocUnit = m_file->getBytesPerSector();

    if (m_asyncWrite) {
      m_executor    = new TThread::Executor();
      m_bufferQueue = new BufferQueue(4, m_allocUnit);
      m_executor->addTask(new WriteBufferTask(m_file, m_bufferQueue));
    }
  }

  ~Imp() {
    delete m_file;
    delete[] m_readBuffer;

    if (m_executor) {
      m_executor->cancel();
      delete m_executor;
    }

    delete m_bufferQueue;
  }

  bool put(int frame, UCHAR *data, TUINT32 dataSize);
  UCHAR *get(int pos, TUINT32 *size);

  TThread::Mutex m_mutex;
  TINT64 m_chunkSize;

  ZFile *m_file;
  int m_allocUnit;
  UCHAR *m_readBuffer;
  int m_lx;
  int m_ly;
  int m_bpp;

  bool m_asyncWrite;
  TThread::Executor *m_executor;
  BufferQueue *m_bufferQueue;
};

//------------------------------------------------------------------------------

bool TDiskCachePersist2::Imp::put(int frame, UCHAR *data, TUINT32 dataSize) {
  if (dataSize != m_chunkSize) return false;

  TINT64 pos         = frame * m_chunkSize;
  TINT64 sectorCount = pos / m_allocUnit;

  if ((pos % m_allocUnit) != 0) ++sectorCount;

  pos = sectorCount * m_allocUnit;

  m_file->write(data, dataSize, pos);
  return true;
}

//------------------------------------------------------------------------------

UCHAR *TDiskCachePersist2::Imp::get(int frame, TUINT32 *size) {
  UCHAR *ret = new UCHAR[TINT32(m_chunkSize)];

  TThread::ScopedLock sl(m_mutex);

  TINT64 pos         = frame * m_chunkSize;
  TINT64 sectorCount = pos / m_allocUnit;

  if ((pos % m_allocUnit) != 0) ++sectorCount;

  pos = sectorCount * m_allocUnit;

  m_file->read(ret, TINT32(m_chunkSize), pos);
  *size = TUINT32(m_chunkSize);
  return ret;
}

//------------------------------------------------------------------------------

TDiskCachePersist2::TDiskCachePersist2(TRasterCodec *codec,
                                       const TFilePath &fullpath)
    : TCachePersist(codec), m_imp(new Imp(fullpath, false /*true*/)) {}

//------------------------------------------------------------------------------

TDiskCachePersist2::~TDiskCachePersist2() { delete m_imp; }

//------------------------------------------------------------------------------

void TDiskCachePersist2::setFrameSize(int lx, int ly, int bpp) {
  m_imp->m_lx  = lx;
  m_imp->m_ly  = ly;
  m_imp->m_bpp = bpp;

  // inizializza m_imp->m_chunkSize in modo che sia un multiplo di
  // m_imp->m_allocUnit
  if (m_codec)
    m_imp->m_chunkSize = m_codec->getMaxCompressionSize(lx * ly * (bpp >> 3));
  else
    m_imp->m_chunkSize = lx * ly * (bpp >> 3);

  TINT64 allocUnitCount = m_imp->m_chunkSize / m_imp->m_allocUnit;
  if ((m_imp->m_chunkSize % m_imp->m_allocUnit) != 0) ++allocUnitCount;

  m_imp->m_chunkSize = allocUnitCount * m_imp->m_allocUnit;
  delete[] m_imp->m_readBuffer;
  m_imp->m_readBuffer = 0;
}

//------------------------------------------------------------------------------

TRasterP TDiskCachePersist2::doGetRaster(int frame) {
  TRasterP outRas;
  TUINT32 size;

  if (!m_imp->m_readBuffer)
    m_imp->m_readBuffer = new UCHAR[TINT32(m_imp->m_chunkSize)];

  TINT64 pos         = frame * m_imp->m_chunkSize;
  TINT64 sectorCount = pos / m_imp->m_allocUnit;

  if ((pos % m_imp->m_allocUnit) != 0) ++sectorCount;

  pos = sectorCount * m_imp->m_allocUnit;

  m_imp->m_file->read(m_imp->m_readBuffer, TINT32(m_imp->m_chunkSize), pos);
  size = TUINT32(m_imp->m_chunkSize);

  if (m_codec)
    m_codec->decompress(m_imp->m_readBuffer, size, outRas);
  else {
    switch (m_imp->m_bpp) {
    case 32: {
      TRaster32P ras(m_imp->m_lx, m_imp->m_ly);
      outRas = ras;
    } break;
    case 64: {
      TRaster64P ras(m_imp->m_lx, m_imp->m_ly);
      outRas = ras;
    } break;
    default:
      throw TException("unsupported pixel format");
      break;
    }

    unsigned int rasSize = outRas->getRowSize() * outRas->getLy();
    assert(size >= rasSize);
    outRas->lock();
    memcpy(outRas->getRawData(), m_imp->m_readBuffer, rasSize);
    outRas->unlock();
  }
  return outRas;
}

//------------------------------------------------------------------------------

bool TDiskCachePersist2::doGetRaster(int frame, TRaster32P &ras) const {
  if (!m_imp->m_readBuffer)
    m_imp->m_readBuffer = new UCHAR[TINT32(m_imp->m_chunkSize)];

  TINT64 pos         = frame * m_imp->m_chunkSize;
  TINT64 sectorCount = pos / m_imp->m_allocUnit;

  if ((pos % m_imp->m_allocUnit) != 0) ++sectorCount;

  pos = sectorCount * m_imp->m_allocUnit;

  TRasterP rasP = ras;
  if (m_codec) {
    m_imp->m_file->read(m_imp->m_readBuffer, TINT32(m_imp->m_chunkSize), pos);
    m_codec->decompress(m_imp->m_readBuffer, TINT32(m_imp->m_chunkSize), rasP);
    assert(rasP->getSize() == ras->getSize());
    ras->copy(rasP);
  } else {
    assert(ras->getLx() == ras->getWrap());
    int rasSize = ras->getRowSize() * ras->getLy();
    ras->lock();
    if (rasSize == m_imp->m_chunkSize)
      m_imp->m_file->read(ras->getRawData(), TINT32(m_imp->m_chunkSize), pos);
    else {
      assert(rasSize < m_imp->m_chunkSize);
      m_imp->m_file->read(m_imp->m_readBuffer, TINT32(m_imp->m_chunkSize), pos);
      memcpy(ras->getRawData(), m_imp->m_readBuffer, rasSize);
    }
    ras->unlock();
  }
  return true;
}

//------------------------------------------------------------------------------

bool TDiskCachePersist2::doPutRaster(int frame, const TRasterP &inRas) {
  UCHAR *outData     = 0;
  TINT32 outDataSize = 0;
  int actualDataSize = 0;

  bool deleteDataBuffer = false;
  if (m_codec) {
    m_codec->compress(inRas, m_imp->m_allocUnit, &outData, outDataSize);
    deleteDataBuffer = true;
    ;
    actualDataSize = outDataSize;
  } else {
    assert(inRas->getLx() == inRas->getWrap());
    int rasSize = inRas->getLx() * inRas->getLy() * inRas->getPixelSize();

    outDataSize = TINT32(m_imp->m_chunkSize);
    inRas->lock();
    if (rasSize == m_imp->m_chunkSize)
      outData = inRas->getRawData();
    else {
      if (!m_imp->m_readBuffer)
        m_imp->m_readBuffer = new UCHAR[TINT32(m_imp->m_chunkSize)];

      memcpy(m_imp->m_readBuffer, inRas->getRawData(), rasSize);
      outData = m_imp->m_readBuffer;
    }
    inRas->unlock();
    actualDataSize = rasSize;
  }

  assert((outDataSize % m_imp->m_allocUnit) == 0);

  bool cached = true;
  if (m_imp->m_asyncWrite)
    m_imp->m_bufferQueue->put(frame, outData, actualDataSize, outDataSize);
  else
    cached = m_imp->put(frame, outData, outDataSize);

  if (deleteDataBuffer) delete[] outData;
  return cached;
}

//------------------------------------------------------------------------------

UCHAR *TDiskCachePersist2::getRawData(int frame, TINT32 &size, int &lx,
                                      int &ly) {
  TUINT32 inDataSize;
  UCHAR *src = m_imp->get(frame, &inDataSize);
  return m_codec->removeHeader(src, inDataSize, size, lx, ly);
}

//------------------------------------------------------------------------------

void TDiskCachePersist2::onInvalidate() {}

//------------------------------------------------------------------------------

void TDiskCachePersist2::onInvalidate(int startFrame, int endFrame) {}

//------------------------------------------------------------------------------

TUINT64 TDiskCachePersist2::getUsedSpace() {
  TFileStatus fs(m_imp->m_file->getFilePath());
  return fs.getSize();
}

//------------------------------------------------------------------------------
#endif  // WIN32
