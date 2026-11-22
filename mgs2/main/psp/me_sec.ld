/* me_sec.ld (task #180 step 4d-3, me_step4_notes.md 4d): the memory the ME writes on 64-byte lines of its own.  Passed to the link with
   -Wl,-T (INSERT: it adds to the default script, it does not replace it).  The ME-safe objects' .data and .bss are renamed .me_data and
   .me_bss after they are compiled (psp-objcopy --rename-section), and psp/xl_psp.c puts the objects of its own the ME writes in
   .bss.me, renamed .me_bss the same way; here both output sections start and end on a 64-byte line, so no line holds a byte the ME
   writes and a byte only the main CPU writes (the ME's data cache and the main CPU's do not see each other's writes: a write-back of a
   shared line from one core would put back the other's old bytes).  The symbols give psp/xl_psp.c and the checker
   (tools/dev/me18/mesec_chk.py) the bounds. */
SECTIONS {
  .me_data : ALIGN(64) { __me_data_start = .; *(.me_data) . = ALIGN(64); __me_data_end = .; }
} INSERT AFTER .data;
SECTIONS {
  .me_bss (NOLOAD) : ALIGN(64) { __me_bss_start = .; *(.me_bss) . = ALIGN(64); __me_bss_end = .; }
} INSERT AFTER .bss;
