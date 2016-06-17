#pragma once

#include "toonz4.6/img.h"
#include "toonz4.6/raster.h"
//#include "toonz4.6/toonz.h"

#define IS_SLASH(C) ((C) == '/' || (C) == '\\')
#define SLASH_CHAR '/'

struct field_desc {
  char present;
  short pos, len;
};

struct name_desc {
  struct field_desc path, name, frame, suffix, type;
};

//---------------------------------------------------------------------

int isTTT(const char *fn);

//---------------------------------------------------------------------

const char *strsave(const char *t);

//---------------------------------------------------------------------

void convertParam(double param[], const char *cParam[], int cParamLen);
