#pragma once

#ifndef TFILETYPE_INCLUDED
#define TFILETYPE_INCLUDED

//#include "tfilepath.h"
#include "tcommon.h"

class TFilePath;
class QString;

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TFileType {

enum Type {

  UNKNOW_FILE = 0,
  LEVEL       = 0x1,
  SCENE       = 0x1000,

  RASTER_IMAGE  = 0x2,
  RASTER_LEVEL  = RASTER_IMAGE | LEVEL,
  VECTOR_IMAGE  = 0x4,
  VECTOR_LEVEL  = VECTOR_IMAGE | LEVEL,
  CMAPPED_IMAGE = 0x8,
  CMAPPED_LEVEL = CMAPPED_IMAGE | LEVEL,
  MESH_IMAGE    = 0x10,
  MESH_LEVEL    = MESH_IMAGE | LEVEL,

  IMAGE = RASTER_IMAGE | VECTOR_IMAGE | CMAPPED_IMAGE | MESH_IMAGE,

  AUDIO_LEVEL   = 0x20 | LEVEL,
  PALETTE_LEVEL = 0x40 | LEVEL,

  TABSCENE   = 0x2000 | SCENE,
  TOONZSCENE = 0x4000 | SCENE,

  VIEWABLE = RASTER_IMAGE | VECTOR_IMAGE | CMAPPED_IMAGE
};

/*!
   * getInfo() returns the TFileType::Type of the filepath.
   * e.g. 'a.tif' => RASTER_IMAGE, 'a..tif' => RASTER_LEVEL, 'a.mov' =>
 * RASTER_LEVEL, 'a.tlv' => CMAPPED_LEVEL, etc.
   * Note!!: in the current implementation, a.0001.tif => RASTER_LEVEL,
 * (probably a bad choice: should be RASTER IMAGE)
   */
//!
DVAPI Type getInfo(const TFilePath &fp);

/*!
   * getInfoFromExtension() returns the TFileType::Type "naturally" associated
 * to a given type (file extension)
   * e.g. 'tif' => RASTER_IMAGE, 'mov' => RASTER_LEVEL, 'tlv' => CMAPPED_LEVEL,
 * etc.
   */
//!
DVAPI Type getInfoFromExtension(const std::string &ext);
DVAPI Type getInfoFromExtension(const QString &ext);

DVAPI void declare(std::string extension, Type type);

inline bool isResource(Type type) { return (type != UNKNOW_FILE); }
inline bool isViewable(Type type) { return (type & VIEWABLE) != 0; }
inline bool isLevel(Type type) { return (type & LEVEL) != 0; }
inline bool isScene(Type type) { return (type & SCENE) != 0; }
inline bool isFullColor(Type type) { return (type & RASTER_IMAGE) != 0; }
inline bool isVector(Type type) { return (type & VECTOR_IMAGE) != 0; }

inline bool isLevelExtension(const std::string &fileExtension) {
  return (getInfoFromExtension(fileExtension) & LEVEL) != 0;
}
inline bool isLevelExtension(const QString &fileExtension) {
  return (getInfoFromExtension(fileExtension) & LEVEL) != 0;
}

inline bool isLevelFilePath(const TFilePath &fp) {
  return (getInfo(fp) & LEVEL) != 0;
}

}  // namespace

#endif
