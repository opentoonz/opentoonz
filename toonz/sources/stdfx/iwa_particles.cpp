//------------------------------------------------------------------
// Iwa_Particle for Marnie
// based on ParticlesFx by Digital Video
//------------------------------------------------------------------

#include "tfxparam.h"
#include "trop.h"
#include "iwa_particles.h"
#include "iwa_particlesengine.h"
#include "hsvutil.h"
#include "timage_io.h"
#include "tofflinegl.h"

/*-----------------------------------------------------------------*/

void Iwa_Particle::create_Animation(const particles_values &values, int first,
                                    int last) {
  switch (values.animation_val) {
  case Iwa_TiledParticlesFx::ANIM_CYCLE:
  case Iwa_TiledParticlesFx::ANIM_S_CYCLE:
    animswing = 0; /*frame <0 perche' c'e' il preroll dialmeno un frame*/
    break;
  case Iwa_TiledParticlesFx::ANIM_SR_CYCLE:
    animswing = random.getFloat() > 0.5 ? 1 : 0;
    break;
  }
}

//------------------------------------------------------------------

Iwa_Particle::Iwa_Particle(
    int g_lifetime, int seed, std::map<int, TTile *> porttiles,
    const particles_values &values, const particles_ranges &ranges, int howmany,
    int first, int level, int last, float posx, float posy,
    bool isUpward,       /*- 初期向き -*/
    int initSourceFrame) /*- Level内の初期フレーム位置 -*/
{
  double random_s_a_range, random_speed;
  std::map<int, float> imagereferences;
  random                 = TRandom(seed);
  float randomxreference = 0.0;
  float randomyreference = 0.0;

  /*- ここで、出発時の素材フレームが決定するので、ParticleOriginに渡す -*/
  create_Animation(values, 0, last);
  frame = initSourceFrame;

  this->level = level;

  x = posx;
  y = posy;

  /*- 粒子パラメータに参照画像が使われている場合、
          参照画像のとる値を先にまとめて得ておく -*/
  for (std::map<int, TTile *>::iterator it = porttiles.begin();
       it != porttiles.end(); ++it) {
    if ((values.lifetime_ctrl_val == it->first ||
         values.speed_ctrl_val == it->first ||
         values.scale_ctrl_val == it->first || values.rot_ctrl_val == it->first
         /*- Speed Angleを明るさでコントロールする場合 -*/
         || (values.speeda_ctrl_val == it->first &&
             !values.speeda_use_gradient_val)) &&
        it->second->getRaster()) {
      float tmpvalue;
      get_image_reference(it->second, values, tmpvalue,
                          Iwa_TiledParticlesFx::GRAY_REF);
      imagereferences[it->first] = tmpvalue;
    }
  }

  /*- 寿命にControlが刺さっている場合は、その値に合わせて寿命を減ずる -*/
  if (values.lifetime_ctrl_val) {
    float lifetimereference = 0.0;
    lifetimereference       = imagereferences[values.lifetime_ctrl_val];
    lifetime                = g_lifetime * lifetimereference;
  } else
    lifetime = g_lifetime;

  genlifetime = lifetime;

  /*- 粒子の初期速度を得る -*/
  if (values.speed_ctrl_val &&
      (porttiles.find(values.speed_ctrl_val) != porttiles.end())) {
    float speedreference = 0.0;
    speedreference       = imagereferences[values.speed_ctrl_val];
    random_speed =
        values.speed_val.first + (ranges.speed_range) * speedreference;
  } else
    random_speed =
        values.speed_val.first + (ranges.speed_range) * random.getFloat();

  /*- 粒子の初期移動方向を得る -*/
  if (values.speeda_ctrl_val &&
      (porttiles.find(values.speeda_ctrl_val) != porttiles.end())) {
    if (values.speeda_use_gradient_val) {
      /*- 参照画像のGradientを得る関数を利用して角度を得るモード -*/
      float dir_x, dir_y;
      get_image_gravity(porttiles[values.speeda_ctrl_val], values, dir_x,
                        dir_y);
      if (dir_x == 0.0f && dir_y == 0.0f)
        random_s_a_range = values.speed_val.first;
      else
        random_s_a_range = atan2f(dir_x, -dir_y);
    } else {
      float speedareference = 0.0;
      speedareference       = imagereferences[values.speeda_ctrl_val];
      random_s_a_range =
          values.speeda_val.first + (ranges.speeda_range) * speedareference;
    }
  } else /*- Controlが無ければランダム -*/
    random_s_a_range =
        values.speeda_val.first + (ranges.speeda_range) * random.getFloat();

  /*- 移動方向から速度のX,Y成分を得る -*/
  vx = random_speed * sin(random_s_a_range);
  vy = -random_speed * cos(random_s_a_range);

  trail =
      (int)(values.trail_val.first + (ranges.trail_range) * random.getFloat());
  oldx = 0;
  oldy = 0;
  /*- 質量を指定（動きにくさ） -*/
  mass = values.mass_val.first + (ranges.mass_range) * random.getFloat();
  /*- サイズを得る -*/
  if (values.scale_ctrl_val &&
      (porttiles.find(values.scale_ctrl_val) != porttiles.end())) {
    float scalereference = 0.0f;
    scalereference       = imagereferences[values.scale_ctrl_val];
    scale = values.scale_val.first + (ranges.scale_range) * scalereference;
  } else {
    /*- ONのとき、かつ、ScaleにControlが無い場合、
            粒子サイズが小さいほど(遠くにあるので)多く分布するようになる。 -*/
    if (values.perspective_distribution_val) {
      scale =
          (values.scale_val.first * values.scale_val.second) /
          (values.scale_val.second - (ranges.scale_range) * random.getFloat());
    } else
      scale = values.scale_val.first + (ranges.scale_range) * random.getFloat();
  }

  /*- 向きを得る -*/
  if (values.rot_ctrl_val &&
      (porttiles.find(values.rot_ctrl_val) != porttiles.end())) {
    float anglereference = 0.0f;
    anglereference       = imagereferences[values.rot_ctrl_val];
    angle = -(values.rot_val.first) - (ranges.rot_range) * anglereference;
  } else
    angle = -(values.rot_val.first) - (ranges.rot_range) * random.getFloat();

  // 140212 初期粒子向き
  if (!isUpward) angle += 180.0f;

  /*- ランダムな動きのふれはば -*/
  if (values.randomx_ctrl_val)
    randomxreference = imagereferences[values.randomx_ctrl_val];
  if (values.randomy_ctrl_val)
    randomyreference = imagereferences[values.randomy_ctrl_val];
  create_Swing(values, ranges, randomxreference, randomyreference);

  create_Colors(values, ranges, porttiles);

  if (scale < 0.001f) scale = 0;

  /*- ひらひらした動き -*/
  if (values.flap_ctrl_val &&
      (porttiles.find(values.flap_ctrl_val) != porttiles.end())) {
    //*- 参照画像のGradientを得る関数を利用して角度を得る -*/
    float dir_x, dir_y;
    float norm;
    norm = get_image_gravity(porttiles[values.flap_ctrl_val], values, dir_x,
                             dir_y);
    if (dir_x == 0.0f && dir_y == 0.0f) {
      flap_theta = 0.0f;
      flap_phi   = 0.0f;
    } else {
      flap_theta = atan2f(dir_y, dir_x) * 180.0f / 3.14159f;
      flap_phi   = values.iw_flap_velocity_val * norm / mass;
    }
  } else {
    flap_theta = 0.0f;
    flap_phi   = 0.0f;
  }

  initial_x     = x;
  initial_y     = y;
  initial_angle = angle;
  initial_scale = scale;

  curlx = 0.0f;
  curly = 0.0f;
  curlz = random.getFloat();
}

