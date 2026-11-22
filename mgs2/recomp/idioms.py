"""Library routines of the game that the runtime runs natively (boot.c rt_idiom_*), found by their exact instruction words.
A match makes the function's first line "FN_ENTER(0x...); GEN_IDIOM_<NAME>();" (gen_pre.h): the native form runs when it can reproduce the
translation exactly (same stores in the same order, same registers, same virtual time) and returns from the function; otherwise the
translation below it runs.  The words carry no addresses (only relative branches), so one entry matches the routine in every stage image.
Used by recomp.py (new translations) and tools/idiom_apply.py (existing headers)."""
import struct
WILD_PREF = 'pref'   # a pref (op 0x33) word matches any pref: hints, no effect in the translation
IDIOMS = [
    # qcopy: a0 dst, a1 src, a2 qwords; one qword at a time up to a 64-byte boundary of dst, four at a time while more than four remain,
    # then one at a time (lq / sq, so the low 4 address bits are ignored).  108 stage images (w00a 0x3b7c88 pref dst, 0x3b7d68 pref src).
    ('QCOPY', [0x0080382d, 0x28e30000, 0x24e4000f, 0x00e0102d, 0x0083100b, 0x00021103, 0x30430003, 0x1060000f, 0x24020004, 0x00431823,
               0x1860000c, 0x00000000, 0x78a20000, 0x00000000, 0x24a50010, 0x24c6ffff, 0x7ce20000, 0x18c00021, 0x24e70010, 0x2463ffff,
               0x00000000, 0x5c60fff8, 0x78a20000, WILD_PREF, 0x28c20005, 0x14400010, 0x00000000, 0x00000000, WILD_PREF, 0x78a30000,
               0x24c6fffc, 0x28c40005, 0x7ce30000, 0x78a20010, 0x7ce20010, 0x78a30020, 0x7ce30020, 0x78a20030, 0x24a50040, 0x7ce20030,
               0x1080fff3, 0x24e70040, 0x18c00008, 0x00000000, 0x78a20000, 0x24a50010, 0x24c6ffff, 0x7ce20000, 0x00000000, 0x1cc0fffa,
               0x24e70010, 0x03e00008, 0x00000000]),
]
def match(W, start, end=None):
    """the idiom name whose words are at start (W(a): the instruction word at a), or None"""
    for name, words in IDIOMS:
        if end is not None and end - start < 4 * len(words): continue
        try:
            if all((W(start + 4 * i) >> 26 == 0x33) if w == WILD_PREF else W(start + 4 * i) == w for i, w in enumerate(words)): return name
        except struct.error: pass
    return None
