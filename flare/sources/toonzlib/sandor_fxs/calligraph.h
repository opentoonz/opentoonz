#pragma once

#ifndef __CALLIGRAPH_H_
#define __CALLIGRAPH_H_

#ifdef __cplusplus
extern "C" {
#endif

int calligraph(const RASTER *inr, RASTER *outr, const int border, int argc,
               const char *argv[], const int shrink, bool isOutBorder);

#ifdef __cplusplus
}
#endif

#endif
