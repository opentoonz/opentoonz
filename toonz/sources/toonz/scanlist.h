#pragma once

#ifndef SCANLIST_INCLUDED
#define SCANLIST_INCLUDED

#include "tfilepath.h"

class TRasterImageP;
class TXshSimpleLevel;

//---------------------------------------------------------

class ScanListFrame {
  TXshSimpleLevel *m_xl;
  TFrameId m_fid;

public:
  ScanListFrame();
  ScanListFrame(TXshSimpleLevel *xl, const TFrameId &fid);
  ~ScanListFrame();
  ScanListFrame(const ScanListFrame &);
  ScanListFrame &operator=(const ScanListFrame &);

  std::wstring getName() const;
  std::wstring getLevelName() const;
  TFrameId getFrameId() const;

  TXshSimpleLevel *getLevel() const { return m_xl; }

  void setRasterImage(const TRasterImageP &ras, bool isBW = false) const;
};

//---------------------------------------------------------

class ScanList {
  std::vector<ScanListFrame> m_frames;

public:
  ScanList();

  void addFrame(const ScanListFrame &frame);

  int getFrameCount() const;
  const ScanListFrame &getFrame(int index) const;

  void update(bool includeScannedFrames);

  bool areScannedFramesSelected();
};

#endif
