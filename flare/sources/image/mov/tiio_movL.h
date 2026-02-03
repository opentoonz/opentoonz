#pragma once

#ifndef TIIO_MOV_H
#define TIIO_MOV_H

#include "openquicktime.h"
#include "tlevel_io.h"
#include "tthread.h"

class TImageWriterMov;
class TImageReaderMov;

bool IsQuickTimeInstalled();

class TLevelWriterMov : public TLevelWriter {
public:
  TLevelWriterMov(const TFilePath &path, TWriterInfo *winfo);
  ~TLevelWriterMov();
  TImageWriterP getFrameWriter(TFrameId fid);
  friend class TImageWriterMov;

public:
  static TLevelWriter *create(const TFilePath &f, TWriterInfo *winfo) {
    return new TLevelWriterMov(f, winfo);
  };
  void saveSoundTrack(TSoundTrack *st);
};

class TLevelReaderMov : public TLevelReader {
public:
  TLevelReaderMov(const TFilePath &path);
  ~TLevelReaderMov();
  TImageReaderP getFrameReader(TFrameId fid);
  friend class TImageReaderMov;
  TLevelP loadInfo();

  int m_IOError;

private:
  TThread::Mutex m_mutex;
  short m_refNum;
  short m_resId;
  long m_depth;
  int m_lx, m_ly;
  oqt_t *m_fileMov;
  int m_lastFrameDecoded;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderMov(f);
  };
};

//-----------------------------------------------------------------------------

class TWriterInfoMov : public TWriterInfo {
  // friend TImageWriterMov;
public:
  static TWriterInfo *create(const string &ext) { return new TWriterInfoMov(); }
  ~TWriterInfoMov() {}
  TWriterInfo *clone() const { return new TWriterInfoMov(); }

private:
  TWriterInfoMov() {}

  TWriterInfoMov(const TWriterInfoMov &);

  TWriterInfoMov &operator=(const TWriterInfoMov &);  // not implemented
};

#endif  // TIIO_MOV_H
