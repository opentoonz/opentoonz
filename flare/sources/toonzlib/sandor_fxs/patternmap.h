#pragma once

#ifndef __PATTERNMAP_H_
#define __PATTERNMAP_H_

#ifdef __cplusplus
extern "C" {
#endif

int patternmap(const RASTER *, RASTER *, const int border, int argc,
               const char *argv[], const int shrink, RASTER *imgContour);

#ifdef __cplusplus
}
#endif

#endif
