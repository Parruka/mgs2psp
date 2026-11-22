#!/usr/bin/env python3
"""patch_b5.py <xl_psp.c (b4, 4be26fab)>: task #180 step 4b-2, diagnostic b5 on b4 (both switches off by default, one at a time a run):
MGS2_ME_PIC_NOLAY=1: the guards still log, but nothing of the log is laid over the copies (the check's control: the read bytes the
guards keep, which the check must then find changed); MGS2_ME_PIC_SCRAMBLE=1 (with MGS2_ME_PIC_CHK=1): after the check of each copy,
every quadword of it the transfer does not read (the margins after tags) turned over (xor 5a5a5a5a) before the copy's walk (do the
lanes' results hang on those bytes?)."""
import sys, hashlib
p = sys.argv[1]
t = open(p).read()
assert hashlib.md5(t.encode()).hexdigest().startswith('4be26fab'), 'unexpected source'
def rep(old, new):
    global t
    assert t.count(old) == 1, ('anchor', old[:80])
    t = t.replace(old, new)
rep("static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd, mp_chk_dboth;\n",
    "static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd, mp_chk_dboth;\n"
    "static int mp_nolay, mp_scr; static long mp_scr_q, mp_scr_rec;   /* me18c b5 (diagnostics, one a run): MGS2_ME_PIC_NOLAY=1 the guards' log\n"
    "                                                                  not laid over the copies (the check's control); MGS2_ME_PIC_SCRAMBLE=1\n"
    "                                                                  (with the check) each copy's margin quadwords turned over before its walk */\n")
rep('    mp_chk = (e = mgs2_getenv("MGS2_ME_PIC_CHK")) && atoi(e);\n',
    '    mp_chk = (e = mgs2_getenv("MGS2_ME_PIC_CHK")) && atoi(e);\n'
    '    mp_nolay = (e = mgs2_getenv("MGS2_ME_PIC_NOLAY")) && atoi(e);   /* b5 */\n'
    '    mp_scr = mp_chk && (e = mgs2_getenv("MGS2_ME_PIC_SCRAMBLE")) && atoi(e);\n'
    '    if (mp_nolay || mp_scr) fprintf(stderr, "me picture diagnostic b5: %s\\n", mp_nolay ? "the guards log as before, but their log is not "\n'
    '                                    "laid over the copies (MGS2_ME_PIC_NOLAY=1): the check must find the read bytes they keep" : "after the "\n'
    '                                    "check, each copy\'s quadwords the transfer does not read (the margins after tags) turned over (xor "\n'
    '                                    "5a5a5a5a) before its walk (MGS2_ME_PIC_SCRAMBLE=1)");\n')
rep("    xm_fill(&xmc, e->x0, e->nx, e->t, mp_log, mp_nlog); e->filled = 1; n++;\n",
    "    xm_fill(&xmc, e->x0, e->nx, e->t, mp_nolay ? 0 : mp_log, mp_nolay ? 0 : mp_nlog); e->filled = 1; n++;   /* (b5: NOLAY) */\n")
rep("      n++; mp_chk_lines++; a = f; }\n  mp_chk_rec += bad; mpc_rec += bad;\n}\n",
    "      if (mp_scr && mp_chr && qm != 15u) {   /* b5: the piece's margin quadwords turned over (the walk reads the copy after this) */\n"
    "        uint8_t *wp = xmc.arena + x[i].off + (a - x[i].a0); int k0 = 0;\n"
    "        for (uint32_t q = a; q + 16 <= f; q += 16) if (!((qm >> ((q >> 4) & 3)) & 1)) {\n"
    "          uint32_t *w = (uint32_t *)(wp + (q - a)); w[0] ^= 0x5a5a5a5au; w[1] ^= 0x5a5a5a5au; w[2] ^= 0x5a5a5a5au; w[3] ^= 0x5a5a5a5au;\n"
    "          mp_scr_q++; k0 = 1; }\n"
    "        if (k0 && !sc) { sc = 1; mp_scr_rec++; } }\n"
    "      n++; mp_chk_lines++; a = f; }\n  mp_chk_rec += bad; mpc_rec += bad;\n}\n")
rep("  const XvExt *x = xmc.ext + e->x0; int n = 0, bad = 0;\n  if (e->nh < 0) return;\n",
    "  const XvExt *x = xmc.ext + e->x0; int n = 0, bad = 0, sc = 0;\n  if (e->nh < 0) return;\n")
rep("                          mp_chk_lines, mp_chk_lrd, mp_chk_diff, mp_chk_rec, mp_chk_drd, mp_chk_dboth, mp_chk_dmg, mp_chk_skip, mp_chk_rdfail);\n",
    "                          mp_chk_lines, mp_chk_lrd, mp_chk_diff, mp_chk_rec, mp_chk_drd, mp_chk_dboth, mp_chk_dmg, mp_chk_skip, mp_chk_rdfail);\n"
    "      if (mp_nolay || mp_scr) fprintf(f, \"me picture diagnostic b5: the guards' log %s; margin quadwords turned over %ld (in %ld copies)\\n\",\n"
    "                                      mp_nolay ? \"not laid\" : \"laid\", mp_scr_q, mp_scr_rec);\n")
open(p, 'w').write(t)
print('patched', hashlib.md5(t.encode()).hexdigest())
