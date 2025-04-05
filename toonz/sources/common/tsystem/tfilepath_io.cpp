#include "tfilepath_io.h"
#include "tconvert.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>

#ifdef _MSC_VER

#include <io.h>
#include <windows.h>

/*!Returns a pointer to a \b FILE, if it exists. The file is opened using _wfopen_s,
   documentation \b
   http://msdn2.microsoft.com/en-us/library/z5hh6ee9(VS.80).aspx.
         \b fp is the file path, \b mode is the opening mode, read ("r"), write ("w"),
   ... , to
         understand the mode, refer to _wfopen_s documentation.
*/
FILE *fopen(const TFilePath &fp, std::string mode) {
  FILE *pFile;
  errno_t err =
      _wfopen_s(&pFile, fp.getWideString().c_str(), ::to_wstring(mode).c_str());
  if (err == -1) return NULL;
  return pFile;
}

Tifstream::Tifstream(const TFilePath &fp) : std::ifstream(m_file = fopen(fp, "rb")) {
  // Manually set the fail bit here, since the constructor ifstream::ifstream(FILE*)
  // does not do so, even if the argument is a null pointer.
  // The state will be checked in "TIStream::operator bool()" (in tstream.cpp)
  if (!m_file) setstate(failbit);
}

Tifstream::~Tifstream() {
  if (m_file) {
    int ret = fclose(m_file);
    assert(ret == 0);
  }
}

void Tifstream::close() {
  m_file = 0;
  std::ifstream::close();
}

Tofstream::Tofstream(const TFilePath &fp, bool append_existing)
    : std::ofstream(m_file = fopen(fp, append_existing ? "ab" : "wb")) {
  // Manually set the fail bit here, since the constructor ofstream::ofstream(FILE*)
  // does not do so, even if the argument is a null pointer.
  // The state will be checked in "TOStream::operator bool()" (in tstream.cpp)
  if (!m_file) setstate(failbit);
}

Tofstream::~Tofstream() {
  if (m_file) {
    flush();
    int ret = fclose(m_file);
    assert(ret == 0);
  }
}

void Tofstream::close() {
  m_file = 0;
  std::ofstream::close();
}

bool Tifstream::isOpen() const { return m_file != 0; }

bool Tofstream::isOpen() const { return m_file != 0; }

#else

//======================
//
// Non-Windows Version
//
//======================

FILE *fopen(const TFilePath &fp, std::string mode) {
  return fopen(QString::fromStdWString(fp.getWideString()).toUtf8().data(),
               mode.c_str());
}

Tifstream::Tifstream(const TFilePath &fp)
    : std::ifstream(QString::fromStdWString(fp.getWideString()).toUtf8().data(),
               std::ios::binary)
/*: ifstream(openFileForReading(fp), ios::binary)
NO! This constructor is non-standard, although it is present in many versions. 
In MAC, it doesn't exist and casts to char* expecting it to be the file name => compiles but doesn't work
*/
{}

Tifstream::~Tifstream() {}

Tofstream::Tofstream(const TFilePath &fp, bool append_existing)
    : std::ofstream(
          QString::fromStdWString(fp.getWideString()).toUtf8().data(),
          std::ios::binary | (append_existing ? std::ios_base::app : std::ios_base::trunc)) {}

Tofstream::~Tofstream() {}

void Tofstream::close() {}

bool Tifstream::isOpen() const {
  // TODO
  return true;
}

bool Tofstream::isOpen() const {
  // TODO
  return true;
}

#endif
