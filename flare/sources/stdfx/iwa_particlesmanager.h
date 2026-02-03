#pragma once

//------------------------------------------------------------------
// Iwa_ParticlesManager for Marnie
// based on ParticlesManager by Digital Video
//------------------------------------------------------------------

#ifndef IWA_PARTICLES_CONTAINER
#define IWA_PARTICLES_CONTAINER

#include "tsmartpointer.h"
#include "trenderresourcemanager.h"
#include "trandom.h"
#include "iwa_particles.h"

#include <QThreadStorage>
#include <QMutex>

//-----------------------------------------------------------------------

//  Forward declarations
class Iwa_Particle;
class TRandom;

#include "iwa_particlesengine.h"

//-----------------------------------------------------------------------

class Iwa_ParticlesManager final : public TRenderResourceManager {
  T_RENDER_RESOURCE_MANAGER

public:
  struct FxData;

  struct FrameData {
    FxData *m_fxData;
    double m_frame;
    TRandom m_random;
    std::list<Iwa_Particle> m_particles;
    bool m_calculated;
    int m_maxTrail;
    int m_totalParticles;

    /*- しきつめ情報 -*/
    QList<ParticleOrigin> m_particleOrigins;

    FrameData(FxData *fxData);
    ~FrameData();

    void buildMaxTrail();
    void clear();
  };

  struct FxData final : public TSmartObject {
    DECLARE_CLASS_CODE

    QThreadStorage<FrameData *> m_frames;

    FxData();
  };

public:
  Iwa_ParticlesManager();
  ~Iwa_ParticlesManager();

  static Iwa_ParticlesManager *instance();

  FrameData *data(unsigned long fxId);

  bool isCached(unsigned long fxId);

private:
  std::map<unsigned long, FxData *> m_fxs;
  QMutex m_mutex;

  int m_renderStatus;

  void onRenderStatusStart(int renderStatus) override;
};

#endif
