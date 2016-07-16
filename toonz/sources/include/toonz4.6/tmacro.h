#pragma once

#ifndef _TMACRO_H_
#define _TMACRO_H_

#if defined(_WIN32) && !defined(WIN32)
#define WIN32
#endif

#ifndef WIN32
#include <sys/param.h>
#endif

#ifdef WIN32
//#include "tmacroW.h"
#endif

#if defined(DBMALLOC) && !defined(_WIN32)
#include "dbmalloc.h"
#endif

/*---------------------------------------------------------------------------*/

#ifdef WIN32
#define TNZ_EXPORT_API __declspec(dllexport)
#define TNZ_IMPORT_API __declspec(dllimport)
#define TNZ_EXPORT_VAR __declspec(dllexport)
#define TNZ_IMPORT_VAR __declspec(dllimport)
#else
#define TNZ_EXPORT_API
#define TNZ_IMPORT_API
#define TNZ_EXPORT_VAR extern
#define TNZ_IMPORT_VAR extern
#endif

/*---------------------------------------------------------------------------*/

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define NIL ((void *)0)

typedef int TBOOL;
typedef signed char SCHAR;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned long ULONG;

/*---------------------------------------------------------------------------*/

typedef struct { int x0, y0, x1, y1; } TRECT;
typedef struct { double x0, y0, x1, y1; } TDRECT;
typedef struct { int x0, y0, x1, y1; } TLINE;
typedef struct { double x0, y0, x1, y1; } TDLINE;
typedef struct { int x, y; } TPOINT;
typedef struct { double x, y; } TDPOINT;

/*---------------------------------------------------------------------------*/

#define STR_EQ(s1, s2) (!strcmp((s1), (s2)))
#define STR_NE(s1, s2) (strcmp((s1), (s2)))
#define STR_GE(s1, s2) (strcmp((s1), (s2)) >= 0)
#define STR_LE(s1, s2) (strcmp((s1), (s2)) <= 0)
#define STR_GT(s1, s2) (strcmp((s1), (s2)) > 0)
#define STR_LT(s1, s2) (strcmp((s1), (s2)) < 0)

#define STRN_EQ(s1, s2, n) (!strncmp((s1), (s2), (n)))
#define STRN_NE(s1, s2, n) (strncmp((s1), (s2), (n)))

#define STR_START(s1, s2) (!strncmp((s1), (s2), strlen(s2)))

#define FILESTR_EQ(s1, s2) (!tnz_filestrcmp((s1), (s2)))
#define FILESTR_NE(s1, s2) (tnz_filestrcmp((s1), (s2)))
#define FILESTR_GE(s1, s2) (tnz_filestrcmp((s1), (s2)) >= 0)
#define FILESTR_LE(s1, s2) (tnz_filestrcmp((s1), (s2)) <= 0)
#define FILESTR_GT(s1, s2) (tnz_filestrcmp((s1), (s2)) > 0)
#define FILESTR_LT(s1, s2) (tnz_filestrcmp((s1), (s2)) < 0)

#define FILESTRN_EQ(s1, s2, n) (!tnz_filestrncmp((s1), (s2), (n)))
#define FILESTRN_NE(s1, s2, n) (tnz_filestrncmp((s1), (s2), (n)))

/*---------------------------------------------------------------------------*/

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define ROUND(x)                                                               \
  ((int)(((int)(-0.9F) == 0 && (x) < 0.0F) ? ((x)-0.5F) : ((x) + 0.5F)))
/* solo per argomenti positivi: */
#define ROUNDP(x) ((int)((x) + 0.5F))

/* equivalenti a (int)floor() e (int)ceil() (ma piu' veloci)
   solo se la risoluzione dell'argomento e' inferiore ad 1 */
#define FLOOR(x) ((int)(x) > (x) ? (int)(x)-1 : (int)(x))
#define CEIL(x) ((int)(x) < (x) ? (int)(x) + 1 : (int)(x))

#define NOT_LESS_THAN(MIN, X)                                                  \
  {                                                                            \
    if ((X) < (MIN)) (X) = (MIN);                                              \
  }
#define NOT_MORE_THAN(MAX, X)                                                  \
  {                                                                            \
    if ((X) > (MAX)) (X) = (MAX);                                              \
  }

#define CROP(X, MIN, MAX) (X < MIN ? MIN : (X > MAX ? MAX : X))

#ifndef MAXINT
#define MAXINT ((int)((~0U) >> 1))
#endif
#ifndef MININT
#define MININT (~MAXINT)
#endif

/*---------------------------------------------------------------------------*/

static const char *const TALLOC_MSG_ =
    "Out of memory allocating %d bytes (%s line %d)";
static const char *const TREALLOC_MSG_ =
    "Out of memory reallocating %d bytes (%s line %d)";

#if defined(_WIN32) && defined(_DEBUG)
#define WIN32_DBG_NBFL_ , _NORMAL_BLOCK, __FILE__, __LINE__
#define WIN32_DBG_NB_ , _NORMAL_BLOCK
#define TMALLOC_FUN_ _malloc_dbg
#define TCALLOC_FUN_ _calloc_dbg
#define TREALLOC_FUN_ _realloc_dbg
#define TFREE_FUN_ _free_dbg
#else
#define WIN32_DBG_NBFL_
#define WIN32_DBG_NB_
#define TMALLOC_FUN_ malloc
#define TCALLOC_FUN_ calloc
#define TREALLOC_FUN_ realloc
#define TFREE_FUN_ free
#endif

#define TMALLOC(ptr, elem)                                                     \
  {                                                                            \
    (ptr) = (void *)TMALLOC_FUN_((elem) * sizeof(*(ptr)) WIN32_DBG_NBFL_);     \
    if (!ptr)                                                                  \
      tmsg_error(TALLOC_MSG_, (int)((elem) * sizeof(*(ptr))), __FILE__,        \
                 __LINE__);                                                    \
  }
#define TCALLOC(ptr, elem)                                                     \
  {                                                                            \
    (ptr) = (void *)TCALLOC_FUN_((elem), sizeof(*(ptr)) WIN32_DBG_NBFL_);      \
    if (!ptr)                                                                  \
      tmsg_error(TALLOC_MSG_, (int)((elem) * sizeof(*(ptr))), __FILE__,        \
                 __LINE__);                                                    \
  }
#define TREALLOC(ptr, elem)                                                    \
  {                                                                            \
    if (ptr)                                                                   \
      (ptr) = (void *)TREALLOC_FUN_((ptr),                                     \
                                    (elem) * sizeof(*(ptr)) WIN32_DBG_NBFL_);  \
    else                                                                       \
      (ptr) = (void *)TMALLOC_FUN_((elem) * sizeof(*(ptr)) WIN32_DBG_NBFL_);   \
    if (!ptr)                                                                  \
      tmsg_error(TREALLOC_MSG_, (int)((elem) * sizeof(*(ptr))), __FILE__,      \
                 __LINE__);                                                    \
  }

#define TFREE(ptr)                                                             \
  {                                                                            \
    if (ptr) {                                                                 \
      TFREE_FUN_((ptr)WIN32_DBG_NB_);                                          \
      ptr = NIL;                                                               \
    }                                                                          \
  }

/*---------------------------------------------------------------------------*/

#define STR_ASSIGN(DST, SRC)                                                   \
  {                                                                            \
    TFREE(DST);                                                                \
    DST = SRC ? strsave(SRC) : 0;                                              \
  }

#endif
