SILENT=@

SHELL=/bin/bash
CC=$(SILENT)gcc
AS=$(SILENT)gcc
LINK=$(SILENT)gcc
EMU_LINK=$(SILENT)ld
STRIP=$(SILENT)strip --strip-all
MKDIR=$(SILENT)mkdir
RM=$(SILENT)rm -r

LDFLAGS=
EMU_LDFLAGS=-z noexecstack #-static

#CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99 -g -DEMU_DEBUG
#CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99 -g
CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99 -Os

TESTCASES_CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99
TRACER_CFLAGS=$(CFLAGS) -Itracer
SYSCALLS_CFLAGS=$(CFLAGS) -Itracer -Isyscalls
RNR_CFLAGS=$(CFLAGS) -Itracer -Isyscalls -Irnr
EMU_CFLAGS=$(CFLAGS) -Iemu

TRACER_TARGETS=record replay
TRACER_OBJECTS=$(patsubst %.c, %.o, $(wildcard tracer/*.c))
TRACER_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/tracer/*.c))

SYSCALLS_OBJECTS=$(patsubst %.c, %.o, $(wildcard syscalls/*.c))
SYSCALLS_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/syscalls/*.c))

RNR_MAINS=$(patsubst %.c, %.o, $(wildcard rnr/*_main.c))
RNR_OBJECTS=$(filter-out $(RNR_MAINS), $(patsubst %.c, %.o, $(wildcard rnr/*.c)))
RNR_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/rnr/*.c))

EMU_TARGETS=temu
EMU_OBJECTS=$(filter-out $(EMU_EXCLUDE), $(patsubst %.c, %.o, $(wildcard emu/*.c)))
EMU_ASM_OBJECTS=$(patsubst %.S, %.o, $(wildcard emu/*.S))
EMU_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/emu/*.c))
EMU_GEN_OBJECTS=$(patsubst %.c, %.o, $(wildcard emu/gen/*.c))

TESTCASES_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/testcases/*.c))
TESTCASES_ASM_OBJECTS=$(patsubst %_asm.S, %.o, $(wildcard test/testcases/*_asm.S))

TEST_OBJECTS_COMMON=\
	test/emu/code_offset.o\
	test/emu/debug.o\
	test/emu/codeexec.o\
	test/emu/memoffsets.o\
	test/syscalls/raise.o

TEST_TARGETS=$(patsubst %.o, %, $(filter-out $(TEST_OBJECTS_COMMON), \
	$(TESTCASES_OBJECTS) \
	$(TESTCASES_ASM_OBJECTS) \
	$(TRACER_TEST_OBJECTS) \
	$(SYSCALLS_TEST_OBJECTS) \
	$(RNR_TEST_OBJECTS) \
	$(EMU_TEST_OBJECTS)))

TARGETS=$(TEST_TARGETS) $(TRACER_TARGETS) $(EMU_TARGETS)

OBJECTS=\
	$(TESTCASES_OBJECTS) $(TESTCASES_ASM_OBJECTS)\
	$(TRACER_OBJECTS) $(TRACER_TEST_OBJECTS)\
	$(SYSCALLS_OBJECTS) $(SYSCALLS_TEST_OBJECTS)\
	$(RNR_MAINS) $(RNR_OBJECTS) $(RNR_TEST_OBJECTS)\
	$(EMU_OBJECTS) $(EMU_ASM_OBJECTS) $(EMU_TEST_OBJECTS)\
	$(EMU_GEN_OBJECTS)


CLEAN=$(TARGETS) $(OBJECTS) .dep

.PHONY: depend clean strip

all: depend $(TARGETS)

depend:
	$(MKDIR) -p .dep{/test,}/{tracer,rnr,syscalls,emu,testcases}/ .dep/emu/gen/

clean:
	-$(RM) $(CLEAN)

-include .dep/*/*.d .dep/test/*/*.d .dep/emu/gen/*.d

strip: $(TARGETS)
	$(STRIP) $(TARGETS)

# Compiling

$(TRACER_OBJECTS) $(TRACER_TEST_OBJECTS): %.o: %.c
	$(CC) $(TRACER_CFLAGS) -c -o $@ $<

$(SYSCALLS_OBJECTS) $(SYSCALLS_TEST_OBJECTS): %.o: %.c
	$(CC) $(SYSCALLS_CFLAGS) -c -o $@ $<

$(RNR_MAINS) $(RNR_OBJECTS) $(RNR_TEST_OBJECTS): %.o: %.c
	$(CC) $(RNR_CFLAGS) -c -o $@ $<

$(TESTCASES_OBJECTS): %.o: %.c
	$(CC) $(TESTCASES_CFLAGS) -c -o $@ $<

$(EMU_OBJECTS) $(EMU_TEST_OBJECTS) $(EMU_GEN_OBJECTS): %.o: %.c
	$(CC) -c $(EMU_CFLAGS) -o $@ $<

$(EMU_ASM_OBJECTS): %.o: %.S
	$(AS) -c $(EMU_CFLAGS) -o $@ $<

$(TESTCASES_ASM_OBJECTS): %.o: %_asm.S
	$(AS) -c $(CFLAGS) -o $@ $<

# Linking

emu/mm.ld: emu/gen/gen_mm_ld
	emu/gen/gen_mm_ld > emu/mm.ld

test/testcases/%: test/testcases/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

test/testcases/intint: test/testcases/intint.o
	$(LINK) -nostdlib -o $@ $^

$(TRACER_TARGETS): %: rnr/%_main.o $(RNR_OBJECTS) $(SYSCALLS_OBJECTS) $(TRACER_OBJECTS)
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

test/emu/cmovtest: test/emu/cmovtest.o emu/opcodes.o emu/syscalls_asm.o emu/scratch_asm.o emu/jit_code.o emu/debug.o emu/error.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/test_jit_fragment: test/emu/test_jit_fragment.o emu/jit_fragment.o emu/opcodes.o emu/syscalls_asm.o emu/scratch_asm.o emu/jit_code.o emu/debug.o emu/error.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

