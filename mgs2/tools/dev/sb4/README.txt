sb4: sb3 (gl9's picture code with the exact trims, tools/dev/sb3) + step c (renderer_notes 11.29, task #112) + the thinned rain and
foot splash (renderer_notes 11.30, task #114), 2026-10-31.  Not in the shared code yet.  Container tree /home/claude/sb4 = /home/claude/sb3
with sb4.diff applied (diff -uN per file; apply it with patch -p1 inside a copy of sb3's tree, then copy the scripts here over sb3's).

Step c (OPT_XL_STEPC 0x8000000, on by default; MGS2_OPT=f7ffffff turns it off): lane 2's P06 calls (their vertices in the game's data,
XlCall.rec) are held at the call and converted at the end of their transfer (xl_flush), one after another, so the converter's code
stays in the I-cache.  The batches and their content are those of the call-time path, in the same order; only their data lies elsewhere
in the frame's data block.  Files: ge/xconv.h, xconv.c (XcHeld, xc_hold_test, xc_hold, xc_hold_pass, xc_held_run, xc_held_pass, hi_io,
sv_rebuild), ge/gebatch.h, gebatch.c (gb_slot, gb_fill_in, gb_fill_at), ge/gelive.h, gelive.c (gl_call's hold, gl_flush, the pass
callback), ge/xlate.h, xlate.c (xl_on_flush at a transfer's end), runtime/host_time.h (the bit), psp/xl_psp.c (cfg MGS2_GL_HOLD: records
in use at most, 1..256, default 256; the log lines), boot/xlive_chk.c (gl_digest_c, the step c line; MGS2_SB_CHK, MGS2_SB_FORCE,
MGS2_SB_POISON, MGS2_GL_HOLD).

Thinned rain and splash (task #114; picture only, the game's state untouched): cfg MGS2_GL_RAIN=N draws 1 strip in N of the sheets of
drops close to the camera (the d_splash_camera.c actor's draws), MGS2_GL_SPLASH=N 1 in N of the splash at Snake's feet (ft_splsh.c);
1 (or no line) = as the PS2, N 1..8.  Files: ge/fxthin.h, fxthin.c (the actor walk and the blocks), ge/xlate.h, xlate.c (XlCall.src:
the guest address of the last UNPACK to T + 0x100), ge/xconv.h, xconv.c (XcThin, XcHost.thin / nthin, xc_fpos's strips), ge/gelive.h,
gelive.c (the scan before a frame's first effect call), psp/xl_psp.c (the cfg lines, the log lines), boot/xlive_chk.c (the env lines,
the report line; the check MGS2_THIN_CHK=1: each thinned call converted again without thinning and compared; fx_dump and the
src field in the MGS2_SB_PROBE=<vb>[:<vb>] window's call lines are dev tools of the search).

Scripts (T = /home/claude/sb4): mk.sh <dir> the x86 twin; codec_x86.sh the codec route; dig_x86.sh per-call digests; mkpsp.sh <new dir>
the PSP model build (gl7's build with this tree's objects, no PGO for the picture code); pgo_gl.sh the PGO + layout script (fxthin in
its lists; XCFG: the cfg lines its training and trace runs add, default gl10's MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2); model.sh <tag> <A> <B> in|codec [cfg lines] one PspProf window of the model build (GL3=/home/claude/sb4r, d -> sb4p/d3).
Dev tools (python3 -I): cmp4.py <on log> <off log> <reference log> (content digests, picture lines, state hashes, other lines);
fndelta.py <dumpA> <dumpB> <build dir> <ovl elf> <steps> [n] (per-function change in ms with the fitted weights); fxsrc.py and
rain_x86s.sh (the probe of the effect calls with their vertex data address).

x86 checks: psp tools/dev/gl1_x86.sh <dir> <tag> i 0:37800 (or d 0:25200) MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_SB_LANE=3
[MGS2_OPT=f7ffffff] [MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 [MGS2_THIN_CHK=1]]; codec_x86.sh <dir> <tag> 40000 MGS2_SB_LANE=3 [...]; compare
with sb3's logs (/home/claude/sb3/x1/out_ion.log, x1d/out_don.log, x1c/out_con.log) and prof/hashref_x86_{in,deck,codec}_h2.txt.
- g10win.py <gl9 log> <gl10 log>: reads gl10's PSP run against gl9's run 1 (task #116): minutes per part, everything on gl10 - gl9 on the same windows, step c off and the trims off against their all-on neighbours; the windows holding gl10's three new shots (30,700, 30,800, 30,900) are left out and printed on their own.
