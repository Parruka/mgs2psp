# PSP timing harness (main_psp.c): the boot-path build of the recompiled game for the PSP-3000.  Run from a build directory whose parent
# (W) holds the reduced generated code (gen_elf.h, gen_s_<stage>.h from tools/prune_cov.py) and stages.h cut to the boot path:
#   python3 $(MG)/tools/stages_subset.py <full stages.h> $(W)/stages.h init r_title r_tnk0 n_title d00t d01t w00a
#   PATH=<pspdev>/bin:$PATH make -f /mnt/project-files/mgs2/psp/Makefile -j4
# Flags as the TT PSP build (tt_port/src/psp_game/Makefile) and the x86 probe's access path (boot/Makefile.probe CF: GEN_MM_FAST GUEST_SPR0),
# plus GEN_PIN23 (the memory base in $s7, -ffixed-23 everywhere).  The machine's exit / abort / getenv go to main_psp.c.
# Memory (the PSP's large-memory mode gives about 51 MB, sandstone_review.md 2.8): each stage image's code is an overlay file ovl_<stage>.bin
# (ovl_hdr.c + gen_s_<stage>.o linked by ovl.ld at main_psp.c's arena against MGS2T.elf's symbols; boot/dispatch.h STAGE_OVL loads it when
# the game first calls into that image), so only one stage's code is in memory; the guest / IOP host stacks come from the main thread's
# stack (HOST_STACK_POOL).  Copy the ovl_*.bin files next to the EBOOT.
TARGET = MGS2T
MG := /mnt/project-files/mgs2
RT := $(MG)/third_party/ps2rc/ps2rc/iop/rt
AU := $(MG)/third_party/ps2rc/audio
IG := $(MG)/iopgen
W := $(abspath $(CURDIR)/..)
STAGES := init r_title r_tnk0 n_title d00t d01t w00a
PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config --pspdev-path)

BASEF := -O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600
MACHF := -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv '-DK_HSTACK=(128*1024)' '-DIOP_HSTACK=(64*1024)'
# v18: SPRSTATE=1 (default) puts the hot machine state (guest registers, VU0 state, poll counters) in the PSP scratchpad: runtime/guest.h
# GSVS / SPRS declare it extern, spr_state.ld (an implicit linker script in the link) gives the addresses, main_psp.c zeroes it; SPRSTATE= for .bss
SPRSTATE := 1
SPRF := $(if $(SPRSTATE),-DGUEST_SPRSTATE)
GENF := $(BASEF) $(MACHF) -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 $(SPRF) -I$(MG)/psp -I$(MG)/runtime -I$(MG)/boot
OVL_SIZE := 0x460000
# Profile-guided builds (exact: only the code layout and the compiler's choices change; the hashes must stay identical):
#   PGO=gen   the game code and the runtime count their branches (-fprofile-generate): with PGO_ELF=1 the main ELF's (gen.o, gen_dispatch.o,
#             boot_rt.o), with PGO_STAGE=<stage> that overlay's; both at once do not fit in the PSP's memory, so two runs.  Run it in the
#             emulator with "pgo A B" in mgs2.cfg (main_psp.c pgo_vb), which writes <this directory>/*.gcda under the memory stick root
#   PGO=use   built with those counts (copy the .gcda files of both runs into this build directory first): hot / cold code split and laid
#             out by the counts, without the size-growing transformations (unrolling, peeling, tracing, cloning): the I-cache is the wall
PGO :=
# PGOX: extra flags for PGO=use, e.g. -freorder-blocks-and-partition
PGOX :=
# Code layout against the I-cache (tools/layout_sim.c, tools/layout_order.py): LAYOUT=1 puts each function of the game code and the runtime
# in its own section; layout_elf.ld in the build directory (an output-section fragment for ld --section-ordering-file) then lays the listed
# ones out first in the main ELF's .text, and layout_<stage>.ld (input-section lines) first in that overlay's code, after its header.
# Only the placement changes (exact).  Relink by hand after changing them (rm MGS2T.elf EBOOT.PBP).
LAYOUT :=
FSEC := $(if $(LAYOUT),-ffunction-sections)
PGO_STAGE := w00a
PGO_ELF := 1
comma := ,
GCOV_SYMS := __gcov_init __gcov_exit __gcov_merge_add __gcov_merge_ior __gcov_merge_time_profile __gcov_merge_topn __gcov_time_profiler_counter \
  __gcov_indirect_call __gcov_indirect_call_profiler_v4 __gcov_interval_profiler __gcov_pow2_profiler __gcov_topn_values_profiler \
  __gcov_average_profiler __gcov_ior_profiler
