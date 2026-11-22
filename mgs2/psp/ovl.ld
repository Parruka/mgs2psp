/* A stage overlay (psp/Makefile): linked at the arena's address in MGS2T.elf (OVL_BASE) against its symbols (--just-symbols), then
   written out as a raw image (header, code, data, zeroed bss) for main_psp.c stage_ovl_call to read into the arena. */
OUTPUT_ARCH(mips:allegrex)
ENTRY(ovl_hdr)
SECTIONS
{
  . = OVL_BASE;
  .ovl : {
    KEEP(*(.ovl_hdr))
    *(.text .text.*)
    . = ALIGN(4);
    __ovl_ctor_a = .;               /* the PGO=gen build's counter registration (psp/Makefile); empty otherwise */
    KEEP(*(.ctors .ctors.* .init_array .init_array.*))
    __ovl_ctor_b = .;
    *(.rodata .rodata.*)
    *(.data .data.* .sdata .sdata.*)
    . = ALIGN(16);
    __ovl_bss = .;
    *(.bss .bss.* .sbss .sbss.* COMMON)
    . = ALIGN(16);
    __ovl_end = .;
  }
  ASSERT(__ovl_end - OVL_BASE <= OVL_SIZE, "stage overlay larger than the arena (psp/Makefile OVL_SIZE)")
  /DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) *(.mdebug.*) *(.gcc_compiled_long32) *(.rel.*) *(.dtors .dtors.* .fini_array .fini_array.*) }
}
