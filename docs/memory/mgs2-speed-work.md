---
name: mgs2-speed-work
description: MGS2 PSP CPU-side speed work (to v26 on the PSP, 2026-10-24): cost model, builds, reference hashes, exactness rules, PGO, layout flow, native forms, deck logic by actor, next levers
metadata:
  type: project
  modified: 2026-10-24T11:25:00.000Z
---

- Cost model (speed_notes §21): ms = (1.4 x instr + 70 x (I-miss + D-miss)) / 333,000 per step. Compared on the SAME steps it reads 6-9% below HW on every route (tutorial x1.08, crew deck x1.075, free play x1.06; §44): predict PSP = model x ~1.07. Never compare a model window with a whole-run HW mean (v23/v24 "3.4% high" was that error). Tracks deltas.
- x86 probe builds MUST use -DGEN_MM_FAST -DGUEST_SPR0; make ignores flag changes (delete objects).
- References: prof/hashes_x86_{deck,w01a}_v21.txt + split finder prof/hashref_x86_{deck,w01a}_v21.txt (v21-v23). Emulator: PPSSPP_VFPU_RN=1 + patched PPSSPP, tools/dev/exact_run.sh, compare with tools/dev/hashcmp.py.
- Exactness: PSP FPU keeps x +- 0 = x even for denormal x (x86 guest.h FADD/FSUB). VU fallbacks round like the VFPU. PSP long is 32-bit.
- PSP build: tools/dev/rebuild_use.sh <dir> [objs] (PGO=use LAYOUT=1; W=, VP=, TITLE=); it builds with -w: check PGO mismatches with a warnings-on recompile.
- PGO: tools/dev/pgo_run.sh (deck + w01a). Profile and order go together. Runtime-only refresh: rebuild boot_rt.o AND main_psp.o (-DMGS2_PGO) in the gen build.
- Layout: trace_run.sh IT=1, 8 windows per route -> layout_funcs.py -> layout_sim (both routes, -wl 2 -wd 1 -test 2, 2 seeds x 40k, ~25 min; with the picture code ~45 min: start background runs with timeout 7200000, the default 30-min limit killed both gl9 seeds 2026-10-30) -> relink_order.sh -> layout_sim -new must equal the search. -base1 = arena + 0x34 (v23 08bcc034).
- PspProf windows run PPSSPP's interpreter (deck trace runs can drift). Kill PPSSPP via /proc/<pid>/cmdline, never pkill -f.
- VU0: all micro programs of both routes compiled since v22 (vp22). Add programs with VP_VFPU=<list> so no hash moves.
- Native forms hook generated headers at exact lines (keeps PGO): v19 line_sph (tools/native_apply.py, HW -2.48 ms logic), v23 P12dbc0 sort key (boot/native_elf.c, tools/native_elf_apply.py; check build -DNAT_DBC0_CHECK compared 4.47 M loops).
- Budget at 25 fps: ~12 ms logic per step; gameplay 50 logic steps/s.
- Dropped: VU0 regs in C locals, hot/cold split, per-reg widths, -Os, slerp on the VFPU (§28). ME offload: don't pursue.
- Area redirect: MGS2_POKE "w01a" = 0x61313077 at 0x1190ed0, 0x1190f78, 0x1af8118, 0x1af81c0 at vb 21000 and 22950.
- Tutorial deck logic by actor (v22, §38): 37 ms, weather + ropes ~59%.
- PGO switch chains (§43): GCC lowers a switch's never-profiled cases into one long compare chain; v26 replaced the stage switches by a sorted table + cache (tools/stage_tab.py, OPT_STAGE_TAB): stage-call path 4.33 -> 1.01 ms/step (§45). layout_sim's I-miss estimates hold in the model (v26 order -32% vs -37% simulated).
- v26 on the PSP (§47, prof/hw_v26): free play 88.72 ms/step (v25f 92.42) = logic 47.68 (-4.01) / draw 33.60 (+0.32, every part: order or data addresses [I]); walk window model -4.02 vs PSP -3.63 ms. Next order: keep or weigh the draw pipeline's code.
- Free play by actor (§46, prof/actors_free_v26.txt): logic 48 ms (model) = weather 7-11, other visual 8, 3 guards (watcher.c) 7, rest 7, player 6.4, ropes 4.4-6.4, IOP wait 2.7, doors 1.5. Flat: 1,250 KB of code per 100 steps, I-misses 30% of logic cost; 47 k calls/step (gen_mm out of line 14.9 k).
- Next CPU levers: native box cull vupv_660a267e_0 (4.06 ms/deck step, ~800 instr/call; must leave VF/VI/CLIP/MAC exact); render-only work elision in the effects (§38); D-cache data placement; GE renderer [[mgs2-renderer-work]] [[mgs2-pipeline-clock]].
