
PHDRS
{
  interp PT_INTERP;
}

SECTIONS
{
  .interp : { *(.interp) } :interp
}

