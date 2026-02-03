#include "igs_color_rgb_hls.h"
void igs::color::rgb_to_hls(const double red,  // 0.0...1.0
                            const double gre,  // 0.0...1.0
                            const double blu,  // 0.0...1.0
                            double &hue,       /* 0.0...360.0	hue(色相) */
                            double &lig,       /* 0.0...1.0	lightness(明度) */
                            double &sat,      /* 0.0...1.0	saturation(彩度) */
                            bool cylindrical  // either cylindrical or conical
) {
  const double maxi =
      (red < gre) ? ((gre < blu) ? blu : gre) : ((red < blu) ? blu : red);
  const double mini =
      (gre < red) ? ((blu < gre) ? blu : gre) : ((blu < red) ? blu : red);

  lig = (maxi + mini) / 2.0;

  if (maxi == mini) { /* RGB各値に差がない(=白黒で色がない) */
    sat = 0.0;        /* saturation(彩度)はゼロ */
    hue = 0.0;        /* hue(色相)は意味を持たない */
  } else {            /* 色のあるとき */
    if (cylindrical) {
      if (lig <= 0.5) {
        sat = (maxi - mini) / (maxi + mini);
      } else {
        sat = (maxi - mini) / (2.0 - (maxi + mini));
      }
    } else
      sat = maxi - mini;

    /* 色相(Hue) */
    const double rmid = (maxi - red) / (maxi - mini);
    const double gmid = (maxi - gre) / (maxi - mini);
    const double bmid = (maxi - blu) / (maxi - mini);

    /* -1 .. 1 マゼンタ(M)から黄色(Y)までの間の色 */
    if (red == maxi) {
      hue = bmid - gmid;
    }

    /*  1 .. 3 黄色(Y)からシアン(C)までの間の色 */
    else if (gre == maxi) {
      hue = 2.0 + rmid - bmid;
    }

    /*  3 .. 5 シアン(C)からマゼンタ(M)までの間の色 */
    else if (blu == maxi) {
      hue = 4.0 + gmid - rmid;
    }
    /*
            M-R-Y-G-C-B-M
           -1 0 1 2 3 4 5
    */
    hue *= 60.0; /* -60 ... 300 */
    if (hue < 0.0) {
      hue += 360.0;
    }
  }
}
namespace {
double hls2rgb_calc(const double m1, const double m2, const double hue) {
  double hh = hue;
  while (360.0 < hh) {
    hh -= 360;
  }
  while (hh < 0.0) {
    hh += 360;
  }
  if (hh < 60.0) {
    return m1 + (m2 - m1) * hh / 60.0;
  } else if (hh < 180.0) {
    return m2;
  } else if (hh < 240.0) {
    return m1 + (m2 - m1) * (240.0 - hh) / 60.0;
  }
  return m1; /* 240 ... 360 */
}
}  // namespace
void igs::color::hls_to_rgb(const double hue, /* 0.0...360.0	hue(色相) */
                            const double lig, /* 0.0...1.0 lightness(明度) */
                            const double sat, /* 0.0...1.0 saturation(彩度) */
                            double &red,      /* 0.0...1.0 */
                            double &gre,      /* 0.0...1.0 */
                            double &blu,      /* 0.0...1.0 */
                            bool cylindrical  // either cylindrical or conical
) {
  if (0.0 == sat) { /* 白黒で色がない */
    red = gre = blu = lig;
  } else { /* 色のあるとき */
    double m2, m1;
    if (cylindrical) {
      m2 = (lig <= 0.5) ? (lig * (1.0 + sat)) : (lig + sat - lig * sat);
      m1 = 2.0 * lig - m2;
    } else {
      m2 = lig + sat * 0.5;
      m1 = lig - sat * 0.5;
    }
    red = hls2rgb_calc(m1, m2, hue + 120.0);
    gre = hls2rgb_calc(m1, m2, hue);
    blu = hls2rgb_calc(m1, m2, hue - 120.0);
  }
}
