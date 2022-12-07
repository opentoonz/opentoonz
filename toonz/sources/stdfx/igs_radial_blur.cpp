#include <vector>     // std::vector
#include <cmath>      // cos(),sin(),sqrt()
#include <limits>     // std::numeric_limits<T>
#include <stdexcept>  // std::domain_error()
#include "igs_ifx_common.h"
#include "igs_radial_blur.h"

#include <QVector2D>
#include <QTransform>

namespace {
//------------------------------------------------------------------
enum Type { Accelerator = 0, Uniform_Length };

class radial_ {
  const float* in_top_;
  const int hh_;
  const int ww_;
  const int cc_;
  const TPointD center_;
  const bool antialias_sw_;
  const bool alpha_rendering_sw_;
  const double intensity_;
  const double blur_radius_;
  const double twist_radian_;
  const double pivot_radius_;
  const int type_;
  const double ellipse_aspect_ratio_;
  const double ellipse_angle_;
  const double intensity_correlation_with_ellipse_;
  QTransform tr_, tr_inv_;

public:
  radial_(const float* in_top, const int height, const int width,
          const int channels, const TPointD center, const bool antialias_sw,
          const bool alpha_rendering_sw,
          const double intensity,   /* 平均値ぼかし強度 */
          const double blur_radius, /* 平均値ぼかしの始まる半径 */
          const double twist_radian, const double pivot_radius, const int type,
          const double ellipse_aspect_ratio, const double ellipse_angle,
          const double intensity_correlation_with_ellipse)
      : in_top_(in_top)
      , hh_(height)
      , ww_(width)
      , cc_(channels)
      , center_(center)
      , antialias_sw_(antialias_sw)
      , alpha_rendering_sw_(alpha_rendering_sw)
      , intensity_(intensity)
      , blur_radius_(blur_radius)
      , twist_radian_(twist_radian)
      , pivot_radius_(pivot_radius)
      , type_(type)
      , ellipse_aspect_ratio_(ellipse_aspect_ratio)
      , ellipse_angle_(ellipse_angle)
      , intensity_correlation_with_ellipse_(
            intensity_correlation_with_ellipse) {
    if (ellipse_aspect_ratio_ != 1.0) {
      double axis_x =
          2.0 * ellipse_aspect_ratio_ / (ellipse_aspect_ratio_ + 1.0);
      double axis_y = axis_x / ellipse_aspect_ratio_;
      tr_           = QTransform()
                .rotateRadians(this->ellipse_angle_)
                .scale(axis_x, axis_y);
      tr_inv_ = QTransform(tr_).inverted();
    }
  }
  void pixel_value(const float* in_current_pixel, const int xx, const int yy,
                   const bool isRGB, const double refVal, float* result_pixel) {
    auto in_pixel = [&](int x, int y) {
      /* clamp */
      x = (x < 0) ? 0 : ((this->ww_ <= x) ? this->ww_ - 1 : x);
      y = (y < 0) ? 0 : ((this->hh_ <= y) ? this->hh_ - 1 : y);
      return this->in_top_ + this->cc_ * y * this->ww_ + this->cc_ * x;
    };
    auto interp = [&](float v1, float v2, float r) {
      return v1 * (1.f - r) + v2 * r;
    };
    auto accum_interp_in_values = [&](QPointF pos, std::vector<float>& accumP,
                                      int z1, int z2, float weight) {
      int xId          = (int)std::floor(pos.x());
      float rx         = pos.x() - (float)xId;
      int yId          = (int)std::floor(pos.y());
      float ry         = pos.y() - (float)yId;
      const float* p00 = in_pixel(xId, yId);
      const float* p01 = in_pixel(xId + 1, yId);
      const float* p10 = in_pixel(xId, yId + 1);
      const float* p11 = in_pixel(xId + 1, yId + 1);
      for (int zz = z1; zz <= z2; zz++) {
        accumP[zz] += weight * interp(interp(p00[zz], p01[zz], rx),
                                      interp(p10[zz], p11[zz], rx), ry);
      }
    };
    auto accum_in_values = [&](QPointF pos, std::vector<float>& accumP, int z1,
                               int z2, float weight) {
      int xId        = (int)std::round(pos.x());
      int yId        = (int)std::round(pos.y());
      const float* p = in_pixel(xId, yId);
      for (int zz = z1; zz <= z2; zz++) {
        accumP[zz] += weight * p[zz];
      }
    };

    int c1, c2;
    if (isRGB) {
#if defined RGBA_ORDER_OF_TOONZ6
      c1 = igs::image::rgba::blu;
      c2 = igs::image::rgba::red;
#elif defined RGBA_ORDER_OF_OPENGL
      c1 = igs::image::rgba::red;
      c2 = igs::image::rgba::blu;
#else
      Must be define / DRGBA_ORDER_OF_TOONZ6 or / DRGBA_ORDER_OF_OPENGL
#endif
    } else {
      c1 = igs::image::rgba::alp;
      c2 = igs::image::rgba::alp;
    }

    const QPointF center(this->center_.x, this->center_.y);

    /* Pixel位置 */
    const QPointF p(static_cast<float>(xx), static_cast<float>(yy));

    /* 中心からPixel位置へのベクトルと長さ */
    const QVector2D v(p - center);
    const float dist = v.length();

    /* 指定半径の範囲内なら何もしない */
    bool is_in_blur_radius = false;
    if (this->ellipse_aspect_ratio_ == 1.f) {
      is_in_blur_radius = (dist <= this->blur_radius_);
    } else {
      is_in_blur_radius =
          QVector2D(this->tr_inv_.map(v.toPointF())).lengthSquared() <=
          (this->blur_radius_ * this->blur_radius_);
    }
    if (is_in_blur_radius) {
      for (int c = c1; c <= c2; ++c) {
        result_pixel[c] = in_current_pixel[c];
      }
      return;
    }

    /* 積算値と積算回数 */
    std::vector<float> accum_val(this->cc_);
    float accum_counter = 0.f;

    /* Radial方向のSamplingの開始位置と終了位置 */
    float scale = this->intensity_ * refVal;

    float distanceRatio = 1.f;
    float length;
    if (this->ellipse_aspect_ratio_ == 1.) {
      if (this->type_ == Accelerator)
        length = (dist - this->blur_radius_) * scale;
      else  // Uniform_Length
        length = (this->pivot_radius_ - this->blur_radius_) * scale;
    } else {
      float dist_mod = QVector2D(this->tr_inv_.map(v.toPointF())).length();
      distanceRatio  = dist / dist_mod;
      if (this->type_ == Accelerator)
        length = (dist - this->blur_radius_ * distanceRatio) * scale *
                 std::pow(distanceRatio, intensity_correlation_with_ellipse_);
      else  // Uniform_Length
        length = (this->pivot_radius_ - this->blur_radius_) * scale *
                 std::pow(distanceRatio, intensity_correlation_with_ellipse_);
    }

    float blur_length_half = length * 0.5f;

    // sampling in both directions
    float sample_length = 0.f;
    while (sample_length < blur_length_half) {
      // compute weight
      float weight = 1.f;
      if (sample_length > blur_length_half - 1.f) {
        if (antialias_sw_)
          weight = blur_length_half - sample_length;
        else
          break;
      }
      // advance to the next sample
      sample_length += weight;

      float sample_dists[2] = {dist + sample_length, dist - sample_length};
      for (int i = 0; i < 2; i++) {
        if (sample_dists[i] < 0.f) continue;  // 中心を突き抜けた場合は無視する
        QPointF v_smpl;
        float scaleVal = sample_dists[i] / dist;
        if (this->twist_radian_ == 0.0) {
          if (this->ellipse_aspect_ratio_ == 1.f)
            v_smpl = v.toPointF() * scaleVal;
          else
            v_smpl = (this->tr_inv_ *
                      QTransform(this->tr_).scale(scaleVal, scaleVal))
                         .map(v.toPointF());
        } else {  // twisted case
          if (this->ellipse_aspect_ratio_ == 1.f) {
            double tmp_twist_angle =
                twist_radian_ * (sample_dists[i] - dist) / pivot_radius_;

            float cos = std::cos(tmp_twist_angle);
            float sin = std::sin(tmp_twist_angle);

            v_smpl =
                QPointF(cos * v.x() - sin * v.y(), sin * v.x() + cos * v.y()) *
                scaleVal;
          } else {
            double tmp_twist_angle = twist_radian_ * (sample_dists[i] - dist) *
                                     distanceRatio / pivot_radius_;
            v_smpl = (this->tr_inv_ * QTransform(this->tr_)
                                          .rotateRadians(tmp_twist_angle)
                                          .scale(scaleVal, scaleVal))
                         .map(v.toPointF());
          }
        }

        if (antialias_sw_)
          accum_interp_in_values(v_smpl + center, accum_val, c1, c2, weight);
        else
          accum_in_values(v_smpl + center, accum_val, c1, c2, weight);
        accum_counter += weight;
      }
    }
    // sample the original pos
    if (antialias_sw_)
      accum_interp_in_values(p, accum_val, c1, c2, 1.f);
    else
      accum_in_values(p, accum_val, c1, c2, 1.f);
    accum_counter += 1.f;

    /* 積算しなかったとき(念のためのCheck) */
    if (accum_counter <= 0.f) {
      for (int c = c1; c <= c2; ++c) {
        result_pixel[c] = in_current_pixel[c];
      }
      return;
    }
    /* ここで画像Pixelに保存 */
    for (int c = c1; c <= c2; ++c) {
      accum_val[c] /= accum_counter;

      if (isRGB && !this->alpha_rendering_sw_ &&
          (in_current_pixel[c] < accum_val[c]) &&
          result_pixel[igs::image::rgba::alp] < 1.f) {
        result_pixel[c] =
            in_current_pixel[c] + (accum_val[c] - in_current_pixel[c]) *
                                      result_pixel[igs::image::rgba::alp];
      } else {
        result_pixel[c] = accum_val[c];
      }
    }
  }

private:
  radial_();

