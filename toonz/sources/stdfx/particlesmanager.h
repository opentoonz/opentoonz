#pragma once

#ifndef PARTICLES_CONTAINER
#define PARTICLES_CONTAINER

#include "tsmartpointer.h"
#include "trenderresourcemanager.h"
#include "trandom.h"
#include "particles.h"

#include <QThreadStorage>
#include <QMutex>

//-----------------------------------------------------------------------

//  Forward declarations
class Particle;
class TRandom;

//-----------------------------------------------------------------------

class ParticlesManager final : public TRenderResourceManager {
  T_RENDER_RESOURCE_MANAGER

public:
  struct FxData;

  struct FrameData {
    FxData *m_fxData;
    double m_frame;
    TRandom m_random;
    std::list<Particle> m_particles;
    bool m_calculated;
    int m_maxTrail;
    int m_totalParticles;

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
  ParticlesManager();
  ~ParticlesManager();

  static ParticlesManager *instance();

  FrameData *data(unsigned long fxId);

private:
  std::map<unsigned long, FxData *> m_fxs;
  QMutex m_mutex;

  int m_renderStatus;

  void onRenderStatusStart(int renderStatus) override;
};

#endif
