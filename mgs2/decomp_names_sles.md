# MGS2 decompilation names mapped onto SLES_503.83 (candidates)

Source of names: GirianSeed/mgs2old `config/symbol_addrs.txt` (binary-derived decompilation of SLUS_205.54, Substance NTSC-U). Addresses there are SLUS; this table moves each to SLES_503.83 (`disc/SLES_503.83`, .text 0x100000-0x17cf78).

Method (2026-10-18, web research thread): list every `addiu sp,sp,-n` prologue in the SLES .text; for each SLUS symbol, take the offset (SLES - SLUS) to a prologue within 0x4000 that is shared by at least 4 of its 10 nearest named neighbours. Code moves in runs with one offset (e.g. libdg -0xca0, mts/printf -0x24d8, sif/zlib -0x2540). Leaf functions without a prologue and isolated symbols are not mapped.

**Confidence:** support = how many neighbours share the offset. 5+ = likely; 4 = candidate. **Checked independently:** `DG_VblankStart` = 0x11d930 and `DG_ActFirst` = 0x11d9d0 match the vblank handler and end-of-frame wait already found in elf_notes.md. Everything else is unverified until checked against the code (or against a SLUS_205.54 dump, sha1 0dce210e0d3fce2158141187cd6dc12dca57f02c). Not mapped yet (no consistent offset): GV_ExecActorSystem, gamed_Act, the GCL interpreter, HZX, MT.

**Correction (2026-11-01, rechecked in the "MGS2 port" thread):** the reference's symbols, placed on our ELF (our function-name lists), fall on 15 of these addresses. DG_StartDaemon agrees. Seven look like the same functions under later names (MC_SetGetdir / MC_SetGetdir2 / MC_Init / MC_Close = MCAccessGetdirStart / MCAccessGetdirNextStart / MCAccessInit / MCAccessEnd; GV_DefaultHook = GV_DefaultSignalFunc; NewGaged = NewGageDaemon; NewSpriteDma = SPR_Init2D_ObjectDriver). DG_ResetGraph 0x11dc78 is DG_VramClear there. **The six actor-system rows are wrong** (struck through below): 0x141b28 and 0x141ba0 are vector helpers (libgv util), and 0x143b98-0x143e00 are libhzx (the whole hzxd object fits there). Nothing in our notes or code used them. Use our function-name lists for names, each as a lead to check.

Engine shape from the MGS1 decompilation (FoxdieTeam/mgs_reversing) and mgs2old `source/main/main.c`: `Main` starts daemons GV, FS, DG, HZX, GCL, MT, GM, then loops `GV_ExecActorSystem()` (one call = one frame). Actor lists run in fixed order DAEMON, MANAGER, ASSIST, PREV2, PREV, USER, AFTER, AFTER2, DAEMON2. In MGS1, `DG` Act1 (first) waits for vblank and kicks the previous frame's draw; Act2 (last) runs the draw pipeline (screen, bound/cull, transform, shade, prim, sort). Draws are submitted one frame late and game state is double-buffered by `GV_Clock` (MGS1; inferred for MGS2). Inferred cut for the GE renderer: `DG_ActLast` and the pipeline it runs, plus the kick in `DG_ActFirst`; actors and DG object queues stay as game logic.

