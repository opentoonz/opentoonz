#pragma once

#ifndef TFILE_H
#define TFILE_H
#include "tcommon.h"
#include <windows.h>

#include "tfilepath.h"

namespace TFileConsts {
enum SeekMode { seekStart = 0, seekCur, seekEnd };

enum eFlags {
  kRead      = 0x00000001,
  kWrite     = 0x00000002,
  kReadWrite = kRead | kWrite,

  kDenyNone  = 0x00000000,
  kDenyRead  = 0x00000010,
  kDenyWrite = 0x00000020,
  kDenyAll   = kDenyRead | kDenyWrite,

  kOpenExisting     = 0x00000100,
  kOpenAlways       = 0x00000200,
  kCreateAlways     = 0x00000300,
  kCreateNew        = 0x00000400,
  kTruncateExisting = 0x00000500,  // not particularly useful, really
  kCreationMask     = 0x0000FF00,

  kSequential   = 0x00010000,
  kRandomAccess = 0x00020000,
  kUnbuffered = 0x00040000,  // much faster on Win32 thanks to the crappy cache,
                             // but possibly bad in Unix?
  kWriteThrough = 0x00080000,

  kAllFileFlags = 0xFFFFFFFF
};
};

typedef __int32 uint32;

class TFile {
  HANDLE m_hFile;
  TINT64 m_FilePosition;

public:
  TFile() {}
  TFile(const TFilePath &fname, uint32 flags = TFileConsts::kRead |
                                               TFileConsts::kDenyWrite |
                                               TFileConsts::kOpenExisting);
  // TFile(const wchar_t *pwszFileName, uint32 flags = TFileConsts::kRead |
  // TFileConsts::kDenyWrite | TFileConsts::kOpenExisting);
  // TFile(VDFileHandle h);
  ~TFile();

  bool open(const TFilePath &fname,
            uint32 flags = TFileConsts::kRead | TFileConsts::kDenyWrite |
                           TFileConsts::kOpenExisting);  // false if failed due
                                                         // to not found or
                                                         // already exists
  // bool	open(const wchar_t *pwszFileName, uint32 flags =
  // TFileConsts::kRead
  // | TFileConsts::kDenyWrite | TFileConsts::kOpenExisting);	// false if
  // failed due to not found or already exists
  bool close();
  bool truncate();

  TINT64 size();
  TINT32 read(void *buffer, TINT32 length);
  TINT32 write(const void *buffer, TINT32 length);

  bool seek(TINT64 newPos, TFileConsts::SeekMode mode = TFileConsts::seekStart);
  bool skip(TINT64 delta);
  TINT64 tell();

  bool isOpen();
  std::string getLastError();

protected:
  bool open_internal(const TFilePath &fname,
                     /* const wchar_t *pwszFilename, */ uint32 flags);

private:
  TFile(const TFile &);
  const TFile &operator=(const TFile &f);
  DWORD m_ec;
};

#endif
