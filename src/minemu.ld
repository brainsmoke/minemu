ENTRY(start);

/* see mm.h for a description of the memory layout */
INCLUDE "src/mm.ld";

PHDRS
{
  text PT_LOAD FILEHDR PHDRS;
  scratch PT_LOAD FLAGS(6);
  bss PT_LOAD FLAGS(6);
  data PT_LOAD FLAGS(6);
  note PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
  /* user: text,brk,bss,heap,mmaps,stack */

  . = .user_end;
  runtime_code_start = . ;
  .text : { *(.text) . = ALIGN(0x1000); } :text
  runtime_code_size = . - .user_end;
  .scratch_data : { src/exec_ctx_asm*(.bss) . = ALIGN(0x1000); } :scratch
  runtime_data_start = . ;
  .runtime_bss : { *(.bss) . = ALIGN(0x1000); } :bss
  .runtime_data : { *(*) . = ALIGN(0x1000); } :data
  runtime_data_size = . - runtime_data_start;
  jit_code_start = . ;
  . = .jit_end;
  jit_code_size = . - jit_code_start;
  jit_data_size = . - runtime_data_start; /* runtime data and jit code combined */

}

