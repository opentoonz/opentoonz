#pragma once

#ifndef CASMFILEINFO_H
#define CASMFILEINFO_H

#include "tfilepath.h"

//------------------------------------------------------------------------------

class CasmFileInfo {
public:
  CasmFileInfo(const TFilePath &fp);
  void getFrameRange(int &startFrame, int &endFrame, bool &interlaced);

private:
  TFilePath m_fp;
};

#endif
