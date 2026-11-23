/* me_sec_pad2.ld (gl39 experiment, 2026-11-23): me_sec_pad.ld with a second pad, XL_PAD bytes (--defsym) in front of ge_xlate.o's
   tables: the hot block took 0x210 bytes out of that object's .me_bss, which moved its later tables (own, VM, gr ...) and every
   table behind it off the cache sets they had in gl38. */
SECTIONS {
  .me_data : ALIGN(64) { __me_data_start = .; *(.me_data) . = ALIGN(64); __me_data_end = .; }
} INSERT AFTER .data;
SECTIONS {
  .me_bss (NOLOAD) : ALIGN(64) { __me_bss_start = .; *(.me_bss.h39) . = ALIGN(64); __me_h39_end = .; . = . + ME_PAD;
    *xl_psp.o(.me_bss) *ge_xlive.o(.me_bss) . = . + XL_PAD; *(.me_bss) . = ALIGN(64); __me_bss_end = .; }
} INSERT AFTER .bss;
