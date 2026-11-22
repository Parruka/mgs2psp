/* A stage overlay with free play's extra functions (v25f, tools/xunit.py; psp/v25f.sh): psp/ovl.ld's overlay, unchanged, then from
   its own end (__ovl_end) the extra part of the units named *_x.o: the OvlX header (magic, count, table) that psp/xhook.c x_call reads
   there, their code, read-only data and data.  So the reduced overlay keeps its bytes and its offsets in the arena (cache sets); the
   loader (main_psp.c stage_ovl_call) reads the whole file and zeroes only the reduced part's bss, so the extra part may have none. */
OUTPUT_ARCH(mips:allegrex)
ENTRY(ovl_hdr)
SECTIONS
{
  . = OVL_BASE;
  .ovl : {
    KEEP(*(.ovl_hdr))
    EXCLUDE_FILE(*_x.o) *(.text .text.*)
    . = ALIGN(4);
    __ovl_ctor_a = .;               /* the PGO=gen build's counter registration (psp/Makefile); empty otherwise */
    KEEP(EXCLUDE_FILE(*_x.o) *(.ctors .ctors.* .init_array .init_array.*))
    __ovl_ctor_b = .;
    EXCLUDE_FILE(*_x.o) *(.rodata .rodata.*)
    EXCLUDE_FILE(*_x.o) *(.data .data.* .sdata .sdata.*)
    . = ALIGN(16);
    __ovl_bss = .;
    EXCLUDE_FILE(*_x.o) *(.bss .bss.* .sbss .sbss.* COMMON)
    . = ALIGN(16);
    __ovl_end = .;
    KEEP(*_x.o(.ovlx_hdr))
    *_x.o(.text .text.*)
    *_x.o(.rodata .rodata.*)
    *_x.o(.data .data.* .sdata .sdata.*)
    __ovlx_bss = .;
    *_x.o(.bss .bss.* .sbss .sbss.* COMMON)
    __ovlx_end = .;
  }
  ASSERT(__ovl_end - OVL_BASE <= OVL_SIZE, "stage overlay larger than the arena (psp/Makefile OVL_SIZE)")
  ASSERT(__ovlx_end - OVL_BASE <= OVL_SIZE, "stage overlay with its extra part larger than the arena (psp/Makefile OVL_SIZE)")
  ASSERT(__ovlx_end == __ovlx_bss, "extra part with zeroed data: the loader zeroes only the reduced part's (main_psp.c stage_ovl_call)")
  /DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) *(.mdebug.*) *(.gcc_compiled_long32) *(.rel.*) *(.dtors .dtors.* .fini_array .fini_array.*) }
}
