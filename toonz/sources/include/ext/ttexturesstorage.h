#pragma once

#ifndef DRAWABLEMESHIMAGE_H
#define DRAWABLEMESHIMAGE_H

#include <memory>

// TnzExt includes
#include "meshtexturizer.h"

// TnzCore includes
#include "tgldisplaylistsmanager.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//***************************************************************************************
//    DrawableMeshImage definition
//***************************************************************************************

//! DrawableTextureData is a MeshTexturizer::TextureData wrapper that
struct DrawableTextureData {
  MeshTexturizer::TextureData *m_textureData;  //!< The wrapped texture data

public:
  DrawableTextureData() {}
  ~DrawableTextureData();

private:
  friend class TTexturesStorage;

  int m_texId;      //!< The texture Id in MeshTexturizer
  int m_dlSpaceId;  //!< Object's display lists id
  int m_objIdx;     //!< Object index in the container

private:
  // Not copyable

  DrawableTextureData(const DrawableTextureData &);
  DrawableTextureData &operator=(const DrawableTextureData &);
};

typedef std::shared_ptr<DrawableTextureData> DrawableTextureDataP;

//***************************************************************************************
//    TexturesStorage declaration
//***************************************************************************************

/*!
  \brief    TTexturesStorage is the class responsible for the storage of
  textures associated with mesh rendering.

  \details  This class deals with textures storage and meshes compilation for
  drawing purposes.
            An OpenGL texture is typically split into several tiles before being
  rendered. A texturized mesh is then
            compiled against a texture in order to decide which mesh faces must
  be rendered with a given texture
            tile.

            The TTextureStorage class uses a QCache to store texture objects up
  to a maximum memory size, plus all
            the already compiled mesh objects against the stored textures.
  Whenever a texture loading procedure would
            exceed the maximum textures size, the least accessed stored textures
  are automatically removed from the
            storage to make space for the newly assigned one.

            Textures can be \a stored only if the OpenGL context they are loaded
  on has a valid known <I>display lists
            proxy</I> submitted in the TGLDisplayListsManager. In case the
  OpenGL context does not have a proxy, the
            loaded texture is a temporary, and will be deleted as soon as the
  returned TexturObject is destroyed
            (so, make sure the associated context is still current at that
  point).

  \note     TTexturesStorage is thread-safe.

  \sa       The TGLDisplayListsManager class.
*/

class DVAPI TTexturesStorage : private TGLDisplayListsManager::Observer {
public:
  static TTexturesStorage *instance();

  /*!
\brief    Stores the specified raster to a group of OpenGL textures, returning a
reference
        pointer ensuring the texture survival during its lifetime.

\warning  This class may keep a copy of the specified texture only for a \b
limited
        amount of time, \a if the current context has an associated display
lists
        space proxy. Users must always be ready to \a reload the texture in case
        it was not found.
*/
  DrawableTextureDataP loadTexture(const std::string &textureId,
                                   const TRaster32P &ras,
                                   const TRectD &geometry);

  //! Releases the texture associated with the specified texture id.
  void unloadTexture(const std::string &textureId);

  //! Returns the texture data associated to the specified string identifier.
  DrawableTextureDataP getTextureData(const std::string &textureId);

private:
  TTexturesStorage();
  ~TTexturesStorage();

  void onDisplayListDestroyed(int dlSpaceId) override;
};

#endif  // DRAWABLEMESHIMAGE_H
