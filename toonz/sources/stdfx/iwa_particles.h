#pragma once

//------------------------------------------------------------------
// Iwa_Particle for Marnie
// based on ParticlesFx by Digital Video
//------------------------------------------------------------------

#ifndef IWA_PARTICLES_H
#define IWA_PARTICLES_H

#include "tsmartpointer.h"
#include "tspectrum.h"
#include "trandom.h"

#include "iwa_particlesengine.h"
#include <QList>
//------------------------------------------------------------------------------

struct particles_values {
  int source_ctrl_val;
  int bright_thres_val;
  bool multi_source_val;
  double x_pos_val;
  double y_pos_val;
  int unit_val;
  double length_val;
  double height_val;
  double maxnum_val;
  DoublePair lifetime_val;
  int lifetime_ctrl_val;
  int column_lifetime_val;
  int startpos_val;
  int randseed_val;
  double gravity_val;
  double g_angle_val;
  int gravity_ctrl_val;
  double friction_val;
  int friction_ctrl_val;
  double windint_val;
  double windangle_val;
  int swingmode_val;
  DoublePair randomx_val;
  DoublePair randomy_val;
  int randomx_ctrl_val;
  int randomy_ctrl_val;
  DoublePair swing_val;
  DoublePair speed_val;
  int speed_ctrl_val;
  DoublePair speeda_val;
  int speeda_ctrl_val;
  bool speeda_use_gradient_val;
  bool speedscale_val;
  char *levelname_val;
  int toplayer_val;
  DoublePair mass_val;
  DoublePair scale_val;
  int scale_ctrl_val;
  bool scale_ctrl_all_val;
  DoublePair rot_val;
  int rot_ctrl_val;
  DoublePair trail_val;
  double trailstep_val;
  int rotswingmode_val;
  double rotspeed_val;
  DoublePair rotsca_val;
  DoublePair rotswing_val;
  bool pathaim_val;
  DoublePair opacity_val;
  int opacity_ctrl_val;
  DoublePair trailopacity_val;
  double mblur_val;
  DoublePair scalestep_val;
  int scalestep_ctrl_val;
  double fadein_val;
  double fadeout_val;
  int animation_val;
  int step_val;
  TSpectrum gencol_val;
  int gencol_ctrl_val;
  double gencol_spread_val;
  double genfadecol_val;
  TSpectrum fincol_val;
  int fincol_ctrl_val;
  double fincol_spread_val;
  double finrangecol_val;
  double finfadecol_val;
  TSpectrum foutcol_val;
  int foutcol_ctrl_val;
  double foutcol_spread_val;
  double foutrangecol_val;
  double foutfadecol_val;
  TRandom *random_val;

  bool source_gradation_val;
  bool reset_random_for_every_frame_val;
  bool pick_color_for_every_frame_val;
  bool perspective_distribution_val;

  /*- 以下、このFxのために追加したパラメータ -*/
  /*-  計算モード （背景＋粒子／粒子／背景／照明された粒子）-*/
  int iw_rendermode_val;
  /*- 粒子に貼られる絵の素材 -*/
  int base_ctrl_val;
  /*- カールノイズ的な動きを与える -*/
  double curl_val;
  int curl_ctrl_1_val;
  int curl_ctrl_2_val;
  /*- 粒子敷き詰め。粒子を正三角形で敷き詰めたときの、
          正三角形の一辺の長さをインチで指定する -*/
  double iw_triangleSize;
  /*- ひらひら回転 -*/
  int flap_ctrl_val;
  double iw_flap_velocity_val;
  double iw_flap_dir_sensitivity_val;
  /*- ひらひら粒子に照明を当てる。normalize_values()内で Degree → Radian 化する
   * -*/
  double iw_light_theta_val;
  double iw_light_phi_val;
  /*- 読み込みマージン -*/
  double margin_val;
  /*- 重力を徐々に与えるためのフレーム長 -*/
  int iw_gravityBufferFrame_val;
};

//------------------------------------------------------------------------------

struct particles_ranges {
  float swing_range;
  float randomx_range;
  float randomy_range;
  float rot_range;
  float rotswing_range;
  float rotsca_range;
  float lifetime_range;
  float speed_range;
  float speeda_range;
  float mass_range;
  float scale_range;
  float scalestep_range;
  int trail_range;
};

//------------------------------------------------------------------------------

struct pos_dummy {
  float x, y, a;
};

