

//#include "stdfx.h"
#include "tfxparam.h"
//#include "tpixelutils.h"
#include "trop.h"
#include "particles.h"
#include "particlesengine.h"
#include "hsvutil.h"
#include "timage_io.h"
#include "tofflinegl.h"

/*-----------------------------------------------------------------*/
void Particle::create_Animation(const particles_values &values, int first,
                                int last) {
  switch (values.animation_val) {
  case ParticlesFx::ANIM_CYCLE:
  case ParticlesFx::ANIM_S_CYCLE:
    frame     = first;
    animswing = 0; /*frame <0 perche' c'e' il preroll dialmeno un frame*/
    break;

  case ParticlesFx::ANIM_SR_CYCLE:
    frame     = (int)(first + (random.getFloat()) * (last - first));
    animswing = random.getFloat() > 0.5 ? 1 : 0;
    break;

  default:
    frame = (int)(first + (random.getFloat()) * (last - first));
    break;
  }
}

//------------------------------------------------------------------
Particle::Particle(int g_lifetime, int seed, std::map<int, TTile *> porttiles,
                   const particles_values &values,
                   const particles_ranges &ranges,
                   std::vector<std::vector<TPointD>> &myregions, int howmany,
                   int first, int level, int last,
                   std::vector<std::vector<int>> &myHistogram,
                   std::vector<float> &myWeight) {
  double random_s_a_range, random_speed;
  std::map<int, double> imagereferences;
  random                  = TRandom(seed);
  double randomxreference = 0.0;
  double randomyreference = 0.0;
  create_Animation(values, 0, last);
  // lifetime=values.lifetime_val.first+ranges.lifetime_range*random->getFloat();

  this->level = level;

  /*- 初期座標値をつくる -*/
  /*-- Perspective DistributionがONかつ、SizeのControlImageが刺さっている場合
   * --*/
  if (myregions.size() &&
      porttiles.find(values.scale_ctrl_val) != porttiles.end() &&
      values.perspective_distribution_val) {
    float size = myWeight[255];
    /*- 候補の中のIndex -*/
    float partindex = size * random.getFloat();
    for (int i = 0; i < 256; i++) {
      if (myWeight[i] > partindex) {
        int m = 255 - i;
        /*- 明度からサイズ サイズから重みを計算 -*/
        float scale =
            values.scale_val.first + ranges.scale_range * (float)m / 255.0f;
        float weight = 1.0f / (scale * scale);
        if (i > 0) partindex -= myWeight[i - 1];
        int index = myHistogram[m][(int)(partindex / weight)];
        x         = myregions[0][index].x + (double)random.getFloat() - 0.5;
        y         = myregions[0][index].y + (double)random.getFloat() - 0.5;
        break;
      }
    }
  }
  /*- 領域がある かつ 発生領域のControlImageが刺さっている場合 -*/
  else if (myregions.size() &&
           porttiles.find(values.source_ctrl_val) != porttiles.end()) {
    /*- howmany：発生Particlesのうち、何番目に発生させたものか。
            myregionが複数有る場合は、均等に割り振ることになる。 -*/
    int regionindex = howmany % myregions.size();
    /*- ウェイトを足しこむ -*/
    float size;
    if (values.source_gradation_val && !values.multi_source_val &&
        myregions.size() == 1)
      size = myWeight[254];
    else /*- ウェイトがすべて１の場合 -*/
      size = (float)myregions[regionindex].size();
    /*- 候補の中のIndex -*/
    int partindex = (int)(size * random.getFloat());

    /*- ウェイトを引いていき、負になったインデックスを選択 -*/
    if (values.source_gradation_val && !values.multi_source_val &&
        myregions.size() == 1) {
      for (int i = 0; i < 255; i++) {
        if (myWeight[i] > partindex) {
          int m = 255 - i;
          if (i > 0) partindex -= myWeight[i - 1];
          int index = myHistogram[m][(int)(partindex / m)];
          x = myregions[regionindex][index].x + (double)random.getFloat() - 0.5;
          y = myregions[regionindex][index].y + (double)random.getFloat() - 0.5;
          break;
        }
      }
    } else /*- ウェイトがすべて１の場合 -*/
    {
      x = myregions[regionindex][(int)partindex].x + (double)random.getFloat() -
          0.5;
      y = myregions[regionindex][(int)partindex].y + (double)random.getFloat() -
          0.5;
    }
  } else {
    x = values.x_pos_val + values.length_val * (random.getFloat() - 0.5);
    y = values.y_pos_val + values.height_val * (random.getFloat() - 0.5);
  }

  for (std::map<int, TTile *>::iterator it = porttiles.begin();
       it != porttiles.end(); ++it) {
    if ((values.lifetime_ctrl_val == it->first ||
         values.speed_ctrl_val == it->first ||
         values.scale_ctrl_val == it->first ||
         values.rot_ctrl_val == it->first
         /*-  Speed Angleを明るさでコントロールする場合 -*/
         || (values.speeda_ctrl_val == it->first &&
             !values.speeda_use_gradient_val)) &&
        it->second->getRaster()) {
      double tmpvalue;
      get_image_reference(it->second, values, tmpvalue, ParticlesFx::GRAY_REF);
      imagereferences[it->first] = tmpvalue;
    }
  }
  if (values.lifetime_ctrl_val) {
    double lifetimereference = 0.0;
    lifetimereference        = imagereferences[values.lifetime_ctrl_val];
    lifetime                 = g_lifetime * lifetimereference;
  } else
    lifetime = g_lifetime;

  // lifetime=g_lifetime;
  genlifetime = lifetime;
  if (values.speed_ctrl_val &&
      (porttiles.find(values.speed_ctrl_val) != porttiles.end())) {
    double speedreference = 0.0;
    speedreference        = imagereferences[values.speed_ctrl_val];
    random_speed =
        values.speed_val.first + (ranges.speed_range) * speedreference;
  } else
    random_speed =
        values.speed_val.first + (ranges.speed_range) * random.getFloat();

  /*- Speed Angleの制御。RangeモードとGradientモードがある -*/
  if (values.speeda_ctrl_val &&
      (porttiles.find(values.speeda_ctrl_val) != porttiles.end())) {
    if (values.speeda_use_gradient_val) {
      /*- 参照画像のGradientを得る関数を利用して角度を得る -*/
      float dir_x, dir_y;
      get_image_gravity(porttiles[values.speeda_ctrl_val], values, dir_x,
                        dir_y);
      if (dir_x == 0.0f && dir_y == 0.0f)
        random_s_a_range = values.speed_val.first;
      else
        random_s_a_range = atan2f(dir_x, -dir_y);
    } else {
      double speedareference = 0.0;
      speedareference        = imagereferences[values.speeda_ctrl_val];
      random_s_a_range =
          values.speeda_val.first + (ranges.speeda_range) * speedareference;
    }
  } else /*- Control Imageが無ければランダム -*/
    random_s_a_range =
        values.speeda_val.first + (ranges.speeda_range) * random.getFloat();

  trail =
      (int)(values.trail_val.first + (ranges.trail_range) * random.getFloat());
  vx   = random_speed * sin(random_s_a_range);
  vy   = -random_speed * cos(random_s_a_range);
  oldx = 0;
  oldy = 0;
  mass = values.mass_val.first + (ranges.mass_range) * random.getFloat();
  if (values.scale_ctrl_val &&
      (porttiles.find(values.scale_ctrl_val) != porttiles.end())) {
    double scalereference = 0.0;
    scalereference        = imagereferences[values.scale_ctrl_val];
    scale = values.scale_val.first + (ranges.scale_range) * scalereference;
  } else {
    /*-
     * ONのとき、かつ、ScaleにControlが無い場合、粒子サイズが小さいほど(遠くにあるので)多く分布するようになる。-*/
    if (values.perspective_distribution_val) {
      scale =
          (values.scale_val.first * values.scale_val.second) /
          (values.scale_val.second - (ranges.scale_range) * random.getFloat());
    } else
      scale = values.scale_val.first + (ranges.scale_range) * random.getFloat();
  }
  if (values.rot_ctrl_val &&
      (porttiles.find(values.rot_ctrl_val) != porttiles.end())) {
    double anglereference = 0.0;
    anglereference        = imagereferences[values.rot_ctrl_val];
    angle = -(values.rot_val.first) - (ranges.rot_range) * anglereference;
  } else
    angle = -(values.rot_val.first) - (ranges.rot_range) * random.getFloat();

  if (values.randomx_ctrl_val)
    randomxreference = imagereferences[values.randomx_ctrl_val];
  if (values.randomy_ctrl_val)
    randomyreference = imagereferences[values.randomy_ctrl_val];
  // if(check_Swing(values))
  create_Swing(values, ranges, randomxreference, randomyreference);

  create_Colors(values, ranges, porttiles);

  if (scale < 0.001) scale = 0;
}

