#!/bin/sh
# model_gl31.sh: PspProf model runs of gl31's ELF (build_gl31/psp), native in every turn (arm 3: cfg_emu/mgs2_m33_p.cfg, whose
# settings are build_gl32/cfg_emu/mgs2_m_n3.cfg's), for gl33's controls: the opening (16,010-16,090 an ME turn, 16,110-16,190 a main
# turn), the steady deck in d01t (27,410-27,490 ME, 27,510-27,590 main) and the rain windows in a run of their own
R=/home/claude/main/build_gl33; cd $R/m || exit 1; C=$R/cfg_emu/mgs2_m33_p.cfg; H=$R/cfg/hashref_gl33.txt; P=/home/claude/main/build_gl31/psp
sh scripts/pgo.sh model $P m31c1 $C $H 16010:16090 16110:16190 > $R/pgo_m31c1.out 2>&1 &
sh scripts/pgo.sh model $P m31c2 $C $H 27410:27490 27510:27590 > $R/pgo_m31c2.out 2>&1 &
sh scripts/pgo.sh model $P m31r $C $H 31010:31090 31110:31190 > $R/pgo_m31r.out 2>&1 &
wait; echo done
