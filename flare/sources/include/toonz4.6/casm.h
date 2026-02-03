#pragma once

#ifndef _CASM_H_
#define _CASM_H_

#include <stdarg.h>

#define MSG_I 1
#define MSG_W 2
#define MSG_E 3
#define MSG_F 4
#define MSG_IE 5
#define MSG_IF 6
#define MSG_P 7

typedef enum CASM_RESAMPLE_TYPE {
  CASM_RESAMPLE_NULL,
  CASM_RESAMPLE_STANDARD,
  CASM_RESAMPLE_IMPROVED,
  CASM_RESAMPLE_HIGH
} CASM_RESAMPLE_TYPE;

typedef struct {
  char *name;
  int (*routine)(void);
} KEYWORD;

typedef struct { double x, y, z; } VECTOR;

typedef struct CASM_WINDOW {
  int x1, y1, x2, y2;
  struct CASM_WINDOW *next;
} CASM_WINDOW;

/*---------------------------------------------------------------------------*/

#define SLICE_LX 1024
#define SLICE_LY 1024

#define LOADABLE_PIXELS (1024 * 1024) /* WARNING  it must be greater or */
                                      /* equal to SLICE_LX*SLICE_LY!    */

#define CASM_RENDER_DEFAULT 1.0

#define CASM_MEM_CHUNK_DEFAULT 1.0

/*---------------------------------------------------------------------------*/

#define INTERSECT(win1, win2, win)                                             \
  (win)->x1 = MAX((win1)->x1, (win2)->x1);                                     \
  (win)->y1 = MAX((win1)->y1, (win2)->y1);                                     \
  (win)->x2 = MIN((win1)->x2, (win2)->x2);                                     \
  (win)->y2 = MIN((win1)->y2, (win2)->y2)

#define UNION(win1, win2, win)                                                 \
  (win)->x1 = MIN((win1)->x1, (win2)->x1);                                     \
  (win)->y1 = MIN((win1)->y1, (win2)->y1);                                     \
  (win)->x2 = MAX((win1)->x2, (win2)->x2);                                     \
  (win)->y2 = MAX((win1)->y2, (win2)->y2)

#define RESIZE_RECT(win, border)                                               \
  {                                                                            \
    (win)->x1 -= border;                                                       \
    (win)->y1 -= border;                                                       \
    (win)->x2 += border;                                                       \
    (win)->y2 += border;                                                       \
  }

/*---------------------------------------------------------------------------*/

#define DDR_FILENAME " DDR"
#define DDR_FILENAME_LEN 4

/*---------------------------------------------------------------------------*/

#ifndef WIN32
#define CASM_LINK_MASK_STRING " -link "
#define CASM_LINK_QUAL_STRING                                                  \
  "-link             : forces casm to create a link to the previous image if " \
  "the\n"                                                                      \
  "                    current image is identical. By default ngcasm writes\n" \
  "                    another identical image on disk.\n"
#else
#define CASM_LINK_MASK_STRING " "
#define CASM_LINK_QUAL_STRING "\n"
#endif

#define CASM_USAGE_MASK                                                        \
  " --pt --shmem%d%s  -trace -cf -constant_focus  -intmove "                   \
  " -fracmove --pipe%d --dimensions%d%d -shrink%d -s%d --rd%d "                \
  " -hq -iq -sq -no_overwrite -no -logfile%s -gamma%f -local_render -clap "    \
  " -step%d -range%d%d -r%d%d -mem%f -tile%f -64bit -32bit -lineart%d "        \
  "-edgealiasing%d -ddr%d" CASM_LINK_MASK_STRING                               \
  " --nowait -frame%d -first%d -bc%s -ac%s -bc_args%s -ac_args%s -mm "         \
  "-mmsingle%d [filename]"

#define CASM_USAGE_STRING                                                      \
  "-range fs fe      : \n"                                                     \
  "-r fs fe          : compose the frames ranging from 'fs' to 'fe'\n"         \
  "-frame n          : compose the frame 'n' only\n"                           \
  "-step  n          : compose each 'n' frames\n"                              \
  "-intmove          : do NOT apply subpixel positioning for full-color "      \
  "levels \n"                                                                  \
  "-fracmove         : apply subpixel positioning also for anim-type\n"        \
  "-64bit            : renders 64 bit color images\n"                          \
  "-mm               : save the levels of each frame separately\n"             \
  "-mmsingle [ #col ]: computes and save only level in the #col column "       \
  "number\n"                                                                   \
  "                    in the exposure sheet \n"                               \
  "-shrink value     :  \n"                                                    \
  "-s      value     : subsample one each 'value' pixels and one each\n"       \
  "-bc scriptfile    : execute scriptfile before frame composition\n"          \
  "-ac scriptfile    : execute scriptfile after  frame composition\n"          \
  "-bc_args args     : argument(s) for 'bc' script \n"                         \
  "-ac_args args     : argument(s) for 'ac' script \n"                         \
  "-no_overwrite     : \n"                                                     \
  "-no               : skip computing of already existing frames on disk\n"    \
  "                    (useful to render scenes on multiple workstations\n"    \
  "                    sharing the same database)\n"                           \
  "-tile xsize       : dimensions (in Kilopixels) of the largest squared "     \
  "portion \n"                                                                 \
  "                    of the frame to be composed in one step; xsize does "   \
  "not \n"                                                                     \
  "                    need to be an integer. \n"                              \
  "-mem value        : dimensions (in Kilopixels) of the squared portion of\n" \
  "                    background image to be kept in RAM memory.\n"           \
  "                    It must be greater or equal to xsize of option -tile\n" \
  "-constant_focus   : \n"                                                     \
  "-cf               : forces casm to work in constant focus mode, thus "      \
  "avoiding\n"                                                                 \
  "                    abrupt changes from slight blurry to focused images "   \
  "as the \n"                                                                  \
  "                    camera zooms toward the final field size. This "        \
  "qualifier\n"                                                                \
  "                    should be used ONLY when a zoom operation creates the " \
  "above\n"                                                                    \
  "                    described problem because its usage will cause a "      \
  "longer\n"                                                                   \
  "                    compositing time and will force the camera to remain "  \
  "\n"                                                                         \
  "                    constantly out-of-focus.\n"                             \
  "-sq               :\n"                                                      \
  "-iq               :\n"                                                      \
  "-hq               : sets the resample quality respectively to standard, "   \
  "improved\n"                                                                 \
  "                    or high quality for geometrical transformations\n"      \
  "                    default: Resample type set in toonzsetup module.\n"     \
  "-logfile filename : writes a log file with all the errors and warnings.\n"  \
  "-ddr startframe   : direct output frames on a Digital Disc Recorder "       \
  "starting from specified position.\n"                                        \
  "-local_render     : makes a local copy of image files locally, to speed "   \
  "up rendering\n"                                                             \
  "-gamma value      : performs a gamma correction of specified value on all " \
  "\n"                                                                         \
  "                    rendered images\n"                                      \
  "-lineart value    : produces an aliased output image, with colors from "    \
  "original palette; \n"                                                       \
  "                    it is ignored if not used in conjunction with -mm or "  \
  "-mmsingle and on \n"                                                        \
  "                    full color input images\n"                              \
  "-edgealiasing value : produces an  output image with contour edges "        \
  "aliased;\n"                                                                 \
  "                      so, matte channel pixel values are either "           \
  "transparent or opaque; \n"                                                  \
  "                    it is ignored if not used in conjunction with -mm or "  \
  "-mmsingle and on \n"                                                        \
  "                    full color input images\n" CASM_LINK_QUAL_STRING

#endif
