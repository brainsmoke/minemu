#ifndef DEBUG_H
#define DEBUG_H

#include <unistd.h>
#include <stdio.h>

/* Various debugging functions with neat color-coding */

/* must be set first */
void debug_init(FILE *debug_out);

void printhex(const void *data, int len);
void printhex_diff(const void *data1, ssize_t len1,
                   const void *data2, ssize_t len2, int grane);

void print_fxsave(char *fxsave1, char *fxsave2);
void print_pushadfd(long *pushadfd1, long *pushadfd2);

void printhex_taint(const void *data, ssize_t len, const void *taint);
void printregs_taint(const void *regs, const void *taint);

#endif /* DEBUG_H */
