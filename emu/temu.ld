ENTRY(start);

/* see mm.h for a description of the memory layout */
INCLUDE "emu/mm.ld";

PHDRS
{
  text PT_LOAD FILEHDR PHDRS;
  scratch PT_LOAD;
  bss PT_LOAD;
  data PT_LOAD;
  data PT_GNU_STACK;
}

SECTIONS
{
  /* user: text,brk,bss,heap,mmaps,stack */

  . = .user_end;
  runtime_code_start = . ;
  .text : { *(.text) . = ALIGN(0x1000); } :text
  runtime_code_size = . - .user_end;
  .scratch_data : { emu/scratch_asm*(.bss) emu/scratch*(.bss) . = ALIGN(0x1000); } :scratch
  runtime_data_start = . ;
  .runtime_bss : { *(.bss) . = ALIGN(0x1000); } :bss
  .runtime_data : { *(*) . = ALIGN(0x1000); } :data
  runtime_data_size = . - runtime_data_start;
  jit_code_start = . ;
  . = .jit_end;
  jit_code_size = . - jit_code_start;
  jit_data_size = . - runtime_data_start; /* runtime data and jit code combined */
}

