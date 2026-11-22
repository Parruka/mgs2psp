#!/bin/sh
# gl16_mk.sh <seed>: gl16's final link (sb14's pgo_gl.sh d with the chosen code order) and the delivery folder /home/claude/gl16:
# the EBOOT re-titled (pk/: unpack, mksfoex, pack, and an unpack check in pk/chk), the 8 overlays, mgs2.cfg = gl16_cfghead.txt + the
# indoor route's cfg (v30 dl_draft) + sbg16/dl_cfg.txt (gl12's way), then gl16_chk.sh.  README and md5_container.txt come after.
X=/home/claude/sbg16; G=/home/claude/gl16; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad; L=$X/drv.log
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
sd=$1; [ -f $X/lay/order_s$sd.txt ] || { st "mk: no lay/order_s$sd.txt"; exit 1; }
[ -f $X/dl_cfg.txt ] || { st "mk: no dl_cfg.txt"; exit 1; }
st "d start (seed $sd)"; ( cd /home/claude/sb14 && sh pgo_gl.sh d $X $sd > $X/d_step.out 2>&1 ); st "d rc $?"
[ -f $X/d/EBOOT.PBP ] || { st "d failed"; exit 1; }
rm -f /home/claude/sbg16/lay/t0m.it /home/claude/sbg16/lay/t1m.it; st "traces freed: $(df -h / | tail -1 | awk '{print $4}') free"   # the d step was their last user
[ ! -e $G ] && mkdir -p $G/pk/chk || { st "mk: $G exists"; exit 1; }
cd $G/pk && unpack-pbp $X/d/EBOOT.PBP > unpack.out 2>&1 && mv PARAM.SFO PARAM_build.SFO && mksfoex -d MEMSIZE=1 "MGS2 harness gl16 meshes" PARAM.SFO \
  && pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL DATA.PSP NULL > pack.out 2>&1 || { st "mk: re-title failed"; exit 1; }
cd $G/pk/chk && unpack-pbp ../EBOOT.PBP > /dev/null 2>&1 && cmp DATA.PSP ../DATA.PSP && cmp PARAM.SFO ../PARAM.SFO || { st "mk: re-title check failed"; exit 1; }
cp $G/pk/EBOOT.PBP $G/ && cp $X/d/ovl_init.bin $X/d/ovl_r_title.bin $X/d/ovl_r_tnk0.bin $X/d/ovl_n_title.bin $X/d/ovl_d00t.bin $X/d/ovl_d01t.bin $X/d/ovl_w00a.bin $X/d/ovl_w01a.bin $G/ \
  && cat $S/gl16/gl16_cfghead.txt /home/claude/v30/dl_draft/mgs2.cfg $X/dl_cfg.txt > $G/mgs2.cfg || { st "mk: copies failed"; exit 1; }
st "folder made: $(ls $G | tr '\n' ' ')"
sh $S/gl16/gl16_chk.sh