//------------------------------------------------------------------

int Iwa_Particle::check_Swing(const particles_values &values) {
  return (values.randomx_val.first || values.randomx_val.second ||
          values.randomy_val.first || values.randomy_val.second ||
          values.rotsca_val.first || values.rotsca_val.second);
}

/*-----------------------------------------------------------------*/

void Iwa_Particle::create_Swing(const particles_values &values,
                                const particles_ranges &ranges,
                                double randomxreference,
                                double randomyreference) {
  changesignx =
      (int)(values.swing_val.first + random.getFloat() * (ranges.swing_range));
  changesigny =
      (int)(values.swing_val.first + random.getFloat() * (ranges.swing_range));
  changesigna = (int)(values.rotswing_val.first +
                      random.getFloat() * (ranges.rotswing_range));
  if (values.swingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
    if (values.randomx_ctrl_val)
      smswingx = abs((int)values.randomx_val.first) +
                 randomxreference * (ranges.randomx_range);
    else
      smswingx = abs((int)values.randomx_val.first) +
                 random.getFloat() * (ranges.randomx_range);
    if (values.randomy_ctrl_val)
      smswingy = abs((int)values.randomy_val.first) +
                 randomyreference * (ranges.randomy_range);
    else
      smswingy = abs((int)values.randomy_val.first) +
                 random.getFloat() * (ranges.randomy_range);
    smperiodx = changesignx;
    smperiody = changesigny;
  }
  if (values.rotswingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
    smswinga = abs((int)(values.rotsca_val.first +
                         random.getFloat() * (ranges.rotsca_range)));
    smperioda = changesigna;
  }
  signx = random.getBool() ? 1 : -1;
  signy = random.getBool() ? 1 : -1;
  signa = random.getBool() ? 1 : -1;
}

/*-----------------------------------------------------------------*/

void Iwa_Particle::create_Colors(const particles_values &values,
                                 const particles_ranges &ranges,
                                 std::map<int, TTile *> porttiles) {
  if (values.genfadecol_val) {
    TPixel32 color;
    if (values.gencol_ctrl_val &&
        (porttiles.find(values.gencol_ctrl_val) != porttiles.end()))
      get_image_reference(porttiles[values.gencol_ctrl_val], values, color);
    else
      color        = values.gencol_val.getPremultipliedValue(random.getFloat());
    gencol.fadecol = values.genfadecol_val;
    if (values.gencol_spread_val) spread_color(color, values.gencol_spread_val);
    gencol.col = color;
  } else {
    gencol.col     = TPixel32::Transparent;
    gencol.fadecol = 0;
  }

  if (values.finfadecol_val) {
    TPixel32 color;
    if (values.fincol_ctrl_val &&
        (porttiles.find(values.fincol_ctrl_val) != porttiles.end()))
      get_image_reference(porttiles[values.fincol_ctrl_val], values, color);
    else
      color = values.fincol_val.getPremultipliedValue(random.getFloat());
    fincol.rangecol = (int)values.finrangecol_val;
    fincol.fadecol  = values.finfadecol_val;
    if (values.fincol_spread_val) spread_color(color, values.fincol_spread_val);
    fincol.col = color;
  } else {
    fincol.col      = TPixel32::Transparent;
    fincol.rangecol = 0;
    fincol.fadecol  = 0;
  }

  if (values.foutfadecol_val) {
    TPixel32 color;
    if (values.foutcol_ctrl_val &&
        (porttiles.find(values.foutcol_ctrl_val) != porttiles.end()))
      get_image_reference(porttiles[values.foutcol_ctrl_val], values, color);
    else
      color = values.foutcol_val.getPremultipliedValue(random.getFloat());
    ;
    foutcol.rangecol = (int)values.foutrangecol_val;
    foutcol.fadecol  = values.foutfadecol_val;
    if (values.foutcol_spread_val)
      spread_color(color, values.foutcol_spread_val);
    foutcol.col = color;
  } else {
    foutcol.col      = TPixel32::Transparent;
    foutcol.rangecol = 0;
    foutcol.fadecol  = 0;
  }
}

/*-----------------------------------------------------------------*/

