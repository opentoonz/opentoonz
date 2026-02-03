#include "igs_color_rgb_hsv.h"
#include <cmath>
void igs::color::rgb_to_hsv(const double red,  // 0.0...1.0
                            const double gre,  // 0.0...1.0
                            const double blu,  // 0.0...1.0
                            double &hue,       /* 0.0...360.0	hue(色相) */
                            double &sat, /* 0.0...1.0	saturation(彩度) */
                            double &val  /* 0.0...1.0	value(明度) */
) {
  const double maxi =
      (red < gre) ? ((gre < blu) ? blu : gre) : ((red < blu) ? blu : red);
  const double mini =
      (gre < red) ? ((blu < gre) ? blu : gre) : ((blu < red) ? blu : red);

  bool invertValue = std::abs(mini) > std::abs(maxi);

  val = (invertValue) ? mini : maxi; /* value(明度) */

  if (maxi == mini) { /* RGB各色に差がない(白黒の)とき */
    sat = 0.0;        /* saturation(彩度)はゼロ */
    hue = 0.0;        /* hue(色相)は意味を持たない */
  } else {            /* 色のあるとき */
    sat = (invertValue) ? (mini - maxi) / mini : (maxi - mini) / maxi;

    const double maxmin = maxi - mini;

    /* -1 .. 1 マゼンタ(M)から黄色(Y)までの間の色 */
    if (red == maxi) {
      hue = (gre - blu) / maxmin;
    }

    /*  1 .. 3 黄色(Y)からシアン(C)までの間の色 */
    else if (gre == maxi) {
      hue = 2.0 + (blu - red) / maxmin;
    }

    /*  3 .. 5 シアン(C)からマゼンタ(M)までの間の色 */
    else if (blu == maxi) {
      hue = 4.0 + (red - gre) / maxmin;
    }
    /*
            M-R-Y-G-C-B-M
           -1 0 1 2 3 4 5
    */
    hue *= 60.0; /* -60 ... 300 */
    if (invertValue) hue -= 180.0;
    if (hue < 0.0) {
      hue += 360.0;
    }
  }
}
#include <cmath>                              /* floor() */
void igs::color::hsv_to_rgb(const double hue, /* 0.0...360.0	hue(色相) */
                            const double sat, /* 0.0...1.0 saturation(彩度) */
                            const double val, /* 0.0...1.0	value(明度) */
                            double &red,      /* 0.0...1.0 */
                            double &gre,      /* 0.0...1.0 */
                            double &blu       /* 0.0...1.0 */
) {
  if (0.0 == sat) { /* 白黒で色がない */
    red = gre = blu = val;
  } else { /* 色のあるとき */
    double hh = hue;
    while (360.0 <= hh) {
      hh -= 360.0;
    }
    hh /= 60.0; /* 0〜359.999... --> 0〜5.999... */

    const double ii = floor(hh); /* 最大整数値 */
    const double ff = hh - ii;   /* hueの小数部 */
    const double pp = val * (1.0 - sat);
    const double qq = val * (1.0 - (sat * ff));
    const double tt = val * (1.0 - (sat * (1.0 - ff)));

    switch (static_cast<int>(ii)) {
    case 0:
      red = val;
      gre = tt;
      blu = pp;
      break;
    case 1:
      red = qq;
      gre = val;
      blu = pp;
      break;
    case 2:
      red = pp;
      gre = val;
      blu = tt;
      break;
    case 3:
      red = pp;
      gre = qq;
      blu = val;
      break;
    case 4:
      red = tt;
      gre = pp;
      blu = val;
      break;
    case 5:
      red = val;
      gre = pp;
      blu = qq;
      break;
    }
  }
}
