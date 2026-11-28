# mgs2psp
MGS2 psp port

Our own work on a personal PSP port of Metal Gear Solid 2 (PAL disc SLES-50383) for a PSP-3000 on ARK-4: a static recompile of the
game's code plus a new renderer for the PSP's graphics chip. Personal use only.

## What is here
- `mgs2/`: a copy of the project folder's `mgs2/` (recompiler, runtime, renderer, PSP harness, tools, scripts, notes, plans, build
  reports and PSP run logs). `mgs2/main/` is the current build tree; the top-level `boot/`, `ge/`, `psp/` and `runtime/` folders are
  older versions kept for history. Start with `mgs2/NOTES.md`, `mgs2/roadmap.md` and `mgs2/psp_harness.md`.
- `docs/`: notes that live outside the project folder's `mgs2/`: the method we work by, a filtered backup of the project's memory
  notes, the upstream check and the earlier port's hand-over notes. See `docs/README.md`.
- `sync_from_project.py`: copies the project folder into `mgs2/` and the notes into `docs/` again, with the rules below.

## Map of `mgs2/`
The folder mirrors the project folder one to one, so paths in the notes stay valid; it is kept organized by these groups.

**Start here**
- `NOTES.md`: charter, where we stand, decisions. `roadmap.md`: now, next, later (`roadmap_history.md`: the old ones).
- `plan_deck_25fps.md`: the lead plan for the Tanker deck. `plan_20fps_2026-11-22.md`: what 20 fps in the rain costs.
- `psp_harness.md`: one `## Build <name>` entry per build: what changed, the checks, the PSP run's results.
- `DEVIATIONS.md`: every difference from the original game a player could see or feel, with its reason.

**Code**
- `main/`: the current tree: `recomp/` (the recompiler), `rt/` and `runtime/` (the runtime), `ge/` (the PSP renderer), `psp/` (the
  PSP harness), `boot/`, `cfg/`, `scripts/`, `tools/`, `build_all.sh`; `main/attic/glNN/`: each build's plan, report and log reader.
- `recomp/`, `tools/`, `play/`: the recompiler's studies and widths, our tools, the scripted pad runs.
- `boot/`, `ge/`, `psp/`, `runtime/` at the top: older versions, kept for history.

**Measurements**
- `prof/`: PSP run logs (`hw_<build>/`), emulator runs (`emu_<build>/`), state hashes and profiles.
- `deck_costs.md`, `speed_notes.md`, `outlook_25fps.md`, `codec_speed.md`, `pipeline_anatomy_2026-11-27.md` (the game's own draw
  preparation on the main CPU, sized for the next cuts).

**Renderer and Media Engine**
- `renderer_notes.md`, `native_renderer_design.md`, `native_draw_study.md`, `native_step0_report.md`, `native_step1_report.md`,
  `native_port_answer.md`; `me_*.md`: the Media Engine's probes, steps and designs.

**The game, in our own words**
- `stage_notes.md`, `stagec_notes.md`, `elf_notes.md`, `decomp_names_sles.md`, `rain_study.md`, `rain_options_2026-11-21.md`,
  `sound_notes.md`, `logic_timing.md`, `halfrate_study.md`, `h1_notes.md`, `b_proof_notes.md`.

**Studies, reviews and audits**
- Other ports and tools: `android_port_study.md`, `peacewalker_notes.md`, `psp_ports_survey.md`, `sandstone_review.md`,
  `vme2_study.md`, `vita_port_study.md`, `ark4_options.md`, `ark5_study.md`.
- Research: `feasibility.md`, `research_review.md`, `research2_port_plan.md`, `web_research.md`, `web_research_2.md`.
- Audits and logs: `audit_2026-10-23.md`, `audit_v30.md`, `overnight_2026-11-22.md`, `memory_status_archive_2026-11-22.md`,
  `freeze_study.md` (the random freeze-then-off stops: evidence, ranked causes, tests).