| Name | SLUS | SLES | Offset | Support |
|---|---|---|---|---|
| main | 0x1002e0 | 0x1001a0 | -0x140 | 4 |
| ResetIOP | 0x100350 | 0x100210 | -0x140 | 4 |
| LoadFile | 0x1003c0 | 0x100280 | -0x140 | 4 |
| LoadModules | 0x100450 | 0x100310 | -0x140 | 4 |
| init_file_info | 0x100500 | 0x1003c0 | -0x140 | 4 |
| load_compressed_file | 0x1008b0 | 0x100708 | -0x1a8 | 4 |
| MC_DoSeek | 0x1036e8 | 0x103258 | -0x490 | 4 |
| MC_DoRead | 0x103868 | 0x1033d8 | -0x490 | 4 |
| MC_DoWrite | 0x103a10 | 0x103580 | -0x490 | 4 |
| MC_DoMkdir | 0x103b98 | 0x103708 | -0x490 | 5 |
| MC_DoChdir | 0x103d00 | 0x103870 | -0x490 | 5 |
| MC_SetGetdir | 0x103e50 | 0x1039c0 | -0x490 | 5 |
| MC_SetGetdir2 | 0x103e90 | 0x103a00 | -0x490 | 6 |
| MC_DoGetDir | 0x103ed0 | 0x103a40 | -0x490 | 5 |
| MC_DoDelete | 0x104030 | 0x103ba0 | -0x490 | 5 |
| MC_DoRename | 0x104198 | 0x103d08 | -0x490 | 5 |
| MC_DoFileInfo | 0x1042f0 | 0x103e60 | -0x490 | 5 |
| MC_DoFormat | 0x104448 | 0x103fb8 | -0x490 | 6 |
| MC_DoUnformat | 0x104588 | 0x1040f8 | -0x490 | 6 |
| MC_Init | 0x1047a8 | 0x104318 | -0x490 | 6 |
| MC_Close | 0x104860 | 0x1043d0 | -0x490 | 5 |
| GM_ResetSystem | 0x104b48 | 0x1046b8 | -0x490 | 5 |
| GM_ResetMemory | 0x104b80 | 0x1046f0 | -0x490 | 4 |
| DG_VblankStart | 0x11e5d0 | 0x11d930 | -0xca0 | 5 |
| DG_ActFirst | 0x11e670 | 0x11d9d0 | -0xca0 | 4 |
| DG_ActLast | 0x11e730 | 0x11da90 | -0xca0 | 4 |
| DG_StartDaemon | 0x11e798 | 0x11daf8 | -0xca0 | 4 |
| DG_ResetGraph | 0x11e918 | 0x11dc78 | -0xca0 | 4 |
| GV_ResetPacketMemory | 0x13fea0 | 0x13eda8 | -0x10f8 | 5 |
| GV_InitMemory | 0x13fed8 | 0x13ede0 | -0x10f8 | 5 |
| GV_ResetSystem | 0x13ff10 | 0x13ee18 | -0x10f8 | 5 |
| GV_ResetMemory | 0x13ff28 | 0x13ee30 | -0x10f8 | 5 |
| gvd_Act | 0x13ff50 | 0x13ee58 | -0x10f8 | 5 |
| GV_StartDaemon | 0x13ff98 | 0x13eea0 | -0x10f8 | 5 |
| ~~GV_InitActor~~ WRONG: GV_VecDir2X | 0x140320 | 0x141b28 | +0x1808 | 4 |
| ~~GV_NewActor~~ WRONG: GV_VecToRot | 0x140398 | 0x141ba0 | +0x1808 | 4 |
| GV_DefaultHook | 0x140480 | 0x13f390 | -0x10f0 | 5 |
| ~~GV_ExecActorHook~~ WRONG: HZX_SwitchTrap | 0x140550 | 0x143b98 | +0x3648 | 4 |
| ~~GV_DestroyActorQuick~~ WRONG: HZX_ResetMemory | 0x140668 | 0x143cb0 | +0x3648 | 4 |
| ~~GV_DestroyActor~~ WRONG: NewHzxSetGroupAdd | 0x140758 | 0x143da0 | +0x3648 | 4 |
| ~~GV_FindActor~~ WRONG: NewHzxResetGroupAdd | 0x1407b8 | 0x143e00 | +0x3648 | 4 |
| gaged_Act | 0x157ea8 | 0x1568f0 | -0x15b8 | 4 |
| gaged_Die | 0x158108 | 0x156b50 | -0x15b8 | 5 |
| NewGaged | 0x158178 | 0x156bc0 | -0x15b8 | 6 |
| sunglasses_Act | 0x158880 | 0x1572c8 | -0x15b8 | 6 |
| sunglasses_Die | 0x158bc0 | 0x157608 | -0x15b8 | 6 |
| sunglasses_GetResources | 0x158c38 | 0x157680 | -0x15b8 | 5 |
| NewSunglasses | 0x158da0 | 0x1577e8 | -0x15b8 | 4 |
| bmbctrl_Act | 0x1598a0 | 0x158288 | -0x1618 | 5 |
| NewBombControl | 0x1599c0 | 0x1583a8 | -0x1618 | 5 |
| sprite_dma_Act | 0x15c8a0 | 0x15b288 | -0x1618 | 5 |
| sprite_dma_Die | 0x15caa0 | 0x15b488 | -0x1618 | 5 |
| sprite_dma_GetResources | 0x15cc70 | 0x15b658 | -0x1618 | 5 |
| NewSpriteDma | 0x15cde0 | 0x15b7c8 | -0x1618 | 5 |
| MTS_ExitThread | 0x15f9c8 | 0x15d4f0 | -0x24d8 | 4 |
| exit_thread | 0x15fa40 | 0x15d568 | -0x24d8 | 5 |
| new_thread | 0x15fae0 | 0x15d608 | -0x24d8 | 6 |
| NewThread | 0x15fb58 | 0x15d680 | -0x24d8 | 6 |
| MTS_NewThread | 0x15fb80 | 0x15d6a8 | -0x24d8 | 6 |
| MTS_BootThread | 0x15fc70 | 0x15d798 | -0x24d8 | 7 |
| MTS_Exit | 0x15fd00 | 0x15d828 | -0x24d8 | 7 |
| cprintf | 0x15fd80 | 0x15d8a8 | -0x24d8 | 6 |
| printf | 0x15fdd0 | 0x15d8f8 | -0x24d8 | 7 |
| excep_printf | 0x15fe20 | 0x15d948 | -0x24d8 | 7 |
| event_callback | 0x15fe78 | 0x15d9a0 | -0x24d8 | 7 |
| MTS_CreateEvent | 0x15fea8 | 0x15d9d0 | -0x24d8 | 8 |
| MTS_IntrSetEvent | 0x15ffe0 | 0x15db08 | -0x24d8 | 8 |
| MTS_SetEvent | 0x160090 | 0x15dbb8 | -0x24d8 | 8 |
| MTS_ClearEvent | 0x160118 | 0x15dc40 | -0x24d8 | 7 |
| MTS_DelayThread | 0x160178 | 0x15dca0 | -0x24d8 | 7 |
| cdbios_callback | 0x160218 | 0x15dd40 | -0x24d8 | 7 |
| cdbios_init | 0x160280 | 0x15dda8 | -0x24d8 | 7 |
| cdbios_callback_read | 0x160370 | 0x15de98 | -0x24d8 | 5 |
| cdbios_callback_read_fast | 0x1603d8 | 0x15de98 | -0x2540 | 6 |
| cdbios_read | 0x160440 | 0x15df00 | -0x2540 | 7 |
| cdbios_stop | 0x160460 | 0x15df20 | -0x2540 | 7 |
| cdbios_preseek | 0x1604a8 | 0x15df68 | -0x2540 | 7 |
| cdbios_get_time | 0x1604f8 | 0x15dfb8 | -0x2540 | 8 |
| cdbios_get_filepos | 0x160598 | 0x15e058 | -0x2540 | 9 |
| sd_init | 0x160618 | 0x15e0d8 | -0x2540 | 9 |
| sd_set_cli | 0x160688 | 0x15e148 | -0x2540 | 9 |
| sif_set_callback_func | 0x160788 | 0x15e248 | -0x2540 | 9 |
| sif_send_packet_and_data | 0x160838 | 0x15e2f8 | -0x2540 | 9 |
| i_sif_send_packet_and_data | 0x160918 | 0x15e3d8 | -0x2540 | 9 |
| sif_send_direct | 0x1609f8 | 0x15e4b8 | -0x2540 | 9 |
| i_sif_send_direct | 0x160a28 | 0x15e4e8 | -0x2540 | 9 |
| i_sif_send_mem | 0x160a58 | 0x15e518 | -0x2540 | 10 |
| sif_send_mem | 0x160a90 | 0x15e550 | -0x2540 | 9 |
| sif_check_status | 0x160ac8 | 0x15e588 | -0x2540 | 9 |
| sif_init | 0x160ae0 | 0x15e5a0 | -0x2540 | 9 |
| rv_man_callback | 0x160b40 | 0x15e600 | -0x2540 | 9 |
| sif_init_rv_man | 0x160c20 | 0x15e6e0 | -0x2540 | 8 |
| sif_rv_call_func | 0x160d08 | 0x15e7c8 | -0x2540 | 8 |
| sif_rv_return_value | 0x160d30 | 0x15e7f0 | -0x2540 | 8 |
| i_sif_rv_return_value | 0x160d78 | 0x15e838 | -0x2540 | 8 |
| sif_get_mem | 0x160dc8 | 0x15e888 | -0x2540 | 8 |
| z_inflateReset | 0x160e38 | 0x15e8f8 | -0x2540 | 9 |
| z_inflateEnd | 0x160e90 | 0x15e950 | -0x2540 | 9 |
| z_inflateInit2_ | 0x160ed8 | 0x15e998 | -0x2540 | 9 |
| z_inflateInit_ | 0x160fd8 | 0x15ea98 | -0x2540 | 9 |
| z_inflate | 0x161000 | 0x15eac0 | -0x2540 | 10 |
| z_adler32 | 0x161528 | 0x15efe8 | -0x2540 | 9 |
| inflate_blocks_reset | 0x1616c8 | 0x15f188 | -0x2540 | 9 |
| inflate_blocks_new | 0x161750 | 0x15f210 | -0x2540 | 8 |
| inflate_blocks | 0x1617b0 | 0x15f270 | -0x2540 | 8 |
| inflate_blocks_free | 0x162328 | 0x15fde8 | -0x2540 | 8 |
| inflate_codes | 0x162378 | 0x15fe38 | -0x2540 | 8 |
| inflate_fast | 0x162b70 | 0x160630 | -0x2540 | 7 |
| huft_build | 0x162f40 | 0x160a00 | -0x2540 | 6 |
| inflate_trees_bits | 0x163580 | 0x161040 | -0x2540 | 5 |
| inflate_trees_dynamic | 0x163620 | 0x1610e0 | -0x2540 | 5 |
| inflate_flush | 0x163808 | 0x1612c8 | -0x2540 | 4 |