//------------------------------------------------------------------
int Particle::check_Swing(const particles_values &values) {
  return (values.randomx_val.first || values.randomx_val.second ||
          values.randomy_val.first || values.randomy_val.second ||
          values.rotsca_val.first || values.rotsca_val.second);
}

/*-----------------------------------------------------------------*/

void Particle::create_Swing(const particles_values &values,
                            const particles_ranges &ranges,
                            double randomxreference, double randomyreference) {
  changesignx =
      (int)(values.swing_val.first + random.getFloat() * (ranges.swing_range));
  changesigny =
      (int)(values.swing_val.first + random.getFloat() * (ranges.swing_range));
  changesigna = (int)(values.rotswing_val.first +
                      random.getFloat() * (ranges.rotswing_range));
  if (values.swingmode_val == ParticlesFx::SWING_SMOOTH) {
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
  if (values.rotswingmode_val == ParticlesFx::SWING_SMOOTH) {
    smswinga  = abs((int)(values.rotsca_val.first +
                         random.getFloat() * (ranges.rotsca_range)));
    smperioda = changesigna;
  }
  signx = random.getBool() ? 1 : -1;
  signy = random.getBool() ? 1 : -1;
  signa = random.getBool() ? 1 : -1;
}

/*-----------------------------------------------------------------*/

void Particle::create_Colors(const particles_values &values,
                             const particles_ranges &ranges,
                             std::map<int, TTile *> porttiles) {
  // TPixel32 color;

  if (values.genfadecol_val) {
    TPixel32 color;
    if (values.gencol_ctrl_val &&
        (porttiles.find(values.gencol_ctrl_val) != porttiles.end()))
      get_image_reference(porttiles[values.gencol_ctrl_val], values, color);
    else
      color = values.gencol_val.getPremultipliedValue(random.getFloat());
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
/*-- modify_colors_and_opacityから呼ばれる。
        lifetime: 粒子の現在の年齢
        gencol/fincol/foutcolから色を決める --*/
void Particle::modify_colors(TPixel32 &color, double &intensity) {
  float percent = 0;
  if ((gencol.fadecol || fincol.fadecol) &&
      (genlifetime - lifetime) <= fincol.rangecol) {
    if (fincol.rangecol)
      percent = (genlifetime - lifetime) / (float)(fincol.rangecol);
    // color=gencol.col+percent*(fincol.col-gencol.col);
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
/*- do_render から呼ばれる。各粒子の描画の直前に色を決める -*/
void Particle::modify_colors_and_opacity(const particles_values &values,
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
    // pop_rgbfade(pars, 0, 0, raster, raster, 1);
  }

  if (curr_opacity != 1.0)
    TRop::rgbmScale(raster32, raster32, 1, 1, 1, curr_opacity);
}

/*-----------------------------------------------------------------*/
void Particle::update_Animation(const particles_values &values, int first,
                                int last, int keep) {
  switch (values.animation_val) {
  case ParticlesFx::ANIM_RANDOM:
    frame = (int)(first + random.getFloat() * (last - first));
    break;

  case ParticlesFx::ANIM_R_CYCLE:
  case ParticlesFx::ANIM_CYCLE:
    if (!keep || frame != keep - 1)
      frame = first + (frame + 1) % (last - first);
    break;

  case ParticlesFx::ANIM_S_CYCLE:
  case ParticlesFx::ANIM_SR_CYCLE:
    if (!keep || frame != keep - 1) {
      if (!animswing && frame < last - 1) {
        frame = (frame + 1);
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
void Particle::update_Swing(const particles_values &values,
                            const particles_ranges &ranges,
                            struct pos_dummy &dummy, double randomxreference,
                            double randomyreference) {
  if (values.swingmode_val == ParticlesFx::SWING_SMOOTH) {
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

  if (values.rotswingmode_val == ParticlesFx::SWING_SMOOTH) {
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
    if (values.swingmode_val == ParticlesFx::SWING_SMOOTH) {
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
    if (values.swingmode_val == ParticlesFx::SWING_SMOOTH) {
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
    if (values.rotswingmode_val == ParticlesFx::SWING_SMOOTH) {
      smperioda = changesigna;
      smswinga =
          values.rotsca_val.first + random.getFloat() * (ranges.rotsca_range);
    }
  }
}
/*-----------------------------------------------------------------*/
void Particle::update_Scale(const particles_values &values,
                            const particles_ranges &ranges,
                            double scalereference, double scalestepreference) {
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
void Particle::get_image_reference(TTile *ctrl, const particles_values &values,
                                   double &imagereference, int type) {
  TRaster32P raster32 = ctrl->getRaster();
  TPointD tmp(x, y);
  tmp -= ctrl->m_pos;
  imagereference = 0.0;
  raster32->lock();
  switch (type) {
  case ParticlesFx::GRAY_REF:
    if (raster32 && tmp.x >= 0 && tmp.x < raster32->getLx() && tmp.y >= 0 &&
        troundp(tmp.y) < raster32->getLy()) {
      TPixel32 pix = raster32->pixels(troundp(tmp.y))[(int)tmp.x];
      imagereference =
          TPixelGR8::from(pix).value / (double)TPixelGR8::maxChannelValue;
    }
    break;

  case ParticlesFx::H_REF:
    if (raster32 && tmp.x >= 0 && tmp.x < raster32->getLx() && tmp.y >= 0 &&
        tround(tmp.y) < raster32->getLy()) {
      double aux = (double)TPixel32::maxChannelValue;
      double h, s, v;
      TPixel32 pix = raster32->pixels(troundp(tmp.y))[(int)tmp.x];
      OLDRGB2HSV(pix.r / aux, pix.g / aux, pix.b / aux, &h, &s, &v);
      imagereference = h / 360.0;
    }
    break;
  }
  raster32->unlock();
}

/*-----------------------------------------------------------------*/
void Particle::get_image_gravity(TTile *ctrl1, const particles_values &values,
                                 float &gx, float &gy) {
  TRaster32P raster32 = ctrl1->getRaster();
  TPointD tmp(x, y);
  tmp -= ctrl1->m_pos;
  int radius = 4;
  gx         = 0;
  gy         = 0;
//#define OLDSTUFF
#ifdef OLDSTUFF
  int i;
#endif
  raster32->lock();
#ifdef OLDSTUFF
  if (!values.gravity_radius_val) {
    radius = 4;
    if (raster32 && tmp.x >= radius && tmp.x < raster32->getLx() - radius &&
        tmp.y >= radius && tmp.y < raster32->getLy() - radius) {
      TPixel32 *pix = &(raster32->pixels(troundp(tmp.y))[(int)tmp.x]);
      double norm   = 1 / ((double)TPixelGR8::maxChannelValue);
      for (i = 1; i < radius; i++) {
        gx += TPixelGR8::from(*(pix + i)).value;
        gx -= TPixelGR8::from(*(pix - i)).value;
        gy += TPixelGR8::from(*(pix + raster32->getWrap() * i)).value;
        gy -= TPixelGR8::from(*(pix - raster32->getWrap() * i)).value;
      }
      gx = gx * norm;
      gy = gy * norm;
    }
  } else {
#endif
    radius = 2;
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

      double norm = sqrt(gx * gx + gy * gy);
      if (norm) {
        double inorm = 0.1 / norm;
        gx           = gx * inorm;
        gy           = gy * inorm;
      }
    }
#ifdef OLDSTUFF
  }
#endif
  raster32->unlock();
}

/*-----------------------------------------------------------------*/
void Particle::get_image_reference(TTile *ctrl1, const particles_values &values,
                                   TPixel32 &color) {
  TRaster32P raster32 = ctrl1->getRaster();
  TPointD tmp(x, y);
  tmp -= ctrl1->m_pos;

  if (raster32 && tmp.x >= 0 && tmp.x < raster32->getLx() && tmp.y >= 0 &&
      troundp(tmp.y) < raster32->getLy()) {
    color = raster32->pixels(troundp(tmp.y))[(int)tmp.x];
  }
  /*-- 参照画像のBBoxの外側では、粒子を透明にする --*/
  else
    color = TPixel32::Transparent;
}
/*-----------------------------------------------------------------*/
void Particle::spread_color(TPixel32 &color, double range) {
  int randcol = (int)((random.getFloat() - 0.5) * range);
  int r       = color.r + randcol;
  int g       = color.g + randcol;
  int b       = color.b + randcol;
  color.r     = (UCHAR)tcrop<TINT32>(r, (TINT32)0, (TINT32)255);
  color.g     = (UCHAR)tcrop<TINT32>(g, (TINT32)0, (TINT32)255);
  color.b     = (UCHAR)tcrop<TINT32>(b, (TINT32)0, (TINT32)255);
}
/*-----------------------------------------------------------------*/

void Particle::move(std::map<int, TTile *> porttiles,
                    const particles_values &values,
                    const particles_ranges &ranges, float windx, float windy,
                    float xgravity, float ygravity, float dpicorr,
                    int lastframe) {
  struct pos_dummy dummy;
  float frictx, fricty;
  // int time;
  std::map<int, double> imagereferences;
  dummy.x = dummy.y = dummy.a = 0.0;
  frictx = fricty = 0.0;

  double frictreference     = 1;
  double scalereference     = 0;
  double scalestepreference = 0;
  double randomxreference   = 1;
  double randomyreference   = 1;

  for (std::map<int, TTile *>::iterator it = porttiles.begin();
       it != porttiles.end(); ++it) {
    if ((values.friction_ctrl_val == it->first ||
         values.scale_ctrl_val == it->first ||
         values.scalestep_ctrl_val == it->first ||
         values.randomx_ctrl_val == it->first ||
         values.randomy_ctrl_val == it->first) &&
        it->second->getRaster()) {
      double tmpvalue;
      get_image_reference(it->second, values, tmpvalue, ParticlesFx::GRAY_REF);
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
  // time=genlifetime-lifetime-1;
  // if(time<0) time=0;
  if (values.gravity_ctrl_val &&
      (porttiles.find(values.gravity_ctrl_val) != porttiles.end())) {
    get_image_gravity(porttiles[values.gravity_ctrl_val], values, xgravity,
                      ygravity);
    xgravity *= values.gravity_val;
    ygravity *= values.gravity_val;
  }

  if (values.friction_val * frictreference) {
    if (vx) {
      frictx = vx * (1 + values.friction_val * frictreference) +
               (10 / vx) * values.friction_val * frictreference;
      if ((frictx / vx) < 0) frictx = 0;
      vx = frictx;
    }
    if (!frictx &&
        fabs(values.friction_val * frictreference * 10) > fabs(xgravity)) {
      xgravity = 0;
      dummy.x  = 0;
      dummy.a  = 0;
      windx    = 0;
    }

    if (vy) {
      fricty = vy * (1 + values.friction_val * frictreference) +
               (10 / vy) * values.friction_val * frictreference;
      if ((fricty / vy) < 0) fricty = 0;
      vy = fricty;
    }
    if (!fricty &&
        fabs(values.friction_val * frictreference * 10) > fabs(ygravity)) {
      ygravity = 0;
      dummy.y  = 0;
      dummy.a  = 0;
      windy    = 0;
    }
  }

  vx += xgravity * mass;
  vy += ygravity * mass;
  if (values.speedscale_val) {
    float scalecorr = scale / dpicorr;
    x += (vx + windx + dummy.x) * scalecorr;
    y += (vy + windy + dummy.y) * scalecorr;
  } else {
    x += vx + windx + dummy.x;
    y += vy + windy + dummy.y;
  }
  angle -= values.rotspeed_val + dummy.a;

  if (!(lifetime % values.step_val) || (frame < 0)) {
    update_Animation(values, 0, lastframe, 0);
  }

  update_Scale(values, ranges, scalereference, scalestepreference);
}

/*-----------------------------------------------------------------*/
double Particle::set_Opacity(std::map<int, TTile *> porttiles,
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
    double opacityreference = 0.0;
    get_image_reference(porttiles[values.opacity_ctrl_val], values,
                        opacityreference, ParticlesFx::GRAY_REF);
    opacity =
        values.opacity_val.first + (opacity_range)*opacityreference * opacity;
  } else
    opacity = values.opacity_val.first + opacity_range * opacity;
  return opacity;
}
