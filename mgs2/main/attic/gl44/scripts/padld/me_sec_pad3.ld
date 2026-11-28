/* me_sec_pad3.ld (gl40, 2026-11-23): me_sec_pad2.ld with a third pad, ME_PRE bytes (a multiple of 64; --defsym) in front of the whole
   .me_bss, as a NOBITS section of its own (.me_pre: __me_bss_start stays the bound of .me_bss, as tools/mesec_chk40.py requires): gl40's main .bss is 0x300 bytes longer than gl39's
   (the XP table, XmCopy's new fields, the plan probe), which moved every .me_bss table, the hot block .me_bss.h39 too, by +0x300 against
   the data cache's 8 KB ways; ME_PRE 0x1d00 puts them back. */
SECTIONS {
  .me_data : ALIGN(64) { __me_data_start = .; *(.me_data) . = ALIGN(64); __me_data_end = .; }
} INSERT AFTER .data;
SECTIONS {
  .me_pre (NOLOAD) : { . = . + ME_PRE; }
  .me_bss (NOLOAD) : ALIGN(64) { __me_bss_start = .; *(.me_bss.h39) . = ALIGN(64); __me_h39_end = .; . = . + ME_PAD;
    *xl_psp.o(.me_bss) *ge_xlive.o(.me_bss) . = . + XL_PAD; *(.me_bss) . = ALIGN(64); __me_bss_end = .; }
} INSERT AFTER .bss;
