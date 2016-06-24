//------------------------------------------------------------------
// Iwa_ParticlesManager for Marnie
// based on ParticlesManager by Digital Video
//------------------------------------------------------------------

#include "trenderer.h"

#include <QMutexLocker>

#include "iwa_particlesmanager.h"

/*
EXPLANATION:

ParticlesManager improves the old particles system as follows - the particles
manager stores the
last particles configuration which had some particle rendered by a thread.
Under normal cicumstances, this means that every thread has the particles
configuration that rendered
last. In case a trail was set, such frame is that beyond the trail.
This managemer works well on the assumption that each thread builds particle in
an incremental timeline.
*/

//--------------------------------------------------------------------------------------------------

DEFINE_CLASS_CODE(Iwa_ParticlesManager::FxData,
                  170) /*- 注：ParticlesManager::FxDataは110 -*/

//--------------------------------------------------------------------------------------------------

typedef std::map<double, Iwa_ParticlesManager::FrameData> FramesMap;

//************************************************************************************************
//    Preliminaries
//************************************************************************************************

class Iwa_ParticlesManagerGenerator final
    : public TRenderResourceManagerGenerator {
public:
  Iwa_ParticlesManagerGenerator() : TRenderResourceManagerGenerator(true) {}

  TRenderResourceManager *operator()(void) override {
    return new Iwa_ParticlesManager;
  }
};

MANAGER_FILESCOPE_DECLARATION(Iwa_ParticlesManager,
                              Iwa_ParticlesManagerGenerator);

//************************************************************************************************
//    FrameData implementation
//************************************************************************************************

Iwa_ParticlesManager::FrameData::FrameData(FxData *fxData)
    : m_fxData(fxData)
    , m_frame((std::numeric_limits<int>::min)())
    , m_calculated(false)
    , m_maxTrail(-1)
    , m_totalParticles(0) {
  m_fxData->addRef();
}

//-------------------------------------------------------------------------

Iwa_ParticlesManager::FrameData::~FrameData() { m_fxData->release(); }

//-------------------------------------------------------------------------

void Iwa_ParticlesManager::FrameData::buildMaxTrail() {
  // Store the maximum trail of each particle
  std::list<Iwa_Particle>::iterator it;
  for (it = m_particles.begin(); it != m_particles.end(); ++it)
    m_maxTrail = std::max(m_maxTrail, it->trail);
}

//-------------------------------------------------------------------------

void Iwa_ParticlesManager::FrameData::clear() {
  m_frame = (std::numeric_limits<int>::min)();
  m_particles.clear();
  m_random         = TRandom();
  m_calculated     = false;
  m_maxTrail       = -1;
  m_totalParticles = 0;
}

//************************************************************************************************
//    FxData implementation
//************************************************************************************************

Iwa_ParticlesManager::FxData::FxData() : TSmartObject(m_classCode) {}

//************************************************************************************************
//    ParticlesContainer implementation
//************************************************************************************************

Iwa_ParticlesManager *Iwa_ParticlesManager::instance() {
  return static_cast<Iwa_ParticlesManager *>(
      Iwa_ParticlesManager::gen()->getManager(TRenderer::renderId()));
}

//-------------------------------------------------------------------------

Iwa_ParticlesManager::Iwa_ParticlesManager() : m_renderStatus(-1) {}

//-------------------------------------------------------------------------

Iwa_ParticlesManager::~Iwa_ParticlesManager() {
  // Release all fxDatas
  std::map<unsigned long, FxData *>::iterator it, end = m_fxs.end();
  for (it = m_fxs.begin(); it != end; ++it) it->second->release();
}

//-------------------------------------------------------------------------

void Iwa_ParticlesManager::onRenderStatusStart(int renderStatus) {
  m_renderStatus = renderStatus;
}

//-------------------------------------------------------------------------

Iwa_ParticlesManager::FrameData *Iwa_ParticlesManager::data(
    unsigned long fxId) {
  QMutexLocker locker(&m_mutex);

  std::map<unsigned long, FxData *>::iterator it = m_fxs.find(fxId);
  if (it == m_fxs.end()) {
    it = m_fxs.insert(std::make_pair(fxId, new FxData)).first;
    it->second->addRef();
  }

  FxData *fxData = it->second;
  FrameData *d   = fxData->m_frames.localData();
  if (!d) {
    d = new FrameData(fxData);
    fxData->m_frames.setLocalData(d);
  }

  return d;
}

bool Iwa_ParticlesManager::isCached(unsigned long fxId) {
  std::map<unsigned long, FxData *>::iterator it = m_fxs.find(fxId);
  return (it != m_fxs.end());
}
