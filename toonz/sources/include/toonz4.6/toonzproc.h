#pragma once

#ifndef _TOONZPROC_H_
#define _TOONZPROC_H_

#include "tmacro.h"
#include "stdarg.h"

#undef TNZAPI
#ifdef TNZ_IS_COMMONLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

#define T_PID int

typedef struct T_CHAN_REC *T_CHAN;

#define TPROC_OPEN_READ 0x1
#define TPROC_OPEN_WRITE 0x2
#define TPROC_OPEN_READ_BINARY 0x4

#ifdef WIN32
#include "windows.h"
#define TPROC_LIB HMODULE
#define TPROC_FUNC FARPROC
#else
#define TPROC_LIB void *
typedef void (*TPROC_FUNC)(void);

/*#define TPROC_FUNC void**/
#endif

/**********************************************************************
Runs without sospending executable filename. A set of arguments to be
passed can be setted in the NULL-terminated array argv. First argument
MUST NOT be executable name.
**********************************************************************/

TNZAPI T_PID tproc_run_process(char *filename, char *argv[]);

/**********************************************************************
Runs sospending executable filename. A set of arguments to be
passed can be setted in the NULL-terminated array argv. First argument
MUST NOT be executable name. Returns TRUE if specified process was
successfully executed, FALSE otherwise.
**********************************************************************/

TNZAPI int tproc_run_process_fg(char *filename, char *argv[]);

/**********************************************************************
(IRIX ONLY, on NT is a void function).
Suspend execution of process until child process identified with pid
exits.
**********************************************************************/

TNZAPI void tproc_waitpid(T_PID pid);

/**********************************************************************
Creates a new process, sharing the virtual address space of parent.
The new process is started with a call to func, passing arg to it.
**********************************************************************/

TNZAPI T_PID tproc_sproc(void (*func)(void *), void *arg);

/**********************************************************************
gets pid of current process
**********************************************************************/

TNZAPI T_PID tproc_getpid(void);

/**********************************************************************
Check if process pid is alive or not
**********************************************************************/

TNZAPI int tproc_is_process_alive(T_PID pid);

/**********************************************************************
kills process pid (return FALSE if the process is not alive)
**********************************************************************/

TNZAPI int tproc_kill_process(T_PID pid);

#define TPROC_SIGNONE 0x0
#define TPROC_SIGABORT 0x2
#define TPROC_SIGUSER1 0x4
#define TPROC_SIGUSER2 0x8

/**********************************************************************
sets the specified callback to be executed when one of the signal
into the signal mask (made by ORing the TPROC_SIG) is sent to current process.
Warning: only one callback can be assigned to a signal type; for
each new assignation the old one is erased.
**********************************************************************/

TNZAPI int tproc_set_signals_callback(int signal_mask, void (*signal_cb)(void));

/* Note: next function is defined in twin, for it can be used only
by graphic modules; it is equal to tproc_set_signals_callback(TPROC_SIGCHLD,
signal_cb);

TNZAPI int tproc_set_sigchld_signals_callback(void (*signal_cb)(void));
*/

/**********************************************************************
erase actions to be executed  for  specified signals:
valid only with TPROC_SIGCHILD, TPROC_SIGUSER1, TPROC_SIGUSER2.
**********************************************************************/

TNZAPI void tproc_unset_signals_callback(int signal_mask);

/**********************************************************************
send to process pid one of the user signals.
**********************************************************************/

TNZAPI void tproc_send_siguser(T_PID pid, int signal);

/**********************************************************************
suspend execution of current process  until it receives a user signal.
If no user signal was set, it return without waiting.
**********************************************************************/

TNZAPI void tproc_wait_for_siguser(void);

/**********************************************************************
This function create a pipe between two process: a parent that writes
messages to a son. For NT only, bytes_dim specifies the buffer size
for the pipe (if 0, a default value is set).
It must be executed BEFORE the son process is run with the tproc_run_process.
To write to the pipe, use tproc_chan_printf.
To read from the pipe, use tproc_chan_gets
The int argument of this function must be passed from parent to son
using a program argument (that is, argv). This int argument is
retrieved by parent with tproc_get_readpipe_id.
The son process can have a chan from the int argument with
tproc_get_chan_from_pipe_id.
 *********************************************************************/

TNZAPI T_CHAN tproc_create_pipe(int bytes_dim);

/**********************************************************************
This function set the dimension of the buffer in the pipe;
It's needed for NT pipes when reading and writing happens within
the same process. On IRIX this function has no effect.
 *********************************************************************/

TNZAPI void tproc_set_pipe_dimension(int bytes_dim);

/**********************************************************************
This function opens a file for read and/or writing.
To write into the file, use tproc_chan_printf; to read,
tproc_chan_gets.
**********************************************************************/

TNZAPI T_CHAN tproc_open_file(char *filename, int mode_mask);

TNZAPI int tproc_get_readpipe_id(T_CHAN t_pipe);
TNZAPI T_CHAN tproc_get_chan_from_pipe_id(int pipeid);
TNZAPI int tproc_chan_printf(T_CHAN chan, char *format, ...);
TNZAPI void tproc_chan_flush(T_CHAN chan);

/**********************************************************************
This function gets a buffer from a stream. The reading stops when a '\n'
or a '\0' is encountered. NOTICE: the buffer is allocated and
owned by the function, which returns pointer to it or NIL if end-of-stream
is encountered. The stream can be a pipe too.
***********************************************************************/

TNZAPI char *tproc_chan_gets(T_CHAN chan);
TNZAPI int tproc_close_chan(T_CHAN chan);

/**********************************************************************
This function open a .ddl (for NT) or a .so (for IRIX) returning
an identifier to it
***********************************************************************/

TNZAPI TPROC_LIB tproc_open_dll(char *libname);

/**********************************************************************
This function return  a pointer to the function funcname defined into the
previously opened .dll (or .so) lib.
***********************************************************************/

TNZAPI TPROC_FUNC tproc_get_func(TPROC_LIB lib, char *funcname);

/**********************************************************************
This function returns a unique filename using the string name_seed.
It replaces the first '#' character (or append at bottom if absent)
with an unique substring. If name_seed has not a path, the path is set
to TOONZ_TMP. The returned string is owned by function, so must be strsaved.
***********************************************************************/

TNZAPI char *tproc_get_unique_filename(char *name_seed);

#endif
