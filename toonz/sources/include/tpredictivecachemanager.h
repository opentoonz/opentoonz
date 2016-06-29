#pragma once

#ifndef TPREDICTIVECACHEMANAGER_H
#define TPREDICTIVECACHEMANAGER_H

#include <memory>

#include "tfxcachemanager.h"

#include "tgeometry.h"
#include "tfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================================

//======================================
//    TPredictiveCacheManager class
//--------------------------------------

/*!
The TPredictiveCacheManager is the TFxCacheManagerDelegate used to cache
intermediate
render results due to predictive analysis of the scene schematic.
*/

class DVAPI TPredictiveCacheManager final : public TFxCacheManagerDelegate {
  T_RENDER_RESOURCE_MANAGER

  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TPredictiveCacheManager();
  ~TPredictiveCacheManager();

  static TPredictiveCacheManager *instance();

  int getMaxTileSize() const;
  int getBPP() const;

  void setMaxTileSize(int maxTileSize);
  void setBPP(int bpp);

  void getResource(TCacheResourceP &resource, const std::string &alias,
                   const TFxP &fx, double frame, const TRenderSettings &rs,
                   ResourceDeclaration *resData) override;

  void onRenderStatusStart(int renderStatus) override;
  void onRenderStatusEnd(int renderStatus) override;
};

#endif  // TPREDICTIVECACHEMANAGER_H
