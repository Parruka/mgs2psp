/* end_pad.ld (gl40 planning, exp40; copied to build_gl40/padld for the gl40 build's pad search): END_PAD bytes of NOBITS after .me_bss, so the image end and the heap block move by END_PAD
   (a model experiment for item 3: the heap block at gl36's offset against the data cache's 8 KB ways). */
SECTIONS { .pad_end (NOLOAD) : { . = . + END_PAD; } } INSERT AFTER .me_bss;
