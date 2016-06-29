#pragma once

#ifndef TSOUND_IO_INCLUDED
#define TSOUND_IO_INCLUDED

#include <QStringList>
#include "tfilepath.h"
#include "tsound.h"

#undef DVAPI
#undef DVVAR
#ifdef TSOUND_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

class TSoundTrackReader;
typedef TSoundTrackReader *TSoundTrackReaderCreateProc(const TFilePath &path);

//--------------------------------------------------------
//!\include sndReader_ex.cpp

/*!
  Superclass for specialized reader of a specific type of audio file
*/
class DVAPI TSoundTrackReader : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  TFilePath m_path;

public:
  TSoundTrackReader(const TFilePath &fp);
  ~TSoundTrackReader();

  virtual TSoundTrackP load() = 0;

  // forse sarebbe il caso di aggiungere un metodo loadInfo()
  // che restituisce una soundtrack con buffer vuoto
  // per accedere alle informazioni sulla soundtrack

  static bool load(const TFilePath &, TSoundTrackP &);

  static void getSupportedFormats(QStringList &names);
  static void define(QString extension, TSoundTrackReaderCreateProc *proc);
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<TSoundTrackReader>;
#endif

class DVAPI TSoundTrackReaderP final
    : public TSmartPointerT<TSoundTrackReader> {
public:
  TSoundTrackReaderP(const TFilePath &fp);
};

//=========================================================

class TSoundTrackWriter;
typedef TSoundTrackWriter *TSoundTrackWriterCreateProc(const TFilePath &path);

//--------------------------------------------------------
//!\include sndWriter_ex.cpp

/*!
  Superclass for specialized writer of a specific type of audio file
*/
class DVAPI TSoundTrackWriter : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  TFilePath m_path;

public:
  TSoundTrackWriter(const TFilePath &fp);
  ~TSoundTrackWriter();

  virtual bool save(const TSoundTrackP &) = 0;

  static bool save(const TFilePath &, const TSoundTrackP &);

  static void getSupportedFormats(QStringList &names);

  static void define(QString extension, TSoundTrackWriterCreateProc *proc);
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<TSoundTrackWriter>;
#endif

class DVAPI TSoundTrackWriterP final
    : public TSmartPointerT<TSoundTrackWriter> {
public:
  TSoundTrackWriterP(const TFilePath &fp);
};

#endif
