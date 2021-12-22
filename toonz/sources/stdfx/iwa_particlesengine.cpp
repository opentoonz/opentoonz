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
//----

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
  /*- 計算モード （背景＋粒子／粒子／背景／照明された粒子） -*/
  myvalues.iw_rendermode_val = m_parent->iw_rendermode_val->getValue();
  /*- 粒子に貼られる絵の素材 -*/
  myvalues.base_ctrl_val = m_parent->base_ctrl_val->getValue();
  /*- カールノイズ的な動きを与える -*/
  myvalues.curl_val        = m_parent->curl_val->getValue(frame);
  myvalues.curl_ctrl_1_val = m_parent->curl_ctrl_1_val->getValue();
  myvalues.curl_ctrl_2_val = m_parent->curl_ctrl_2_val->getValue();
  /*- 粒子敷き詰め。粒子を正三角形で敷き詰めたときの、
          正三角形の一辺の長さをインチで指定する -*/
  myvalues.iw_triangleSize = m_parent->iw_triangleSize->getValue(frame);
  /*- ひらひら回転 -*/
  myvalues.flap_ctrl_val = m_parent->flap_ctrl_val->getValue();
  myvalues.iw_flap_velocity_val =
      m_parent->iw_flap_velocity_val->getValue(frame);
  myvalues.iw_flap_dir_sensitivity_val =
      m_parent->iw_flap_dir_sensitivity_val->getValue(frame);
  /*- ひらひら粒子に照明を当てる normalize_values()内で Degree → Radian 化する
   * -*/
  myvalues.iw_light_theta_val = m_parent->iw_light_theta_val->getValue(frame);
  myvalues.iw_light_phi_val   = m_parent->iw_light_phi_val->getValue(frame);
  /*- 読み込みマージン -*/
  myvalues.margin_val = m_parent->margin_val->getValue(frame);
  /*- 重力を徐々に与えるためのフレーム長 -*/
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

