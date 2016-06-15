#pragma once

#ifndef TCACHERESOURCE_INCLUDED
#define TCACHERESOURCE_INCLUDED

#include "tgeometry.h"
#include "tsmartpointer.h"
#include "tfilepath.h"

#include "traster.h"
#include "tpalette.h"

#include <QMutex>
#include <QString>
#include <QRegion>

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

//  Forward declarations
class TTile;

//=========================================================================

//============================
//    Cache Resource class
//----------------------------

/*!
The TCacheResource class models generic tile objects suitable for caching
in render processes.

Whereas common TTile instances can be used to represent images bounded on a
specific
rect of the plane, the caching procedures in a render process require certain
generalizations of the tile model that are fulfilled by the TCacheResource
class:

<li> A cache resource has a textual description of its content that we'll call
'name'.
It is the associative key used to retrieve a resource among all the others
stored
in the cache. <\li>
<li> The spatial domain of a cache resource is the entire plane, meaning that
access
methods are not bound to work on a specific rect.
This is necessary since the image data accessed on non-consecutive renders could
cover
sparse regions of the plane. <\li>
<li> For simplicity, the pixel geometry of a cache is coherent to the plane
origin - that is,
pixel corners always have integer coordinates. <\li>
<li> Filled resources have a <b> raster type <\b>, specifying the type of
rasters that are
currently storing. It is forbidden to upload rasters to resources when their
raster type differs.
Empty resources are compatible to all possible raster types. <\li>
<li> Cache resources provide reference counters to either the entire resource
and each
piece of its internal memory model, in order to perform automatic release of
unnecessary
data. <\li>
<li> The resource provides an accessible resume of its currently filled areas,
in the form of
a QRegion. <\li>

In order to use or retrieve a cache resource, users must instantiate a smart
pointer reference
to the resource, providing its name. Please refer to the TCacheResourceP
documentation for this
topic.

\sa TRaster, TTile classes.
*/

class DVAPI TCacheResource {
  TAtomicVar m_refCount;

  struct PointLess {
    int x, y;

    PointLess(int x_, int y_) : x(x_), y(y_) {}
    bool operator<(const PointLess &p) const {
      return x < p.x ? true : x > p.x ? false : y < p.y;
    }
  };

  struct CellData {
    int m_refsCount;
    bool m_referenced;
    bool m_modified;

    CellData() : m_refsCount(0), m_modified(false), m_referenced(false) {}
  };

private:
  std::map<std::string, TCacheResource *>::iterator m_pos;
  TFilePath m_path;
  unsigned long m_id;

  QRegion m_region;

  int m_tileType;
  int m_cellsCount;

  TPaletteP m_palette;

  std::map<PointLess, CellData> m_cellDatas;
  int m_locksCount;

  QMutex m_mutex;

  bool m_backEnabled;
  bool m_invalidated;

  friend class TCacheResourcePool;
  friend class THDCacheResourcePool;

private:
  TCacheResource();
  ~TCacheResource();

  friend class TCacheResourceP;
  inline void addRef() { ++m_refCount; }
  void release();

public:
  const std::string &getName() const { return m_pos->first; }

  QMutex *getMutex() { return &m_mutex; }

  enum Type { NONE, RGBM32, RGBM64, CM32 };
  int getRasterType() const { return m_tileType; }
  TRasterP buildCompatibleRaster(const TDimension &size);

  QRegion getAvailableRegion() const { return m_region; }

  bool upload(const TPoint &pos, TRasterP ras);
  bool upload(const TTile &tile);
  QRegion download(const TPoint &pos, TRasterP ras);
  QRegion download(TTile &tile);
  bool downloadAll(const TPoint &pos, TRasterP ras);
  bool downloadAll(TTile &tile);

  bool uploadPalette(TPaletteP palette);
  void downloadPalette(TPaletteP &palette);

  void clear(QRegion region);

