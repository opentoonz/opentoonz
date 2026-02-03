

#include "trop.h"
#include "hsvutil.h"

void OLDRGB2HSV(double r, double g, double b, double *h, double *s, double *v) {
  double max, min;
  double delta;

  max = std::max({r, g, b});
  min = std::min({r, g, b});

  *v = max;

  if (max != 0)
    *s = (max - min) / max;
  else
    *s = 0;

  if (*s == 0)
    *h = 0;
  else {
    delta = max - min;

    if (r == max)
      *h = (g - b) / delta;
    else if (g == max)
      *h = 2 + (b - r) / delta;
    else if (b == max)
      *h = 4 + (r - g) / delta;
    *h   = *h * 60;
    if (*h < 0) *h += 360;
  }
}

void OLDHSV2RGB(double hue, double sat, double value, double *red,
                double *green, double *blue) {
  int i;
  double p, q, t, f;

  //  if (hue > 360 || hue < 0)
  //    hue=0;
  if (hue > 360) hue -= ((int)hue / 360) * 360;
  // hue-=360;
  if (hue < 0) hue += (1 - (int)hue / 360) * 360;
  // hue+=360;
  if (sat < 0) sat     = 0;
  if (sat > 1) sat     = 1;
  if (value < 0) value = 0;
  if (value > 1) value = 1;
  if (sat == 0) {
    *red   = value;
    *green = value;
    *blue  = value;
  } else {
    if (hue == 360) hue = 0;

    hue = hue / 60;
    i   = (int)hue;
    f   = hue - i;
    p   = value * (1 - sat);
    q   = value * (1 - (sat * f));
    t   = value * (1 - (sat * (1 - f)));

    switch (i) {
    case 0:
      *red   = value;
      *green = t;
      *blue  = p;
      break;
    case 1:
      *red   = q;
      *green = value;
      *blue  = p;
      break;
    case 2:
      *red   = p;
      *green = value;
      *blue  = t;
      break;
    case 3:
      *red   = p;
      *green = q;
      *blue  = value;
      break;
    case 4:
      *red   = t;
      *green = p;
      *blue  = value;
      break;
    case 5:
      *red   = value;
      *green = p;
      *blue  = q;
      break;
    }
  }
}
