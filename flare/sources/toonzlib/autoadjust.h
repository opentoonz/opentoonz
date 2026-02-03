#pragma once

#ifndef _AUTOADJUST_H_
#define _AUTOADJUST_H_

#include "trasterimage.h"
void set_autoadjust_window(int x0, int y0, int x1, int y1);

void black_eq_algo(const TRasterImageP &image);

void build_lw(const TRasterImageP &image, float lw[256]);
void build_lw_lut(float ref_lw[256], float lw[256], UCHAR lut[256]);

void build_gr_cum(const TRasterImageP &image, int cum[256]);
void build_gr_lut(int ref_cum[256], int cum[256], UCHAR lut[256]);
void apply_lut(const TRasterImageP &image, UCHAR lut[256]);

void histo_l_algo(const TRasterImageP &image, int reference);

int eval_image_th(const TRasterImageP &image, int *threshold, float *linewidth);
// void thresh_image (const TRasterImageP &image, int threshold, int
// oversample_factor);

void autoadj_set_black_value(int black);

#endif
