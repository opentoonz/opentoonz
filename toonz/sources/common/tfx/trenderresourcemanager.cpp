

#include "trenderresourcemanager.h"
#include "trenderer.h"

//***********************************************************************************************
//    Resource managers builder for instance-scoped resource managers
//***********************************************************************************************

/*
This manager class is used to create and destroy instance-scope managers through
the
renderStart/renderEnd notifications. Observe that this involves maintenance of a
container
structure of the active renderIds against resource managers.
*/

class RenderInstanceManagersBuilder final : public TRenderResourceManager {
  T_RENDER_RESOURCE_MANAGER

  typedef std::vector<TRenderResourceManager *> ManagersVector;
  std::map<unsigned long, ManagersVector> m_managersMap;

public:
  RenderInstanceManagersBuilder() {}
  ~RenderInstanceManagersBuilder() {}

  static RenderInstanceManagersBuilder *instance();

  TRenderResourceManager *getManager(unsigned long renderId,
                                     unsigned int idx) const;

  void onRenderInstanceStart(unsigned long id) override;
  void onRenderInstanceEnd(unsigned long id) override;

  bool renderHasOwnership() override { return false; }
};

//===============================================================================================

class RenderInstanceManagersBuilderGenerator final
    : public TRenderResourceManagerGenerator {
public:
  TRenderResourceManager *operator()(void) override {
    return RenderInstanceManagersBuilder::instance();
  }
};

MANAGER_FILESCOPE_DECLARATION(RenderInstanceManagersBuilder,
                              RenderInstanceManagersBuilderGenerator);

//***********************************************************************************************
//    Stub managers and generators
//***********************************************************************************************

/*
These manager-generator stubs are substitutes used to maintain dependency order
about managers
which have render instance scope. They retrieve ordered event calls that are
passed to the
dedicated instanceScope handler.
*/

class InstanceResourceManagerStub final : public TRenderResourceManager {
  TRenderResourceManagerGenerator *m_generator;

public:
  InstanceResourceManagerStub(TRenderResourceManagerGenerator *generator)
      : m_generator(generator) {}

  void onRenderInstanceStart(unsigned long id) override;
  void onRenderInstanceEnd(unsigned long id) override;

  void onRenderFrameStart(double f) override;
  void onRenderFrameEnd(double f) override;

  void onRenderStatusStart(int renderStatus) override;
  void onRenderStatusEnd(int renderStatus) override;
};

//===============================================================================================

class StubGenerator final : public TRenderResourceManagerGenerator {
  TRenderResourceManagerGenerator *m_generator;

public:
  StubGenerator(TRenderResourceManagerGenerator *generator)
      : m_generator(generator) {}

  TRenderResourceManager *operator()() override {
    return new InstanceResourceManagerStub(m_generator);
  }
};

//***********************************************************************************************
//    TRenderResourceManagerGenerator methods
//***********************************************************************************************

std::vector<TRenderResourceManagerGenerator *>
    &TRenderResourceManagerGenerator::generators() {
  static std::vector<TRenderResourceManagerGenerator *> generatorsInstance;
  return generatorsInstance;
}

//-------------------------------------------------------------------------

std::vector<TRenderResourceManagerGenerator *>
    &TRenderResourceManagerGenerator::generators(bool instanceScope) {
  static std::vector<TRenderResourceManagerGenerator *> generatorsInstance;
  static std::vector<TRenderResourceManagerGenerator *> generatorsRenderer;
  return instanceScope ? generatorsInstance : generatorsRenderer;
}

//===============================================================================================

TRenderResourceManagerGenerator::TRenderResourceManagerGenerator(
    bool renderInstanceScope)
    : m_instanceScope(renderInstanceScope) {
  // In case this has a renderInstanceScope, build a stub generator
  if (renderInstanceScope) {
    RenderInstanceManagersBuilder::gen();  // Stubs depend on this manager

    static std::vector<TRenderResourceManagerGenerator *> stubGenerators;
    stubGenerators.push_back(new StubGenerator(this));
  }

  generators().push_back(this);

  std::vector<TRenderResourceManagerGenerator *> &scopeGenerators =
      generators(renderInstanceScope);

  scopeGenerators.push_back(this);
  m_managerIndex = scopeGenerators.size() - 1;
}

