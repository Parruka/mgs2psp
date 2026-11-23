/* ro_pad.ld (gl39 experiment): RO_PAD bytes before .rodata and DA_PAD bytes before .data (--defsym). */
SECTIONS { .pad_ro : { . = . + RO_PAD; } } INSERT BEFORE .rodata;
SECTIONS { .pad_da : { . = . + DA_PAD; } } INSERT BEFORE .data;
