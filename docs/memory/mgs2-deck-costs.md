---
name: mgs2-deck-costs
description: MGS2 rainy aft deck cost split (PSP parts, logic by actor, picture by owner, pipeline and walk by job) and the costed cut/downgrade options with the recommendation (task #160, mgs2/deck_costs.md)
metadata:
  type: project
  modified: 2026-11-03T18:20:00.000Z
---

- Ffm 2026-11-03 16:32: "what cost more ? what options we have to cut/downgrade?". Answer mgs2/deck_costs.md, reply 17:24. Bucket model build /home/claude/sb17km/d (ephemeral) = sb17 + tools/dev/sb17k (fxbk.c/h, sb17k.diff, mkpsp17k.sh, models17k.sh, models17f.sh; readers bkread.py, bkgroup.py, fnsplit.py, jobs.py). Model/PSP in the rainy window: logic x0.98, draw pipeline x0.90, walk x0.73, picture x0.83.
- gl16 PSP, rainy aft deck: 144.4 = logic 41.8, DG 29.5, IOP 2.5, walk 31.3, picture 39.4.
- Logic (model): weather 9.8 (8.2 without wave2 and wind), guards 6.5 (watcher), Snake 5.4, ropes 4.4, lamps 3.5 (fade_lgt 2.9), rest 11.3.
- Picture (model, effects on): people 20.1, ship df0 9.8 (15.0 while Snake waits), frame 7.0, gl16's weather effects 6.7 (d_splash_fall 5.7), gl16's other effects 4.9, C4 rain 10.1 (streaks 4.6; sprites charaspread, line_sph, body_sph, rain_gas_pers 5.5), C4 lamps 2.3, C4 rest 0.6. Today's plan +16.4 model (picture 14.0, walk 2.4), about +13 PSP. stagec_notes 4.12's "glows, smoke, haze" is mostly the rain's own sprites on the deck.
- DG 32.8 model: VU0 programs 7.0, DMA 6.6, libdg/screen 6.2, sort pass 5.1, ship 3.8, people 1.8. Walk 45.3: unpack 11.3, DMA chain 7.7, texture/palette 7.5, gif_qw 4.8, GS registers 4.1, translator calls 4.0, lanes 3.0.
- RECOMMENDED: gl17 = C4 without the rain's effects, sheets whole. Ffm chose No streaks (17:38); sprites filter built, exact, measured -5.26 model (~4.3 PSP [I]); Ffm chose Leave them out (card 18:19; deck_costs.md §5 updated). Then a game-side rain study (rain off or fewer drops; the rain actors share the LCG seed 0x1fc954, speed_notes §15). Logic after the exact work about 30 [I] > 20, so 25 fps on the rainy deck needs at least 10 ms of logic content cut.
- Owner names come from actor+0x28 at the end of the run: early-run owners are garbage, deck-window owners valid. Related: [[mgs2-stagec-effects]], [[mgs2-outlook]].
