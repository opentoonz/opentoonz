#include <iostream>   // std::cout
#include <vector>     // std::vector
#include <cmath>      // cos(),sin(),sqrt()
#include <limits>     // std::numeric_limits<T>
#include <stdexcept>  // std::domain_error()
#include "igs_ifx_common.h"
#include "igs_rotate_blur.h"

#include <QVector2D>
#include <QTransform>
namespace {

enum Type { Accelerator = 0, Uniform_Angle, Uniform_Length };

//------------------------------------------------------------------
class Rotator {
  const float* in_top_;
  const int hh_;
  const int ww_;
  const int cc_;
  const TPointD center_;
  const bool antialias_sw_;
  const bool alpha_rendering_sw_;
  const double radian_;
  const double blur_radius_;
  const double spin_radius_;
  const int type_;
  const double ellipse_aspect_ratio_;
  const double ellipse_angle_;
  QTransform tr_, tr_inv_;

public:
  Rotator(const float* in_top, const int height, const int width,
          const int channels, const TPointD center, const bool antialias_sw,
          const bool alpha_rendering_sw, const double radian,
          const double blur_radius, const double spin_radius, const int type,
          const double ellipse_aspect_ratio, const double ellipse_angle)
      : in_top_(in_top)
      , hh_(height)
      , ww_(width)
      , cc_(channels)
      , center_(center)
      , antialias_sw_(antialias_sw)
      , alpha_rendering_sw_(alpha_rendering_sw)
      , radian_(radian)
      , blur_radius_(blur_radius)
      , spin_radius_(spin_radius)
      , type_(type)
      , ellipse_aspect_ratio_(ellipse_aspect_ratio)
      , ellipse_angle_(ellipse_angle) {
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
    float accum_counter = 0.f;  // TODO 重みづけを均一以外も選べるようにしたい

    /* 参照画像による強弱付加 */
    float radian = this->radian_ * refVal;  // TODO: it can be bidirectional..

    if (type_ == Accelerator) { /* 外への強調 */
      radian *= (dist - this->blur_radius_) / this->spin_radius_;
    } else if (type_ == Uniform_Length &&
               dist > 0.) {  // decrease radian so that the blur length becomes
                             // the same along radius
      radian *= (this->spin_radius_ + this->blur_radius_) / dist;
      radian = std::min(radian, 2.f * (float)M_PI);
    }

    // blur pixel length at the current pos
    float spin_length_half = dist * radian * 0.5f;

    // sampling in both directions
    float sample_length = 0.f;
    while (sample_length < spin_length_half) {
      // compute weight
      float weight = 1.f;
      if (sample_length >= spin_length_half - 1.f) {
        if (antialias_sw_)
          weight = spin_length_half - sample_length;
        else
          break;
      }
      // advance to the next sample
      sample_length += weight;

      // compute for both side
      float sample_radian = sample_length / dist;
      QPointF vrot1, vrot2;
      if (this->ellipse_aspect_ratio_ == 1.f) {
        float cos = std::cos(sample_radian);
        float sin = std::sin(sample_radian);

        vrot1 = QPointF(cos * v.x() - sin * v.y(), sin * v.x() + cos * v.y());
        vrot2 = QPointF(cos * v.x() + sin * v.y(), -sin * v.x() + cos * v.y());
      } else {
        vrot1 =
            (this->tr_inv_ * QTransform(this->tr_).rotateRadians(sample_radian))
                .map(v.toPointF());
        vrot2 = (this->tr_inv_ *
                 QTransform(this->tr_).rotateRadians(-sample_radian))
                    .map(v.toPointF());
      }
      if (antialias_sw_) {
        accum_interp_in_values(vrot1 + center, accum_val, c1, c2, weight);
        accum_interp_in_values(vrot2 + center, accum_val, c1, c2, weight);
      } else {
        accum_in_values(vrot1 + center, accum_val, c1, c2, weight);
        accum_in_values(vrot2 + center, accum_val, c1, c2, weight);
      }
      accum_counter += weight * 2.f;
    }

    // sample the original pos
    if (antialias_sw_)
      accum_interp_in_values(p, accum_val, c1, c2, 1.f);
    else
      accum_in_values(p, accum_val, c1, c2, 1.f);
    accum_counter += 1.f;

    //}
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
        /* 増分のみMask! */
        result_pixel[c] =
            in_current_pixel[c] + (accum_val[c] - in_current_pixel[c]) *
                                      result_pixel[igs::image::rgba::alp];
      } else {
        result_pixel[c] = accum_val[c];
      }
    }
  }

private:
  // disable
  Rotator();
  Rotator(const Rotator&);
  Rotator& operator=(const Rotator&) {}
};
//------------------------------------------------------------------

