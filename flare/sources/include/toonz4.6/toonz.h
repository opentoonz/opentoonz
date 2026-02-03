#pragma once

#ifndef _TOONZ_H_
#define _TOONZ_H_

#include <sys/types.h>
#include <sys/stat.h>
#include "tmacro.h"

#undef TNZAPI
#ifdef TNZ_IS_COMMONLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

/*---------------------------------------------------------------------------*/

/*

   MEMORIA

*/

TNZAPI int meminfo_can_I_alloc(int size);
TNZAPI int meminfo_get_lswap_free(void);

TNZAPI void drop_chunk(void *buffer);
TNZAPI void release_memory_chunk(void *buffer);
TNZAPI void *get_memory_chunk(int size);
TNZAPI int not_enough_memory(void);

TNZAPI void init_check_memory(void);

/*---------------------------------------------------------------------------*/

typedef enum {
  TOONZ_ROOT,
  TOONZ_BIN,
  TOONZ_CONFIG,
  TOONZ_XRESOURCES,
  TOONZ_MISC,
  TOONZ_SCRIPTS,
  TOONZ_PIXMAP,
  TOONZ_SHELL_BUTTON,
  TOONZ_FLIP_BUTTON,
  TOONZ_WORK,
  TOONZ_ADM,
  TOONZ_TUTORIAL,
  TOONZ_SETUP_FILE,
  TOONZ_TMP,
  TOONZ_VARTMP,
  TOONZ_LINETEST_DATABASE,

  TOONZ_DIR_HOW_MANY
} TOONZ_DIRECTORY;
/*
  NON FARE 'free' del puntatore ritornato
  CONTROLLARE sempre la validita' del puntatore ritornato (!=NIL)

  TOONZ_TMP e TOONZ_VARTMP possono coincidere
  tipicamente valgono c:\temp\tmp_toonz su NT
  e rispettivamente /tmp/tmp_toonz e /var/tmp/tmp_toonz su Irix
*/

/* Introduciamo tnz_access per differenziarla sui vari systems
   si puo' pensare di metterla altrove ( a vostra discrezione ) */

#ifdef WIN32

#include <stdio.h>
#include <direct.h>
/* Per access */
#define TNZ_F_OK 0x00
#define TNZ_R_OK 0x04
#define TNZ_W_OK 0x02
#define TNZ_X_OK 0x08

/* Per chmod */
#define TNZ_S_IRWXU _S_IWRITE | _S_IREAD /* read, write, execute: owner */
#define TNZ_S_IRUSR _S_IREAD             /* read permission: owner */
#define TNZ_S_IWUSR _S_IWRITE            /* write permission: owner */
#define TNZ_S_IXUSR 0                    /* execute permission: owner */
#define TNZ_S_IRWXG TNZ_S_IRWXU          /* read, write, execute: group */
#define TNZ_S_IRGRP TNZ_S_IRUSR          /* read permission: group */
#define TNZ_S_IWGRP TNZ_S_IWUSR          /* write permission: group */
#define TNZ_S_IXGRP TNZ_S_IXUSR          /* execute permission: group */
#define TNZ_S_IRWXO TNZ_S_IRWXU          /* read, write, execute: other */
#define TNZ_S_IROTH TNZ_S_IRUSR          /* read permission: other */
#define TNZ_S_IWOTH TNZ_S_IWUSR          /* write permission: other */
#define TNZ_S_IXOTH TNZ_S_IXUSR          /* execute permission: other */

#else

#include <unistd.h>
/* Per access */
#define TNZ_F_OK F_OK
#define TNZ_R_OK R_OK
#define TNZ_W_OK W_OK
#define TNZ_X_OK X_OK

/* Per chmod */
#define TNZ_S_IRWXU S_IRWXU /* read, write, execute: owner */
#define TNZ_S_IRUSR S_IRUSR /* read permission: owner */
#define TNZ_S_IWUSR S_IWUSR /* write permission: owner */
#define TNZ_S_IXUSR S_IXUSR /* execute permission: owner */
#define TNZ_S_IRWXG S_IRWXG /* read, write, execute: group */
#define TNZ_S_IRGRP S_IRGRP /* read permission: group */
#define TNZ_S_IWGRP S_IWGRP /* write permission: group */
#define TNZ_S_IXGRP S_IXGRP /* execute permission: group */
#define TNZ_S_IRWXO S_IRWXO /* read, write, execute: other */
#define TNZ_S_IROTH S_IROTH /* read permission: other */
#define TNZ_S_IWOTH S_IWOTH /* write permission: other */
#define TNZ_S_IXOTH S_IXOTH /* execute permission: other */

#endif

/*---------------------------------------------------------------------------*/
/* Prototipi delle funzioni contenute nella libreria common */

// TNZAPI char  *strsave (const char *str);
TNZAPI char *strnsave(const char *str, int n);

TNZAPI int tnz_filestrcmp(const char *str1, const char *str2);
TNZAPI int tnz_filestrncmp(const char *str1, const char *str2, int n);
TNZAPI int tnz_strcasecmp(const char *str1, const char *str2);
TNZAPI int tnz_strncasecmp(const char *str1, const char *str2, int n);

TNZAPI char *tnz_get_user_name(void);
TNZAPI char *tnz_get_group_name(void);
TNZAPI char *tnz_get_host_name(void);

TNZAPI char *tnzdir_get(TOONZ_DIRECTORY dir);

TNZAPI int tnz_access(char *filename, int mode);
TNZAPI int tnz_stat(const char *name, struct stat *info);
TNZAPI int tnz_mkdir(const char *path, int mode);
TNZAPI int tnz_chdir(const char *path);
TNZAPI int tnz_chmod(const char *path, int mode);
TNZAPI int tnz_rename(const char *oldpath, const char *newpath);
TNZAPI int tnz_remove(const char *path);

TNZAPI void tnz_sleep(const float seconds);
TNZAPI int tnz_ms_time(void);
TNZAPI double tnz_sec_time(void);

TNZAPI void tnz_random_seed(UINT seed);
TNZAPI UCHAR tnz_random_byte(void);  /* in [0, 255] */
TNZAPI int tnz_random_int(void);     /* in [MININT, MAXINT] */
TNZAPI UINT tnz_random_uint(void);   /* in [0, (UINT)MAXINT*2+1] */
TNZAPI float tnz_random_float(void); /* in [0.0, 1.0) */

typedef struct trandom *TRANDOM;

TNZAPI TRANDOM trandom_create(void); /* seed: 0 */
TNZAPI void trandom_destroy(TRANDOM trandom);
TNZAPI void trandom_seed(TRANDOM trandom, UINT seed);
TNZAPI UCHAR trandom_byte(TRANDOM trandom);  /* in [0, 255] */
TNZAPI int trandom_int(TRANDOM trandom);     /* in [MININT, MAXINT] */
TNZAPI UINT trandom_uint(TRANDOM trandom);   /* in [0, (UINT)MAXINT*2+1] */
TNZAPI float trandom_float(TRANDOM trandom); /* in [0.0, 1.0) */

TNZAPI short swap_short(short val);
TNZAPI long swap_long(long val);
TNZAPI USHORT swap_ushort(USHORT val);
TNZAPI ULONG swap_ulong(ULONG val);

#ifdef WIN32

TNZAPI TBOOL IsWin2000(void);

#endif

/*temporaneo*/
TNZAPI char *tnzdir_get_toonz_linetest_database(void);

/*---------------------------------------------------------------------------*/
/* Prototipi delle funzioni contenute nella libreria security.a */

int GetLibProtection(void);

#endif
