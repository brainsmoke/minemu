#ifndef RECORD_H
#define RECORD_H

#include "serialize.h"

/* runs a program and writes the neccesary information to replay
 * its execution to a filedescriptor
 */
void record(int write_fd, char **args, int debug_lvl, int verbosity);

#endif /* RECORD_H */
