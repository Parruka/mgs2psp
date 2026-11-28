# The random "freeze, then off" stops: study (2026-11-27)

Research only: no PSP run, no build, no repo push. Written by the thread "Find the cause of the random PSP freeze" for the MGS2 port
thread. Marks: **[R]** read (in our logs on the PC, our notes, or a public page named); **[I]** inferred, basis given; **UNKNOWN** = no basis.
Hard lines kept: nothing here writes flash, goes above 333 MHz or writes a power or clock register.

## 1. Short answer

- The cause is still UNKNOWN. Public PSP sources add little: nothing found describes this signature under heavy load (section 5).
- **One belief in our notes is wrong [R].** The notes say "USB charging runs: 0 stopped" and the split by external power was the
  strongest clue. The battery logs on the PC say otherwise: **gl29 r1 stopped while charging** (its last line, t 1,994 s, vb 30,760:
  100 %, 4,230 mV, charging 1), and **gl42 r1 had the USB cable charging until t 1,341 s** (100 %), then not charging for 312 s before
  its stop at vb 29,561 (96 %). From gl16ep2 on: battery alone **4 stops in 15 runs**; cable in and charging lines in the log **2 in 16**;
  gl42in r1 unknown (its log was overwritten). That difference is chance-level (one-sided p ~0.29 [R arithmetic]). The power source is
  not the clear split we thought. The memory line "gl29 r1 battery alone" needs a correction.
- **Where it happens [R]:** 5 of the last 7 stops fall in vb 28,400-31,600 (rain deck, the walk, the door): gl40 r1 28,459, gl42 r1
  29,561, gl29 r1 30,760, gl29x r1 31,039, gl42in r1 ~31,3xx; the other two were gl27 r1 16,551 (codec 1) and gl17 r1 33,044 (rooms).
  This is the heaviest stretch of the route: window means 76-146 ms a step, the worst single step 278 ms, main CPU, ME and GE all busy
  (gl29 window 30,700: GE wait 19.8 ms; gl29x window 31,000: picture 22.2 ms) [R log.txt]. Each run passes through it once, so the
  stops are more likely per minute there than elsewhere [I by count: small numbers].
- **Charge does not matter much [R]:** stops at 77-100 % and 4,074-4,230 mV. No stop came near low battery. A simple low-voltage cut
  is unlikely [I]. A short current peak is not ruled out: the gauge moves in 6-mV steps every 5 s and cannot see ms-long dips [R, me_switchoff_plan.md].
- **The signature [R, our notes]:** picture freezes, then the PSP is off after a few seconds (sometimes black at once). No crash record
  (catcher 3b), the battery thread (priority 17) stops too, cold boot after. Our own catchers, when they stopped the CPU with
  interrupts off, switched the PSP off about 10 s later [R, psp_harness.md gl4]. So "frozen picture, then off" fits **the whole CPU
  stopped (interrupts off, a bus lock, or an electrical glitch that hangs the chip) followed by the system's power cut** better than a
  clean power loss, which would be black at once [I].

## 2. Causes, ranked