## Latest builds
| Build | What it was | PSP result |
|---|---|---|
| gl44 | measurement: the rain's hidden drop and streak work left out exactly, in turns (nothing changes on screen); the rain cut of gl43 as the baseline; the harness now keeps the logs of the two runs before and writes a one-line-a-second black box for the freezes | delivered 2026-11-28, not run yet; on the emulator exact (62 state checks, 61 picture checks); expected about 1 ms a rain step from the new cut (model 1.3) |
| gl43 | measurement: the rain's game-logic cut in turns (only the rain lines nearest the camera kept; the guards' dice kept exactly) | exact (62 state checks, 61 picture checks; first run on the AC adapter, no stop); the cut saves 4.8 to 6.0 ms a rain step; rain wait 69.7 ms a step (7.2 fps) with the cut and the native ship and people; of the main CPU's 69.3 ms, 38.4 is logic and 24.7 the game's own draw preparation, the next target; `prof/hw_gl43/gl43_psp_read.md` |
| gl42in | measurement: gl42's turns moved indoors (the crew deck after the door) | exact (run 2; run 1 froze after the deck door and its log was lost); indoors 52.4 ms a step (9.6 fps), 47.9 with the native ship and people (10.4 fps), 36.6 for the main CPU alone (13.7 fps); `prof/hw_gl42in/gl42in_psp_read.md` |
| gl42 | measurement: the people drawn by the new native renderer too, the renderer checkpoint turns (ship and people only, nothing drawn) | exact (run 2; run 1 lost power at 27 min); rain wait 75.9 ms a step (6.6 fps); native ship and people save the Media Engine ~6 ms but the main CPU ~1: the rain is main-CPU (game logic) bound, so the renderer is not the route through the rain (checkpoint P ~67 >= 62); `prof/hw_gl42/gl42_psp_read.md` |
| gl41 | measurement: the ship drawn by the new native renderer in turns, two small main-CPU levers, memory fix | exact; rain wait 74.8 ms a step (6.7 fps); the native ship takes 2.7 ms off the Media Engine but the rain is main-CPU limited; `prof/hw_gl41/gl41_psp_read.md` |
| gl40 | measurement: three ways of making the copy plan in turns, the kernel walk lever, plan timers | exact (run 2; run 1 lost power at 26 min); rain wait 76.4 ms a step (6.5 fps) with the new plan, 2.0 faster than gl39's; the rain is now main-CPU limited; gl39's unexplained ~8 ms was mostly the copy plan; `prof/hw_gl40/gl40_psp_read.md` |
| gl39 | measurement: frame check off in some turns, the Media Engine loaded on its own memory, timers for the queue | exact; rain wait 78.1 ms a step (6.4 fps) as a game build runs it, main-CPU limited; ~8 ms of drawing-time work on the main CPU still unexplained; `prof/hw_gl39/gl39_psp_read.md` |
| gl38 | measurement: one kind of drawing left out per turn, new timers | exact; rain wait 85.0 ms a step, of it 13.6 the test's own picture check; costs per kind in `prof/hw_gl38/gl38_psp_read.md` |
| gl36 | cull of hidden effect calls, re-use of the ship's pieces | exact; rain wait 85.1 ms a step (5.9 fps), 3.4 faster than gl35 |
| gl35 | the fast native path for the ship's pieces in every turn, per-piece keeps in turns | exact; rain wait 88.6 ms a step |

Every build, older ones too: `mgs2/psp_harness.md`. A step is 1/50 s; the game shows a picture every 2 steps, so fps = 500 / (ms a step).

## What is not here, on purpose
Nothing taken from the game is in this repository: no disc or HD files, no game binaries or modules, no code generated from the
game's code (or written by hand from it), no pictures, and no built programs. `.gitignore` and `sync_from_project.py` hold the full list.
So the tree does not build on its own: a build needs the owner's disc and the generated code, made in the project's container.

The builds also use parts of other projects, kept in the project folder under their own licences: PS2RC by SandstoneDev
(https://github.com/SandstoneDev/PS2RC; sound, movie and Media Engine bring-up code) and mcidclan's Media Engine samples (MIT).

## Updating
```
python3 -I sync_from_project.py            # copies /mnt/project-files/mgs2/ into mgs2/ and the notes into docs/, skipping the files listed above
git add -A && git status                    # look before committing
```
