#pragma once

#ifndef TIIO_PSD_INCLUDED
#define TIIO_PSD_INCLUDED

#include "../../common/psdlib/psd.h"
#include "tlevel_io.h"

class TLevelReaderPsd final : public TLevelReader {
public:
  TLevelReaderPsd(const TFilePath &path);
  ~TLevelReaderPsd();
  TImageReaderP getFrameReader(TFrameId fid) override;
  TLevelP loadInfo() override;

  const TImageInfo *getImageInfo(TFrameId fid) override { return m_info; }
  const TImageInfo *getImageInfo() override { return m_info; }

  void load(TRasterImageP &rasP, int shrinkX = 1, int shrinkY = 1,
            TRect region = TRect());
  TDimension getSize() const { return TDimension(m_lx, m_ly); }
  TRect getBBox() const { return TRect(0, 0, m_lx - 1, m_ly - 1); }

  void setLayerId(int layerId) { m_layerId = layerId; }
  // int m_IOError;

private:
  TFilePath m_path;
  int m_lx, m_ly;
  int m_layersCount;
  TPSDReader *m_psdreader;
  int m_layerId;
  std::map<TFrameId, int> m_frameTable;  // frameID, layerId
public:
  static TLevelReader *create(const TFilePath &f);
  TThread::Mutex m_mutex;
};

class TLevelWriterPsd final : public TLevelWriter {
public:
  TLevelWriterPsd(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterPsd();
  TImageWriterP getFrameWriter(TFrameId fid) override;

  void save(const TImageP &img, int layerId);

private:
public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterPsd(f, winfo);
  };
};

#endif  // TIIO_PSD_H
