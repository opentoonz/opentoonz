

#include "trop.h"
#include "tfxparam.h"
#include "tofflinegl.h"
//#include "tstroke.h"
//#include "drawutil.h"
#include "tstopwatch.h"
//#include "tpalette.h"
//#include "tvectorrenderdata.h"
#include "tsystem.h"
#include "timagecache.h"
#include "tconvert.h"

#include "trasterimage.h"

#include "timage_io.h"

#include "tcolorfunctions.h"
#include "toonz/tcolumnfx.h"

#include "particlesmanager.h"

#include "particlesengine.h"

#include "trenderer.h"

#include <sstream>

/*-----------------------------------------------------------------*/

Particles_Engine::Particles_Engine(ParticlesFx *parent, double frame)
    : m_parent(parent), m_frame(frame) {}

static void printTime(TStopWatch &sw, std::string name) {
  std::stringstream ss;
  ss << name << " : ";
  sw.print(ss);
  ss << '\n' << '\0';
  TSystem::outputDebug(ss.str());
}
/*-----------------------------------------------------------------*/
/*
void Particles_Engine::scramble_particles(std::list <Particle*> &myParticles)
{
double size=myParticles.size()
for(i=0; i<myParticles.size();i++)
 {
  j=(int)((size)*tnz_random_float());
  k=(int)((size)*tnz_random_float());

 }
}
*/
/*-----------------------------------------------------------------*/

void Particles_Engine::fill_value_struct(struct particles_values &myvalues,
                                         double frame) {
  myvalues.source_ctrl_val  = m_parent->source_ctrl_val->getValue();
  myvalues.bright_thres_val = m_parent->bright_thres_val->getValue();
  myvalues.multi_source_val = m_parent->multi_source_val->getValue();
  myvalues.x_pos_val        = m_parent->center_val->getValue(frame).x;
  myvalues.y_pos_val        = m_parent->center_val->getValue(frame).y;
  //  myvalues.unit_val=m_parent->unit_val->getValue(frame);
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
  //  myvalues.mblur_val=m_parent->mblur_val->getValue(frame);
  myvalues.scalestep_val      = m_parent->scalestep_val->getValue(frame);
  myvalues.scalestep_ctrl_val = m_parent->scalestep_ctrl_val->getValue();
  myvalues.fadein_val         = m_parent->fadein_val->getValue(frame);
  myvalues.fadeout_val        = m_parent->fadeout_val->getValue(frame);
  myvalues.animation_val      = m_parent->animation_val->getValue();
  myvalues.step_val           = m_parent->step_val->getValue();

  myvalues.gencol_val         = m_parent->gencol_val->getValue(frame);
  myvalues.gencol_ctrl_val    = m_parent->gencol_ctrl_val->getValue();
  myvalues.gencol_spread_val  = m_parent->gencol_spread_val->getValue(frame);
  myvalues.genfadecol_val     = m_parent->genfadecol_val->getValue(frame);
  myvalues.fincol_val         = m_parent->fincol_val->getValue(frame);
  myvalues.fincol_ctrl_val    = m_parent->fincol_ctrl_val->getValue();
  myvalues.fincol_spread_val  = m_parent->fincol_spread_val->getValue(frame);
  myvalues.finrangecol_val    = m_parent->finrangecol_val->getValue(frame);
  myvalues.finfadecol_val     = m_parent->finfadecol_val->getValue(frame);
  myvalues.foutcol_val        = m_parent->foutcol_val->getValue(frame);
  myvalues.foutcol_ctrl_val   = m_parent->foutcol_ctrl_val->getValue();
  myvalues.foutcol_spread_val = m_parent->foutcol_spread_val->getValue(frame);
  myvalues.foutrangecol_val   = m_parent->foutrangecol_val->getValue(frame);
  myvalues.foutfadecol_val    = m_parent->foutfadecol_val->getValue(frame);

  myvalues.source_gradation_val = m_parent->source_gradation_val->getValue();
  myvalues.pick_color_for_every_frame_val =
      m_parent->pick_color_for_every_frame_val->getValue();
  myvalues.perspective_distribution_val =
      m_parent->perspective_distribution_val->getValue();
}

/*-----------------------------------------------------------------*/