  /* copy constructorを無効化 */
  radial_(const radial_&);

  /* 代入演算子を無効化 */
  radial_& operator=(const radial_&);
};

void radial_convert(
    const float* in, float* out, const int margin, /* 参照画像(in)がもつ余白 */
    const TDimension out_dim,             /* 求める画像(out)のサイズ */
    const int channels, const float* ref, /* 求める画像(out)と同じ高さ、幅 */
    const TPointD center,
    const double intensity, /* 強度。ゼロより大きく2以下 */
    /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
    const double radius, /* 平均値ぼかしの始まる半径 */
    const double twist_radian, const double pivot_radius,
    const int type,          // 0: Accelerator, 1: Uniform Length
    const bool antialias_sw, /*  when true, sampled pixel will be
                                bilinear-interpolated */
    const bool alpha_rendering_sw, const double ellipse_aspect_ratio,
    const double ellipse_angle,
    const double intensity_correlation_with_ellipse) {
  assert(intensity > 0.0);

  radial_ radial(in, out_dim.ly + margin * 2, out_dim.lx + margin * 2, channels,
                 center, antialias_sw, alpha_rendering_sw, intensity, radius,
                 twist_radian, pivot_radius, type, ellipse_aspect_ratio,
                 ellipse_angle * M_PI_180, intensity_correlation_with_ellipse);

  const float* p_in =
      in + margin * (out_dim.lx + margin * 2) * channels + margin * channels;

  float* p_out       = out;
  const float* p_ref = ref;

  for (int yy = margin; yy < out_dim.ly + margin;
       ++yy, p_in += 2 * margin * channels) {
    for (int xx = margin; xx < out_dim.lx + margin;
         ++xx, p_in += channels, p_out += channels) {
      using namespace igs::image::rgba;
      float refVal = (ref) ? *p_ref : 1.f;

      // if the reference value is zero
      if (refVal == 0.f) {
        for (int c = 0; c < channels; ++c) p_out[c] = p_in[c];
        p_ref++;
        continue;
      }

      if (alpha_rendering_sw) {  // blur alpha
        radial.pixel_value(p_in, xx, yy, false, refVal, p_out);
      } else {  // use the src alpha as-is
        p_out[alp] = p_in[alp];
      }

      if (p_out[alp] == 0.f) {
        p_out[red] = p_in[red];
        p_out[gre] = p_in[gre];
        p_out[blu] = p_in[blu];
        if (ref) p_ref++;
        continue;
      }

      // blur RGB channels
      radial.pixel_value(p_in, xx, yy, true, refVal, p_out);

      if (ref) p_ref++;
    }
  }
}
}  // namespace