ifeq ($(PGO),gen)
PGOF := -fprofile-generate -fprofile-update=single
PGOF_ELF := $(if $(PGO_ELF),$(PGOF))
ifneq ($(PGO_STAGE),)
OVL_SIZE := 0xb40000
endif
LIBS_PGO := $(addprefix -Wl$(comma)-u$(comma),$(GCOV_SYMS)) -lgcov
MAIN_PGO := -DMGS2_PGO -DPGO_STAGE='"$(PGO_STAGE)"'
endif
ifeq ($(PGO),use)
PGOF := -fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone \
  -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning $(PGOX)
PGOF_ELF := $(PGOF)
endif
IOPF := $(BASEF) $(MACHF) $(FSEC) -I$(MG)/psp -I$(RT)
GEN_DEPS := $(MG)/boot/gen_pre.h $(MG)/runtime/guest.h $(MG)/runtime/mmi.h $(MG)/runtime/vu0.h $(MG)/runtime/vif.h
RT_DEPS := $(wildcard $(MG)/runtime/*.h) $(MG)/boot/boot.c $(MG)/psp/ucontext.h

STAGE_OBJS := $(addprefix gen_s_,$(addsuffix .o,$(STAGES)))
IOP_OBJS := iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o iop_main.o spu2_lite.o
# XOBJS: extra objects linked after the rest (empty unless set; from the environment too).  The live translator's test build (psp/xl1.sh,
# renderer_notes 11.12): XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o", MAINX=-DMGS2_XL and RTX with -DMGS2_XLIVE (boot_rt.o's DMA hook)
XOBJS ?=
OBJS = main_psp.o hwprobe.o psp_ctx.o boot_rt.o gen.o gen_dispatch.o $(IOP_OBJS) $(XOBJS)
OVL_BINS := $(addprefix ovl_,$(addsuffix .bin,$(STAGES)))

CFLAGS = $(BASEF)
# XLIBS: extra libraries first in the link (empty unless set; from the environment too), e.g. -lpspgu for the live picture (psp/gl1.sh)
XLIBS ?=
LIBS = $(XLIBS) $(LIBS_PGO) -lpspfpu -lpsppower -lpspdebug -lpspctrl -lpspdisplay -lm
PSP_LARGE_MEMORY = 1
EXTRA_TARGETS = EBOOT.PBP $(OVL_BINS)
PSP_EBOOT_TITLE = MGS2 timing harness v8

# MAINX: extra flags for main_psp.c only (empty unless set; from the environment too), e.g. -DMGS2_XL: the translator's timing region
MAINX ?=
main_psp.o: $(MG)/psp/main_psp.c $(MG)/runtime/host_time.h
	psp-gcc $(BASEF) $(MAIN_PGO) $(MAINX) $(FSEC) $(SPRF) -DOVL_SIZE=$(OVL_SIZE) -DHARNESS_TITLE='"$(PSP_EBOOT_TITLE)"' -I$(PSPSDK)/include -I$(MG)/runtime -c $< -o $@
hwprobe.o: $(MG)/psp/hwprobe.c
	psp-gcc $(BASEF) -I$(PSPSDK)/include -c $< -o $@
# (xl_psp.c takes MAINX too: -DMGS2_GL, the live picture, psp/gl1.sh)
xl_psp.o: $(MG)/psp/xl_psp.c $(MG)/runtime/host_time.h $(wildcard $(MG)/ge/*.h)
	psp-gcc $(BASEF) $(FSEC) $(MAINX) -I$(PSPSDK)/include -I$(MG)/runtime -c $< -o $@
ge_%.o: $(MG)/ge/%.c $(wildcard $(MG)/ge/*.h)
	psp-gcc $(BASEF) $(FSEC) -c $< -o $@
psp_ctx.o: $(MG)/psp/psp_ctx.S
	psp-gcc -c $< -o $@
# RTX: flags for the runtime unit only, e.g. RTX='-DMGS2_GEN_VU="<dir>/gen_vu.h"' (the AOT VU0 micro programs, recomp/vupipe.py)
RTX :=
boot_rt.o: $(RT_DEPS)
	psp-gcc $(GENF) $(PGOF_ELF) $(FSEC) $(RTX) -Dmain=mgs2_main -I$(RT) -I$(AU) -DGUEST_SPLIT_RT -c $(MG)/boot/boot.c -o $@
gen.o: $(W)/gen_elf.h $(MG)/boot/gen.c $(GEN_DEPS)
	psp-gcc $(GENF) $(PGOF_ELF) $(FSEC) -DGEN='"$(W)/gen_elf.h"' -c $(MG)/boot/gen.c -o $@
stages_ovl.h: $(W)/stages.h $(MG)/tools/stages_subset.py
	python3 $(MG)/tools/stages_subset.py --ovl $< $@ $(STAGES)
gen_dispatch.o: stages_ovl.h $(MG)/boot/gen_dispatch.c $(MG)/boot/dispatch.h $(GEN_DEPS)
	psp-gcc $(GENF) $(PGOF_ELF) $(FSEC) -DSTAGE_OVL -DSTAGES='"$(CURDIR)/stages_ovl.h"' -c $(MG)/boot/gen_dispatch.c -o $@
gen_s_%.o: $(W)/gen_s_%.h $(MG)/boot/gen_stage.c $(GEN_DEPS)
	psp-gcc $(GENF) $(if $(filter $*,$(PGO_STAGE)),$(PGOF)) $(FSEC) -DSTAGE_GEN='"$(W)/$(notdir $<)"' -c $(MG)/boot/gen_stage.c -o $@
ovl_hdr_%.o: $(MG)/psp/ovl_hdr.c
	psp-gcc $(BASEF) -DSTAGE=$* -c $< -o $@
ovl_%.bin: ovl_hdr_%.o gen_s_%.o $(TARGET).elf $(MG)/psp/ovl.ld
	sed '/KEEP(\*(.ovl_hdr))/r layout_$*.ld' $(MG)/psp/ovl.ld > ovl_$*.ld
	psp-ld -T ovl_$*.ld --defsym OVL_BASE=0x$$(psp-nm $(TARGET).elf | awk '$$3 == "ovl_arena" { print $$1 }') --defsym OVL_SIZE=$(OVL_SIZE) \
	  -R $(TARGET).elf -o ovl_$*.elf ovl_hdr_$*.o gen_s_$*.o $$(psp-gcc -print-libgcc-file-name)
	psp-objcopy -O binary -j .ovl ovl_$*.elf $@
.SECONDARY: $(addprefix gen_s_,$(addsuffix .o,$(STAGES))) $(addprefix ovl_hdr_,$(addsuffix .o,$(STAGES)))
iop_rt.o: $(RT)/iop_rt.c
	psp-gcc $(IOPF) -c $< -o $@
iop_rt_libs.o: $(RT)/iop_rt_libs.c
	psp-gcc $(IOPF) -c $< -o $@
iop_rt_ucontext.o: $(RT)/iop_rt_ucontext.c $(MG)/psp/ucontext.h
	psp-gcc $(IOPF) -c $< -o $@
jsifman.o cdbios.o iop_main.o: %.o: $(IG)/%.c
	psp-gcc $(IOPF) -c $< -o $@
spu2_lite.o: $(AU)/spu2_lite.c $(AU)/spu2_lite.h
	psp-gcc $(BASEF) $(MACHF) $(FSEC) -c $< -o $@

LDFLAGS := $(if $(wildcard layout_elf.ld),-Wl$(comma)--section-ordering-file=layout_elf.ld) $(if $(SPRSTATE),$(MG)/psp/spr_state.ld)
include $(PSPSDK)/lib/build.mak
.DEFAULT_GOAL := all
