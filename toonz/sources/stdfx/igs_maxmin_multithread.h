#pragma once

#ifndef igs_maxmin_multithread_h
#define igs_maxmin_multithread_h

#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_resource_multithread.h"
#include "igs_maxmin_slrender.h"

namespace igs {
namespace maxmin {
template <class IT, class RT>
class thread final
    : public igs::resource::thread_execute_interface { /* thread単位の実行設定
                                                    */
public:
  thread() {}
  void setup(
      /* 入出力画像 */
      const IT *inn, IT *out, int height, int width, int channels

      /* Pixel毎に効果の強弱 */
      ,
      const RT *ref /* 求める画像(out)と同じ高、幅、チャンネル数 */
      ,
      const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

      /* Action Geometry */
      ,
      int y_begin, int y_end, std::vector<int> *lens_offsets_p,
      std::vector<int> *lens_sizes_p,
      std::vector<std::vector<double>> *lens_ratio_p

      ,
      double radius, double smooth_outer_range, int polygon_number,
      double roll_degree

      /* Action Type */
      ,
      bool min_sw, bool alpha_rendering_sw, bool add_blend_sw) {
    this->inn_      = inn;
    this->out_      = out;
    this->height_   = height;
    this->width_    = width;
    this->channels_ = channels;

    this->ref_      = ref;
    this->ref_mode_ = ref_mode;

    this->y_begin_        = y_begin;
    this->y_end_          = y_end;
    this->lens_offsets_p_ = lens_offsets_p;
    this->lens_sizes_p_   = lens_sizes_p;
    this->lens_ratio_p_   = lens_ratio_p;

    this->radius_             = radius;
    this->smooth_outer_range_ = smooth_outer_range;
    this->polygon_number_     = polygon_number;
    this->roll_degree_        = roll_degree;

    this->min_sw_             = min_sw;
    this->alpha_rendering_sw_ = alpha_rendering_sw;
    this->add_blend_sw_       = add_blend_sw;

    igs::maxmin::slrender::resize(
        static_cast<int>(this->lens_offsets_p_->size()), this->width_,
        (ref != 0 || 4 <= channels) ? true : false, this->pixe_tracks_,
        this->alpha_ref_, this->result_);
  }
  void run(void) override { /* threadで実行する部分 */
    bool rgb_ren_sw = true;
    bool alp_ren_sw = this->alpha_rendering_sw_;
    bool add_ble_sw = this->add_blend_sw_;

    /* 小さいすぎて処理しない */
    if (this->pixe_tracks_.size() <= 1) {
      rgb_ren_sw = false;
      alp_ren_sw = false;
    }  // not render,then copy

    /* first scanline-->next scanlineで処理するので
    かならすchannel毎にまとめてループする */

    if (igs::image::rgba::siz == this->channels_) {
      using namespace igs::image::rgba;

      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, alp, alp_ren_sw, false);
      }

      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, blu, rgb_ren_sw, add_ble_sw);
      }
      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, gre, rgb_ren_sw, add_ble_sw);
      }
      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, red, rgb_ren_sw, add_ble_sw);
      }
    } else if (igs::image::rgb::siz == this->channels_) {
      using namespace igs::image::rgb;
      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, blu, rgb_ren_sw, add_ble_sw);
      }
      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, gre, rgb_ren_sw, add_ble_sw);
      }
      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, red, rgb_ren_sw, add_ble_sw);
      }
    } else if (1 == this->channels_) { /* grayscale */
      for (int yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, 0, rgb_ren_sw, add_ble_sw);
      }
    }
  }
  void clear(void) {
    igs::maxmin::slrender::clear(this->pixe_tracks_, this->alpha_ref_,
                                 this->result_);
  }

