#pragma once

#ifndef STROKES_DATA_H
#define STROKES_DATA_H

#include "tcommon.h"
#include "tvectorimage.h"
#include "toonzqt/dvmimedata.h"
#include "ttoonzimage.h"
#include "trasterimage.h"

class ToonzImageData;
class FullColorImageData;

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// StrokesData
//-----------------------------------------------------------------------------

class DVAPI StrokesData final : public DvMimeData {
public:
  TVectorImageP m_image;

  StrokesData() {}
  StrokesData(const StrokesData *src) : m_image(src->m_image) {}
  StrokesData(TVectorImage *image) : m_image(image) {}
  ~StrokesData() {}

  StrokesData *clone() const override {
    TVectorImage *vi = 0;
    if (m_image) vi  = dynamic_cast<TVectorImage *>(m_image->cloneImage());
    return new StrokesData(vi);
  }

  // data <- image; copia gli stroke indicati da indices
  void setImage(TVectorImageP image, const std::set<int> &indices);

  // image <- data;
  // se insert==true aggiunge le nuove strokes e mette in indices[] i nuovi
  // indici
  // se insert==false rimpiazza le strokes indicati da indices[]
  void getImage(TVectorImageP image, std::set<int> &indices, bool insert) const;

  ToonzImageData *toToonzImageData(const TToonzImageP &imageToPaste) const;
  FullColorImageData *toFullColorImageData(
      const TRasterImageP &imageToPaste) const;
};

#endif  // STROKES_DATA_H
