# ME start-up study: our Media Engine code against Sony's and the homebrew projects

2026-10-26. Research only: nothing in the project was changed except this new file. The question: why does Ffm's PSP-3000 (ARK-4) sometimes switch itself off while our ME code mixes the sound? I compared how we start and run the ME with Sony's kernel and with homebrew ME projects.
Our files are under /mnt/project-files/mgs2 (path:line). Other code is cited as project path:line at the commit below.
[I] marks my own inference. Register meanings are quoted from psdevwiki or from the code's own comments.

**The main new finding.** Our stub goes to 0xBFC00000. That overwrites the kernel's shared NMI entry for the main CPU.
Sony and DaedalusX64 put their ME code at 0xBFC00040 and leave that entry alone (difference 1 below).

## Sources, and what I could not reach

- uofw: uOFW (Sony's kernel, reverse-engineered), master 28306bb, https://github.com/uofw/uofw
- uofw-power: uOFW branch "power", f7ed03a, https://github.com/uofw/uofw/tree/power (master has no power module).
- mc-core: mcidclan psp-media-engine-custom-core 0197c99, https://github.com/mcidclan/psp-media-engine-custom-core
- mc-feed: mcidclan me-feed-graphics-engine ef3f3cd, https://github.com/mcidclan/me-feed-graphics-engine
- ps2rc: PS2RC 710a6c1, https://github.com/SandstoneDev/PS2RC . Its hle/psp/hle_me.c is the base of our psp/me/hle_me_mgs2.c.
- daedalus: DaedalusX64 580c0c4, https://github.com/DaedalusX64/daedalus , Source/SysPSP/PRX/MediaEngine. Its stub says "this stub is from the melib package, (c)mrbrown" (me_stub.S:1).
- ark4: ARK-4 7c03c37, https://github.com/PSP-Archive/ARK-4
- HWR: https://www.psdevwiki.com/psp/Hardware_Registers . Also read: https://www.psdevwiki.com/psp/Media_Engine and https://www.psdevwiki.com/psp/Syscon (nothing there on watchdogs or power-off).
- Our runs: prof/hw_v30me5/run2/log.txt and psp_harness.md.
- Could not reach:
  - psdevwiki from the shell (HTTP 403 from the proxy); the three pages above came through the web-fetch tool. https://www.psdevwiki.com/psp/Battery failed there too.
  - https://forums.ps2dev.org/viewtopic.php?t=3523 and ?t=5006 (the fetch failed).
  - https://github.com/mcidclan/psp-media-engine-libs (the clone asked for a login, so it is private or gone). crazyc's original MediaEngine.prx: I found only DaedalusX64's copy.
  - The syscon's watchdog-expiry code: uOFW has not reversed it (see Q2).

## Q1. What Sony's kernel does with the ME after boot

Once, when me_wrapper loads:
- module_start (uofw src/kd/me_wrapper/me_wrapper.c:218-226) does five things.
  - It sets up the request channel: interrupt 31, an event flag and a mutex (:53-60).
  - It boots Sony's ME image with sceMeBootStart(2) (:222).
  - It stores the PLL frequency in 0xBFC00710 (:223).
  - It registers the sysevent handler "SceMeRpc" with mask 0x01ffff00 (:216, :225). uOFW's comment (:224): "This is what makes mediaengine.prx hang on standby/poweroff".
  - It turns AVC power off (:226).
- The boot (sub_00001C30, :1090-1118): ME reset on, ME bus clock on; the boot code is copied to **0xBFC00040** (:1095), not to 0xBFC00000.
  Then VME reset off and ME reset off; it waits until the ME changes 0xBFC00700, then turns AVC reset off.
- Sony's ME boot code is uofw src/kd/me_wrapper/init.S.
  - It first reads 0xBC100000 ("NMI enable mask", HWR). If that is not zero, it jumps to the NMI handler (init.S:19-26).
  - Otherwise it writes 0xBC100040 = the main CPU's value & 3, 0xBC100050 = 7 and 0xBC100004 = -1 (init.S:28-37).
  - It clears both caches with Index Store Tag, so nothing is written back (init.S:39-55).
  - It sets Cause = 0 and Status = 0x20000000 (init.S:57-59).
  - It resets the VME (0xBCC00010 = 1, then waits), then writes 0xBCC00070 = 1, 0xBCC00030 = 8 and 0xBCC00040 = 2 (init.S:60-74).
  - It sets sp = 0x88400000 (init.S:76).

After that:
- Nothing polls the ME. me_wrapper.c has no thread, alarm or timer; its only delay is in the boot wait (:1111).
- Sony's kernel talks to the ME in only two cases.
- **Codec requests** (for example uofw src/kd/avcodec/audiocodec.c:33) go through sceMeCore_driver_FA398D71 (me_wrapper.c:1190-1206).
  - It locks the mutex, fills 0xBFC00600 onwards and flushes the DDR.
  - It posts an interrupt to the ME with sceSysregInterruptToOther, then waits on the event flag with no timeout.
  - The ME's reply interrupt sets the flag (:39-41).
- **Sysevents to SceMeRpc** (:186-214):
  - 0x402 (suspend query): when 0x700 is 0, it only tries the mutex (:63-77).
  - 0x4005 (suspend phase 0): it sends request 399 ("save eDRAM, halt") and an interrupt. Then it runs `while (*hwAddress != -2);` with no timeout, then sets ME reset on and the ME bus clock off (:87-111).
  - 0x10005, 0x1000f and 0x100000 (resume): it re-boots the ME, spins until 0x700 is -4 with no timeout, and sets AVC power (:79-85, :172-184, :113-119).
  - 0x1000002 (PLL change start): it takes the mutex and writes the new frequency to 0x710. It sends request 7 only when (freq>>1) >= 133 with 0x714 == 1, or (freq>>1) >= 148 with 0x714 == 3 (:143-170).
  - 0x1000020 (PLL change end): it always sends request 391, waits with no timeout, then frees the mutex (:122-141).
- The power module sends the PLL events only when the PLL output changes (uofw-power src/kd/power/clockFrequency.c:514-632).
- Every standby, suspend or reboot request waits until no power lock is held. The source says: "We only proceed with processing the requested power switch when there is no power lock placed in the system" (uofw-power src/kd/power/powerSwitch.c:730-738).
- Sony changes the reset register 0xBC10004C by read-modify-write with interrupts suspended (uofw src/kd/lowio/sysreg_driver.c:9-11).
- I found no ME call in the power module's battery code (uofw-power src/kd/power/battery.c) [I: read, not proven].
- ARK-4 does not touch the ME at run time.
  - Its only sysevent handler acts on 0x400000 ("resume complete") and only switches LEDs (ark4 core/compat/psp/syspatch.c:340-364).
  - Its exception handler is built only with DEBUG (core/systemctrl/Makefile:71-76).
  - 0xBFC00000 and 0xBC10004C appear only in its IPL SDK and 1.50-kernel extras [I: grep].

## Q2. Can a system thread wait forever on our ME? Could that switch the PSP off?

- Yes, a thread can wait forever. Our ME never answers Sony's requests. Every codec request, the PLL-end handler, and the suspend and resume spins wait with no timeout (Q1).
- A hung thread on its own does not switch the PSP off in any code I read [I].
- The one route I found from a blocked ME wait to the syscon is a blocked suspend.
  - The suspend loop waits 10 ms per turn. After `watchdog` seconds blocked, it calls sceSysconCtrlTachyonWDT(0x5) and carries on (uofw-power powerSwitch.c:1000-1090; the call is at :1086).
  - `watchdog` comes from the Baryon data, & 0x7F (uofw-power src/kd/power/power.c:266).
  - The syscon driver sends that value with bit 7 set (uofw src/syscon/syscon.c:1507).
  - In uOFW's syscon firmware this starts a counter at twice the value (src/syscon_firmware/firmware_ta096.c:1515-1530).
  - What happens when the counter runs out is unknown. The timer interrupt bodies are empty in uOFW (:570-588), and so is allegrex_handshake (:1943).
  - That firmware is for the TA-096 board (its file name). I found no PSP-3000 syscon firmware.
  - Nothing else in uOFW calls sceSysconCtrlTachyonWDT [I: grep of src/kd and the power branch]. So I found no periodic watchdog on either CPU.
- In our runs that route needs a suspend, and no suspend can start.
  - hle_me_start takes scePowerLock(0) at the first takeover and keeps it (psp/me/hle_me_mgs2.c:307-308).
  - The power module waits for every lock (above).
  - Every battery line of v30me5 runs 1 and 2 shows "suspend required 0" and "low 0" (psp_harness.md:364-365).
- PLL events: the clock was set to 333 MHz before the takeover (prof/hw_v30me5/run2/log.txt:14).
  - Sony's words were 0x600 = 0x187 (391), 0x608 = 0x1ff, 0x700 = 0, 0x710 = 0x14d (333) and 0x714 = 2 (log.txt:229).
  - They were the same "at every wake and in every battery line" (psp_harness.md:364-365).
  - A PLL start writes 0x710 (me_wrapper.c:148), so no PLL change came [I].
- Requests: in v30me5 the ME's interrupt flags were only ever 0x80000000.
  - The rests ended by an interrupt match our kicks: run 1 had 3,341 + 1 = 3,342 kicks, and run 2 had 76,526 + 7 = 76,533 (psp_harness.md:364-365).
  - A Sony request posts the same interrupt. 0xBC100044: "Writing 1 to bit 0 posts an interrupt to the opposite CPU" (HWR). So no Sony request reached our ME while the logs ran [I].
- The gap: the battery lines come every 5 s, so the last few seconds before each stop are not covered.

## Q3. What other ME projects do that we don't

| step | Sony (uofw) | DaedalusX64 | mcidclan mc-core | PS2RC | ours |
|---|---|---|---|---|---|
| code copied to | 0xBFC00040 (me_wrapper.c:1095) | 0xBFC00040 (main.c:133) | 0xBFC00000 (me-core-custom.c:251, hw-registers.h:21) | 0xBFC00000 (hle_me.c:13, :147) | 0xBFC00000 (hle_me_mgs2.c:28, :190-197) |
| ME reset | sceSysreg*, interrupts off (sysreg_driver.c:9-11) | sceSysreg* (main.c:137-139) | 0x14 then 0: ME and VME (me-core-custom.c:256-257) | read-modify-write, no guard | same (hle_me_mgs2.c:193-203) |
| VME | reset and set up each boot (init.S:60-74) | not touched | reset with the ME, set up (me-lib.h:91-103) | not touched | not touched |
| 0xBC100050 from the ME | 7 (init.S:34-35) | 7 (me_stub.S:20-21) | 0x0f (me-core.h:33) | 6 | 6 (hle_me_mgs2.c:60-61) |
| caches at start | Index Store Tag (init.S:39-55) | Index Store Tag (me_stub.S:24-35) | not checked | writeback-invalidate | same (hle_me_mgs2.c:106-127) |
| Status | 0x20000000 (init.S:59) | 0x20000000 (me_stub.S:46-47) | 0x30000000 | 0x30000000 | 0x30000000 (hle_me_mgs2.c:71-72) |
| SceMeRpc | Sony's own | left in place | handler swapped (kernel/main.c:41-55) | swapped by its PRX (psp_exc/exc_me.c:232-272) | left in place: no PRX (psp/me_mix.c:35) |
| sleep | save, halt, re-boot | none | parks / resumes (me-core-custom.h:47-55, :141-149) | parks within 20 ms (exc_me.c:82-85), cold restart | no sleep: power lock (hle_me_mgs2.c:308) |
| idle | waits for requests (interrupts) | spins | spins | spins | spins (v30), HALT until a kick (v30me5, psp/me_rest.S:59-82) |

- mc-feed writes 6 to 0xBC100050 ("enable clocks: AW bus RegA & RegB (bits 0-2)"), resets the ME with absolute writes and has no power handling (main.cpp:62-92).
- Daedalus and mc-core unlock memory protection (0xBC000000-0x2C; daedalus me_stub.S:38-44). mc-core sets the gears 0xBC200000/04/08 to 511<<16|511 (me-core.h:23-25). Sony's boot code does neither.
- mcidclan's README says its core is "patching sceMeRpc on the main CPU side" and "manages suspend events".

## Q4. Known causes of a sudden PSP power-off with no crash screen

- **A crash.** On this PSP, with the first catcher, a main-CPU exception switched the console off (psp_harness.md:265-266). tt_port lessons A10 says the same (cited at psp_harness.md:260).
  - Catcher 2 catches such an exception and the PSP stays on (psp_harness.md:280).
  - Every later stop left crash.txt with no record, so those stops were not exceptions that reached the catcher [I].
- **A blocked or failed sleep.**
  - PS2RC docs/PORTING_GUIDE.md:356-357: "putting the PSP to sleep while the ME runs your code hangs or powers off the console unless the sleep handler is replaced".
  - GBAdhoc issue 12 (a PSP-3001 on ARK-4): sleep in a game ended in a power-off and a cold boot. The ME was not involved; a power lock and stopping the work fixed it. https://github.com/ShoshinFauteux/GBAdhoc/issues/12
  - The kernel arms the syscon watchdog when a suspend is blocked (Q2).
- **The battery.**
  - An iFixit answer says "Failing batteries can give unusual symptoms" and advises starting "with a known good battery": https://ru.ifixit.com/Answers/View/479143/PSP+won't+turn+on+without+AC
  - A PSP Go that shut down, with no cause found: https://github.com/GovanifY/procfw/issues/406
  - Against the battery alone in our runs:
    - v30me5 run 1 stopped at 100% on the cheap battery, with "low 0" (psp_harness.md:364).
    - v30 run 2 stopped while charging over USB at 100% (psp_harness.md:335).
    - ME-off runs stop 1 time in 27 (psp_harness.md:365) [I].
- **Syscon-level cuts** (brown-out, over-current, a watchdog on the main CPU): I found no reachable source for the PSP-3000.

## Differences between ours and theirs, ranked

How to read a two-run test: ME-on runs stopped 5 times in about 5.3 hours, about 0.95 per hour (psp_harness.md:365).
With no real change, two 40-minute runs would both finish by luck about 28% of the time (e^-1.27) [I].
So one stop in a test pair rules a change out as the whole cause, while two clean runs are only a hint.
The logged readings proposed below say more per run than the stop count.

**1. Our stub overwrites the main CPU's NMI entry at 0xBFC00000.** (Most likely to matter.)
- The difference: Sony copies its ME boot code to 0xBFC00040 (me_wrapper.c:1095), and so does DaedalusX64 (main.c:133). We copy to 0xBFC00000 (hle_me_mgs2.c:28, :190-197), as PS2RC and mc-core do.
- What lives there: at boot the kernel copies a shared entry to 0xBFC00000-0x3F (uofw src/kd/exceptionman/exceptions.c:53-74; code at excep.S:102-124). The code is `ctc0 $v0,$6; mfc0 $v0,$22; bne $zr,$v0,<+0x40>; nop; cfc0 $v0,$9; jr $v0`.
  - The main CPU (id 0) jumps to its NMI handler.
  - The ME (id 1) branches to 0xBFC00040, where Sony and Daedalus put their code [I].
- NMIs enter at 0xBFC00000. uOFW's pre-IPL there says "in case interrupts are enabled, it means we're not at boot time but recovering from an interrupt" and jumps to the NMI handler (src/preipl/loader.S:7-14, :43-48).
- After our takeover, an NMI on the main CPU would run our ME stub on the main CPU [I].
  - The stub writes the main CPU's 0xBC100050 = 6 ("Bus clock enable", HWR) and sets Status and the exception vector.
  - It then jumps into the ME loop (hle_me_mgs2.c:55-137).
  - The PSP would die with no exception and no record [I]. That fits every stop.
- Against:
  - I found no source that says the main CPU takes NMIs in normal play. No uOFW module registers an NMI handler [I: grep].
  - But the DMA manager does clear bits 5 and 6 of 0xBC100000 at run time, including in its interrupt handler (uofw src/kd/dmacman/dmacman.c:120, :128, :757, :762).
  - PS2RC and mcidclan also write 0xBFC00000, and I found no power-off reports from them.
  - v26me copied the stub and held the lock with the ME parked, and ran to the end, but that is one run (psp_harness.md:247) [I].
- Test (code change, two 40-minute ME-on runs on the cheap battery):
  - Before the copy, log the 16 words at 0xBFC00000-0x3F and check them against excep.S:102-124. Only if they match, copy the stub to 0xBFC00040 and leave 0x00-0x3F alone.
  - Log the main CPU's 0xBC100000 / 0xBC100004 in every battery line. A nonzero mask would be direct support.
  - The 0x40 start is safe to try: a dead start leaves the ME parked and the run goes on, as in v26me (psp_harness.md:247).

**2. Sony's ME driver still thinks its ME is alive.**
- The difference: we stop Sony's running ME. The reset register read 0x40 at our start, so the ME was out of reset (log.txt:230).
  - We leave me_wrapper's SceMeRpc handler and its request functions in place (no sleep PRX in this port: psp/me_mix.c:35).
  - PS2RC (psp_exc/exc_me.c:232-272) and mc-core (kernel/main.c:41-55) replace that handler.
- Why it might matter: any request or PLL change then waits forever (Q1), and a blocked suspend arms the syscon watchdog (Q2). PS2RC's guide warns of power-offs (Q4).
- Against: our lock stops every suspend, and no PLL change or Sony request reached the ME while the logs ran (Q2). Only the last seconds are open, and I know of nothing in our run that would send a request then [I].
- Test (code change, two runs): swap the SceMeRpc handler as PS2RC does (find it by name in sceKernelReferSysEventHandler's list) and keep the lock. Count each sysevent id in a word that the battery line prints. An event would be direct evidence.

**3. The VME stays as Sony's firmware left it.**
- The difference: reset register 0x40 means only bit 6 (USB) is held. HWR "Reset enable" gives bit 4 = VME and bit 5 = AVC, so both were running.
  - Sony resets the VME at every ME boot (init.S:60-74). HWR for 0xBCC00010: "VME reset: writing anything here causes the VME to reset".
  - mc-core resets the ME and VME together (0x14; me-core-custom.c:256-257), then sets the VME up from the ME (me-lib.h:91-103; HWR: "can only be accessed from the ME CPU").
  - We reset only the ME (bit 2; hle_me_mgs2.c:29, :193-203) and never touch 0xBCC000xx.
- Why it might matter: once Sony's ME firmware is gone, nothing manages the VME [I]. I found no source on what that does to power or the bus.
- Test (code change, two runs): hold bit 4 along with bit 2 and release both, as mc-core does, or add Sony's four VME writes to the stub. Log 0xBC10004C before and after.

**4. Only ME-on runs hold the power lock and the callback thread.**
- This is not a difference from PS2RC (hle_me.c:224 takes the same lock). It is a difference between our ME-on and ME-off runs.
  - scePowerLock(0) is taken at hle_me_mgs2.c:308.
  - The "ps2rc_me_pwr" thread runs at priority 0x11 (hle_me_mgs2.c:420; started from psp/me_mix.c:167).
- For: "the ME's load is now a weaker suspect than the ME being started at all" (psp_harness.md:365), and the lock comes with every start.
- Against: no suspend request was seen (Q2), and v26me held the lock and the callback and ran to the end (one run, psp_harness.md:247).
- Test (cfg switch, two runs): take the lock and the callback but do not start the ME. A stop would point at the lock.

**5. 0xBC100050 written from the ME: 6, where Sony and Daedalus write 7.**
- Sony 7 (init.S:34-35), Daedalus 7 (me_stub.S:20-21), mc-core 0x0f (me-core.h:33), ours 6 (hle_me_mgs2.c:60-61), mc-feed 6.
- HWR: "Bus clock enable", bit 0 "ME", bit 1 "AW RegA Bus". I found nothing on what bit 0 means when the ME writes it.
- The ME has its own copy: at reset it read 0x40 / 0x50 = 1 / 0, while the main CPU read 0x82000002 at 0x40 (log.txt:228) [I].
- Test (one-line change, two runs): write 7. I would run tests 1-3 first.

**6. The reset register is changed with no interrupt guard.**
- k_start reads 0xBC10004C once. It then copies, flushes both caches, and writes the old value back with bit 2 cleared (hle_me_mgs2.c:193-203).
- Sony suspends interrupts around the same change (sysreg_driver.c:9-11), and Daedalus uses Sony's functions (main.c:137-139).
- A change another driver makes in between would be undone [I]. Against: this happens once, at start, while the stops came 1.5 to 39 minutes later.
- Test: none needed. Suspend interrupts around it, or call sceSysregMeResetEnable/Disable. No runs.

**7. The ME's load on the bus.**
- Sony's ME works on request (me_wrapper.c:1201-1206). Our spinning ME polls the ring with uncached reads (psp/me_mix_me.c:112), 13,366 idle polls a second (log.txt:229).
- Resting cut the ME's activity by about 95% but not the stops. Resting had 1 stop in 2 runs, spinning 4 in 8, and ME off 1 in 27 (psp_harness.md:365).
- Already tested; no new test.

**8. Small differences that should not matter.**
- Status 0x30000000 against Sony's 0x20000000: CU0 changes nothing in kernel mode [I].
- Cache start: Sony throws the ME's caches away (init.S:39-55). Our warm start writes back the ME's dirty lines first (hle_me_mgs2.c:121-127); at the first takeover those lines are Sony's firmware's own [I].
- Memory-protection unlock and ME gears: Sony's boot code does neither.
- The ME interrupt-controller writes (psp/me_rest.S:37-43) exist only in resting builds, and spinning builds stopped too.
- No test.
