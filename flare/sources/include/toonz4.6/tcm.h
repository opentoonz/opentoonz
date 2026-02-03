#pragma once

#ifndef _TCM_H_
#define _TCM_H_

#include "tmacro.h"

/* TCM = toonz color map/mapping/mapped/manager */

typedef struct {
  /*UCHAR    tone_offs; sempre 0 */
  UCHAR tone_bits;
  UCHAR color_offs;
  UCHAR color_bits;
  UCHAR pencil_offs;
  UCHAR pencil_bits;
  USHORT offset_mask; /* fa allo stesso tempo sia da offset che da maschera */
  USHORT default_val; /* da utilizzare, p.es., per pixel fuori dall'immagine */
  short n_tones;
  short n_colors;
  short n_pencils;
} TCM_INFO;

static const TCM_INFO Tcm_old_default_info = {
    /*0,*/ 4, 4, 5, 9, 2, 0x0800, 0x080f, 16, 32, 4};
static const TCM_INFO Tcm_new_default_info = {
    /*0,*/ 4, 4, 7, 11, 5, 0x0000, 0x000f, 16, 128, 32};
static const TCM_INFO Tcm_24_default_info = {
    /*0,*/ 8, 8, 8, 16, 8, 0x0000, 0x00ff, 256, 256, 256};

static const TCM_INFO Tcm_32_default_info = {
    /*0,*/ 8, 8, 12, 20, 12, 0x0000, 0x00ff, 256, 4096, 4096};

#define TCM_TONE_MASK(TCM) ((1U << (TCM).tone_bits) - 1U)
#define TCM_COLOR_MASK(TCM)                                                    \
  (((1U << (TCM).color_bits) - 1U) << (TCM).color_offs)
#define TCM_PENCIL_MASK(TCM)                                                   \
  (((1U << (TCM).pencil_bits) - 1U) << (TCM).pencil_offs)

#define TCM_COLOR_INDEX(TCM, ID)                                               \
  ((ID) << (TCM).color_offs | ((TCM).n_tones - 1) | (TCM).offset_mask)
#define TCM_PENCIL_INDEX(TCM, ID)                                              \
  ((ID) << (TCM).pencil_offs | (TCM).offset_mask)

#define TCM_INDEX_IS_COLOR_ONLY(TCM, INDEX)                                    \
  (((INDEX)&TCM_TONE_MASK(TCM)) == TCM_TONE_MASK(TCM))
#define TCM_INDEX_IS_PENCIL_ONLY(TCM, INDEX) (((INDEX)&TCM_TONE_MASK(TCM)) == 0)

#define TCM_COLOR_ID(TCM, INDEX)                                               \
  ((int)(((INDEX) >> (TCM).color_offs) & ((1U << (TCM).color_bits) - 1U)))
#define TCM_PENCIL_ID(TCM, INDEX)                                              \
  ((int)(((INDEX) >> (TCM).pencil_offs) & ((1U << (TCM).pencil_bits) - 1U)))

#define TCM_MIN_CMAP_BUFFER_SIZE(TCM)                                          \
  ((((TCM).n_pencils - 1) << (TCM).pencil_offs |                               \
    ((TCM).n_colors - 1) << (TCM).color_offs | (TCM).n_tones - 1) +            \
   1)

#define TCM_MIN_CMAP_COLBUFFER_SIZE(TCM) ((TCM).n_colors * (TCM).n_tones)

#define TCM_MIN_CMAP_PENBUFFER_SIZE(TCM) ((TCM).n_pencils * (TCM).n_tones)

#define TCM_CMAP_BUFFER_SIZE(TCM)                                              \
  (1 << ((TCM).pencil_bits + (TCM).color_bits + (TCM).tone_bits))

#define TCM_CMAP_COLBUFFER_SIZE(TCM) (1 << ((TCM).color_bits + (TCM).tone_bits))

#define TCM_CMAP_PENBUFFER_SIZE(TCM)                                           \
  (uint64_t(1) << ((TCM).pencil_bits + (TCM).tone_bits))

#endif
