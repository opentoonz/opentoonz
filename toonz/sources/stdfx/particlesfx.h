#pragma once

#ifndef PARTICLESFX_H
#define PARTICLESFX_H

#include "stdfx.h"
#include "tfxparam.h"
#include "tspectrumparam.h"

//******************************************************************
//    Particles Fx  class
//******************************************************************

class ParticlesFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(ParticlesFx)

  TFxPortDG m_source, m_control;

public:
  TIntParamP source_ctrl_val;
  TIntParamP bright_thres_val;
  TBoolParamP multi_source_val;
  TPointParamP center_val;
  TDoubleParamP length_val;
  TDoubleParamP height_val;
  TDoubleParamP maxnum_val;
  TRangeParamP lifetime_val;
  TIntParamP lifetime_ctrl_val;
  TBoolParamP column_lifetime_val;
  TIntParamP startpos_val;
  TIntParamP randseed_val;
  TDoubleParamP gravity_val;
  TDoubleParamP g_angle_val;
  TIntParamP gravity_ctrl_val;
  //  TIntParamP        gravity_radius_val;
  TDoubleParamP friction_val;
  TIntParamP friction_ctrl_val;
  TDoubleParamP windint_val;
  TDoubleParamP windangle_val;
  TIntEnumParamP swingmode_val;
  TRangeParamP randomx_val;
  TRangeParamP randomy_val;
  TIntParamP randomx_ctrl_val;
  TIntParamP randomy_ctrl_val;
  TRangeParamP swing_val;
  TRangeParamP speed_val;
  TIntParamP speed_ctrl_val;
  TRangeParamP speeda_val;
  TIntParamP speeda_ctrl_val;
  TBoolParamP speeda_use_gradient_val;
  TBoolParamP speedscale_val;
  TIntEnumParamP toplayer_val;
  TRangeParamP mass_val;
  TRangeParamP scale_val;
  TIntParamP scale_ctrl_val;
  TBoolParamP scale_ctrl_all_val;
  TRangeParamP rot_val;
  TIntParamP rot_ctrl_val;
  TRangeParamP trail_val;
  TDoubleParamP trailstep_val;
  TIntEnumParamP rotswingmode_val;
  TDoubleParamP rotspeed_val;
  TRangeParamP rotsca_val;
  TRangeParamP rotswing_val;
  TBoolParamP pathaim_val;
  TRangeParamP opacity_val;
  TIntParamP opacity_ctrl_val;
  TRangeParamP trailopacity_val;
  TRangeParamP scalestep_val;
  TIntParamP scalestep_ctrl_val;
  TDoubleParamP fadein_val;
  TDoubleParamP fadeout_val;
  TIntEnumParamP animation_val;
  TIntParamP step_val;
  TSpectrumParamP gencol_val;
  TIntParamP gencol_ctrl_val;
  TDoubleParamP gencol_spread_val;
  TDoubleParamP genfadecol_val;
  TSpectrumParamP fincol_val;
  TIntParamP fincol_ctrl_val;
  TDoubleParamP fincol_spread_val;
  TDoubleParamP finrangecol_val;
  TDoubleParamP finfadecol_val;
  TSpectrumParamP foutcol_val;
  TIntParamP foutcol_ctrl_val;
  TDoubleParamP foutcol_spread_val;
  TDoubleParamP foutrangecol_val;
  TDoubleParamP foutfadecol_val;

  TBoolParamP source_gradation_val;
  TBoolParamP pick_color_for_every_frame_val;
  TBoolParamP perspective_distribution_val;

public:
  enum { UNIT_SMALL_INCH, UNIT_INCH };
  enum { MATTE_REF, GRAY_REF, H_REF };
  enum { SWING_RANDOM, SWING_SMOOTH };
  enum {
    ANIM_HOLD,
    ANIM_RANDOM,
    ANIM_CYCLE,
    ANIM_R_CYCLE,
    ANIM_S_CYCLE,
    ANIM_SR_CYCLE
  };
  enum { TOP_YOUNGER, TOP_OLDER, TOP_SMALLER, TOP_BIGGER, TOP_RANDOM };

public:
  ParticlesFx();
  ~ParticlesFx();

  bool isZerary() const override { return true; }

  int dynamicPortGroupsCount() const override { return 2; }
  const TFxPortDG *dynamicPortGroup(int g) const override {
    return (g == 0) ? &m_source : (g == 1) ? &m_control : 0;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool allowUserCacheOnPort(int port) override;

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  TFxTimeRegion getTimeRegion() const override {
    return TFxTimeRegion::createUnlimited();
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  void compatibilityTranslatePort(int majorVersion, int minorVersion,
                                  std::string &portName) override;
};

#endif  // PARTICLESFX_H
