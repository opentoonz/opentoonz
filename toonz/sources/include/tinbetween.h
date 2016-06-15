#pragma once

#ifndef TINBETWEEN_H
#define TINBETWEEN_H

#include <memory>
#include "tcommon.h"

class TVectorImageP;

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TInbetween {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TInbetween(const TVectorImageP firstImage, const TVectorImageP lastImage);

  virtual ~TInbetween();

  TVectorImageP tween(double t) const;
};

#endif
