#pragma once

#ifndef MESHTEXTURIZER_H
#define MESHTEXTURIZER_H

#include <memory>

// TnzCore includes
#include "traster.h"
#include "tgl.h"  // OpenGL includes

// STL includes
#include <vector>
#include <deque>

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//******************************************************************************************
//    MeshTexturizer  declaration
//******************************************************************************************

/*!
  \brief    The MeshTexturizer class acts as a management wrapper to OpenGL
  texturing
            capabilities, in particular concerning texture virtualization.

  \details  The first step in OpenGL texturing requires copying a texture into
  the video ram
            (\a VRAM). Now, OpenGL documentation specifies that, although there
  is no limit
            to the \a number of allowed textures, there \a are limits to the \b
  size of said
            textures, which can be tested by asking OpenGL whether it can
  accomodate specific
            texture requests or not.

            Technically, the MeshTexturizer class acts as a virtualization
  manager that can be
            used to allow texturization with arbitrary texture sizes. The
  manager accepts raster
            images that are cut up into tiles and bound to VRAM.

            Please, observe that the number of texture objects is typically not
  an issue, as
            OpenGL is able to swap texture objects between RAM and VRAM,
  depending on which
            textures have been used most frequently. In other words, since
  MeshTexturizer ensures
            the granularity required by OpenGL, textures can be used until the
  process runs out of
            memory - of course, swapping can be a source of slowdowns, though.

  \remark   Subdivision into tiles is designed to work for both premultiplied \a
  and
            non-premultiplied textures.
  \remark   MeshTexturizer instances are thread-safe.
*/

class DVAPI MeshTexturizer {
public:
  struct TextureData;

  enum PremultMode     //! Premultiplication specifier for texture binding.
  { NONPREMULTIPLIED,  //!< Texture is not premultiplied.
    PREMULTIPLIED,     //!< Texture is premultiplied.
  };

public:
  MeshTexturizer();
  ~MeshTexturizer();

  /*! \details  The bindTexture() function is used to specify a 32-bit fullcolor
          raster as a texture to be used for mesh rendering. The supplied
          geometry rect defines its position with respect to compiled meshes.
\remark   The premultiplication specifier is used to tell the texturizer how
          to add a proper transparent border surrounding texture tiles.
          This border is essential to draw transparent pixels beyond a texture
          tile's content.
\returns  The internal id used to reference to the input raster. */

  int bindTexture(
      const TRaster32P &ras,
      const TRectD
          &geometry,  //!< Moves a \a copy of the supplied raster image to VRAM,
      PremultMode premultiplyMode =
          NONPREMULTIPLIED);  //!  and assigns an identification number to it.
                              //!  \param ras  Texture to be copied to VRAM.
                              //!  \param geometry  The texture's geometry in
                              //!  the world - with respect to compiled meshes.
                              //!  \param premultiplyMode  Specified whether ras
                              //!  is to be intended premultiplied or not.

  /*! \warning  The pointer returned by getTextureData() is <B>owned by this
     class</B> -
          it must not be deleted. */

  TextureData *getTextureData(
      int textureId);  //!< Retrieves the texture data built from a previously
  //!  bound texture image.                                       \param
  //!  textureId  Identifier of the texture to be retrieved  \returns See
  //!  summary.

  /*! \details  The texture identifier specified to rebindTexture() is retained,
          and the effect is that of an unbindTexture() - bindTexture() combo.
\note     Pre-compiled mesh data against the old texture can be retained \b if
          the newly supplied texture size and geometry are the \b same as the
old one.  */

  void rebindTexture(
      int textureId, const TRaster32P &ras,
      const TRectD
          &geometry,  //!< Rebinds the image bound to the specified texture
      PremultMode premultiplyMode = NONPREMULTIPLIED);  //!  id to a new image.
  void unbindTexture(int textureId);  //!< Deletes the texture associated to the
                                      //! passed id.           \param textureId
  //! Identifier of the texture to be
  //! unbound.

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

private:
  // Not copyable
  MeshTexturizer(const MeshTexturizer &);
  MeshTexturizer &operator=(const MeshTexturizer &);
};

//******************************************************************************************
//    MeshTexturizer:::CompiledData  definition
//******************************************************************************************

/*!
  \brief    The MeshTexturizer-owned data about a texture.

  \details  The purpose of MeshTexturizer is that of storing in VRAM a set of
  textures
            cut up into tiles. This struture holds the data about the
  end-product of
            texture loading into a MeshTexturizer instance.
*/

struct MeshTexturizer::TextureData {
  struct TileData  //!  Data structure for a texture tile.
  {
    GLuint m_textureId;     //!< OpenGL texture identifier.
    TRectD m_tileGeometry;  //!< The tile's world geometry.
  };

public:
  TRectD m_geom;  //!< The original texture's world geometry.
  std::vector<TileData>
      m_tileDatas;  //!< The texture tiles the original texture was split into.

public:
  TextureData() {}
  TextureData(const TRectD &geom) : m_geom(geom) {}

  ~TextureData() {
    int t, tilesCount = m_tileDatas.size();
    for (t = 0; t < tilesCount; ++t)
      glDeleteTextures(1, &m_tileDatas[t].m_textureId);
  }

private:
  // Not copyable
  TextureData(const TextureData &);
  TextureData &operator=(const TextureData &);
};

#endif  // MESHTEXTURIZER_H
