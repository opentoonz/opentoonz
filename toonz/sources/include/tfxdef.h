#pragma once

#ifndef TFXDEF_INCLUDED
#define TFXDEF_INCLUDED

#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_DLL
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//----------------------------------------------------------

class TRasterFx;
class TRasterFxPort;
class ITFxNode;
class TParam;

//----------------------------------------------------------

//
//  Fx Arguments
//
class DVAPI ITFxArguments {
public:
  ITFxArguments(){};
  virtual ~ITFxArguments(){};

  virtual void add(TParam *)        = 0;
  virtual void add(TRasterFxPort *) = 0;
};

//----------------------------------------------------------

//
//  RasterPort
//
class DVAPI TRasterFxPort {
  TRasterFx *m_fx;
  string m_name;

public:
  TRasterFxPort(ITFxArguments *args, string name) : m_name(name), m_fx(0) {
    args->add(this);
  };
  ~TRasterFxPort(){};

  string getName() const { return m_name; };
  TRasterFx *operator->() const {
    assert(m_fx != 0);
    return m_fx;
  };
  bool isConnected() const { return m_fx != 0; };

  void setRasterFx(TRasterFx *fx) { m_fx = fx; };

private:
  // not implemented
  TRasterFxPort(const TRasterFxPort &);
  TRasterFxPort &operator=(const TRasterFxPort &);
};

//----------------------------------------------------------

//
// RasterFx
//
class DVAPI TRasterFx {
public:
  TRasterFx(){};
  virtual ~TRasterFx(){};

  virtual bool getBBox(double frame,
                       TRectD &bBox,  // n.b Input/Output
                       TPixel32 &bgColor) = 0;

  virtual string getName() const = 0;

  virtual TRect getInvalidRect(const TRect &max) = 0;

  virtual void compute(double frame, const TTile &tile) = 0;

  virtual void allocateAndCompute(double frame, const TDimension size,
                                  const TPointD &pos, TTile &tile);

private:
  TRasterFx(const TRasterFx &);
  TRasterFx &operator=(const TRasterFx &);
};

//----------------------------------------------------------

#endif