/*- modify_colors_and_opacityから呼ばれる。
        lifetimeが粒子の現在の年齢。 gencol/fincol/foutcolから色を決める -*/
void Iwa_Particle::modify_colors(TPixel32 &color, double &intensity) {
  float percent = 0;

  if ((gencol.fadecol || fincol.fadecol) &&
      (genlifetime - lifetime) <= fincol.rangecol) {
    if (fincol.rangecol)
      percent = (genlifetime - lifetime) / (float)(fincol.rangecol);
    color     = blend(gencol.col, fincol.col, percent);
    intensity = gencol.fadecol + percent * (fincol.fadecol - gencol.fadecol);
  } else if (foutcol.fadecol && lifetime <= foutcol.rangecol) {
    if (foutcol.rangecol)
      percent = 1 - (lifetime - 1) / (float)(foutcol.rangecol);
    if (fincol.rangecol && fincol.fadecol) {
      color     = blend(fincol.col, foutcol.col, percent);
      intensity = fincol.fadecol + percent * (foutcol.fadecol - fincol.fadecol);
    } else {
      color     = blend(gencol.col, foutcol.col, percent);
      intensity = gencol.fadecol + percent * (foutcol.fadecol - gencol.fadecol);
    }
  } else {
    if (fincol.fadecol && fincol.rangecol) {
      color     = fincol.col;
      intensity = fincol.fadecol;
    } else {
      color     = gencol.col;
      intensity = gencol.fadecol;
    }
  }
}

/*-----------------------------------------------------------------*/

/*- do_render から呼ばれる。各粒子の描画の直前に色を決めるところ -*/
void Iwa_Particle::modify_colors_and_opacity(const particles_values &values,
                                             float curr_opacity, int dist_frame,
                                             TRaster32P raster32) {
  double intensity = 0;
  TPixel32 col;
  if (gencol.fadecol || fincol.fadecol || foutcol.fadecol) {
    modify_colors(col, intensity);
    int j;
    raster32->lock();
    for (j = 0; j < raster32->getLy(); j++) {
      TPixel32 *pix    = raster32->pixels(j);
      TPixel32 *endPix = pix + raster32->getLx();
      while (pix < endPix) {
        double factor = pix->m / 255.0;
        pix->r        = (UCHAR)(pix->r + intensity * (factor * col.r - pix->r));
        pix->g        = (UCHAR)(pix->g + intensity * (factor * col.g - pix->g));
        pix->b        = (UCHAR)(pix->b + intensity * (factor * col.b - pix->b));
        pix->m        = (UCHAR)(pix->m + intensity * (factor * col.m - pix->m));
        ++pix;
      }
    }
    raster32->unlock();
  }

  if (curr_opacity != 1.0)
    TRop::rgbmScale(raster32, raster32, 1, 1, 1, curr_opacity);
}

/*-----------------------------------------------------------------*/
void Iwa_Particle::update_Animation(const particles_values &values, int first,
                                    int last, int keep) {
  switch (values.animation_val) {
  case Iwa_TiledParticlesFx::ANIM_RANDOM:
    frame = (int)(first + random.getFloat() * (last - first));
    break;
  case Iwa_TiledParticlesFx::ANIM_R_CYCLE:
  case Iwa_TiledParticlesFx::ANIM_CYCLE:
    if (!keep || frame != keep - 1)
      frame = first + (frame + 1) % (last - first);
    break;
  case Iwa_TiledParticlesFx::ANIM_S_CYCLE:
  case Iwa_TiledParticlesFx::ANIM_SR_CYCLE:
    if (!keep || frame != keep - 1) {
      if (!animswing && frame < last - 1) {
        frame                            = (frame + 1);
        if (frame == last - 1) animswing = 1;
      } else
        frame = (frame - 1);
      if (frame <= first) {
        animswing = 0;
        frame     = first;
      }
    }
    break;
  }
}

/*-----------------------------------------------------------------*/
void Iwa_Particle::update_Swing(const particles_values &values,
                                const particles_ranges &ranges,
                                struct pos_dummy &dummy,
                                double randomxreference,
                                double randomyreference) {
  if (values.swingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
    if (smperiodx)
      dummy.x =
          smswingx * randomxreference * sin((M_PI * changesignx) / smperiodx);
    else
      dummy.x = 0;
    if (smperiody)
      dummy.y =
          smswingy * randomyreference * sin((M_PI * changesigny) / smperiody);
    else
      dummy.y = 0;
  } else {
    if (values.randomx_ctrl_val)
      dummy.x = (values.randomx_val.first +
                 (ranges.randomx_range) * randomxreference);
    else
      dummy.x = (values.randomx_val.first +
                 (ranges.randomx_range) * random.getFloat());
    if (values.randomy_ctrl_val)
      dummy.y = (values.randomy_val.first +
                 (ranges.randomy_range) * randomyreference);
    else
      dummy.y = (values.randomy_val.first +
                 (ranges.randomy_range) * random.getFloat());
  }

  if (values.rotswingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
    if (smperioda)
      dummy.a = smswinga * sin((M_PI * changesigna) / smperioda);
    else
      dummy.a = 0;
  } else
    dummy.a =
        (values.rotsca_val.first + (ranges.rotsca_range) * random.getFloat());

  if (!(genlifetime - lifetime)) {
    signx = dummy.x > 0 ? 1 : -1;
    signy = dummy.y > 0 ? 1 : -1;
    signa = dummy.a > 0 ? 1 : -1;
  } else {
    dummy.x = (fabs(dummy.x)) * signx;
    dummy.y = (fabs(dummy.y)) * signy;
    dummy.a = (fabs(dummy.a)) * signa;
  }

  changesignx--;
  changesigny--;
  changesigna--;
  if (changesignx <= 0) {
    //  if(random->getFloat()<0.5);
    signx *= -1;
    changesignx = abs((int)(values.swing_val.first) +
                      (int)(random.getFloat() * (ranges.swing_range)));
    if (values.swingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
      smperiodx = changesignx;
      if (values.randomx_ctrl_val)
        smswingx = values.randomx_val.first +
                   randomxreference * (ranges.randomx_range);
      else
        smswingx = values.randomx_val.first +
                   random.getFloat() * (ranges.randomx_range);
    }
  }
  if (changesigny <= 0) {
    //  if(random->getFloat()<0.5);
    signy *= -1;
    changesigny = abs((int)(values.swing_val.first) +
                      (int)(random.getFloat() * (ranges.swing_range)));
    if (values.swingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
      smperiody = changesigny;
      if (values.randomy_ctrl_val)
        smswingy = values.randomy_val.first +
                   randomyreference * (ranges.randomy_range);
      else
        smswingy = values.randomy_val.first +
                   random.getFloat() * (ranges.randomy_range);
    }
  }
  if (changesigna <= 0) {
    signa *= -1;
    changesigna = abs((int)(values.rotswing_val.first) +
                      (int)(random.getFloat() * (ranges.rotswing_range)));
    if (values.rotswingmode_val == Iwa_TiledParticlesFx::SWING_SMOOTH) {
      smperioda = changesigna;
      smswinga =
          values.rotsca_val.first + random.getFloat() * (ranges.rotsca_range);
    }
  }
}
/*-----------------------------------------------------------------*/
void Iwa_Particle::update_Scale(const particles_values &values,
                                const particles_ranges &ranges,
                                double scalereference,
                                double scalestepreference) {
  double scalestep;

  if (values.scale_ctrl_val && values.scale_ctrl_all_val)
    scale = values.scale_val.first + (ranges.scale_range) * scalereference;
  else {
    if (values.scalestep_ctrl_val)
      scalestep = values.scalestep_val.first +
                  (ranges.scalestep_range) * scalestepreference;
    else
      scalestep = values.scalestep_val.first +
                  (ranges.scalestep_range) * random.getFloat();
    if (scalestep) scale += scalestep;
  }
  if (scale < 0.001) scale = 0;
}

