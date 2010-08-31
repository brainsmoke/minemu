CC=gcc
LINK=gcc
EMU_LINK=ld

LDFLAGS=
EMU_LDFLAGS=-z noexecstack #-static
CFLAGS=-Wall -Wshadow -pedantic -std=gnu99 -g
#CFLAGS=-Wall -Wshadow -pedantic -std=gnu99 -Os
STRIP=strip --strip-all

TEST_TARGETS=\
	test/testcases/getppid\
	test/testcases/hello_world\
	test/testcases/intint\
	test/testcases/pidgallore\
	test/testcases/sigalrm\
	test/testcases/sigpipe\
	test/testcases/sigsegv\
	test/testcases/sigalrm-sysv\
	test/testcases/highfd\
	test/testcases/noaddrrand\
	test/testcases/rdtsc\
	test/testcases/fork\
	test/testcases/exec\
	test/testcases/abort\
	test/testcases/sigalrm-uspace\
	test/testcases/sigalrm-sigsegv\
	test/testcases/nordtsc\
	test/testcases/timestamps\
	test/testcases/sigchld\
	test/testcases/sigprocmask\
	test/testcases/sysall\
	test/testcases/killsegv\
	test/testcases/fstat\
	test/testcases/newns\
	test/testcases/environ\
	test/testcases/newfs\
	test/testcases/rdtrunc
#	test/testcases/raise

TARGETS=$(TEST_TARGETS)\
	test/bdiff\
	test/tracer/writeecho\
	test/tracer/faketsc\
	test/syscalls/nosignals\
	test/syscalls/printregs\
	test/rnr/printtrace\
	test/rnr/openstat\
	test/emu/shellcode\
	test/emu/mprotect\
	test/emu/stack\
	test/emu/compat\
	test/emu/write_vdso\
	test/emu/offset_mem\
	test/emu/taint_test\
	record\
	replay\
	temu\
#	test/emu/memoffsets\
	#rnr

TRACER_OBJECTS=\
	tracer/dataset.o\
	tracer/debug.o\
	tracer/errors.o\
	tracer/signal_info.o\
	tracer/signal_queue.o\
	tracer/trace.o\
	tracer/trace_map.o\
	tracer/util.o\
	tracer/process.o

SYSCALLS_OBJECTS=\
	syscalls/syscall_info.o\
	syscalls/debug_syscalls.o\
	syscalls/debug_wrap.o

RNR_OBJECTS=$(TRACER_OBJECTS) $(SYSCALLS_OBJECTS)\
	rnr/call_data.o\
	rnr/emu_data.o\
	rnr/fd_data.o\
	rnr/record.o\
	rnr/replay.o\
	rnr/serialize.o\
	rnr/syscall_copy.o\
	rnr/syscall_validate.o\

EMU_OBJECTS=\
	emu/runtime_asm.o\
	emu/runtime.o\
	emu/temu.o\
	emu/error.o\
	emu/load_elf.o\
	emu/lib.o\
	emu/debug_asm.o\
	emu/mm.o\
	emu/init_asm.o\
	emu/scratch_asm.o\
	emu/syscalls_asm.o\
	emu/opcodes.o\
	emu/jit.o\
	emu/codemap.o\
	emu/jmpcache.o\
	emu/jitmm.o\
	emu/call_emul.o

RECORD_MAIN=rnr/record_main.o
REPLAY_MAIN=rnr/replay_main.o
RNR_MAIN=rnr/rnr.o

PROGS=$(RECORD_MAIN) $(REPLAY_MAIN) $(RNR_MAIN)

OBJECTS=$(RNR_OBJECTS) $(EMU_OBJECTS) $(PROGS)

CLEAN=$(TARGETS) $(OBJECTS)

.PHONY: depend clean strip

all: $(TARGETS)

#strip: $(TARGETS)
#	$(STRIP) --strip-all $^

$(OBJECTS): depend

tracer/%.o: tracer/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

rnr/%.o: rnr/%.c
	$(CC) $(CFLAGS) -Itracer -Isyscalls -c -o $@ $<

syscalls/%.o: syscalls/%.c
	$(CC) $(CFLAGS) -Itracer -c -o $@ $<

