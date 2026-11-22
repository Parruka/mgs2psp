# PS1 and PS2 games on the PSP: ports, remakes, recompiles (survey 2026-11-10)

Web research only; no MGS2 build touched (STOP ALL of 2026-11-08 still holds for builds).
Facts below come from the pages listed per item. Lines marked [I] are from general knowledge
and were NOT checked this session. "Claim" means the author says it and we could not verify it.

## 1. What helps the MGS2 port (read this first)

| # | Project | Why it matters to us | Usable as-is? |
|---|---|---|---|
| 1 | **sa1-psp** (Sonic Adventure, Dreamcast, static recompile, PSP-3001 + ARK-4) | Same machine and CFW as Ffm's. Reports that an EBOOT started from the Memory Stick gets ~4 MB less user memory than under PSPLink (largest free block 7680 KB vs 11776 KB). Its Media Engine work hangs the next PSPLink reboot (branch `me-wip`). Frame-time numbers per area (Station Square ~31.5 ms, Emerald Coast ~61 ms to draw). | Findings only. Check: our headroom figures (gl17 444 KB, gl18me ~840 KB [I]) are taken from the Memory Stick launch, so they are the real ones; worth confirming in psp_harness.md. |
| 2 | **BullyPSP-PS2R2C** (Bully PAL SLES-53561 on PS2RC) | Closest twin to our pipeline: PS2 code to C, VU1 microprograms rewritten natively to the GE, light GS model with a texture cache and deferred block uploads, VFPU skinning. Profile: game code ~30%, VIF parsing ~7%, batch setup ~15% for ~1,100 batches a frame, GS packets ~4%. Tried without gain: native sphere/box test, per-call-site indirect cache, FPU float saturation, skinned-vertex reuse. Open idea: replay identical DMA blocks (est. 2-4 ms). | Findings only. Their "replay identical DMA blocks" idea is close to our gelist/H1 work and worth a measurement on w00a once STOP ALL lifts. Their draw-distance x0.75 is a visual change (needs Ffm's OK, and the charter's drawing-side rule). Numbers are PPSSPP only, 80 ms/frame (~12.4 fps), not real PSP. |
| 3 | **PS2RC** (SandstoneDev; GTA San Andreas PAL SLES-52541) | Already reviewed in sandstone_review.md. New since: the README says the runtime uses the Media Engine; needs ~50 MB (code ~13 MB + PS2 RAM ~28 MB + buffers) with ARK "Use Extra Memory". SandstoneDev claims SA is playable start to finish on PSP and that Bully, Resident Evil 4 and Silent Hill 2 are in progress (claim, sa1-psp issue #1). No fps figures published. | Licence as before (only sound/movie code reused). Nothing new to take; no fps data to compare against ours. |
| 4 | **Star Wars: Battlefront II PSP** (Savage Entertainment, 2005) | Only official port with a quoted optimisation story: "a huge 'pop the champagne' day if you got back one whole frame a second"; LucasArts brought engineers from other teams. | No method detail. Confirms the PS2-to-PSP CPU wall we already measure. |
| 5 | **Tomb Raider: Anniversary PSP** (Buzz Monkey) | Dev called the PSP conversion "primarily a brute-force approach"; PS2 design ambitions "caused problems". | Nothing technical to reuse. |

Bottom line: no official PS2-to-PSP port kept the original logic and frame rate unchanged; the
ones with detail cut content, areas or frame rate (Persona 3 Portable dropped the walkable town,
Battlefront II dropped a campaign). The fan recompiles (PS2RC, BullyPSP, sa1-psp) use the same
method as ours, and none publishes a real-PSP frame rate near 25 fps for a 3D PS2 game yet.
None of them does exact game logic under frame skips the way our E5 skips do.

## 2. Fan-made and homebrew (native, not emulation)

| Project | Source | Method | State | PSP / numbers | Source URL |
|---|---|---|---|---|---|
| PS2RC (SandstoneDev) | GTA SA, PS2 | Static recompile + runtime (GE, ME) | Release 0.1.1; claim playable start to finish | PSP-2000/3000 + ARK, ~50 MB; no fps published | github.com/SandstoneDev/PS2RC |
| BullyPSP-PS2R2C (CamilleKMI) | Bully, PS2 | PS2RC-based | Experimental: title, movement, HUD; silent sound, no movies, no blur/glow | PPSSPP 333 MHz: 680 -> 116 -> 80 ms/frame; real PSP untested | github.com/CamilleKMI/BullyPSP-PS2R2C |
| Bully-psp (aliargersayed6-coder) | Bully, PS2 | unknown | unknown | page rate-limited, not read | github.com/aliargersayed6-coder/Bully-psp |
| Resident Evil 4, Silent Hill 2 (SandstoneDev) | PS2 | PS2RC | Claim "in progress" | none | github.com/countallegrex/sa1-psp/issues/1 |
| Two Towers port (ours) | LOTR TTT, PS2 | Static recompile + new GE renderer | Level 00isul1 | see tt_port/ | /mnt/project-files/tt_port/ |
| MGS2 port (ours) | MGS2 PAL, PS2 | Static recompile + new GE renderer | gl17: deck 3.4 fps, inside 5.5 fps, exact | real PSP-3000 | NOTES.md |
| OpenLara PSP (XProger) | Tomb Raider 1, PS1 | Engine reimplementation, original PSX data | alpha 3 (2018) | no figures | gamebrew.org/wiki/OpenLara_PSP |
| silent-hill-decomp (SlickAmogus) | Silent Hill 1, PS1 | Decompilation + PsyCross | PC port; PSP listed as "in development" only | none | github.com/SlickAmogus/silent-hill-decomp |
| sa1-psp (countallegrex) | Sonic Adventure, Dreamcast (not PS1/PS2, included for method) | Static recompile SH-4 to C, Katana layer replaced by sceGu/sceAudio/sceIo | Whole story runs; paused 2026-10-28 | PSP-3001 ARK-4; 31.5 ms Station Square, ~9 fps Emerald Coast with frame skip | github.com/countallegrex/sa1-psp |
| ps2-web-recomp (snuri00) | PS2 | EE AOT to C++ -> WebAssembly; IOP interpreted; VU1 JIT/interp | Browser only | no PSP relevance | news.ycombinator.com/item?id=50052251 |

Emulation, not ports: PS1 on PSP via POPS (official) and PSX-P (homebrew); no usable PS2 emulator exists on the PSP [I].
Not found: decomps.net (robots refusal) and recompedia.com list no PSP targets; no fan PS1 static recompile to PSP turned up.

## 3. Official releases

### PS one Classics on PSN (emulated by Sony's POPS, not ports)
- PAL PS1 downloads were at first PSP-only; PS3 play came with an April 2007 update (Wikipedia, PAL list).
- Metal Gear Solid (PS1) came out as a PS one Classic for PSP in PAL on 18 June 2009.
- Total count of PSP-compatible titles: not established (the Wikipedia tables were truncated). Several hundred across regions [I].

### PS2 games ported to the PSP (checked this session)
| Game | PSP developer | Notes |
|---|---|---|
| The Warriors | Rockstar Leeds (with Rockstar Toronto), 2007 | RenderWare; "drastic change" to controls |
| Midnight Club 3: DUB Edition | Rockstar Leeds, 2005 | No race editor, no Remix content |
| Tomb Raider: Legend | Buzz Monkey, 2006 | Shares level design; adds Tomb Trials, ad-hoc |
| Tomb Raider: Anniversary | Buzz Monkey, 2007 | "brute-force" conversion |
| Prince of Persia: Revelations (Warrior Within) | Pipeworks, 2005 | |
| Prince of Persia: Rival Swords (The Two Thrones) | not named, 2007 | Adds chariot sections |
| Star Wars: Battlefront II | Savage Entertainment, 2005 | Rise of the Empire campaign cut |
| Persona 3 Portable (FES base) | Atlus [I], 2009 | Town no longer walkable (cursor), portraits instead of 3D scenes, The Answer cut |
| Disgaea: Afternoon of Darkness | NIS, 2006/07 | 16:9, extra mode; two extras cut |
| Gitaroo Man Lives! | iNiS/Koei, 2006 | Duet mode added |
| Tony Hawk's Underground 2 Remix | not named, 2005 | Extra levels |
| Mortal Kombat: Unchained (Deception) | not named on MobyGames; Just Games Interactive [I] | Adds four fighters, Endurance mode |
| Monster Hunter Freedom (MH G) | Capcom, 2005 | G was a PS2/Wii title [I] |

Not ports (checked): Burnout Legends (new compilation), Silent Hill: Shattered Memories (Wii first).
Other PS2-to-PSP conversions [I], not checked: Tales of Destiny 2 and Tales of Rebirth (JP), Disgaea 2: Dark Hero Days, Warriors Orochi, Monster Hunter Freedom 2 (MH2 Dos), Need for Speed Most Wanted 5-1-0 and Underground Rivals (cut-down versions), Manhunt 2, Spider-Man 3, Lego Star Wars II and other same-day multi-platform releases.

### PS1 games remade or ported to the PSP [I], not checked
Final Fantasy Tactics: The War of the Lions, Valkyrie Profile: Lenneth, Star Ocean: Second Evolution, Breath of Fire III, Persona and Persona 2 (both parts), Tales of Eternia, Suikoden I & II, Mega Man Legends (JP), MediEvil: Resurrection, Ape Escape: On the Loose, Castlevania: Symphony of the Night (inside The Dracula X Chronicles), Street Fighter Alpha 3 MAX, Darkstalkers Chronicle.

## 4. Follow-ups (none started; STOP ALL)
- After STOP ALL lifts: measure "skip identical DMA/GE blocks" on w00a as an exact drawing-side change (idea from BullyPSP; overlaps H1).
- Confirm in psp_harness.md that headroom numbers come from the Memory Stick launch (sa1-psp's 4 MB note).
- Watch PS2RC for published real-PSP fps on SA or RE4: the only external yardstick for our numbers.

## Sources
- https://github.com/countallegrex/sa1-psp , https://github.com/countallegrex/sa1-psp/issues/1
- https://github.com/SandstoneDev/PS2RC , https://github.com/SandstoneDev/PS2RC/releases/tag/v0.1.1 , https://github.com/SandstoneDev/PS2RC/issues/3
- https://github.com/CamilleKMI/BullyPSP-PS2R2C
- https://github.com/SlickAmogus/silent-hill-decomp
- https://recompedia.com/platform/ps2 , https://github.com/wangxiaoxiang5599/recompiled-games-list
- https://news.ycombinator.com/item?id=50052251
- https://www.gamebrew.org/wiki/OpenLara_PSP
- https://en.wikipedia.org/wiki/The_Warriors_(video_game) , https://en.wikipedia.org/wiki/Midnight_Club_3:_DUB_Edition
- https://en.wikipedia.org/wiki/Tomb_Raider:_Legend , https://en.wikipedia.org/wiki/Tomb_Raider:_Anniversary
- https://en.wikipedia.org/wiki/Prince_of_Persia:_Warrior_Within , https://en.wikipedia.org/wiki/Prince_of_Persia:_The_Two_Thrones
- https://en.wikipedia.org/wiki/Star_Wars:_Battlefront_II_(2005_video_game)
- https://www.siliconera.com/persona-3-portable-features-upgrades-and-downgrades/
- https://en.wikipedia.org/wiki/Disgaea:_Hour_of_Darkness , https://en.wikipedia.org/wiki/Gitaroo_Man
- https://en.wikipedia.org/wiki/Tony_Hawk%27s_Underground_2 , https://www.mobygames.com/game/25986
- https://en.wikipedia.org/wiki/Monster_Hunter_Freedom , https://en.wikipedia.org/wiki/Burnout_Legends , https://en.wikipedia.org/wiki/Silent_Hill:_Shattered_Memories
- https://en.wikipedia.org/wiki/List_of_downloadable_PlayStation_games_(PAL_region)
