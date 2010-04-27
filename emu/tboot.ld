ENTRY(start);

INCLUDE "mm.ld";

SECTIONS
{
  . = .user_stack_start;
  .user_stack              : { . = .user_stack_size; }
  .jit_scratch   ALIGN(16) : { . += .jit_scratch_size; }
  .jit_text                : { stub*(.text) }
  .jit_data                : { stub*(.data) }
  .jit_bss                 : { stub*(.bss) }
  jit_code_start ALIGN(16) : { . = .jit_end - ABSOLUTE(.); }
  jit_code_end = ALIGN(16);
  .taint                   : { . = .taint_size; }
  .text                    : { *(.text) }
  .data                    : { *(.data) }
  .bss                     : { *(.bss) }
  .fill                    : { . = .temu_stack_start - ABSOLUTE(.); }
}

