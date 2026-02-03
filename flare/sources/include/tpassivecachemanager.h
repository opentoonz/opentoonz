#pragma once

#ifndef TPASSIVECACHEMANAGER_INCLUDED
#define TPASSIVECACHEMANAGER_INCLUDED

#include "tfxcachemanager.h"

//=========================================================================

//  Forward declarations
class TXsheet;

//  Typedefs
typedef void (*TreeDescriptor)(std::string &desc, const TFxP &root);

//=========================================================================

//===================================
//    TPassiveCacheManager class
//-----------------------------------

/*!
The TPassiveCacheManager is the cache delegate manager that allows render
processes to cache
fx results from externally specified schematic nodes for future reuse.
Observe that this manager takes no effort in verifying the usefulness of caching
an fx, as the
expected behaviour for such cached results is that of being recycled on future,
\a unpredictable render
instances under the same passive cache management. Active, predictive caching
takes place
under control of another manager class - namely, the TFxCacheManager - for
single render instances.
*/

class DVAPI TPassiveCacheManager final : public TFxCacheManagerDelegate {
  T_RENDER_RESOURCE_MANAGER

private:
  struct FxData {
    TFxP m_fx;
    UCHAR m_storageFlag;
    int m_passiveCacheId;
    // std::set<TCacheResourceP> m_resources;
    std::string m_treeDescription;

    FxData();
    ~FxData();

    /*void insert(const TCacheResourceP& resource);
std::set<TCacheResourceP>::iterator erase(const
std::set<TCacheResourceP>::iterator& it);
void clear();*/
  };

  class ResourcesContainer;

private:
  QMutex m_mutex;

  std::vector<FxData> m_fxDataVector;
  std::set<std::string> m_invalidatedLevels;
  ResourcesContainer *m_resources;
  std::map<std::string, UCHAR> m_contextNames;
  std::map<unsigned long, std::string> m_contextNamesByRenderId;

  bool m_updatingPassiveCacheIds;
  int m_currentPassiveCacheId;

  bool m_enabled;

public:
  TPassiveCacheManager();
  ~TPassiveCacheManager();

  static TPassiveCacheManager *instance();

  void setContextName(unsigned long renderId, const std::string &name);
  void releaseContextNamesWithPrefix(const std::string &prefix);

  TFx *getNotAllowingAncestor(TFx *fx);

  void setEnabled(bool enabled);
  bool isEnabled() const;

  void reset();

  enum StorageFlag { NONE = 0x0, IN_MEMORY = 0x1, ON_DISK = 0x2 };
  void setStorageMode(StorageFlag mode);
  StorageFlag getStorageMode() const;

  int declareCached(TFx *fx, int passiveCacheId);
  void onSceneLoaded();
  int getPassiveCacheId(TFx *fx);

  void enableCache(TFx *fx);
  void disableCache(TFx *fx);
  bool cacheEnabled(TFx *fx);
  void toggleCache(TFx *fx);

  StorageFlag getStorageMode(TFx *fx);

  void invalidateLevel(const std::string &levelName);
  void forceInvalidate();

  void getResource(TCacheResourceP &resource, const std::string &alias,
                   const TFxP &fx, double frame, const TRenderSettings &rs,
                   ResourceDeclaration *resData) override;

  void onRenderInstanceStart(unsigned long renderId) override;
  void onRenderInstanceEnd(unsigned long renderId) override;

  void onRenderStatusEnd(int renderStatus) override;

  bool renderHasOwnership() override { return false; }

public:
  void setTreeDescriptor(TreeDescriptor callback);

  void onFxChanged(const TFxP &fx);
  void onXsheetChanged();

private:
  TreeDescriptor m_descriptorCallback;
  StorageFlag m_currStorageFlag;

private:
  void touchFxData(int &idx);

  int getNewPassiveCacheId();
  int updatePassiveCacheId(int id);

  std::string getContextName();
  void releaseOldResources();
};

#endif  // TPASSIVECACHEMANAGER_INCLUDED
