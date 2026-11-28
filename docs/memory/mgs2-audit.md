---
name: mgs2-audit
description: MGS2 audit 2026-10-23 (mgs2/audit_2026-10-23.md): the 4 High and 8 Medium findings, what has been measured or decided for each, what is still open
metadata:
  type: project
  modified: 2026-10-23T18:20:00.000Z
---

Audit asked by Ffm 2026-10-23 10:37; file mgs2/audit_2026-10-23.md. Verified: all HW hash claims v5-v23, note timings vs steps.csv, rules kept.

High:
- H1 code size. MEASURED (feasibility.md §9): full -O2 ELF 4.20 MB + w00a 16.33 MB (w11a 21.88): 56.5-62 MB vs ~51 MB: does not fit. Plan told Ffm: compiled hot set per stage + exact interpreter for the rest (replaces rt_missing). Real-play working set MEASURED (speed_notes §42, x86 play probe): 19 deck sessions + tutorial = 3,247 functions 8.58 MB; hottest set in today's room leaves the interpreter ~21 instr/step (max 76); unseen modes run whole interpreted (codec 54k/step). Free play needs 1,429 functions v24 lacks. PSP free-play build MGS2T_v25f BUILT 2026-10-23 (psp_harness.md "Build v25f", speed_notes §43): v24 code + the route's 1,071 extra functions via a hook; emulator exact 431/431; model 85.6 ms/step (tutorial 72.4), ~82.8 on HW [I]; goes to Ffm after the v24s log is read. Then interpreter (x86 exact first, then PSP speed).
- H2 GE fit measured with CPU idle. MEASURED (MGS2GE v6): 41.8-45.1 ms with the CPU busy, CPU at 75-83% speed meanwhile (renderer_notes §11.11).
- H3 25 fps estimates omit IOP+sound: deck 13.4-15.0 fps (Ffm was told 15-17), crew deck 22.6 (told "close to 25"). HW v24w: w01a 26.84 ms/step = logic 15.21 + draw 8.88 + IOP 1.96 + GS 0.78 (v18 27.97). HW v24 deck 70.04 (v23 70.00). CAVEAT (§42): all deck timings are the scripted tutorial; free play ~78-84 ms/step [I]; v25f model 85.6 (§43), HW run pending.
- H4 audio not built/budgeted. Census (sound_notes): Hall reverb both cores, per-voice sends, core 1 stream input; deck 19.3 voices mean / 24 max. MODEL: spu2_lite full mix (MGS2_SPU_MIX=1; hashes exact on x86 252/252 and PPSSPP 370/370) +5.6 ms per deck step before reverb/Gaussian/sends (~11 ms of a 40 ms frame). ME option = KUBridge kernel call + writes to ME reset / memory-range / bus-clock-enable regs: needs Ffm's word. HW v24s (2026-10-23 18:14, prof/hw_v24s): hashes exact; mix +7.53 ms per deck step (model 5.6, 1.34x) = 15 ms per 40 ms frame at 25 fps before reverb. DECIDED (Ffm 18:28, card): MEDIA ENGINE for the audio mix. Next: ME sound probe (sound_notes); describe it to Ffm before it ships.

Medium:
- M1 DECIDED (Ffm 12:49, card): keep the PSP denormal rule; each new route also gets an x86 PS2-rule run (m1den README) that must equal build A (speed_notes §40).
- M2 MEASURED (v24w HW): break never moves; end: 2291 KB in use, malloc gives 2960 (above break) + 1792 (in arena) KB + 764 outside heap. Old readings: 4096 at start = free chunk in the 4 MB arena the start-up probe left, 2960 at end = room above break [I]. PPSSPP has 5120 KB more.
- M3 MEASURED (v24 deck HW 14:32, prof/hw_v24): the 12 deck IOP spikes are the 12 pack reads on the game thread: 12.9-31.0 ms reading + 5-8 ms IOP work each (other steps 2.58 ms). Fix when it matters: read-ahead stream thread. They are Otacon's voice (VOX.DAT, x86 disc log) in the scripted M9 tutorial.
- M4 §11.6 pairs merged-list CPU with unmerged GE time. M5 fixed in v24 (hash every 100 vb on the deck). M6 PPSSPP interp/JIT drift (bisect open). M7 CHECKED (sound_notes): PCSX2 ADMA timing moves game state from vb 15,540, nothing visible; keep ours until the audio output. M8 research_review items open.

Low: stray RAM dumps deleted with Ffm's OK (boot.c refuses relative or /mnt dump prefixes). L items open.

Related: [[mgs2-speed-work]], [[mgs2-renderer-work]].
