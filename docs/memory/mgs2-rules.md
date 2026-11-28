---
name: mgs2-rules
description: Ffm's standing working rules for the MGS2 port (full lead since 2026-11-18, hardware safety, data handling, build cadence, method), with dates; older consents in mgs2-consents
metadata:
  type: feedback
  modified: 2026-11-22T17:36:00.000Z
---

- FULL LEAD (Ffm 2026-11-18 20:10-20:19): "full freedom ... your the leader, just complete the deck runing at 25/20 fps ... downscale what is needed for a psp port"; 20:19 "the only one you cant break is the writing in the psp flash! WE CANT BRICK THE PSP". Cuts, downgrades, rule changes and build choices are now mine; still logged in DEVIATIONS.md and told to Ffm. Older consent limits in [[mgs2-consents]] no longer bind.
- ALL DECISIONS MINE (Ffm 2026-11-23 15:57: "you are the boss, i dont take decisions, im only and extension of your work for whaterver you need"): no cards or yes/no asks for project choices; decide, act, tell Ffm what and why. Still tell Ffm before every PSP run and ask for things only Ffm can do (PSP, PC, access).
- My own safety lines (told Ffm 20:20): never write flash0/NAND; never above 333 MHz; no power/clock register writes. Never auto-exit on hardware.
- Method I keep (it is how we prove a build): state hashes identical on x86, emulator and PSP for the same cut; one change per measurement; only real-PSP numbers count; [C]/[C model]/[I] marks; UNKNOWN when not known.
- BUILD CADENCE: one build out at a time; a new PSP build only after the previous run's log is read (v13 shipped before v12's log and kept its bug). Builds need no OK.
- PSP STICK (Ffm 2026-11-19 11:31 "always delet the old builds from the psp"): once a run's logs are fetched to LOGS\<ver> and md5-checked, remove that build from ms0:/PSP/GAME (psprun.py remove MGS2T_<ver> <logs dir>, refuses unless every log has a matching copy). Only MGS2T_* folders; FF8T is the FF8 thread's. Tell Ffm before every launch started from the PC. [[psp-link]]
- Test runs on the OSTENT or the charger; never ask which pack. Ffm doesn't babysit PPSSPP runs; "any access you need, just ask".
- NO RUSH (2026-10-21): perfectionists; never hand over a build early. Always suggest the best option with a recommendation.
- EFFORT (2026-11-10): medium, high or xhigh to fit the task; max only when 100% needed. FABLE 5.2 (claude-fable-5-2) STANDING (Ffm 2026-11-20 16:21 "everytime you need fable 5.2 brig it"): use it when a task needs it, no ask; never above high effort without asking (Ffm 2026-11-11). It has a 5-hour usage limit: a stopped agent resumes by SendMessage after the reset. It also has a longer limit: hit 2026-11-23 ~14:10 UTC, resets 2026-11-27 04:00 UTC; until then agents run on the default model (a stopped Fable agent can't switch model: start a new one from its state.md).
- RUN LENGTH (lead decision 2026-11-19, Ffm asked to cut the ~50 min wait): short runs by default (game logic only until the deck's loading screen ~vb 23,200, stop after the deck ~vb 31,400, ~25-30 min [I]); a full-route run every ~3rd build or when game code outside the deck changes.
- LOG FOLDERS (Ffm 2026-11-19 02:03): every delivered build gets its empty LOGS\<version> folder on the PC.
- WORKFLOW: ultracode was on 2026-11-21 23:06 and is off now (2026-11-22): run a Workflow only on Ffm's opt-in.
- CARDS (2026-11-11): explain every option in a message before the card (what, change, cost, risk, why recommended).
- CONTAINER HOUSEKEEPING (my rules after past mistakes): kill only by PID or TaskStop; rm only with literal absolute paths, look first; nothing in Ffm's PC folder deleted; never edit a script or rebuild a binary while a step of it runs; python3 -I always.
