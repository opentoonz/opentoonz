#pragma once

#ifndef TEXTURE_MANAGER
#define TEXTURE_MANAGER

#include "tgeometry.h"
#include "tgl.h"

class TextureManager {
  TDimension m_textureSize;
  bool m_isRGBM;
  static TextureManager *m_instance;

  TextureManager();

public:
  static TextureManager *instance();
  TDimension getMaxSize(bool isRGBM);
  void getFmtAndType(bool isRGBM, GLenum &fmt, GLenum &type);
  TDimension selectTexture(TDimension reqSize, bool isRGBM);
};

#endif  // TEXTURE_MANAGER
