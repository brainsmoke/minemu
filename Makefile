SILENT=@

SHELL=bash
CC=$(SILENT)gcc
AS=$(SILENT)gcc
OBJCOPY=$(SILENT)objcopy
LINK=$(SILENT)gcc
EMU_LINK=$(SILENT)ld
STRIP=$(SILENT)strip --strip-all
MKDIR=$(SILENT)mkdir
RM=$(SILENT)rm -r

LDFLAGS=-m32
EMU_LDFLAGS=-z noexecstack --warn-common -melf_i386

#SETTINGS=-g 
SETTINGS=-Os -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
WARNINGS=-Wno-unused-parameter -Wextra -Wshadow -pedantic -std=gnu99 -fno-stack-protector -U_FORTIFY_SOURCE

CFLAGS=-MMD -MF .dep/$@.d $(WARNINGS) $(SETTINGS) -m32

#EMU_EXCLUDE=
EMU_EXCLUDE=src/debug.o

TESTCASES_CFLAGS=-MMD -MF .dep/$@.d -Wall -Wshadow -pedantic -std=gnu99 -m32
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

TARGETS=$(EMU_TARGETS) $(TEST_TARGETS)

OBJECTS=\
	$(TESTCASES_OBJECTS) $(TESTCASES_ASM_OBJECTS)\
	$(EMU_OBJECTS) $(EMU_ASM_OBJECTS) $(EMU_TEST_OBJECTS)\
	$(EMU_GEN_OBJECTS)


CLEAN=$(TARGETS) $(OBJECTS) $(EMU_EXCLUDE) src/runtime_asm.o-tmp src/reloc_runtime_asm.o-tmp gen/gen_mm_ld .dep src/asm_consts_gen.h gen/gen_asm_consts_gen_h

.PHONY: test/emu/test_jit_fragment
.PHONY: depend clean strip

all: depend $(TARGETS)

depend:
	$(MKDIR) -p .dep/test{/testcases,/emu} .dep{/src,/gen/,/test}

clean:
	-$(RM) $(CLEAN)

-include .dep/*/*.d .dep/test/*/*.d

$(EMU_OBJECTS) $(EMU_ASM_OBJECTS): src/asm_consts_gen.h
src/runtime_asm.o-tmp src/reloc_runtime_asm.o-tmp: src/asm_consts_gen.h

strip: $(TARGETS)
	$(STRIP) $(TARGETS)

# Compiling

$(TESTCASES_OBJECTS): %.o: %.c
	$(CC) $(TESTCASES_CFLAGS) -c -o $@ $<

$(EMU_OBJECTS) $(EMU_TEST_OBJECTS) $(EMU_GEN_OBJECTS): %.o: %.c
	$(CC) -c $(EMU_CFLAGS) -o $@ $<

$(filter-out src/runtime_asm.o src/reloc_runtime_asm.o, $(EMU_ASM_OBJECTS)): %.o: %.S
	$(AS) -c $(EMU_CFLAGS) -o $@ $<

src/runtime_asm.o-tmp src/reloc_runtime_asm.o-tmp: %.o-tmp: %.S
	$(AS) -c $(EMU_CFLAGS) -o $@ $<

src/runtime_asm.o: %.o: %.o-tmp
	$(OBJCOPY) --redefine-sym offset__jit_eip_HACK=offset__jit_eip $< $@

src/reloc_runtime_asm.o: %.o: %.o-tmp
	$(OBJCOPY) --redefine-sym offset__jit_eip_HACK=offset__jit_fragment_exit_addr \
	           --redefine-sym taint_fault=reloc_taint_fault \
	           --redefine-sym runtime_cache_resolution_start=reloc_runtime_cache_resolution_start \
	           --redefine-sym runtime_cache_resolution_end=reloc_runtime_cache_resolution_end \
	           --redefine-sym runtime_ret=reloc_runtime_ret \
	           --redefine-sym runtime_ijmp=reloc_runtime_ijmp \
	           --redefine-sym cpuid_emu=reloc_cpuid_emu \
	           --redefine-sym jit_return=reloc_jit_return $< $@

$(TESTCASES_ASM_OBJECTS): %.o: %_asm.S
	$(AS) -c $(CFLAGS) -o $@ $<

# Linking

src/mm.ld: gen/gen_mm_ld
	$(SILENT)gen/gen_mm_ld > src/mm.ld

src/syscalls_asm.S: src/asm_consts_gen.h
src/jit_fragment_asm.S: src/asm_consts_gen.h

src/asm_consts_gen.h: gen/gen_asm_consts_gen_h
	$(SILENT)gen/gen_asm_consts_gen_h > $@

test/testcases/killthread: test/testcases/killthread.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lpthread

test/testcases/segvthread: test/testcases/segvthread.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lpthread

test/testcases/tlstest: test/testcases/tlstest.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lpthread

test/testcases/cmp1_10_100_1000: test/testcases/cmp1_10_100_1000.c
	$(LINK) -o $@ $^ $(LDFLAGS) -Os

test/testcases/%: test/testcases/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

test/testcases/intint: test/testcases/intint.o
	$(LINK) -nostdlib -o $@ $^ $(LDFLAGS)

test/emu/%: test/emu/%.o
	$(LINK) -o $@ $^ $(LDFLAGS)

minemu: src/mm.ld src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS)
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS)

test/emu/shellcode: test/emu/shellcode.o test/emu/debug.o test/emu/codeexec.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/offset_mem: test/emu/offset_mem.o test/emu/codeexec.o src/taint_code.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/taint_test: test/emu/taint_test.o test/emu/codeexec.o src/taint_code.o test/emu/debug.o src/segments.o src/threads.o src/syscalls_asm.o src/threads.o src/error.o src/threads_asm.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/cmovtest: test/emu/cmovtest.o src/opcodes.o src/syscalls_asm.o src/threads.o src/jit_code.o src/debug.o src/error.o src/taint_code.o src/sigwrap_asm.o src/hexdump.o src/segments.o src/threads_asm.o
	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

#test/emu/test_jit_fragment: test/emu/test_jit_fragment.o src/jit_fragment.o src/opcodes.o src/syscalls_asm.o src/jit_code.o src/debug.o src/error.o src/taint_code.o src/sigwrap_asm.o src/hexdump.o src/runtime_asm.o src/reloc_runtime_asm.o
#	$(LINK) -o $@ $^ $(LDFLAGS) -lreadline

test/emu/test_jit_lookup: test/emu/test_jit_lookup.o $(filter-out src/minemu.o, src/mm.ld src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T src/minemu.ld test/emu/test_jit_lookup.o $(filter-out src/minemu.o, $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))

test/emu/test_hexdump: test/emu/test_hexdump.o $(filter-out src/minemu.o, src/mm.ld src/minemu.ld $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))
	$(EMU_LINK) $(EMU_LDFLAGS) -o $@ -T src/minemu.ld test/emu/test_hexdump.o $(filter-out src/minemu.o, $(EMU_OBJECTS) $(EMU_ASM_OBJECTS))