| # | Cause | For | Against | Rank |
|---|---|---|---|---|
| 1 | **A whole-chip hang under peak load** (bus lock or deadlock between main CPU, ME, GE and memory-stick DMA on shared memory; or an electrical glitch at 333/166 MHz that hangs the chip), ended by the system's power cut | Stops cluster in the heaviest stretch [R]; whole system stops, picture first [R]; not at low charge [R]; random, not at a fixed vb [R]; the psp-developer reference names "kernel hang -> watchdog -> power cut" as the usual meaning of "switched itself off" [R third_party/psp-developer power-exit-sleep.md §4] | v24 and gl1 stopped with the ME off [R]; the old codec-call stops were in light stretches [R]; no direct trace yet | most likely [I] |
| 2 | **Supply path under load** (battery internal resistance or its protection chip, the PSP's power regulator, the cable/USB path), a dip that hangs or cuts | The cheap GH-168 pack: 4 stops in 63 min of codec calls, 0 in 88 min without it [R, me_switchoff_plan.md]; peaks invisible to the gauge [R]; heaviest stretch = most current [I] | The OSTENT also stops; stops at 100 % with the cable in (gl29) [R]; the charging split is gone (section 1) | possible [I] |
| 3 | **Heat** after ~25-35 min at full load | Most stops come 26-37 min in (t 1,577-2,210 s; gl27 r1 at 1,014 s is the exception) [R]; charging adds heat and charging runs stop as well (gl29) [R] | Many runs pass 37-70 min (gl18me 70 min, gl27 r2 71 min, gl4c) [R]; battery temperature reads 20 C always, so no data [R] | possible [I] |
| 4 | **System code waiting on something it never gets** (the system's ME handler, the battery driver's thread starved, a system event) | The system's ME wrapper waits on the ME without a timeout on some power events [R, psp-developer power-exit-sleep.md §6, citing uofw me_wrapper.c]; low-priority threads wait up to 0.35-1 s in these stretches [R gl42 battery.txt] | No power event logged before any stop (the power callback writes at once) [R]; gl42 r1's battery thread was on time 5 s before the stop (late 41-163 us) [R]; gl28 removed the long starvation and stops went on [R] | less likely [I] |
| 5 | **Memory-stick I/O** (live shots, log commits, the stage load at the door) | gl42in r1 stopped at the door load; live shots run in gl29, gl29x, gl42in r1 [R] | Live shots also run in 13 passing runs (gl30-gl42 r2); gl40 r1 and gl42 r1 stopped with no live shots [R]; memory stick bursts were ruled out in October [R memory] | weak [I] |
| 6 | **PSPLink / USB plugin** | It is loaded in many runs [R] | Stops with the plugin off (gl29x, gl27, gl40) and on (gl29, gl42) [R] | weak [I] |
| 7 | Auto-sleep / auto-off timer | - | scePowerTick every window in main_psp.c [R]; a timer would not freeze the picture first | ruled out [R] |
| 8 | idstorage key check that powers the PSP off | A public post: a PSP that "freezes then shuts off" can have bad idstorage keys, checked on a random ~12.5 % of boots and resumes [R, SilverSpring blog 2008-01 at uofw.github.io] | Only at boot and resume; we never sleep [I] | ruled out [I] |
| 9 | Our code (a crash in generated or harness code) | - | Exact to every stop; catcher 3b catches user crashes and passed its self-test; the emulator, the PC twin and sanitizers ran the same route clean [R notes] | unlikely, not zero (system-code faults are not caught) [I] |

The main open split is **1/2/3 (hardware state under load) against 4/9 (software)**. The tests below split those first.

## 3. Cheap tests and log changes for the next build (no new register write)

Listed best first. Each is user mode, behind a cfg switch, one change per run where it is a lever.

1. **Never lose a log again (H1, must have).** At start, if log.txt has no end line, rename log.txt, battery.txt, crash.txt (and
   steps files) to `*_prev1.txt` (shift prev1 to prev2, keep 3) before writing anything. gl42in r1 was lost this way [R]. Pure file
   renames; cost nil.
2. **Black-box heartbeat (H2).** A thread at priority 0x10-0x11 (beside the battery thread, above the game) writes one fixed 128-byte
   record a second into a file made full size at start (e.g. 3,600 records = 450 KB, zero-filled), with open, seek, write, close each
   time so the FAT never has to change the file's size [I: FAT keeps a size only from its close, research_review.md M13]. Fields:
   t (us), vb, the game thread's step counter, a **phase tag** the game thread sets at each boundary (logic, draw pipeline, GE wait,
   ME wait, disc read with file+offset+size, log commit, shot write), the ME loop heart and its last command, the last GE list id and
   a non-blocking sync status of it, battery mV, %, charging, external power, free memory, the low-priority canary wait.
   How to read a stop:
   - the game counter stopped but the heartbeat went on for N more seconds -> a stall the system survived for N s (software or a wait),
     and the phase tag names where;
   - both end in the same second -> the whole chip stopped (interrupts off, bus lock or power) [I], which points at causes 1-3;
   - the last record's phase tag over several stops: always "GE wait" or "ME wait" points to a bus or ME/GE deadlock; random tags point
     to power or heat [I].
   Cost: one small write a second on another thread, about 1-5 ms of that thread [I; one in-frame log write was measured at 107 ms on a
   PSP-3000, psp-developer real-hw-gotchas.md, so keep it off the game thread].