private:
  const IT *inn_;
  IT *out_;
  int height_;
  int width_;
  int channels_;

  const RT *ref_;
  int ref_mode_;

  int y_begin_;
  int y_end_;

  std::vector<int> *lens_offsets_p_;
  std::vector<int> *lens_sizes_p_;
  std::vector<std::vector<double>> *lens_ratio_p_;

  double radius_;
  double smooth_outer_range_;
  int polygon_number_;
  double roll_degree_;

  bool min_sw_;
  bool alpha_rendering_sw_;
  bool add_blend_sw_;

  std::vector<std::vector<double>> pixe_tracks_;
  std::vector<double> alpha_ref_;
  std::vector<double> result_;

  void rendering_sl_ch_(const int yy, const int zz, const bool render_sw,
                        const bool add_blend_sw) {
    if (!render_sw) {
      igs::maxmin::getput::copy(this->inn_, this->height_, this->width_,
                                this->channels_, yy, zz, this->out_);
      return;
    }
    if (yy == this->y_begin_) {
      igs::maxmin::getput::get_first(
          this->inn_, this->out_, this->height_, this->width_, this->channels_,
          this->ref_, this->ref_mode_, yy, zz,
          static_cast<int>(this->pixe_tracks_.size() / 2), add_blend_sw,
          this->pixe_tracks_, this->alpha_ref_, this->result_);
    } else {
      igs::maxmin::slrender::shift(this->pixe_tracks_);
      igs::maxmin::getput::get_next(
          this->inn_, this->out_, this->height_, this->width_, this->channels_,
          this->ref_, this->ref_mode_, yy, zz,
          static_cast<int>(this->pixe_tracks_.size() / 2), add_blend_sw,
          this->pixe_tracks_, this->alpha_ref_, this->result_);
    }
    igs::maxmin::slrender::render(
        this->radius_, this->smooth_outer_range_, this->polygon_number_,
        this->roll_degree_, this->min_sw_, *(this->lens_offsets_p_),
        *(this->lens_sizes_p_), *(this->lens_ratio_p_), this->pixe_tracks_,
        this->alpha_ref_, this->result_);

    igs::maxmin::getput::put(this->result_, this->height_, this->width_,
                             this->channels_, yy, zz, this->out_);
  }
};
}
}

//------------------------------------------------------------

#include "igs_maxmin_lens_matrix.h"

namespace igs {
namespace maxmin {
template <class IT, class RT>
class multithread {
public:
  multithread(
      /* 入出力画像 */
      const IT *inn, IT *out, const int height, const int width,
      const int channels

      /* Pixel毎に効果の強弱 */
      ,
      const RT *ref /* 求める画像(out)と同じ高、幅、チャンネル数 */
      ,
      const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

      /* Action Geometry */
      ,
      const double radius /* =1.0  0..100...DOUBLE_MAX */
      ,
      const double smooth_outer_range /*=1.0  0..100...DOUBLE_MAX */
      ,
      const int polygon_number /* =2    2...16...INT_MAX */
      ,
      const double roll_degree /* =0.0  0...360...DOUBLE_MAX */

      /* Action Type */
      ,
      const bool min_sw /* =false */
      ,
      const bool alpha_rendering_sw /* =true */
      ,
      const bool add_blend_sw /* =false */

      /* Speed up */
      ,
      const int number_of_thread /* =1    1...24...INT_MAX */
      ) {
    /*--------------メモリ確保--------------------------*/
    igs::maxmin::alloc_and_shape_lens_matrix(
        radius, radius + smooth_outer_range, polygon_number, roll_degree,
        this->lens_offsets_, this->lens_sizes_, this->lens_ratio_);
    /*-------スレッド毎の処理指定-----------------------*/
    int thread_num = number_of_thread;
    /* ゼロ以下の場合強制変更。そもそもGUIでエラーにすべき */
    if (thread_num < 1) {
      thread_num = 1;
    }
    /* 高さより多い場合強制変更。そもそもGUIでエラーにすべき */
    if (height < thread_num) {
      thread_num = height;
    }
    /* threadメモリ確保 */
    this->threads_.resize(thread_num);
    /* thread set */
    int yy = 0;
    for (int ii = 0; ii < thread_num; ++ii) {
      const int y_end =
          static_cast<int>(static_cast<double>(height) * (ii + 1) / thread_num +
                           0.999999) -
          1;
      this->threads_.at(ii).setup(
          inn, out, height, width, channels, ref, ref_mode, yy, y_end,
          &(this->lens_offsets_), &(this->lens_sizes_), &(this->lens_ratio_)

                                                            ,
          radius, smooth_outer_range, polygon_number, roll_degree

          ,
          min_sw, alpha_rendering_sw, add_blend_sw);
      yy = y_end;
    }
    /*------スレッド毎のスレッド指定------*/
    for (int ii = 0; ii < thread_num; ++ii) {
      this->mthread_.add(&(this->threads_.at(ii)));
    }
  }
  void run(void) { this->mthread_.run(); }
  void clear() {
    this->mthread_.clear();
    this->threads_.clear();
    this->lens_ratio_.clear();
    this->lens_sizes_.clear();
    this->lens_offsets_.clear();
  }

private:
  std::vector<int> lens_offsets_;
  std::vector<int> lens_sizes_;
  std::vector<std::vector<double>> lens_ratio_;

  std::vector<igs::maxmin::thread<IT, RT>> threads_;
  igs::resource::multithread mthread_;
};
}
}

#endif /* !igs_maxmin_multithread_h */
