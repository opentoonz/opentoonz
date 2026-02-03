#pragma once

#ifndef TIMAGECACHE_H
#define TIMAGECACHE_H

#include <memory>

// TnzCore includes
#include "tcommon.h"
#include "timage.h"
#include "tsmartpointer.h"

// Qt includes
#ifndef TNZCORE_LIGHT
#include <QString>
#endif

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=====================================================

//  Forward declarations

class TFilePath;

//=====================================================

//************************************************************************************************
//    Toonz Image Cache  declaration
//************************************************************************************************

//! The Image Cache is the global container used throughout Toonz to store
//! images that need to be
//! retrieved at later times.
/*!
  TImageCache is Toonz's low-level manager singleton that deals with image
storage.
\n\n
  The main task of TImageCache is that of acting as an associative container
that maps
  a name (of std::string type) into an image smart pointer. Many-to-one
relationships
  where multiple names map to the same image \a are supported.
  Thus, users can add() an image and get() it later, until comes the time to
remove() it.
\n\n
  An important reason to use TImageCache instead of direct storage of TImageP or
TRasterP instances
  is that it takes care of verifying memory shortages in the process' virtual
address space, and
  in case either compresses or ships to disk unreferenced images with
last-access precedence.

\warning Memory-hungry tasks should always use TImageCache to store images,
since it prevents abuses
of system memory. This is especially true on 32-bit OSes.
*/
class DVAPI TImageCache {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  static TImageCache *instance();

  std::string getUniqueId();

  //! Enables or disables the image cache on current thread.
  //! When the cache is disabled, images can't be added to the cache.
  void setEnabled(bool enable);

  //! Returns true or false whether the cache is active or not.
  bool isEnabled();

  //! Sets the hard disk swap directory. It is set by default in the
  //! \it{stuff\cache} folder.
  void setRootDir(const TFilePath &fp);

  //! Adds the passed image to the cache, under the specified id. The optional
  //! \b overwrite parameter
  //! may be specified in case an image with the same id has already been
  //! cached.
  void add(const std::string &id, const TImageP &img, bool overwrite = true);

  //! Removes from the image cache the image associated to passed id.
  void remove(const std::string &id);

  //! Rebinds the association between a previously cached image and its id.
  void remap(const std::string &dstId, const std::string &srcId);

  // provvisorio? Bisogna ripensarci
  // l'idea e' rimappare tutte gli id della forma srcId:xxxxx
  void remapIcons(const std::string &dstId, const std::string &srcId);

  //! Clears the image cache. After calling this method, every image in the
  //! cache
  //! is released - and optionally the cache swap folder is deleted.
  void clear(bool deleteFolder = false);

  //! Clears the image cache of all images which belong to current scene. This
  //! spares
  //! images (typically file browser icons) which are scene-independent.
  //! \n \n
  //! \b{NOTE:} Scene-independent images are simply assumed to have the "$:" id
  //! prefix.
  void clearSceneImages();

  //! Returns true or false whether an image under the passed id is found in the
  //! cache.
  bool isCached(const std::string &id) const;

  //! Returns the subsampling level of the image under the specified id.
  bool getSubsampling(const std::string &id, int &subs) const;

  //! Retrieves the image associated to input \b id. Returns an empty pointer if
  //! no image was found.
  TImageP get(const std::string &id, bool toBeModified) const;

  //! Returns the RAM memory size (KB) occupied by the image cache.
  UINT getMemUsage() const;
  //! Returns the swap files size (KB) currently allocated by the image cache.
  //! \n \n \b{NOTE:} This function is not implemented yet!
  UINT getDiskUsage() const;

  UINT getUncompressedMemUsage(const std::string &id) const;

  //! Returns the RAM memory size (KB) of the image associated to passed id.
  UINT getMemUsage(const std::string &id) const;
  UINT getDiskUsage(const std::string &id) const;

  void dump(std::ostream &os) const;  // per debug

  UCHAR *compressAndMalloc(TUINT32 requestedSize);  // compress in the cache
                                                    // till it can allocate the
                                                    // requested memory

  // for debug
  void outputMap(UINT chunkRequested, std::string filename);

  bool hasBeenModified(const std::string &id, bool reset) const;

#ifndef TNZCORE_LIGHT
  void add(const QString &id, const TImageP &img, bool overwrite = true);
  void remove(const QString &id);
  TImageP get(const QString &id, bool toBeModified) const;
#endif

  // compress id (in memory)
  void compress(const std::string &id);

private:
  TImageCache();
  ~TImageCache();

  // not implemented
  TImageCache(const TImageCache &);
  void operator=(const TImageCache &);
};

//************************************************************************************************
//    Toonz Image Cache  declaration
//************************************************************************************************

//! The TCachedImage class implements a shared reference to an image that is
//! stored under TImageCache.
class DVAPI TCachedImage final : public TSmartObject {
  DECLARE_CLASS_CODE

  std::string m_ref;

public:
  TCachedImage();
  TCachedImage(const TImageP &img);
  ~TCachedImage();

  //! Stores the supplied image in the cache
  void setImage(const TImageP &img, bool overwrite = true);

  //! Retrieves the associated image from the cache
  TImageP image(bool toBeModified = true);
};

//-----------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TCachedImage>;
#endif

typedef TSmartPointerT<TCachedImage> TCachedImageP;

#endif  // TIMAGECACHE_H
