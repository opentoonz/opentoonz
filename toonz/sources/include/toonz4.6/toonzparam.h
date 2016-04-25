#pragma once

#ifndef _TOONZPARAM_H__
#define _TOONZPARAM_H__

#ifdef WIN32
#include <stdlib.h>
#define TOONZMAXPATHLEN _MAX_PATH
#else
#include <sys/param.h>
#define TOONZMAXPATHLEN MAXPATHLEN
#endif

#endif
