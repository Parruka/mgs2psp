# Can MGS2 reach 25 fps on the PSP? (2026-10-24)

**Superseded for its numbers (2026-11-01):** this predates the picture on the PSP, whose CPU cost came out far larger than assumed here. Current estimates: `native_draw_study.md` §5 and `roadmap.md` "The road to 25 fps".

Ffm asked (2026-10-24 11:44): is 25 fps impossible, should we stop, or can hard work and time make it? And (11:45): "if a downgrade is needed ofc we do it".

Measured numbers come from Ffm's PSP-3000 unless marked. **[I]** marks an estimate. Sources: speed_notes §16, §46-§48; renderer_notes §11.6; psp_harness.md (v24, v24w, v26); sandstone_review §2.4.

## Short answer

- **25 fps everywhere: not realistic [I].** The rainy Tanker deck in free play, the heaviest place measured so far, needs about 5-6 times less CPU work per logic step than today.
- **25 fps in lighter areas: likely reachable [I].** The crew deck on our scripted walk needs about 13% less; the exact work in the plan gives ~~about 30%~~ about 22-28% [Edit 13:08, see below]. Free play there is still to be measured.
- **The rainy deck: about ~~15-19~~ 13-18.5 fps after all the logic work below, ~~17-21~~ 14.5-20 with lighter rain [I]**; ~~near 25~~ 17.5-25 only if the renderer's CPU work is also cut about in half [I]. Since the game has no time step, ~~15-19 fps also means the game runs at 60-77%~~ 13-18.5 fps also means the game runs at 52-74% of normal speed there.
- *Corrected 2026-10-24 13:08: the plan for the new code generator (recomp/lean_plan.md, speed_notes §50) sizes its parts one by one and adds them up to 6-10 ms per deck step [I], not about 11 ms; about 11 was the top of that range. Every row below that includes it moves by the difference, 1-5 ms per step: 1-3.5 fps lower at the low end of each deck range, 0.5-1 fps at the top; the crew deck keeps 25 fps on the scripted walk with less room.*
- *Corrected 2026-10-24 12:25: the first version of this file applied the new code generator's gain to all of the logic; it only acts on the generated code, 68% of the logic (§ "Where the deck's free-play logic goes"), and it counted lighter rain and the effects on the Media Engine twice. The deck figures fell by 1-2 fps; the crew deck's margin shrank.*
- **Recommendation: keep going**, with the target set per area. Nothing measured so far is a dead end. The exact build stays as the reference that every native rewrite and every downgrade is checked against.

## What 25 fps needs

- The game has no time step (checked in the code, speed_notes §16): gameplay must run 50 logic steps per second. We show every other step, so 50 steps per second gives 25 fps. When a frame takes longer than 40 ms, the game itself slows down: 8 fps today on the deck is a third of normal speed.
- Per shown frame: 2 x (logic + IOP and sound) + the renderer's CPU work must fit in 40 ms.
  - Deck: the renderer's CPU work is about 20-25 ms per frame [I] (renderer_notes §11.6), so logic + IOP must be about **7.5-10 ms per step**.
  - Crew deck: about 10 ms per frame [I], so logic + IOP must be about **15 ms per step**.
- The GE must also draw each frame in 40 ms. Deck frame (MGS2GE v5 / v6, measured): 38.9 ms with the CPU idle, 41.8-45.1 ms while the CPU works, so the deck is slightly over on the GE side too.

## Measured on the PSP: logic + IOP and sound per step

| Area | Build | Logic + IOP | Needed for 25 fps [I] | Gap |
|---|---|---|---|---|
| Crew deck, scripted walk | v24w | 17.2 ms | about 15 | 1.15x |
| Deck, Otacon's tutorial (scripted) | v24 | 36.3 ms | about 7.5-10 | 3.6-4.8x |
| Deck, free play | v26 | 50.2 ms | about 7.5-10 | 5-6.7x |

Not measured yet: free play indoors (next run), and anything in the Plant chapter, which is most of the game.

## Where the deck's free-play logic goes (emulator model, speed_notes §46)

- **Visual effects 23.2 of 48 ms:** rain, splashes and sea 10.9; lights and other visual work 7.9; ropes 4.4.
- **Gameplay 22.1 ms:** three guards 7.2, the player 6.4, doors 1.5, the rest 7.1. More guards cost more (about 2.4 ms per guard here).
- End-of-frame wait (IOP, sound) 2.7 ms.
- By cost: instructions 53%, code-cache misses 30%, data-cache misses 17%. The translated code is 6.5-7 times the size of the PS2 code; PS2RC, a PS2-to-PSP recompiler, reached 2.3 times on the PSP (sandstone_review §2.4), TT 3.6.
- By kind of PS2 instruction (measured on the emulator, speed_notes §49, `prof/guest_class_free_v26.txt`): the generated code is 32.7 ms (68%), the runtime 15.2 ms (copies, I/O registers, compiled VU0 programs, the native rain lines). In the generated code: calls and returns 7.8 ms (31 PSP instructions per call), VU0 vector work 6.4 ms (25 PSP instructions per VU0 operation, 8.6 per VU0 load or store), memory accesses 9.8 ms, branches 4.3 ms, arithmetic 3.3 ms.

