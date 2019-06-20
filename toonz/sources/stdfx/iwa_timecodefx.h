#pragma once

#ifndef IWA_TIMECODEFX_H
#define IWA_TIMECODEFX_H

#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"

//******************************************************************
//	Iwa_TimeCode Fx  class
//******************************************************************

class Iwa_TimeCodeFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_TimeCodeFx)

public:
  TIntEnumParamP m_displayType;  // - SMPTE HH;MM;SS;FF
                                 // - FrameNumber [######]
  TIntParamP m_frameRate;
  TIntParamP m_startFrame;
  TPointParamP m_position;
  TDoubleParamP m_size;
  TPixelParamP m_textColor;
  TBoolParamP m_showBox;
  TPixelParamP m_boxColor;

  QString getTimeCodeStr(double frame, const TRenderSettings &ri);

  template <typename RASTER, typename PIXEL>
  void putTimeCodeImage(const RASTER srcRas, TPoint &pos, QImage &img);

public:
  enum { TYPE_HHMMSSFF, TYPE_FRAME, TYPE_HHMMSSFF2 };

  Iwa_TimeCodeFx();

  bool isZerary() const override { return true; }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;
};

#endif