//--------------------------------------------------------------------
void igs::radial_blur::convert(
    const float* in, float* out, const int margin, /* 参照画像(in)がもつ余白 */
    const TDimension out_dim,             /* 求める画像(out)の大きさ*/
    const int channels, const float* ref, /* outと同じ高さ、幅 */
    const TPointD center, const double twist_radian, const double twist_radius,
    const double intensity, /* 強度。ゼロより大きく2以下 */
    /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
    const double radius, /* 平均値ぼかしの始まる半径 */
    const int type, const bool antialias_sw, const bool alpha_rendering_sw,
    const double ellipse_aspect_ratio, const double ellipse_angle,
    const double intensity_correlation_with_ellipse) {
  /* 強度のないとき */
  if (intensity <= 0.0) {
    igs::image::copy_except_margin(in, margin, out, out_dim.ly, out_dim.lx,
                                   channels);
    return;
  }
  radial_convert(in, out, margin, out_dim, channels, ref, center, intensity,
                 radius, twist_radian, twist_radius, type, antialias_sw,
                 alpha_rendering_sw, ellipse_aspect_ratio, ellipse_angle,
                 intensity_correlation_with_ellipse);
}
//-------------------- comment out end -------------------------
namespace {

double reference_margin_length_(const TPointD center, const double xp,
                                const double yp, const double intensity,
                                const double radius, const double pivot_radius,
                                const int type,
                                const double ellipse_aspect_ratio,
                                const double intensity_correlation_with_ellipse,
                                const QTransform& tr_inv) {
  const QPointF c(center.x, center.y);
  const QPointF p(xp, yp);
  const QVector2D v(p - c);
  const float dist = v.length();
  if (dist <= radius) {
    return 0;
  }
  double length;
  if (ellipse_aspect_ratio == 1.) {
    if (type == Accelerator)
      length = (dist - radius) * intensity;
    else
      length = (pivot_radius - radius) * intensity;
  } else {
    float dist_mod       = QVector2D(tr_inv.map(v.toPointF())).length();
    double distanceRatio = dist / dist_mod;
    if (type == Accelerator)
      length = (dist - radius * distanceRatio) * intensity *
               std::pow(distanceRatio, intensity_correlation_with_ellipse);
    else  // Uniform_Length
      length = (pivot_radius - radius) * intensity *
               std::pow(distanceRatio, intensity_correlation_with_ellipse);
  }
  return length;
}

}  // namespace
int igs::radial_blur::
    reference_margin(/* Twist時は正確ではない... */
                     const int height, const int width, const TPointD center,
                     const double twist_radian, const double twist_radius,
                     const double intensity, /* 強度。ゼロより大きく2以下 */
                     /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい
                      */
                     const double radius, /* 平均値ぼかしの始まる半径 */
                     const int type, const double ellipse_aspect_ratio,
                     const double ellipse_angle,
                     const double intensity_correlation_with_ellipse) {
  /* 強度のないとき、2以上のとき、なにもしない */
  if (intensity <= 0.0 || 2.0 <= intensity) {
    return 0;
  }

  double margin1 = 0;
  double margin2 = 0;

  QTransform tr_inv;
  if (ellipse_aspect_ratio != 1.0) {
    double axis_x = 2.0 * ellipse_aspect_ratio / (ellipse_aspect_ratio + 1.0);
    double axis_y = axis_x / ellipse_aspect_ratio;
    tr_inv        = QTransform()
                 .rotateRadians(ellipse_angle)
                 .scale(axis_x, axis_y)
                 .inverted();
  }

  /*
  四隅から参照する外部への最大マージンを計算する
  */
  margin1 = reference_margin_length_(
      center, -width / 2.0, -height / 2.0, intensity, radius, twist_radius,
      type, ellipse_aspect_ratio, intensity_correlation_with_ellipse, tr_inv);

  margin2 = reference_margin_length_(
      center, -width / 2.0, height / 2.0, intensity, radius, twist_radius, type,
      ellipse_aspect_ratio, intensity_correlation_with_ellipse, tr_inv);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 = reference_margin_length_(
      center, width / 2.0, -height / 2.0, intensity, radius, twist_radius, type,
      ellipse_aspect_ratio, intensity_correlation_with_ellipse, tr_inv);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 = reference_margin_length_(
      center, width / 2.0, height / 2.0, intensity, radius, twist_radius, type,
      ellipse_aspect_ratio, intensity_correlation_with_ellipse, tr_inv);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  return static_cast<int>(ceil(margin1));
}