## The levers

### Exact: no visible change, the state hashes stay identical

| Lever | Size per step [I] | Where |
|---|---|---|
| IOP and sound on the Media Engine (v26me is the first step) | up to 2.5 ms | everywhere |
| A new, leaner code generator: functions pass guest values in registers, no write-back of values nobody reads, masks and checks only where needed, VU0 work on the vector unit | ~~about a third of the generated code's time: about 11 ms of the deck's 48 (§49)~~ 6-10 ms of the deck's 48, a fifth to a third of the generated code's time (recomp/lean_plan.md) | everywhere; it also helps the whole game's code fit in memory (today w00a alone is 16.3 MB at full size; without the address check alone it is 33% smaller) |
| Exact native forms of the weather and the ropes (line_sph was done this way: -55%) | 5-8 ms | the deck |
| Skipping stateless render-only effects on the steps that are not shown | 2-4 ms | effect-heavy areas |

### Not bit-exact: each one shown to Ffm with numbers, pictures and the best alternative before it ships

| Route | Size per step [I] | What it costs in fidelity [I] | Work |
|---|---|---|---|
| Rain, splashes, sea, ropes and lights rewritten as native PSP code (vector unit; updated every other step, since only every other step is shown) | deck effects 23 -> about 3-6 ms | same look; particles not at bit-identical places; random numbers the effects share with other code may come out in a different order | large, one effect at a time |
| Guards and player rewritten as native code (watcher.c, raiden) | 13.5 -> about 4-6 ms on the deck; more where there are more guards | behaviour must match the exact build, checked step by step; float rounding differs in the last bit | very large (hundreds of functions) |
| Lighter weather: fewer drops and splashes, simpler sea | most of what is left of the effects | visible: less rain | small |
| The effects run on the Media Engine one step behind the game (instead of lighter weather: keeps the full rain) | their remaining 3-6 ms off the main CPU | effects 20 ms behind (not visible [I]) | large and uncertain: past PSP projects only moved loosely coupled jobs such as audio to the ME, and in a published PSP test 8 sound voices on the ME slowed the GE's texture reads from RAM by 10-20% (web_research.md); v26me shows how the bus behaves |
| Fewer frames shown on the deck (1 in 3 steps, 16.7 fps) | the renderer's cost spread over 3 steps | lower frame rate in exchange for game speed (at 17 ms per step: 71% speed at 17.7 fps, or 82% at 13.6 fps) | small |

## What it adds up to [I]

Deck in free play (renderer 20-25 ms per shown frame):

| After | Logic + IOP | Per shown frame | fps | Game speed |
|---|---|---|---|---|
| Today (v26, measured step) | 50.2 | 122-125 | 8.0-8.2 | 33% |
| All exact levers, with the new code generator | ~~29-33~~ 30-38 | ~~78-91~~ 80-101 | ~~11-13~~ 10-12.5 | ~~44-51%~~ 40-50% |
| + effects rewritten natively | ~~22-25~~ 23-30 | ~~64-75~~ 66-85 | ~~13-15.5~~ 12-15 | ~~53-62%~~ 47-61% |
| + guards and player rewritten natively | ~~16-21~~ 17-26 | ~~52-67~~ 54-77 | ~~15-19~~ 13-18.5 | ~~60-77%~~ 52-74% |
| + lighter rain, or the effects on the Media Engine | ~~14-17~~ 15-22 | ~~48-59~~ 50-69 | ~~17-21~~ 14.5-20 | ~~68-83%~~ 58-80% |
| + the renderer's CPU work cut to 10-13 ms per frame (native culls and matrices) | ~~14-17~~ 15-22 | ~~38-47~~ 40-57 | ~~21-26~~ 17.5-25 | ~~85-100%~~ 70-100% |

Crew deck: 17.2 measured -> ~~about 12 ms~~ about 12.3-13.5 ms with the ME and the new code generator -> ~~about 34 ms~~ about 35-37 ms per frame: 25 fps with a little room [I]. If free play there is a third heavier than the scripted walk (as on the deck, +32%): ~~about 16 ms per step, 42 ms per frame, just short of 25~~ about 16.4-18 ms per step, 43-46 ms per frame, about 22-23 fps; the other exact levers would have to close that [I].

## What would change this picture

- The renderer's real CPU cost on the PSP (every fps above assumes 20-25 ms on the deck, 10 on the crew deck). The first measured frame rate comes when the translator runs on the PSP (renderer task #30).
- Free play indoors on the PSP (next timing run).
- The Plant: not run at all. Rooms with many guards may cost like the deck [I].
- v26me: whether the Media Engine slows the main CPU or the GE through the shared bus.

## Recommended order

1. Read v26me's run (sound on the Media Engine).
2. One PSP run of free play indoors: tells how much of the game can reach 25 fps.
3. The new code generator (exact; helps every area and the memory fit).
4. The renderer on the PSP, alternating with 3: the first real frame rate.
5. Native rewrites, deck effects first, then guards and player; lighter weather only if the measured numbers still need it, shown to Ffm with pictures first.
