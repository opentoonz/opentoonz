

// TnzCore includes
#include "tofflinegl.h"
#include "tstroke.h"
#include "tpalette.h"
#include "tvectorimage.h"
#include "tvectorrenderdata.h"
#include "tflash.h"
#include "texception.h"
#include "trasterimage.h"
#include "drawutil.h"

// TnzBase includes
#include "trasterfx.h"
#include "tparamuiconcept.h"

// TnzLib includes
#include "toonz/toonzimageutils.h"

// TnzStdfx includes
#include "particlesengine.h"
#include "particlesmanager.h"

#include "particlesfx.h"

//**************************************************************************
//    Particles Fx  implementation
//**************************************************************************

ParticlesFx::ParticlesFx()
    : m_source("Texture")
    , m_control("Control")
    , source_ctrl_val(0)
    , bright_thres_val(25)
    , multi_source_val(false)
    , center_val(TPointD(0.0, 0.0))
    , length_val(5.0)
    , height_val(4.0)
    , maxnum_val(10.0)
    , lifetime_val(DoublePair(100., 100.))
    , lifetime_ctrl_val(0)
    , column_lifetime_val(false)
    , startpos_val(1)
    , randseed_val(1)
    , gravity_val(0.0)
    , g_angle_val(0.0)
    , gravity_ctrl_val(0)
    //, gravity_radius_val (4)
    , friction_val(0.0)
    , friction_ctrl_val(0)
    , windint_val(0.0)
    , windangle_val(0.0)
    , swingmode_val(new TIntEnumParam(SWING_RANDOM, "Random"))
    , randomx_val(DoublePair(0., 0.))
    , randomy_val(DoublePair(0., 0.))
    , randomx_ctrl_val(0)
    , randomy_ctrl_val(0)
    , swing_val(DoublePair(0., 0.))
    , speed_val(DoublePair(0., 10.))
    , speed_ctrl_val(0)
    , speeda_val(DoublePair(0., 0.))
    , speeda_ctrl_val(0)
    , speeda_use_gradient_val(false)
    , speedscale_val(false)
    , toplayer_val(new TIntEnumParam(TOP_YOUNGER, "Younger"))
    , mass_val(DoublePair(1., 1.))
    , scale_val(DoublePair(100., 100.))
    , scale_ctrl_val(0)
    , scale_ctrl_all_val(false)
    , rot_val(DoublePair(0., 0.))
    , rot_ctrl_val(0)
    , trail_val(DoublePair(0., 0.))
    , trailstep_val(0.0)
    , rotswingmode_val(new TIntEnumParam(SWING_RANDOM, "Random"))
    , rotspeed_val(0.0)
    , rotsca_val(DoublePair(0., 0.))
    , rotswing_val(DoublePair(0., 0.))
    , pathaim_val(false)
    , opacity_val(DoublePair(0., 100.))
    , opacity_ctrl_val(0)
    , trailopacity_val(DoublePair(0., 100.))
    , scalestep_val(DoublePair(0., 0.))
    , scalestep_ctrl_val(0)
    , fadein_val(0.0)
    , fadeout_val(0.0)
    , animation_val(new TIntEnumParam(ANIM_HOLD, "Hold Frame"))
    , step_val(1)
    , gencol_ctrl_val(0)
    , gencol_spread_val(0.0)
    , genfadecol_val(0.0)
    , fincol_ctrl_val(0)
    , fincol_spread_val(0.0)
    , finrangecol_val(0.0)
    , finfadecol_val(0.0)
    , foutcol_ctrl_val(0)
    , foutcol_spread_val(0.0)
    , foutrangecol_val(0.0)
    , foutfadecol_val(0.0)
    , source_gradation_val(false)
    , pick_color_for_every_frame_val(false)
    , perspective_distribution_val(false) {
  addInputPort("Texture1", new TRasterFxPort, 0);
  addInputPort("Control1", new TRasterFxPort, 1);

  length_val->setMeasureName("fxLength");
  height_val->setMeasureName("fxLength");
  center_val->getX()->setMeasureName("fxLength");
  center_val->getY()->setMeasureName("fxLength");

  bindParam(this, "source_ctrl", source_ctrl_val);
  bindParam(this, "bright_thres", bright_thres_val);
  bright_thres_val->setValueRange(0, 255);
  bindParam(this, "multi_source", multi_source_val);
  bindParam(this, "center", center_val);
  bindParam(this, "length", length_val);
  length_val->setValueRange(1.0, (std::numeric_limits<double>::max)());
  bindParam(this, "height", height_val);
  height_val->setValueRange(1.0, (std::numeric_limits<double>::max)());
  bindParam(this, "birth_rate", maxnum_val);
  maxnum_val->setValueRange(0.0, (std::numeric_limits<double>::max)());
  bindParam(this, "lifetime", lifetime_val);
  lifetime_val->getMin()->setValueRange(0., +3000.);
  lifetime_val->getMax()->setValueRange(0., +3000.);
  bindParam(this, "lifetime_ctrl", lifetime_ctrl_val);
  bindParam(this, "column_lifetime", column_lifetime_val);
  bindParam(this, "starting_frame", startpos_val);
  bindParam(this, "random_seed", randseed_val);
  bindParam(this, "gravity", gravity_val);
  gravity_val->setValueRange(0.0, (std::numeric_limits<double>::max)());
  bindParam(this, "gravity_angle", g_angle_val);
  g_angle_val->setMeasureName("angle");
  bindParam(this, "gravity_ctrl", gravity_ctrl_val);
  //  bindParam(this,"gravity_radius", gravity_radius_val);
  //  gravity_radius_val->setValueRange(0,40);
  bindParam(this, "friction", friction_val);
  bindParam(this, "friction_ctrl", friction_ctrl_val);
  bindParam(this, "wind", windint_val);
  bindParam(this, "wind_angle", windangle_val);
  windangle_val->setMeasureName("angle");
  bindParam(this, "swing_mode", swingmode_val);
  swingmode_val->addItem(SWING_SMOOTH, "Smooth");
  bindParam(this, "scattering_x", randomx_val);
  randomx_val->getMin()->setMeasureName("fxLength");
  randomx_val->getMax()->setMeasureName("fxLength");
  randomx_val->getMin()->setValueRange(-1000., +1000.);
  randomx_val->getMax()->setValueRange(-1000., +1000.);
  bindParam(this, "scattering_y", randomy_val);
  randomy_val->getMin()->setMeasureName("fxLength");
  randomy_val->getMax()->setMeasureName("fxLength");
  randomy_val->getMin()->setValueRange(-1000., +1000.);
  randomy_val->getMax()->setValueRange(-1000., +1000.);
  bindParam(this, "scattering_x_ctrl", randomx_ctrl_val);
  bindParam(this, "scattering_y_ctrl", randomy_ctrl_val);
  bindParam(this, "swing", swing_val);
  swing_val->getMin()->setValueRange(-1000., +1000.);
  swing_val->getMax()->setValueRange(-1000., +1000.);
  speed_val->getMin()->setMeasureName("fxLength");
  speed_val->getMax()->setMeasureName("fxLength");
  bindParam(this, "speed", speed_val);
  speed_val->getMin()->setValueRange(-1000., +1000.);
  speed_val->getMax()->setValueRange(-1000., +1000.);
  bindParam(this, "speed_ctrl", speed_ctrl_val);
  bindParam(this, "speed_angle", speeda_val);
  speeda_val->getMin()->setValueRange(-1000., +1000.);
  speeda_val->getMax()->setValueRange(-1000., +1000.);
  speeda_val->getMin()->setMeasureName("angle");
  speeda_val->getMax()->setMeasureName("angle");
  bindParam(this, "speeda_ctrl", speeda_ctrl_val);
  bindParam(this, "speeda_use_gradient", speeda_use_gradient_val);
  bindParam(this, "speed_size", speedscale_val);
  bindParam(this, "top_layer", toplayer_val);
  toplayer_val->addItem(TOP_OLDER, "Older");
  toplayer_val->addItem(TOP_SMALLER, "Smaller");
  toplayer_val->addItem(TOP_BIGGER, "Bigger");
  toplayer_val->addItem(TOP_RANDOM, "Random");
  bindParam(this, "mass", mass_val);
  mass_val->getMin()->setValueRange(0., +1000.);
  mass_val->getMax()->setValueRange(0., +1000.);
  bindParam(this, "scale", scale_val);
  scale_val->getMin()->setValueRange(0., +1000.);
  scale_val->getMax()->setValueRange(0., +1000.);
  bindParam(this, "scale_ctrl", scale_ctrl_val);
  bindParam(this, "scale_ctrl_all", scale_ctrl_all_val);
  bindParam(this, "rot", rot_val);
  rot_val->getMin()->setValueRange(-1000., +1000.);
  rot_val->getMax()->setValueRange(-1000., +1000.);
  rot_val->getMin()->setMeasureName("angle");
  rot_val->getMax()->setMeasureName("angle");
  bindParam(this, "rot_ctrl", rot_ctrl_val);
  bindParam(this, "trail", trail_val);
  trail_val->getMin()->setValueRange(0., +1000.);
  trail_val->getMax()->setValueRange(0., +1000.);
  bindParam(this, "trail_step", trailstep_val);
  trailstep_val->setValueRange(1.0, (std::numeric_limits<double>::max)());
  bindParam(this, "spin_swing_mode", rotswingmode_val);
  rotswingmode_val->addItem(SWING_SMOOTH, "Smooth");
  bindParam(this, "spin_speed", rotspeed_val);
  rotspeed_val->setMeasureName("angle");
  bindParam(this, "spin_random", rotsca_val);
  rotsca_val->getMin()->setValueRange(-1000., +1000.);
  rotsca_val->getMax()->setValueRange(-1000., +1000.);
  rotsca_val->getMin()->setMeasureName("angle");
  rotsca_val->getMax()->setMeasureName("angle");
  bindParam(this, "spin_swing", rotswing_val);
  rotswing_val->getMin()->setValueRange(-1000., +1000.);
  rotswing_val->getMax()->setValueRange(-1000., +1000.);
  rotswing_val->getMin()->setMeasureName("angle");
  rotswing_val->getMax()->setMeasureName("angle");
  bindParam(this, "path_aim", pathaim_val);
  bindParam(this, "opacity", opacity_val);
  opacity_val->getMin()->setValueRange(0., +100.);
  opacity_val->getMax()->setValueRange(0., +100.);
  bindParam(this, "opacity_ctrl", opacity_ctrl_val);
  bindParam(this, "trail_opacity", trailopacity_val);
  trailopacity_val->getMin()->setValueRange(0., +100.);
  trailopacity_val->getMax()->setValueRange(0., +100.);
  bindParam(this, "scale_step", scalestep_val);
  bindParam(this, "scale_step_ctrl", scalestep_ctrl_val);
  scalestep_val->getMin()->setValueRange(-100., +100.);
  scalestep_val->getMax()->setValueRange(-100., +100.);
  bindParam(this, "fade_in", fadein_val);
  bindParam(this, "fade_out", fadeout_val);
  bindParam(this, "animation", animation_val);
  animation_val->addItem(ANIM_RANDOM, "Random Frame");
  animation_val->addItem(ANIM_CYCLE, "Column");
  animation_val->addItem(ANIM_R_CYCLE, "Column - Random Start");
  animation_val->addItem(ANIM_SR_CYCLE, "Column Swing - Random Start");
  bindParam(this, "step", step_val);
  step_val->setValueRange(1, (std::numeric_limits<int>::max)());
  TSpectrum::ColorKey colors[] = {TSpectrum::ColorKey(0, TPixel32::Red),
                                  TSpectrum::ColorKey(1, TPixel32::Red)};
  gencol_val = TSpectrumParamP(tArrayCount(colors), colors);
  bindParam(this, "birth_color", gencol_val);
  bindParam(this, "birth_color_ctrl", gencol_ctrl_val);
  bindParam(this, "birth_color_spread", gencol_spread_val);
  gencol_spread_val->setValueRange(0.0, (std::numeric_limits<int>::max)());
  bindParam(this, "birth_color_fade", genfadecol_val);
  genfadecol_val->setValueRange(0.0, 100.0);
  TSpectrum::ColorKey colors1[] = {TSpectrum::ColorKey(0, TPixel32::Green),
                                   TSpectrum::ColorKey(1, TPixel32::Green)};
  fincol_val = TSpectrumParamP(tArrayCount(colors1), colors1);
  bindParam(this, "fadein_color", fincol_val);
  bindParam(this, "fadein_color_ctrl", fincol_ctrl_val);
  bindParam(this, "fadein_color_spread", fincol_spread_val);
  fincol_spread_val->setValueRange(0.0, (std::numeric_limits<int>::max)());
  bindParam(this, "fadein_color_range", finrangecol_val);
  finrangecol_val->setValueRange(0.0, (std::numeric_limits<double>::max)());
  bindParam(this, "fadein_color_fade", finfadecol_val);
  finfadecol_val->setValueRange(0.0, 100.0);
  TSpectrum::ColorKey colors2[] = {TSpectrum::ColorKey(0, TPixel32::Blue),
                                   TSpectrum::ColorKey(1, TPixel32::Blue)};
  foutcol_val = TSpectrumParamP(tArrayCount(colors2), colors2);
  bindParam(this, "fadeout_color", foutcol_val);
  bindParam(this, "fadeout_color_ctrl", foutcol_ctrl_val);
  bindParam(this, "fadeout_color_spread", foutcol_spread_val);
  foutcol_spread_val->setValueRange(0.0, (std::numeric_limits<int>::max)());
  bindParam(this, "fadeout_color_range", foutrangecol_val);
  foutrangecol_val->setValueRange(0.0, (std::numeric_limits<double>::max)());
  bindParam(this, "fadeout_color_fade", foutfadecol_val);
  foutfadecol_val->setValueRange(0.0, 100.0);
  bindParam(this, "source_gradation", source_gradation_val);
  bindParam(this, "pick_color_for_every_frame", pick_color_for_every_frame_val);
  bindParam(this, "perspective_distribution", perspective_distribution_val);
}

