#pragma once

#ifndef TTESSELLATOR_H
#define TTESSELLATOR_H

//#include "tpixel.h"
#include "traster.h"
#include "tgl.h"
#include "tthreadmessage.h"

class TColorFunction;
class TRegionOutline;

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//==================== TTessellator ===========================================
//=============================================================================

class DVAPI TTessellator {
public:
  virtual ~TTessellator() {}

  virtual void tessellate(const TColorFunction *cf, const bool antiAliasing,
                          TRegionOutline &outline, TPixel32 color) = 0;
  virtual void tessellate(const TColorFunction *cf, const bool antiAliasing,
                          TRegionOutline &outline, TRaster32P texture) = 0;
};

//=============================================================================
//==================== OpenGL Tessellator
//===========================================
//=============================================================================

class DVAPI TglTessellator final : public TTessellator {
public:
  // TThread::Mutex m_mutex;

  class DVAPI GLTess {
  public:
#ifdef GLU_VERSION_1_2
    GLUtesselator *m_tess;
#else
#ifdef GLU_VERSION_1_1
    GLUtriangulatorObj *m_tess;
#else
    void *m_tess;
#endif
#endif

    GLTess();
    ~GLTess();
  };

private:
  // static GLTess m_glTess;

  void doTessellate(GLTess &glTess, const TColorFunction *cf,
                    const bool antiAliasing, TRegionOutline &outline);
  void doTessellate(GLTess &glTess, const TColorFunction *cf,
                    const bool antiAliasing, TRegionOutline outline,
                    const TAffine &aff);

public:
  // void tessellate(const TVectorRenderData &rd, TRegionOutline &outline );
  void tessellate(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &outline, TPixel32 color) override;
  void tessellate(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &outline, TRaster32P texture) override;
};

//=============================================================================

#endif