/*-----------------------------------------------------------------*/
void Iwa_Particle::get_image_reference(TTile *ctrl,
                                       const particles_values &values,
                                       float &imagereference, int type) {
  TRaster32P raster32 = ctrl->getRaster();
  TRaster64P raster64 = ctrl->getRaster();
  TPointD tmp(x, y);
  tmp -= ctrl->m_pos;
  imagereference = 0.0;

  if (raster32)
    raster32->lock();
  else if (raster64)
    raster64->lock();

  switch (type) {
  case Iwa_TiledParticlesFx::GRAY_REF:
    if (raster32 && tmp.x >= 0 && tmp.x < raster32->getLx() && tmp.y >= 0 &&
        troundp(tmp.y) < raster32->getLy()) {
      TPixel32 pix = raster32->pixels(troundp(tmp.y))[(int)tmp.x];
      imagereference =
          TPixelGR8::from(pix).value / (float)TPixelGR8::maxChannelValue;
    } else if (raster64 && tmp.x >= 0 && tmp.x < raster64->getLx() &&
               tmp.y >= 0 && troundp(tmp.y) < raster64->getLy()) {
      TPixel64 pix = raster64->pixels(troundp(tmp.y))[(int)tmp.x];
      imagereference =
          TPixelGR16::from(pix).value / (float)TPixelGR16::maxChannelValue;
    }
    break;
  case Iwa_TiledParticlesFx::H_REF:
    if (raster32 && tmp.x >= 0 && tmp.x < raster32->getLx() && tmp.y >= 0 &&
        tround(tmp.y) < raster32->getLy()) {
      float aux = (float)TPixel32::maxChannelValue;
      double h, s, v;
      TPixel32 pix = raster32->pixels(troundp(tmp.y))[(int)tmp.x];
      OLDRGB2HSV(pix.r / aux, pix.g / aux, pix.b / aux, &h, &s, &v);
      imagereference = (float)h / 360.0f;
    } else if (raster64 && tmp.x >= 0 && tmp.x < raster64->getLx() &&
               tmp.y >= 0 && tround(tmp.y) < raster64->getLy()) {
      float aux = (float)TPixel64::maxChannelValue;
      double h, s, v;
      TPixel64 pix = raster64->pixels(troundp(tmp.y))[(int)tmp.x];
      OLDRGB2HSV(pix.r / aux, pix.g / aux, pix.b / aux, &h, &s, &v);
      imagereference = (float)h / 360.0f;
    }
    break;
  }

  if (raster32)
    raster32->unlock();
  else if (raster64)
    raster64->unlock();
}

