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
  /* taint                               */
  /* jit code                            */
  . = minemu_start ;
  minemu_code_start = . ;
  .text : { *(.text) . = ALIGN(0x1000); } :text
  minemu_code_end = . ;
  .scratch_data : { src/thread_ctx*(.bss) src/thread_ctx*(COMMON) . = ALIGN(0x1000); } :scratch
  .runtime_bss : { *(.bss) *(COMMON) . = ALIGN(0x1000); } :bss
  .runtime_data : { *(*) . = ALIGN(0x1000); } :data
  minemu_end = . ;
}

