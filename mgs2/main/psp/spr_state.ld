/* v18: addresses of the hot machine state in the PSP scratchpad (16 KB at 0x00010000), for builds with -DGUEST_SPRSTATE (runtime/guest.h
   GSVS / SPRS make these extern; the PSP Makefile passes this file to the link as an implicit linker script, so the overlays see the
   symbols through -R MGS2T.elf).  Zero-initialised state only: main_psp.c zeroes the scratchpad after the probes, before the game starts,
   and boot.c rt_spr_check prints whether these ranges stay inside the scratchpad without overlapping.  Hottest first; 16-byte aligned
   where the VFPU forms use lv.q / sv.q (VF, VN, VACC, VUMEM). */
VF             = 0x00010000;   /* VQ[32]       512 */
VN             = 0x00010200;   /* VQ[32]       512 */
R              = 0x00010400;   /* int64_t[32]  256 */
RH             = 0x00010500;   /* int64_t[32]  256 */
F              = 0x00010600;   /* float[32]    128 */
HI             = 0x00010680;   /* int64_t */
LO             = 0x00010688;
HI1            = 0x00010690;
LO1            = 0x00010698;
ACC            = 0x000106a0;   /* float */
CC             = 0x000106a4;   /* int */
SAR            = 0x000106a8;   /* uint32_t */
k_left         = 0x000106ac;   /* int32_t */
k_tbase        = 0x000106b0;   /* uint64_t */
k_poll_at      = 0x000106b8;   /* uint64_t */
VIr            = 0x000106c0;   /* uint32_t[16]  64 */
VACC           = 0x00010700;   /* uint32_t[4]   16 */
VQr            = 0x00010710;   /* uint32_t each, to VCMSAR0 */
VPr            = 0x00010714;
VIreg          = 0x00010718;
VRr            = 0x0001071c;
VMAC           = 0x00010720;
VSTAT          = 0x00010724;
VCLIP          = 0x00010728;
VCMSAR0        = 0x0001072c;
g_vmac         = 0x00010730;
g_io_arm       = 0x00010734;
g_io_pend      = 0x00010738;
g_vu_ext       = 0x0001073c;
g_vgrp_fb      = 0x00010740;
g_fn_hook      = 0x00010744;   /* pointer */
vu_prog_clean  = 0x00010748;
vu_prog_hash   = 0x0001074c;
vu_steps       = 0x00010750;   /* long (32-bit on the PSP) */
vu_calls       = 0x00010754;
vu_trace       = 0x00010758;
vu_tpc         = 0x0001075c;
g_unsup_vu     = 0x00010760;
g_vu_old_fcget = 0x00010764;
rt_idiom_n     = 0x00010768;   /* long[2] */
g_hash_ee      = 0x00010770;   /* uint64_t */
g_hash_iop     = 0x00010778;
VUMEM          = 0x00010800;   /* uint8_t[4096] */
VUPROG         = 0x00011800;   /* uint32_t[1024] 4096, ends at 0x00012800 */
