ENTRY(start);

/* see mm.h for a description of the memory layout */
INCLUDE "src/mm.ld";

PHDRS
{
  text PT_LOAD FILEHDR PHDRS;
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
  . = SIZEOF_HEADERS + .;
  .text : { *(.text) . = ALIGN(0x1000); } :text
  minemu_code_end = . ;
  .bss : { *(.bss) *(COMMON) . = ALIGN(0x1000); } :bss
  .data : { *(*) . = ALIGN(0x1000); } :data
  minemu_end = . ;
}