//-------------------------------------------------------------------------

TRenderResourceManager *TRenderResourceManagerGenerator::getManager(
    const TRenderer &renderer) const {
  return m_instanceScope ? 0 : renderer.getManager(m_managerIndex);
}

//-------------------------------------------------------------------------

TRenderResourceManager *TRenderResourceManagerGenerator::getManager(
    unsigned long renderId) const {
  return m_instanceScope
             ? RenderInstanceManagersBuilder::instance()->getManager(
                   renderId, m_managerIndex)
             : 0;
}

//***********************************************************************************************
//    "Instance-scoped Managers" - Management methods
//***********************************************************************************************

RenderInstanceManagersBuilder *RenderInstanceManagersBuilder::instance() {
  static RenderInstanceManagersBuilder theInstance;
  return &theInstance;
}

//-------------------------------------------------------------------------

inline TRenderResourceManager *RenderInstanceManagersBuilder::getManager(
    unsigned long renderId, unsigned int idx) const {
  std::map<unsigned long, ManagersVector>::const_iterator it =
      m_managersMap.find(renderId);
  return it == m_managersMap.end() ? 0 : it->second[idx];
}

//-------------------------------------------------------------------------

void RenderInstanceManagersBuilder::onRenderInstanceStart(unsigned long id) {
  // Build the instance managers
  std::map<unsigned long, ManagersVector>::iterator it =
      m_managersMap.insert(std::make_pair(id, ManagersVector())).first;

  std::vector<TRenderResourceManagerGenerator *> &instanceScopeGenerators =
      TRenderResourceManagerGenerator::generators(true);

  unsigned int i;
  for (i = 0; i < instanceScopeGenerators.size(); ++i)
    it->second.push_back((*instanceScopeGenerators[i])());
}

//-------------------------------------------------------------------------

void RenderInstanceManagersBuilder::onRenderInstanceEnd(unsigned long id) {
  // Delete the instance managers
  std::map<unsigned long, ManagersVector>::iterator it = m_managersMap.find(id);

  assert(it != m_managersMap.end());

  unsigned int i;
  for (i = 0; i < it->second.size(); ++i) {
    if (it->second[i]->renderHasOwnership()) delete it->second[i];
  }

  m_managersMap.erase(it);
}

//===============================================================================================

void InstanceResourceManagerStub::onRenderInstanceStart(unsigned long id) {
  RenderInstanceManagersBuilder::instance()
      ->getManager(id, m_generator->getGeneratorIndex())
      ->onRenderInstanceStart(id);
}

//-------------------------------------------------------------------------

void InstanceResourceManagerStub::onRenderInstanceEnd(unsigned long id) {
  RenderInstanceManagersBuilder::instance()
      ->getManager(id, m_generator->getGeneratorIndex())
      ->onRenderInstanceEnd(id);
}

//-------------------------------------------------------------------------

void InstanceResourceManagerStub::onRenderFrameStart(double f) {
  RenderInstanceManagersBuilder::instance()
      ->getManager(TRenderer::renderId(), m_generator->getGeneratorIndex())
      ->onRenderFrameStart(f);
}

//-------------------------------------------------------------------------

void InstanceResourceManagerStub::onRenderFrameEnd(double f) {
  RenderInstanceManagersBuilder::instance()
      ->getManager(TRenderer::renderId(), m_generator->getGeneratorIndex())
      ->onRenderFrameEnd(f);
}

//-------------------------------------------------------------------------

void InstanceResourceManagerStub::onRenderStatusStart(int renderStatus) {
  RenderInstanceManagersBuilder::instance()
      ->getManager(TRenderer::renderId(), m_generator->getGeneratorIndex())
      ->onRenderStatusStart(renderStatus);
}

//-------------------------------------------------------------------------

void InstanceResourceManagerStub::onRenderStatusEnd(int renderStatus) {
  RenderInstanceManagersBuilder::instance()
      ->getManager(TRenderer::renderId(), m_generator->getGeneratorIndex())
      ->onRenderStatusEnd(renderStatus);
}
