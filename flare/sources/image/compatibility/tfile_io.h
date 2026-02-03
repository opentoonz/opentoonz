#pragma once

#ifndef TFILE_IO_H
#define TFILE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef MACOSX
#include <stddef.h>
#endif

#if defined(LINUX) || defined(FREEBSD)
#include <wchar.h>
#endif

char *convertWCHAR2CHAR(const wchar_t *fname);

#if defined(MACOSX) || defined(LINUX) || defined(FREEBSD) || defined(HAIKU)

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *_wfopen(const wchar_t *fname, const wchar_t *mode);
int _wstat(const wchar_t *fname, struct stat *buf);
int _wremove(const wchar_t *fname);
#else

#endif

#ifdef __cplusplus
}
#endif

#endif
