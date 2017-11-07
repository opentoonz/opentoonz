#pragma once

#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <memory>

// TnzCore includes
#include "timage.h"
#include "timageinfo.h"

// STD includes
#include <string>

// Qt includes
#include <QReadWriteLock>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=====================================================

//  Forward declarations

class TFrameId;
class TImageInfo;
class TImageReader;

class QReadWriteLock;

class ImageBuilder;

class TXshSimpleLevel;
//=====================================================

//***************************************************************************************
//    Image Manager declaration
//***************************************************************************************

//! ImageManager is a singleton management class that associates string
//! identifiers to
//! image-building objects, able to fetch images on request.
/*!
    The ImageManager's purpose is similar to that of TImageCache - which is
   Toonz's other
    images storage class - with the difference that users do not supply it
   directly with
    the image to be stored, but rather with an object \a capable of fetching
   that image.
    \n\n
    Said objects must be reimplemented from the base ImageBuilder class, and
   return an
    image, and its specifications, upon request.

    The role of these image-building objects is that of providing the \a default
   image
    for an identifier on-demand, without prior allocation of resources.
    \n\n
    The ImageManager automatically stores built images in the global TImageCache
   instance
    (unless the user explicitly requires not to) in order to minimize the images
   building
    process.
    \n\n
    As long as an image bound in the ImageManager remains unmodified, users can
   rebuild
    default images according to different control modifiers (for example,
   requiring 64-bit
    preference, or image subsampling).
    \n\n
    Images stored in the ImageManager can be modified by the user (using the
   toBeModified
    control flag) or even externally supplied using the setImage() method.

    When an image is modified, it is assumed to override any default-buildable
   images - thus
    it is stored in the cache for retrieval, and any control flag modifier is
   ignored as long as
    the image is not invalidated().
    \n\n
    Invalidation of an image can be requested to revert any changes to the image
   and clear
    the cache.
    \n\n
    Please, observe that ImageManager's image caching function is thread-safe -
   and
    ensures that image building happens only once in case multiple threads
   simultaneously
    require an image that was not cached before. Furthermore, there is no
   serialization
    overhead for already cached images or image specifications in read-only
   access.

    \sa The TImageCache and ImageBuilder classes.
*/

class DVAPI ImageManager {
public:
  enum {
    none           = 0x0,
    dontPutInCache = 0x1,  // Prevents IM from storing built images in the cache
    forceRebuild   = 0x2,  // Forces rebuild of images (like an invalidate())
    toBeModified =
        0x4,          // Whether the retrieved image will be signed as modified
    toBeSaved = 0x8,  // User will save the image, reverts toBeModified

    is64bitEnabled = 0x10,  // Whether 64-bit rasters are allowed to return

    controlFlags = 0xF,  // Flags dealing with management control
    imageFlags =
        ~controlFlags  // ImageManager flags supportable by custom ImageBuilders
  };

public:
  static ImageManager *instance();

  /*!
Binds a string identifier to an ImageBuilder instance, which is necessary before
any image or
image data can be retrieved. If the specified id was already bound, it is first
unbound.
Binding an id to 0 is equivalent to unbinding it.
*/
  void bind(const std::string &id, ImageBuilder *builder);

  //! Unbinds the specified identifier, returning true if the operation
  //! succeeded.
  bool unbind(const std::string &id);

  //! Returns true if the specified id is bound to an image builder.
  bool isBound(const std::string &id) const;

  //! Rebinds to a different builder identifier, returning true if the operation
  //! succeeded.
  bool rebind(const std::string &srcId, const std::string &dstId);

  bool renumber(const std::string &srcId, const TFrameId &fid);

  //! Unbinds all known identifiers, resetting the image manager to its empty
  //! state.
  void clear();

  // load icon (and image) data of all frames into cache
  void loadAllTlvIconsAndPutInCache(TXshSimpleLevel *, std::vector<TFrameId>,
                                    std::vector<std::string>, bool);

  /*!
Returns the image built by the object associated with the specified identifier,
using the
supplied control flags for additional options. Provided the builder type
associated to the
identifier is known, a further external reference can be supplied to the
request. In this case,
users should enforce manual invalidate() invocations whenever the supplied data
changes.

\warning Users take responsibility in modifying the returned image's data \b
only when imFlags
contains the \c toBeModified bit.
*/
  TImageP getImage(const std::string &id, int imFlags, void *extData);

