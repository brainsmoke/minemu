#ifndef TAINT_DUMP_H
#define TAINT_DUMP_H

#include "lib.h"

extern int dump_on_exit;
extern int dump_all;

void set_taint_dump_dir(const char *dir);
char *get_taint_dump_dir(void);

void hexdump_taint(int fd, const void *data, ssize_t len,
                           const unsigned char *taint, int offset, int ascii,
                           const char *descriptions[]);

void dump_map(int fd, char *addr, unsigned long len);

void do_taint_dump(long *regs);

#endif /* TAINT_DUMP_H */
