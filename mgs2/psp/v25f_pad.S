/* v25f (psp/v25f.sh): fills after the new units so that v24's read-only data and data, and the libraries' data, keep their offsets
   modulo 8 KB (the PSP's caches index 8 KB per way); sizes from the first link (PAD_TEXT, PAD_RODATA, PAD_DATA). */
  .section .text.zz_pad, "ax", @progbits
  .space PAD_TEXT
  .section .rodata.zz_pad, "a", @progbits
  .space PAD_RODATA
  .section .data.zz_pad, "aw", @progbits
  .space PAD_DATA
