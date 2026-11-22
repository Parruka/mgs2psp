sb3: gl9's picture code (tools/dev/sb2, lanes 1 and 2) with the exact trims of renderer_notes 11.28 (task #111), 2026-10-31.  Not in the
shared code yet.  Container tree /home/claude/sb3 = /home/claude/sb2 with sb3_trims.diff applied (apply it to a copy of sb2's tree).
The trims, each the same game state and the same picture (checked on x86 from boot on the three routes, all on and taking turns):
  1. OPT_LANE_MATCH 0x800000: the walk tries only the lane that can take a group (the TTE word of the tag after its first material:
     STCYCL 3 / 1 for lane_p04, 1 / 1 for lane_p06), none when there is no room for either.
  2. io_dma_start not inlined into io_flush (no switch): the register writes that start no DMA no longer save and restore the walk's
     registers.
  3. OPT_DIRECT_QW 0x1000000: DIRECT data goes to the GIF path a quadword at a time from the data itself when one starts at qi 0.
  4. OPT_MPG_CMP 0x2000000: an MPG's words compared with the program eight at a time.
  5. OPT_CLIP_FAST 0x4000000: the effects' clip positions (ge/xconv.c xc_fpos) without the VU float clamps when every input is 0 or
     within 2^-28 .. 2^60 in size (ge/vu1conv.h vc_clip_in, vc_vu_clip_nc); XcHost.clipfast, set from g_opt each frame (PSP
     xl_psp.c, x86 xlive_chk.c).  The log line "effect vertices N, M of them through the clamps with the fast path on".
All four bits are on by default; MGS2_OPT=f87fffff (x86 env or PSP cfg) turns all four off.
mk.sh <dir>: the x86 twin; codec_x86.sh: the codec route on it; dig_x86.sh: per-call digests; mkpsp.sh <new dir>: the PSP model build
(gl7's build with this tree's boot_rt, ge and xl_psp objects, no PGO for those; /home/claude/sb3p/d); pgo_gl.sh: the PGO + layout
script with T = this tree.
x86 checks (as sb2's): psp tools/dev/gl1_x86.sh <dir> <tag> i 0:37800 (or d 0:25200) MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688
MGS2_SB_LANE=3 [MGS2_OPT_CYCLE=0:100:ffffffff,f87fffff,ff7fffff,feffffff,fdffffff,fbffffff]; codec_x86.sh <dir> <tag> 40000
MGS2_SB_LANE=3 [...]; compare with sb2's logs (/home/claude/sb2/x1/out_i3n.log, x1d/out_d3n.log, x1c/out_c3n.log) and
prof/hashref_x86_{in,deck,codec}_h2.txt.
