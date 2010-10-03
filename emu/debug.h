#ifndef DEBUG_H
#define DEBUG_H

void printhex(const void *data, int len);
void printhex_diff(const void *data1, ssize_t len1,
                   const void *data2, ssize_t len2, int grane);

#ifdef EMU_DEBUG
void print_debug_data(void);
#endif

#endif /* DEBUG_H */
