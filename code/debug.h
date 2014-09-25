
#ifndef __DEBUG_H_
#define __DEBUG_H_

void dbg_printf(int module, const char *fmt, ...);

#define MODULE_CACHE (1 << 0)
#define MODULE_MAIN  (1 << 1)

void dbg_init(int std, int file, const char * dbg_name);

#endif /* __DEBUG_H_ */