/*-----------------------------------------------------------------*/
/*- ベクタ長を返す -*/
float Iwa_Particle::get_image_gravity(TTile *ctrl1,
                                      const particles_values &values, float &gx,
                                      float &gy) {
  TRaster32P raster32 = ctrl1->getRaster();
  TRaster64P raster64 = ctrl1->getRaster();
  TPointD tmp(x, y);
  tmp -= ctrl1->m_pos;
  int radius = 2;
  gx         = 0;
  gy         = 0;

  float norm = 1.0f;

  if (raster32) {
    raster32->lock();
    if (raster32 && tmp.x >= radius && tmp.x < raster32->getLx() - radius &&
        tmp.y >= radius && tmp.y < raster32->getLy() - radius) {
      TPixel32 *pix = &(raster32->pixels(troundp(tmp.y))[(int)tmp.x]);

      gx += 2 * TPixelGR8::from(*(pix + 1)).value;
      gx += TPixelGR8::from(*(pix + 1 + raster32->getWrap() * 1)).value;
      gx += TPixelGR8::from(*(pix + 1 - raster32->getWrap() * 1)).value;

      gx -= 2 * TPixelGR8::from(*(pix - 1)).value;
      gx -= TPixelGR8::from(*(pix - 1 + raster32->getWrap() * 1)).value;
      gx -= TPixelGR8::from(*(pix - 1 - raster32->getWrap() * 1)).value;

      gy += 2 * TPixelGR8::from(*(pix + raster32->getWrap() * 1)).value;
      gy += TPixelGR8::from(*(pix + raster32->getWrap() * 1 + 1)).value;
      gy += TPixelGR8::from(*(pix + raster32->getWrap() * 1 - 1)).value;

      gy -= 2 * TPixelGR8::from(*(pix - raster32->getWrap() * 1)).value;
      gy -= TPixelGR8::from(*(pix - raster32->getWrap() * 1 + 1)).value;
      gy -= TPixelGR8::from(*(pix - raster32->getWrap() * 1 - 1)).value;

      norm = sqrtf(gx * gx + gy * gy);
      if (norm) {
        float inorm = 0.1f / norm;
        gx          = gx * inorm;
        gy          = gy * inorm;
      }
    }
    raster32->unlock();

    norm /= (float)(TPixelGR8::maxChannelValue);

  } else if (raster64) {
    raster64->lock();
    if (raster64 && tmp.x >= radius && tmp.x < raster64->getLx() - radius &&
        tmp.y >= radius && tmp.y < raster64->getLy() - radius) {
      TPixel64 *pix = &(raster64->pixels(troundp(tmp.y))[(int)tmp.x]);

      gx += 2 * TPixelGR16::from(*(pix + 1)).value;
      gx += TPixelGR16::from(*(pix + 1 + raster64->getWrap() * 1)).value;
      gx += TPixelGR16::from(*(pix + 1 - raster64->getWrap() * 1)).value;

      gx -= 2 * TPixelGR16::from(*(pix - 1)).value;
      gx -= TPixelGR16::from(*(pix - 1 + raster64->getWrap() * 1)).value;
      gx -= TPixelGR16::from(*(pix - 1 - raster64->getWrap() * 1)).value;

      gy += 2 * TPixelGR16::from(*(pix + raster64->getWrap() * 1)).value;
      gy += TPixelGR16::from(*(pix + raster64->getWrap() * 1 + 1)).value;
      gy += TPixelGR16::from(*(pix + raster64->getWrap() * 1 - 1)).value;

      gy -= 2 * TPixelGR16::from(*(pix - raster64->getWrap() * 1)).value;
      gy -= TPixelGR16::from(*(pix - raster64->getWrap() * 1 + 1)).value;
      gy -= TPixelGR16::from(*(pix - raster64->getWrap() * 1 - 1)).value;

      norm = sqrtf(gx * gx + gy * gy);
      if (norm) {
        float inorm = 0.1 / norm;
        gx          = gx * inorm;
        gy          = gy * inorm;
      }
    }

    raster64->unlock();
    norm /= (float)(TPixelGR16::maxChannelValue);
  }
  return norm;
}

/*-----------------------------------------------------------------*/

bool Iwa_Particle::get_image_curl(TTile *ctrl1, const particles_values &values,
                                  float &cx, float &cy) {
  TRaster32P raster32 = ctrl1->getRaster();
  TRaster64P raster64 = ctrl1->getRaster();
  TPointD tmp(x, y);
  tmp -= ctrl1->m_pos;
  int radius = 4;
  cx         = 0;
  cy         = 0;

  bool ret;

  if (raster32) {
    raster32->lock();
    if (raster32 && tmp.x >= radius + 1 &&
        tmp.x < raster32->getLx() - radius - 1 && tmp.y >= radius + 1 &&
        tmp.y < raster32->getLy() - radius - 1) {
      TPixel32 *pix = &(raster32->pixels(troundp(tmp.y))[(int)tmp.x]);

      cx = -TPixelGR8::from(*(pix + radius * raster32->getWrap())).value +
           TPixelGR8::from(*(pix - radius * raster32->getWrap())).value;
      cy = TPixelGR8::from(*(pix + radius)).value -
           TPixelGR8::from(*(pix - radius)).value;
      ret = true;
    } else
      ret = false;
    raster32->unlock();
  } else if (raster64) {
    raster64->lock();
    if (raster64 && tmp.x >= radius + 1 &&
        tmp.x < raster64->getLx() - radius - 1 && tmp.y >= radius + 1 &&
        tmp.y < raster64->getLy() - radius - 1) {
      TPixel64 *pix = &(raster64->pixels(troundp(tmp.y))[(int)tmp.x]);

      cx = -TPixelGR16::from(*(pix + radius * raster64->getWrap())).value +
           TPixelGR16::from(*(pix - radius * raster64->getWrap())).value;
      cy = TPixelGR16::from(*(pix + radius)).value -
           TPixelGR16::from(*(pix - radius)).value;
      cx /= 256.0f;
      cy /= 256.0f;
      ret = true;
    } else
      ret = false;
    raster64->unlock();
  }
  return ret;
}

/*-----------------------------------------------------------------*/

void Iwa_Particle::get_image_reference(TTile *ctrl1,
                                       const particles_values &values,
                                       TPixel32 &color) {
  TRaster32P raster32 = ctrl1->getRaster();
  TRaster64P raster64 = ctrl1->getRaster();

  TPointD tmp(x, y);
  tmp -= ctrl1->m_pos;

  if (raster32 && tmp.x >= 0 && tmp.x < raster32->getLx() && tmp.y >= 0 &&
      troundp(tmp.y) < raster32->getLy()) {
    color = raster32->pixels(troundp(tmp.y))[(int)tmp.x];
  } else if (raster64 && tmp.x >= 0 && tmp.x < raster64->getLx() &&
             tmp.y >= 0 && troundp(tmp.y) < raster64->getLy()) {
    TPixel64 pix64 = raster64->pixels(troundp(tmp.y))[(int)tmp.x];
    color = TPixel32((int)pix64.r / 256, (int)pix64.g / 256, (int)pix64.b / 256,
                     (int)pix64.m / 256);
  }
  /*- 参照画像のBBoxの外側で、粒子の色を透明にする -*/
  else
    color = TPixel32::Transparent;
}

