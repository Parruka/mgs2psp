/* me_sec_pad.ld (gl39 experiment, 2026-11-23): psp/me_sec.ld with ME_PAD bytes (a multiple of 64; --defsym) between the hot block
   (.me_bss.h39) and the picture code's tables, so the tables can be put back on the cache sets they had in gl38. */
SECTIONS {
  .me_data : ALIGN(64) { __me_data_start = .; *(.me_data) . = ALIGN(64); __me_data_end = .; }
} INSERT AFTER .data;
SECTIONS {
  .me_bss (NOLOAD) : ALIGN(64) { __me_bss_start = .; *(.me_bss.h39) . = ALIGN(64); __me_h39_end = .; . = . + ME_PAD; *(.me_bss) . = ALIGN(64); __me_bss_end = .; }
} INSERT AFTER .bss;