void rotate_convert(
    const float* in, float* out, const int margin, /* 参照画像(in)がもつ余白 */
    const TDimension out_dim,             /* 求める画像(out)のサイズ */
    const int channels, const float* ref, /* 求める画像(out)と同じ高さ、幅 */
    const TPointD center, const double degree,
    const double blur_radius, /* ぼかしの始まる半径 */
    const double spin_radius, /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    const int type,  // 0: Accelerator, 1: Uniform Angle, 2: Uniform Length
    const bool antialias_sw, /*  when true, sampled pixel will be
                                bilinear-interpolated */
    const bool alpha_rendering_sw, const double ellipse_aspect_ratio,
    const double ellipse_angle) {
  assert(degree > 0.0);

  Rotator rotator(in, out_dim.ly + margin * 2, out_dim.lx + margin * 2,
                  channels, center, antialias_sw, alpha_rendering_sw,
                  degree * M_PI_180, blur_radius, spin_radius, type,
                  ellipse_aspect_ratio, ellipse_angle * M_PI_180);

  const float* p_in =
      in + margin * (out_dim.lx + margin * 2) * channels + margin * channels;
  float* p_out       = out;
  const float* p_ref = ref;  // may be nullptr

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
        rotator.pixel_value(p_in, xx, yy, false, refVal, p_out);
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
      rotator.pixel_value(p_in, xx, yy, true, refVal, p_out);

      if (ref) p_ref++;
    }
  }
}

//------------------------------------------------------------------
}  // namespace

void igs::rotate_blur::convert(
    const float* in, float* out, const int margin,
    const TDimension out_dim,             /* 求める画像(out)の大きさ */
    const int channels, const float* ref, /* outと同じ高さ、幅 */
    const TPointD center, const double degree, /* ぼかしの回転角度 */
    const double blur_radius,                  /* ぼかしの始まる半径 */
    const double spin_radius, /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    const int type,  // 0: Accelerator, 1: Uniform Angle, 2: Uniform Length
    const bool antialias_sw, /* when true, sampled pixel will be
                                bilinear-interpolated */
    const bool alpha_rendering_sw, const double ellipse_aspect_ratio,
    const double ellipse_angle) {
  /* 強度のないとき */
  if (degree <= 0.0) {
    igs::image::copy_except_margin(in, margin, out, out_dim.ly, out_dim.lx,
                                   channels);
    return;
  }

  rotate_convert(in, out, margin, out_dim, channels, ref, center, degree,
                 blur_radius, spin_radius, type, antialias_sw,
                 alpha_rendering_sw, ellipse_aspect_ratio, ellipse_angle);
}
//--------------------------------------------------------------------
namespace {
double reference_margin_length_(const TPointD center, const double xp,
                                const double yp, double radian,
                                const double blur_radius,
                                const double spin_radius, const int type) {
  const QPointF c(center.x, center.y);
  const QPointF p(xp, yp);
  const QVector2D v(p - c);
  const float dist = v.length();

  if (type == Accelerator) { /* 外への強調 */
    radian *= (dist - blur_radius) / spin_radius;
  } else if (type == Uniform_Length &&
             dist > 0.) {  // decrease radian so that the blur length becomes
                           // the same along radius
    radian *= (spin_radius + blur_radius) / dist;
    radian = std::min(radian, 2. * M_PI);
  }

  double cosval = cos(radian / 2.0);
  double sinval = sin(radian / 2.0);
  QPointF vrot1(v.x() * cosval - v.y() * sinval,
                v.x() * sinval + v.y() * cosval);
  QPointF vrot2(v.x() * cosval + v.y() * sinval,
                -v.x() * sinval + v.y() * cosval);
  float dist1 = QVector2D(vrot1 + c - p).length();
  float dist2 = QVector2D(vrot2 + c - p).length();

  return (dist1 < dist2) ? dist2 : dist1;
}
}  // namespace
int igs::rotate_blur::reference_margin(
    const int height, /* 求める画像(out)の高さ */
    const int width,  /* 求める画像(out)の幅 */
    const TPointD center, const double degree, /* ぼかしの回転角度 */
    const double blur_radius,                  /* ぼかしの始まる半径 */
    const double spin_radius, /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    const int type,  // 0: Accelerator, 1: Uniform Angle, 2: Uniform Length
    const double ellipse_aspect_ratio) {
  /* 強度のないとき、なにもしない */
  if (degree <= 0.0) {
    return 0;
  }

  double margin1 = 0;
  double margin2 = 0;

  double deg = degree;
  if (180.0 < deg) {
    deg = 180.0;
  }

  margin1 =
      reference_margin_length_(center, -width / 2.0, -height / 2.0,
                               deg * M_PI_180, blur_radius, spin_radius, type);

  margin2 =
      reference_margin_length_(center, -width / 2.0, height / 2.0,
                               deg * M_PI_180, blur_radius, spin_radius, type);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 =
      reference_margin_length_(center, width / 2.0, -height / 2.0,
                               deg * M_PI_180, blur_radius, spin_radius, type);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 =
      reference_margin_length_(center, width / 2.0, height / 2.0,
                               deg * M_PI_180, blur_radius, spin_radius, type);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  // Consider ellipse deformation.
  // Instead of precise computing, return the maximum possible value.
  if (ellipse_aspect_ratio != 1.0) {
    double axis_x = 2.0 * ellipse_aspect_ratio / (ellipse_aspect_ratio + 1.0);
    double axis_y = axis_x / ellipse_aspect_ratio;
    margin1 *= std::max(axis_x, axis_y);
  }

  return static_cast<int>(ceil(margin1));
}
