#pragma once

#ifndef PARTICLES_H
#define PARTICLES_H

#include "tsmartpointer.h"
#include "tspectrum.h"
#include "trandom.h"

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
  // double lifetimemin_val;
  // double lifetimemax_val;
  int startpos_val;
  int randseed_val;
  double gravity_val;
  double g_angle_val;
  int gravity_ctrl_val;
  //  int gravity_radius_val;
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

class Particle {
public:
  double x;
  double y;
  double oldx;
  double oldy;
  double vx; /*sono le velox iniziali*/
  double vy; /*sono le velox iniziali*/
  double mass;
  double scale; /*potrebbe diventare la z*/
  double angle;
  double smswingx;
  double smswingy;
  double smswinga;
  int smperiodx;
  int smperiody;
  int smperioda;
  int lifetime;
  int genlifetime;
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

public:
  Particle(int lifetime, int seed, const std::map<int, TTile *> porttiles,
           const particles_values &values, const particles_ranges &ranges,
           std::vector<std::vector<TPointD>> &myregions, int howmany, int first,
           int level, int last, std::vector<std::vector<int>> &myHistogram,
           std::vector<float> &myWeight);
  // Constructor
  ~Particle() {}
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
                           double &imagereference, int type);
  void get_image_reference(TTile *ctrl1, const particles_values &values,
                           TPixel32 &color);

  void get_image_gravity(TTile *ctrl1, const particles_values &values,
                         float &gx, float &gy);
};

class ComparebySize {
public:
  bool operator()(const Particle &f1, const Particle &f2) {
    if ((f1.scale - f2.scale) > 0) return 1;

    return 0;
  }
};

class ComparebyLifetime {
public:
  bool operator()(const Particle &f1, const Particle &f2) {
    if ((f1.lifetime - f1.genlifetime - f2.lifetime + f2.genlifetime) > 0)
      return 1;
    return 0;
  }
};

#endif