void Particles_Engine::fill_range_struct(struct particles_values &values,
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

bool Particles_Engine::port_is_used(int i, struct particles_values &values) {
  return values.fincol_ctrl_val == i || values.foutcol_ctrl_val == i ||
         values.friction_ctrl_val == i || values.gencol_ctrl_val == i ||
         values.gravity_ctrl_val == i || values.opacity_ctrl_val == i ||
         values.rot_ctrl_val == i || values.scale_ctrl_val == i ||
         values.scalestep_ctrl_val == i || values.source_ctrl_val == i ||
         values.speed_ctrl_val == i || values.speeda_ctrl_val == i ||
         values.lifetime_ctrl_val == i || values.randomx_ctrl_val == i ||
         values.randomy_ctrl_val == i;
}
/*-----------------------------------------------------------------*/
/*-- Startフレームからカレントフレームまで順番に回す関数 --*/
void Particles_Engine::roll_particles(
    TTile *tile, std::map<int, TTile *> porttiles, const TRenderSettings &ri,
    std::list<Particle> &myParticles, struct particles_values &values, float cx,
    float cy, int frame, int curr_frame, int level_n, bool *random_level,
    float dpi, std::vector<int> lastframe, int &totalparticles) {
  particles_ranges ranges;
  int i, newparticles;
  float xgravity, ygravity, windx, windy;
  /*-- 風の強さ／重力の強さをX,Y成分に分ける --*/
  windx    = values.windint_val * sin(values.windangle_val);
  windy    = values.windint_val * cos(values.windangle_val);
  xgravity = values.gravity_val * sin(values.g_angle_val);
  ygravity = values.gravity_val * cos(values.g_angle_val);

  fill_range_struct(values, ranges);

  std::vector<std::vector<TPointD>> myregions;

  /*-- [1〜255]
   * そのIndexに対応するアルファ値を持つピクセルのインデックス値を保存。 [0]
   * 使用せず --*/
  std::vector<std::vector<int>> myHistogram;
  /*--
   * アルファ値255から下がっていき、ピクセル数×重み又はアルファ値を次々足した値を格納
   * --*/
  std::vector<float> myWeight;

  std::map<int, TTile *>::iterator it = porttiles.find(values.source_ctrl_val);
  /*-- Perspective
   * DistributionがONのとき、Sizeに刺さったControlImageが粒子の発生分布を決める
   * --*/
  std::map<int, TTile *>::iterator sizeIt =
      porttiles.find(values.scale_ctrl_val);
  if (values.perspective_distribution_val && (sizeIt != porttiles.end())) {
    /*-- ソース画像にコントロールが付いていた場合、そのアルファ値をマスクに使う
     * --*/
    if (values.source_ctrl_val && (it != porttiles.end()))
      fill_regions_with_size_map(myregions, myHistogram, sizeIt->second,
                                 it->second, values.bright_thres_val);
    else
      fill_regions_with_size_map(myregions, myHistogram, sizeIt->second, 0,
                                 values.bright_thres_val);
    /*- パーティクルを作る前に myregion内の候補数を合計する--*/
    if ((int)myHistogram.size() == 256) {
      for (int m = 255; m >= 0; m--) {
        /*-- 明度からサイズ サイズから重みを出す --*/
        float scale =
            values.scale_val.first + ranges.scale_range * (float)m / 255.0f;
        float weight = 1.0f / (scale * scale);

        float tmpSum = weight * (float)myHistogram[m].size();
        int index    = 255 - m;
        if (index > 0) /*-- これまでの合計に追加する --*/
          tmpSum += myWeight[index - 1];
        myWeight.push_back(tmpSum);
      }
    }
  } else {
    /*- ソース画像にコントロールが付いていた場合 -*/
    if (values.source_ctrl_val && (it != porttiles.end()))
      /*-- 入力画像のアルファ値に比例して発生濃度を変える --*/
      fill_regions(1, myregions, it->second, values.multi_source_val,
                   values.bright_thres_val, values.source_gradation_val,
                   myHistogram);

    /*- パーティクルを作る前に myregion内の候補数を合計する--*/
    /*-- myWeight
     * の中には、アルファ255から0まで、各アルファ値×ポイント数を足しこんでいったものが格納される。--*/
    if ((int)myHistogram.size() == 256) {
      for (int m = 255; m > 0; m--) {
        float tmpSum = (float)(m * (int)myHistogram[m].size());
        int index    = 255 - m;
        if (index > 0) tmpSum += myWeight[index - 1];
        myWeight.push_back(tmpSum);
      }
    }
  }

  /*- birth rate を格納 -*/
  newparticles = (int)values.maxnum_val;
  if (myParticles.empty() && newparticles)  // Initial creation
  {
    /*- 新たに作るパーティクルの数だけ繰り返す -*/
    for (i = 0; i < newparticles; i++) {
      int seed = (int)((std::numeric_limits<int>::max)() *
                       values.random_val->getFloat());
      int level = (int)(values.random_val->getFloat() * level_n);

      int lifetime = 0;
      if (values.column_lifetime_val)
        lifetime = lastframe[level];
      else
        lifetime = (int)(values.lifetime_val.first +
                         ranges.lifetime_range * values.random_val->getFloat());
      if (lifetime > curr_frame - frame)
        myParticles.push_back(Particle(
            lifetime, seed, porttiles, values, ranges, myregions,
            totalparticles, 0, level, lastframe[level], myHistogram, myWeight));

      totalparticles++;
    }
  } else {
    std::list<Particle>::iterator it;
    for (it = myParticles.begin(); it != myParticles.end();) {
      std::list<Particle>::iterator current = it;
      ++it;

      Particle &part = (*current);
      if (part.lifetime <= 0)        // Note: This is in line with the above
                                     // "lifetime>curr_frame-frame"
        myParticles.erase(current);  // insertion counterpart
      else
        part.move(porttiles, values, ranges, windx, windy, xgravity, ygravity,
                  dpi, lastframe[part.level]);
    }

    int oldparticles = myParticles.size();
    switch (values.toplayer_val) {
    case ParticlesFx::TOP_YOUNGER:
      for (i = 0; i < newparticles; i++) {
        int seed = (int)((std::numeric_limits<int>::max)() *
                         values.random_val->getFloat());
        int level = (int)(values.random_val->getFloat() * level_n);

        int lifetime = 0;
        if (values.column_lifetime_val)
          lifetime = lastframe[level];
        else
          lifetime =
              (int)(values.lifetime_val.first +
                    ranges.lifetime_range * values.random_val->getFloat());

        if (lifetime > curr_frame - frame)
          myParticles.push_front(Particle(lifetime, seed, porttiles, values,
                                          ranges, myregions, totalparticles, 0,
                                          level, lastframe[level], myHistogram,
                                          myWeight));

        totalparticles++;
      }
      break;

    case ParticlesFx::TOP_RANDOM:
      for (i = 0; i < newparticles; i++) {
        double tmp = values.random_val->getFloat() * myParticles.size();
        std::list<Particle>::iterator it = myParticles.begin();
        for (int j = 0; j < tmp; j++, it++)
          ;
        {
          int seed = (int)((std::numeric_limits<int>::max)() *
                           values.random_val->getFloat());
          int level    = (int)(values.random_val->getFloat() * level_n);
          int lifetime = 0;

          if (values.column_lifetime_val)
            lifetime = lastframe[level];
          else
            lifetime =
                (int)(values.lifetime_val.first +
                      ranges.lifetime_range * values.random_val->getFloat());
          if (lifetime > curr_frame - frame)
            myParticles.insert(
                it, Particle(lifetime, seed, porttiles, values, ranges,
                             myregions, totalparticles, 0, level,
                             lastframe[level], myHistogram, myWeight));

          totalparticles++;
        }
      }
      break;

    default:
      for (i = 0; i < newparticles; i++) {
        int seed = (int)((std::numeric_limits<int>::max)() *
                         values.random_val->getFloat());
        int level    = (int)(values.random_val->getFloat() * level_n);
        int lifetime = 0;

        if (values.column_lifetime_val)
          lifetime = lastframe[level];
        else
          lifetime =
              (int)(values.lifetime_val.first +
                    ranges.lifetime_range * values.random_val->getFloat());
        if (lifetime > curr_frame - frame)
          myParticles.push_back(Particle(lifetime, seed, porttiles, values,
                                         ranges, myregions, totalparticles, 0,
                                         level, lastframe[level], myHistogram,
                                         myWeight));

        totalparticles++;
      }
      break;
    }
  }
}

/*-----------------------------------------------------------------*/

void Particles_Engine::normalize_values(struct particles_values &values,
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
  values.genfadecol_val                    = (values.genfadecol_val) * 0.01;
  values.finfadecol_val                    = (values.finfadecol_val) * 0.01;
  values.foutfadecol_val                   = (values.foutfadecol_val) * 0.01;
}

/*-----------------------------------------------------------------*/

void Particles_Engine::render_particles(
    TTile *tile, std::vector<TRasterFxPort *> part_ports,
    const TRenderSettings &ri, TDimension &p_size, TPointD &p_offset,
    std::map<int, TRasterFxPort *> ctrl_ports, std::vector<TLevelP> partLevel,
    float dpi, int curr_frame, int shrink, double startx, double starty,
    double endx, double endy, std::vector<int> last_frame, unsigned long fxId) {
  int frame, startframe, intpart = 0, level_n = 0;
  struct particles_values values;
  double dpicorr = dpi * 0.01, fractpart = 0, dpicorr_shrinked = 0,
         opacity_range = 0;
  bool random_level    = false;
  level_n              = part_ports.size();

  bool isPrecomputingEnabled = false;
  {
    TRenderer renderer(TRenderer::instance());
    isPrecomputingEnabled =
        (renderer && renderer.isPrecomputingEnabled()) ? true : false;
  }

  memset(&values, 0, sizeof(values));
  /*- 現在のフレームでの各種パラメータを得る -*/
  fill_value_struct(values, m_frame);
  /*- 不透明度の範囲（透明〜不透明を 0〜1 に正規化）-*/
  opacity_range = (values.opacity_val.second - values.opacity_val.first) * 0.01;
  /*- 開始フレーム -*/
  startframe = (int)values.startpos_val;
  if (values.unit_val == ParticlesFx::UNIT_SMALL_INCH)
    dpicorr_shrinked = dpicorr / shrink;
  else
    dpicorr_shrinked = dpi / shrink;

  std::map<std::pair<int, int>, double> partScales;
  curr_frame = curr_frame / values.step_val;

  ParticlesManager *pc = ParticlesManager::instance();

  // Retrieve the last rolled frame
  ParticlesManager::FrameData *particlesData = pc->data(fxId);

  std::list<Particle> myParticles;
  TRandom myRandom;
  values.random_val  = &myRandom;
  myRandom           = m_parent->randseed_val->getValue();
  int totalparticles = 0;

  int pcFrame = particlesData->m_frame;
  if (pcFrame > curr_frame) {
    // Clear stored particlesData
    particlesData->clear();
    pcFrame = particlesData->m_frame;
  } else if (pcFrame >= startframe - 1) {
    myParticles    = particlesData->m_particles;
    myRandom       = particlesData->m_random;
    totalparticles = particlesData->m_totalParticles;
  }
  /*- スタートからカレントフレームまでループ -*/
  for (frame = startframe - 1; frame <= curr_frame; ++frame) {
    int dist_frame = curr_frame - frame;
    /*-
     * ループ内の現在のフレームでのパラメータを取得。スタートが負ならフレーム=0のときの値を格納
     * -*/
    fill_value_struct(values, frame < 0 ? 0 : frame * values.step_val);
    /*- パラメータの正規化 -*/
    normalize_values(values, ri);
    /*- maxnum_valは"birth_rate"のパラメータ -*/
    intpart = (int)values.maxnum_val;
    /*-
     * /birth_rateが小数だったとき、各フレームの小数部分を足しこんだ結果の整数部分をintpartに渡す。
     * -*/
    fractpart = fractpart + values.maxnum_val - intpart;
    if ((int)fractpart) {
      values.maxnum_val += (int)fractpart;
      fractpart = fractpart - (int)fractpart;
    }

    std::map<int, TTile *> porttiles;

    // Perform the roll
    /*- RenderSettingsを複製して現在のフレームの計算用にする -*/
    TRenderSettings riAux(ri);
    riAux.m_affine = TAffine();
    riAux.m_bpp    = 32;

    int r_frame;  // Useful in case of negative roll frames
    if (frame < 0)
      r_frame = 0;
    else
      r_frame = frame;
    /*- 出力画像のバウンディングボックス -*/
    TRectD outTileBBox(tile->m_pos, TDimensionD(tile->getRaster()->getLx(),
                                                tile->getRaster()->getLy()));

    // enlarge bounding box for control images with infinite bbox in case the
    // source region is larger than output tile
    TRectD bboxForInifiniteSource = ri.m_affine.inv() * outTileBBox;
    TRectD sourceBbox;
    if (values.source_ctrl_val &&
        ctrl_ports.at(values.source_ctrl_val)->isConnected()) {
      (*(ctrl_ports.at(values.source_ctrl_val)))
          ->getBBox(r_frame, sourceBbox, riAux);
    }
    if (sourceBbox.isEmpty() || sourceBbox == TConsts::infiniteRectD) {
      sourceBbox = TRectD(values.x_pos_val - values.length_val * 0.5,
                          values.y_pos_val - values.height_val * 0.5,
                          values.x_pos_val + values.length_val * 0.5,
                          values.y_pos_val + values.height_val * 0.5);
    }
    bboxForInifiniteSource += sourceBbox;

    /*- Controlに刺さっている各ポートについて -*/
    for (std::map<int, TRasterFxPort *>::iterator it = ctrl_ports.begin();
         it != ctrl_ports.end(); ++it) {
      TTile *tmp;
      /*- ポートが接続されていて、Fx内で実際に使用されていたら -*/
      if ((it->second)->isConnected() && port_is_used(it->first, values)) {
        TRectD bbox;
        (*(it->second))->getBBox(r_frame, bbox, riAux);
        /*- 素材が存在する場合、portTilesにコントロール画像タイルを格納 -*/
        if (!bbox.isEmpty()) {
          if (bbox == TConsts::infiniteRectD)  // There could be an infinite
                                               // bbox - deal with it
            bbox = bboxForInifiniteSource;

          if (frame <= pcFrame) {
            // This frame will not actually be rolled. However, it was
            // dryComputed - so, declare the same here.
            (*it->second)->dryCompute(bbox, r_frame, riAux);
          } else {
            tmp = new TTile;

            if (isPrecomputingEnabled)
              (*it->second)
                  ->allocateAndCompute(*tmp, bbox.getP00(),
                                       convert(bbox).getSize(), 0, r_frame,
                                       riAux);
            else {
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

                addRenderCache(alias, TRasterImageP(tmp->getRaster()));
              }
            }

            porttiles[it->first] = tmp;
          }
        }
      }
    }

    if (frame > pcFrame) {
      // Invoke the actual rolling procedure
      roll_particles(tile, porttiles, riAux, myParticles, values, 0, 0, frame,
                     curr_frame, level_n, &random_level, 1, last_frame,
                     totalparticles);

      // Store the rolled data in the particles manager
      if (!particlesData->m_calculated ||
          particlesData->m_frame + particlesData->m_maxTrail < frame) {
        particlesData->m_frame     = frame;
        particlesData->m_particles = myParticles;
        particlesData->m_random    = myRandom;
        particlesData->buildMaxTrail();
        particlesData->m_calculated     = true;
        particlesData->m_totalParticles = totalparticles;
      }
    }

    // Render the particles if the distance from current frame is a trail
    // multiple
    if (frame >= startframe - 1 &&
        !(dist_frame %
          (values.trailstep_val > 1.0 ? (int)values.trailstep_val : 1))) {
      // Store the maximum particle size before the do_render cycle
      std::list<Particle>::iterator pt;
      for (pt = myParticles.begin(); pt != myParticles.end(); ++pt) {
        Particle &part = *pt;
        int ndx        = part.frame % last_frame[part.level];
        std::pair<int, int> ndxPair(part.level, ndx);

        std::map<std::pair<int, int>, double>::iterator it =
            partScales.find(ndxPair);

        if (it != partScales.end())
          it->second = std::max(part.scale, it->second);
        else
          partScales[ndxPair] = part.scale;
      }

      if (values.toplayer_val == ParticlesFx::TOP_SMALLER ||
          values.toplayer_val == ParticlesFx::TOP_BIGGER)
        myParticles.sort(ComparebySize());

      if (values.toplayer_val == ParticlesFx::TOP_SMALLER) {
        std::list<Particle>::iterator pt;
        for (pt = myParticles.begin(); pt != myParticles.end(); ++pt) {
          Particle &part = *pt;
          if (dist_frame <= part.trail && part.scale && part.lifetime > 0 &&
              part.lifetime <=
                  part.genlifetime)  // This last... shouldn't always be?
          {
            do_render(&part, tile, part_ports, porttiles, ri, p_size, p_offset,
                      last_frame[part.level], partLevel, values, opacity_range,
                      dist_frame, partScales);
          }
        }
      } else {
        std::list<Particle>::reverse_iterator pt;
        for (pt = myParticles.rbegin(); pt != myParticles.rend(); ++pt) {
          Particle &part = *pt;
          if (dist_frame <= part.trail && part.scale && part.lifetime > 0 &&
              part.lifetime <= part.genlifetime)  // Same here..?
          {
            do_render(&part, tile, part_ports, porttiles, ri, p_size, p_offset,
                      last_frame[part.level], partLevel, values, opacity_range,
                      dist_frame, partScales);
          }
        }
      }
    }

    std::map<int, TTile *>::iterator it;
    for (it = porttiles.begin(); it != porttiles.end(); ++it) delete it->second;
  }
}

