

#include "toonz/dpiscale.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tcamera.h"
#include "toonz/txshleveltypes.h"
#include "toonz/stage.h"

#include "timagecache.h"
#include "trasterimage.h"
#include "ttoonzimage.h"

//-----------------------------------------------------------------------------

TAffine getDpiAffine(TXshSimpleLevel *level, const TFrameId &fid,
                     bool forceFullSampling) {
  const double factor = Stage::inch;

  if (level->getType() == PLI_XSHLEVEL) return TAffine();
  LevelProperties *prop = level->getProperties();
  TAffine aff;

  std::string imageId = level->getImageId(fid);
  int frameStatus     = level->getFrameStatus(fid);
  if (frameStatus & TXshSimpleLevel::CleanupPreview) {
    TRasterImageP ri = TImageCache::instance()->get(imageId, false);
    if (ri) {
      double dpix = 0, dpiy = 0;
      ri->getDpi(dpix, dpiy);
      if (dpix != 0.0 && dpiy != 0.0)
        return TScale(factor / dpix, factor / dpiy);
    }
  }

  int subs = prop->getSubsampling();
  if (level->getType() != PLI_XSHLEVEL) {
    TPointD dpi                       = level->getDpi(fid);
    if (dpi.x != 0 && dpi.y != 0) aff = TScale(factor / dpi.x, factor / dpi.y);

    if (!forceFullSampling)
      TImageCache::instance()->getSubsampling(imageId, subs);
  }

  if (subs != 1 && !forceFullSampling) aff = TScale(subs) * aff;

  return aff;
}

//-----------------------------------------------------------------------------

TAffine getDpiAffine(TCamera *camera) {
  const double factor = Stage::inch;

  TDimensionD size = camera->getSize();
  TDimension res   = camera->getRes();
  double sx        = factor * size.lx / res.lx;
  double sy        = factor * size.ly / res.ly;
  return TScale(sx, sy);
}

//-----------------------------------------------------------------------------

TPointD getCurrentDpiScale(TXshSimpleLevel *sl, const TFrameId &fid) {
  TAffine aff = getDpiAffine(sl, fid);
  if ((sl->getType() == TZP_XSHLEVEL || sl->getType() == OVL_XSHLEVEL) &&
      sl->getProperties()->getSubsampling() > 1) {
    int subs    = sl->getProperties()->getSubsampling();
    TImageP img = TImageCache::instance()->get(sl->getImageId(fid), false);
    TToonzImageP ti(img);
    TRasterImageP ri(img);
    if (ti)
      subs = ti->getSubsampling();
    else if (ri)
      subs   = ri->getSubsampling();
    double f = 1.0 / subs;
    return aff * TPointD(f, f);
  } else
    return aff * TPointD(1, 1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef FORSE_OBSOLETO

double getCurrentCameraStandDpi() {
  const double defaultDpi = 100;
  TApplication *app       = TApplication::instance();
  if (app->isEditingLevelFrame()) {
    TXshSimpleLevel *sl =
        app->getCurrentLevel() ? app->getCurrentLevel()->getSimpleLevel() : 0;
    if (sl) {
      double dpi = sl->getDpi().x;
      return dpi > 0.0 ? dpi : defaultDpi;
      return sl->getDpi().x;
    } else
      return defaultDpi;
  } else {
    ToonzScene *scene = app->getCurrentScene();
    TCamera *camera   = scene->getCurrentCamera();

    TDimensionD size = camera->getSize();
    TDimension res   = camera->getRes();
    assert(size.lx > 0);
    if (size.lx > 0)
      return res.lx / size.lx;
    else
      return defaultDpi;
  }
}

#endif