/*-----------------------------------------------------------------*/

void Iwa_Particle::get_base_image_texture(TTile *ctrl1,
                                          const particles_values &values,
                                          TRasterP texRaster,
                                          const TRectD &texBBox,
                                          const TRenderSettings &ri) {
  /*- ベースの絵を粒子の初期位置にあわせ移動させる -*/
  TPointD pos(initial_x, initial_y);
  pos = ri.m_affine * pos;
  TRotation rotM(-initial_angle);
  TScale scaleM(1.0);
  TAffine M = TTranslation(-texBBox.getP00()) * scaleM * rotM *
              TTranslation(-pos + ctrl1->m_pos);

  TRaster32P baseRas32 = ctrl1->getRaster();
  TRaster64P baseRas64 = ctrl1->getRaster();
  if (baseRas32) {
    baseRas32->lock();

    /*- 粒子と同サイズのラスタを用意 -*/
    TRaster32P kirinukiBaseRas(texRaster->getSize());
    kirinukiBaseRas->lock();
    TRaster32P texRas32 = texRaster;
    texRas32->lock();
    /*- そこにquickput -*/
    TRop::quickPut(kirinukiBaseRas, baseRas32, M);

    /*- 粒子のRGBをベース絵で置換していく -*/
    for (int j = 0; j < texRaster->getLy(); j++) {
      TPixel32 *pix     = texRas32->pixels(j);
      TPixel32 *endPix  = pix + texRas32->getLx();
      TPixel32 *basePix = kirinukiBaseRas->pixels(j);
      while (pix < endPix) {
        double factor = pix->m / 255.0;
        pix->r        = (UCHAR)(factor * basePix->r);
        pix->g        = (UCHAR)(factor * basePix->g);
        pix->b        = (UCHAR)(factor * basePix->b);
        pix->m        = (UCHAR)(factor * basePix->m);

        ++pix;
        ++basePix;
      }
    }

    kirinukiBaseRas->unlock();
    baseRas32->unlock();
    texRas32->unlock();
  } else if (baseRas64) {
    baseRas64->lock();

    /*- 粒子と同サイズのラスタを用意 -*/
    TRaster64P kirinukiBaseRas(texRaster->getSize());
    kirinukiBaseRas->lock();
    TRaster64P texRas64 = texRaster;
    texRas64->lock();
    /*- そこにquickput -*/
    TRop::quickPut(kirinukiBaseRas, baseRas64, M);

    /*- 粒子のRGBをベース絵で置換していく -*/
    for (int j = 0; j < texRaster->getLy(); j++) {
      TPixel64 *pix     = texRas64->pixels(j);
      TPixel64 *endPix  = pix + texRas64->getLx();
      TPixel64 *basePix = kirinukiBaseRas->pixels(j);
      while (pix < endPix) {
        double factor = (double)pix->m / (double)TPixel64::maxChannelValue;
        pix->r        = (TPixel64::Channel)(factor * basePix->r);
        pix->g        = (TPixel64::Channel)(factor * basePix->g);
        pix->b        = (TPixel64::Channel)(factor * basePix->b);
        pix->m        = (TPixel64::Channel)(factor * basePix->m);

        ++pix;
        ++basePix;
      }
    }

    kirinukiBaseRas->unlock();
    baseRas64->unlock();
    texRas64->unlock();
  }
}

/*-----------------------------------------------------------------*/

void Iwa_Particle::get_base_image_color(TTile *ctrl1,
                                        const particles_values &values,
                                        TRasterP texRaster,
                                        const TRectD &texBBox,
                                        const TRenderSettings &ri) {
  /*- まず、色を拾う -*/
  TPixel32 color;
  TRaster32P baseRas32 = ctrl1->getRaster();
  TRaster64P baseRas64 = ctrl1->getRaster();

  TPointD tmp(initial_x, initial_y);

  tmp = ri.m_affine * tmp;

  tmp -= ctrl1->m_pos;
  if (baseRas32 && tmp.x >= 0 && tmp.x < baseRas32->getLx() && tmp.y >= 0 &&
      troundp(tmp.y) < baseRas32->getLy()) {
    color = baseRas32->pixels(troundp(tmp.y))[(int)tmp.x];
  } else if (baseRas64 && tmp.x >= 0 && tmp.x < baseRas64->getLx() &&
             tmp.y >= 0 && troundp(tmp.y) < baseRas64->getLy()) {
    TPixel64 pix64 = baseRas64->pixels(troundp(tmp.y))[(int)tmp.x];
    color = TPixel32((int)pix64.r / 256, (int)pix64.g / 256, (int)pix64.b / 256,
                     (int)pix64.m / 256);
  } else
    color = TPixel32::Transparent;

  TRaster32P texRas32 = texRaster;
  TRaster64P texRas64 = texRaster;
  if (texRas32) {
    texRas32->lock();
    for (int j = 0; j < texRas32->getLy(); j++) {
      TPixel32 *pix    = texRas32->pixels(j);
      TPixel32 *endPix = pix + texRas32->getLx();
      while (pix < endPix) {
        double factor = pix->m / (double)TPixel32::maxChannelValue;
        pix->r        = (UCHAR)(factor * color.r);
        pix->g        = (UCHAR)(factor * color.g);
        pix->b        = (UCHAR)(factor * color.b);
        pix->m        = (UCHAR)(factor * color.m);
        ++pix;
      }
    }
    texRas32->unlock();
  } else if (texRas64) {
    texRas64->lock();
    TPixel64 color64(color.r * 256, color.g * 256, color.b * 256,
                     color.m * 256);
    for (int j = 0; j < texRas64->getLy(); j++) {
      TPixel64 *pix    = texRas64->pixels(j);
      TPixel64 *endPix = pix + texRas64->getLx();
      while (pix < endPix) {
        double factor = pix->m / (double)TPixel64::maxChannelValue;
        pix->r        = (USHORT)(factor * color64.r);
        pix->g        = (USHORT)(factor * color64.g);
        pix->b        = (USHORT)(factor * color64.b);
        pix->m        = (USHORT)(factor * color64.m);
        ++pix;
      }
    }
    texRas64->unlock();
  }
}

