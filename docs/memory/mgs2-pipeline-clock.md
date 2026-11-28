---
name: mgs2-pipeline-clock
description: MGS2 draw pipeline vs the game clock: zero-time clock (ZT, adopted), reduced pipeline on undisplayed steps (OPT_LITE, on by default from v21), from-boot read/image checks, x86 dev tools
metadata:
  type: project
  modified: 2026-11-20T18:47:00.000Z
---

- Why: skipping PS2 draw-pipeline work (call of 0x11da90) on undisplayed steps, or a GE renderer, changes its virtual time, so hashes move. ZT makes the pipeline take no game time, so both can be checked bit-exact. Notes: speed_notes §31-34.
- DECIDED (Ffm, 2026-10-21 00:14, "A"): ZT is the clock model. boot.c rt_dg_call (last function in the file), g_opt OPT_ZT (32). References: prof/hashes_x86_{deck,w01a}_zt.txt; v20 cycle prof/hashes_x86_deck_v20.txt.
- Reduced pipeline: GEN_LITE builds (genf.txt -DGEN_LITE), boot/dg_lite.h functions return at FN_ENTER while g_dg_lite (only 0x1214a0, mesh packet builder). Rule (v21): reduced iff OPT_LITE && k_vblanks != dg_shown_vb && k_vblanks - dg_shown_vb < 2 (every call in a shown vblank is full: v20 reduced the 2nd call of two-call vblanks). DEFAULT ON from v21 (g_opt = ~0u).
- From-boot check (2026-10-21, speed_notes §34): game reads (x86 GEN_DG_TRACE MGS2_READ_HASH / READ_SKIP / READ_LOG; tools/dev/rh_cmp.py, rl_cmp.py with lanes, rl_skip.py) equal in count everywhere; 676 differing loads in 25 deck / 27 w01a vblanks, all leftover lanes their readers ignore (P125f18, S_d00t_P293a28, S_d00t_P295fa8 lane y, S_w00a_P3b7c88 loader, S_w01a_P2e8200, S_w01a_P3aaef8 slerp zw). Images (gsref): w01a full frames identical; deck full frames identical except the motion trail (samples the previous, reduced frame); gsref GSREF_SKIPTEX=0,1000 drops the trail sprite.
- Trail at 25 fps for the GE renderer: blend previous shown frame at 25% (FIX 0x20) = same weights as PS2's 50%-per-frame; recommended, Ffm decides (renderer_notes §9).
- Dev tools (x86): MGS2_PKT_HASH window + ee_io.h PKT_TAG, MGS2_PKT_DUMP + tools/dev/pkt_cmp.py, "dgc:" lines per call; GEN_DG_TRACE "pd" lines in tools/dgt_report.py. x86 GEN_LITE build /home/claude/v21/xlt (ephemeral).
- boot.c MGS2_GS_MASK=<file> adds "gs <hash>" (RAM without renderer-only ranges): prof/gs_mask_v1.txt, gs_mask_zt.txt, gs_mask_lite_v20.txt.
- Cross-call state: the pipeline reads 24,473 words it wrote in an earlier call; game code reads 1,716 pipeline-written words (prof/dgt_*_vb21000_25200.txt). A reduced call must keep both.
- Timing logs: the GS walk at vblank v walks call v-1's packets: tools/dev/steps_percall.py steps.csv.
- GCC PGO matches functions by definition order: new boot.c functions go at the end of the file.
- GSCAP GOTCHA (2026-11-20): scripts/twin.sh sets MGS2_OPT_CYCLE=1000:100:ffffffff,..., which overrides MGS2_OPT, so a capture keeps LITE on and every odd-vb frame gsref draws is our partial reduced frame (no 1991d8 meshes, ~18 luma darker), NOT the PS2's. For true PS2 pictures pass MGS2_OPT_CYCLE=1000:100:ffffffbf,ffffffbf. With that, odd and even frames match (<1 level) and our deck = PS2 display within ~2 levels, our shadows ~2 lighter [C, scratchpad r29cap ps6/, 2026-11-20 shadow-look check].
