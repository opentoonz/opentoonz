#pragma once

#include "tlevel_io.h"
#include "tfile.h"
#include <map>

#ifdef DOPO_LO_FACCIAMO
class TImageReaderWriterZCC;
struct C {
  C() : m_offset(0), m_size(0), m_lx(0), m_ly(0) {}
  C(TINT64 offset, long size, int lx, int ly)
      : m_offset(offset), m_size(size), m_lx(lx), m_ly(ly) {}
  TINT64 m_offset;
  long m_size;
  int m_lx;
  int m_ly;
};

class TLevelReaderWriterZCC : public TLevelReaderWriter {
  friend class TImageReaderWriterZCC;
  TFile m_file;
  TFile m_indexFile;
  bool m_initDone;
  ULONG m_blockSize;
  std::map<int, C> m_map;

public:
  TLevelReaderWriterZCC(const TFilePath &path, TReaderWriterInfo *winfo);
  ~TLevelReaderWriterZCC();
  TImageReaderWriterP getFrameReaderWriter(TFrameId fid);

  static TLevelReaderWriter *create(const TFilePath &f,
                                    TReaderWriterInfo *winfo) {
    return new TLevelReaderWriterZCC(f, winfo);
  };
  TLevelP loadInfo();
  void saveSoundTrack(TSoundTrack *st);
};

//------------------------------------------------------------------------------

class TReaderWriterInfoZCC : public TReaderWriterInfo {
public:
  friend TLevelReaderWriterZCC;

public:
  static TReaderWriterInfo *create(const string &ext);
  ~TReaderWriterInfoZCC();
  TReaderWriterInfo *clone() const;

private:
  TReaderWriterInfoZCC();

  TReaderWriterInfoZCC(const TReaderWriterInfoZCC &);

  TReaderWriterInfoZCC &operator=(
      const TReaderWriterInfoZCC &);  // not implemented
};
#endif