/*-----------------------------------------------------------------*/
/*- 照明モードのとき、その明るさを色に格納 -*/
void Iwa_Particle::set_illuminated_colors(float illuminant,
                                          TRasterP texRaster) {
  TRaster32P texRas32 = texRaster;
  TRaster64P texRas64 = texRaster;
  if (texRas32) {
    texRas32->lock();
    TPixel32::Channel val =
        (TPixel32::Channel)(illuminant * TPixel32::maxChannelValue);
    for (int j = 0; j < texRas32->getLy(); j++) {
      TPixel32 *pix    = texRas32->pixels(j);
      TPixel32 *endPix = pix + texRas32->getLx();
      while (pix < endPix) {
        double factor = pix->m / (double)TPixel32::maxChannelValue;
        pix->r        = (UCHAR)(factor * val);
        pix->g        = (UCHAR)(factor * val);
        pix->b        = (UCHAR)(factor * val);
        ++pix;
      }
    }
    texRas32->unlock();
  } else if (texRas64) {
    texRas64->lock();
    TPixel64::Channel val =
        (TPixel64::Channel)(illuminant * TPixel64::maxChannelValue);
    for (int j = 0; j < texRas64->getLy(); j++) {
      TPixel64 *pix    = texRas64->pixels(j);
      TPixel64 *endPix = pix + texRas64->getLx();
      while (pix < endPix) {
        double factor = pix->m / (double)TPixel64::maxChannelValue;
        pix->r        = (USHORT)(factor * val);
        pix->g        = (USHORT)(factor * val);
        pix->b        = (USHORT)(factor * val);
        ++pix;
      }
    }
    texRas64->unlock();
  }
}

/*-----------------------------------------------------------------*/

void Iwa_Particle::spread_color(TPixel32 &color, double range) {
  int randcol = (int)((random.getFloat() - 0.5) * range);
  int r       = color.r + randcol;
  int g       = color.g + randcol;
  int b       = color.b + randcol;
  color.r     = (UCHAR)tcrop<TINT32>(r, (TINT32)0, (TINT32)255);
  color.g     = (UCHAR)tcrop<TINT32>(g, (TINT32)0, (TINT32)255);
  color.b     = (UCHAR)tcrop<TINT32>(b, (TINT32)0, (TINT32)255);
}

/*-----------------------------------------------------------------*/

/*-----------------------------------------------
 Iwa_Particles_Engine::roll_particles から呼ばれる
 粒子の移動
-----------------------------------------------*/

