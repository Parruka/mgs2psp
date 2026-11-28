---
name: mgs2-routes-and-hashes
description: The scripted MGS2 routes (deck tutorial, deck free play, indoor crew deck), what the game does at which vblank (codec calls, stage loads), the state-hash references builds are checked against
metadata:
  type: project
  modified: 2026-10-27T20:16:40.293Z
---

- Boot: the x86 probe boots title -> New Game -> Tanker deck w00a with a scripted pad, deterministic (mgs2/elf_notes.md, sound_notes.md, recomp/README.md, psp_harness.md).
- Deck route: all deck timings (vb 24,001-25,200) are the scripted M9 tutorial (pad ignored). Free play on the aft deck from ~vb 30,000 (play/pad_free.txt, START taps); its last part vb 34,201-36,500 never alerts (called "alert" in old notes: label wrong). Free play logic 1.07-1.27 M guest instructions a step against the tutorial's 0.92 M; PSP logic time follows the count (34.5-38.1 ns an instruction).
- Indoor route (task #33, play/w01a_route.md): pad play/pad_w01af.txt, timed vb 31,801-37,800 (walk, crawl, first person, a real alert, evasion). Same game as the deck route to vb 23,400.
- Indoor route by vblank (x86 xlive, 2026-10-27): stage loads init 32, n_title 407, r_tnk0 3,119, d00t 3,554 (opening; black on the picture builds, its VU1 entries have no converters), d01t 20,136, w00a 23,244, d01t 23,663. CODEC CALL 1 vb 15,643-19,901 (faces from 15,759); CODEC CALL 2 vb 26,070-28,762 (faces 26,188-28,708, voice reads from VOX.DAT every ~60 vb end at 28,478). START taps every 1,000 vb from 23,410.
- Working set (task #20): 19 sessions 8.58 MB of code; with today's room (ELF 2.31 + w00a 4.2 MB) an interpreter fallback gets ~21 instructions a step; new modes (codec 54 k instructions a step) are the risk.
- STATE HASH 2 (2026-10-26, task #43, psp_harness.md "State hash 2"): word hashes 8x faster than the old bytewise FNV-1a (1.9 s each on the PSP); references prof/hashes_x86_*_h2.txt (every 100 vb: ee iop vfb) and hashref_x86_*_h2.txt (the split finder) for every build from v31; MGS2_HASH_FNV=1 prints the old pair. Compare with tools/dev/hashcmp.py.
- x86 RUNTIME REBUILDS must carry the AOT VU0 programs (tools/dev/x86_ref.sh: -DMGS2_GEN_VU=/home/claude/psp/vp22/gen_vu.h -DVP_FAST -DVP_VFPU): without them the EE hash leaves the reference at vb 4,300 (IOP, vfb equal) (2026-11-07, v31 objects + shared boot.c 707ba23e: 310/310 on route i with them).
- Alert check: 0x1bcacc is NOT an alert flag (2 while ALERT shows indoors); 0x1e10b4 0x00c03030 = the guard's call [I]; frames are the alert check.
