/* ro_pad2.ld (gl39, 2026-11-23): RO_PAD bytes (--defsym) of read-only zero bytes before .rodata, so .rodata lies at the offset it had in
   gl38 against the data cache's 8 KB ways.  The section has contents and is read-only: it stays in the first (R E) load segment with
   .text and .rodata, FileSiz = MemSiz, and the program headers are those of every build before (a NOBITS pad made ld split the
   segment in two and mark the first one writable: ro_pad.ld, not used for a build that leaves the container). */
SECTIONS { .pad_ro (READONLY) : { BYTE(0) . = . + RO_PAD - 1; } } INSERT BEFORE .rodata;
