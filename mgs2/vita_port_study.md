# How the PS Vita version of MGS2 cut its costs (study of 2026-11-27)

Asked by the owner on 2026-11-27 ("how did they simplify the logic for vita?"). Reading study only: no build, no PSP run.
This page is the summary: public facts, what the reference reading showed in our own words, and what could apply to the PSP
deck. The detailed reading notes are kept with our private reference notes.

Marks: [P] read on a public page; [R] from our reference reading (a lead until checked on our own PAL code); [C model] our
cost model; [I] inferred, basis given; UNKNOWN.

## Short answer

They did not simplify the game's logic. At 30 fps the Vita still steps the logic 60 times a second, with the same AI,
collision and counters [R]. The savings were elsewhere:

1. **Draw preparation on other cores** [R, P]. The game thread only finishes the bone matrices and snapshots the draw queues;
   a second thread does the cull, lights, shadows and draw lists; a third submits to the GPU; workers take small jobs. The
   developer's own page says they "pipelined the renderer and multi-threaded the game to take advantage of multiple cores" [P].
2. **A picture every other step** when a step does not fit, always on the Tanker deck, with all picture-only work skipped on
   the unshown steps [R]. Reviews saw exactly this: MGS2 flips between 60 and 30, game speed never drops [P].
3. **About 225 effects at half rate**, the deck's rain among them (streaks, line splashes, drops on the camera, puddle rings,
   splashes on characters and bodies): they update only on drawn steps, with no time correction [R]. Thirteen effects were
   kept at full rate. The guards' random numbers move as a side effect, which they accepted [I].
4. **The marines in the holds** (up to 128): their update is cut into jobs of 10 for the workers, and their draw commands are
   built once at load and merged by model [R]. Spread over cores, not simplified.

Not touched on the Vita: the floor and falling splashes, the near drop sheets, the ropes, the lamps, the deck's sea, and all
of the AI and physics [R]. No effect was removed [P, R].

## Public sources [P]

- Developer page: https://armature.com/games/mgshd (in-house Vita version; renderer pipelined, game multi-threaded; claims a
  solid 30 fps).
- Gaming Age review, 2012-06-19: MGS2 tries for 60 and often drops to 30; MGS3 a steady 30.
- TheSixthAxis, 2012-06-21: 60 or half of it "whenever there's stuff going on"; below the screen's native resolution, UI
  sharp, action slightly soft.
- No public developer talk with numbers was found; no review names the deck's rain or a removed effect.

## What could apply to the PSP deck

Sizes are [I] from our [C model] per-actor figures (rain_options_2026-11-21.md §2.2, rain wait). None is a PSP measurement.

1. **Rain at half rate, the Vita's list.** Run rain.c, line_sph, ripple, charaspread, rain_cm and body_sph's vertex work only
   on our shown steps (we show every second step already). Their acts are 3.6 model ms a step in the rain wait, so about
   -1.8 ms a step [I]. Visible: splashes and rings animate at half speed in real time, as on the Vita's deck. Keep the guards'
   dice with the seed jump we already use. New hash reference needed. Stacks with gl43's cut B, which covers the two actors
   the Vita left alone. Variant: rain.c's streaks are never drawn in our build, so its act could be a stub on every step (our
   exact-skip method, up to 1.2 ms [I]).
2. **Fewer drops at creation.** The 2003 PC version thinned the rain where the actors are made: the streak unit count and
   the line splashes' per-unit drop counts come from the stage script and were scaled there [R]. A data-level cut for the
   whole deck with no per-step code; the seed jump must add the missing draws. Overlaps with idea 1.
3. **Draw preparation off the main CPU by the Vita's seam.** Main processor: bone matrices and a snapshot (object headers,
   rotation or movement arrays, root and light matrices, packet descriptors; no vertex data; primitive vertex buffers read in
   place because flips happen only on drawn steps). Other processor (for us the ME): bounding test and cull, lights, lists,
   shadows. Nothing is written back to the game [R]. New for us: the cull itself may be able to leave the main CPU, not just
   move to the VFPU. Check with the read hash that no game code reads a visibility result. This is the lever for the indoor
   draw-preparation cost, not for the rain.
4. **Draw lists built once for static and repeated models.** Ours already for the ship; the holds will need the crowd form.
5. **No precedent for cheaper game logic.** The deck's logic outside the rain actors was paid for by a faster processor.
   Half-rate logic was never shipped by anyone; only half-rate effects were.
