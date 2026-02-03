

#include "tfile.h"

/*
namespace {
        bool IsWindowsNT() {
                return (LONG)GetVersion()>=0;
        }
};
*/
using namespace TFileConsts;

TFile::TFile(const TFilePath &fname, uint32 flags) : m_hFile(NULL), m_ec(0) {
  open_internal(fname, /*NULL,*/ flags);
}

//------------------------------------------------------------------------------

TFile::~TFile() { close(); }

//------------------------------------------------------------------------------

bool TFile::open(const TFilePath &fname, uint32 flags) {
  return open_internal(fname, flags);
}

//------------------------------------------------------------------------------

bool TFile::open_internal(const TFilePath &fname,
                          /*const wchar_t *pwszFilename,*/ uint32 flags) {
  close();

  // At least one of the read/write flags must be set.
  assert(flags & (kRead | kWrite));

  DWORD dwDesiredAccess = 0;

  if (flags & kRead) dwDesiredAccess = GENERIC_READ;
  if (flags & kWrite) dwDesiredAccess |= GENERIC_WRITE;

  // Win32 docs are screwed here -- FILE_SHARE_xxx is the inverse of a deny
  // flag.

  DWORD dwShareMode                  = FILE_SHARE_READ | FILE_SHARE_WRITE;
  if (flags & kDenyRead) dwShareMode = FILE_SHARE_WRITE;
  if (flags & kDenyWrite) dwShareMode &= ~FILE_SHARE_WRITE;

  // One of the creation flags must be set.
  assert(flags & kCreationMask);

  DWORD dwCreationDisposition;

  uint32 creationType = flags & kCreationMask;

  switch (creationType) {
  case kOpenExisting:
    dwCreationDisposition = OPEN_EXISTING;
    break;
  case kOpenAlways:
    dwCreationDisposition = OPEN_ALWAYS;
    break;
  case kCreateAlways:
    dwCreationDisposition = CREATE_ALWAYS;
    break;
  case kCreateNew:
    dwCreationDisposition = CREATE_NEW;
    break;
  case kTruncateExisting:
    dwCreationDisposition = TRUNCATE_EXISTING;
    break;
  default:
    assert(0);
    return false;
  }

  assert((flags & (kSequential | kRandomAccess)) !=
         (kSequential | kRandomAccess));

  DWORD dwAttributes = FILE_ATTRIBUTE_NORMAL;

  if (flags & kSequential) dwAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
  if (flags & kRandomAccess) dwAttributes |= FILE_FLAG_RANDOM_ACCESS;
  if (flags & kWriteThrough) dwAttributes |= FILE_FLAG_WRITE_THROUGH;
  if (flags & kUnbuffered) dwAttributes |= FILE_FLAG_NO_BUFFERING;
  /*
if (pwszFilename && !IsWindowsNT())
  pszFilename = VDFastTextWToA(pwszFilename);
*/
  // if (pszFilename)
  m_hFile =
      CreateFileW(fname.getWideString().c_str(), dwDesiredAccess, dwShareMode,
                  NULL, dwCreationDisposition, dwAttributes, NULL);
  /*
else
  m_hFile = CreateFileW(pwszFilename, dwDesiredAccess, dwShareMode, NULL,
dwCreationDisposition, dwAttributes, NULL);
*/
  // VDFastTextFree();

  // INVALID_HANDLE_VALUE isn't NULL.  *sigh*

  if (m_hFile == INVALID_HANDLE_VALUE) {
    m_hFile = NULL;

    m_ec = GetLastError();

    if (m_ec == ERROR_FILE_NOT_FOUND) return false;

    if (m_ec == ERROR_PATH_NOT_FOUND && creationType == kOpenExisting)
      return false;

    /*
VDStringA
fname(TFileSplitPathRight(pszFilename?VDString(pszFilename):VDTextWToA(VDStringW(pwszFilename))));

throw MyWin32Error("Cannot open file \"%s\":\n%%s", err, fname.c_str());
*/
  }

  m_FilePosition = 0;

  return true;
}

//------------------------------------------------------------------------------

bool TFile::close() {
  if (m_hFile) {
    HANDLE h = m_hFile;
    m_hFile  = NULL;
    if (!CloseHandle(h)) {
      m_ec = GetLastError();
      return false;
    }
  }
  return true;
}

//------------------------------------------------------------------------------

bool TFile::truncate() {
  DWORD rc     = SetEndOfFile(m_hFile);
  if (rc) m_ec = GetLastError();
  return 0 != rc;
}

//------------------------------------------------------------------------------

TINT32 TFile::read(void *buffer, TINT32 length) {
  DWORD dwActual;

  if (!ReadFile(m_hFile, buffer, (DWORD)length, &dwActual, NULL)) {
    m_ec = GetLastError();
    return 0;
  }

  m_FilePosition += dwActual;

  return dwActual;
}

//------------------------------------------------------------------------------

TINT32 TFile::write(const void *buffer, TINT32 length) {
  DWORD dwActual;

  if (!WriteFile(m_hFile, buffer, (DWORD)length, &dwActual, NULL) ||
      dwActual != (DWORD)length) {
    m_ec = GetLastError();
    return 0;
  }

  m_FilePosition += dwActual;

  return dwActual;
}

//------------------------------------------------------------------------------

bool TFile::seek(TINT64 newPos, SeekMode mode) {
  DWORD dwMode;

  switch (mode) {
  case seekStart:
    dwMode = FILE_BEGIN;
    break;
  case seekCur:
    dwMode = FILE_CURRENT;
    break;
  case seekEnd:
    dwMode = FILE_END;
    break;
  default:
    assert(0);
    return false;
  }

  union {
    TINT64 pos;
    LONG l[2];
  } u = {newPos};

  u.l[0] = SetFilePointer(m_hFile, u.l[0], &u.l[1], dwMode);

  if (u.l[0] == -1 && GetLastError() != NO_ERROR) {
    m_ec = GetLastError();
    return false;
  }

  m_FilePosition = u.pos;
  return true;
}

//------------------------------------------------------------------------------

bool TFile::skip(TINT64 delta) {
  if (!delta) return true;

  char buf[1024];

  if (delta <= sizeof buf)
    return (TINT32)delta == read(buf, (TINT32)delta);
  else
    return seek(delta, seekCur);
}

//------------------------------------------------------------------------------

TINT64 TFile::size() {
  union {
    UINT64 siz;
    DWORD l[2];
  } u;

  u.l[0] = GetFileSize(m_hFile, &u.l[1]);

  DWORD err;

  if (u.l[0] == (DWORD)-1L && (err = GetLastError()) != NO_ERROR)
  // throw MyWin32Error("Error retrieving file size: %%s", err);
  {
    m_ec = GetLastError();
    return (TINT64)(-1);
  }
  return (TINT64)u.siz;
}

//------------------------------------------------------------------------------

TINT64 TFile::tell() { return m_FilePosition; }

//------------------------------------------------------------------------------

bool TFile::isOpen() { return m_hFile != 0; }

//------------------------------------------------------------------------------

string TFile::getLastError() {
  LPVOID lpMsgBuf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, m_ec,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                (LPTSTR)&lpMsgBuf, 0, NULL);
  string s = string(reinterpret_cast<char *>(lpMsgBuf));
  LocalFree(lpMsgBuf);
  return s;
}
