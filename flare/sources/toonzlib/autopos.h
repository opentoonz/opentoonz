#pragma once

#ifndef AUTOPOS_H
#define AUTOPOS_H

//#include "cleanup.h"
#include "trasterimage.h"
#include "toonz/cleanupparameters.h"

/*---------------------------------------------------------------------------*/

bool do_autoalign(const TRasterImageP &image);

int compute_strip_pixel(CleanupTypes::FDG_INFO *fdg, double dpi);

void convert_dots_mm_to_pixel(CleanupTypes::DOT *dots, int nd, double x_res,
                              double y_res);

bool get_image_rotation_and_center(const TRasterP &img, int strip_width,
                                  CleanupTypes::PEGS_SIDE pegs_side,
                                  double *p_ang, double *cx, double *cy,
                                  CleanupTypes::DOT ref[], int ref_dot);

#endif
