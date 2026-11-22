# env.sh: the mainline tree's settings, sourced by every script in scripts/ (2026-11-18, the container-reset rebuild of gl18me).
# M: this tree; MG: the shared project folder (read only: third_party/, iopgen/, recomp/, tools/, play/, prof/ are used in place);
# the inputs (game data, personal use only, container only): SL, STG, PAK, PADF; the toolchain PSPDEV; outputs under $M/gen and $M/build.
M=${M:-$(cd "$(dirname "$0")/.." && pwd)}
MG=${MG:-/mnt/project-files/mgs2}
SL=${SL:-/home/claude/mgs2/SLES_503.83}                      # md5 e3f4c0df..
STG=${STG:-/home/claude/mgs2/stage}                          # tools/extract_stage.sh's 123 stages
PAK=${PAK:-"/mnt/user-data/uploads/mg2 port/Builds/MGS2T_gl18/mgs2disc.pak"}   # md5 189ca53f..
PADF=${PADF:-$MG/play/pad_w01af.txt}                         # route i's pad (gl18's pad.txt 7bc72ace is this file)
PSPDEV=${PSPDEV:-/home/claude/pspsdk/pspdev}
PPSSPP=${PPSSPP:-/home/claude/ppsspp/src/ppsspp-1.20.4/build/PPSSPPHeadless}
G=$M/gen; B=${B:-$M/build}                                # gl19: B=<folder> builds and runs elsewhere (a copy)
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio; IG=$MG/iopgen; HP=$MG/third_party/ps2rc/hle/psp
STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a"
export PATH=$PSPDEV/bin:$PATH
