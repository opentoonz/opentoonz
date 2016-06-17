#pragma once

#ifndef TSTENCILCONTROL_H
#define TSTENCILCONTROL_H

#include <memory>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TGL_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//------------------------------------------------------

// singleton
class DVAPI TStencilControl {
public:
  enum MaskType { SHOW_INSIDE = 0, SHOW_OUTSIDE };

  enum DrawMode {
    DRAW_ONLY_MASK = 0,
    DRAW_ALSO_ON_SCREEN,
    DRAW_ON_SCREEN_ONLY_ONCE
  };

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  static TStencilControl *instance();

  TStencilControl();
  ~TStencilControl();

  void beginMask(DrawMode drawMode = DRAW_ONLY_MASK);
  void endMask();

  void enableMask(MaskType maskType);
  void disableMask();
};

//------------------------------------------------------

#endif