void Iwa_Particle::move(std::map<int, TTile *> porttiles,
                        const particles_values &values,
                        const particles_ranges &ranges, float windx,
                        float windy, float xgravity, float ygravity,
                        float dpicorr, int lastframe) {
  struct pos_dummy dummy;
  float frictx, fricty;
  std::map<int, float> imagereferences;
  dummy.x = dummy.y = dummy.a = 0.0;
  frictx = fricty = 0.0;

  float frictreference     = 1;
  float scalereference     = 0;
  float scalestepreference = 0;
  float randomxreference   = 1;
  float randomyreference   = 1;

  /*-
   * 移動に用いるパラメータに参照画像が刺さっている場合は、あらかじめ取得しておく
   * -*/
  for (std::map<int, TTile *>::iterator it = porttiles.begin();
       it != porttiles.end(); ++it) {
    if ((values.friction_ctrl_val == it->first ||
         values.scale_ctrl_val == it->first ||
         values.scalestep_ctrl_val == it->first ||
         values.randomx_ctrl_val == it->first ||
         values.randomy_ctrl_val == it->first) &&
        it->second->getRaster()) {
      float tmpvalue;
      get_image_reference(it->second, values, tmpvalue,
                          Iwa_TiledParticlesFx::GRAY_REF);
      imagereferences[it->first] = tmpvalue;
    }
  }

  if (values.randomx_ctrl_val)
    randomxreference = imagereferences[values.randomx_ctrl_val];
  if (values.randomy_ctrl_val)
    randomyreference = imagereferences[values.randomy_ctrl_val];

  if (check_Swing(values))
    update_Swing(values, ranges, dummy, randomxreference, randomyreference);

  if (values.friction_ctrl_val)
    frictreference = imagereferences[values.friction_ctrl_val];

  if (values.scale_ctrl_val)
    scalereference = imagereferences[values.scale_ctrl_val];
  if (values.scalestep_ctrl_val)
    scalestepreference = imagereferences[values.scalestep_ctrl_val];
  lifetime--;
  oldx = x;
  oldy = y;

  if (values.gravity_ctrl_val &&
      (porttiles.find(values.gravity_ctrl_val) != porttiles.end())) {
    get_image_gravity(porttiles[values.gravity_ctrl_val], values, xgravity,
                      ygravity);
    xgravity *= values.gravity_val;
    ygravity *= values.gravity_val;
  }

  if (values.friction_val * frictreference) {
    if (vx) {
      frictx = vx * (1.0f + values.friction_val * frictreference) +
               (10.0f / vx) * values.friction_val * frictreference;
      if ((frictx / vx) < 0.0f) frictx = 0.0f;
      vx                               = frictx;
    }
    if (!frictx &&
        fabs(values.friction_val * frictreference * 10.0f) > fabs(xgravity)) {
      xgravity = 0.0f;
      dummy.x  = 0.0f;
      dummy.a  = 0.0f;
      windx    = 0.0f;
    }

    if (vy) {
      fricty = vy * (1.0f + values.friction_val * frictreference) +
               (10.0f / vy) * values.friction_val * frictreference;
      if ((fricty / vy) < 0.0f) fricty = 0.0f;
      vy                               = fricty;
    }
    if (!fricty &&
        fabs(values.friction_val * frictreference * 10.0f) > fabs(ygravity)) {
      ygravity = 0.0f;
      dummy.y  = 0.0f;
      dummy.a  = 0.0f;
      windy    = 0.0f;
    }
  }

  /*- 重力を徐々に付けていく処理を入れる -*/
  if (genlifetime - lifetime < values.iw_gravityBufferFrame_val) {
    float ratio = (float)(genlifetime - lifetime) /
                  (float)values.iw_gravityBufferFrame_val;
    xgravity *= ratio;
    ygravity *= ratio;
  }

  /*- 重力の寄与 -*/
  vx += xgravity * mass;
  vy += ygravity * mass;

  /*- カールノイズ的動き 奥行き持たせる -*/
  if (values.curl_ctrl_1_val &&
      (porttiles.find(values.curl_ctrl_1_val) != porttiles.end())) {
    float tmpCurlx, tmpCurly;
    if (get_image_curl(porttiles[values.curl_ctrl_1_val], values, tmpCurlx,
                       tmpCurly)) {
      if (values.curl_ctrl_2_val &&
          (porttiles.find(values.curl_ctrl_2_val) != porttiles.end())) {
        float tmpCurlx2, tmpCurly2;
        if (get_image_curl(porttiles[values.curl_ctrl_2_val], values, tmpCurlx2,
                           tmpCurly2)) {
          float length1 = sqrtf(tmpCurlx * tmpCurlx + tmpCurly * tmpCurly);
          float length2 = sqrtf(tmpCurlx2 * tmpCurlx2 + tmpCurly2 * tmpCurly2);
          float length  = length1 * curlz + length2 * (1.0f - curlz);

          tmpCurlx     = tmpCurlx * curlz + tmpCurlx2 * (1.0f - curlz);
          tmpCurly     = tmpCurly * curlz + tmpCurly2 * (1.0f - curlz);
          float tmpLen = tmpCurlx * tmpCurlx + tmpCurly * tmpCurly;
          if (tmpLen > 0.0f) {
            tmpLen = sqrtf(tmpLen);
            tmpCurlx *= length / tmpLen;
            tmpCurly *= length / tmpLen;
          }
        }
      }

      tmpCurlx *= values.curl_val * mass;
      tmpCurly *= values.curl_val * mass;

      /*- ローパスフィルタをかます -*/
      curlx = 0.5f * curlx + 0.5f * tmpCurlx;
      curly = 0.5f * curly + 0.5f * tmpCurly;

      vx = vx * 0.5 + curlx;
      vy = vy * 0.5 + curly;
    }
  }

  /*- 現在位置に速度を足す -*/
  if (values.speedscale_val) {
    float scalecorr = scale / dpicorr;
    x += (vx + windx + dummy.x) * scalecorr;
    y += (vy + windy + dummy.y) * scalecorr;
  } else {
    x += vx + windx + dummy.x;
    y += vy + windy + dummy.y;
  }
  /*- 粒子の向きを計算 -*/
  angle -= values.rotspeed_val + dummy.a;

  if (!(lifetime % values.step_val) || (frame < 0)) {
    update_Animation(values, 0, lastframe, 0);
  }

  update_Scale(values, ranges, scalereference, scalestepreference);

  /*-  ひらひら -*/
  if (values.flap_ctrl_val &&
      (porttiles.find(values.flap_ctrl_val) != porttiles.end())) {
    /*- 参照画像のGradientを得る関数を利用して角度を得る -*/
    float dir_x, dir_y;
    double norm;
    norm = get_image_gravity(porttiles[values.flap_ctrl_val], values, dir_x,
                             dir_y);
    if (dir_x == 0.0f && dir_y == 0.0f) {
    } else {
      float newTheta = atan2f(dir_y, dir_x) * 180.0f / 3.14159f;

      /*- Thetaを補間する。右回り/左回りで近いほうを選ぶ -*/
      if (newTheta > flap_theta && newTheta - flap_theta > 180.0f)
        newTheta -= 360.0f;
      else if (newTheta < flap_theta && flap_theta - newTheta > 180.0f)
        newTheta += 360.0f;
      flap_theta = flap_theta * (1.0f - values.iw_flap_dir_sensitivity_val) +
                   newTheta * values.iw_flap_dir_sensitivity_val;
      if (flap_theta < 0.0f)
        flap_theta += 360.0f;
      else if (flap_theta > 360.0f)
        flap_theta -= 360.0f;

      flap_phi += values.iw_flap_velocity_val * norm / mass;

      while (flap_phi > 360.0f) flap_phi -= 360.0f;
    }
  }
}

/*-----------------------------------------------------------------*/

double Iwa_Particle::set_Opacity(std::map<int, TTile *> porttiles,
                                 const particles_values &values,
                                 float opacity_range, double dist_frame) {
  double opacity = 1.0, trailcorr;

  if (values.fadein_val && (genlifetime - lifetime) < values.fadein_val)
    opacity *= (genlifetime - lifetime - 1) / (values.fadein_val);
  if (values.fadeout_val && lifetime < values.fadeout_val)
    opacity *= (lifetime) / values.fadeout_val;

  if (trail) {
    trailcorr =
        values.trailopacity_val.first +
        (values.trailopacity_val.second - values.trailopacity_val.first) *
            (1 - (dist_frame) / trail);
    opacity *= trailcorr;
  }
  if (values.opacity_ctrl_val &&
      (porttiles.find(values.opacity_ctrl_val) != porttiles.end())) {
    float opacityreference = 0.0f;
    get_image_reference(porttiles[values.opacity_ctrl_val], values,
                        opacityreference, Iwa_TiledParticlesFx::GRAY_REF);
    opacity =
        values.opacity_val.first + (opacity_range)*opacityreference * opacity;
  } else
    opacity = values.opacity_val.first + opacity_range * opacity;
  return opacity;
}
