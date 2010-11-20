#ifndef TAINT_DUMP_H
#define TAINT_DUMP_H

#include "lib.h"

void hexdump_taint(int fd, const void *data, ssize_t len,
                           const unsigned char *taint, int offset, int ascii,
                           const char *descriptions[]);

void dump_map(int fd, char *addr, unsigned long len);

void do_taint_dump(long *regs);

#endif /* TAINT_DUMP_H */
