#!/bin/sh
# em38a.sh (gl38, the no-PGO build np2, EBOOT fecd657b, as package pk_n38 on the emulator): n2 the gl38 cfg (cfg/mgs2_gl38.cfg), then n3
# F1's check mode with the re-use recording every run (cfg_emu/mgs2_gl38_f1chk.cfg: + MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1
# MGS2_NAT_F1CHK=1, without the shots and the re-use cycle: 42 keys).  Each game folder is deleted after its run (literal path).
R=/home/claude/main/build_gl38; cd $R || exit 1
nice -n 10 sh em38.sh n2 > $R/em_n2.out 2>&1; rm -rf "/home/claude/main/build_gl38/ms/em_n2"
nice -n 10 sh em38.sh n3 $R/cfg_emu/mgs2_gl38_f1chk.cfg > $R/em_n3.out 2>&1; rm -rf "/home/claude/main/build_gl38/ms/em_n3"
echo "em38a.sh: done $(date -u +%H:%M:%S)"
