#pragma once

#ifndef T_TILE_INCLUDED
#define T_TILE_INCLUDED

#include "traster.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "timagecache.h"
#undef DVAPI
#undef DVVAR
#ifdef TRASTER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TTile {
private:
  std::string m_rasterId;
  TRect m_subRect;
  TTile(const TTile &);
  TTile &operator=(const TTile &);
  void addInCache(const TRasterP &raster);

public:
  TPointD m_pos;

  TTile() : m_rasterId(""), m_pos(), m_subRect() {}
  TTile(const TRasterP &raster);
  TTile(const TRasterP &raster, TPointD pos);
  ~TTile();

  void setRaster(const TRasterP &raster);
  inline const TRasterP getRaster() const {
    TImageP img        = TImageCache::instance()->get(m_rasterId, true);
    TRasterImageP rimg = (TRasterImageP)img;
    if (rimg) {
      if (m_subRect == rimg->getRaster()->getBounds())
        return rimg->getRaster();
      else
        return rimg->getRaster()->extract(m_subRect.x0, m_subRect.y0,
                                          m_subRect.x1, m_subRect.y1);
    }

    TToonzImageP timg = (TToonzImageP)img;
    if (timg) {
      if (m_subRect == timg->getRaster()->getBounds())
        return timg->getRaster();
      else
        return timg->getRaster()->extract(m_subRect.x0, m_subRect.y0,
                                          m_subRect.x1, m_subRect.y1);
    }

    return TRasterP();
  }
};

#endif
