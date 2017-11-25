#pragma once

// TODO:
// エフェクトの縮小表示時のアイコンのファイル名。要プラグインFxへの対応。ここに集めるのではなく、Fxに自己申告させるか、アイコン自体を無くす。　2016/1/12
// shun_iwasawa

#ifndef FXICONMANAGER_H
#define FXICONMANAGER_H

#include "tcommon.h"

class QPixmap;

namespace {

const struct {
  const char *fxType;
  const char *pixmapFilename;
} fxTypeInfo[] = {

    {"checkBoardFx", "fx_checkboard"},
    {"colorCardFx", "fx_colorcard"},
    {"STD_tileFx", "fx_tile"},
    {"STD_blurFx", "fx_blur"},
    {"STD_directionalBlurFx", "fx_directionalblur"},
    {"STD_inoBlurFx", "fx_ino_blur"},
    {"STD_inoMotionBlurFx", "fx_ino_motionblur"},
    {"STD_inoRadialBlurFx", "fx_ino_radialblur"},
    {"STD_inoSpinBlurFx", "fx_ino_spinblur"},
    {"STD_localBlurFx", "fx_localblur"},
    {"STD_motionBlurFx", "fx_motionblur"},
    {"STD_radialBlurFx", "fx_radialblur"},
    {"STD_rotationalBlurFx", "fx_spinblur"},
    {"STD_freeDistortFx", "fx_freedistort"},
    {"STD_inoWarphvFx", "fx_ino_warphv"},
    {"STD_linearWaveFx", "fx_linearwave"},
    {"STD_perlinNoiseFx", "fx_perlinnoise"},
    {"STD_randomWaveFx", "fx_randomwave"},
    {"STD_rippleFx", "fx_ripple"},
    {"STD_warpFx", "fx_warp"},
    {"STD_diamondGradientFx", "fx_diamondgradient"},
    {"STD_fourPointsGradientFx", "fx_fourpoints"},
    {"STD_linearGradientFx", "fx_lineargradient"},
    {"STD_multiLinearGradientFx", "fx_multilineargradient"},
    {"STD_multiRadialGradientFx", "fx_multiradialgradient"},
    {"STD_radialGradientFx", "fx_radialgradient"},
    {"STD_spiralFx", "fx_spiral"},
    {"STD_squareGradientFx", "fx_squaregradient"},
    {"STD_toneCurveFx", "fx_tonecurve"},
    {"STD_inoChannelSelectorFx", "fx_ino_channelselector"},
    {"STD_inoDensityFx", "fx_ino_density"},
    {"STD_inohlsAddFx", "fx_ino_hlsadd"},
    {"STD_inohlsAdjustFx", "fx_ino_hlsadjust"},
    {"STD_inohsvAddFx", "fx_ino_hsvadd"},
    {"STD_inohsvAdjustFx", "fx_ino_hsvadjust"},
    {"STD_inoLevelAutoFx", "fx_ino_levelauto"},
    {"STD_inoLevelAutoInCameraFx", "fx_ino_levelautoincamera"},
    {"STD_inoLevelMasterFx", "fx_ino_levelmaster"},
    {"STD_inoLevelrgbaFx", "fx_ino_levelrgba"},
    {"STD_inoNegateFx", "fx_ino_negate"},
    {"STD_localTransparencyFx", "fx_localtransparency"},
    {"STD_multiToneFx", "fx_multitone"},
    {"STD_premultiplyFx", "fx_premultiply"},
    {"STD_rgbmCutFx", "fx_rgbmcut"},
    {"STD_rgbmFadeFx", "fx_rgbmfade"},
    {"STD_rgbmScaleFx", "fx_rgbmscale"},
    {"STD_sharpenFx", "fx_sharpen"},
    {"STD_fadeFx", "fx_transparency"},
    {"STD_inoOverFx", "fx_ino_over"},
    {"STD_inoCrossDissolveFx", "fx_ino_crossdissolve"},
    {"STD_inoDarkenFx", "fx_ino_darken"},
    {"STD_inoMultiplyFx", "fx_ino_multiply"},
    {"STD_inoColorBurnFx", "fx_ino_colorburn"},
    {"STD_inoLinearBurnFx", "fx_ino_linearburn"},
    {"STD_inoDarkerColorFx", "fx_ino_darkercolor"},
    {"STD_inoAddFx", "fx_ino_add"},
    {"STD_inoLightenFx", "fx_ino_lighten"},
    {"STD_inoScreenFx", "fx_ino_screen"},
    {"STD_inoColorDodgeFx", "fx_ino_colordodge"},
    {"STD_inoLinearDodgeFx", "fx_ino_lineardodge"},
    {"STD_inoLighterColorFx", "fx_ino_lightercolor"},
    {"STD_inoOverlayFx", "fx_ino_overlay"},
    {"STD_inoSoftLightFx", "fx_ino_softlight"},
    {"STD_inoHardLightFx", "fx_ino_hardlight"},
    {"STD_inoVividLightFx", "fx_ino_vividlight"},
    {"STD_inoLinearLightFx", "fx_ino_linearlight"},
    {"STD_inoPinLightFx", "fx_ino_pinlight"},
    {"STD_inoHardMixFx", "fx_ino_hardmix"},
    {"STD_inoDivideFx", "fx_ino_divide"},
    {"STD_inoSubtractFx", "fx_ino_subtract"},
    {"STD_backlitFx", "fx_backlit"},
    {"STD_bodyHighLightFx", "fx_bodyhighlight"},
    {"STD_castShadowFx", "fx_castshadow"},
    {"STD_glowFx", "fx_glow"},
    {"STD_inoFogFx", "fx_ino_fog"},
    {"STD_lightSpotFx", "fx_lightspot"},
    {"STD_raylitFx", "fx_raylit"},
    {"STD_targetSpotFx", "fx_targetspot"},
    {"STD_hsvKeyFx", "fx_hsvkey"},
    {"inFx", "fx_in"},
    {"outFx", "fx_out"},
    {"STD_rgbKeyFx", "fx_rgbkey"},
    {"atopFx", "fx_visiblemattein"},
    {"STD_dissolveFx", "fx_dissolve"},
    {"STD_inohlsNoiseFx", "fx_ino_hlsnoise"},
    {"STD_inohlsNoiseInCameraFx", "fx_ino_hlsnoiseincamera"},
    {"STD_inohsvNoiseFx", "fx_ino_hsvnoise"},
    {"STD_inohsvNoiseInCameraFx", "fx_ino_hsvnoiseincamera"},
    {"STD_inoMedianFilterFx", "fx_ino_medianfilter"},
    {"STD_noiseFx", "fx_noise"},
    {"STD_saltpepperNoiseFx", "fx_saltpeppernoise"},
    {"STD_cloudsFx", "fx_clouds"},
    {"STD_inopnCloudsFx", "fx_ino_pnclouds"},
    {"STD_particlesFx", "fx_particles"},
    {"STD_colorEmbossFx", "fx_coloremboss"},
    {"STD_embossFx", "fx_emboss"},
    {"STD_inoMaxMinFx", "fx_ino_maxmin"},
    {"STD_inoMotionWindFx", "fx_ino_motionwind"},
    {"STD_posterizeFx", "fx_posterize"},
    {"STD_solarizeFx", "fx_solarize"},
    {"STD_artContourFx", "fx_artcontour"},
    {"STD_calligraphicFx", "fx_calligraphic"},
    {"STD_blendTzFx", "fx_blendtz"},
    {"STD_externalPaletteFx", "fx_externalpalette"},
    {"STD_outBorderFx", "fx_outline"},
    {"STD_paletteFilterFx", "fx_palettefilter"},
    {"STD_cornerPinFx", "fx_pinnedtexture"},
    {"STD_textureFx", "fx_texture"},
    {"STD_iwa_TileFx", "fx_iwa_tile"},
    {"STD_iwa_MotionBlurFx", "fx_iwa_motionblur"},
    {"STD_iwa_SpectrumFx", "fx_iwa_spectrum"},
    {"STD_iwa_PerspectiveDistortFx", "fx_iwa_perspective_distort"},
    {"STD_iwa_BokehFx", "fx_iwa_bokeh"},
    {"STD_iwa_BokehRefFx", "fx_iwa_bokeh_ref"},
    {"STD_iwa_SoapBubbleFx", "fx_iwa_soapbubble"},
    {0, 0}};
};

class FxIconPixmapManager {  // singleton

  std::map<std::string, QPixmap> m_pms;

  FxIconPixmapManager();

public:
  static FxIconPixmapManager *instance();

  const QPixmap &getFxIconPm(std::string type);
};

#endif