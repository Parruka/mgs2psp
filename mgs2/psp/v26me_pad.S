/* v26me (psp/v26me.sh): fills after the new units so that v26's read-only data and data, and the libraries' data and .bss, keep their
   offsets modulo 8 KB (the PSP's caches index 8 KB per way); sizes found by tools/dev/padfit.py from the link maps.  The read-only fill
   is 16-byte aligned, which makes .rodata 16-byte aligned (v26's starts at a multiple of 16): the new imports grow the NID table before
   it by 4 bytes each after the 16-byte aligned module info, so with .rodata only 8-byte aligned its move could stay 8 bytes off. */
  .section .text.zz_pad, "ax", @progbits
  .space PAD_TEXT
  .section .rodata.zz_pad, "a", @progbits
  .balign 16
  .space PAD_RODATA
  .section .data.zz_pad, "aw", @progbits
  .space PAD_DATA
  .section .bss.zz_pad, "aw", @nobits
  .space PAD_BSS