//------------------------------------------------------------------

ParticlesFx::~ParticlesFx() {}

//------------------------------------------------------------------

void ParticlesFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 2];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Center";
  concepts[0].m_params.push_back(center_val);

  concepts[1].m_type = TParamUIConcept::RECT;
  concepts[1].m_params.push_back(length_val);
  concepts[1].m_params.push_back(height_val);
  concepts[1].m_params.push_back(center_val);
}

//------------------------------------------------------------------

bool ParticlesFx::doGetBBox(double frame, TRectD &bBox,
                            const TRenderSettings &info) {
  // Returning an infinite rect. This is necessary since building the actual
  // bbox
  // is a very complicate task.

  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------------

std::string ParticlesFx::getAlias(double frame,
                                  const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)
  for (int i = 0; i < getInputPortCount(); ++i) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  std::string paramalias("");
  for (int i = 0; i < getParams()->getParamCount(); ++i) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }

  return alias + std::to_string(frame) + "," + std::to_string(getIdentifier()) +
         paramalias + "]";
}

//------------------------------------------------------------------

bool ParticlesFx::allowUserCacheOnPort(int portNum) {
  // Only control port are currently allowed to cache upon explicit user's
  // request
  std::string tmpName = getInputPortName(portNum);
  return tmpName.find("Control") != std::string::npos;
}

