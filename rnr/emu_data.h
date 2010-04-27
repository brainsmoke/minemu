#ifndef EMU_DATA_H
#define EMU_DATA_H

#include "trace.h"

/* remember data about emulated syscalls across callbacks:
 * - remember the return value that should be returned for emulated calls
 * - remember the last syscall that returned -ERESTART_RESTARTBLOCK so that
 *   we can use it when restart_syscall() is 'called'
 */

typedef struct
{
	long open_args[4];
	long open_flags;
	long retval, call_skipped;
	long restart_syscall;
} emu_data_t;

emu_data_t *emu_data(trace_t *t);
emu_data_t *add_emu_data(trace_t *t);
void del_emu_data(trace_t *t);

#endif /* EMU_DATA_H */