//-------------
/*-  Startフレームからカレントフレームまで順番に回す関数 生成もここで -*/
void Iwa_Particles_Engine::roll_particles(
    TTile *tile,                      /*-結果を格納するTile-*/
    std::map<int, TTile *> porttiles, /*-コントロール画像のポート番号／タイル-*/
    const TRenderSettings &ri, /*-現在のフレームの計算用RenderSettings-*/
    std::list<Iwa_Particle> &myParticles, /*-パーティクルのリスト-*/
    struct particles_values &values, /*-現在のフレームでのパラメータ-*/
    float cx,                        /*- 0 で入ってくる-*/
    float cy,                        /*- 0 で入ってくる-*/
    int frame,          /*-現在のフレーム値（forで回す値）-*/
    int curr_frame,     /*-カレントフレーム-*/
    int level_n,        /*-テクスチャ素材画像の数-*/
    bool *random_level, /*-ループの最初にfalseで入ってくる-*/
    float dpi,          /*- 1 で入ってくる-*/
    std::vector<int> lastframe, /*-テクスチャ素材のそれぞれのカラム長-*/
    int &totalparticles, QList<ParticleOrigin> &particleOrigins,
    int genPartNum /*- 実際に生成したい粒子数 -*/
    ) {
  particles_ranges ranges;
  int i;
  float xgravity, ygravity, windx, windy;

  /*- 風の強さ／重力の強さをX,Y成分に分ける -*/
  windx    = values.windint_val * sin(values.windangle_val);
  windy    = values.windint_val * cos(values.windangle_val);
  xgravity = values.gravity_val * sin(values.g_angle_val);
  ygravity = values.gravity_val * cos(values.g_angle_val);

  fill_range_struct(values, ranges);

  std::vector<TPointD> myregions;
  QList<QList<int>> myHistogram;

  std::map<int, TTile *>::iterator it = porttiles.find(values.source_ctrl_val);

  /*- ソース画像にコントロールが付いていた場合 -*/
  if (values.source_ctrl_val && (it != porttiles.end()) &&
      it->second->getRaster())
    /*- 入力画像のアルファ値に比例して発生濃度を変える -*/
    fill_single_region(myregions, it->second, values.bright_thres_val,
                       values.source_gradation_val, myHistogram,
                       particleOrigins);

  /*- 粒子が出きったらもう出さない -*/
  int actualBirthParticles = std::min(genPartNum, particleOrigins.size());
  /*- 出発する粒子のインデックス -*/
  QList<int> leavingPartIndex;
  if (myregions.size() &&
      porttiles.find(values.source_ctrl_val) != porttiles.end()) {
    int partLeft = actualBirthParticles;
    while (partLeft > 0) {
      int PrePartLeft = partLeft;

      int potential_sum = 0;
      QList<int> myWeight;
      /*- 各濃度のポテンシャルの大きさmyWeightと、合計ポテンシャルを計算 -*/
      for (int m = 0; m < 256; m++) {
        int pot = myHistogram[m].size() * m;
        myWeight.append(pot);
        potential_sum += pot;
      }
      /*- 各濃度について(濃い方から) -*/
      for (int m = 255; m > 0; m--) {
        /*- 割り当てを計算（切り上げ） -*/
        int wariate = tceil((float)(myWeight[m]) * (float)(partLeft) /
                            (float)potential_sum);
        /*- 実際にこのポテンシャルから出発する粒子数 -*/
        int leaveNum = std::min(myHistogram.at(m).size(), wariate);
        /*- 割り当てられた粒子を頭から登録 -*/
        for (int lp = 0; lp < leaveNum; lp++) {
          /*- Histogramから減らしながら追加 -*/
          leavingPartIndex.append(myHistogram[m].takeFirst());
          /*- 残数を減らす -*/
          partLeft--;
          if (partLeft == 0) break;
        }
        if (partLeft == 0) break;
      }

      /*- ひとつも出発出来なければbreak -*/
      if (partLeft == PrePartLeft) break;
    }
    /*- 実際に飛び出せた粒子数 -*/
    actualBirthParticles = leavingPartIndex.size();
  } else /*- 何も刺さっていなければ、ランダムに発生させる -*/
  {
    for (int i = 0; i < actualBirthParticles; i++) leavingPartIndex.append(i);
  }

  /*- 動かす粒子も生まれる粒子も無い -*/
  if (myParticles.empty() && actualBirthParticles == 0) {
    std::cout << "no particles generated nor alive. returning function"
              << std::endl;
    return;
  }

  /*- 背景だけを描画するモードのときは、particlesOriginを更新するだけでOK -*/
  if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_BG) {
    /*- インデックスを小さい順にならべる -*/
    std::sort(leavingPartIndex.begin(), leavingPartIndex.end());
    /*- インデックス大きい方から消していく -*/
    for (int lp = leavingPartIndex.size() - 1; lp >= 0; lp--)
      particleOrigins.removeAt(leavingPartIndex.at(lp));
    return;
  }

  /*- 新規粒子の生成 -*/
  /*- 新規粒子しかない場合 -*/
  if (myParticles.empty() && actualBirthParticles)  // Initial creation
  {
    /*- 新たに作るパーティクルの数だけループ -*/
    for (i = 0; i < actualBirthParticles; i++) {
      /*- 出発する粒子 -*/
      ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

      /*-  どのTextureレベルを使うか -*/
      int seed = (int)((std::numeric_limits<int>::max)() *
                       values.random_val->getFloat());

      /*-  Lifetimeを得る -*/
      int lifetime = 0;
      if (values.column_lifetime_val)
        lifetime = lastframe[po.level];
      else {
        lifetime = (int)(values.lifetime_val.first +
                         ranges.lifetime_range * values.random_val->getFloat());
      }
      /*-
       * この粒子が、レンダリングするフレームでも生きているか判断し、生きているなら生成
       * -*/
      if (lifetime > curr_frame - frame) {
        myParticles.push_back(Iwa_Particle(
            lifetime, seed, porttiles, values, ranges, totalparticles, 0,
            (int)po.level, lastframe[po.level], po.pos[0],
            po.pos[1],               /*- 座標を指定して発生 -*/
            po.isUpward,             /*- orientation を追加 -*/
            (int)po.initSourceFrame) /*- 素材内の初期フレーム位置 -*/
                              );
      }
      totalparticles++;
    }
  }
  /*- 既存粒子を動かし、かつ新規粒子を作る -*/
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
        /*- 出発する粒子 -*/
        ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

        int seed = (int)((std::numeric_limits<int>::max)() *
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
              (int)po.initSourceFrame) /*- 素材内の初期フレーム位置 -*/
                                 );
        }
        totalparticles++;
      }
      break;

    case Iwa_TiledParticlesFx::TOP_RANDOM:
      for (i = 0; i < actualBirthParticles; i++) {
        double tmp = values.random_val->getFloat() * myParticles.size();
        std::list<Iwa_Particle>::iterator it = myParticles.begin();
        for (int j = 0; j < tmp; j++, it++)
          ;
        {
          /*- 出発する粒子 -*/
          ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

          int seed = (int)((std::numeric_limits<int>::max)() *
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
                it,
                Iwa_Particle(
                    lifetime, seed, porttiles, values, ranges, totalparticles,
                    0, (int)po.level, lastframe[po.level], po.pos[0], po.pos[1],
                    po.isUpward,
                    (int)po.initSourceFrame) /*- 素材内の初期フレーム位置 -*/
                );
          }
          totalparticles++;
        }
      }
      break;

    default:
      for (i = 0; i < actualBirthParticles; i++) {
        /*- 出発する粒子 -*/
        ParticleOrigin po = particleOrigins.at(leavingPartIndex.at(i));

        int seed = (int)((std::numeric_limits<int>::max)() *
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
              (int)po.initSourceFrame) /*-  素材内の初期フレーム位置  -*/
                                );
        }
        totalparticles++;
      }
      break;
    }
  }

  /*- すでに発生したparticleOriginを消去する
          インデックスを小さい順にならべる -*/
  std::sort(leavingPartIndex.begin(), leavingPartIndex.end());
  /*- インデックス大きい方から消していく -*/
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
  values.genfadecol_val                    = (values.genfadecol_val) * 0.01;
  values.finfadecol_val                    = (values.finfadecol_val) * 0.01;
  values.foutfadecol_val                   = (values.foutfadecol_val) * 0.01;
  (values.curl_val)                        = (values.curl_val) * dpicorr * 0.1;
  /*- ひらひら粒子に照明を当てる normalize_values()内で Degree → Radian 化する
   * -*/
  (values.iw_light_theta_val) = (values.iw_light_theta_val) * M_PI_180;
  (values.iw_light_phi_val)   = (values.iw_light_phi_val) * M_PI_180;
  /*- 読み込みマージン -*/
  (values.margin_val) = (values.margin_val) * dpicorr;
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::render_particles(
    TTile *tile,                             /*- 結果を格納するTile -*/
    std::vector<TRasterFxPort *> part_ports, /*- テクスチャ素材画像のポート -*/
    const TRenderSettings &ri,
    TDimension
        &p_size,       /*- テクスチャ素材のバウンディングボックスの足し合わさったもの
                          -*/
    TPointD &p_offset, /*- バウンディングボックス左下の座標 -*/
    std::map<int, TRasterFxPort *>
        ctrl_ports, /*- コントロール画像のポート番号／ポート -*/
    std::vector<TLevelP>
        partLevel, /*- テクスチャ素材のリスト -*/
    float dpi,     /*- 1 が入ってくる -*/
    int curr_frame,
    int shrink,                  /*- 1 が入ってくる -*/
    double startx,               /*- 0 が入ってくる -*/
    double starty,               /*- 0 が入ってくる -*/
    double endx,                 /*- 0 が入ってくる -*/
    double endy,                 /*- 0 が入ってくる -*/
    std::vector<int> last_frame, /*- テクスチャ素材のそれぞれのカラム長 -*/
    unsigned long fxId) {
  /*- 各種パーティクルのパラメータ -*/
  struct particles_values values;
  memset(&values, 0, sizeof(values));
  /*- 現在のフレームでの各パラメータを得る -*/
  fill_value_struct(values, m_frame);

  int frame, intpart = 0;

  int level_n = part_ports.size();
  /*- 開始フレーム -*/
  int startframe = (int)values.startpos_val;

  float dpicorr = dpi * 0.01f, fractpart = 0;

  /*- 不透明度の範囲（透明～不透明を 0～1 に正規化） -*/
  float opacity_range =
      (values.opacity_val.second - values.opacity_val.first) * 0.01f;

  bool random_level = false;

  bool isPrecomputingEnabled = false;
  {
    TRenderer renderer(TRenderer::instance());
    isPrecomputingEnabled =
        (renderer && renderer.isPrecomputingEnabled()) ? true : false;
  }

  /*- シュリンクしている場合 -*/
  float dpicorr_shrinked;
  if (values.unit_val == Iwa_TiledParticlesFx::UNIT_SMALL_INCH)
    dpicorr_shrinked = dpicorr / shrink;
  else
    dpicorr_shrinked = dpi / shrink;

  std::map<std::pair<int, int>, float> partScales;

  /*- 現在のフレームをステップ値にする -*/
  curr_frame = curr_frame / values.step_val;

  Iwa_ParticlesManager *pc = Iwa_ParticlesManager::instance();

  bool isFirstFrame = !(pc->isCached(fxId));

  // Retrieve the last rolled frame
  Iwa_ParticlesManager::FrameData *particlesData = pc->data(fxId);

  std::list<Iwa_Particle> myParticles;
  TRandom myRandom  = m_parent->randseed_val->getValue();
  values.random_val = &myRandom;

  int totalparticles = 0;

  /*- 規則正しく並んだ（まだ出発していない）粒子情報 -*/
  QList<ParticleOrigin> particleOrigins;
  /*- 出力画像のバウンディングボックス -*/
  TRectD outTileBBox(tile->m_pos, TDimensionD(tile->getRaster()->getLx(),
                                              tile->getRaster()->getLy()));

  /*- 現在取っておいてあるデータのフレーム番号 -*/
  int pcFrame = particlesData->m_frame;

  /*- マージンをピクセル単位に換算する -*/
  double pixelMargin;
  {
    TPointD vect(values.margin_val, 0.0);
    TAffine aff(ri.m_affine);
    aff.a13 = aff.a23 = 0;
    vect              = aff * vect;
    pixelMargin       = sqrt(vect.x * vect.x + vect.y * vect.y);
  }
  /*- 外側にマージンを取って粒子を生成 -*/
  TRectD resourceTileBBox = outTileBBox.enlarge(pixelMargin);

  /*- 初期粒子量。これが変わっていなければ、BGはそのまま描く -*/
  int initialOriginsSize;
  if (pcFrame > curr_frame) {
    /*- データを初期化 -*/
    // Clear stored particlesData
    particlesData->clear();
    pcFrame = particlesData->m_frame;

    /*- まだ出発していない粒子情報を初期化 -*/
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
    /*- まだ出発していない粒子情報を初期化 -*/
    initParticleOrigins(resourceTileBBox, particleOrigins, curr_frame,
                        ri.m_affine, values, level_n, last_frame, pixelMargin);
    initialOriginsSize = particleOrigins.size();
  }

  /*- スタートからカレントフレームまでループ -*/
  for (frame = startframe - 1; frame <= curr_frame; ++frame) {
    /*-  参照画像はキャッシュされてるフレームでは必要ないのでは？ -*/
    if (frame <= pcFrame) continue;

    int dist_frame = curr_frame - frame;
    /*-
     * ループ内の現在のフレームでのパラメータを取得。スタートが負ならフレーム=0のときの値を格納。
     * -*/
    fill_value_struct(values, frame < 0 ? 0 : frame * values.step_val);
    /*- パラメータの正規化 -*/
    normalize_values(values, ri);
    /*- maxnum_valは"birth_rate"のパラメータ -*/
    intpart = (int)values.maxnum_val;
    /*-
     * birth_rateが小数だったとき、各フレームの小数部分を足しこんだ結果の整数部分をintpartに渡す
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

    /*- 64bitにする -*/
    riAux.m_bpp = 64;
    // riAux.m_bpp = 32;

    int r_frame;  // Useful in case of negative roll frames
    if (frame < 0)
      r_frame = 0;
    else
      r_frame = frame;

    /*- コントロールに刺さっている各ポートについて -*/
    for (std::map<int, TRasterFxPort *>::iterator it = ctrl_ports.begin();
         it != ctrl_ports.end(); ++it) {
      TTile *tmp;
      /*- ポートが接続されていて、Fx内で実際に使用されていたら -*/
      if ((it->second)->isConnected() && port_is_used(it->first, values)) {
        TRectD bbox = resourceTileBBox;

        /*- 素材が存在する場合、portTilesにコントロール画像タイルを格納 -*/
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
            Iwa_TiledParticlesFx::
                REND_ILLUMINATED) /*- 照明モードなら、BG素材は要らない -*/
    {
      std::string alias =
          "BG_CTRL: " +
          (*ctrl_ports.at(values.base_ctrl_val))->getAlias(r_frame, ri);
      TRasterImageP rimg = TImageCache::instance()->get(alias, false);
      if (rimg) {
        baseImgTile.m_pos = tile->m_pos;
        baseImgTile.setRaster(rimg->getRaster());
      } else {
        /*- 出力と同じbpcにする -*/
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
                   intpart /*- 実際に生成したい粒子数 -*/
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
    /*- さしあたり、trailは無視する -*/
    if (dist_frame == 0) {
      //--------
      // Store the maximum particle size before the do_render cycle
      /*- 表示されている粒子のうち、各素材について最大サイズのものを記録しておく
              条件にあわせ、飛んでいる粒子と飛び立つ前の粒子の両方で記録を行う
         -*/
      /*-	①飛んでいる粒子 -*/
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
      /*- ②飛び立つ前の粒子がひとつでもあった場合、最大値でおきかえる -*/
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
      //--------

      /*- ここで、出発した粒子の分、穴を開けた背景を描く -*/
      /*- スイッチがONなら -*/
      if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ALL ||
          values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_BG) {
        /*- まだ粒子が飛び立っていない場合、そのまま背景を描く -*/
        if (initialOriginsSize == particleOrigins.size()) {
          TRop::resample(tile->getRaster(), baseImgTile.getRaster(), TAffine());
        } else {
          renderBackground(tile, particleOrigins, part_ports, ri, partLevel,
                           partScales, &baseImgTile);
        }
      }

      /*- 粒子の描画。もし、背景モードなら描かない -*/
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
      /*- 粒子の描画 ここまで -*/
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
 render_particles から来る
 粒子の数だけ繰り返し
-----------------------------------------------------------------*/

void Iwa_Particles_Engine::do_render(
    Iwa_Particle *part, TTile *tile, std::vector<TRasterFxPort *> part_ports,
    std::map<int, TTile *> porttiles, const TRenderSettings &ri,
    TDimension &p_size, TPointD &p_offset, int lastframe,
    std::vector<TLevelP> partLevel, struct particles_values &values,
    float opacity_range, int dist_frame,
    std::map<std::pair<int, int>, float> &partScales, TTile *baseImgTile) {
  /*- カメラに対してタテになっている粒子を描かずに飛ばす -*/
  if (std::abs(cosf(part->flap_phi * 3.14159f / 180.0f)) < 0.03f) {
    return;
  }
  // Retrieve the particle frame - that is, the *column frame* from which we are
  // picking
  // the particle to be rendered.
  int ndx = part->frame % lastframe;

  TRasterP tileRas(tile->getRaster());

  std::string levelid;
  double aim_angle = 0;
  if (values.pathaim_val) {
    float arctan = atan2f(part->vy, part->vx);
    aim_angle    = arctan * M_180_PI;
  }

  /*- 粒子の回転、スケールをアフィン行列に入れる -*/
  // Calculate the rotational and scale components we have to apply on the
  // particle
  TRotation rotM(part->angle + aim_angle);
  TScale scaleM(part->scale);

  /*- ひらひら -*/
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

    /*- ここで、照明モードのとき、その明るさを計算する -*/
    if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ILLUMINATED) {
      float liTheta  = values.iw_light_theta_val;
      float liPhi    = values.iw_light_phi_val;
      float3 normVec = {sinf(theta) * sinf(phi), cosf(theta) * sinf(phi),
                        cosf(phi)};
      float3 lightVec = {sinf(liTheta) * sinf(liPhi),
                         cosf(liTheta) * sinf(liPhi), cosf(liPhi)};
      /*- 法線ベクトルと光源ベクトルの内積の絶対値 -*/
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
    // infinite bbox
    // are just NOT rendered.

    // NOTE: No fx returns half-planes or similar (ie if any coordinate is
    // either
    // (std::numeric_limits<double>::max)() or its opposite, then the rect IS
    // THE infiniteRectD)
    if (bbox == TConsts::infiniteRectD) return;
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
  /*- 照明モードのとき、その明るさを色に格納 -*/
  if (values.iw_rendermode_val == Iwa_TiledParticlesFx::REND_ILLUMINATED) {
    rfinalpart2 = rfinalpart->clone();
    part->set_illuminated_colors(illuminant, rfinalpart2);
  } else if (baseImgTile->getRaster() && !baseImgTile->getRaster()->isEmpty()) {
    rfinalpart2 = rfinalpart->clone();
    /*- サイズが小さい場合は、単に色を拾う -*/
    if (partResolution.lx <= 4.0 && partResolution.ly <= 4.0)
      part->get_base_image_color(baseImgTile, values, rfinalpart2, bbox, ri);
    else
      part->get_base_image_texture(baseImgTile, values, rfinalpart2, bbox, ri);
  } else
    rfinalpart2 = rfinalpart;

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
    TRop::over(tileRas, rfinalpart2, M);
  else if (TRaster64P myras64 = tile->getRaster())
    TRop::over(tileRas, rfinalpart2, M);
  else {
    throw TException("ParticlesFx: unsupported Pixel Type");
  }
}

/*-----------------------------------------------------------------*/

void Iwa_Particles_Engine::fill_array(
    TTile *ctrl1,     /*- ソース画像のTile -*/
    int &regioncount, /*- 領域数を返す -*/
    std::vector<int>
        &myarray, /*- インデックスを返すと思われる。サイズはソースTileの縦横 -*/
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

void Iwa_Particles_Engine::normalize_array(
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

/*- 入力画像のアルファ値に比例して発生濃度を変える 各Pointにウェイトを持たせる
 * -*/
void Iwa_Particles_Engine::fill_single_region(
    std::vector<TPointD> &myregions, TTile *ctrl1, int threshold,
    bool do_source_gradation, QList<QList<int>> &myHistogram,
    QList<ParticleOrigin> &particleOrigins) {
  assert(ctrl1->getRaster());

  TRaster32P raster32(ctrl1->getRaster()->getSize());
  TRop::convert(raster32, ctrl1->getRaster());
  assert(raster32);  // per ora gestisco solo i Raster32

  myregions.clear();

  raster32->lock();

  /*- 初期化 -*/
  for (int i = 0; i < 256; i++) {
    QList<int> tmpVec;
    myHistogram.push_back(tmpVec);
  }

  if (!do_source_gradation) /*- 1階調の場合 -*/
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
        /*-  Histogramの登録 -*/
        myHistogram[(int)pix->m].push_back(po);
        myregions.push_back(TPointD(particleOrigins.at(po).pos[0],
                                    particleOrigins.at(po).pos[1]));
      }
    }
  }

  raster32->unlock();
}

/*----------------------------------------------------------------
 まだ出発していない粒子情報を初期化
----------------------------------------------------------------*/

static bool potentialLessThan(const ParticleOrigin &po1,
                              const ParticleOrigin &po2) {
  return po1.potential < po2.potential;
}

void Iwa_Particles_Engine::initParticleOrigins(
    TRectD &resourceTileBBox, QList<ParticleOrigin> &particleOrigins,
    const double frame, const TAffine affine, struct particles_values &values,
    int level_n, std::vector<int> &lastframe, /*- 素材カラムのフレーム長 -*/
    double pixelMargin) {
  /*- 敷き詰め三角形の一辺の長さをピクセル単位に換算する -*/
  TPointD vect(values.iw_triangleSize, 0.0);
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0;
  vect              = aff * vect;
  double triPixSize = sqrt(vect.x * vect.x + vect.y * vect.y);

  double pix2Inch = values.iw_triangleSize / triPixSize;

  /*- 横方向の移動距離 -*/
  double d_hori = values.iw_triangleSize * 0.5;
  /*- 縦方向の移動距離 -*/
  double d_vert = values.iw_triangleSize * 0.8660254;
  /*- 正三角形を横に上下交互に並べたときの、中心位置のオフセット -*/
  double vOffset = values.iw_triangleSize * 0.14433758;

  /*- ピクセル位置の方も格納する -*/
  double d_hori_pix  = triPixSize * 0.5;
  double d_vert_pix  = triPixSize * 0.8660254;
  double vOffset_pix = triPixSize * 0.14433758;

  TRectD inchBBox(
      resourceTileBBox.x0 * pix2Inch, resourceTileBBox.y0 * pix2Inch,
      resourceTileBBox.x1 * pix2Inch, resourceTileBBox.y1 * pix2Inch);
  /*- インチ位置のスタート -*/
  double curr_y = inchBBox.y0;
  /*- 行の1列目のタテのオフセット値 -*/
  double startOff = -vOffset;

  /*- ピクセル位置のスタート -*/
  double curr_y_pix   = 0.0;
  double startOff_pix = -vOffset_pix;

  /*- メモリの見積もり -*/
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

  /*- タテでループ -*/
  while (curr_y <=
         inchBBox.y1 +
             d_vert *
                 0.5) /* ←d_vert * 0.5 は最後の一行を敷き詰めるための追加分 -*/
  {
    double curr_x = inchBBox.x0;
    double off    = startOff;
    /*- 三角形が上下どっちを向いているかのフラグ -*/
    bool isUp = (off < 0);

    /*- ピクセル位置のスタート -*/
    double curr_x_pix = 0.0;
    double off_pix    = startOff_pix;

    /*- ヨコでループ -*/
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

  /*- 粒子をランダム値の大きい順に並べる -*/
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
 ここで、出発した粒子の分、穴を開けた背景を描く
--------------------------------------------------*/
void Iwa_Particles_Engine::renderBackground(
    TTile *tile, QList<ParticleOrigin> &origins,
    std::vector<TRasterFxPort *> part_ports, const TRenderSettings &ri,
    std::vector<TLevelP> partLevel,
    std::map<std::pair<int, int>, float> &partScales, TTile *baseImgTile) {
  TRasterP tileRas(tile->getRaster());
  int unit = 1 + (int)origins.size() / 100;

  /*- まだ残っている粒子源について -*/
  for (int po = 0; po < origins.size(); po++) {
    ParticleOrigin origin = origins.at(po);

    int ndx = origin.initSourceFrame;

    /*- 粒子の回転、スケール -*/
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
    /*- 縮小済みのParticleのサイズ -*/
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
  /*- サイズ縮める操作をいれる -*/
  TRasterP resizedBGRas;
  if (TRaster32P myBgRas32 = baseImgTile->getRaster())
    resizedBGRas = TRaster32P(tileRas->getSize());
  else if (TRaster64P myBgRas64 = baseImgTile->getRaster())
    resizedBGRas = TRaster64P(tileRas->getSize());
  else
    return;
  TAffine aff;
  /*- リサンプル -*/
  TRop::resample(resizedBGRas, baseImgTile->getRaster(), aff);

  TRop::ropin(resizedBGRas, tileRas, tileRas);

  std::cout << std::endl;
}
