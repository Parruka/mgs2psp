sb9: Stage C step 0, texture look-ups by upload identity (stagec_notes.md 0.3-0.7, task #128).  2026-11-01 22:45.
Container tree /home/claude/sb9 = sb8's tree (gl12's code) with private copies of boot/boot.c and psp/main_psp.c; pristine copies in
ge.orig, boot.orig, psp.orig, runtime.orig.  sb9.diff (diff -u against those copies) holds all of it.  To rebuild the tree in a new
container: copy /home/claude/sb8 (or apply tools/dev/sb8/sb8.diff over sb6), copy the shared boot/boot.c to boot/boot.c and the shared
psp/main_psp.c to psp/main_psp.c, then patch -p0 < sb9.diff in its root (-p1 finds no files; checked 2026-11-02); the scripts here go in the tree's root.

What it is (all exact; switches in a second word g_opt2, since g_opt's 32 bits are all taken):
- runtime/host_time.h: g_opt2; OPT2_TX_UMEMO 1 (0a), OPT2_GU_LDID 2 (0b).  boot/boot.c: g_opt2 = ~0u; opt_window reads MGS2_OPT2 (the
  whole run) and MGS2_OPT2_CYCLE=<hex>,<hex>... (g_opt2 in each of MGS2_OPT_CYCLE's configurations, in order).
- 0a, ge/texcache.c tk_get_um (gelive.c gl_tex calls it in place of tk_get_cl): G, 4-way x 128 sets (512 entries, 136 bytes each on
  the PSP), keyed by TEX0 & 0x3ffffffff and CLAMP: whether the texture is served (indexed, or 16-bit with a TEXA tk_get_cl takes), its
  size, region and the pages and blocks it reads (gsup2.c gu_area_pages, at most 16 pages).  T, 4-way x 256 sets (1024 entries, 64
  bytes), keyed by G's key, the pages' signature (gsup2.c gu_pages_sig: each page and lane group read holds one plain final record or
  nothing; one mix of each record identity) and TEXA for a 16-bit texture: the cache entry and its identity, the palette's last identity
  and entry (a hint).  A hit does exactly what tk_get_cl's hit does (uses, gu_clut_id, hits, touch, the palette through tk_pal with the
  hint).  The least recently used way of a set goes (a clock per look-up).  Check mode (x86 MGS2_TX_UMEMO_CHK=1): each would-be hit
  runs tk_get_cl and compares entry, identity, data, description, size, region and palette entry.
- 0b, ge/gsup2.c gu_clut_load: when the parameter table's look-up (ld_now) fails and that table holds a load with these parameters
  (its blocks are the new load's), the identity of the records those blocks hold now (ld_sig, all 3 lane groups of each block) finds a
  load in the identity table (4-way x 128 sets, 512 entries, in GuMap) with the same parameters, blocks and record identities block by
  block; it is used again (snapshot refreshed, seq and epoch now, the parameter table and the CLUT rows pointed at it).  The sweep keeps
  the table's loads and their records alive.  Check mode (MGS2_GU_LDID_CHK=1): clut_new runs and its load is compared.
- psp/xl_psp.c applies the two bits per transfer (as OPT_GU_MEMO) and prints "picture: step 0:" at the end; boot/xlive_chk.c the same
  on x86, plus the check modes.  psp/main_psp.c (private): steps.csv's cfg column and the log's per-setting means count a step with a
  g_opt2 switch off as 22 ("texture memo off"), 23 ("CLUT by identity off") or 4 ("other"); "speedups 2:" lines; built with the title
  HT (mkpsp.sh, pgo_gl.sh use), so the log's first line names the build again (gl8-gl12's said gl7's, from gl7's main_psp.o).

Results (x86 twin from boot: indoor route to vb 37,800, deck tutorial to 25,200, codec route to 40,000; chk9.sh):
- First version (direct-mapped G 256, T 512 tied to G by a serial, identity table 64): exact; look-ups answered 85.2% indoor, 88.5%
  deck, 93.7% codec; CLUT loads by identity 0.30 M / 0.21 M / 0.30 M.  The misses were table conflicts: an x86 simulation of other
  shapes on the same look-ups (chk9s.sh, -DUM_SIM, ge/texcache.c sim_*) gave the 4-way sizes above (T's key no longer tied to G).
- 4-way version, check modes: 0 differing of 5.70 M / 2.13 M / 5.53 M textures and 1.10 M / 0.57 M / 0.93 M CLUT loads; answered
  99.41% / 99.14% / 99.63%; the map's pools no larger than gl12's (records 96 KB, nodes 32 KB, loads 152 KB at most, x86 sizes).
- 4-way version on: every frame's picture line (both digests) and every state hash equal to gl12's (sb8/x1 out_*11on) and to the hash
  references: deck 11,766 frames / 252 hashes, codec 18,931 / 400, indoor 17,664 / 378; the same in the runs with both bits taking
  turns as gl13's cfg sets them (x1 out_*2turns.log).

Scripts: mk.sh <dir> [-DUM_SIM] the x86 twin; chk9.sh <i|d|c> <tag> [env] one run (x1), chk9s.sh the same on x1s (simulator);
mkpsp.sh <new dir> the PSP model build (no PGO; HT the title); models9.sh the model windows on /home/claude/sb9m (on, MGS2_OPT2=0, and
on the deck each bit alone off); pgo_gl.sh as sb8's (T = sb9, main_psp.o private in the use step, HT default "MGS2 harness gl13 texture
pages").  Compare with tools/dev/sb6 cmp6.py, href6.py, turnchk.py, fitcmp.py.
