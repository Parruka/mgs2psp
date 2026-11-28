/* hot39.h (gl39, the data-placement fix): H39 puts a small object the picture's hot paths read or write at every call (the lanes'
   records, the recognizers' counters, the walk's pointers, the last transform kept) in one section, .bss.h39, which psp.sh renames
   .me_bss.h39 in the ME-safe objects and psp/me_sec.ld places first in .me_bss, on 64-byte lines of its own: the objects lie together
   in the link's order, whatever the tables before and after them weigh, instead of each among its file's tables (gl38: the same objects
   on 29 lines among cold data; attic/gl39/dh_main_turn.txt).  The code is the same; only where the objects lie changes.
   -DMGS2_NO_H39 (and every build that is not the PSP's): the objects where they were */
#ifndef MGS2_HOT39_H
#define MGS2_HOT39_H
#if defined(__psp__) && !defined(MGS2_NO_H39)
#define H39 __attribute__((section(".bss.h39")))
#else
#define H39
#endif
#endif