//-----------------------------------------------------------------
/*- render_particles から呼ばれる。粒子の数だけ繰り返し -*/
void Particles_Engine::do_render(
    Particle *part, TTile *tile, std::vector<TRasterFxPort *> part_ports,
    std::map<int, TTile *> porttiles, const TRenderSettings &ri,
    TDimension &p_size, TPointD &p_offset, int lastframe,
    std::vector<TLevelP> partLevel, struct particles_values &values,
    double opacity_range, int dist_frame,
    std::map<std::pair<int, int>, double> &partScales) {
  // Retrieve the particle frame - that is, the *column frame* from which we are
  // picking
  // the particle to be rendered.
  int ndx = part->frame % lastframe;

  TRasterP tileRas(tile->getRaster());

  std::string levelid;
  double aim_angle = 0;
  if (values.pathaim_val) {
    double arctan = atan2(part->vy, part->vx);
    aim_angle     = arctan * M_180_PI;
  }

  // Calculate the rotational and scale components we have to apply on the
  // particle
  TRotation rotM(part->angle + aim_angle);
  TScale scaleM(part->scale);
  TAffine M(rotM * scaleM);

  // Particles deal with dpi affines on their own
  TAffine scaleAff(m_parent->handledAffine(ri, m_frame));
  double partScale =
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
    // infinite bbox
    // are just NOT rendered.

    // NOTE: No fx returns half-planes or similar (ie if any coordinate is
    // either
    // (std::numeric_limits<double>::max)() or its opposite, then the rect IS
    // THE infiniteRectD)
    if (bbox.isEmpty() || bbox == TConsts::infiniteRectD) return;
  }

  // Now, these are the particle rendering specifications
  bbox            = bbox.enlarge(3);
  standardRefBBox = bbox;
  riNew.m_affine  = TScale(partScale);
  bbox            = riNew.m_affine * bbox;
  /*- 縮小済みのParticleのサイズ -*/
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
  // resolution
  // bijective - since we shall cache by using resolution as a partial
  // identification parameter.
  // Therefore, we find the current bbox Lx and take a unique scale out of it.
  partScale      = partResolution.lx / standardRefBBox.getLx();
  riNew.m_affine = TScale(partScale);
  bbox           = riNew.m_affine * standardRefBBox;

  // If no image was retrieved from the cache (or it was not scaled enough),
  // calculate it
  if (!ras) {
    TTile auxTile;
    (*part_ports[part->level])
        ->allocateAndCompute(auxTile, bbox.getP00(),
                             TDimension(partResolution.lx, partResolution.ly),
                             tile->getRaster(), ndx, riNew);
    ras = auxTile.getRaster();

    // For now, we'll just use 32 bit particles
    TRaster32P rcachepart;
    rcachepart = ras;
    if (!rcachepart) {
      rcachepart = TRaster32P(ras->getSize());
      TRop::convert(rcachepart, ras);
    }
    ras = rcachepart;

    // Finally, cache the particle
    addRenderCache(alias, TRasterImageP(ras));
  }

  if (!ras) return;  // At this point, it should never happen anyway...

  // Deal with particle colors/opacity
  TRaster32P rfinalpart;
  double curr_opacity =
      part->set_Opacity(porttiles, values, opacity_range, dist_frame);
  if (curr_opacity != 1.0 || part->gencol.fadecol || part->fincol.fadecol ||
      part->foutcol.fadecol) {
    /*- 毎フレーム現在位置のピクセル色を参照 -*/
    if (values.pick_color_for_every_frame_val && values.gencol_ctrl_val &&
        (porttiles.find(values.gencol_ctrl_val) != porttiles.end()))
      part->get_image_reference(porttiles[values.gencol_ctrl_val], values,
                                part->gencol.col);

    rfinalpart = ras->clone();
    part->modify_colors_and_opacity(values, curr_opacity, dist_frame,
                                    rfinalpart);
  } else
    rfinalpart = ras;

  // Now, let's build the particle transform before it is overed on the output
  // tile

  // First, complete the transform by adding the rotational and scale
  // components from
  // Particles parameters
  M = ri.m_affine * M * TScale(1.0 / partScale);

  // Then, retrieve the particle position in current reference.
  TPointD pos(part->x, part->y);
  pos = ri.m_affine * pos;

  // Finally, add the translational component to the particle
  // NOTE: p_offset is added to account for the particle relative position
  // inside its level's bbox
  M = TTranslation(pos - tile->m_pos) * M * TTranslation(bbox.getP00());

  if (TRaster32P myras32 = tile->getRaster())
    TRop::over(tileRas, rfinalpart, M);
  else if (TRaster64P myras64 = tile->getRaster())
    TRop::over(tileRas, rfinalpart, M);
  else
    throw TException("ParticlesFx: unsupported Pixel Type");
}

