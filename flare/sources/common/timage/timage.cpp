

#include "timage.h"

#ifndef TNZCORE_LIGHT

#include "tpalette.h"

void TImage::setPalette(TPalette *palette) {
  if (m_palette == palette) return;
  if (palette) palette->addRef();
  if (m_palette) m_palette->release();
  m_palette = palette;
}

TImage::~TImage() {
  if (m_palette) m_palette->release();
}

#else

class TPalette {};

void TImage::setPalette(TPalette *palette) { assert(false); }

TImage::~TImage() {}

#endif

//#include "tiio.h"

DEFINE_CLASS_CODE(TImage, 4)

TImage::TImage() : TSmartObject(m_classCode), m_palette(0) {}
