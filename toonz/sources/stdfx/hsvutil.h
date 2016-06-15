#pragma once

#ifndef HSVUTIL_H
#define HSVUTIL_H

void OLDRGB2HSV(double r, double g, double b, double *h, double *s, double *v);

void OLDHSV2RGB(double hue, double sat, double value, double *red,
                double *green, double *blue);

#endif