/*-----------------------------------------------------------------*/

void Particles_Engine::fill_array(TTile *ctrl1, int &regioncount,
                                  std::vector<int> &myarray,
                                  std::vector<int> &lista,
                                  std::vector<int> &listb, int threshold) {
  int pr = 0;
  int i, j;
  int lx, ly;
  lx = ctrl1->getRaster()->getLx();
  ly = ctrl1->getRaster()->getLy();

  /*prima riga*/
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
        mask[2]                  = myarray[i + lx * (j - 1)];
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

void Particles_Engine::normalize_array(
    std::vector<std::vector<TPointD>> &myregions, TPointD pos, int lx, int ly,
    int regioncounter, std::vector<int> &myarray, std::vector<int> &lista,
    std::vector<int> &listb, std::vector<int> & final) {
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
    k                       = listb[l];
    /*TMSG_INFO("k vale %d\n", k);*/
    while (final[k] != k) k = final[k];
    if (j != k) final[j]    = k;
  }
  // TMSG_INFO("esco dal for\n");
  for (j                                         = 1; j <= regioncounter; j++)
    while (final[j] != final[final[j]]) final[j] = final[final[j]];

  /*conto quante cavolo di regioni sono*/

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
/*- multiがONのときのSource画像（ctrl1）の領域を分析 -*/
void Particles_Engine::fill_subregions(
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
/*- 入力画像のアルファ値に比例して発生濃度を変える。各Pointにウェイトを持たせる
 * -*/
void Particles_Engine::fill_single_region(
    std::vector<std::vector<TPointD>> &myregions, TTile *ctrl1, int threshold,
    bool do_source_gradation, std::vector<std::vector<int>> &myHistogram) {
  TRaster32P raster32 = ctrl1->getRaster();
  assert(raster32);  // per ora gestisco solo i Raster32
                     //  int lx=raster32->getLx();
                     //  int ly=raster32->getLy();
  int j;
  myregions.resize(1);
  myregions[0].clear();
  int cc  = 0;
  int icc = 0;
  raster32->lock();

  if (!do_source_gradation) /*- 2階調の場合 -*/
  {
    for (j = 0; j < raster32->getLy(); j++) {
      TPixel32 *pix    = raster32->pixels(j);
      TPixel32 *endPix = pix + raster32->getLx();
      int i            = 0;
      while (pix < endPix) {
        cc++;
        if (pix->m > threshold) {
          icc++;
          TPointD tmp;
          tmp.y = j;
          tmp.x = i;
          tmp += ctrl1->m_pos;
          myregions[0].push_back(tmp);
          /*TMSG_INFO("total=%d\n", Region[0].total);*/

        } else {
          //           int a=0;
        }
        i++;
        pix++;
      }
    }
  } else {
    for (int i = 0; i < 256; i++) myHistogram.push_back(std::vector<int>());

    TRandom rand = TRandom(1);
    for (j = 0; j < raster32->getLy(); j++) {
      TPixel32 *pix    = raster32->pixels(j);
      TPixel32 *endPix = pix + raster32->getLx();
      int i            = 0;
      while (pix < endPix) {
        cc++;
        /*-- アルファの濃度に比例してパーティクルを発生させるための、
                シンプルな方法。そのピクセルのアルファ値の数だけ「立候補」させる。
        --*/
        if (pix->m > 0) {
          icc++;
          TPointD tmp;
          tmp.y = j;
          tmp.x = i;
          tmp += ctrl1->m_pos;

          /*- Histogramの登録 -*/
          myHistogram[(int)pix->m].push_back((int)myregions[0].size());
          /*-  各Pointにウェイトを持たせる -*/
          myregions[0].push_back(tmp);
        } else {
        }
        i++;
        pix++;
      }
    }
  }
  if (myregions[0].size() == 0) myregions.resize(0);
  raster32->unlock();
}

/*-----------------------------------------------------------------*/
/*-
 * 入力画像のアルファ値に比例して発生濃度を変える。Histogramを格納しながら領域を登録
 * -*/
void Particles_Engine::fill_regions(
    int frame, std::vector<std::vector<TPointD>> &myregions, TTile *ctrl1,
    bool multi, int thres, bool do_source_gradation,
    std::vector<std::vector<int>> &myHistogram) {
  TRaster32P ctrl1ras = ctrl1->getRaster();
  if (!ctrl1ras) return;
  int i;
  if (frame <= 0)
    i = 0;
  else
    i = frame;

  if (multi) {
    fill_subregions(i, myregions, ctrl1, thres);
  } else {
    fill_single_region(myregions, ctrl1, thres, do_source_gradation,
                       myHistogram);
  }
}

//----------------------------------------------------------------
/*-- Perspective
DistributionがONのとき、Sizeに刺さったControlImageが粒子の発生分布を決める。
        そのとき、SourceのControlが刺さっている場合は、マスクとして用いられる
--*/

void Particles_Engine::fill_regions_with_size_map(
    std::vector<std::vector<TPointD>> &myregions,
    std::vector<std::vector<int>> &myHistogram, TTile *sizeTile,
    TTile *sourceTile, int thres) {
  TRaster32P sizeRas = sizeTile->getRaster();
  if (!sizeRas) return;

  TRaster32P sourceRas;
  if (sourceTile) sourceRas = sourceTile->getRaster();

  sizeRas->lock();
  if (sourceRas) sourceRas->lock();

  myregions.resize(1);
  myregions[0].clear();
  for (int i = 0; i < 256; i++) myHistogram.push_back(std::vector<int>());

  for (int j = 0; j < sizeRas->getLy(); j++) {
    TPixel32 *pix    = sizeRas->pixels(j);
    TPixel32 *endPix = pix + sizeRas->getLx();

    TPixel32 *sourcePixHead = 0;
    if (sourceRas) {
      int sourceYPos = troundp(j + sizeTile->m_pos.y - sourceTile->m_pos.y);
      if (sourceYPos >= 0 && sourceYPos < sourceRas->getLy())
        sourcePixHead = sourceRas->pixels(sourceYPos);
    }

    int i               = 0;
    TPixel32 *sourcePix = 0;
    while (pix < endPix) {
      if (sourceRas) {
        int sourceXPos = (int)(i + sizeTile->m_pos.x - sourceTile->m_pos.x);
        if (sourcePixHead && sourceXPos >= 0 && sourceXPos < sourceRas->getLx())
          sourcePix = sourcePixHead + sourceXPos;
        else
          sourcePix = 0;
      } else
        sourcePix = 0;

      /*-
       * Source画像があって、ピクセルがバウンディング外またはアルファが０なら抜かす。
       * -*/
      if (sourceRas && (!sourcePix || sourcePix->m <= thres)) {
      }
      /*-
         明度に比例してパーティクルを発生させる。そのピクセルのアルファ値の数だけ「立候補」させる。-*/
      else {
        TPointD tmp;
        tmp.y = j;
        tmp.x = i;
        tmp += sizeTile->m_pos;

        int val = (int)TPixelGR8::from(*pix).value;

        /*- Histogramの登録 -*/
        myHistogram[val].push_back((int)myregions[0].size());

        /*- 各Pointにウェイトを持たせる -*/
        myregions[0].push_back(tmp);
      }

      i++;
      pix++;
    }
  }

  if (myregions[0].size() == 0) myregions.resize(0);

  sizeRas->unlock();
  if (sourceRas) sourceRas->unlock();
}
