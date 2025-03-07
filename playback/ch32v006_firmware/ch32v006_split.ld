ENTRY( InterruptVector )
MEMORY
{
	FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 62K

	/* Mapped from 0x1fff0000. Codgen uses auipc, and it's
	   "running from" 0x0000000 so we have to go with it. */
	BOOTLOADER (rx) : ORIGIN = 0x00000000, LENGTH = 3328

	RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 8K
}
SECTIONS
{
    .init :
    {
      _sinit = .;
      . = ALIGN(4);
      KEEP(*(SORT_NONE(.init)))
      . = ALIGN(4);
      _einit = .;
    } >BOOTLOADER AT>BOOTLOADER

    .text :
    {
      . = ALIGN(4);
      *(.text)
      *(.text.*)
      *(.rodata)
      *(.rodata*)
      *(.gnu.linkonce.t.*)
      . = ALIGN(256);
    } >BOOTLOADER AT>BOOTLOADER

	.fixedflash :
	{
		*(.fixedflash*)
	} >FLASH AT>FLASH

    .fini :
    {
      KEEP(*(SORT_NONE(.fini)))
      . = ALIGN(4);
    } >FLASH AT>FLASH
    PROVIDE( _etext = . );
    PROVIDE( _eitcm = . );
    .preinit_array :
    {
      PROVIDE_HIDDEN (__preinit_array_start = .);
      KEEP (*(.preinit_array))
      PROVIDE_HIDDEN (__preinit_array_end = .);
    } >FLASH AT>FLASH
    .init_array :
    {
      PROVIDE_HIDDEN (__init_array_start = .);
      KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
      KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
      PROVIDE_HIDDEN (__init_array_end = .);
    } >FLASH AT>FLASH
    .fini_array :
    {
      PROVIDE_HIDDEN (__fini_array_start = .);
      KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
      KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
      PROVIDE_HIDDEN (__fini_array_end = .);
    } >FLASH AT>FLASH
    .ctors :
    {
      KEEP (*crtbegin.o(.ctors))
      KEEP (*crtbegin?.o(.ctors))
      KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
      KEEP (*(SORT(.ctors.*)))
      KEEP (*(.ctors))
    } >FLASH AT>FLASH
    .dtors :
    {
      KEEP (*crtbegin.o(.dtors))
      KEEP (*crtbegin?.o(.dtors))
      KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
      KEEP (*(SORT(.dtors.*)))
      KEEP (*(.dtors))
    } >FLASH AT>FLASH
    .dalign :
    {
      . = ALIGN(4);
      PROVIDE(_data_vma = .);
    } >RAM AT>FLASH
    .dlalign :
    {
      . = ALIGN(4);
      PROVIDE(_data_lma = .);
    } >FLASH AT>FLASH
    .data :
    {
      . = ALIGN(4);
      *(.gnu.linkonce.r.*)
      *(.data .data.*)
      *(.gnu.linkonce.d.*)
      . = ALIGN(8);
      PROVIDE( __global_pointer$ = . + 0x3fc );
      *(.sdata .sdata.*)
      *(.sdata2*)
      *(.gnu.linkonce.s.*)
      . = ALIGN(8);
      *(.srodata.cst16)
      *(.srodata.cst8)
      *(.srodata.cst4)
      *(.srodata.cst2)
      *(.srodata .srodata.*)
      . = ALIGN(4);
      PROVIDE( _edata = .);
    } >RAM AT>FLASH
    .bss :
    {
      . = ALIGN(4);
      PROVIDE( _sbss = .);
      *(.sbss*)
      *(.gnu.linkonce.sb.*)
      *(.bss*)
      *(.gnu.linkonce.b.*)
      *(COMMON*)
      . = ALIGN(4);
      PROVIDE( _ebss = .);
    } >RAM AT>FLASH
    PROVIDE( _end = _ebss);
 PROVIDE( end = . );
 PROVIDE( _eusrstack = ORIGIN(RAM) + LENGTH(RAM));
    /DISCARD/ : {
      *(.note .note.*)
      *(.eh_frame .eh_frame.*)
      *(.comment .comment.*)
      *(.ARM.extab* .gnu.linkonce.armextab.*)
      *(.ARM.exidx*)
    }
}
