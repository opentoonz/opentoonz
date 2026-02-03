

#include "toonz/textureutils.h"

// TnzLib includes
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobject.h"
#include "toonz/stage.h"
#include "toonz/toonzscene.h"
#include "toonz/imagemanager.h"
#include "imagebuilders.h"

// TnzCore includes
#include "tpalette.h"
#include "tconvert.h"
#include "ttoonzimage.h"
#include "trop.h"
#include "tropcm.h"
#include "tgl.h"

//********************************************************************************************
//    TXshSimpleLevel Texture Utilities  locals
//********************************************************************************************

namespace {

TRasterImageP convert32(const TImageP &img) {
  struct locals {
    static TRasterImageP depremultiplied(const TRasterImageP &ri) {
      assert(ri->getRaster());

      TRop::depremultiply(ri->getRaster());
      return ri;
    }
  };  // locals

  if (TRasterImageP ri = img) {
    TRasterP ras(ri->getRaster());

    TRaster32P ras32;
    {
      if (TRaster32P(ras))
        ras32 = ras->clone();
      else {
        ras32 = TRaster32P(ras->getSize());
        TRop::convert(ras32, ras);
      }
    }

    TPointD dpi;
    ri->getDpi(dpi.x, dpi.y);

    TRasterImageP ri32(ras32);
    ri32->setDpi(dpi.x, dpi.y);
    ri32->setSubsampling(ri->getSubsampling());
    ri32->setOffset(ri->getOffset());

    return locals::depremultiplied(ri32);
  }

  if (TToonzImageP ti = img) {
    TRasterCM32P rasCM32(ti->getRaster());

    TRaster32P ras32(rasCM32->getSize());
    TRop::convert(ras32, rasCM32, ti->getPalette());

    TPointD dpi;
    ti->getDpi(dpi.x, dpi.y);

    TRasterImageP ri32(ras32);
    ri32->setDpi(dpi.x, dpi.y);
    ri32->setSubsampling(ti->getSubsampling());
    ri32->setOffset(ti->getOffset());

    return locals::depremultiplied(ri32);
  }

  return TRasterImageP();
}

//-----------------------------------------------------------------------------

TRasterImageP getTexture(const TXshSimpleLevel *sl, const TFrameId &fid,
                         int subsampling) {
  if (sl->getType() != PLI_XSHLEVEL) {
    TImageP texImg =
        sl->getFrame(fid, ImageManager::dontPutInCache, subsampling);
    return convert32(texImg);
  }

  // Vector case
  std::string id = sl->getImageId(fid) + "_rasterized";

  ImageLoader::BuildExtData extData(sl, fid);
  TRasterImageP ri(ImageManager::instance()->getImage(
      id, ImageManager::dontPutInCache, &extData));

  return convert32(ri);
}

}  // namespace

//********************************************************************************************
//    TXshSimpleLevel Texture Utilities  implementation
//********************************************************************************************

DrawableTextureDataP texture_utils::getTextureData(const TXshSimpleLevel *sl,
                                                   const TFrameId &fid,
                                                   int subsampling) {
  const std::string &texId = sl->getImageId(fid);

  // Now, we must associate a texture
  DrawableTextureDataP data(
      TTexturesStorage::instance()->getTextureData(texId));
  if (data) return data;

  // There was no associated texture. We must bind the texture and repeat

  // First, retrieve the image to be used as texture
  TRasterImageP ri(::getTexture(sl, fid, subsampling));
  if (!ri) return DrawableTextureDataP();

  TRaster32P ras(ri->getRaster());
  assert(ras);

  TRectD geom(0, 0, ras->getLx(), ras->getLy());
  geom = TScale(ri->getSubsampling()) *
         TTranslation(convert(ri->getOffset()) - ras->getCenterD()) * geom;

  return TTexturesStorage::instance()->loadTexture(texId, ras, geom);
}

//-----------------------------------------------------------------------------

void texture_utils::invalidateTexture(const TXshSimpleLevel *sl,
                                      const TFrameId &fid) {
  const std::string &texId = sl->getImageId(fid);
  TTexturesStorage::instance()->unloadTexture(texId);
}

//-----------------------------------------------------------------------------

void texture_utils::invalidateTextures(const TXshSimpleLevel *sl) {
  int f, fCount = sl->getFrameCount();

  for (f = 0; f != fCount; ++f) invalidateTexture(sl, sl->getFrameId(f));
}

//********************************************************************************************
//    TXsheet Texture Utilities  locals
//********************************************************************************************

namespace {

std::string getImageId(const TXsheet *xsh, int frame) {
  return "X" + std::to_string(xsh->id()) + "_" + std::to_string(frame);
}

}  // namespace

//********************************************************************************************
//    TXsheet Texture Utilities  implementation
//********************************************************************************************

DrawableTextureDataP texture_utils::getTextureData(const TXsheet *xsh,
                                                   int frame) {
  // Check if an xsheet texture already exists
  const std::string &texId = ::getImageId(xsh, frame);

  DrawableTextureDataP data(
      TTexturesStorage::instance()->getTextureData(texId));
  if (data) return data;

  // No available texture - we must build and load it
  TRaster32P tex(
      1024,
      1024);  // Fixed texture size. It's the same that currently happens with
              // vector images' textures - and justified since this is camstand
  // mode, and besides we want to make sure that textures are limited.

  // Retrieve the sub-xsheet bbox (world coordinates of the sub-xsheet)
  TRectD bbox(xsh->getBBox(frame));

  // Since xsh represents a sub-xsheet, its camera affine must be applied
  const TAffine &cameraAff =
      xsh->getPlacement(xsh->getStageObjectTree()->getCurrentCameraId(), frame);
  bbox = (cameraAff.inv() * bbox).enlarge(1.0);

// Render the xsheet on the specified bbox
#ifdef MACOSX
  xsh->getScene()->renderFrame(tex, frame, xsh, bbox, TAffine());
#else
  // The call below will change context (I know, it's a shame :( )
  TGlContext currentContext = tglGetCurrentContext();
  {
    tglDoneCurrent(currentContext);
    xsh->getScene()->renderFrame(tex, frame, xsh, bbox, TAffine());
    tglMakeCurrent(currentContext);
  }
#endif

  TRop::depremultiply(tex);  // Stored textures are rendered nonpremultiplied

  // Store the texture for future retrieval
  return TTexturesStorage::instance()->loadTexture(texId, tex, bbox);
}

//-----------------------------------------------------------------------------

void texture_utils::invalidateTexture(const TXsheet *xsh, int frame) {
  const std::string &texId = ::getImageId(xsh, frame);
  TTexturesStorage::instance()->unloadTexture(texId);
}

//-----------------------------------------------------------------------------

void texture_utils::invalidateTextures(const TXsheet *xsh) {
  int f, fCount = xsh->getFrameCount();
  for (f = 0; f != fCount; ++f) invalidateTexture(xsh, f);
}
