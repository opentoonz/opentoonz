#include <memory>

// TnzCore includes
#include "tgl.h"

// TnzExt includes
#include "ext/meshtexturizer.h"

// tcg includes
#include "tcg/tcg_list.h"

// Qt includes
#include <QString>
#include <QCache>
#include <QMutex>
#include <QMutexLocker>

#include "ext/ttexturesstorage.h"

//***************************************************************************************
//    Local namespace - structures
//***************************************************************************************

struct TexturesContainer {
  MeshTexturizer
      m_texturizer;  //!< The mesh texturizer - actual textures container
  tcg::list<QString> m_keys;  //!< Keys in the storage

public:
  TexturesContainer() {}

private:
  TexturesContainer(const TexturesContainer &);
  TexturesContainer &operator=(const TexturesContainer &);
};

//***************************************************************************************
//    Local namespace - variables
//***************************************************************************************

namespace {

QMutex l_mutex(QMutex::Recursive);  // A mutex is needed to synchronize access
                                    // to the following objects

std::map<int, TexturesContainer *>
    l_texturesContainers;  // Texture Containers by display lists space id
QCache<QString, DrawableTextureDataP> l_objects(500 * 1024);  // 500 MB cache
                                                              // for now - NOTE:
                                                              // MUST be
                                                              // allocated
                                                              // before the
                                                              // following

}  // namespace

//***************************************************************************************
//    Local namespace - global functions
//***************************************************************************************

namespace {

inline QString textureString(int dlSpaceId, const std::string &texId) {
  return QString::number(dlSpaceId) + "_" + QString::fromStdString(texId);
}

//-------------------------------------------------------------------------------------

inline void deleteTexturesContainer(
    const std::pair<int, TexturesContainer *> &pair) {
  delete pair.second;
}
}

//***************************************************************************************
//    DrawableTextureData implementation
//***************************************************************************************

DrawableTextureData::~DrawableTextureData() {
  QMutexLocker locker(&l_mutex);

  TexturesContainer *texContainer = l_texturesContainers[m_dlSpaceId];

  if (m_dlSpaceId >= 0) {
    // Load the container's display lists space (remember current OpenGL
    // context, too)
    TGLDisplayListsProxy *proxy =
        TGLDisplayListsManager::instance()->dlProxy(m_dlSpaceId);

    TGlContext currentContext = tglGetCurrentContext();

    // Unbind the textures
    {
      QMutexLocker locker(proxy->mutex());

      proxy->makeCurrent();
      texContainer->m_texturizer.unbindTexture(m_texId);
    }

    // Restore OpenGL context - equivalent to tglDoneCurrent if currentContext
    // == TGlContext()
    tglMakeCurrent(currentContext);
  } else
    // Temporary - use current OpenGL context directly
    texContainer->m_texturizer.unbindTexture(m_texId);

  texContainer->m_keys.erase(m_objIdx);
}

//***************************************************************************************
//    TTexturesStorage implementation
//***************************************************************************************

TTexturesStorage::TTexturesStorage() {
  // This singleton is dependent on TGLDisplayListsManager
  TGLDisplayListsManager::instance()->addObserver(this);
}

//-------------------------------------------------------------------------------------

TTexturesStorage::~TTexturesStorage() {
  l_objects.clear();
  std::for_each(l_texturesContainers.begin(), l_texturesContainers.end(),
                deleteTexturesContainer);
}

//-------------------------------------------------------------------------------------

TTexturesStorage *TTexturesStorage::instance() {
  static TTexturesStorage theInstance;
  return &theInstance;
}

//-------------------------------------------------------------------------------------

DrawableTextureDataP TTexturesStorage::loadTexture(const std::string &textureId,
                                                   const TRaster32P &ras,
                                                   const TRectD &geometry) {
  // Try to retrieve the proxy associated to current OpenGL context
  TGlContext currentContext = tglGetCurrentContext();
  int dlSpaceId =
      TGLDisplayListsManager::instance()->displayListsSpaceId(currentContext);

  QString texString(::textureString(dlSpaceId, textureId));

  // Deal with containers
  QMutexLocker locker(&l_mutex);

  // If necessary, allocate a textures container
  std::map<int, TexturesContainer *>::iterator it =
      l_texturesContainers.find(dlSpaceId);
  if (it == l_texturesContainers.end())
    it = l_texturesContainers
             .insert(std::make_pair(dlSpaceId, new TexturesContainer))
             .first;

  MeshTexturizer &texturizer = it->second->m_texturizer;

  DrawableTextureDataP dataPtr = std::make_shared<DrawableTextureData>();
  DrawableTextureData *data    = dataPtr.get();

  data->m_dlSpaceId   = dlSpaceId;
  data->m_texId       = texturizer.bindTexture(ras, geometry);
  data->m_objIdx      = it->second->m_keys.push_back(texString);
  data->m_textureData = texturizer.getTextureData(data->m_texId);

  l_objects.insert(texString, new DrawableTextureDataP(dataPtr),
                   (ras->getLx() * ras->getLy() * ras->getPixelSize()) >> 10);

  if (dlSpaceId < 0) {
    // obj is a temporary. It was pushed in the cache to make space for it -
    // however, it must not be
    // stored. Remove it now.
    l_objects.remove(texString);
  }

  return dataPtr;
}

//-------------------------------------------------------------------------------------

void TTexturesStorage::unloadTexture(const std::string &textureId) {
  QMutexLocker locker(&l_mutex);

  // Remove the specified texture from ALL the display lists spaces
  std::map<int, TexturesContainer *>::iterator it,
      iEnd(l_texturesContainers.end());
  for (it = l_texturesContainers.begin(); it != iEnd; ++it)
    l_objects.remove(::textureString(it->first, textureId));
}

//-----------------------------------------------------------------------------------

void TTexturesStorage::onDisplayListDestroyed(int dlSpaceId) {
  QMutexLocker locker(&l_mutex);

  // Remove the textures container associated with dlSpaceId
  std::map<int, TexturesContainer *>::iterator it =
      l_texturesContainers.find(dlSpaceId);
  if (it == l_texturesContainers.end()) return;

  tcg::list<QString>::iterator st, sEnd(it->second->m_keys.end());

  for (st = it->second->m_keys.begin(); st != sEnd;)  // Note that the increment
                                                      // is performed BEFORE the
                                                      // texture is removed.
    l_objects.remove(*st++);  // This is because texture removal may destroy the
                              // key being addressed,
                              // whose iterator would then be invalidated.
  delete it->second;
  l_texturesContainers.erase(it);
}

//-------------------------------------------------------------------------------------

DrawableTextureDataP TTexturesStorage::getTextureData(
    const std::string &textureId) {
  // Get current display lists space
  TGlContext currentContext = tglGetCurrentContext();
  int dlSpaceId =
      TGLDisplayListsManager::instance()->displayListsSpaceId(currentContext);

  // If there is no known associated display lists space, the texture cannot be
  // stored.
  if (dlSpaceId < 0) return DrawableTextureDataP();

  QMutexLocker locker(&l_mutex);

  // Search the texture object
  QString texString(::textureString(dlSpaceId, textureId));
  if (!l_objects.contains(texString)) return DrawableTextureDataP();

  return *l_objects.object(texString);
}
