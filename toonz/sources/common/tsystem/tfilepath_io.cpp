

#include "tfilepath_io.h"
#include "tconvert.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>

using namespace std;
#ifdef _MSC_VER

#include <io.h>
#include <windows.h>

/*!Return a pointer to a \b FILE, if exist. The file is opened using _wfopen_s,
   documentation \b
   http://msdn2.microsoft.com/en-us/library/z5hh6ee9(VS.80).aspx.
         \b fp is file path, \b mode is the way open, read ("r"), write ("w"),
   ... , to
         know mode view _wfopen_s documentation.
*/
FILE *fopen(const TFilePath &fp, string mode) {
  FILE *pFile;
  errno_t err =
      _wfopen_s(&pFile, fp.getWideString().c_str(), ::to_wstring(mode).c_str());
  if (err == -1) return NULL;
  return pFile;
}

Tifstream::Tifstream(const TFilePath &fp) : ifstream(m_file = fopen(fp, "rb")) {
  // manually set the fail bit here, since constructor ifstream::ifstream(FILE*)
  // does not so even if the argument is null pointer.
  // the state will be refered in "TIStream::operator bool()" ( in tstream.cpp )
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
    : ofstream(m_file = fopen(fp, append_existing ? "ab" : "wb")) {
  // manually set the fail bit here, since constructor ofstream::ofstream(FILE*)
  // does not so even if the argument is null pointer.
  // the state will be refered in "TOStream::operator bool()" ( in tstream.cpp )
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
// Versione non windows
//
//======================

FILE *fopen(const TFilePath &fp, string mode) {
  return fopen(QString::fromStdWString(fp.getWideString()).toUtf8().data(),
               mode.c_str());
}

Tifstream::Tifstream(const TFilePath &fp)
    : ifstream(QString::fromStdWString(fp.getWideString()).toUtf8().data(),
               ios::binary)
/*: ifstream(openFileForReading(fp), ios::binary)
NO! Questo costruttore non e' standard, anche se e' presente
in molte versioni. Nel MAC non c'e e fa un cast a char*
sperando che sia il nome del file => compila ma non funziona
*/
{}

Tifstream::~Tifstream() {}

Tofstream::Tofstream(const TFilePath &fp, bool append_existing)
    : ofstream(
          QString::fromStdWString(fp.getWideString()).toUtf8().data(),
          ios::binary | (append_existing ? ios_base::app : ios_base::trunc)) {}

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
