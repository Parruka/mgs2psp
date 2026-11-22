# gl31 plan: gl30 (deck shadow marks) + native step 1 (ship meshes from records), 2026-11-20

Lead plan for the MGS2 port thread. gl31 is the next PSP build after gl30. It is BUILT and GATED now, but DELIVERED only after gl30's PSP log is read (one build out at a time). If gl30's log shows a problem in the shadow-mark code, the fix goes into gl31 before delivery.

## What gl31 is
- gl30 (tree /home/claude/t30, build /home/claude/main/build_gl30, EBOOT 9d2e688c): the deck shadow marks drawn from their floor piece (arm X) plus subset pairing (XS) and the cheap reject. Report mgs2/main/attic/gl30/report.md.
- Native step 1 (tree /home/claude/t30n, build /home/claude/main/build_gl30n, EBOOT 8d72094d, not delivered): ship meshes drawn from 48-B group records. Report mgs2/native_step1_report.md.
- gl31 = both, with `MGS2_NAT_CYCLE=0,0,3,3` (builder, builder, native, native; 100-vb turns from vb 24,001; each arm meets one ME turn and one main-CPU turn every 400 vb).
- **No arm C (4) in gl31** (lead decision): arm C removes the ship for 200 of every 600 vb, a visible change, and the owner watches the deck in these runs. The upper bound arm C would give is already known roughly (ship meshes 8.95 ms a step on the ME, gl26 split [C]). Arm 4 stays in the code.
- cfg = gl30's (deliver/mgs2.cfg d0ff49e0; `MGS2_GL_DZ=29490:31400:X`, `MGS2_ME_PIC=4`) with gl31 header lines and the NAT key with its comment at the end. Nothing else changes.

## Tree
- New tree /home/claude/t31 = a copy of /home/claude/t30n (its runtime/ is a real folder; check that NO path in t31 is a symlink into /home/claude/main before editing anything: `find /home/claude/t31 -type l`).
- From gl30: ge/gelist.c (c0173220) and ge/gelist.h (d2cbeda6) from t30; t30n's copies equal t29's (gelist.c 9192694d), so take t30's whole.
- psp/xl_psp.c: diff3 of base t29 (90ee3f3c), t30 (1649af98), t30n (3b1c4ee8). The native agent's dry run merged clean.
- The twin's boot/xlive_chk.c: gl30 changed its own copy (build_gl30/m30/boot/xlive_chk.c ab40060a: the digest line gains `sub:`); step 1 changed t30n's boot/xlive_chk.c. Merge both against their common base. Check for any other file both changed (compare against t29/main).
- Read-only: /home/claude/t29, /home/claude/t30, /home/claude/t30n, /home/claude/main (everything outside main/build_gl31), main/build_gl30*, /mnt/project-files/mgs2/main/ (except attic/gl31/). t28/t29/t30 runtime/ are symlinks into main/runtime: never write through them.

## Gates (all on the final tree and the exact deliverables)
- **G1 twin.** (a) NAT key absent: hashref and every digest equal gl30's twin (build_gl30/x86/out_g1.log, hashref_g1 4304f7e0). (b) With `0,0,3,3` and gl30's DZ: dz counts (incl. sub:) equal gl30's twin in every frame; content digests equal gl30's in arm-0 turns; in arm-3 turns equal (a) except the known HUD never-tested-Z set (natq equal in all). The new hashref.txt comes from this twin run.
- **G2 read hash.** Base vs `0,0,3,3`: hash and read count equal in every vblank to 31,400. Name the readers of any value that differs.
- **G3 emulator.** PPSSPP 1.20.4 headless, `PPSSPP_VFPU_RN=1`, the exact EBOOT + cfg + hashref (em30.sh pattern): 62/62 hashes and 916/916 digests = twin. Pictures vs gl30's emu_g3/pic: arm-0 turns identical (shot_24784 is known noise); arm-3 turns identical or explained. Counters: dropped 0, fallbacks 0, early fills 0; dz fallbacks 0.
- **G4 ME.** xme_rest_chk, xme_me_chk, mesec_chk, me_call_chk pass; psp.sh rc 0; no register writes or system calls beyond gl30n's.
- **G5 memory** (emulator, not the PSP's): heap headroom at its lowest and the outside-heap blocks, vs gl30 and gl30n.
- **G6 reader.** rd31.py = rd30.py + the `nat turn` lines (per arm and per ME/main turn: the harness's turn times, nat counters, holds); reads the emulator log; give the command line for the PSP log.
- **G7 reproducible.** A rebuild into a second folder gives the same EBOOT and overlays.
- Optional if cheap: PspProf model [C model] of main-turn and ME-turn cost, arm 0 vs arm 3, in a rain window.

## Deliverables (build_gl31/deliver, not delivered)
EBOOT.PBP, the 8 overlays, mgs2.cfg, hashref.txt, pad.txt, README.txt draft (what changed, what the run should show, keep the USB cable in: PSPLink stays on), MANIFEST.md5 in gl30's format.

## Limits
No PSP access, nothing on the owner's PC, no game data or pictures outside the container. Delete only inside build_gl31 and t31, by literal path after looking. Stop processes only by PID. python3 -I. Disk: about 6.9 GB free at the start; keep the new use under about 2 GB; delete emulator game folders after checking. Report in the final message (md5s, gates [C], UNKNOWN, deviations from this plan).

## Update 2026-11-20 22:00 (after gl30's PSP log was read; sent to the build agent)
1. Flip wait in every turn from the start: `MGS2_GL_FLIPWAIT=2`, `MGS2_GL_FLIPWAIT_FROM=0` (was 30500). gl30's log: 1 list drawn into the buffer on the screen in the flip-off ME turn vb 28,800-28,900, right after codec call 2, where the owner saw a flick; 28 in 26,000-26,100 [C counts; that this was the flick is I, by time].
2. Shadow arm X for the whole run (was 29490:31400). Before vb 29,490 the deck after the codec was drawn O with the old holes; the owner saw the shadows come late. Census over the whole route; pictures outside 29,490-31,400 vs gl30's emulator differ only inside 0x42 coverage.
