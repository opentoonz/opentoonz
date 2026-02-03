#pragma once

#ifndef T_TFILEPATH_IO_INCLUDED
#define T_TFILEPATH_IO_INCLUDED

#ifndef _DEBUG
#undef _STLP_DEBUG
#else
#define _STLP_DEBUG 1
#endif

#include "tfilepath.h"

#include <stdio.h>
// #include <stl/_iosfwd.h>

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

/*=========================================================

  Regole da seguire per evitare HANDLE leaks o doppie chiusure


  {
    Tofstream os(path);
    os << "ok";
  }
  {
    Tofstream os(path);
    int fd = os.fd();
    _write(fd,"ok",2);
  }

  {
    Tofstream os(path);
    FILE *chan = fdopen(_dup(os.fd()),"wb");
    fprintf(chan, "ok");
    int ret = fclose(chan);
    assert(ret==0);
  }

  {
    FILE *chan = fopen(path, mode);
    fprintf(chan, "ok");
    int ret = fclose(chan);
    assert(ret==0);
  }


=========================================================*/

DVAPI FILE *fopen(const TFilePath &fp, std::string mode);

class DVAPI Tifstream final : public std::ifstream {
  FILE *m_file;

public:
  Tifstream(const TFilePath &fp);
  ~Tifstream();
  bool isOpen() const;
  void close();
};

class DVAPI Tofstream : public std::ofstream {
  FILE *m_file;

public:
  Tofstream(const TFilePath &fp, bool append_existing = false);

  ~Tofstream();

  bool isOpen() const;
  void close();
};

#endif
