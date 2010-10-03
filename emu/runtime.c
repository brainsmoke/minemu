#include <sys/mman.h>

#include "syscalls.h"
#include "runtime.h"
#include "mm.h"
#include "jmpcache.h"

long (*runtime_ijmp_addr)(void) = runtime_ijmp;
long (*runtime_ret_addr)(void) = runtime_ret;
long (*jit_block_exit_addr)(void) = jit_block_exit;
long (*int80_emu_addr)(void) = int80_emu;
long (*linux_sysenter_emu_addr)(void) = linux_sysenter_emu;
