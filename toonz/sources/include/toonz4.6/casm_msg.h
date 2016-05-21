#pragma once

#ifndef _MSG_H_
#define _MSG_H_

#include <stdarg.h>

#undef TNZAPI
#ifdef TNZ_IS_CASMLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

TNZAPI void casm_msg(int msg_type, char *format, ...);

#endif
