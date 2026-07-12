

//------------------------------------------------------------------
// Iwa_Particles_Engine for Marnie
// based on Particles_Engine by Digital Video
//------------------------------------------------------------------
#include "trop.h"
#include "tfxparam.h"
#include "tofflinegl.h"
#include "tstopwatch.h"
#include "tsystem.h"
#include "timagecache.h"
#include "tconvert.h"

#include "trasterimage.h"

#include "timage_io.h"

#include "tcolorfunctions.h"
#include "toonz/tcolumnfx.h"

#include "iwa_particlesmanager.h"

#include "iwa_particlesengine.h"

#include "trenderer.h"

#include <QMutex>
#include <QMutexLocker>

#include <sstream>

namespace {
QMutex mutex;

void printTime(TStopWatch &sw, std::string name) {
  std::stringstream ss;
  ss << name << " : ";
  sw.print(ss);
  ss << '\n' << '\0';
  TSystem::outputDebug(ss.str());
}
};  // namespace

/*-----------------------------------------------------------------*/

Iwa_Particles_Engine::Iwa_Particles_Engine(Iwa_TiledParticlesFx *parent,
                                           double frame)
    : m_parent(parent), m_frame(frame) {}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::fill_value_struct(struct particles_values &myvalues,
                                             double frame) {
  myvalues.source_ctrl_val     = m_parent->source_ctrl_val->getValue();
  myvalues.bright_thres_val    = m_parent->bright_thres_val->getValue();
  myvalues.x_pos_val           = m_parent->center_val->getValue(frame).x;
  myvalues.y_pos_val           = m_parent->center_val->getValue(frame).y;
  myvalues.length_val          = m_parent->length_val->getValue(frame);
  myvalues.height_val          = m_parent->height_val->getValue(frame);
  myvalues.maxnum_val          = m_parent->maxnum_val->getValue(frame);
  myvalues.lifetime_val        = m_parent->lifetime_val->getValue(frame);
  myvalues.lifetime_ctrl_val   = m_parent->lifetime_ctrl_val->getValue();
  myvalues.column_lifetime_val = m_parent->column_lifetime_val->getValue();
  myvalues.startpos_val        = m_parent->startpos_val->getValue();
  myvalues.randseed_val        = m_parent->randseed_val->getValue();
  myvalues.gravity_val         = m_parent->gravity_val->getValue(frame);
  myvalues.g_angle_val         = m_parent->g_angle_val->getValue(frame);
  myvalues.gravity_ctrl_val    = m_parent->gravity_ctrl_val->getValue();
  myvalues.friction_val        = m_parent->friction_val->getValue(frame);
  myvalues.friction_ctrl_val   = m_parent->friction_ctrl_val->getValue();
  myvalues.windint_val         = m_parent->windint_val->getValue(frame);
  myvalues.windangle_val       = m_parent->windangle_val->getValue(frame);
  myvalues.swingmode_val       = m_parent->swingmode_val->getValue();
  myvalues.randomx_val         = m_parent->randomx_val->getValue(frame);
  myvalues.randomy_val         = m_parent->randomy_val->getValue(frame);
  myvalues.randomx_ctrl_val    = m_parent->randomx_ctrl_val->getValue();
  myvalues.randomy_ctrl_val    = m_parent->randomy_ctrl_val->getValue();
  myvalues.swing_val           = m_parent->swing_val->getValue(frame);
  myvalues.speed_val           = m_parent->speed_val->getValue(frame);
  myvalues.speed_ctrl_val      = m_parent->speed_ctrl_val->getValue();
  myvalues.speeda_val          = m_parent->speeda_val->getValue(frame);
  myvalues.speeda_ctrl_val     = m_parent->speeda_ctrl_val->getValue();
  myvalues.speeda_use_gradient_val =
      m_parent->speeda_use_gradient_val->getValue();
  myvalues.speedscale_val     = m_parent->speedscale_val->getValue();
  myvalues.toplayer_val       = m_parent->toplayer_val->getValue();
  myvalues.mass_val           = m_parent->mass_val->getValue(frame);
  myvalues.scale_val          = m_parent->scale_val->getValue(frame);
  myvalues.scale_ctrl_val     = m_parent->scale_ctrl_val->getValue();
  myvalues.scale_ctrl_all_val = m_parent->scale_ctrl_all_val->getValue();
  myvalues.rot_val            = m_parent->rot_val->getValue(frame);
  myvalues.rot_ctrl_val       = m_parent->rot_ctrl_val->getValue();
  myvalues.trail_val          = m_parent->trail_val->getValue(frame);
  myvalues.trailstep_val      = m_parent->trailstep_val->getValue(frame);
  myvalues.rotswingmode_val   = m_parent->rotswingmode_val->getValue();
  myvalues.rotspeed_val       = m_parent->rotspeed_val->getValue(frame);
  myvalues.rotsca_val         = m_parent->rotsca_val->getValue(frame);
  myvalues.rotswing_val       = m_parent->rotswing_val->getValue(frame);
  myvalues.pathaim_val        = m_parent->pathaim_val->getValue();
  myvalues.opacity_val        = m_parent->opacity_val->getValue(frame);
  myvalues.opacity_ctrl_val   = m_parent->opacity_ctrl_val->getValue();
  myvalues.trailopacity_val   = m_parent->trailopacity_val->getValue(frame);
  myvalues.scalestep_val      = m_parent->scalestep_val->getValue(frame);
  myvalues.scalestep_ctrl_val = m_parent->scalestep_ctrl_val->getValue();
  myvalues.fadein_val         = m_parent->fadein_val->getValue(frame);
  myvalues.fadeout_val        = m_parent->fadeout_val->getValue(frame);
  myvalues.animation_val      = m_parent->animation_val->getValue();
  myvalues.step_val           = m_parent->step_val->getValue();

  const TSpectrum &gencol     = m_parent->gencol_val->getValue(frame);
  myvalues.gencol_ctrl_val    = m_parent->gencol_ctrl_val->getValue();
  myvalues.gencol_spread_val  = m_parent->gencol_spread_val->getValue(frame);
  myvalues.genfadecol_val     = m_parent->genfadecol_val->getValue(frame);
  const TSpectrum &fincol     = m_parent->fincol_val->getValue(frame);
  myvalues.fincol_ctrl_val    = m_parent->fincol_ctrl_val->getValue();
  myvalues.fincol_spread_val  = m_parent->fincol_spread_val->getValue(frame);
  myvalues.finrangecol_val    = m_parent->finrangecol_val->getValue(frame);
  myvalues.finfadecol_val     = m_parent->finfadecol_val->getValue(frame);
  const TSpectrum &foutcol    = m_parent->foutcol_val->getValue(frame);
  myvalues.foutcol_ctrl_val   = m_parent->foutcol_ctrl_val->getValue();
  myvalues.foutcol_spread_val = m_parent->foutcol_spread_val->getValue(frame);
  myvalues.foutrangecol_val   = m_parent->foutrangecol_val->getValue(frame);
  myvalues.foutfadecol_val    = m_parent->foutfadecol_val->getValue(frame);

  myvalues.source_gradation_val = m_parent->source_gradation_val->getValue();
  myvalues.pick_color_for_every_frame_val =
      m_parent->pick_color_for_every_frame_val->getValue();
  /*- Rendering mode (background+particles/particles/background/illuminated
   * particles) -*/
  myvalues.iw_rendermode_val = m_parent->iw_rendermode_val->getValue();
  /*- Image material to be applied to particles -*/
  myvalues.base_ctrl_val = m_parent->base_ctrl_val->getValue();
  /*- Add curl noise-like movement -*/
  myvalues.curl_val        = m_parent->curl_val->getValue(frame);
  myvalues.curl_ctrl_1_val = m_parent->curl_ctrl_1_val->getValue();
  myvalues.curl_ctrl_2_val = m_parent->curl_ctrl_2_val->getValue();
  /*- Particle tiling. When particles are arranged in equilateral triangles,
          specify the side length of the equilateral triangle in inches -*/
  myvalues.iw_triangleSize = m_parent->iw_triangleSize->getValue(frame);
  /*- Fluttering rotation -*/
  myvalues.flap_ctrl_val = m_parent->flap_ctrl_val->getValue();
  myvalues.iw_flap_velocity_val =
      m_parent->iw_flap_velocity_val->getValue(frame);
  myvalues.iw_flap_dir_sensitivity_val =
      m_parent->iw_flap_dir_sensitivity_val->getValue(frame);
  /*- Illuminate fluttering particles. Convert Degree to Radian inside
   * normalize_values() -*/
  myvalues.iw_light_theta_val = m_parent->iw_light_theta_val->getValue(frame);
  myvalues.iw_light_phi_val   = m_parent->iw_light_phi_val->getValue(frame);
  /*- Loading margin -*/
  myvalues.margin_val = m_parent->margin_val->getValue(frame);
  /*- Frame length for gradually applying gravity -*/
  myvalues.iw_gravityBufferFrame_val =
      m_parent->iw_gravityBufferFrame_val->getValue();
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::fill_range_struct(struct particles_values &values,
                                             struct particles_ranges &ranges) {
  ranges.swing_range = values.swing_val.second - values.swing_val.first;
  ranges.rotswing_range =
      values.rotswing_val.second - values.rotswing_val.first;
  ranges.randomx_range = values.randomx_val.second - values.randomx_val.first;
  ranges.randomy_range = values.randomy_val.second - values.randomy_val.first;
  ranges.rotsca_range  = values.rotsca_val.second - values.rotsca_val.first;
  ranges.rot_range     = values.rot_val.second - values.rot_val.first;
  ranges.speed_range   = values.speed_val.second - values.speed_val.first;
  ranges.speeda_range  = values.speeda_val.second - values.speeda_val.first;
  ranges.mass_range    = values.mass_val.second - values.mass_val.first;
  ranges.scale_range   = values.scale_val.second - values.scale_val.first;
  ranges.lifetime_range =
      values.lifetime_val.second - values.lifetime_val.first;
  ranges.scalestep_range =
      values.scalestep_val.second - values.scalestep_val.first;
  ranges.trail_range = (int)(values.trail_val.second - values.trail_val.first);
}

bool Iwa_Particles_Engine::port_is_used(int i,
                                        struct particles_values &values) {
  return values.fincol_ctrl_val == i || values.foutcol_ctrl_val == i ||
         values.friction_ctrl_val == i || values.gencol_ctrl_val == i ||
         values.gravity_ctrl_val == i || values.opacity_ctrl_val == i ||
         values.rot_ctrl_val == i || values.scale_ctrl_val == i ||
         values.scalestep_ctrl_val == i || values.source_ctrl_val == i ||
         values.speed_ctrl_val == i || values.speeda_ctrl_val == i ||
         values.lifetime_ctrl_val == i || values.randomx_ctrl_val == i ||
         values.randomy_ctrl_val == i || values.base_ctrl_val == i ||
         values.curl_ctrl_1_val == i || values.curl_ctrl_2_val == i ||
         values.flap_ctrl_val == i;
}
/*-----------------------------------------------------------------*/

/*- Function to iterate sequentially from Start frame to Current frame.
 * Generation also happens here -*/
void Iwa_Particles_Engine::roll_particles(
    TTile *tile,                      /*- Tile to store the result -*/
    std::map<int, TTile *> porttiles, /*- Port number/tile of control images -*/
    const TRenderSettings
        &ri, /*- Current frame's calculation RenderSettings -*/
    std::list<Iwa_Particle> &myParticles, /*- List of particles -*/
    struct particles_values &values, /*- Parameters for the current frame -*/
    float cx,                        /*- Comes in as 0 -*/
    float cy,                        /*- Comes in as 0 -*/
    int frame, /*- Current frame value (value being iterated in for loop) -*/
    int curr_frame,     /*- Current frame -*/
    int level_n,        /*- Number of texture material images -*/
    bool *random_level, /*- Comes in as false at the beginning of the loop -*/
    float dpi,          /*- Comes in as 1 -*/
    std::vector<int> lastframe, /*- Column length of each texture material -*/
    int &totalparticles, QList<ParticleOrigin> &particleOrigins,
    int genPartNum /*- Actual number of particles to generate -*/
) {
  particles_ranges ranges;
  int i;
  float xgravity, ygravity, windx, windy;

  /*- Separate wind strength/gravity strength into X,Y components -*/
  windx    = values.windint_val * sin(values.windangle_val);
  windy    = values.windint_val * cos(values.windangle_val);
  xgravity = values.gravity_val * sin(values.g_angle_val);
  ygravity = values.gravity_val * cos(values.g_angle_val);

  fill_range_struct(values, ranges);

  std::vector<TPointD> myregions;
  QList<QList<int>> myHistogram;

  std::map<int, TTile *>::iterator it = porttiles.find(values.source_ctrl_val);

  /*- If there's a control attached to the source image -*/
  if (values.source_ctrl_val && (it != porttiles.end()) &&
      it->second->getRaster())
    /*- Vary emission density proportional to alpha value of input image -*/
    fill_single_region(myregions, it->second, values.bright_thres_val,
                       values.source_gradation_val, myHistogram,
                       particleOrigins);

  /*- Once particles are exhausted, stop emitting -*/
  int actualBirthParticles = std::min(genPartNum, particleOrigins.size());
  /*- Index of particles that will depart -*/
  QList<int> leavingPartIndex;
  if (myregions.size() &&
      porttiles.find(values.source_ctrl_val) != porttiles.end()) {
    int partLeft = actualBirthParticles;
    while (partLeft > 0) {
      int PrePartLeft = partLeft;

      int potential_sum = 0;
      QList<int> myWeight;
      /*- Calculate potential magnitude myWeight and total potential for each
       * intensity -*/
      for (int m = 0; m < 256; m++) {
        int pot = myHistogram[m].size() * m;
        myWeight.append(pot);
        potential_sum += pot;
      }
      /*- For each intensity (from darkest to lightest) -*/
      for (int m = 255; m > 0; m--) {
        /*- Calculate allocation (round up) -*/
        int wariate = tceil((float)(myWeight[m]) * (float)(partLeft) /
                            (float)potential_sum);
        /*- Actual number of particles departing from this potential -*/
        int leaveNum = std::min(myHistogram.at(m).size(), wariate);
        /*- Register allocated particles from the beginning -*/
        for (int lp = 0; lp < leaveNum; lp++) {
          /*- Add while reducing from Histogram -*/
          leavingPartIndex.append(myHistogram[m].takeFirst());
          /*- Reduce remaining count -*/
          partLeft--;
          if (partLeft == 0) break;
        }
        if (partLeft == 0) break;
      }

      /*- If no particles can depart, break -*/
      if (partLeft == PrePartLeft) break;
    }
    /*- Actual number of particles that could take off -*/
    actualBirthParticles = leavingPartIndex.size();
  } else /*- If nothing is connected, generate randomly -*/
  {
    for (int i = 0; i < actualBirthParticles; i++) leavingPartIndex.append(i);
  }

  /*- No particles to move nor to be born -*/
  if (myParticles.empty() && actualBirthParticles == 0) {
    std::cout << "no particles generated nor alive. returning function"
              << std::endl;
    return;
  }

  /*- When in background-only rendering mode, just update particleOrigins -*/
  if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_BG) {
    /*- Sort indexes in ascending order -*/
    std::sort(leavingPartIndex.begin(), leavingPartIndex.end());
    /*- Remove from larger indexes -*/
    for (int lp = leavingPartIndex.size() - 1; lp >= 0; lp--)
      particleOrigins.removeAt(leavingPartIndex.at(lp));
    return;
  }

  /*- New particle generation -*/
  /*- When there are only new particles -*/
  if (myParticles.empty() && actualBirthParticles)  // Initial creation
  {
    /*- Loop for the number of new particles to create -*/
    for (i = 0; i < actualBirthParticles; i++) {
      /*- Departing particle -*/
      ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

      /*-  Which texture level to use -*/
      int seed = static_cast<int>(
          static_cast<double>(std::numeric_limits<int>::max()) *
          values.random_val->getFloat());

      /*-  Get lifetime -*/
      int lifetime = 0;
      if (values.column_lifetime_val)
        lifetime = lastframe[po.level];
      else {
        lifetime = (int)(values.lifetime_val.first +
                         ranges.lifetime_range * values.random_val->getFloat());
      }
      /*- Determine if this particle is alive at the frame being rendered, and
       * generate if alive -*/
      if (lifetime > curr_frame - frame) {
        myParticles.push_back(Iwa_Particle(
            lifetime, seed, porttiles, values, ranges, totalparticles, 0,
            (int)po.level, lastframe[po.level], po.pos[0],
            po.pos[1],                /*- Specify coordinates for generation -*/
            po.isUpward,              /*- Add orientation -*/
            (int)po.initSourceFrame,  // Material initial frame
            m_parent));
      }
      totalparticles++;
    }
  }
  /*- Move existing particles and create new ones -*/
  else {
    std::list<Iwa_Particle>::iterator it;
    for (it = myParticles.begin(); it != myParticles.end();) {
      std::list<Iwa_Particle>::iterator current = it;
      ++it;

      Iwa_Particle &part = (*current);
      if (part.scale <= 0.0) continue;
      if (part.lifetime <= 0)        // Note: This is in line with the above
                                     // "lifetime>curr_frame-frame"
        myParticles.erase(current);  // insertion counterpart
      else {
        part.move(porttiles, values, ranges, windx, windy, xgravity, ygravity,
                  dpi, lastframe[part.level]);
      }
    }

    switch (values.toplayer_val) {
    case Iwa_TiledParticlesFx::TOP_YOUNGER:
      for (i = 0; i < actualBirthParticles; i++) {
        /*- Departing particle -*/
        ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

        int seed = static_cast<int>(
            static_cast<double>(std::numeric_limits<int>::max()) *
            values.random_val->getFloat());

        int lifetime = 0;
        if (values.column_lifetime_val)
          lifetime = lastframe[po.level];
        else {
          lifetime =
              (int)(values.lifetime_val.first +
                    ranges.lifetime_range * values.random_val->getFloat());
        }
        if (lifetime > curr_frame - frame) {
          myParticles.push_front(Iwa_Particle(
              lifetime, seed, porttiles, values, ranges, totalparticles, 0,
              (int)po.level, lastframe[po.level], po.pos[0], po.pos[1],
              po.isUpward,
              (int)po.initSourceFrame,  // Material initial frame
              m_parent                  // pointer
              ));
        }
        totalparticles++;
      }
      break;

    case Iwa_TiledParticlesFx::TOP_RANDOM:
      for (i = 0; i < actualBirthParticles; i++) {
        double tmp = values.random_val->getFloat() * myParticles.size();
        std::list<Iwa_Particle>::iterator it = myParticles.begin();
        std::advance(it, static_cast<int>(tmp));
        {
          /*- Departing particle -*/
          ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

          int seed = static_cast<int>(
              static_cast<double>(std::numeric_limits<int>::max()) *
              values.random_val->getFloat());

          int lifetime = 0;

          if (values.column_lifetime_val)
            lifetime = lastframe[po.level];
          else {
            lifetime =
                (int)(values.lifetime_val.first +
                      ranges.lifetime_range * values.random_val->getFloat());
          }
          if (lifetime > curr_frame - frame) {
            myParticles.insert(
                it, Iwa_Particle(
                        lifetime, seed, porttiles, values, ranges,
                        totalparticles, 0, (int)po.level, lastframe[po.level],
                        po.pos[0], po.pos[1], po.isUpward,
                        (int)po.initSourceFrame,  // Material initial frame
                        m_parent                  // pointer
                        ));
          }
          totalparticles++;
        }
      }
      break;

    default:
      for (i = 0; i < actualBirthParticles; i++) {
        /*- Departing particle -*/
        ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

        int seed = static_cast<int>(
            static_cast<double>(std::numeric_limits<int>::max()) *
            values.random_val->getFloat());

        int lifetime = 0;

        if (values.column_lifetime_val)
          lifetime = lastframe[po.level];
        else
          lifetime =
              (int)(values.lifetime_val.first +
                    ranges.lifetime_range * values.random_val->getFloat());
        if (lifetime > curr_frame - frame) {
          myParticles.push_back(Iwa_Particle(
              lifetime, seed, porttiles, values, ranges, totalparticles, 0,
              (int)po.level, lastframe[po.level], po.pos[0], po.pos[1],
              po.isUpward,
              (int)po.initSourceFrame,  // Material initial frame
              m_parent                  // pointer
              ));
        }
        totalparticles++;
      }
      break;
    }
  }

  /*- Delete already generated particleOrigins
          Sort indexes in ascending order -*/
  std::sort(leavingPartIndex.begin(), leavingPartIndex.end());
  /*- Remove from larger indexes -*/
  for (int lp = leavingPartIndex.size() - 1; lp >= 0; lp--)
    particleOrigins.removeAt(leavingPartIndex.at(lp));
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::normalize_values(struct particles_values &values,
                                            const TRenderSettings &ri) {
  double dpicorr = 1;
  TPointD pos(values.x_pos_val, values.y_pos_val);

  (values.x_pos_val)               = pos.x;
  (values.y_pos_val)               = pos.y;
  (values.length_val)              = (values.length_val) * dpicorr;
  (values.height_val)              = (values.height_val) * dpicorr;
  (values.gravity_val)             = (values.gravity_val) * dpicorr * 0.1;
  (values.windint_val)             = (values.windint_val) * dpicorr;
  (values.speed_val.first)         = (values.speed_val.first) * dpicorr;
  (values.speed_val.second)        = (values.speed_val.second) * dpicorr;
  (values.randomx_val.first)       = (values.randomx_val.first) * dpicorr;
  (values.randomx_val.second)      = (values.randomx_val.second) * dpicorr;
  (values.randomy_val.first)       = (values.randomy_val.first) * dpicorr;
  (values.randomy_val.second)      = (values.randomy_val.second) * dpicorr;
  (values.scale_val.first)         = (values.scale_val.first) * 0.01;
  (values.scale_val.second)        = (values.scale_val.second) * 0.01;
  (values.scalestep_val.first)     = (values.scalestep_val.first) * 0.01;
  (values.scalestep_val.second)    = (values.scalestep_val.second) * 0.01;
  (values.opacity_val.first)       = (values.opacity_val.first) * 0.01;
  (values.opacity_val.second)      = (values.opacity_val.second) * 0.01;
  (values.trailopacity_val.first)  = (values.trailopacity_val.first) * 0.01;
  (values.trailopacity_val.second) = (values.trailopacity_val.second) * 0.01;
  (values.mblur_val)               = (values.mblur_val) * 0.01;
  (values.friction_val)            = -(values.friction_val) * 0.01;
  (values.windangle_val)           = (values.windangle_val) * M_PI_180;
  (values.g_angle_val)             = (values.g_angle_val + 180) * M_PI_180;
  (values.speeda_val.first)        = (values.speeda_val.first) * M_PI_180;
  (values.speeda_val.second)       = (values.speeda_val.second) * M_PI_180;
  if (values.step_val < 1) values.step_val = 1;
  values.genfadecol_val  = (values.genfadecol_val) * 0.01;
  values.finfadecol_val  = (values.finfadecol_val) * 0.01;
  values.foutfadecol_val = (values.foutfadecol_val) * 0.01;
  (values.curl_val)      = (values.curl_val) * dpicorr * 0.1;
  /*- Illuminate fluttering particles. Convert Degree to Radian inside
   * normalize_values() -*/
  (values.iw_light_theta_val) = (values.iw_light_theta_val) * M_PI_180;
  (values.iw_light_phi_val)   = (values.iw_light_phi_val) * M_PI_180;
  /*- Loading margin -*/
  (values.margin_val) = (values.margin_val) * dpicorr;
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::render_particles(
    TTile *tile, /*- Tile to store the result -*/
    std::vector<TRasterFxPort *>
        part_ports, /*- Ports of texture material images -*/
    const TRenderSettings &ri,
    TDimension &p_size, /*- Combined bounding box of texture materials -*/
    TPointD &p_offset,  /*- Bottom-left coordinates of bounding box -*/
    std::map<int, TRasterFxPort *>
        ctrl_ports,                 /*- Port number/port of control images -*/
    std::vector<TLevelP> partLevel, /*- List of texture materials -*/
    float dpi,                      /*- Comes in as 1 -*/
    int curr_frame, int shrink,     /*- Comes in as 1 -*/
    double startx,                  /*- Comes in as 0 -*/
    double starty,                  /*- Comes in as 0 -*/
    double endx,                    /*- Comes in as 0 -*/
    double endy,                    /*- Comes in as 0 -*/
    std::vector<int> last_frame, /*- Column length of each texture material -*/
    unsigned long fxId) {
  /*- Various particle parameters -*/
  struct particles_values values;
  // memset(&values, 0, sizeof(values));
  /*- Get each parameter for the current frame -*/
  fill_value_struct(values, m_frame);

  int frame, intpart = 0;

  int level_n = part_ports.size();
  /*- Start frame -*/
  int startframe = (int)values.startpos_val;

  float dpicorr = dpi * 0.01f, fractpart = 0;

  /*- Opacity range (normalize transparent~opaque to 0~1) -*/
  float opacity_range =
      (values.opacity_val.second - values.opacity_val.first) * 0.01f;

  bool random_level = false;

  bool isPrecomputingEnabled = false;
  {
    TRenderer renderer(TRenderer::instance());
    isPrecomputingEnabled =
        (renderer && renderer.isPrecomputingEnabled()) ? true : false;
  }

  /*- When shrunk -*/
  float dpicorr_shrinked;
  if (values.unit_val == Iwa_TiledParticlesFx::UNIT_SMALL_INCH)
    dpicorr_shrinked = dpicorr / shrink;
  else
    dpicorr_shrinked = dpi / shrink;

  std::map<std::pair<int, int>, float> partScales;

  /*- Make current frame the step value -*/
  curr_frame = curr_frame / values.step_val;

  Iwa_ParticlesManager *pc = Iwa_ParticlesManager::instance();

  bool isFirstFrame = !(pc->isCached(fxId));

  // Retrieve the last rolled frame
  Iwa_ParticlesManager::FrameData *particlesData = pc->data(fxId);

  std::list<Iwa_Particle> myParticles;
  TRandom myRandom  = m_parent->randseed_val->getValue();
  values.random_val = &myRandom;

  int totalparticles = 0;

  /*- Regularly arranged (not yet departed) particle information -*/
  QList<ParticleOrigin> particleOrigins;
  /*- Bounding box of output image -*/
  TRectD outTileBBox(tile->m_pos, TDimensionD(tile->getRaster()->getLx(),
                                              tile->getRaster()->getLy()));

  /*- Frame number of currently stored data -*/
  int pcFrame = particlesData->m_frame;

  /*- Convert margin to pixel units -*/
  double pixelMargin;
  {
    TPointD vect(values.margin_val, 0.0);
    TAffine aff(ri.m_affine);
    aff.a13 = aff.a23 = 0;
    vect              = aff * vect;
    pixelMargin       = sqrt(vect.x * vect.x + vect.y * vect.y);
  }
  /*- Generate particles with margin outside -*/
  TRectD resourceTileBBox = outTileBBox.enlarge(pixelMargin);

  /*- Initial particle count. If unchanged, BG can be drawn as is -*/
  int initialOriginsSize;
  if (pcFrame > curr_frame) {
    /*- Initialize data -*/
    // Clear stored particlesData
    particlesData->clear();
    pcFrame = particlesData->m_frame;

    /*- Initialize not-yet-departed particle information -*/
    initParticleOrigins(resourceTileBBox, particleOrigins, curr_frame,
                        ri.m_affine, values, level_n, last_frame, pixelMargin);
    initialOriginsSize = particleOrigins.size();

  }

  else if (pcFrame >= startframe - 1) {
    myParticles        = particlesData->m_particles;
    myRandom           = particlesData->m_random;
    totalparticles     = particlesData->m_totalParticles;
    particleOrigins    = particlesData->m_particleOrigins;
    initialOriginsSize = -1;
  } else {
    /*- Initialize not-yet-departed particle information -*/
    initParticleOrigins(resourceTileBBox, particleOrigins, curr_frame,
                        ri.m_affine, values, level_n, last_frame, pixelMargin);
    initialOriginsSize = particleOrigins.size();
  }

  /*- Loop from start to current frame -*/
  for (frame = startframe - 1; frame <= curr_frame; ++frame) {
    /*-  Reference images are not needed for frames already cached? -*/
    if (frame <= pcFrame) continue;

    int dist_frame = curr_frame - frame;
    /*-
     * Get parameters for current frame within loop. If start is negative, store
     * values for frame=0.
     * -*/
    fill_value_struct(values, frame < 0 ? 0 : frame * values.step_val);
    /*- Parameter normalization -*/
    normalize_values(values, ri);
    /*- maxnum_val is the "birth_rate" parameter -*/
    intpart = (int)values.maxnum_val;
    /*-
     * When birth_rate is decimal, add the decimal parts of each frame and pass
     * the integer result to intpart
     * -*/
    fractpart = fractpart + values.maxnum_val - intpart;
    if ((int)fractpart) {
      values.maxnum_val += (int)fractpart;
      fractpart = fractpart - (int)fractpart;
    }

    std::map<int, TTile *> porttiles;

    // Perform the roll
    /*- Duplicate RenderSettings for current frame calculation -*/
    TRenderSettings riAux(ri);

    /*- Make 64bit -*/
    riAux.m_bpp = 64;
    // riAux.m_bpp = 32;

    int r_frame;  // Useful in case of negative roll frames
    if (frame < 0)
      r_frame = 0;
    else
      r_frame = frame;

    /*- For each port connected to control -*/
    for (std::map<int, TRasterFxPort *>::iterator it = ctrl_ports.begin();
         it != ctrl_ports.end(); ++it) {
      TTile *tmp;
      /*- If port is connected and actually used within Fx -*/
      if ((it->second)->isConnected() && port_is_used(it->first, values)) {
        TRectD bbox = resourceTileBBox;

        /*- If material exists, store control image tile in portTiles -*/
        if (!bbox.isEmpty()) {
          if (frame <= pcFrame) {
            // This frame will not actually be rolled. However, it was
            // dryComputed - so, declare the same here.
            (*it->second)->dryCompute(bbox, r_frame, riAux);
          } else {
            tmp = new TTile;

            if (isPrecomputingEnabled) {
              (*it->second)
                  ->allocateAndCompute(*tmp, bbox.getP00(),
                                       convert(bbox).getSize(), 0, r_frame,
                                       riAux);
            } else {
              std::string alias =
                  "CTRL: " + (*(it->second))->getAlias(r_frame, riAux);
              TRasterImageP rimg = TImageCache::instance()->get(alias, false);

              if (rimg) {
                tmp->m_pos = bbox.getP00();
                tmp->setRaster(rimg->getRaster());
              } else {
                (*it->second)
                    ->allocateAndCompute(*tmp, bbox.getP00(),
                                         convert(bbox).getSize(), 0, r_frame,
                                         riAux);
              }
            }

            porttiles[it->first] = tmp;
          }
        }
      }
    }

    TTile baseImgTile;
    if (values.base_ctrl_val &&
        ctrl_ports.at(values.base_ctrl_val)->isConnected() &&
        port_is_used(values.base_ctrl_val, values) &&
        values.iw_rendermode_val !=
            Iwa_TiledParticlesFx::REND_ILLUMINATED) /*- In illumination mode, BG
                                                       material is not needed
                                                       -*/
    {
      std::string alias =
          "BG_CTRL: " +
          (*ctrl_ports.at(values.base_ctrl_val))->getAlias(r_frame, ri);
      TRasterImageP rimg = TImageCache::instance()->get(alias, false);
      if (rimg) {
        baseImgTile.m_pos = tile->m_pos;
        baseImgTile.setRaster(rimg->getRaster());
      } else {
        /*- Make same bpc as output -*/
        (*ctrl_ports.at(values.base_ctrl_val))
            ->allocateAndCompute(baseImgTile, tile->m_pos,
                                 convert(resourceTileBBox).getSize(),
                                 tile->getRaster(), r_frame, ri);
        addRenderCache(alias, TRasterImageP(baseImgTile.getRaster()));
      }
      baseImgTile.getRaster()->lock();
    }

    // Invoke the actual rolling procedure
    roll_particles(tile, porttiles, riAux, myParticles, values, 0, 0, frame,
                   curr_frame, level_n, &random_level, 1, last_frame,
                   totalparticles, particleOrigins,
                   intpart /*- Actual number of particles to generate -*/
    );

    // Store the rolled data in the particles manager
    if (!particlesData->m_calculated ||
        particlesData->m_frame + particlesData->m_maxTrail < frame) {
      particlesData->m_frame     = frame;
      particlesData->m_particles = myParticles;
      particlesData->m_random    = myRandom;
      particlesData->buildMaxTrail();
      particlesData->m_calculated      = true;
      particlesData->m_totalParticles  = totalparticles;
      particlesData->m_particleOrigins = particleOrigins;
    }

    // Render the particles if the distance from current frame is a trail
    // multiple
    /*- For now, ignore trail -*/
    if (dist_frame == 0) {
      //--------
      // Store the maximum particle size before the do_render cycle
      /*- Among displayed particles, record the maximum size for each material
              Record for both flying particles and pre-departure particles
         according to conditions
         -*/
      /*- ① Flying particles -*/
      if (values.iw_rendermode_val != Iwa_TiledParticlesFx::REND_BG) {
        std::list<Iwa_Particle>::iterator pt;
        for (pt = myParticles.begin(); pt != myParticles.end(); ++pt) {
          Iwa_Particle &part = *pt;
          int ndx            = part.frame % last_frame[part.level];
          std::pair<int, int> ndxPair(part.level, ndx);

          std::map<std::pair<int, int>, float>::iterator it =
              partScales.find(ndxPair);

          if (it != partScales.end())
            it->second = std::max(part.scale, it->second);
          else
            partScales[ndxPair] = part.scale;
        }
      }
      /*- ② If there's at least one pre-departure particle, replace with maximum
       * value -*/
      if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ALL ||
          values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_BG) {
        for (int lev = 0; lev < level_n; lev++) {
          std::pair<int, int> ndxPair(lev, 0);
          std::map<std::pair<int, int>, float>::iterator it =
              partScales.find(ndxPair);
          if (it != partScales.end())
            it->second = std::max((float)values.scale_val.second, it->second);
          else
            partScales[ndxPair] = values.scale_val.second;
        }
      }
      /*- Here, draw background with holes for departed particles -*/
      /*- If switch is ON -*/
      if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ALL ||
          values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_BG) {
        /*- If no particles have taken off yet, draw background as is -*/
        if (initialOriginsSize == particleOrigins.size()) {
          TRop::resample(tile->getRaster(), baseImgTile.getRaster(), TAffine());
        } else {
          renderBackground(tile, particleOrigins, part_ports, ri, partLevel,
                           partScales, &baseImgTile);
        }
      }

      /*- Particle rendering. If in background mode, don't render -*/
      if (values.iw_rendermode_val != Iwa_TiledParticlesFx::REND_BG) {
        if (values.toplayer_val == Iwa_TiledParticlesFx::TOP_SMALLER ||
            values.toplayer_val == Iwa_TiledParticlesFx::TOP_BIGGER)
          myParticles.sort(Iwa_ComparebySize());

        if (values.toplayer_val == Iwa_TiledParticlesFx::TOP_SMALLER) {
          int unit  = 1 + (int)myParticles.size() / 100;
          int count = 0;
          std::list<Iwa_Particle>::iterator pt;
          for (pt = myParticles.begin(); pt != myParticles.end(); ++pt) {
            count++;

            Iwa_Particle &part = *pt;
            if (dist_frame <= part.trail && part.scale > 0.0f &&
                part.lifetime > 0 &&
                part.lifetime <=
                    part.genlifetime)  // This last... shouldn't always be?
            {
              do_render(&part, tile, part_ports, porttiles, ri, p_size,
                        p_offset, last_frame[part.level], partLevel, values,
                        opacity_range, dist_frame, partScales, &baseImgTile);
            }
          }

        } else {
          int unit  = 1 + (int)myParticles.size() / 100;
          int count = 0;
          std::list<Iwa_Particle>::reverse_iterator pt;
          for (pt = myParticles.rbegin(); pt != myParticles.rend(); ++pt) {
            count++;

            Iwa_Particle &part = *pt;
            if (dist_frame <= part.trail && part.scale > 0.0f &&
                part.lifetime > 0 &&
                part.lifetime <= part.genlifetime)  // Same here..?
            {
              do_render(&part, tile, part_ports, porttiles, ri, p_size,
                        p_offset, last_frame[part.level], partLevel, values,
                        opacity_range, dist_frame, partScales, &baseImgTile);
            }
          }
        }
      }
      /*- End of particle rendering -*/
    }

    std::map<int, TTile *>::iterator it;
    for (it = porttiles.begin(); it != porttiles.end(); ++it) delete it->second;

    if (values.base_ctrl_val &&
        ctrl_ports.at(values.base_ctrl_val)->isConnected() &&
        port_is_used(values.base_ctrl_val, values) &&
        values.iw_rendermode_val != Iwa_TiledParticlesFx::REND_ILLUMINATED)
      baseImgTile.getRaster()->unlock();
  }
}