//------------------------------------------------------------------

void ParticlesFx::doDryCompute(TRectD &rect, double frame,
                               const TRenderSettings &info) {
  ParticlesManager *pc = ParticlesManager::instance();
  unsigned long fxId   = getIdentifier();
  int inputPortCount   = getInputPortCount();

  int i, j, curr_frame = frame, startframe = startpos_val->getValue();

  TRenderSettings infoOnInput(info);
  infoOnInput.m_affine =
      TAffine();  // Using the standard reference - indep. from cameras.
  infoOnInput.m_bpp =
      32;  // Control ports rendered at 32 bit - since not visible.

  for (i = startframe - 1; i <= curr_frame; ++i) {
    double frame = std::max(0, i);

    for (j = 0; j < inputPortCount; ++j) {
      TFxPort *port       = getInputPort(j);
      std::string tmpName = getInputPortName(j);
      if (port->isConnected()) {
        TRasterFxP fx = port->getFx();

        // Now, consider that source ports work different than control ones
        QString portName = QString::fromStdString(tmpName);
        if (portName.startsWith("C")) {
          // Control ports are calculated from start to current frame, since
          // particle mechanics at current frame is influenced by previous ones
          // (and therefore by all previous control images).

          TRectD bbox;
          fx->getBBox(frame, bbox, infoOnInput);
          if (bbox == TConsts::infiniteRectD) bbox = info.m_affine.inv() * rect;
          fx->dryCompute(bbox, frame, infoOnInput);
        } else if (portName.startsWith("T")) {
          // Particles handle source ports caching procedures on its own.
        }
      }
    }
  }
}

