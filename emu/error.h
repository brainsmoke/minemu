#ifndef ERROR_H
#define ERROR_H

void die(char *fmt, ...);
void debug(char *fmt, ...);

#define BREAK {char c[1]; while (sys_read(0, c, 1) == 1 && c[0] != '\n');}

#endif /* ERROR_H */