/*-----------------------------------------------------------------
 Comes from render_particles
 Repeat for each particle
-----------------------------------------------------------------*/

void Iwa_Particles_Engine::do_render(
    Iwa_Particle *part, TTile *tile, std::vector<TRasterFxPort *> part_ports,
    std::map<int, TTile *> porttiles, const TRenderSettings &ri,
    TDimension &p_size, TPointD &p_offset, int lastframe,
    std::vector<TLevelP> partLevel, struct particles_values &values,
    float opacity_range, int dist_frame,
    std::map<std::pair<int, int>, float> &partScales, TTile *baseImgTile) {
  /*- Skip particles that are vertical relative to camera without rendering -*/
  if (std::abs(cosf(part->flap_phi * 3.14159f / 180.0f)) < 0.03f) {
    return;
  }
  // Retrieve the particle frame - that is, the *column frame* from which we are
  // picking the particle to be rendered.
  int ndx = part->frame % lastframe;

  TRasterP tileRas(tile->getRaster());

  std::string levelid;
  double aim_angle = 0;
  if (values.pathaim_val) {
    float arctan = atan2f(part->vy, part->vx);
    aim_angle    = arctan * M_180_PI;
  }

  /*- Put particle rotation and scale into affine matrix -*/
  // Calculate rotation & scale for particle
  TRotation rotM(part->angle + aim_angle);
  TScale scaleM(part->scale);

  /*- Fluttering -*/
  TAffine testAff;
  float illuminant;
  {
    float theta = part->flap_theta * 3.14159f / 180.0f;
    float phi   = part->flap_phi * 3.14159f / 180.0f;
    float cosT  = cosf(theta);
    float sinT  = sinf(theta);
    float k     = 1.0f - cosf(phi);

    testAff.a11 = 1.0f - k * cosT * cosT;
    testAff.a21 = -k * cosT * sinT;
    testAff.a12 = -k * cosT * sinT;
    testAff.a22 = 1.0f - k * sinT * sinT;

    /*- Here, calculate brightness for illumination mode -*/
    if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ILLUMINATED) {
      float liTheta   = values.iw_light_theta_val;
      float liPhi     = values.iw_light_phi_val;
      float3 normVec  = {sinf(theta) * sinf(phi), cosf(theta) * sinf(phi),
                         cosf(phi)};
      float3 lightVec = {sinf(liTheta) * sinf(liPhi),
                         cosf(liTheta) * sinf(liPhi), cosf(liPhi)};
      /*- Absolute value of dot product of normal vector and light vector -*/
      illuminant = std::abs(normVec.x * lightVec.x + normVec.y * lightVec.y +
                            normVec.z * lightVec.z);
    }
  }

  TAffine M(rotM * scaleM * testAff);

  // Particles deal with dpi affines on their own
  TAffine scaleAff(m_parent->handledAffine(ri, m_frame));
  float partScale =
      scaleAff.a11 * partScales[std::pair<int, int>(part->level, ndx)];
  TDimensionD partResolution(0, 0);
  TRenderSettings riNew(ri);

  // Retrieve the bounding box in the standard reference
  TRectD bbox(-5.0, -5.0, 5.0, 5.0), standardRefBBox;
  if (part->level <
          (int)part_ports.size() &&  // Not the default levelless cases
      part_ports[part->level]->isConnected()) {
    TRenderSettings riIdentity(ri);
    riIdentity.m_affine = TAffine();

    (*part_ports[part->level])->getBBox(ndx, bbox, riIdentity);

    // A particle's bbox MUST be finite. Gradients and such which have an
    // infinite bbox are just NOT rendered.

    // NOTE: No fx returns half-planes or similar (ie if any coordinate is
    // either (std::numeric_limits<double>::max)() or its opposite, then the
    // rect IS THE infiniteRectD)
    if (bbox == TConsts::infiniteRectD) return;
  }

  // Now, these are the particle rendering specifications
  bbox            = bbox.enlarge(3);
  standardRefBBox = bbox;
  riNew.m_affine  = TScale(partScale);
  bbox            = riNew.m_affine * bbox;
  /*- Size of scaled down Particle -*/
  partResolution = TDimensionD(tceil(bbox.getLx()), tceil(bbox.getLy()));

  TRasterP ras;

  std::string alias;
  TRasterImageP rimg;
  rimg = partLevel[part->level]->frame(ndx);
  if (rimg) {
    ras = rimg->getRaster();
  } else {
    alias = "PART: " + (*part_ports[part->level])->getAlias(ndx, riNew);
    rimg  = TImageCache::instance()->get(alias, false);
    if (rimg) {
      ras = rimg->getRaster();

      // Check that the raster resolution is sufficient for our purposes
      if (ras->getLx() < partResolution.lx || ras->getLy() < partResolution.ly)
        ras = 0;
      else
        partResolution = TDimensionD(ras->getLx(), ras->getLy());
    }
  }

  // We are interested in making the relation between scale and (integer)
  // resolution bijective - since we shall cache by using resolution as a
  // partial identification parameter. Therefore, we find the current bbox Lx
  // and take a unique scale out of it.
  partScale      = partResolution.lx / standardRefBBox.getLx();
  riNew.m_affine = TScale(partScale);
  bbox           = riNew.m_affine * standardRefBBox;

  // Compute image if missing or underscaled
  if (!ras) {
    TTile auxTile;
    (*part_ports[part->level])
        ->allocateAndCompute(auxTile, bbox.getP00(),
                             TDimension(partResolution.lx, partResolution.ly),
                             tile->getRaster(), ndx, riNew);
    ras = auxTile.getRaster();

    // Finally, cache the particle
    addRenderCache(alias, TRasterImageP(ras));
  }

  if (!ras) return;  // At this point, it should never happen anyway...

  // Deal with particle colors/opacity
  TRasterP rfinalpart;
  // TRaster32P rfinalpart;
  double curr_opacity =
      part->set_Opacity(porttiles, values, opacity_range, dist_frame);
  if (curr_opacity != 1.0 || part->gencol.fadecol || part->fincol.fadecol ||
      part->foutcol.fadecol) {
    if (values.pick_color_for_every_frame_val && values.gencol_ctrl_val &&
        (porttiles.find(values.gencol_ctrl_val) != porttiles.end()))
      part->get_image_reference(porttiles[values.gencol_ctrl_val], values,
                                part->gencol.col);

    rfinalpart = ras->clone();
    part->modify_colors_and_opacity(values, curr_opacity, dist_frame,
                                    rfinalpart);
  } else
    rfinalpart = ras;

  TRasterP rfinalpart2;
  /*- In illumination mode, store brightness in color -*/
  if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ILLUMINATED) {
    rfinalpart2 = rfinalpart->clone();
    part->set_illuminated_colors(illuminant, rfinalpart2);
  } else if (baseImgTile->getRaster() && !baseImgTile->getRaster()->isEmpty()) {
    rfinalpart2 = rfinalpart->clone();
    /*- If size is small, simply pick color -*/
    if (partResolution.lx <= 4.0 && partResolution.ly <= 4.0)
      part->get_base_image_color(baseImgTile, values, rfinalpart2, bbox, ri);
    else
      part->get_base_image_texture(baseImgTile, values, rfinalpart2, bbox, ri);
  } else
    rfinalpart2 = rfinalpart;

  // Build particle transform before output tile

  // Incorporate rotation/scale from particle parameters
  M = ri.m_affine * M * TScale(1.0 / partScale);

  // Then, retrieve the particle position in current reference.
  TPointD pos(part->x, part->y);
  pos = ri.m_affine * pos;

  // Finally, add the translational component to the particle
  // NOTE: p_offset is added to account for the particle relative position
  // inside its level's bbox
  M = TTranslation(pos - tile->m_pos) * M * TTranslation(bbox.getP00());

  if (TRaster32P myras32 = tile->getRaster())
    TRop::over(tileRas, rfinalpart2, M);
  else if (TRaster64P myras64 = tile->getRaster())
    TRop::over(tileRas, rfinalpart2, M);
  else {
    throw TException("ParticlesFx: unsupported Pixel Type");
  }
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::fill_array(
    TTile *ctrl1,     /*- Source image Tile -*/
    int &regioncount, /*- Returns region count -*/
    std::vector<int> &
        myarray, /*- Returns indexes. Size is source Tile's width and height -*/
    std::vector<int> &lista, std::vector<int> &listb, int threshold) {
  int pr = 0;
  int i, j;
  int lx, ly;
  lx = ctrl1->getRaster()->getLx();
  ly = ctrl1->getRaster()->getLy();

  /*first row*/
  TRaster32P raster32 = ctrl1->getRaster();
  raster32->lock();
  TPixel32 *pix = raster32->pixels(0);
  for (i = 0; i < lx; i++) {
    if (pix->m > threshold) {
      pr++;
      if (!i) {
        (regioncount)++;
        myarray[i] = (regioncount);
      } else {
        if (myarray[i - 1]) myarray[i] = myarray[i - 1];
      }
    }
    pix++;
  }

  for (j = 1; j < ly; j++) {
    for (i = 0, pix = raster32->pixels(j); i < lx; i++, pix++) {
      /*TMSG_INFO("j=%d i=%d\n", j, i);*/
      if (pix->m > threshold) {
        std::vector<int> mask(4);
        pr++;
        /* l,ul,u,ur;*/
        if (i) {
          mask[0] = myarray[i - 1 + lx * j];
          mask[1] = myarray[i - 1 + lx * (j - 1)];
        }
        if (i != lx - 1) mask[3] = myarray[i + 1 + lx * (j - 1)];
        mask[2] = myarray[i + lx * (j - 1)];
        if (!mask[0] && !mask[1] && !mask[2] && !mask[3]) {
          (regioncount)++;
          myarray[i + lx * j] = (regioncount);
        } else {
          int mc, firsttime = 1;
          for (mc = 0; mc < 4; mc++) {
            if (mask[mc]) {
              if (firsttime) {
                myarray[i + lx * j] = mask[mc];
                firsttime           = 0;
              } else {
                if (myarray[i + lx * j] != mask[mc]) {
                  lista.push_back(myarray[i + lx * j]);
                  listb.push_back(mask[mc]);

                  /*TMSG_INFO("j=%d i=%d mc=%d, mask=%d\n", j, i, mc,
                   * mask[mc]);*/
                }
              }
            }
          }
        }
      }
    }
  }
  raster32->unlock();
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::normalize_array(
    std::vector<std::vector<TPointD>> &myregions, TPointD pos, int lx, int ly,
    int regioncounter, std::vector<int> &myarray, std::vector<int> &lista,
    std::vector<int> &listb, std::vector<int> &final) {
  int i, j, k, l;

  std::vector<int> tmp;
  int maxregioncounter = 0;
  int listsize         = (int)lista.size();
  // TMSG_INFO("regioncounter %d, eqcount=%d\n", regioncounter, eqcount);
  for (k = 1; k <= regioncounter; k++) final[k] = k;

  for (l = 0; l < listsize; l++) {
    j = lista[l];
    /*TMSG_INFO("j vale %d\n", j);*/
    while (final[j] != j) j = final[j];
    k = listb[l];
    /*TMSG_INFO("k vale %d\n", k);*/
    while (final[k] != k) k = final[k];
    if (j != k) final[j] = k;
  }
  // TMSG_INFO("esco dal for\n");
  for (j = 1; j <= regioncounter; j++)
    while (final[j] != final[final[j]]) final[j] = final[final[j]];

  /*count how many damn regions there are*/

  tmp.push_back(final[1]);
  maxregioncounter = 1;
  for (i = 2; i <= regioncounter; i++) {
    int diff = 1;
    for (j = 0; j < maxregioncounter; j++) {
      if (tmp[j] == final[i]) {
        diff = 0;
        break;
      }
    }
    if (diff) {
      tmp.push_back(final[i]);
      maxregioncounter++;
    }
  }

  myregions.resize(maxregioncounter);
  for (j = 0; j < ly; j++) {
    for (i = 0; i < lx; i++) {
      int tmpindex;
      if (myarray[i + lx * j]) {
        tmpindex = final[myarray[i + lx * j]];
        /*TMSG_INFO("tmpindex=%d\n", tmpindex);*/
        for (k = 0; k < maxregioncounter; k++) {
          if (tmp[k] == tmpindex) break;
        }
        /*TMSG_INFO("k=%d\n", k);*/
        TPointD tmppoint;
        tmppoint.x = i;
        tmppoint.y = j;
        tmppoint += pos;
        myregions[k].push_back(tmppoint);
      }
    }
  }
}

/*-----------------------------------------------------------------*/

/*- Analyze source image (ctrl1) regions when multi is ON -*/
void Iwa_Particles_Engine::fill_subregions(
    int cont_index, std::vector<std::vector<TPointD>> &myregions, TTile *ctrl1,
    int thres) {
  int regioncounter = 0;

  int lx = ctrl1->getRaster()->getLx();
  int ly = ctrl1->getRaster()->getLy();

  std::vector<int> myarray(lx * ly);
  std::vector<int> lista;
  std::vector<int> listb;

  fill_array(ctrl1, regioncounter, myarray, lista, listb, thres);

  if (regioncounter) {
    std::vector<int> final(regioncounter + 1);
    normalize_array(myregions, ctrl1->m_pos, lx, ly, regioncounter, myarray,
                    lista, listb, final);
  }
}

/*-----------------------------------------------------------------*/

/*- Vary emission density proportional to alpha value of input image. Give
 * weight to each Point -*/
void Iwa_Particles_Engine::fill_single_region(
    std::vector<TPointD> &myregions, TTile *ctrl1, int threshold,
    bool do_source_gradation, QList<QList<int>> &myHistogram,
    QList<ParticleOrigin> &particleOrigins) {
  assert(ctrl1->getRaster());

  TRaster32P raster32(ctrl1->getRaster()->getSize());
  TRop::convert(raster32, ctrl1->getRaster());
  assert(raster32);  // for now only handle Raster32

  myregions.clear();

  raster32->lock();

  /*- Initialize -*/
  for (int i = 0; i < 256; i++) {
    QList<int> tmpVec;
    myHistogram.push_back(tmpVec);
  }

  if (!do_source_gradation) /*- Single tone case -*/
  {
    for (int po = 0; po < particleOrigins.size(); po++) {
      int index_x = (int)particleOrigins.at(po).pixPos[0];
      int index_y = (int)particleOrigins.at(po).pixPos[1];
      if (index_x < 0)
        index_x = 0;
      else if (index_x >= raster32->getLx())
        index_x = raster32->getLx() - 1;
      if (index_y < 0) {
        index_y = 0;
        continue;
      } else if (index_y >= raster32->getLy()) {
        index_y = raster32->getLy() - 1;
        continue;
      }
      TPixel32 *pix = raster32->pixels(index_y);
      pix += index_x;
      if (pix->m > threshold) {
        myHistogram[1].push_back(po);
        myregions.push_back(TPointD(particleOrigins.at(po).pos[0],
                                    particleOrigins.at(po).pos[1]));
      }
    }
  }

  else {
    TRandom rand = TRandom(1);

    for (int po = 0; po < particleOrigins.size(); po++) {
      int index_x = (int)particleOrigins.at(po).pixPos[0];
      int index_y = (int)particleOrigins.at(po).pixPos[1];
      if (index_x < 0)
        index_x = 0;
      else if (index_x >= raster32->getLx())
        index_x = raster32->getLx() - 1;
      if (index_y < 0)
        index_y = 0;
      else if (index_y >= raster32->getLy())
        index_y = raster32->getLy() - 1;

      TPixel32 *pix = raster32->pixels(index_y);
      pix += index_x;
      if (pix->m > 0) {
        /*- Histogram registration -*/
        myHistogram[(int)pix->m].push_back(po);
        myregions.push_back(TPointD(particleOrigins.at(po).pos[0],
                                    particleOrigins.at(po).pos[1]));
      }
    }
  }

  raster32->unlock();
}

/*----------------------------------------------------------------
 Initialize not-yet-departed particle information
----------------------------------------------------------------*/

static bool potentialLessThan(const ParticleOrigin &po1,
                              const ParticleOrigin &po2) {
  return po1.potential < po2.potential;
}

void Iwa_Particles_Engine::initParticleOrigins(
    TRectD &resourceTileBBox, QList<ParticleOrigin> &particleOrigins,
    const double frame, const TAffine affine, struct particles_values &values,
    int level_n,
    std::vector<int> &lastframe, /*- Frame length of material column -*/
    double pixelMargin) {
  /*- Convert side length of tiling triangle to pixel units -*/
  TPointD vect(values.iw_triangleSize, 0.0);
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0;
  vect              = aff * vect;
  double triPixSize = sqrt(vect.x * vect.x + vect.y * vect.y);

  double pix2Inch = values.iw_triangleSize / triPixSize;

  /*- Horizontal movement distance -*/
  double d_hori = values.iw_triangleSize * 0.5;
  /*- Vertical movement distance -*/
  double d_vert = values.iw_triangleSize * 0.8660254;
  /*- Offset of center position when equilateral triangles are arranged
   * horizontally alternating up and down -*/
  double vOffset = values.iw_triangleSize * 0.14433758;

  /*- Also store pixel position -*/
  double d_hori_pix  = triPixSize * 0.5;
  double d_vert_pix  = triPixSize * 0.8660254;
  double vOffset_pix = triPixSize * 0.14433758;

  TRectD inchBBox(
      resourceTileBBox.x0 * pix2Inch, resourceTileBBox.y0 * pix2Inch,
      resourceTileBBox.x1 * pix2Inch, resourceTileBBox.y1 * pix2Inch);
  /*- Start position in inches -*/
  double curr_y = inchBBox.y0;
  /*- Vertical offset value for first column of row -*/
  double startOff = -vOffset;

  /*- Start position in pixels -*/
  double curr_y_pix   = 0.0;
  double startOff_pix = -vOffset_pix;

  /*- Memory estimation -*/
  int gridSize;
  {
    int ySize = 0;
    while (curr_y <= inchBBox.y1 + d_vert * 0.5) {
      curr_y += d_vert;
      ySize++;
    }
    int xSize     = 0;
    double curr_x = inchBBox.x0;
    while (curr_x <= inchBBox.x1 + d_hori * 0.5) {
      curr_x += d_hori;
      xSize++;
    }
    gridSize = xSize * ySize;
  }
  curr_y = inchBBox.y0;
  particleOrigins.reserve(gridSize);

  /*- Loop vertically -*/
  while (
      curr_y <=
      inchBBox.y1 +
          d_vert *
              0.5) /* ← d_vert * 0.5 is additional for tiling the last row -*/
  {
    double curr_x = inchBBox.x0;
    double off    = startOff;
    /*- Flag for whether triangle is facing up or down -*/
    bool isUp = (off < 0);

    /*- Start position in pixels -*/
    double curr_x_pix = 0.0;
    double off_pix    = startOff_pix;

    /*- Loop horizontally -*/
    while (curr_x <= inchBBox.x1 + d_hori * 0.5) {
      unsigned char level =
          (unsigned char)(values.random_val->getFloat() * level_n);
      particleOrigins.append(ParticleOrigin(
          curr_x, curr_y + off, values.random_val->getFloat(), isUp, level,
          getInitSourceFrame(values, 0, lastframe[level]),
          (short int)tround(curr_x_pix),
          (short int)tround(curr_y_pix + off_pix)));

      off = -off;
      curr_x += d_hori;
      isUp = !isUp;

      off_pix = -off_pix;
      curr_x_pix += d_hori_pix;
    }

    startOff = -startOff;
    curr_y += d_vert;

    startOff_pix = -startOff_pix;
    curr_y_pix += d_vert_pix;
  }

  /*- Sort particles in descending order of random value -*/
  std::sort(particleOrigins.begin(), particleOrigins.end(), potentialLessThan);
}

//--------------------------------------------------

unsigned char Iwa_Particles_Engine::getInitSourceFrame(
    const particles_values &values, int first, int last) {
  switch (values.animation_val) {
  case Iwa_TiledParticlesFx::ANIM_CYCLE:
  case Iwa_TiledParticlesFx::ANIM_S_CYCLE:
    return (unsigned char)first;

  case Iwa_TiledParticlesFx::ANIM_SR_CYCLE:
    return (unsigned char)(first +
                           (values.random_val->getFloat()) * (last - first));

  default:
    return (unsigned char)(first +
                           (values.random_val->getFloat()) * (last - first));
  }
}

/*--------------------------------------------------
 Here, draw background with holes for departed particles
--------------------------------------------------*/
void Iwa_Particles_Engine::renderBackground(
    TTile *tile, QList<ParticleOrigin> &origins,
    std::vector<TRasterFxPort *> part_ports, const TRenderSettings &ri,
    std::vector<TLevelP> partLevel,
    std::map<std::pair<int, int>, float> &partScales, TTile *baseImgTile) {
  TRasterP tileRas(tile->getRaster());
  int unit = 1 + (int)origins.size() / 100;

  /*- For remaining particle sources -*/
  for (int po = 0; po < origins.size(); po++) {
    ParticleOrigin origin = origins.at(po);

    int ndx = origin.initSourceFrame;

    /*- Particle rotation and scale -*/
    TRotation rotM((origin.isUpward) ? 0.0 : 180.0);
    TAffine M(rotM);
    // Particles deal with dpi affines on their own
    TAffine scaleAff(m_parent->handledAffine(ri, m_frame));
    float partScale =
        scaleAff.a11 * partScales[std::pair<int, int>(origin.level, ndx)];
    TDimensionD partResolution(0, 0);
    TRenderSettings riNew(ri);
    // Retrieve the bounding box in the standard reference
    TRectD bbox(-5.0, -5.0, 5.0, 5.0), standardRefBBox;
    if (origin.level <
            (int)part_ports.size() &&  // Not the default levelless cases
        part_ports[origin.level]->isConnected()) {
      TRenderSettings riIdentity(ri);
      riIdentity.m_affine = TAffine();
      (*part_ports[origin.level])->getBBox(ndx, bbox, riIdentity);
      if (bbox == TConsts::infiniteRectD) return;
    }

    // Now, these are the particle rendering specifications
    bbox            = bbox.enlarge(3);
    standardRefBBox = bbox;
    riNew.m_affine  = TScale(partScale);
    bbox            = riNew.m_affine * bbox;
    /*- Size of scaled down Particle -*/
    partResolution = TDimensionD(tceil(bbox.getLx()), tceil(bbox.getLy()));

    TRasterP ras;

    std::string alias;
    TRasterImageP rimg;
    rimg = partLevel[origin.level]->frame(ndx);
    if (rimg) {
      ras = rimg->getRaster();
    } else {
      alias = "PART: " + (*part_ports[origin.level])->getAlias(ndx, riNew);
      rimg  = TImageCache::instance()->get(alias, false);
      if (rimg) {
        ras = rimg->getRaster();

        // Check that the raster resolution is sufficient for our purposes
        if (ras->getLx() < partResolution.lx ||
            ras->getLy() < partResolution.ly)
          ras = 0;
        else
          partResolution = TDimensionD(ras->getLx(), ras->getLy());
      }
    }

    // Ensure a bijective relation between scale and integer resolution for
    // caching by deriving a unique scale from the current bbox Lx.
    partScale      = partResolution.lx / standardRefBBox.getLx();
    riNew.m_affine = TScale(partScale);
    bbox           = riNew.m_affine * standardRefBBox;

    // If no image was retrieved from the cache (or it was not scaled enough),
    // calculate it
    if (!ras) {
      TTile auxTile;
      (*part_ports[origin.level])
          ->allocateAndCompute(auxTile, bbox.getP00(),
                               TDimension(partResolution.lx, partResolution.ly),
                               tile->getRaster(), ndx, riNew);
      ras = auxTile.getRaster();

      // Finally, cache the particle
      addRenderCache(alias, TRasterImageP(ras));
    }

    if (!ras) return;  // At this point, it should never happen anyway...

    M = ri.m_affine * M * TScale(1.0 / partScale);
    TPointD pos(origin.pos[0], origin.pos[1]);
    pos = ri.m_affine * pos;
    M   = TTranslation(pos - tile->m_pos) * M * TTranslation(bbox.getP00());

    if (TRaster32P myras32 = tile->getRaster())
      TRop::over(tileRas, ras, M);
    else if (TRaster64P myras64 = tile->getRaster())
      TRop::over(tileRas, ras, M);
  }
  /*- Include size reduction operation -*/
  TRasterP resizedBGRas;
  if (TRaster32P myBgRas32 = baseImgTile->getRaster())
    resizedBGRas = TRaster32P(tileRas->getSize());
  else if (TRaster64P myBgRas64 = baseImgTile->getRaster())
    resizedBGRas = TRaster64P(tileRas->getSize());
  else
    return;
  TAffine aff;
  /*- Resample -*/
  TRop::resample(resizedBGRas, baseImgTile->getRaster(), aff);

  TRop::ropin(resizedBGRas, tileRas, tileRas);

}
