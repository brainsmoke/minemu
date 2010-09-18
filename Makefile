SILENT=@

CC=$(SILENT)gcc
AS=$(SILENT)gcc
LINK=$(SILENT)gcc
EMU_LINK=$(SILENT)ld
STRIP=$(SILENT)strip --strip-all
MKDIR=$(SILENT)mkdir
RM=$(SILENT)rm -r

LDFLAGS=
EMU_LDFLAGS=-z noexecstack #-static
CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99 -g
#CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99 -Os
TESTCASES_CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99
TRACER_CFLAGS=$(CFLAGS) -Itracer
SYSCALLS_CFLAGS=$(CFLAGS) -Itracer -Isyscalls
RNR_CFLAGS=$(CFLAGS) -Itracer -Isyscalls -Irnr
EMU_CFLAGS=$(CFLAGS) -Iemu

TEST_TARGETS=$(patsubst %.c, %, $(wildcard test/testcases/*.c))\
	test/tracer/bdiff\
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
	test/emu/taint_test

TARGETS=$(TEST_TARGETS)\
	record\
	replay\
	temu

TRACER_OBJECTS=$(patsubst %.c, %.o, $(wildcard tracer/*.c))
SYSCALLS_OBJECTS=$(patsubst %.c, %.o, $(wildcard syscalls/*.c))
RNR_OBJECTS=\
	rnr/call_data.o\
	rnr/emu_data.o\
	rnr/fd_data.o\
	rnr/record.o\
	rnr/replay.o\
	rnr/serialize.o\
	rnr/syscall_copy.o\
	rnr/syscall_validate.o\

EMU_OBJECTS=$(patsubst %.c, %.o, $(wildcard emu/*.c))
EMU_ASM_OBJECTS=$(patsubst %.S, %.o, $(wildcard emu/*.S))

TESTCASES_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/testcases/*.c))
TRACER_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/tracer/*.c))
SYSCALLS_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/syscalls/*.c))
RNR_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/rnr/*.c))
EMU_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/emu/*.c))

RECORD_MAIN=rnr/record_main.o
REPLAY_MAIN=rnr/replay_main.o
RNR_MAIN=rnr/rnr.o

PROGS=$(RECORD_MAIN) $(REPLAY_MAIN) $(RNR_MAIN)

OBJECTS=\
	$(PROGS) $(TESTCASES_OBJECTS)\
	$(TRACER_OBJECTS) $(TRACER_TEST_OBJECTS)\
	$(SYSCALLS_OBJECTS) $(SYSCALLS_TEST_OBJECTS)\
	$(RNR_OBJECTS) $(RNR_TEST_OBJECTS)\
	$(EMU_OBJECTS) $(EMU_ASM_OBJECTS) $(EMU_TEST_OBJECTS)

CLEAN=$(TARGETS) $(OBJECTS) .dep

.PHONY: depend clean strip

all: depend $(TARGETS)

depend:
	$(MKDIR) -p .dep{/test,}/{tracer,rnr,syscalls,emu,testcases}/

clean:
	-$(RM) $(CLEAN)

-include .dep/*/*.d .dep/test/*/*.d

strip: $(TARGETS)
	$(STRIP) --strip-all $(TARGETS)

# Compiling

$(TRACER_OBJECTS) $(TRACER_TEST_OBJECTS): %.o: %.c
	$(CC) $(TRACER_CFLAGS) -c -o $@ $<

$(SYSCALLS_OBJECTS) $(SYSCALLS_TEST_OBJECTS): %.o: %.c
	$(CC) $(SYSCALLS_CFLAGS) -c -o $@ $<

$(RECORD_MAIN) $(REPLAY_MAIN) $(RNR_OBJECTS) $(RNR_TEST_OBJECTS): %.o: %.c
	$(CC) $(RNR_CFLAGS) -c -o $@ $<

$(TESTCASES_OBJECTS): %.o: %.c
	$(CC) $(TESTCASES_CFLAGS) -c -o $@ $<

$(EMU_OBJECTS) $(EMU_TEST_OBJECTS): %.o: %.c
	$(CC) -c $(EMU_CFLAGS) -o $@ $<

$(EMU_ASM_OBJECTS): %.o: %.S
	$(AS) -c $(EMU_CFLAGS) -o $@ $<

test/testcases/intint: test/testcases/intint.S
	$(AS) -nostdlib -o $@ $<

# Linking

emu/mm.ld: emu/gen/gen_mm_ld
	emu/gen/gen_mm_ld > emu/mm.ld

test/testcases/%: test/testcases/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

rnr: rnr.o $(RNR_OBJECTS) $(SYSCALLS_OBJECTS) $(TRACER_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

record replay: %: rnr/%_main.o $(RNR_OBJECTS) $(SYSCALLS_OBJECTS) $(TRACER_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/rnr/%: test/rnr/%.o $(RNR_OBJECTS) $(SYSCALLS_OBJECTS) $(TRACER_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/syscalls/%: test/syscalls/%.o $(TRACER_OBJECTS) $(SYSCALLS_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/tracer/%: test/tracer/%.o $(TRACER_OBJECTS)
	$(LINK) -o $@ $^ $(LDFLAGS)

test/emu/%: test/emu/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

temu: emu/mm.ld emu/temu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS)
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T emu/temu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS)

test/emu/shellcode: test/emu/shellcode.o test/emu/debug.o test/emu/codeexec.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/offset_mem: test/emu/offset_mem.o test/emu/codeexec.o emu/taint.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/taint_test: test/emu/taint_test.o test/emu/codeexec.o emu/taint.o test/emu/debug.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

