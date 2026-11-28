---
name: psp-recomp-port-method
description: Method and standing rules for the owner's console-to-PSP recompiled ports (MGS2, FF8). Load at the start of any thread that plans, builds, measures, delivers or reports on a port build.
---

# PSP recompiled-port method

This is how the ports in this project are built and how the owner wants the work done. It was written on 2026-11-16 from project memory and the ports' notes. It is a snapshot. Rules change, so:

- **The owner's latest word wins, then project memory and the port's NOTES.md, then this skill.** If they differ from this page, follow them and tell the owner in one line that the skill needs an update.
- This page gives the method and the reasons. Figures, addresses, build names and the state of each port live in memory and the notes. Never take a status from here.

## 1. First ten minutes of a thread

1. Read the memory index, then the topic files it links for your port and task.
2. Read the port's top page: `mgs2/NOTES.md` and `mgs2/roadmap.md`, or `ff8/feasibility.md` plus the newest stage or build notes.
3. Check whether the port is stopped. A stop by the owner holds until the owner lifts it in their own words. Start nothing on a stopped port. Memory says which port each stop covers.
4. Find the build that is out and whether its log has been read (section 5).
5. Choose the effort level that fits the task. The owner's rule: medium, high or xhigh as the task needs; the top level only when it is truly needed. The exact wording is in memory.
6. Stay in your lane: one port's thread never edits another port's files, builds or threads. Read and copy only. Findings from side studies go to the port's own thread through the coordinator.

## 2. The method, stage by stage

The game's code is translated once, ahead of time, into C or assembly for the PSP (a static recompile). Everything the console's hardware did is replaced by our own code. Each stage ends with a proof, not an impression.

1. **Reference on the PC.** An exact model of the console running the owner's own disc, deterministic from power-on, with a scripted pad file. It prints a state hash at fixed intervals. This is the truth every later stage is compared with.
2. **Recompiler.** The translated game must give the reference's hashes at every checkpoint, with no interpreted leftovers on the tested route.
3. **PSP harness.** The same code on the PSP: logs, hashes, timing per window, a config file with switches, no pacing tricks. First goal: exact on the real PSP, however slow.
4. **Renderer on the PSP's graphics chip (GE).** The picture is rebuilt for the GE. Logic stays exact; the picture is judged against reference shots under the margin the owner set for that port.
5. **Speed.** One lever at a time, each exact, each measured on the PSP (section 4).
6. **Upgrades and the whole game.** Visual upgrades, each behind a switch, each shown with shots and OKed by the owner first. Then coverage of the full game.

Why this order: speed work on code that is not yet proven exact produces fast wrong builds, and nobody can tell which change broke it.

## 3. Exact means proven

- **The hash test is the gate.** State hashes must be identical on the PC build, the emulator and the PSP for the same pad script. A change to generated code that moves a hash is a bug until proven otherwise.
- **One change per measurement.** Two changes in one build give one number and no way to split it.
- **Every change sits behind its own switch** and is compared with its own twin (the same build with the switch off).
- **Game logic stays the original's.** No half-rate logic, no thinning of game-side actors, no timing shortcuts, unless the owner chose it on a card. Where the owner narrowed or widened a rule, the card's date and words are in memory. Use those, not a paraphrase.
- **Pictures:** each port has its own rule for how far a picture may differ. Look it up; do not assume pixel exact or free.
- A proof that failed is reported as failed, with what differed and where. Then the choice goes back to the owner.

## 4. Speed: only the PSP counts

- Only a real PSP run's log settles a speed. The emulator and the cost model choose and check work; they never settle it.
- Mark every figure: **[C]** measured or read in code, **[C model]** from the cost model, **[I]** inferred, with its basis named. Where there is no basis write **UNKNOWN** and say how we will find out. An [I] or [C model] figure is never reported as reached.
- Each port has a fitted cost model (instructions, instruction-cache misses, data-cache misses). Use the port's own fitted numbers from memory. After each PSP run, compare model and run and note the gap.
- To time a change on the PSP, let the harness switch it on and off in turns inside one run. Two separate runs differ by more than most levers are worth.
- Single estimates have been wrong by 2x and more in both ports. Say so when you give one, and do not build a plan on an unmeasured gain.
- The usual wall on the PSP is the CPU's small instruction cache. Code size, function placement and profile-guided builds matter more than clever arithmetic. Check where the time goes before choosing a lever.
- Know which clock a run used. The harness asks for 333 MHz and logs what the firmware reports; a custom firmware setting can override that silently. A timed fixed loop in the log proves it.

## 5. Builds and deliveries

