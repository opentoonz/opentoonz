#pragma once

#ifndef __PATTERNMAP_H_
#define __PATTERNMAP_H_

int patternmap(RASTER *, RASTER *, const int border,
			   int argc, const char *argv[],
			   const int shrink, RASTER *imgContour);

#endif
