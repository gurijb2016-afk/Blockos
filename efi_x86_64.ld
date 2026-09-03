
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  . = 0;
  ImageBase = .;

  .hash : { *(.hash) }

  . = ALIGN(4096);
  .text :
  {
    _text = .;
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
    *(.plt)
    . = ALIGN(16);
  }
  _etext = .;
  _text_size = _etext - _text;

  . = ALIGN(4096);
  .init_array :
  {
    __init_array_start = .;
    *(SORT(.init_array.*))
    *(.init_array)
    __init_array_end = .;
  }

  . = ALIGN(4096);
  .ctors :
  {
    __CTOR_LIST__ = .;
    *(SORT(.ctors.*))
    *(.ctors)
    __CTOR_END__ = .;
  }

  . = ALIGN(4096);
  .dtors :
  {
    __DTOR_LIST__ = .;
    *(SORT(.dtors.*))
    *(.dtors)
    __DTOR_END__ = .;
  }

  . = ALIGN(4096);
  .fini_array :
  {
    __fini_array_start = .;
    *(SORT(.fini_array.*))
    *(.fini_array)
    __fini_array_end = .;
  }

  . = ALIGN(4096);
  .rodata :
  {
    *(.rodata*)
  }

  . = ALIGN(4096);
  .data :
  {
    _data = .;
    *(.got.plt)
    *(.got)
    *(.data*)
    *(.sdata)
    *(.sbss)
    *(.scommon)
    *(.bss*)
    *(COMMON)
    *(.rel.local)
  }

  _edata = .;
  _data_size = _edata - _etext;

  .note.gnu.build-id : { *(.note.gnu.build-id) }

  . = ALIGN(4096);
  .reloc :
  {
    KEEP (*(.reloc))
  }

  . = ALIGN(4096);
  _DYNAMIC = .;
  .dynamic : { *(.dynamic) }

  . = ALIGN(4096);
  .rela :
  {
    *(.rela.text*)
    *(.rela.data*)
    *(.rela.got)
    *(.rela.dyn)
    *(.rela.stab)
    *(.rela.init_array*)
    *(.rela.fini_array*)
    *(.rela.ctors*)
    *(.rela.dtors*)
  }

  . = ALIGN(4096);
  .rela.plt : { *(.rela.plt) }

  .ignored.reloc :
  {
    *(.rela.reloc)
    *(.note.GNU-stack)
  }

  .comment 0 : { *(.comment) }
}