//------------------------------------------------------------------------------

typedef struct {
  TPixel32 col;
  int rangecol;
  double fadecol;
} coldata;

//------------------------------------------------------------------------------

class Iwa_Particle {
public:
  double x;
  float y;
  float oldx;
  float oldy;
  float vx;
  float vy;
  float mass;
  float scale;
  float angle;
  float smswingx;
  float smswingy;
  float smswinga;
  int smperiodx;
  int smperiody;
  int smperioda;
  int lifetime;    /*- 現在の残り寿命 -*/
  int genlifetime; /*- 発生時の寿命 -*/
  int level;
  int frame;
  int signx;
  int trail;
  coldata gencol;
  coldata fincol;
  coldata foutcol;
  int changesignx;
  int signy;
  int changesigny;
  int signa;
  int changesigna;
  bool animswing;
  TRandom random;
  int seed;

  float initial_x;
  float initial_y;
  float initial_angle;
  float initial_scale;
  float curlx;
  float curly;
  float curlz;
  /*- ひらひら動かす回転角（Degree）-*/
  float flap_theta;
  float flap_phi;

public:
  Iwa_Particle(int lifetime, int seed, const std::map<int, TTile *> porttiles,
               const particles_values &values, const particles_ranges &ranges,
               int howmany, int first, int level, int last, float posx,
               float posy,    /*- 座標を指定 -*/
               bool isUpward, /*-  初期向き -*/
               int initSourceFrame);
  // Constructor
  ~Iwa_Particle() {}
  // Destructor
  void create_Animation(const particles_values &values, int first, int last);

  int check_Swing(const particles_values &values);

  void create_Swing(const particles_values &values,
                    const particles_ranges &ranges, double randomxreference,
                    double randomyreference);
  void create_Colors(const particles_values &values,
                     const particles_ranges &ranges,
                     std::map<int, TTile *> porttiles);

  void move(const std::map<int, TTile *> porttiles,
            const particles_values &values, const particles_ranges &ranges,
            float windx, float windy, float xgravity, float ygravity, float dpi,
            int lastframe);

  void spread_color(TPixel32 &color, double range);
  void update_Animation(const particles_values &values, int first, int last,
                        int keep);
  void update_Swing(const particles_values &values,
                    const particles_ranges &ranges, struct pos_dummy &dummy,
                    double randomxreference, double randomyreference);
  void update_Scale(const particles_values &values,
                    const particles_ranges &ranges, double scalereference,
                    double scalestepreference);

  double set_Opacity(std::map<int, TTile *> porttiles,
                     const particles_values &values, float opacity_range,
                     double dist_frame);

  void modify_colors(TPixel32 &color, double &intensity);
  void modify_colors_and_opacity(const particles_values &values,
                                 float curr_opacity, int dist_frame,
                                 TRaster32P raster);

  bool canHandle(const TRenderSettings &info, double frame) { return false; }
  void get_image_reference(TTile *ctrl1, const particles_values &values,
                           float &imagereference, int type);
  void get_image_reference(TTile *ctrl1, const particles_values &values,
                           TPixel32 &color);

  /*- ベクタ長を返す -*/
  float get_image_gravity(TTile *ctrl1, const particles_values &values,
                          float &gx, float &gy);

  void get_base_image_texture(TTile *ctrl1, const particles_values &values,
                              TRasterP texRaster, const TRectD &texBBox,
                              const TRenderSettings &ri);

  void get_base_image_color(TTile *ctrl1, const particles_values &values,
                            TRasterP texRaster, const TRectD &texBBox,
                            const TRenderSettings &ri);

  /*- Control圏内ならtrueを返す -*/
  bool get_image_curl(TTile *ctrl1, const particles_values &values, float &cx,
                      float &cy);

  /*- 照明モードのとき、その明るさを色に格納 -*/
  void set_illuminated_colors(float illuminant, TRasterP texRaster);
};

class Iwa_ComparebySize {
public:
  bool operator()(const Iwa_Particle &f1, const Iwa_Particle &f2) {
    if ((f1.scale - f2.scale) > 0) return 1;

    return 0;
  }
};

class Iwa_ComparebyLifetime {
public:
  bool operator()(const Iwa_Particle &f1, const Iwa_Particle &f2) {
    if ((f1.lifetime - f1.genlifetime - f2.lifetime + f2.genlifetime) > 0)
      return 1;
    return 0;
  }
};

#endif
