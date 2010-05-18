CC=gcc
LINK=gcc
EMU_LINK=ld

LDFLAGS=
EMU_LDFLAGS=-z noexecstack #-static
CFLAGS=-Wall -Wshadow -pedantic -std=gnu99 -g -DHASH_IJMP=1
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
