#pragma once

#ifndef PARTICLESENGINE_H
#define PARTICLESENGINE_H

#include "tlevel.h"
#include "particles.h"
#include "particlesfx.h"

struct float4 {
  float x, y, z, w;
};
class Particle;

class Particles_Engine {
public:
  ParticlesFx *m_parent;
  double m_frame;

public:
  Particles_Engine(ParticlesFx *parent, double frame);
  ~Particles_Engine(){};
  // Destructor

  void scramble_particles(void);
  void fill_range_struct(struct particles_values &values,
                         struct particles_ranges &ranges);
  void fill_value_struct(struct particles_values &value, double frame);
  void roll_particles(TTile *tile, std::map<int, TTile *> porttiles,
                      const TRenderSettings &ri,
                      std::list<Particle> &myParticles,
                      struct particles_values &values, float cx, float cy,
                      int frame, int curr_frame, int level_n,
                      bool *random_level, float dpi, std::vector<int> lastframe,
                      int &totalparticles);
  void normalize_values(struct particles_values &values,
                        const TRenderSettings &ri);

  void render_particles(TTile *tile, std::vector<TRasterFxPort *> part_ports,
                        const TRenderSettings &ri, TDimension &p_size,
                        TPointD &p_offset,
                        std::map<int, TRasterFxPort *> ctrl_ports,
                        std::vector<TLevelP> partLevel, float dpi,
                        int curr_frame, int shrink, double startx,
                        double starty, double endx, double endy,
                        std::vector<int> lastframe, unsigned long fxId);

  void do_render(Particle *part, TTile *tile,
                 std::vector<TRasterFxPort *> part_ports,
                 std::map<int, TTile *> porttiles, const TRenderSettings &ri,
                 TDimension &p_size, TPointD &p_offset, int lastframe,
                 std::vector<TLevelP> partLevel,
                 struct particles_values &values, double opacity_range,
                 int curr_frame,
                 std::map<std::pair<int, int>, double> &partScales);

  bool do_render_motion_blur(Particle *part, TTile *tile, TRasterP tileRas,
                             TRaster32P rfinalpart, TAffine &M,
                             const TRectD &bbox, const DoublePair &trailOpacity,
                             const double gamma_adjust,
                             const TRenderSettings &ri);

  bool port_is_used(int i, struct particles_values &values);
  bool port_is_used_for_value(int i, struct particles_values &values);
  bool port_is_used_for_gradient(int i, struct particles_values &values);

  /*-
     do_source_gradationがONのとき、入力画像のアルファ値に比例して発生濃度を変える。
          入力画像のHistogramを格納しながら領域を登録する -*/
  void fill_regions(int frame, std::vector<std::vector<TPointD>> &myregions,
                    TTile *ctrl1, bool multi, int thres,
                    bool do_source_gradation,
                    std::vector<std::vector<int>> &myHistogram);

  void fill_single_region(std::vector<std::vector<TPointD>> &myregions,
                          TTile *ctrl1, int thres, bool do_source_gradation,
                          std::vector<std::vector<int>> &myHistogram);

  /*-- Perspective
     DistributionがONのとき、Sizeに刺さったControlImageが粒子の発生分布を決める。
          そのとき、SourceのControlが刺さっている場合は、マスクとして用いられる
     --*/
  void fill_regions_with_size_map(std::vector<std::vector<TPointD>> &myregions,
                                  std::vector<std::vector<int>> &myHistogram,
                                  TTile *sizeTile, TTile *sourceTile,
                                  int thres);

  void fill_subregions(int cont_index,
                       std::vector<std::vector<TPointD>> &myregions,
                       TTile *ctrl1, int thres);

  void normalize_array(std::vector<std::vector<TPointD>> &myregions,
                       TPointD pos, int lx, int ly, int regioncounter,
                       std::vector<int> &myarray, std::vector<int> &lista,
                       std::vector<int> &listb, std::vector<int> &final);

  void fill_array(TTile *ctrl1, int &regioncount, std::vector<int> &myarray,
                  std::vector<int> &lista, std::vector<int> &listb, int thres);
};

#endif