test/rnr/%.o: test/rnr/%.c
	$(CC) $(CFLAGS) -Isyscalls -Itracer -Irnr -c -o $@ $<

test/syscalls/%.o: test/syscalls/%.c
	$(CC) $(CFLAGS) -Isyscalls -Itracer -c -o $@ $<

test/%.o: test/%.c
	$(CC) $(CFLAGS) -Itracer -c -o $@ $<

test/testcases/%: test/testcases/%.c
	$(CC) $(CFLAGS) -o $@ $<

test/testcases/intint: test/testcases/intint.S
	$(CC) -nostdlib -o $@ $<

rnr: rnr.o $(RNR_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

record: $(RECORD_MAIN) $(RNR_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

replay: $(REPLAY_MAIN) $(RNR_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/rnr/%: test/rnr/%.o $(RNR_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/syscalls/%: test/syscalls/%.o $(TRACER_OBJECTS) $(SYSCALLS_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/emu/%: test/emu/%.c
	$(CC) $(CFLAGS) -Iemu -o $@ $<

test/%: test/%.o $(TRACER_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

emu/%.o: emu/%.S
	$(CC) -c $(CFLAGS) -o $@ $<

emu/%.o: emu/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

emu/mm.ld: emu/gen/gen_mm_ld
	emu/gen/gen_mm_ld > emu/mm.ld

temu: emu/mm.ld emu/temu.ld $(EMU_OBJECTS)
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T emu/temu.ld $(EMU_OBJECTS)

test/emu/shellcode: test/emu/shellcode.c test/emu/debug.c test/emu/codeexec.c
	$(CC) $(CFLAGS) -o $@ $^ -lreadline

test/emu/offset_mem: test/emu/offset_mem.c test/emu/codeexec.c emu/taint.c
	$(CC) $(CFLAGS) -Iemu -o $@ $^ -lreadline

test/emu/taint_test: test/emu/taint_test.c test/emu/codeexec.c emu/taint.c test/emu/debug.c
	$(CC) $(CFLAGS) -Iemu -o $@ $^ -lreadline
 

depend:
	makedepend -- -Y -I{tracer,rnr,syscalls} -- {test/,}{tracer/,rnr/,syscalls/,}*.c 2>/dev/null
	makedepend -a -- -Y -I{tracer,rnr,syscalls} -- test/testcases/*.c 2>/dev/null
	makedepend -a -- -Y -Iemu -- {test/,}emu/*.c 2>/dev/null

clean:
	-rm $(CLEAN)
	makedepend -- -Y -- 2>/dev/null
	-rm Makefile.bak

# DO NOT DELETE

test/tracer/faketsc.o: tracer/trace.h tracer/arch.h tracer/util.h
test/tracer/faketsc.o: tracer/signal_queue.h tracer/process.h
test/tracer/writeecho.o: tracer/debug.h tracer/trace.h tracer/arch.h
test/tracer/writeecho.o: tracer/util.h tracer/signal_queue.h tracer/process.h
test/rnr/openstat.o: tracer/debug.h tracer/trace.h tracer/arch.h
test/rnr/openstat.o: tracer/util.h tracer/signal_queue.h tracer/process.h
test/rnr/printtrace.o: tracer/debug.h tracer/trace.h tracer/arch.h
test/rnr/printtrace.o: tracer/util.h tracer/signal_queue.h
test/rnr/printtrace.o: syscalls/debug_syscalls.h rnr/serialize.h
test/syscalls/nosignals.o: tracer/trace.h tracer/arch.h syscalls/debug_wrap.h
test/syscalls/nosignals.o: tracer/util.h tracer/signal_queue.h
test/syscalls/nosignals.o: tracer/process.h
test/syscalls/printregs.o: tracer/errors.h tracer/debug.h tracer/trace.h
test/syscalls/printregs.o: tracer/arch.h syscalls/debug_syscalls.h
test/syscalls/printregs.o: tracer/util.h tracer/signal_queue.h
test/syscalls/printregs.o: tracer/process.h
test/bdiff.o: tracer/errors.h tracer/debug.h tracer/trace.h tracer/arch.h
tracer/dataset.o: tracer/dataset.h tracer/errors.h
tracer/debug.o: tracer/debug.h tracer/trace.h tracer/arch.h tracer/util.h
tracer/debug.o: tracer/signal_queue.h
tracer/errors.o: tracer/errors.h
tracer/process.o: tracer/process.h tracer/errors.h
tracer/signal_info.o: tracer/signal_info.h
tracer/signal_queue.o: tracer/trace.h tracer/arch.h tracer/util.h
tracer/signal_queue.o: tracer/signal_queue.h tracer/dataset.h tracer/errors.h
tracer/trace.o: tracer/trace.h tracer/arch.h tracer/errors.h
tracer/trace.o: tracer/trace_map.h tracer/util.h tracer/signal_queue.h
tracer/trace_map.o: tracer/trace_map.h tracer/trace.h tracer/arch.h
tracer/trace_map.o: tracer/errors.h
tracer/util.o: tracer/trace.h tracer/arch.h tracer/util.h
tracer/util.o: tracer/signal_queue.h tracer/errors.h
rnr/call_data.o: tracer/trace.h tracer/arch.h tracer/util.h
rnr/call_data.o: tracer/signal_queue.h tracer/dataset.h tracer/errors.h
rnr/call_data.o: syscalls/syscall_info.h rnr/call_data.h
rnr/emu_data.o: tracer/trace.h tracer/arch.h tracer/dataset.h rnr/emu_data.h
rnr/fd_data.o: tracer/trace.h tracer/arch.h tracer/dataset.h tracer/errors.h
rnr/fd_data.o: rnr/fd_data.h tracer/util.h tracer/signal_queue.h
rnr/record.o: rnr/record.h rnr/serialize.h tracer/trace.h tracer/arch.h
rnr/record.o: tracer/util.h tracer/signal_queue.h tracer/process.h
rnr/record.o: syscalls/syscall_info.h rnr/syscall_copy.h
rnr/record.o: rnr/syscall_validate.h tracer/signal_info.h rnr/emu_data.h
rnr/record.o: rnr/call_data.h rnr/fd_data.h syscalls/debug_wrap.h
rnr/record.o: tracer/debug.h
rnr/record_main.o: rnr/record.h rnr/serialize.h tracer/trace.h tracer/arch.h
rnr/record_main.o: tracer/errors.h
rnr/replay.o: tracer/errors.h rnr/replay.h tracer/debug.h tracer/trace.h
rnr/replay.o: tracer/arch.h syscalls/debug_syscalls.h syscalls/debug_wrap.h
rnr/replay.o: rnr/serialize.h tracer/util.h tracer/signal_queue.h
rnr/replay.o: tracer/process.h syscalls/syscall_info.h rnr/syscall_copy.h
rnr/replay.o: tracer/signal_info.h rnr/call_data.h rnr/emu_data.h
rnr/replay_main.o: rnr/replay.h tracer/errors.h
rnr/rnr.o: rnr/record.h rnr/serialize.h tracer/trace.h tracer/arch.h
rnr/rnr.o: rnr/replay.h tracer/errors.h
rnr/serialize.o: tracer/errors.h rnr/serialize.h tracer/trace.h tracer/arch.h
rnr/serialize.o: tracer/debug.h
rnr/syscall_copy.o: tracer/errors.h tracer/trace.h tracer/arch.h
rnr/syscall_copy.o: tracer/util.h tracer/signal_queue.h rnr/serialize.h
rnr/syscall_copy.o: syscalls/syscall_info.h rnr/syscall_copy.h
rnr/syscall_copy.o: rnr/call_data.h tracer/debug.h
rnr/syscall_validate.o: tracer/util.h tracer/trace.h tracer/arch.h
rnr/syscall_validate.o: tracer/signal_queue.h rnr/syscall_validate.h
rnr/syscall_validate.o: syscalls/syscall_info.h rnr/call_data.h
syscalls/debug_syscalls.o: tracer/util.h tracer/trace.h tracer/arch.h
syscalls/debug_syscalls.o: tracer/signal_queue.h tracer/debug.h
syscalls/debug_syscalls.o: syscalls/debug_syscalls.h syscalls/syscall_info.h
syscalls/debug_wrap.o: tracer/errors.h tracer/debug.h tracer/trace.h
syscalls/debug_wrap.o: tracer/arch.h syscalls/debug_syscalls.h
syscalls/debug_wrap.o: syscalls/debug_wrap.h tracer/util.h
syscalls/debug_wrap.o: tracer/signal_queue.h tracer/signal_info.h
syscalls/syscall_info.o: syscalls/syscall_info.h tracer/trace.h tracer/arch.h
syscalls/syscall_info.o: tracer/util.h tracer/signal_queue.h

test/testcases/raise.o: syscalls/debug_syscalls.c tracer/util.h
test/testcases/raise.o: tracer/trace.h tracer/arch.h tracer/signal_queue.h
test/testcases/raise.o: tracer/debug.h syscalls/debug_syscalls.h
test/testcases/raise.o: syscalls/syscall_info.h

test/emu/debug.o: test/emu/debug.h
test/emu/memoffsets.o: emu/mm.h
test/emu/offset_mem.o: test/emu/debug.h test/emu/codeexec.h emu/opcodes.h
test/emu/offset_mem.o: emu/lib.h emu/taint.h
test/emu/shellcode.o: test/emu/debug.h test/emu/codeexec.h
test/emu/taint_test.o: test/emu/debug.h test/emu/codeexec.h emu/taint.h
test/emu/taint_test.o: emu/lib.h emu/opcodes.h
emu/call_emul.o: emu/scratch.h emu/runtime.h emu/codemap.h emu/jitmm.h
emu/call_emul.o: emu/jit.h emu/opcodes.h emu/lib.h emu/mm.h emu/error.h
emu/call_emul.o: emu/syscalls.h
emu/codemap.o: emu/lib.h emu/mm.h emu/jit.h emu/opcodes.h emu/error.h
emu/codemap.o: emu/jmpcache.h emu/codemap.h emu/jitmm.h emu/runtime.h
emu/codemap.o: emu/scratch.h
emu/error.o: emu/lib.h emu/error.h emu/syscalls.h
emu/jit.o: emu/lib.h emu/mm.h emu/jit.h emu/opcodes.h emu/jitmm.h
emu/jit.o: emu/codemap.h emu/jmpcache.h emu/error.h emu/runtime.h
emu/jit.o: emu/scratch.h
emu/jitmm.o: emu/lib.h emu/mm.h emu/jit.h emu/opcodes.h emu/codemap.h
emu/jitmm.o: emu/jitmm.h emu/jmpcache.h emu/error.h emu/runtime.h
emu/jitmm.o: emu/scratch.h
emu/jmpcache.o: emu/lib.h emu/error.h emu/jmpcache.h emu/scratch.h
emu/lib.o: emu/syscalls.h emu/error.h
emu/load_elf.o: emu/error.h emu/syscalls.h emu/load_elf.h emu/lib.h emu/mm.h
emu/mm.o: emu/mm.h emu/error.h emu/lib.h emu/syscalls.h emu/runtime.h
emu/mm.o: emu/scratch.h emu/codemap.h emu/jitmm.h emu/jit.h emu/opcodes.h
emu/opcodes.o: emu/opcodes.h emu/lib.h emu/jmpcache.h emu/scratch.h
emu/opcodes.o: emu/runtime.h emu/codemap.h emu/jitmm.h emu/error.h
emu/opcodes.o: emu/taint.h
emu/runtime.o: emu/syscalls.h emu/runtime.h emu/scratch.h emu/codemap.h
emu/runtime.o: emu/jitmm.h emu/mm.h emu/jmpcache.h
emu/taint.o: emu/taint.h emu/lib.h emu/opcodes.h emu/error.h
emu/temu.o: emu/syscalls.h emu/error.h emu/load_elf.h emu/lib.h emu/mm.h
emu/temu.o: emu/runtime.h emu/scratch.h emu/codemap.h emu/jitmm.h emu/jit.h
emu/temu.o: emu/opcodes.h
