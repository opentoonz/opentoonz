#pragma once

#ifndef _TRANGE_DATA_INCLUDED_
#define _TRANGE_DATA_INCLUDED_

typedef struct TRANGE_DATA { int from, to, step, inc; } TRANGE_DATA;

#define TRANGE_AUTO_STEP (-1)
#define TRANGE_AUTO_INC (-2)

#define TRANGE_FROM_M 0x1
#define TRANGE_TO_M 0x2
#define TRANGE_STEP_M 0x4
#define TRANGE_INC_M 0x8

#endif