3. **What Ffm sees and hears at a freeze (zero cost; put it in the README).** (a) does the sound stop, keep playing, or buzz in a short
   loop? A buzz loop means the audio hardware is still powered and replaying its last buffer while the CPU is stuck [I]; (b) the power
   LED: steady, blinking, or off; (c) roughly how many seconds from freeze to off; (d) does the back feel hot. And after a freeze: start
   only PSPLink, never the game (already a rule).
4. **A stress EBOOT with no game (H5, splits hardware from our game).** Main CPU running a hot recompiled loop, the ME running the picture
   loop on recorded lists, the GE drawing them, a 4 MB pack read every 60 s and a BMP write every 10 s, 60 min, with the heartbeat. On
   battery, 3 runs. Stops in it -> hardware, power or the system under this load (our game code is out); 3 clean hours -> the game's own
   path or its timing [I]. Proposed already in gl42in_psp_read.md; it costs a few hours of build work and no game data on the stick
   beyond the lists.
5. **Load arms on the same build, on battery (H6).** One run with nothing drawn after vb 28,000 (K7 for the rest; gl4c-p0's idea), one as
   normal. Few stops per run, so this needs 5+ runs per arm to say much (psp-developer power-exit-sleep.md rule) [R]. Use it only if
   the stress EBOOT does not reproduce.
6. **A lower-clock arm (optional; the lead's call).** The same documented call the harness already makes at start, with 266/133 or
   222/111 instead of 333/166. Not a register write and not above 333. Lower clock lowers chip power and heat; if the stops vanish at a
   lower clock over several runs, the margin at 333 (power or heat) is the cause [I]. The run is slower but the hashes stay exact (no
   pacing). Check the clock line in battery.txt: ARK-4 can override the call.
7. **Heat arm (informal).** One run started right after 30 min of the stress EBOOT (warm PSP) and one with the PSP kept cool (a desk
   fan). An early stop in the warm run points to heat [I].
8. **Peak current, indirectly.** Log the capacity (mAh) and voltage every second in the heartbeat instead of every 5 s. It will not show
   ms dips [R: the gauge is smoothed], but a step in the gauge right before stops would be a clue. Low value; free with item 2.

What NOT to do: no performance counters (one of their registers powers the PSP off [R, psp_harness.md A10]), no clock changes during a
run (psp-developer overclocking.md) [R], no new power or clock register write, no overclock.

## 4. Data table (recent era, LOGS on the PC; battery.txt read 2026-11-27) [R]

Stopped: gl17 r1 (battery, 86 %, vb 33,044, t 2,210 s); gl27 r1 (battery, 93 %, 16,551, 1,014 s); gl29 r1 (charging, 100 %, 30,760,
1,994 s); gl29x r1 (battery, 77 %, 31,039, 2,030 s); gl40 r1 (battery, 88 %, 28,459, 1,577 s); gl42 r1 (cable in, charging ended at
1,341 s, 96 %, 29,561, 1,653 s); gl42in r1 (unknown power, ~31,3xx, log lost).
Passed on battery alone: gl16ep2, gl18me, gl19, gl25, gl26, gl27 r2, gl31, gl34, gl36, gl40 r2, gl42 r2.
Passed with charging lines: gl17b, gl20, gl21, gl22, gl23, gl24, gl28, gl30, gl33, gl35, gl38, gl39, gl41, gl42in r2.
"external power 1" never appears in these logs: the AC adapter was not used in this era [R].
Live shots (memory-stick writes in the deck) in gl28-gl42in r2 except gl40 r1 and gl42 r1 [R].

## 5. Public sources read

- psp-developer skill copy, mgs2/third_party/psp-developer/references/power-exit-sleep.md §4-7 and real-hw-gotchas.md §5, §8 (PS2RC
  licence; the project's own copy): "switched itself off" = usually a kernel hang ended by the watchdog; the system's ME handler waits
  without timeout; per-frame memory-stick writes stall; scePowerTick; battery temperature reads a fixed 20 C on a PSP-3000 [R].
- SilverSpring blog 2008-01 (uofw.github.io/upspd/docs/SilverSpring_Blog/...): idstorage keys checked at random on boot/resume can
  power a PSP off [R]; not our case.
- exophase forum "how to handle runtime errors": a homebrew crash makes the PSP "turn itself off after a while" [R]: fits cause 1/9.
- reddit r/psphacks "ARK-4 custom launcher freezes and automatically [shuts off]": freeze then off, blamed on memory-stick adapters,
  replacing the adapter did not fix it, no cause found [R].
- PSPTEK (uofw): power supply facts only, nothing on watchdog or thermal cut [R].
- Not readable from here: gbatemp thread "333 MHz kills battery, polyfuse?" (robots-blocked), old procfw issues (need JavaScript).
- No public page found on a freeze-then-off under heavy ME+GE load on a PSP-3000. Our own data is the best evidence we have.

## 7. Sony's PSP-2000 service manual (TA-085 board, level 3; shared by Ffm 2026-11-27)

The manual is schematics and a parts list only: no text on fault states, power light codes, thermal cutoff or troubleshooting [R].
Ffm's PSP-3000 has a different board, so these are leads for the same design family, not facts about the 3000 [I].
What the schematics show [R, pages 1-8 and 1-9 of the manual]:
- **Two power chips.** A charger and power-path chip (IC6601) takes the battery (connector with plus, minus and a battery data line),
  the USB input (1 A fuse, with a USB charge-enable line from the controller) and the DC jack (2.5 A fuse), and makes one system
  supply, VSYS. A power chip (IC7001, Fujitsu MB44C015, called "Pommel" in the signal names) makes every rail from VSYS: core 1.2 V,
  DDR 1.9 V, I/O 1.9 and 3.2 V, memory stick 3.2 V, LCD 3.2 V, Wi-Fi, audio and others.
- **Hardware fault lines to the controller.** The power chip has an alert output (/POMMEL_ALERT), a power-fail detect output (/PF_DET),
  a regulator enable input (REG_EN), a reset line and an I2C link. So a dip or fault on a rail can be flagged and acted on in hardware,
  without the operating system seeing an event [I from the signal names; how the controller reacts is not in the manual].
- **The battery's data line** goes to the controller [R]: the pack is read over its own line. A fake pack answering wrongly could be acted
  on there [I]; it fits the old GH-168 stops.
- No temperature sensor line was found on the power pages [R, pages 1-8 and 1-9 only].

What it changes in this study [I]:
- It supports cause 2 (supply path) as a real mechanism: a short dip under peak load can trip the power-fail detector and the controller
  can cut power with no record and no power event, which is our signature. With a hang first, the order would be: dip, chip glitch
  (picture freezes), then the controller's cut.
- On USB the input is limited (fused at 1 A; a PC port gives about 0.5 A), so the battery still carries the peaks. That explains why
  stops also come with the cable in (gl29 r1, gl42 r1). **Only the DC jack (the AC adapter) can carry the peaks from the mains.**
- New test, cheap: **AC adapter arm**, plugin OFF (no USB), the same build, several runs (5+ per arm). In this era no run used the AC
  adapter ("external power 1" never appears in the logs [R]). In October the adapter had 0 stops in 2 runs [R]. If the stops vanish
  on the adapter over 5+ runs, power is the cause; if they stay, power is out and cause 1 (a hang from our load or code) remains.

## 6. Suggested order for the MGS2 port thread

1. In the next build (gl43 or gl44): H1 (keep unfinished logs) and H2 (1-s black-box heartbeat with phase tags), plus the README
   observation list. Neither changes the game or its timing on the game thread [I].
2. If a stop comes: read the last heartbeat records first (section 3 item 2). That alone splits "whole chip" from "a wait".
3. In parallel when there is time: the stress EBOOT (item 4), and AC adapter runs with the plugin off (section 7).
4. Fix memory: gl29 r1 was charging at its stop; the "USB charging 0 stopped" line is wrong.