  bool canUpload(const TTile &tile) const;
  bool canDownloadSome(const TTile &tile) const;
  bool canDownloadAll(const TTile &tile) const;

  bool canUpload(int rasType) const {
    return m_tileType == NONE || m_tileType == rasType;
  }
  bool canDownloadSome(const TRect &rect) const;
  bool canDownloadAll(const TRect &rect) const;

  void addRef2(const TRect &rect);
  void release2(const TRect &rect);

  void addLock();
  void releaseLock();

  int size() const;

  void enableBackup();
  void invalidate();

  void save(const TFilePath &fp);

private:
  bool checkRasterType(const TRasterP &ras, int &rasType) const;
  bool checkTile(const TTile &tile) const;

  TRasterP createCellRaster(int rasterType, const std::string &cacheId);

  inline TPoint getCellPos(const TPoint &pos) const;
  inline TPoint getCellPos(const TPointD &pos) const;

  inline PointLess getCellIndex(const TPoint &pos) const;
  inline TPoint getCellPos(const PointLess &cellIndex) const;

  inline std::string getCellName(int idxX, int idxY) const;
  inline std::string getCellCacheId(const TPoint &cellPos) const;
  inline std::string getCellCacheId(int idxX, int idxY) const;

private:
  void setPath(const TFilePath &path);
  const TFilePath &getPath() const;

  void clear();
  void save();

  bool save(const PointLess &cellIndex, TRasterP cellRas = 0) const;
  TRasterP load(const PointLess &cellIndex);

  std::pair<TRasterP, CellData *> touch(const PointLess &cellIndex);
  void releaseCell(const QRect &cellQRect, const PointLess &cellIndex,
                   bool save);

  bool addMark(QString flag);
  bool removeMark(QString flag);
  bool hasMark(QString flag);
};

//=========================================================================

//=====================================
//    Cache Resource Smart pointer
//-------------------------------------

/*!
The TCacheResourceP class implements 'smart' referenced pointers to
TCacheResource
instances.

Smart pointers are typically used to hold references to data that is
automatically freed
once it is no more referenced by any smart pointer instance.
The TCacheResourceP is a specialized smart pointer-like class that references
TCacheResource
instances.
\n \n
In order to retrieve a Cache resource, it is sufficient to know the resource
name and invoke the
\c TCacheResourceP(const std::string&, bool) constructor. The bool optionally
specified as second
argument specifies whether the resource has to be created if none is found with
passed name.

\sa TCacheResource, TSmartPointer classes.
*/

class DVAPI TCacheResourceP {
  TCacheResource *m_pointer;

public:
  TCacheResourceP() : m_pointer(0) {}

  TCacheResourceP(const TCacheResourceP &src) : m_pointer(src.m_pointer) {
    if (m_pointer) m_pointer->addRef();
  }

  TCacheResourceP(const std::string &resourceName, bool createIfNone = false);
  ~TCacheResourceP();

  TCacheResourceP &operator=(const TCacheResourceP &src) {
    TCacheResource *old = m_pointer;
    m_pointer           = src.m_pointer;
    if (m_pointer) m_pointer->addRef();
    if (old) old->release();
    return *this;
  }

  TCacheResource *operator->() const {
    assert(m_pointer);
    return m_pointer;
  }

  TCacheResource &operator*() const {
    assert(m_pointer);
    return *m_pointer;
  }

  TCacheResource *getPointer() const { return m_pointer; }

  bool operator!() const { return m_pointer == 0; }

  operator bool() const { return m_pointer != 0; }

  bool operator<(const TCacheResourceP &res) const {
    return m_pointer < res.m_pointer;
  }

  bool operator==(const TCacheResourceP &p) const {
    return m_pointer == p.m_pointer;
  }

  bool operator!=(const TCacheResourceP &p) const { return !(operator==(p)); }
};

#endif  // TCACHERESOURCE_INCLUDED