  /*!
Returns the image info associated to the specified identifier.

\warning The return type is not explicitly \c const since some of TImageInfo's
data (namely,
the image's bounding box) may need to be modified by the user together with the
associated image.
Like in getImage(), users take responsibility in modifying the returned data \b
only when imFlags
contains the \c toBeModified bit.
*/
  TImageInfo *getInfo(const std::string &id, int imFlags, void *extData);

  //! Invalidates cached data associated to the specified id, forcing the next
  //! getInfo() or
  //! getImage() to rebuild it.
  bool invalidate(const std::string &id);

  /*!
Overrides the image builder, and manually associates an image to the specified
identifier,
putting the image in cache and updating the stored image info. If the image
reference is
empty, the result is equivalent to invalidate().
*/
  bool setImage(const std::string &id, const TImageP &img);

  //! Returns the ImageBuilder instance associate to specified id, or 0 if none
  //! was found.
  //! \warning The ImageManger retains ownership of the returned object, which
  //! must \b not be deleted.
  ImageBuilder *getBuilder(const std::string &id);

  //! Returns whether the identifier has an associated image in cache.
  bool isCached(const std::string &id);

  /*!
Returns whether the image associated to the specified id has been modified.
Observe that
modifying an image overrides the build-up of the associated default image - ie
any subsequent
call to getImage() will ignore control flags and return the stored modified
image.
*/
  bool isModified(const std::string &id);

private:
  struct Imp;
  std::unique_ptr<Imp> m_imp;

private:
  ImageManager();
  ~ImageManager();

  // Not copyable
  ImageManager(const ImageManager &);
  ImageManager &operator=(const ImageManager &);
};

//***************************************************************************************
//    ImageBuilder declaration
//***************************************************************************************

//! ImageBuilder is the class model for self-creating image objects bound in the
//! ImageManager.
class DVAPI ImageBuilder : public TSmartObject {
  DECLARE_CLASS_CODE

public:
  ImageBuilder();
  virtual ~ImageBuilder();

  bool imageCached() { return m_cached; }
  bool imageModified() { return m_modified; }

  /*!
Returns whether any currently cached image is compatible with requested
specifications,
Returning true prevents ImageBuilder::build() invocations, fetching the cached
data instead.
The default implementation returns true if m_info has positive m_lx and m_ly.
*/
  virtual bool isImageCompatible(int imFlags, void *extData);

  /*!
Returns whether any currently cached image infos are compatible with requested
specifications.
Returning true prevents ImageBuilder::build() invocations, fetching the cached
data instead.
The default implementation returns m_info.m_valid.
*/
  virtual bool areInfosCompatible(int imFlags, void *extData);

public:
  // Helper functions to fill in a TImageInfo structure.
  // Each of these sets info.m_valid at true if the associated infos could be
  // loaded.

  static bool setImageInfo(TImageInfo &info, const TDimension &size);
  static bool setImageInfo(TImageInfo &info, TImage *img);
  static bool setImageInfo(TImageInfo &info, TImageReader *ir);

protected:
  TImageInfo m_info;  //!< Currently cached image data - size, bpp, etc
  int m_imFlags;      //!< Currently cached image control flags

protected:
  //! Builds the image as specified by the supplied control flags and external
  //! data.
  virtual TImageP build(int imFlags, void *extData) = 0;

  //! Returns the image infos as specified by the supplied control flags and
  //! external data.
  virtual bool getInfo(TImageInfo &info, int imFlags, void *extData) = 0;

  // load icon (and image) data of all frames into cache. impremented in
  // ImageLoader
  virtual void buildAllIconsAndPutInCache(TXshSimpleLevel *,
                                          std::vector<TFrameId>,
                                          std::vector<std::string>, bool){};

  virtual void setFid(const TFrameId &fid){};

  //! Clears the builder's cached data.
  virtual void invalidate() {
    m_info    = TImageInfo();
    m_imFlags = ImageManager::none;
  }

private:
  friend class ImageManager;

  QReadWriteLock
      m_imageBuildingLock;  //!< Image building is mutexed by the ImageManager

  bool m_cached;    //!< Whether the associated image was stored in cache
  bool m_modified;  //!< Whether the associated image was modified

private:
  // Not copyable
  ImageBuilder(const ImageBuilder &);
  const ImageBuilder &operator=(const ImageBuilder &);

  void setImageCachedAndModified() {
    m_cached   = true;
    m_modified = true;
  }
};

//-----------------------------------------------------------------------------

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<ImageBuilder>;
#endif

typedef TSmartPointerT<ImageBuilder> ImageBuilderP;

//-----------------------------------------------------------------------------

#endif
