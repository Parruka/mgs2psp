/* me_sec_pad3.ld (gl41s layout check, 2026-11-23): build_gl39/padld/me_sec_pad2.ld with .me_data and .me_bss started at gl39's offsets
   against the 8 KB cache ways (gl39: .me_data 08c46240, .me_bss 0b1b8a40), whatever the sections before them grew by: ME_DPAD (>= 1) and ME_BPAD
   bytes (--defsym, read off a first link).  The pads are
   sections of their own in front (mesec_chk39 wants __me_*_start / _end to be the sections' bounds), the data one with contents so the
   RW segment is not split. */
SECTIONS {
  .me_data_pad : { BYTE(0) . = . + ME_DPAD - 1; }
} INSERT AFTER .data;
SECTIONS {
  .me_data : ALIGN(64) { __me_data_start = .; *(.me_data) . = ALIGN(64); __me_data_end = .; }
} INSERT AFTER .me_data_pad;
SECTIONS {
  .me_bss_pad (NOLOAD) : { . = . + ME_BPAD; }
} INSERT AFTER .bss;
SECTIONS {
  .me_bss (NOLOAD) : ALIGN(64) { __me_bss_start = .; *(.me_bss.h39) . = ALIGN(64); __me_h39_end = .; . = . + ME_PAD;
    *xl_psp.o(.me_bss) *ge_xlive.o(.me_bss) . = . + XL_PAD; *(.me_bss) . = ALIGN(64); __me_bss_end = .; }
} INSERT AFTER .me_bss_pad;