- **One build out at a time. A new PSP build ships only after the previous run's log is read.** A build once shipped before the previous log was read and carried the same bug; the owner spent a run for nothing. Preparation may overlap a run; delivery may not.
- Builds need no OK from the owner when the checks pass and the build is worth a run. Deliver, then tell them. The consents in section 7 still come first.
- Before delivery: hashes equal the reference on the PC and the emulator, shots compared, heap headroom noted, a README written for that build (what changed, what to do, what the run should show, where the logs land).
- Delivery goes to the port's `Builds` folder on the owner's PC, one folder per version, and the logs come back in its `LOGS` folder under the same version. The exact paths and file list per port are in memory. After copying, check every file's md5 on the PC.
- Game binaries staged in the shared project folder for a copy are deleted right after. Nothing in the owner's PC folder is ever deleted without asking.
- **No rush.** Never hand over a build early because the owner is about to leave. The owner's words: no rush, be perfectionists.
- When a log comes back: read all of it, compare with the model, write the build's entry in the harness notes (build, hash result, ms a frame or step, fps, what is left), update memory, then report.

## 6. Hardware lines never crossed

- Never write flash0 or NAND. Never write a power, clock or other hardware register the owner has not OKed; the OKed writes are listed in memory.
- Never above 333 MHz. No overclock, not even as a test.
- Never exit automatically on the real PSP: the build waits on its end screen.
- Test runs use the charger or the good battery. A cheap battery pack caused a series of unexplained shutdowns. Do not ask the owner which pack they use.
- Kernel-mode code, new register writes and Media Engine work are the risky part of this hobby: each step is described to the owner before it ships.

## 7. What needs the owner's OK first

Ask before:

- any gameplay or visual change, cut or downgrade. Show shots and numbers first, with the best alternative;
- a new register write, a new kernel module, hardware performance counters;
- changing any rule of the charter;
- deleting anything of theirs, or passing game data to another thread.

No OK needed for: builds that pass their checks, exact speed work, notes, research, reading public sources.

When a choice is theirs, write a decision card. Their rule for cards: explain each option in plain words (what it is, what it changes, what it costs, the risk) and why one is recommended, and leave out nothing that changes the choice. The card's fields are short, so the full explanation goes in a message just before the card. Always recommend the best option we have; before dropping or degrading an effect, propose the best alternative.

## 8. Notes that must stay current

- **NOTES.md** (per port): charter, where we stand, decisions, mapped addresses, open questions.
- **DEVIATIONS.md**: every visual or gameplay difference from the original, with its impact, before it ships.
- **Harness notes**: one entry per build, as in section 5.
- **Roadmap**: now, next, later. The owner asked us not to get disorganized.
- **Project memory**: anything a new thread would have to ask the owner again, saved in the turn you learn it. Convert "today" to a date.

Shared files are edited by several threads: read a file again just before changing it, and read it back about ten seconds after writing.

## 9. Talking to the owner

- English, direct, plain words. No guesses. Real data, with the marks from section 4.
- Lead with the result or the question. They read on the go and skim.
- Times in their local time.
- They do not babysit emulator runs and do not want to be asked for things we can look up. They said: any access you need, just ask.
- They want ideas, including unusual ones, each with a recommendation and its cost.

## 10. Game data

- Everything derived from the games is for the owner's personal use only: never published, never uploaded, no game pictures in messages. Shots, dumps and disassembly stay in the container or on the owner's PC.
- Only the owner's own discs and BIOS are used.

## 11. Container and PC habits (each learned the hard way)

- The container is temporary. Anything worth keeping goes to the shared project folder or the owner's PC; game data in the container must be copied again in a new one.
- Stop processes only by their id. Delete only with literal full paths, after looking at the target. Never edit a script or rebuild a binary while a step that uses it runs.
- Free disk space before a profile-guided build; look before deleting; memory lists the trees that must never be deleted.
- A picture copied to the PC can gain a metadata chunk, so compare image data, not file hashes.
- Run Python that reads downloaded files in isolated mode, from a folder that holds no downloaded files.

## 12. Tools and references

- Toolchain: the pspdev tarball kept in the owner's `Sources` folder. Any toolchain or emulator switch means re-measuring the baseline, so it needs a reason and a word to the owner.
- Emulator: the release named in memory for everyday runs, and the patched development build named there as the exact reference, with its environment switches. Judge shots by the reference build. Its headless program has a debugger port, useful for questions that would otherwise cost a rebuild each.
- The emulator's free memory is not the PSP's; take memory figures from PSP logs only.
- `mgs2/third_party/psp-developer` is a copy of a public PSP skill with hardware facts and measured costs; copy it into the session's skills folder when needed. Other projects' numbers are leads, not our measurements.
- The owner has OKed installing a Ghidra headless skill when a task needs game code read that no decompile covers.
- The last check of upstream tools and public skills is in `upstream_check_2026-11-16.md`.

## 13. Before you report a step as done

- Built behind its switch?
- Hashes equal on the PC, the emulator and, if it ran, the PSP?
- Speed figure marked [C], [C model] or [I], with the PSP run named if [C]?
- Deviation logged, if anything the player sees or feels changed?
- Notes, roadmap and memory updated?
- The reply says what is left?
