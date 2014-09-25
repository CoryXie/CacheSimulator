#include <stdarg.h>
#include <stdio.h>
#include "debug.h"

int dbg_mod_stderr = 0;
int dbg_mod_file = 0;
FILE *dbg_file = 0;

void dbg_printf(int module , const char *fmt, ...)
    {
    if (dbg_mod_stderr & module)
        {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        }

    if (dbg_file != NULL && (dbg_mod_file & module))
        {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(dbg_file, fmt, ap);
        va_end(ap);
        }
    }

void dbg_init(int std, int file, const char * dbg_name)
    {
    dbg_mod_stderr = std;
    dbg_mod_file = file;

    if (file != 0)
        dbg_file = fopen(dbg_name, "rw+");
    }