//------------------------------------------------------------------

void ParticlesFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  std::vector<int> lastframe;
  std::vector<TLevelP> partLevel;

  TPointD p_offset;
  TDimension p_size(0, 0);

  /*-- 参照画像ポートの取得 --*/
  std::vector<TRasterFxPort *> part_ports; /*- テクスチャ素材画像のポート -*/
  std::map<int, TRasterFxPort *>
      ctrl_ports; /*- コントロール画像のポート番号／ポート -*/
  int portsCount = this->getInputPortCount();

  for (int i = 0; i < portsCount; ++i) {
    std::string tmpName = this->getInputPortName(i);
    QString portName    = QString::fromStdString(tmpName);

    if (portName.startsWith("T")) {
      TRasterFxPort *tmpPart = (TRasterFxPort *)this->getInputPort(tmpName);
      if (tmpPart->isConnected())
        part_ports.push_back((TRasterFxPort *)this->getInputPort(tmpName));
    } else {
      portName.replace(QString("Control"), QString(""));
      TRasterFxPort *tmpCtrl = (TRasterFxPort *)this->getInputPort(tmpName);
      if (tmpCtrl->isConnected())
        ctrl_ports[portName.toInt()] =
            (TRasterFxPort *)this->getInputPort(tmpName);
    }
  }
  /*-- テクスチャ素材のバウンディングボックスを足し合わせる --*/
  if (!part_ports.empty()) {
    TRectD outTileBBox(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                               tile.getRaster()->getLy()));
    TRectD bbox;

    for (unsigned int i = 0; i < (int)part_ports.size(); ++i) {
      const TFxTimeRegion &tr = (*part_ports[i])->getTimeRegion();

      lastframe.push_back(tr.getLastFrame() + 1);
      partLevel.push_back(new TLevel());
      partLevel[i]->setName((*part_ports[i])->getAlias(0, ri));

      // The particles offset must be calculated without considering the
      // affine's translational
      // component
      TRenderSettings riZero(ri);
      riZero.m_affine.a13 = riZero.m_affine.a23 = 0;

      // Calculate the bboxes union
      for (int t = 0; t <= tr.getLastFrame(); ++t) {
        TRectD inputBox;
        (*part_ports[i])->getBBox(t, inputBox, riZero);
        bbox += inputBox;
      }
    }

    if (bbox == TConsts::infiniteRectD) bbox *= outTileBBox;

    p_size.lx = (int)bbox.getLx() + 1;
    p_size.ly = (int)bbox.getLy() + 1;
    p_offset  = TPointD(0.5 * (bbox.x0 + bbox.x1), 0.5 * (bbox.y0 + bbox.y1));
  }
  /*- テクスチャ素材が無い場合、丸を描く -*/
  else {
    partLevel.push_back(new TLevel());
    partLevel[0]->setName("particles");
    TDimension vecsize(10, 10);
    TOfflineGL *offlineGlContext = new TOfflineGL(vecsize);
    offlineGlContext->makeCurrent();
    offlineGlContext->clear(TPixel32(0, 0, 0, 0));

    TStroke *stroke;
    stroke = makeEllipticStroke(
        0.07, TPointD((vecsize.lx - 1) * .5, (vecsize.ly - 1) * .5), 2.0, 2.0);
    TVectorImageP vectmp = new TVectorImage();

    TPalette *plt = new TPalette();
    vectmp->setPalette(plt);
    vectmp->addStroke(stroke);
    TVectorRenderData rd(AffI, TRect(vecsize), plt, 0, true, true);

    offlineGlContext->draw(vectmp, rd);

    partLevel[0]->setFrame(
        0, TRasterImageP(offlineGlContext->getRaster()->clone()));
    p_size.lx = vecsize.lx + 1;
    p_size.ly = vecsize.ly + 1;
    lastframe.push_back(1);

    delete offlineGlContext;
  }

  Particles_Engine myEngine(this, frame);

  // Retrieving the dpi multiplier from the accumulated affine (which is
  // isotropic). That is,
  // the affine will be applied *before* this effect - and we'll multiply
  // geometrical parameters
  // by this dpi mult. in order to compensate.
  float dpi = sqrt(fabs(ri.m_affine.det())) * 100;

  TTile tileIn;
  if (TRaster32P raster32 = tile.getRaster()) {
    TFlash *flash = 0;
    myEngine.render_particles(flash, &tile, part_ports, ri, p_size, p_offset,
                              ctrl_ports, partLevel, 1, (int)frame, 1, 0, 0, 0,
                              0, lastframe, getIdentifier());
  } else if (TRaster64P raster64 = tile.getRaster()) {
    TFlash *flash = 0;
    myEngine.render_particles(flash, &tile, part_ports, ri, p_size, p_offset,
                              ctrl_ports, partLevel, 1, (int)frame, 1, 0, 0, 0,
                              0, lastframe, getIdentifier());
  } else
    throw TException("ParticlesFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

void ParticlesFx::compute(TFlash &flash, int frame) {
  // Particles is currently disabled in Flash...
  return;
}

//------------------------------------------------------------------

void ParticlesFx::compatibilityTranslatePort(int major, int minor,
                                             std::string &portName) {
  VersionNumber version(major, minor);

  if (version < VersionNumber(1, 16)) {
    if (portName == "Texture") portName = "Texture1";
  } else if (version < VersionNumber(1, 20)) {
    int idx;

    bool chop =
        ((idx = portName.find("Texture")) != std::string::npos && idx > 0) ||
        ((idx = portName.find("Control")) != std::string::npos && idx > 0);

    if (chop) portName.erase(portName.begin(), portName.begin() + idx);
  }
}

//==============================================================================

FX_PLUGIN_IDENTIFIER(ParticlesFx, "particlesFx");
