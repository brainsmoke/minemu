SILENT=@

SHELL=bash
CC=$(SILENT)gcc
AS=$(SILENT)gcc
LINK=$(SILENT)gcc
EMU_LINK=$(SILENT)ld
STRIP=$(SILENT)strip --strip-all
MKDIR=$(SILENT)mkdir
RM=$(SILENT)rm -r

LDFLAGS=
EMU_LDFLAGS=-z noexecstack

#SETTINGS=-g -DEMU_DEBUG
#SETTINGS=-Os -DEMU_DEBUG
SETTINGS=-Os
WARNINGS=-Wno-unused-parameter -Wextra -Wshadow -pedantic -std=gnu99

CFLAGS=-MMD -MF .dep/$@.d $(WARNINGS) $(SETTINGS)

#EMU_EXCLUDE=
EMU_EXCLUDE=src/debug.o

TESTCASES_CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99
EMU_CFLAGS=$(CFLAGS) -Isrc

EMU_TARGETS=minemu
EMU_OBJECTS=$(filter-out $(EMU_EXCLUDE), $(patsubst %.c, %.o, $(wildcard src/*.c)))
EMU_ASM_OBJECTS=$(patsubst %.S, %.o, $(wildcard src/*.S))
EMU_TEST_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/emu/*.c))
EMU_GEN_OBJECTS=$(patsubst %.c, %.o, $(wildcard gen/*.c))

TESTCASES_OBJECTS=$(patsubst %.c, %.o, $(wildcard test/testcases/*.c))
TESTCASES_ASM_OBJECTS=$(patsubst %_asm.S, %.o, $(wildcard test/testcases/*_asm.S))

TEST_OBJECTS_COMMON=\
	test/emu/code_offset.o\
	test/emu/debug.o\
	test/emu/codeexec.o\
	test/emu/memoffsets.o

TEST_TARGETS=$(patsubst %.o, %, $(filter-out $(TEST_OBJECTS_COMMON), \
	$(TESTCASES_OBJECTS) \
	$(TESTCASES_ASM_OBJECTS) \
	$(EMU_TEST_OBJECTS)))

TARGETS=$(TEST_TARGETS) $(EMU_TARGETS)

OBJECTS=\
	$(TESTCASES_OBJECTS) $(TESTCASES_ASM_OBJECTS)\
	$(EMU_OBJECTS) $(EMU_ASM_OBJECTS) $(EMU_TEST_OBJECTS)\
	$(EMU_GEN_OBJECTS)


CLEAN=$(TARGETS) $(OBJECTS) $(EMU_EXCLUDE) gen/gen_mm_ld .dep

.PHONY: depend clean strip

all: depend $(TARGETS)

depend:
	$(MKDIR) -p .dep/test{/testcases,/emu} .dep{/src,/gen/,/test}

clean:
	-$(RM) $(CLEAN)

-include .dep/*/*.d .dep/test/*/*.d

strip: $(TARGETS)
	$(STRIP) $(TARGETS)

# Compiling

$(TESTCASES_OBJECTS): %.o: %.c
	$(CC) $(TESTCASES_CFLAGS) -c -o $@ $<

$(EMU_OBJECTS) $(EMU_TEST_OBJECTS) $(EMU_GEN_OBJECTS): %.o: %.c
	$(CC) -c $(EMU_CFLAGS) -o $@ $<

$(EMU_ASM_OBJECTS): %.o: %.S
	$(AS) -c $(EMU_CFLAGS) -o $@ $<

$(TESTCASES_ASM_OBJECTS): %.o: %_asm.S
	$(AS) -c $(CFLAGS) -o $@ $<

# Linking

src/mm.ld: gen/gen_mm_ld
	gen/gen_mm_ld > src/mm.ld

test/testcases/%: test/testcases/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

test/testcases/intint: test/testcases/intint.o
	$(LINK) -nostdlib -o $@ $^

test/emu/%: test/emu/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

minemu: src/mm.ld src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS)
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS)

test/emu/shellcode: test/emu/shellcode.o test/emu/debug.o test/emu/codeexec.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/offset_mem: test/emu/offset_mem.o test/emu/codeexec.o src/taint_code.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/taint_test: test/emu/taint_test.o test/emu/codeexec.o src/taint_code.o test/emu/debug.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/cmovtest: test/emu/cmovtest.o src/opcodes.o src/syscalls_asm.o src/scratch_asm.o src/jit_code.o src/debug.o src/error.o src/taint_code.o src/sigwrap_asm.o src/hexdump.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/test_jit_fragment: test/emu/test_jit_fragment.o src/jit_fragment.o src/opcodes.o src/syscalls_asm.o src/scratch_asm.o src/jit_code.o src/debug.o src/error.o src/taint_code.o src/sigwrap_asm.o src/hexdump.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/test_jit_lookup: test/emu/test_jit_lookup.o $(filter-out src/minemu.o, src/mm.ld src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T src/minemu.ld test/emu/test_jit_lookup.o $(filter-out src/minemu.o, $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))

test/emu/test_hexdump: test/emu/test_hexdump.o $(filter-out src/minemu.o, src/mm.ld src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T src/minemu.ld test/emu/test_hexdump.o $(filter-out src/minemu.o, $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))
