#pragma once

//------------------------------------------------------------------
// Iwa_Particles_Engine for Marnie
// based on Particles_Engine by Digital Video
//------------------------------------------------------------------

#ifndef IWA_PARTICLESENGINE_H
#define IWA_PARTICLESENGINE_H

#include "tlevel.h"
#include "iwa_particles.h"
#include "iwa_particlesfx.h"

struct float3 {
  float x, y, z;
};

class Iwa_Particle;

//--------------------
/* 粒子を規則正しく配する、その位置情報と
 （剥がれるきっかけとなる）現在のポテンシャルを格納 -*/
struct ParticleOrigin {
  float pos[2];
  float potential;
  /*- 上を向いているかどうか -*/
  bool isUpward;
  /*- どのテクスチャ素材を使うか -*/
  unsigned char level;
  /*- 何番目のフレームを使うか -*/
  unsigned char initSourceFrame;
  /*- ピクセル位置 -*/
  short int pixPos[2];

  ParticleOrigin(float x, float y, float _potential, bool _isUpward,
                 unsigned char _level, unsigned char _initSourceFrame,
                 short int pix_x, short int pix_y) {
    pos[0]          = x;
    pos[1]          = y;
    potential       = _potential;
    isUpward        = _isUpward;
    level           = _level;
    initSourceFrame = _initSourceFrame;
    pixPos[0]       = pix_x;
    pixPos[1]       = pix_y;
  }
};

//--------------------

class Iwa_Particles_Engine {
public:
  Iwa_TiledParticlesFx *m_parent;
  double m_frame;

public:
  Iwa_Particles_Engine(Iwa_TiledParticlesFx *parent, double frame);
  ~Iwa_Particles_Engine(){};
  // Destructor

  void scramble_particles(void);

  void fill_range_struct(struct particles_values &values,
                         struct particles_ranges &ranges);

  void fill_value_struct(struct particles_values &value, double frame);

  void roll_particles(TTile *tile, std::map<int, TTile *> porttiles,
                      const TRenderSettings &ri,
                      std::list<Iwa_Particle> &myParticles,
                      struct particles_values &values, float cx, float cy,
                      int frame, int curr_frame, int level_n,
                      bool *random_level, float dpi, std::vector<int> lastframe,
                      int &totalparticles,
                      QList<ParticleOrigin> &particleOrigin, int genPartNum);

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

  void do_render(Iwa_Particle *part, TTile *tile,
                 std::vector<TRasterFxPort *> part_ports,
                 std::map<int, TTile *> porttiles, const TRenderSettings &ri,
                 TDimension &p_size, TPointD &p_offset, int lastframe,
                 std::vector<TLevelP> partLevel,
                 struct particles_values &values, float opacity_range,
                 int curr_frame,
                 std::map<std::pair<int, int>, float> &partScales,
                 TTile *baseImgTile);

  bool port_is_used(int i, struct particles_values &values);

  void fill_single_region(std::vector<TPointD> &myregions, TTile *ctrl1,
                          int thres, bool do_source_gradation,
                          QList<QList<int>> &myHistogram,
                          QList<ParticleOrigin> &particleOrigins);

  void fill_subregions(int cont_index,
                       std::vector<std::vector<TPointD>> &myregions,
                       TTile *ctrl1, int thres);

  void normalize_array(std::vector<std::vector<TPointD>> &myregions,
                       TPointD pos, int lx, int ly, int regioncounter,
                       std::vector<int> &myarray, std::vector<int> &lista,
                       std::vector<int> &listb, std::vector<int> & final);

  void fill_array(TTile *ctrl1, int &regioncount, std::vector<int> &myarray,
                  std::vector<int> &lista, std::vector<int> &listb, int thres);

  /*- 敷き詰めのため。まだ出発していない粒子情報を初期化 -*/
  void initParticleOrigins(TRectD &outTileBBox,
                           QList<ParticleOrigin> &particleOrigins,
                           const double frame, const TAffine affine,
                           struct particles_values &values, int level_n,
                           std::vector<int> &lastframe, double pixelMargin);

  /*- Particle::create_Animationと同じ。粒子発生前に
          あらかじめ計算してparticesOriginに持たせるため -*/
  unsigned char getInitSourceFrame(const particles_values &values, int first,
                                   int last);

  /*- ここで、出発した粒子の分、穴を開けた背景を描く -*/
  void renderBackground(TTile *tile, QList<ParticleOrigin> &origins,
                        std::vector<TRasterFxPort *> part_ports,
                        const TRenderSettings &ri,
                        std::vector<TLevelP> partLevel,
                        std::map<std::pair<int, int>, float> &partScales,
                        TTile *baseImgTile);
};

#endif
